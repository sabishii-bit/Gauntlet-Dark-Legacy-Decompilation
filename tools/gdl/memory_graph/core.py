"""Build and query the GDL project-memory graph.

The SQLite file is a disposable materialized view. Durable reviewed facts live
as JSON records under knowledge/memory/records; legacy notes are preserved and
indexed with provenance but never promoted to verified facts automatically.
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import sqlite3
import subprocess
import tempfile
import ast
import uuid
from contextlib import closing
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable, Iterator


SCHEMA_VERSION = 1
PACKAGE_DIR = Path(__file__).resolve().parent
REPO_ROOT = PACKAGE_DIR.parent.parent.parent
SCHEMA_PATH = PACKAGE_DIR / "schema.sql"
RECORDS_DIR = REPO_ROOT / "knowledge" / "memory" / "records"
INBOX_DIR = REPO_ROOT / "knowledge" / "memory" / "inbox"

PDB_MODULE_RE = re.compile(r"^==\s+\.\\Release\\(.+?)\s+\((.*?)\)\s*$", re.I)
PDB_SYMBOL_RE = re.compile(
    r"^\[(\d{4}):([0-9A-Fa-f]{8})\]\s+([0-9A-Fa-f]+)\s+([GLD])\s+(.*)$"
)
GCN_SYMBOL_RE = re.compile(
    r"^(\S+)\s*=\s*([^:]+):0x([0-9A-Fa-f]+);\s*//\s*(.*)$"
)
SPLIT_MODULE_RE = re.compile(r"^([^\s].*?):\s*$")
SPLIT_TEXT_RE = re.compile(
    r"^\s*\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)"
)
META_RE = re.compile(r"([A-Za-z_]+):([^\s]+)")
PARKED_ENTRY_RE = re.compile(
    r"^([A-Za-z_~][A-Za-z0-9_:~]*)\s*(?:\[([^\]]+)\])?\s*(?:#\s*(.*))?$"
)
HEADING_RE = re.compile(r"^(#{1,6})\s+(.+?)\s*$")
TOOL_SOURCE_SUFFIXES = {".py", ".ps1", ".c", ".asm"}


class MemoryGraphError(RuntimeError):
    """Raised for invalid records or an unusable graph."""


@dataclass(frozen=True)
class Chunk:
    ordinal: int
    heading: str | None
    line_start: int
    line_end: int
    content: str


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _repo_relative(root: Path, path: Path) -> str:
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return str(path.resolve())


def default_database_path(root: Path = REPO_ROOT) -> Path:
    """Return a worktree-shared, Git-local database path when possible."""
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--git-common-dir"],
            cwd=root,
            check=True,
            capture_output=True,
            text=True,
        )
        common = Path(result.stdout.strip())
        if not common.is_absolute():
            common = (root / common).resolve()
        return common / "gdl-memory" / "memory.sqlite"
    except (OSError, subprocess.CalledProcessError):
        return root / ".gdl-memory" / "memory.sqlite"


def _iter_input_paths(root: Path) -> Iterator[Path]:
    for base in (RECORDS_DIR, INBOX_DIR):
        adjusted = root / base.relative_to(REPO_ROOT)
        if adjusted.exists():
            yield from sorted(adjusted.rglob("*.json"))
    legacy = root / ".claude" / "memory"
    if legacy.exists():
        for suffix in ("*.md", "*.txt"):
            yield from sorted(legacy.rglob(suffix))
    for path in (
        root / "config" / "GUNE5D" / "symbols.txt",
        root / "config" / "GUNE5D" / "splits.txt",
        root / "research" / "xbox_symbols" / "shell3D.pdb",
        root / "research" / "xbox_symbols" / "xbox_structs.tsv",
    ):
        if path.exists():
            yield path
    pdb_index = root / ".claude" / "memory" / "xbox_symbols" / "functions_by_module.txt"
    if pdb_index.exists():
        yield pdb_index
    for path in (
        root / "tools" / "gdl" / "memory_graph" / "schema.sql",
        root / "knowledge" / "memory" / "schema" / "record.schema.json",
    ):
        if path.exists():
            yield path
    yield from _iter_tool_source_paths(root)


def _iter_tool_source_paths(root: Path) -> Iterator[Path]:
    tools_root = root / "tools" / "gdl"
    if not tools_root.exists():
        return
    for path in sorted(tools_root.rglob("*")):
        if (
            path.is_file()
            and path.suffix.lower() in TOOL_SOURCE_SUFFIXES
            and "tests" not in path.parts
            and "__pycache__" not in path.parts
            and ".venv" not in path.parts
        ):
            yield path


def source_fingerprint(root: Path = REPO_ROOT) -> str:
    digest = hashlib.sha256()
    for path in _iter_input_paths(root):
        stat = path.stat()
        digest.update(_repo_relative(root, path).encode("utf-8"))
        digest.update(str(stat.st_size).encode("ascii"))
        digest.update(str(stat.st_mtime_ns).encode("ascii"))
    return digest.hexdigest()


def _open_raw(path: Path, *, readonly: bool = False) -> sqlite3.Connection:
    if readonly:
        connection = sqlite3.connect(f"file:{path.as_posix()}?mode=ro", uri=True)
    else:
        connection = sqlite3.connect(path)
    connection.row_factory = sqlite3.Row
    connection.execute("PRAGMA foreign_keys = ON")
    connection.execute("PRAGMA busy_timeout = 5000")
    return connection


def open_database(
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    *,
    readonly: bool = True,
) -> sqlite3.Connection:
    path = db_path or default_database_path(root)
    if not path.exists():
        raise MemoryGraphError(
            f"memory database does not exist: {path}; run gdlmem.py build"
        )
    return _open_raw(path, readonly=readonly)


def _artifact(
    connection: sqlite3.Connection,
    *,
    key: str,
    platform: str,
    build: str,
    kind: str,
    path: Path | None,
    root: Path,
    provenance: str,
    hash_file: bool = True,
) -> int:
    relative = _repo_relative(root, path) if path else None
    sha256 = _sha256(path) if path and hash_file else None
    connection.execute(
        """
        INSERT INTO source_artifact(
            artifact_key, platform, build_name, artifact_kind,
            path, sha256, provenance_note
        ) VALUES (?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(artifact_key) DO UPDATE SET
            platform=excluded.platform,
            build_name=excluded.build_name,
            artifact_kind=excluded.artifact_kind,
            path=excluded.path,
            sha256=excluded.sha256,
            provenance_note=excluded.provenance_note
        """,
        (key, platform, build, kind, relative, sha256, provenance),
    )
    row = connection.execute(
        "SELECT id FROM source_artifact WHERE artifact_key = ?", (key,)
    ).fetchone()
    assert row is not None
    return int(row["id"])


def _document_title(path: Path, text: str) -> str:
    for line in text.splitlines():
        heading = HEADING_RE.match(line)
        if heading:
            return heading.group(2).strip()
        if line.strip():
            return line.strip()[:160]
    return path.name


def _document_class(path: Path) -> str:
    name = path.name.lower()
    relative = path.as_posix().lower()
    if "xbox_symbols/" in relative:
        return "generated_index"
    if name in {
        "session-ledger.md", "parked.txt", "next-wave-workorders.md", "teed-up-work.md"
    }:
        return "operational_ledger"
    if any(
        token in name
        for token in ("policy", "playbook", "recipes", "codegen-tells", "orchestration")
    ):
        return "workflow_guidance"
    if "symbol" in name or "items_symbol_map" in name:
        return "symbol_research"
    if any(
        token in name
        for token in ("survey", "frontier", "status", "dependency", "campaign", "work")
    ):
        return "campaign_research"
    if "frank" in name or "mwcc" in name or "compiler" in name:
        return "compiler_research"
    return "research_note"


def _chunk_text(
    text: str,
    suffix: str,
    max_chars: int = 2800,
    *,
    filename: str = "",
) -> list[Chunk]:
    lines = text.splitlines()
    chunks: list[Chunk] = []
    heading: str | None = None
    current: list[str] = []
    start = 1

    def flush(end: int) -> None:
        nonlocal current, start
        content = "\n".join(current).strip()
        if content:
            chunks.append(Chunk(len(chunks), heading, start, end, content))
        current = []
        start = end + 1

    parked_mode = filename.lower() == "parked.txt"
    for number, line in enumerate(lines, 1):
        match = HEADING_RE.match(line) if suffix.lower() == ".md" else None
        if match:
            flush(number - 1)
            heading = match.group(2).strip()
            current = [line]
            start = number
            continue
        if parked_mode and line and not line[0].isspace() and not line.startswith("#"):
            flush(number - 1)
            current = [line]
            start = number
            continue
        if suffix.lower() == ".md" and line.startswith("|"):
            flush(number - 1)
            current = [line]
            start = number
            flush(number)
            continue
        projected = sum(len(item) + 1 for item in current) + len(line)
        if current and projected > max_chars:
            flush(number - 1)
            start = number
        current.append(line)
        if not line.strip() and any(item.strip() for item in current[:-1]):
            flush(number)
    flush(len(lines))
    return chunks


def _normalized_chunk_hash(content: str) -> str:
    normalized = re.sub(r"\s+", " ", content).strip().lower()
    return hashlib.sha256(normalized.encode("utf-8")).hexdigest()


def _import_legacy_documents(connection: sqlite3.Connection, root: Path) -> dict[str, int]:
    legacy = root / ".claude" / "memory"
    document_ids: dict[str, int] = {}
    if not legacy.exists():
        return document_ids
    paths: list[Path] = []
    for suffix in ("*.md", "*.txt"):
        paths.extend(legacy.rglob(suffix))
    for path in sorted(set(paths)):
        relative = _repo_relative(root, path)
        text = path.read_text(encoding="utf-8", errors="replace")
        artifact_id = _artifact(
            connection,
            key=f"legacy-document:{relative}",
            platform="project",
            build="working-copy",
            kind="legacy_memory_document",
            path=path,
            root=root,
            provenance="Legacy project memory; indexed verbatim, not automatically verified.",
        )
        cursor = connection.execute(
            """
            INSERT INTO document(
                artifact_id, path, title, format, document_class,
                lifecycle_state, sha256, byte_size
            ) VALUES (?, ?, ?, ?, ?, 'legacy_unreviewed', ?, ?)
            """,
            (
                artifact_id,
                relative,
                _document_title(path, text),
                path.suffix.lower().lstrip(".") or "text",
                _document_class(path),
                _sha256(path),
                path.stat().st_size,
            ),
        )
        document_id = int(cursor.lastrowid)
        document_ids[relative] = document_id
        for chunk in _chunk_text(text, path.suffix, filename=path.name):
            chunk_cursor = connection.execute(
                """
                INSERT INTO document_chunk(
                    document_id, ordinal, heading, line_start, line_end, content,
                    content_sha256, normalized_sha256
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    document_id,
                    chunk.ordinal,
                    chunk.heading,
                    chunk.line_start,
                    chunk.line_end,
                    chunk.content,
                    hashlib.sha256(chunk.content.encode("utf-8")).hexdigest(),
                    _normalized_chunk_hash(chunk.content),
                ),
            )
            connection.execute(
                "INSERT INTO document_chunk_fts VALUES (?, ?, ?, ?)",
                (int(chunk_cursor.lastrowid), relative, chunk.heading, chunk.content),
            )
    return document_ids


def _parse_splits(path: Path) -> list[tuple[int, int, str]]:
    ranges: list[tuple[int, int, str]] = []
    current: str | None = None
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        module = SPLIT_MODULE_RE.match(line)
        if module and not line.startswith((" ", "\t")) and module.group(1) != "Sections":
            current = module.group(1)
            continue
        text_range = SPLIT_TEXT_RE.match(line)
        if current and text_range:
            ranges.append((int(text_range.group(1), 16), int(text_range.group(2), 16), current))
    return ranges


def _module_for_address(ranges: list[tuple[int, int, str]], address: int) -> str | None:
    for start, end, module in ranges:
        if start <= address < end:
            return module
    return None


def _normalize_symbol(name: str) -> str:
    return name.strip().lower()


def _import_gcn_symbols(connection: sqlite3.Connection, root: Path) -> int:
    symbol_path = root / "config" / "GUNE5D" / "symbols.txt"
    splits_path = root / "config" / "GUNE5D" / "splits.txt"
    if not symbol_path.exists():
        return 0
    artifact_id = _artifact(
        connection,
        key="gcn:gune5d:symbol-map",
        platform="gamecube",
        build="GUNE5D",
        kind="symbol_map",
        path=symbol_path,
        root=root,
        provenance="GameCube symbol map maintained by the decompilation project.",
    )
    ranges = _parse_splits(splits_path) if splits_path.exists() else []
    modules: dict[str, int] = {}
    ordinal_by_module: dict[int, int] = {}
    count = 0
    for line in symbol_path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = GCN_SYMBOL_RE.match(line)
        if not match:
            continue
        name, section, address_text, metadata = match.groups()
        fields = dict(META_RE.findall(metadata))
        address = int(address_text, 16)
        module_name = _module_for_address(ranges, address) or f"<section:{section}>"
        if module_name not in modules:
            cursor = connection.execute(
                """
                INSERT INTO binary_module(
                    artifact_id, platform, object_name, source_name, module_ordinal
                ) VALUES (?, 'gamecube', ?, ?, ?)
                """,
                (artifact_id, module_name, module_name, len(modules)),
            )
            modules[module_name] = int(cursor.lastrowid)
        module_id = modules[module_name]
        ordinal = ordinal_by_module.get(module_id, 0)
        ordinal_by_module[module_id] = ordinal + 1
        size = int(fields["size"], 16) if "size" in fields else None
        kind = fields.get("type", "unknown")
        cursor = connection.execute(
            """
            INSERT INTO binary_symbol(
                module_id, artifact_id, platform, symbol_kind, scope,
                raw_name, normalized_name, section, address, size, source_ordinal
            ) VALUES (?, ?, 'gamecube', ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                module_id,
                artifact_id,
                kind,
                fields.get("scope"),
                name,
                _normalize_symbol(name),
                section,
                address,
                size,
                ordinal,
            ),
        )
        connection.execute(
            "INSERT INTO binary_symbol_fts VALUES (?, 'gamecube', ?, ?, ?)",
            (int(cursor.lastrowid), module_name, name, _normalize_symbol(name)),
        )
        count += 1
    return count


def _import_xbox_symbols(connection: sqlite3.Connection, root: Path) -> int:
    index_path = root / ".claude" / "memory" / "xbox_symbols" / "functions_by_module.txt"
    pdb_path = root / "research" / "xbox_symbols" / "shell3D.pdb"
    if not index_path.exists():
        return 0
    artifact_path = pdb_path if pdb_path.exists() else index_path
    artifact_id = _artifact(
        connection,
        key="xbox:shell3d:pdb",
        platform="xbox",
        build="shell3D",
        kind="pdb",
        path=artifact_path,
        root=root,
        provenance=(
            "Locally held Xbox PDB reference. Names, order, sizes, and layouts are "
            "cross-platform evidence, not GameCube authority."
        ),
    )
    current_module: int | None = None
    module_name = ""
    module_ordinal = -1
    symbol_ordinal = 0
    count = 0
    for line in index_path.read_text(encoding="utf-8", errors="replace").splitlines():
        module = PDB_MODULE_RE.match(line)
        if module:
            module_name = module.group(1)
            module_ordinal += 1
            symbol_ordinal = 0
            cursor = connection.execute(
                """
                INSERT INTO binary_module(
                    artifact_id, platform, object_name, source_name, module_ordinal
                ) VALUES (?, 'xbox', ?, ?, ?)
                """,
                (artifact_id, module_name, module.group(2), module_ordinal),
            )
            current_module = int(cursor.lastrowid)
            continue
        symbol = PDB_SYMBOL_RE.match(line)
        if not symbol or current_module is None:
            continue
        segment_text, offset_text, size_text, scope_code, name = symbol.groups()
        scope = {"G": "global", "L": "local", "D": "data"}[scope_code]
        kind = "data" if scope_code == "D" else "function"
        cursor = connection.execute(
            """
            INSERT INTO binary_symbol(
                module_id, artifact_id, platform, symbol_kind, scope,
                raw_name, normalized_name, segment, offset, size, source_ordinal
            ) VALUES (?, ?, 'xbox', ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                current_module,
                artifact_id,
                kind,
                scope,
                name.strip(),
                _normalize_symbol(name),
                int(segment_text),
                int(offset_text, 16),
                int(size_text, 16),
                symbol_ordinal,
            ),
        )
        connection.execute(
            "INSERT INTO binary_symbol_fts VALUES (?, 'xbox', ?, ?, ?)",
            (int(cursor.lastrowid), module_name, name.strip(), _normalize_symbol(name)),
        )
        symbol_ordinal += 1
        count += 1
    return count


def _import_pdb_types(connection: sqlite3.Connection, root: Path) -> tuple[int, int]:
    path = root / "research" / "xbox_symbols" / "xbox_structs.tsv"
    if not path.exists():
        return 0, 0
    artifact_id = _artifact(
        connection,
        key="xbox:shell3d:type-table",
        platform="xbox",
        build="shell3D",
        kind="pdb_type_extract",
        path=path,
        root=root,
        provenance="Deterministic type/field extract derived from the local Xbox PDB.",
    )
    current_type: int | None = None
    type_count = 0
    field_count = 0
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        parts = line.split("\t")
        if not parts:
            continue
        if parts[0] == "S" and len(parts) >= 4:
            name, size_text, category = parts[1:4]
            existing = connection.execute(
                "SELECT id FROM pdb_type WHERE artifact_id=? AND name=? AND category=?",
                (artifact_id, name, category),
            ).fetchone()
            if existing:
                current_type = int(existing["id"])
            else:
                cursor = connection.execute(
                    """
                    INSERT INTO pdb_type(
                        artifact_id, name, type_kind, size, category, source_file
                    ) VALUES (?, ?, 'struct_or_union', ?, ?, ?)
                    """,
                    (artifact_id, name, int(size_text), category, f"{category}.h"),
                )
                current_type = int(cursor.lastrowid)
                type_count += 1
        elif parts[0] == "F" and len(parts) >= 4 and current_type is not None:
            offset_text, size_text, name = parts[1:4]
            connection.execute(
                """
                INSERT INTO pdb_field(type_id, name, byte_offset, byte_size)
                VALUES (?, ?, ?, ?)
                """,
                (current_type, name, int(offset_text), int(size_text)),
            )
            field_count += 1
    return type_count, field_count


def _import_exact_name_candidates(connection: sqlite3.Connection) -> int:
    rows = connection.execute(
        """
        SELECT g.id AS gcn_id, x.id AS xbox_id, g.raw_name AS name
        FROM binary_symbol g
        JOIN binary_symbol x ON x.normalized_name = g.normalized_name
        WHERE g.platform='gamecube' AND x.platform='xbox'
          AND g.symbol_kind='function' AND x.symbol_kind='function'
          AND g.raw_name NOT LIKE 'fn\\_%' ESCAPE '\\'
          AND g.raw_name NOT LIKE 'lbl\\_%' ESCAPE '\\'
        """
    ).fetchall()
    for row in rows:
        connection.execute(
            """
            INSERT OR IGNORE INTO cross_platform_symbol_link(
                gcn_symbol_id, xbox_symbol_id, relation, verification,
                confidence, method, note
            ) VALUES (?, ?, 'probable_equivalent', 'candidate', 0.50,
                      'exact_name', ?)
            """,
            (
                row["gcn_id"],
                row["xbox_id"],
                "Exact spelling only; target behavior and platform applicability remain unverified.",
            ),
        )
    return len(rows)


def _validate_record(record: dict[str, Any], source: Path) -> None:
    required = {"schema_version", "id", "kind"}
    missing = sorted(required - record.keys())
    if missing:
        raise MemoryGraphError(f"{source}: missing record fields: {', '.join(missing)}")
    if record["schema_version"] != SCHEMA_VERSION:
        raise MemoryGraphError(
            f"{source}: schema_version {record['schema_version']} is not {SCHEMA_VERSION}"
        )
    if not isinstance(record["id"], str) or not record["id"]:
        raise MemoryGraphError(f"{source}: record id must be a non-empty string")
    kind = record["kind"]
    allowed = {
        "entity", "edge", "claim", "evidence", "attempt", "work_claim", "tool"
    }
    if kind not in allowed:
        raise MemoryGraphError(f"{source}: unsupported record kind {kind!r}")
    per_kind = {
        "entity": {"entity_type", "key", "name"},
        "edge": {"source", "relation", "target"},
        "claim": {"subject", "predicate", "epistemic_state"},
        "evidence": {"evidence_kind", "locator", "detail"},
        "attempt": {"function", "attempted_axis", "outcome"},
        "work_claim": {"function", "owner", "state", "claimed_at"},
        "tool": {"tool_key", "name", "tool_kind", "status", "purpose"},
    }
    missing = sorted(per_kind[kind] - record.keys())
    if missing:
        raise MemoryGraphError(
            f"{source}: {kind} record missing: {', '.join(missing)}"
        )
    if kind == "claim" and "object" not in record and "value" not in record:
        raise MemoryGraphError(f"{source}: claim needs object or value")
    if kind == "evidence" and "claim" not in record and "edge" not in record:
        raise MemoryGraphError(f"{source}: evidence needs claim or edge")
    anchors: list[str] = []
    attributes = record.get("attributes", {})
    if isinstance(attributes, dict):
        for key in ("evidence", "implementation_anchors", "reference_anchors"):
            value = attributes.get(key, [])
            if isinstance(value, str):
                anchors.append(value)
            elif isinstance(value, list):
                anchors.extend(item for item in value if isinstance(item, str))
    if kind == "evidence" and isinstance(record.get("locator"), str):
        anchors.append(record["locator"])
    for anchor in anchors:
        normalized = anchor.replace("\\", "/").lower()
        if ".claude/memory/" in normalized or re.search(r"\.md(?::\d+)?$", normalized):
            raise MemoryGraphError(
                f"{source}: structured knowledge cannot use Markdown as a truth anchor: {anchor}"
            )


def _entity_id(connection: sqlite3.Connection, key: str) -> int:
    row = connection.execute("SELECT id FROM entity WHERE entity_key=?", (key,)).fetchone()
    if row is None:
        raise MemoryGraphError(f"record references unknown entity {key!r}")
    return int(row["id"])


def _import_records(connection: sqlite3.Connection, root: Path) -> int:
    paths: list[Path] = []
    for relative in (Path("knowledge/memory/records"), Path("knowledge/memory/inbox")):
        directory = root / relative
        if directory.exists():
            paths.extend(directory.rglob("*.json"))
    loaded: list[tuple[Path, dict[str, Any], str]] = []
    for path in sorted(paths):
        record = json.loads(path.read_text(encoding="utf-8"))
        if not isinstance(record, dict):
            raise MemoryGraphError(f"{path}: top-level JSON value must be an object")
        _validate_record(record, path)
        state = "proposed" if "inbox" in path.parts else record.get("record_state", "accepted")
        connection.execute(
            """
            INSERT INTO record_ingest(record_id, record_kind, record_state, source_path, raw_json)
            VALUES (?, ?, ?, ?, ?)
            """,
            (
                record["id"],
                record["kind"],
                state,
                _repo_relative(root, path),
                json.dumps(record, sort_keys=True, separators=(",", ":")),
            ),
        )
        loaded.append((path, record, state))

    # Entities must exist before relationship-bearing records are inserted.
    for _, record, state in loaded:
        if record["kind"] != "entity":
            continue
        cursor = connection.execute(
            """
            INSERT INTO entity(entity_key, entity_type, name, state, attributes_json)
            VALUES (?, ?, ?, ?, ?)
            """,
            (
                record["key"],
                record["entity_type"],
                record["name"],
                record.get("state", state),
                json.dumps(record.get("attributes", {}), sort_keys=True),
            ),
        )
        entity_id = int(cursor.lastrowid)
        aliases = record.get("aliases", [])
        for alias in aliases:
            connection.execute(
                "INSERT INTO entity_alias(entity_id, alias) VALUES (?, ?)",
                (entity_id, alias),
            )
        connection.execute(
            "INSERT INTO entity_fts VALUES (?, ?, ?, ?, ?)",
            (
                entity_id,
                record["key"],
                record["entity_type"],
                record["name"],
                " ".join(aliases),
            ),
        )

    # Relationship-bearing records come next. Evidence is deliberately deferred
    # to a final pass because JSON filename order must not determine whether its
    # referenced claim or edge already exists.
    for _, record, record_state in loaded:
        kind = record["kind"]
        if kind in {"entity", "evidence"}:
            continue
        if kind == "edge":
            connection.execute(
                """
                INSERT INTO edge(
                    record_id, source_entity_id, relation, target_entity_id,
                    state, note, valid_from, valid_to, superseded_by
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    record["id"],
                    _entity_id(connection, record["source"]),
                    record["relation"],
                    _entity_id(connection, record["target"]),
                    record.get("state", "active"),
                    record.get("note"),
                    record.get("valid_from"),
                    record.get("valid_to"),
                    record.get("superseded_by"),
                ),
            )
        elif kind == "claim":
            object_id = _entity_id(connection, record["object"]) if record.get("object") else None
            value_json = json.dumps(record["value"], sort_keys=True) if "value" in record else None
            connection.execute(
                """
                INSERT INTO claim(
                    record_id, subject_entity_id, predicate, object_entity_id,
                    value_json, epistemic_state, note, valid_from, valid_to,
                    superseded_by
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    record["id"],
                    _entity_id(connection, record["subject"]),
                    record["predicate"],
                    object_id,
                    value_json,
                    record["epistemic_state"],
                    record.get("note"),
                    record.get("valid_from"),
                    record.get("valid_to"),
                    record.get("superseded_by"),
                ),
            )
        elif kind == "attempt":
            connection.execute(
                """
                INSERT INTO attempt(
                    record_id, function_entity_id, tu_entity_id,
                    compiler_entity_id, source_revision, attempted_axis,
                    outcome, residual_class, semantic_note, commit_hash,
                    started_at, finished_at
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    record["id"],
                    _entity_id(connection, record["function"]),
                    _entity_id(connection, record["tu"]) if record.get("tu") else None,
                    _entity_id(connection, record["compiler"]) if record.get("compiler") else None,
                    record.get("source_revision"),
                    record["attempted_axis"],
                    record["outcome"],
                    record.get("residual_class"),
                    record.get("semantic_note"),
                    record.get("commit_hash"),
                    record.get("started_at"),
                    record.get("finished_at"),
                ),
            )
            for phase in ("before", "after"):
                measurement = record.get(phase)
                if not measurement:
                    continue
                connection.execute(
                    """
                    INSERT INTO measurement(
                        attempt_record_id, phase, target_instructions,
                        current_instructions, fuzzy_percent, real_diffs,
                        frame_size, project_fuzzy_percent, project_matched_percent
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                    """,
                    (
                        record["id"], phase,
                        measurement.get("target_instructions"),
                        measurement.get("current_instructions"),
                        measurement.get("fuzzy_percent"),
                        measurement.get("real_diffs"),
                        measurement.get("frame_size"),
                        measurement.get("project_fuzzy_percent"),
                        measurement.get("project_matched_percent"),
                    ),
                )
        elif kind == "work_claim":
            connection.execute(
                """
                INSERT INTO work_claim(
                    record_id, function_entity_id, owner, branch, worktree,
                    state, claimed_at, released_at
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    record["id"],
                    _entity_id(connection, record["function"]),
                    record["owner"],
                    record.get("branch"),
                    record.get("worktree"),
                    record["state"],
                    record["claimed_at"],
                    record.get("released_at"),
                ),
            )
        elif kind == "tool":
            usage = record.get("usage", [])
            constraints = record.get("constraints", [])
            source_kind = (
                "reviewed_record" if record_state == "accepted" else "proposal"
            )
            cursor = connection.execute(
                """
                INSERT INTO tool_catalog(
                    record_id, tool_key, name, tool_kind, source_path,
                    entrypoint, status, purpose, usage_json, constraints_json,
                    attributes_json, source_kind, supersedes, valid_from, valid_to
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    record["id"], record["tool_key"], record["name"],
                    record["tool_kind"], record.get("source_path"),
                    record.get("entrypoint"), record["status"], record["purpose"],
                    json.dumps(usage, sort_keys=True),
                    json.dumps(constraints, sort_keys=True),
                    json.dumps(record.get("attributes", {}), sort_keys=True),
                    source_kind,
                    record.get("supersedes"), record.get("valid_from"),
                    record.get("valid_to"),
                ),
            )
            connection.execute(
                "INSERT INTO tool_catalog_fts VALUES (?, ?, ?, ?, ?, ?, ?)",
                (
                    int(cursor.lastrowid), record["tool_key"], record["name"],
                    record.get("source_path"), record["purpose"],
                    " ".join(str(item) for item in usage),
                    " ".join(str(item) for item in constraints),
                ),
            )

    for _, record, _ in loaded:
        if record["kind"] != "evidence":
            continue
        connection.execute(
            """
            INSERT INTO evidence(
                record_id, claim_record_id, edge_record_id, evidence_kind,
                locator, detail, content_sha256
            ) VALUES (?, ?, ?, ?, ?, ?, ?)
            """,
            (
                record["id"],
                record.get("claim"),
                record.get("edge"),
                record["evidence_kind"],
                record["locator"],
                record["detail"],
                record.get("content_sha256"),
            ),
        )
    return len(loaded)


def _python_description(path: Path) -> str:
    try:
        module = ast.parse(path.read_text(encoding="utf-8", errors="replace"))
        return (ast.get_docstring(module) or "").strip()
    except (SyntaxError, OSError):
        return ""


def _tool_key(path: Path) -> str:
    parts = list(path.with_suffix("").parts)
    lowered = [part.lower() for part in parts]
    try:
        gdl_index = lowered.index("gdl")
        parts = parts[gdl_index + 1 :]
    except ValueError:
        pass
    stem = "-".join(parts).lower()
    return "tool:" + re.sub(r"[^a-z0-9]+", "-", stem).strip("-")


def _import_discovered_tools(connection: sqlite3.Connection, root: Path) -> int:
    """Catalog project tools without inventing guidance not present in source."""
    count = 0
    for path in _iter_tool_source_paths(root):
        relative = _repo_relative(root, path)
        key = _tool_key(Path(relative))
        if connection.execute(
            "SELECT 1 FROM tool_catalog WHERE tool_key=? AND source_kind='reviewed_record'",
            (key,),
        ).fetchone():
            continue
        description = _python_description(path) if path.suffix.lower() == ".py" else ""
        first_line = description.splitlines()[0] if description else f"Project tool at {relative}."
        entrypoint = f"python {relative}" if path.suffix.lower() == ".py" else relative
        cursor = connection.execute(
            """
            INSERT INTO tool_catalog(
                tool_key, name, tool_kind, source_path, entrypoint, status,
                purpose, usage_json, constraints_json, attributes_json, source_kind
            ) VALUES (?, ?, ?, ?, ?, 'discovered', ?, '[]', '[]', '{}', 'source_scan')
            """,
            (key, path.stem, path.suffix.lower().lstrip("."), relative, entrypoint, first_line),
        )
        connection.execute(
            "INSERT INTO tool_catalog_fts VALUES (?, ?, ?, ?, ?, '', '')",
            (int(cursor.lastrowid), key, path.stem, relative, first_line),
        )
        count += 1
    return count


def _import_migration_proposals(
    connection: sqlite3.Connection,
    root: Path,
    document_ids: dict[str, int],
) -> int:
    count = 0
    parked_path = root / ".claude" / "memory" / "PARKED.txt"
    parked_relative = _repo_relative(root, parked_path)
    parked_document = document_ids.get(parked_relative)
    if parked_path.exists() and parked_document:
        for number, line in enumerate(
            parked_path.read_text(encoding="utf-8", errors="replace").splitlines(), 1
        ):
            if not line or line[0].isspace() or line.startswith("#"):
                continue
            match = PARKED_ENTRY_RE.match(line)
            if not match:
                continue
            symbol, legacy_status, reason = match.groups()
            payload = {
                "symbol": symbol,
                "legacy_status": legacy_status,
                "inline_reason": reason,
                "asserted": False,
            }
            connection.execute(
                """
                INSERT INTO migration_proposal(
                    proposal_key, proposal_kind, subject_key, title, payload_json,
                    source_document_id, line_start, line_end, review_state
                ) VALUES (?, 'parking_legacy', ?, ?, ?, ?, ?, ?, 'pending')
                """,
                (
                    f"legacy-parking:{number}:{symbol}",
                    f"function:{symbol}",
                    f"Review legacy parking record for {symbol}",
                    json.dumps(payload, sort_keys=True),
                    parked_document,
                    number,
                    number,
                ),
            )
            count += 1

    playbook_path = root / ".claude" / "memory" / "matching-playbook.md"
    playbook_relative = _repo_relative(root, playbook_path)
    playbook_document = document_ids.get(playbook_relative)
    if playbook_path.exists() and playbook_document:
        for number, line in enumerate(
            playbook_path.read_text(encoding="utf-8", errors="replace").splitlines(), 1
        ):
            heading = HEADING_RE.match(line)
            if not heading or "law" not in heading.group(2).lower():
                continue
            title = heading.group(2).strip()
            connection.execute(
                """
                INSERT INTO migration_proposal(
                    proposal_key, proposal_kind, title, payload_json,
                    source_document_id, line_start, line_end, review_state
                ) VALUES (?, 'law_section_legacy', ?, ?, ?, ?, ?, 'pending')
                """,
                (
                    f"legacy-law-section:{number}",
                    f"Review legacy law section: {title}",
                    json.dumps({"heading": title, "asserted": False}, sort_keys=True),
                    playbook_document,
                    number,
                    number,
                ),
            )
            count += 1
    return count


def build_database(
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    *,
    include_legacy: bool = True,
) -> dict[str, Any]:
    """Rebuild the materialized graph atomically and return its statistics."""
    root = root.resolve()
    destination = (db_path or default_database_path(root)).resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    temp_fd, temp_name = tempfile.mkstemp(
        prefix="memory-", suffix=".sqlite", dir=destination.parent
    )
    os.close(temp_fd)
    temp_path = Path(temp_name)
    try:
        connection = _open_raw(temp_path)
        try:
            connection.executescript(SCHEMA_PATH.read_text(encoding="utf-8"))
            connection.execute(
                "INSERT INTO meta(key, value) VALUES ('schema_version', ?)",
                (str(SCHEMA_VERSION),),
            )
            connection.execute(
                "INSERT INTO meta(key, value) VALUES ('build_root', ?)",
                (str(root),),
            )
            connection.execute(
                "INSERT INTO meta(key, value) VALUES ('built_at', ?)", (_utc_now(),)
            )
            connection.execute(
                "INSERT INTO meta(key, value) VALUES ('source_fingerprint', ?)",
                (source_fingerprint(root),),
            )
            record_count = _import_records(connection, root)
            discovered_tool_count = _import_discovered_tools(connection, root)
            document_ids = _import_legacy_documents(connection, root) if include_legacy else {}
            gcn_count = _import_gcn_symbols(connection, root)
            xbox_count = _import_xbox_symbols(connection, root)
            type_count, field_count = _import_pdb_types(connection, root)
            candidate_count = _import_exact_name_candidates(connection)
            proposal_count = _import_migration_proposals(connection, root, document_ids)
            foreign_key_errors = connection.execute("PRAGMA foreign_key_check").fetchall()
            if foreign_key_errors:
                raise MemoryGraphError(
                    f"generated graph has {len(foreign_key_errors)} foreign-key violations"
                )
            integrity = connection.execute("PRAGMA quick_check").fetchone()[0]
            if integrity != "ok":
                raise MemoryGraphError(f"generated graph failed quick_check: {integrity}")
            connection.execute("ANALYZE")
            connection.commit()
        finally:
            connection.close()
        os.replace(temp_path, destination)
    except Exception:
        if temp_path.exists():
            temp_path.unlink()
        raise
    stats = memory_stats(root, destination)
    stats.update(
        {
            "records_imported": record_count,
            "discovered_tools_imported": discovered_tool_count,
            "gcn_symbols_imported": gcn_count,
            "xbox_symbols_imported": xbox_count,
            "pdb_types_imported": type_count,
            "pdb_fields_imported": field_count,
            "exact_name_candidates": candidate_count,
            "migration_proposals_imported": proposal_count,
            "database": str(destination),
        }
    )
    return stats


def ensure_database(root: Path = REPO_ROOT, db_path: Path | None = None) -> Path:
    """Create a missing DB; refresh a DB built from the same checkout if stale."""
    root = root.resolve()
    path = (db_path or default_database_path(root)).resolve()
    if not path.exists():
        build_database(root, path)
        return path
    try:
        with closing(open_database(root, path)) as connection:
            meta = dict(connection.execute("SELECT key, value FROM meta").fetchall())
        if meta.get("schema_version") != str(SCHEMA_VERSION):
            build_database(root, path)
        elif Path(meta.get("build_root", "")).resolve() == root:
            if meta.get("source_fingerprint") != source_fingerprint(root):
                build_database(root, path)
    except (sqlite3.DatabaseError, MemoryGraphError):
        build_database(root, path)
    return path


def memory_stats(root: Path = REPO_ROOT, db_path: Path | None = None) -> dict[str, Any]:
    with closing(open_database(root, db_path)) as connection:
        counts = {}
        for table in (
            "record_ingest", "document", "document_chunk", "entity", "edge",
            "claim", "attempt", "work_claim", "binary_module", "binary_symbol",
            "cross_platform_symbol_link", "pdb_type", "pdb_field",
            "migration_proposal", "tool_catalog",
        ):
            counts[table] = int(connection.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0])
        meta = dict(connection.execute("SELECT key, value FROM meta").fetchall())
    return {"meta": meta, "counts": counts}


def _fts_query(query: str) -> str:
    tokens = re.findall(r"[A-Za-z0-9_:+.~/-]+", query)
    if not tokens:
        raise MemoryGraphError("search query contains no searchable tokens")
    return " AND ".join(f'"{token.replace(chr(34), chr(34) * 2)}"' for token in tokens)


def search_memory(
    query: str,
    *,
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    limit: int = 20,
) -> dict[str, list[dict[str, Any]]]:
    ensure_database(root, db_path)
    fts = _fts_query(query)
    pattern = f"%{query}%"
    with closing(open_database(root, db_path)) as connection:
        documents = [
            dict(row)
            for row in connection.execute(
                """
                SELECT f.path, f.heading, c.line_start, c.line_end,
                       snippet(document_chunk_fts, 3, '[', ']', ' … ', 36) AS snippet,
                       bm25(document_chunk_fts) AS rank
                FROM document_chunk_fts f
                JOIN document_chunk c ON c.id = CAST(f.chunk_id AS INTEGER)
                WHERE document_chunk_fts MATCH ?
                ORDER BY rank LIMIT ?
                """,
                (fts, limit),
            )
        ]
        symbols = [
            dict(row)
            for row in connection.execute(
                """
                SELECT s.id, s.platform, s.raw_name, s.symbol_kind, s.scope,
                       s.address, s.segment, s.offset, s.size,
                       m.object_name AS module
                FROM binary_symbol s
                LEFT JOIN binary_module m ON m.id=s.module_id
                WHERE s.raw_name LIKE ? OR m.object_name LIKE ?
                ORDER BY CASE WHEN lower(s.raw_name)=lower(?) THEN 0 ELSE 1 END,
                         s.platform, s.raw_name
                LIMIT ?
                """,
                (pattern, pattern, query, limit),
            )
        ]
        entities = [
            dict(row)
            for row in connection.execute(
                """
                SELECT entity_key, entity_type, name, state, attributes_json
                FROM entity
                WHERE name LIKE ? OR entity_key LIKE ?
                ORDER BY name LIMIT ?
                """,
                (pattern, pattern, limit),
            )
        ]
    return {"documents": documents, "symbols": symbols, "entities": entities}


def _symbol_row(connection: sqlite3.Connection, query: str, platform: str) -> sqlite3.Row | None:
    if re.fullmatch(r"(?:0x)?[0-9A-Fa-f]{8}", query):
        address = int(query, 16)
        return connection.execute(
            """
            SELECT s.*, m.object_name AS module
            FROM binary_symbol s LEFT JOIN binary_module m ON m.id=s.module_id
            WHERE s.platform=? AND s.address=? ORDER BY s.id LIMIT 1
            """,
            (platform, address),
        ).fetchone()
    return connection.execute(
        """
        SELECT s.*, m.object_name AS module
        FROM binary_symbol s LEFT JOIN binary_module m ON m.id=s.module_id
        WHERE s.platform=? AND lower(s.raw_name)=lower(?)
        ORDER BY CASE WHEN s.symbol_kind='function' THEN 0 ELSE 1 END, s.id LIMIT 1
        """,
        (platform, query),
    ).fetchone()


def _neighbors(connection: sqlite3.Connection, symbol: sqlite3.Row, radius: int = 3) -> list[dict[str, Any]]:
    if symbol["module_id"] is None or symbol["source_ordinal"] is None:
        return []
    return [
        dict(row)
        for row in connection.execute(
            """
            SELECT raw_name, symbol_kind, scope, segment, offset, address, size,
                   source_ordinal
            FROM binary_symbol
            WHERE module_id=? AND source_ordinal BETWEEN ? AND ?
            ORDER BY source_ordinal
            """,
            (
                symbol["module_id"],
                max(0, symbol["source_ordinal"] - radius),
                symbol["source_ordinal"] + radius,
            ),
        )
    ]


def symbol_context(
    symbol_name: str,
    *,
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    document_limit: int = 12,
) -> dict[str, Any]:
    ensure_database(root, db_path)
    with closing(open_database(root, db_path)) as connection:
        gcn = _symbol_row(connection, symbol_name, "gamecube")
        links: list[dict[str, Any]] = []
        xbox_neighbors: dict[str, list[dict[str, Any]]] = {}
        if gcn is not None:
            links = [
                dict(row)
                for row in connection.execute(
                    """
                    SELECT l.relation, l.verification, l.confidence, l.method,
                           l.note, x.raw_name AS xbox_name, x.scope, x.segment,
                           x.offset, x.size, m.object_name AS xbox_module, x.id AS xbox_id
                    FROM cross_platform_symbol_link l
                    JOIN binary_symbol x ON x.id=l.xbox_symbol_id
                    LEFT JOIN binary_module m ON m.id=x.module_id
                    WHERE l.gcn_symbol_id=?
                    ORDER BY l.verification='verified' DESC, l.confidence DESC
                    """,
                    (gcn["id"],),
                )
            ]
            for link in links[:5]:
                xbox = connection.execute(
                    "SELECT * FROM binary_symbol WHERE id=?", (link["xbox_id"],)
                ).fetchone()
                if xbox is not None:
                    xbox_neighbors[f"{link['xbox_module']}::{link['xbox_name']}"] = _neighbors(
                        connection, xbox
                    )
        proposals = [
            dict(row)
            for row in connection.execute(
                """
                SELECT proposal_kind, title, payload_json, line_start, line_end,
                       review_state
                FROM migration_proposal
                WHERE lower(subject_key)=lower(?)
                ORDER BY line_start
                """,
                (f"function:{symbol_name}",),
            )
        ]
        claims: list[dict[str, Any]] = []
        entity = connection.execute(
            "SELECT id FROM entity WHERE lower(entity_key)=lower(?)",
            (f"function:{symbol_name}",),
        ).fetchone()
        if entity:
            claims = [
                dict(row)
                for row in connection.execute(
                    """
                    SELECT c.record_id, r.record_state, c.predicate, c.value_json,
                           c.epistemic_state, c.note, c.valid_from, c.valid_to
                    FROM claim c JOIN record_ingest r ON r.record_id=c.record_id
                    WHERE c.subject_entity_id=?
                    ORDER BY r.record_state='accepted' DESC, c.id DESC
                    """,
                    (entity["id"],),
                )
            ]
    try:
        documents = search_memory(
            symbol_name, root=root, db_path=db_path, limit=document_limit
        )["documents"]
    except MemoryGraphError:
        documents = []
    return {
        "query": symbol_name,
        "gamecube_symbol": dict(gcn) if gcn is not None else None,
        "xbox_links": links,
        "xbox_neighbors": xbox_neighbors,
        "claims": claims,
        "migration_proposals": proposals,
        "legacy_provenance": documents,
        "authority_note": (
            "Xbox symbols and legacy notes are reference evidence. GameCube target "
            "assembly/object data remains authoritative until a link or claim is verified."
        ),
    }


def xbox_symbol_context(
    query: str,
    *,
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    limit: int = 20,
    radius: int = 4,
) -> dict[str, Any]:
    ensure_database(root, db_path)
    pattern = f"%{query}%"
    with closing(open_database(root, db_path)) as connection:
        rows = connection.execute(
            """
            SELECT s.*, m.object_name AS module
            FROM binary_symbol s
            LEFT JOIN binary_module m ON m.id=s.module_id
            WHERE s.platform='xbox'
              AND (s.raw_name LIKE ? OR m.object_name LIKE ?)
            ORDER BY CASE WHEN lower(s.raw_name)=lower(?) THEN 0 ELSE 1 END,
                     m.module_ordinal, s.source_ordinal
            LIMIT ?
            """,
            (pattern, pattern, query, limit),
        ).fetchall()
        matches = [dict(row) for row in rows]
        neighborhoods = {
            f"{row['module']}::{row['raw_name']}": _neighbors(connection, row, radius)
            for row in rows[:5]
        }
        type_rows = [
            dict(row)
            for row in connection.execute(
                """
                SELECT name, size, category, source_file
                FROM pdb_type WHERE name LIKE ? ORDER BY name LIMIT ?
                """,
                (pattern, limit),
            )
        ]
    return {
        "query": query,
        "matches": matches,
        "neighborhoods": neighborhoods,
        "types": type_rows,
        "authority_note": "Candidate evidence only until verified against the GameCube target.",
    }


def migration_proposals(
    *,
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    kind: str | None = None,
    state: str = "pending",
    limit: int = 100,
) -> list[dict[str, Any]]:
    ensure_database(root, db_path)
    with closing(open_database(root, db_path)) as connection:
        if kind:
            rows = connection.execute(
                """
                SELECT p.*, d.path AS source_path
                FROM migration_proposal p JOIN document d ON d.id=p.source_document_id
                WHERE p.review_state=? AND p.proposal_kind=?
                ORDER BY p.id LIMIT ?
                """,
                (state, kind, limit),
            ).fetchall()
        else:
            rows = connection.execute(
                """
                SELECT p.*, d.path AS source_path
                FROM migration_proposal p JOIN document d ON d.id=p.source_document_id
                WHERE p.review_state=? ORDER BY p.id LIMIT ?
                """,
                (state, limit),
            ).fetchall()
    return [dict(row) for row in rows]


def tool_context(
    query: str,
    *,
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    limit: int = 20,
) -> dict[str, Any]:
    ensure_database(root, db_path)
    pattern = f"%{query}%"
    with closing(open_database(root, db_path)) as connection:
        rows = connection.execute(
            """
            SELECT t.*,
                   CASE WHEN EXISTS(
                       SELECT 1 FROM tool_catalog newer WHERE newer.supersedes=t.record_id
                   ) THEN 1 ELSE 0 END AS is_superseded
            FROM tool_catalog t
            WHERE t.name LIKE ? OR t.tool_key LIKE ? OR t.source_path LIKE ?
               OR t.purpose LIKE ? OR t.usage_json LIKE ? OR t.constraints_json LIKE ?
            ORDER BY t.source_kind='reviewed_record' DESC,
                     t.source_kind='proposal' DESC,
                     lower(t.name)=lower(?) DESC, t.id DESC
            LIMIT ?
            """,
            (pattern, pattern, pattern, pattern, pattern, pattern, query, limit),
        ).fetchall()
    tools: list[dict[str, Any]] = []
    for row in rows:
        item = dict(row)
        for field in ("usage_json", "constraints_json", "attributes_json"):
            item[field.removesuffix("_json")] = json.loads(item.pop(field))
        tools.append(item)
    try:
        evidence = search_memory(query, root=root, db_path=db_path, limit=limit)["documents"]
    except MemoryGraphError:
        evidence = []
    return {"query": query, "tools": tools, "legacy_provenance": evidence}


def memory_audit(
    *,
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    duplicate_limit: int = 100,
) -> dict[str, Any]:
    ensure_database(root, db_path)
    with closing(open_database(root, db_path)) as connection:
        documents = [
            dict(row)
            for row in connection.execute(
                """
                SELECT path, title, document_class, lifecycle_state, byte_size
                FROM document ORDER BY path
                """
            )
        ]
        duplicate_documents = [
            dict(row)
            for row in connection.execute(
                """
                SELECT sha256, COUNT(*) AS copies, group_concat(path, ' | ') AS paths
                FROM document GROUP BY sha256 HAVING COUNT(*) > 1
                ORDER BY copies DESC, paths LIMIT ?
                """,
                (duplicate_limit,),
            )
        ]
        duplicate_chunks = [
            dict(row)
            for row in connection.execute(
                """
                SELECT c.normalized_sha256, COUNT(*) AS copies,
                       group_concat(d.path || ':' || c.line_start, ' | ') AS locations,
                       substr(min(c.content), 1, 240) AS sample
                FROM document_chunk c JOIN document d ON d.id=c.document_id
                GROUP BY c.normalized_sha256 HAVING COUNT(*) > 1
                ORDER BY copies DESC, locations LIMIT ?
                """,
                (duplicate_limit,),
            )
        ]
        largest_documents = [
            dict(row)
            for row in connection.execute(
                """
                SELECT path, byte_size, title, document_class
                FROM document ORDER BY byte_size DESC LIMIT 20
                """
            )
        ]
        documents_by_class = {
            row["document_class"]: row["count"]
            for row in connection.execute(
                """
                SELECT document_class, COUNT(*) AS count FROM document
                GROUP BY document_class ORDER BY document_class
                """
            )
        }
        lifecycle_counts = {
            row["lifecycle_state"]: row["count"]
            for row in connection.execute(
                """
                SELECT lifecycle_state, COUNT(*) AS count FROM document
                GROUP BY lifecycle_state ORDER BY lifecycle_state
                """
            )
        }
        proposal_counts = {
            row["proposal_kind"]: row["count"]
            for row in connection.execute(
                """
                SELECT proposal_kind, COUNT(*) AS count FROM migration_proposal
                WHERE review_state='pending' GROUP BY proposal_kind
                """
            )
        }
        tool_counts = {
            row["source_kind"]: row["count"]
            for row in connection.execute(
                "SELECT source_kind, COUNT(*) AS count FROM tool_catalog GROUP BY source_kind"
            )
        }
        records_by_kind = {
            row["record_kind"]: row["count"]
            for row in connection.execute(
                "SELECT record_kind, COUNT(*) AS count FROM record_ingest GROUP BY record_kind"
            )
        }

    # Hyphen/underscore variants have repeatedly accumulated as parallel notes.
    # Surface those families without declaring them duplicates or choosing one.
    filename_families: dict[str, list[str]] = {}
    for document in documents:
        stem = Path(document["path"]).stem.lower()
        family = re.sub(r"[^a-z0-9]+", "", stem)
        filename_families.setdefault(family, []).append(document["path"])
    filename_collisions = [
        {"normalized_name": key, "copies": len(paths), "paths": sorted(paths)}
        for key, paths in sorted(filename_families.items())
        if len(paths) > 1
    ]
    return {
        "document_inventory": documents,
        "documents_by_class": documents_by_class,
        "document_lifecycle": lifecycle_counts,
        "duplicate_documents": duplicate_documents,
        "duplicate_chunks": duplicate_chunks,
        "filename_collisions": filename_collisions,
        "largest_documents": largest_documents,
        "pending_migration_proposals": proposal_counts,
        "tools": tool_counts,
        "records": records_by_kind,
        "interpretation": (
            "Duplicate and filename-family findings are analytical only. "
            "No source document is removed, rewritten, or automatically treated as authoritative."
        ),
    }


def stage_record_proposal(
    record: dict[str, Any],
    *,
    root: Path = REPO_ROOT,
) -> Path:
    """Atomically stage one validated record in the review-required inbox."""
    if not isinstance(record, dict):
        raise MemoryGraphError("proposed record must be a JSON object")
    _validate_record(record, Path("<proposal>"))
    record_id = record["id"]
    for relative in (Path("knowledge/memory/records"), Path("knowledge/memory/inbox")):
        directory = root / relative
        if not directory.exists():
            continue
        for path in directory.rglob("*.json"):
            try:
                existing = json.loads(path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                continue
            if isinstance(existing, dict) and existing.get("id") == record_id:
                raise MemoryGraphError(f"record id {record_id!r} already exists at {path}")
    slug = re.sub(r"[^a-zA-Z0-9._-]+", "-", record_id).strip(".-") or "record"
    destination_dir = root / "knowledge" / "memory" / "inbox"
    destination_dir.mkdir(parents=True, exist_ok=True)
    destination = destination_dir / f"{slug}.json"
    if destination.exists():
        raise MemoryGraphError(f"proposal destination already exists: {destination}")
    temp = destination.with_suffix(f".{uuid.uuid4().hex}.tmp")
    try:
        temp.write_text(
            json.dumps(record, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        os.replace(temp, destination)
    finally:
        if temp.exists():
            temp.unlink()
    return destination


def register_tool_proposal(
    *,
    name: str,
    purpose: str,
    tool_kind: str,
    source_path: str | None = None,
    entrypoint: str | None = None,
    status: str = "active",
    usage: Iterable[str] = (),
    constraints: Iterable[str] = (),
    supersedes: str | None = None,
    root: Path = REPO_ROOT,
) -> Path:
    """Write an atomic, review-required tool record to the migration inbox."""
    slug = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-") or "tool"
    now = datetime.now(timezone.utc)
    record_id = f"tool.{slug}.{now.strftime('%Y%m%dT%H%M%SZ')}.{uuid.uuid4().hex[:8]}"
    record: dict[str, Any] = {
        "schema_version": SCHEMA_VERSION,
        "id": record_id,
        "kind": "tool",
        "tool_key": f"tool:{slug}",
        "name": name,
        "tool_kind": tool_kind,
        "status": status,
        "purpose": purpose,
        "usage": list(usage),
        "constraints": list(constraints),
        "valid_from": now.isoformat(timespec="seconds"),
    }
    if source_path:
        record["source_path"] = source_path
    if entrypoint:
        record["entrypoint"] = entrypoint
    if supersedes:
        record["supersedes"] = supersedes
    return stage_record_proposal(record, root=root)


def validate_records(root: Path = REPO_ROOT) -> dict[str, Any]:
    paths: list[Path] = []
    for relative in (Path("knowledge/memory/records"), Path("knowledge/memory/inbox")):
        directory = root / relative
        if directory.exists():
            paths.extend(directory.rglob("*.json"))
    ids: set[str] = set()
    for path in sorted(paths):
        record = json.loads(path.read_text(encoding="utf-8"))
        if not isinstance(record, dict):
            raise MemoryGraphError(f"{path}: top-level JSON value must be an object")
        _validate_record(record, path)
        if record["id"] in ids:
            raise MemoryGraphError(f"duplicate record id {record['id']!r} at {path}")
        ids.add(record["id"])
    return {"valid": True, "record_count": len(paths), "schema_version": SCHEMA_VERSION}
