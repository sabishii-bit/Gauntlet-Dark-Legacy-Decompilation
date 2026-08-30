"""Build and query the GDL project-memory graph.

The SQLite file is a disposable materialized view. Durable reviewed facts live
as JSON records under memory_graph/records; legacy notes are preserved and
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
from typing import Any, Iterable, Iterator, Mapping


SCHEMA_VERSION = 1
PACKAGE_DIR = Path(__file__).resolve().parent
REPO_ROOT = PACKAGE_DIR.parent
SCHEMA_PATH = PACKAGE_DIR / "schema.sql"
RECORDS_DIR = REPO_ROOT / "memory_graph" / "records"
INBOX_DIR = REPO_ROOT / "memory_graph" / "inbox"

# Attempt records may grow to hold real forensics, but each function keeps at
# most ATTEMPT_LIMIT_PER_FUNCTION accepted attempt records; older ones are
# ejected by prune_attempts() (git history retains them).
ATTEMPT_BYTE_CAP = 16384
ATTEMPT_LIMIT_PER_FUNCTION = 5

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
    legacy = root / "memory_graph" / "legacy"
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
    pdb_index = root / "research" / "xbox_symbols" / "functions_by_module.txt"
    if pdb_index.exists():
        yield pdb_index
    for path in (
        root / "memory_graph" / "schema.sql",
        root / "memory_graph" / "schema" / "record.schema.json",
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
    legacy = root / "memory_graph" / "legacy"
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
    index_path = root / "research" / "xbox_symbols" / "functions_by_module.txt"
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
        version = record["schema_version"]
        raise MemoryGraphError(
            f"{source}: schema_version must be the JSON number {SCHEMA_VERSION},"
            f" got {type(version).__name__} {version!r}"
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
    if kind == "attempt":
        encoded = json.dumps(record, sort_keys=True).encode("utf-8")
        if len(encoded) > ATTEMPT_BYTE_CAP:
            raise MemoryGraphError(
                f"{source}: attempt record is {len(encoded)} bytes (cap"
                f" {ATTEMPT_BYTE_CAP}); keep the do-not-retry head compact —"
                " fold history into one-line axis_log entries and put deep"
                " forensics in an evidence record or the commit itself"
            )
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
        if ".claude/memory/" in normalized or "memory_graph/legacy/" in normalized or re.search(r"\.md(?::\d+)?$", normalized):
            raise MemoryGraphError(
                f"{source}: structured knowledge cannot use Markdown as a truth anchor: {anchor}"
            )


def _entity_id(connection: sqlite3.Connection, key: str) -> int:
    row = connection.execute("SELECT id FROM entity WHERE entity_key=?", (key,)).fetchone()
    if row is not None:
        return int(row["id"])
    resolved = _autoresolve_entity(connection, key)
    if resolved is not None:
        return resolved
    raise MemoryGraphError(f"record references unknown entity {key!r}")


def _autoresolve_entity(connection: sqlite3.Connection, key: str) -> int | None:
    """Materialize a minimal entity from the deterministic symbol/module import.

    `function:<raw_name>` resolves against the imported GameCube symbol table
    and `tu:<module>` against the imported module table (with or without the
    source extension), so records never have to duplicate symbol facts just to
    satisfy referential checks. Explicit entity records remain the way to add
    curated attributes, and ambiguous names still require one.
    """
    if key.startswith("function:"):
        name = key.split(":", 1)[1]
        rows = connection.execute(
            "SELECT raw_name, address, size FROM binary_symbol"
            " WHERE platform='gamecube' AND raw_name=? AND symbol_kind='function'",
            (name,),
        ).fetchall()
        if len(rows) > 1:
            raise MemoryGraphError(
                f"{key!r} matches {len(rows)} GameCube symbols; add an explicit"
                " entity record to disambiguate"
            )
        if len(rows) == 1:
            attributes = {
                "auto_resolved_from": "gamecube_symbol_import",
                "gamecube_address": hex(rows[0]["address"]) if rows[0]["address"] else None,
                "size_bytes": hex(rows[0]["size"]) if rows[0]["size"] else None,
            }
            return _insert_auto_entity(connection, key, "function", name, attributes)
        return None
    if key.startswith("tu:"):
        name = key.split(":", 1)[1]
        rows = connection.execute(
            "SELECT object_name FROM binary_module"
            " WHERE platform='gamecube' AND (object_name=? OR object_name=? OR object_name=?)",
            (name, name + ".c", name + ".cpp"),
        ).fetchall()
        if rows:
            attributes = {
                "auto_resolved_from": "gamecube_module_import",
                "object_name": rows[0]["object_name"],
            }
            return _insert_auto_entity(connection, key, "translation_unit", name, attributes)
        return None
    return None


def _insert_auto_entity(
    connection: sqlite3.Connection,
    key: str,
    entity_type: str,
    name: str,
    attributes: dict[str, Any],
) -> int:
    cursor = connection.execute(
        """
        INSERT INTO entity(entity_key, entity_type, name, state, attributes_json)
        VALUES (?, ?, ?, 'active', ?)
        """,
        (key, entity_type, name, json.dumps(attributes, sort_keys=True)),
    )
    entity_id = int(cursor.lastrowid)
    connection.execute(
        "INSERT INTO entity_fts VALUES (?, ?, ?, ?, ?)",
        (entity_id, key, entity_type, name, ""),
    )
    return entity_id


def _import_records(connection: sqlite3.Connection, root: Path) -> int:
    paths: list[Path] = []
    for relative in (Path("memory_graph/records"), Path("memory_graph/inbox")):
        directory = root / relative
        if directory.exists():
            paths.extend(directory.rglob("*.json"))
    loaded: list[tuple[Path, dict[str, Any], str]] = []
    rejected: list[dict[str, str]] = []

    def _record_fts_body(record: Mapping[str, object]) -> str:
        # Flatten every authored key and scalar so any phrase an author wrote
        # in a record (law text, attempt axes, residual notes, locators) is
        # reachable through full-text search.
        parts: list[str] = []

        def walk(value: object) -> None:
            if isinstance(value, Mapping):
                for key, item in sorted(value.items()):
                    if key == "schema_version":
                        continue
                    parts.append(str(key))
                    walk(item)
            elif isinstance(value, (list, tuple)):
                for item in value:
                    walk(item)
            elif value is not None:
                parts.append(str(value))

        walk(record)
        return " ".join(parts)

    for path in sorted(paths):
        is_inbox = "inbox" in path.parts
        try:
            record = json.loads(path.read_text(encoding="utf-8-sig"))
            if not isinstance(record, dict):
                raise MemoryGraphError(f"{path}: top-level JSON value must be an object")
            _validate_record(record, path)
        except (MemoryGraphError, json.JSONDecodeError) as error:
            # A malformed inbox proposal must never take down the whole graph:
            # reject it, keep building, and surface it in the build stats.
            if not is_inbox:
                raise
            rejected.append(
                {
                    "path": _repo_relative(root, path),
                    "record_id": "",
                    "error": str(error),
                }
            )
            continue
        state = "proposed" if is_inbox else record.get("record_state", "accepted")
        connection.execute(
            """
            INSERT INTO record_ingest(record_id, record_kind, record_state,
                                      source_path, raw_json, valid_from, recorded_at)
            VALUES (?, ?, ?, ?, ?, ?, ?)
            """,
            (
                record["id"],
                record["kind"],
                state,
                _repo_relative(root, path),
                json.dumps(record, sort_keys=True, separators=(",", ":")),
                record.get("valid_from"),
                record.get("recorded_at"),
            ),
        )
        connection.execute(
            "INSERT INTO record_fts VALUES (?, ?, ?, ?)",
            (record["id"], record["kind"], state, _record_fts_body(record)),
        )
        loaded.append((path, record, state))

    # Entities must exist before relationship-bearing records are inserted.
    for path, record, state in loaded:
        if record["kind"] != "entity":
            continue
        connection.execute("SAVEPOINT record_insert")
        try:
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
        except (MemoryGraphError, sqlite3.Error) as error:
            connection.execute("ROLLBACK TO record_insert")
            connection.execute("RELEASE record_insert")
            if "inbox" not in path.parts:
                raise
            connection.execute(
                "DELETE FROM record_ingest WHERE record_id=?", (record["id"],)
            )
            connection.execute(
                "DELETE FROM record_fts WHERE record_id=?", (record["id"],)
            )
            rejected.append(
                {
                    "path": _repo_relative(root, path),
                    "record_id": record["id"],
                    "error": str(error),
                }
            )
            continue
        connection.execute("RELEASE record_insert")

    # Relationship-bearing records come next. Evidence is deliberately deferred
    # to a final pass because JSON filename order must not determine whether its
    # referenced claim or edge already exists.
    for path, record, record_state in loaded:
        kind = record["kind"]
        if kind in {"entity", "evidence"}:
            continue
        connection.execute("SAVEPOINT record_insert")
        try:
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
                attributes = record.get("attributes")
                laws_applied = (
                    attributes.get("laws_applied")
                    if isinstance(attributes, dict) else None
                )
                if isinstance(laws_applied, list):
                    for law_id in laws_applied:
                        if isinstance(law_id, str) and law_id:
                            connection.execute(
                                "INSERT OR IGNORE INTO attempt_law_application"
                                " (attempt_record_id, law_record_id)"
                                " VALUES (?, ?)",
                                (record["id"], law_id),
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
        except (MemoryGraphError, sqlite3.Error) as error:
            connection.execute("ROLLBACK TO record_insert")
            connection.execute("RELEASE record_insert")
            if "inbox" not in path.parts:
                raise
            connection.execute(
                "DELETE FROM record_ingest WHERE record_id=?", (record["id"],)
            )
            connection.execute(
                "DELETE FROM record_fts WHERE record_id=?", (record["id"],)
            )
            rejected.append(
                {
                    "path": _repo_relative(root, path),
                    "record_id": record["id"],
                    "error": str(error),
                }
            )
            continue
        connection.execute("RELEASE record_insert")

    for path, record, _ in loaded:
        if record["kind"] != "evidence":
            continue
        connection.execute("SAVEPOINT record_insert")
        try:
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
        except (MemoryGraphError, sqlite3.Error) as error:
            connection.execute("ROLLBACK TO record_insert")
            connection.execute("RELEASE record_insert")
            if "inbox" not in path.parts:
                raise
            connection.execute(
                "DELETE FROM record_ingest WHERE record_id=?", (record["id"],)
            )
            connection.execute(
                "DELETE FROM record_fts WHERE record_id=?", (record["id"],)
            )
            rejected.append(
                {
                    "path": _repo_relative(root, path),
                    "record_id": record["id"],
                    "error": str(error),
                }
            )
            continue
        connection.execute("RELEASE record_insert")
    remaining = connection.execute(
        "SELECT COUNT(*) FROM record_ingest"
    ).fetchone()[0]
    return int(remaining), rejected


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
    parked_path = root / "memory_graph" / "legacy" / "PARKED.txt"
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

    playbook_path = root / "memory_graph" / "legacy" / "matching-playbook.md"
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
            # Symbols import first so record references can resolve against
            # the deterministic GameCube symbol/module tables.
            gcn_count = _import_gcn_symbols(connection, root)
            record_count, inbox_rejected = _import_records(connection, root)
            connection.execute(
                "INSERT INTO meta(key, value) VALUES ('inbox_rejected', ?)",
                (json.dumps(inbox_rejected, sort_keys=True),),
            )
            discovered_tool_count = _import_discovered_tools(connection, root)
            document_ids = _import_legacy_documents(connection, root) if include_legacy else {}
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
            "inbox_rejected": inbox_rejected,
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
    overflow = {
        function: len(rows)
        for function, rows in _accepted_attempts_by_function(root).items()
        if len(rows) > ATTEMPT_LIMIT_PER_FUNCTION
    }
    if overflow:
        stats["attempt_overflow"] = overflow
        stats["attempt_overflow_hint"] = (
            f"functions above the {ATTEMPT_LIMIT_PER_FUNCTION}-attempt cap;"
            " run gdlmem.py prune-attempts (dry-run) then --apply"
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
        elif meta.get("source_fingerprint") != source_fingerprint(root):
            # Worktrees share the database under the Git common directory.
            # A database materialized from a sibling worktree is safe to read
            # only when its complete input fingerprint matches this checkout.
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
        records = [
            {
                **dict(row),
                "age_days": _record_age_days(row["valid_from"], row["recorded_at"]),
            }
            for row in connection.execute(
                """
                SELECT record_fts.record_id, record_fts.record_kind,
                       record_fts.record_state,
                       r.valid_from, r.recorded_at,
                       snippet(record_fts, 3, '[', ']', ' … ', 36) AS snippet,
                       bm25(record_fts) AS rank
                FROM record_fts
                JOIN record_ingest r ON r.record_id = record_fts.record_id
                WHERE record_fts MATCH ?
                ORDER BY CASE record_fts.record_state
                         WHEN 'accepted' THEN 0 ELSE 1 END,
                         rank
                LIMIT ?
                """,
                (fts, limit),
            )
        ]
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
    return {
        "records": records,
        "documents": documents,
        "symbols": symbols,
        "entities": entities,
    }


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
        attempts: list[dict[str, Any]] = []
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
            # Compact projection: the do-not-retry payload only. Full
            # forensic attributes stay in the record; fetch them on demand
            # with the `record <id>` operation so briefings stay small.
            for row in connection.execute(
                """
                SELECT a.record_id, r.record_state, a.attempted_axis, a.outcome,
                       a.residual_class, a.commit_hash, r.raw_json
                FROM attempt a JOIN record_ingest r ON r.record_id=a.record_id
                WHERE a.function_entity_id=?
                  AND NOT EXISTS (
                      SELECT 1 FROM record_ingest newer
                      WHERE json_extract(newer.raw_json, '$.supersedes') = a.record_id
                        AND newer.record_state = 'accepted'
                  )
                ORDER BY r.record_state='accepted' DESC, a.id DESC
                """,
                (entity["id"],),
            ):
                attempt = dict(row)
                raw = json.loads(attempt.pop("raw_json") or "{}")
                axis = attempt.get("attempted_axis") or ""
                if len(axis) > 300:
                    attempt["attempted_axis"] = axis[:297] + "..."
                if raw.get("attributes"):
                    attempt["detail"] = f"gdlmem.py record {attempt['record_id']}"
                attempts.append(attempt)
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
        "attempts": attempts,
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


def xbox_struct_layout(
    query: str,
    *,
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    offset: str | None = None,
    limit: int = 8,
) -> dict[str, Any]:
    """PDB struct field layout, pad-gap analysis, and offset-to-field lookup.

    Answers the two questions that keep raw-offset casts out of reconstructed
    source: "which field sits at +0xNN of this struct?" and "exactly how many
    bytes of padding fill the hole between two known fields?".
    """
    ensure_database(root, db_path)
    off_val: int | None = None
    if offset is not None and str(offset).strip() != "":
        off_val = int(str(offset).strip(), 0)
    pattern = f"%{query}%"
    with closing(open_database(root, db_path)) as connection:
        type_rows = connection.execute(
            """
            SELECT id, name, type_kind, size, category, source_file
            FROM pdb_type WHERE name LIKE ?
            ORDER BY CASE WHEN lower(name)=lower(?) THEN 0 ELSE 1 END,
                     length(name), name
            LIMIT ?
            """,
            (pattern, query, limit),
        ).fetchall()
        types: list[dict[str, Any]] = []
        for trow in type_rows:
            field_rows = connection.execute(
                """
                SELECT name, field_type, byte_offset, bit_offset, bit_size,
                       byte_size, array_count
                FROM pdb_field WHERE type_id=?
                ORDER BY byte_offset, bit_offset
                """,
                (trow["id"],),
            ).fetchall()
            fields: list[dict[str, Any]] = []
            gaps: list[dict[str, Any]] = []
            prev_end = 0
            prev_name = "<start>"
            for frow in field_rows:
                start = frow["byte_offset"]
                if start is None:
                    continue
                size = frow["byte_size"] or 0
                if start > prev_end:
                    gaps.append({
                        "after_field": prev_name,
                        "before_field": frow["name"],
                        "start": hex(prev_end),
                        "size": start - prev_end,
                    })
                field = {
                    "name": frow["name"],
                    "offset": hex(start),
                    "size": size or None,
                    "type": frow["field_type"],
                }
                if frow["array_count"]:
                    field["array_count"] = frow["array_count"]
                if frow["bit_size"]:
                    field["bit_offset"] = frow["bit_offset"]
                    field["bit_size"] = frow["bit_size"]
                fields.append(field)
                prev_end = max(prev_end, start + size)
                prev_name = frow["name"]
            struct_size = trow["size"]
            if struct_size and struct_size > prev_end:
                gaps.append({
                    "after_field": prev_name,
                    "before_field": "<end>",
                    "start": hex(prev_end),
                    "size": struct_size - prev_end,
                })
            entry: dict[str, Any] = {
                "name": trow["name"],
                "kind": trow["type_kind"],
                "size": struct_size,
                "size_hex": hex(struct_size) if struct_size else None,
                "category": trow["category"],
                "source_file": trow["source_file"],
                "fields": fields,
                "pad_gaps": gaps,
            }
            if off_val is not None:
                hit = None
                for frow in field_rows:
                    start = frow["byte_offset"]
                    if start is None:
                        continue
                    size = frow["byte_size"] or 0
                    if start <= off_val < start + max(size, 1):
                        hit = {
                            "field": frow["name"],
                            "field_offset": hex(start),
                            "delta_into_field": off_val - start,
                            "type": frow["field_type"],
                        }
                        break
                if hit is None:
                    for gap in gaps:
                        gstart = int(gap["start"], 16)
                        if gstart <= off_val < gstart + gap["size"]:
                            hit = {"in_pad_gap": gap}
                            break
                entry["offset_lookup"] = {
                    "offset": hex(off_val),
                    "result": hit or "outside recorded layout",
                }
            types.append(entry)
    return {
        "query": query,
        "offset": hex(off_val) if off_val is not None else None,
        "types": types,
        "authority_note": (
            "Xbox PDB layouts are cross-platform reference evidence; the GC"
            " record may be more compact. Verify offsets against GameCube"
            " target displacements before adopting names."
        ),
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


def _reference_resolvable(connection: sqlite3.Connection, key: str) -> bool:
    row = connection.execute(
        "SELECT id FROM entity WHERE entity_key=?", (key,)
    ).fetchone()
    if row is not None:
        return True
    if key.startswith("function:"):
        name = key.split(":", 1)[1]
        count = connection.execute(
            "SELECT COUNT(*) FROM binary_symbol"
            " WHERE platform='gamecube' AND raw_name=? AND symbol_kind='function'",
            (name,),
        ).fetchone()[0]
        return int(count) == 1
    if key.startswith("tu:"):
        name = key.split(":", 1)[1]
        count = connection.execute(
            "SELECT COUNT(*) FROM binary_module"
            " WHERE platform='gamecube' AND (object_name=? OR object_name=? OR object_name=?)",
            (name, name + ".c", name + ".cpp"),
        ).fetchone()[0]
        return int(count) >= 1
    return False


def _probe_record_references(
    record: dict[str, Any], root: Path, db_path: Path | None = None
) -> None:
    """Run the same reference resolution the build applies, before staging.

    A proposal that the build would reject must never reach the inbox: the
    build is fail-soft about inbox errors, but the proposer should learn about
    a bad reference immediately, with the build's own error text.
    """
    kind = record.get("kind")
    entity_refs: list[str] = []
    if kind == "edge":
        entity_refs = [record["source"], record["target"]]
    elif kind == "claim":
        entity_refs = [record["subject"]]
        if record.get("object"):
            entity_refs.append(record["object"])
    elif kind in {"attempt", "work_claim"}:
        entity_refs = [record["function"]]
        for optional in ("tu", "compiler"):
            if record.get(optional):
                entity_refs.append(record[optional])
    elif kind != "evidence":
        return
    ensure_database(root, db_path)
    with closing(open_database(root, db_path)) as connection:
        for key in entity_refs:
            if not _reference_resolvable(connection, key):
                raise MemoryGraphError(
                    f"proposal references unknown entity {key!r}; use an existing"
                    " entity key, or a `function:<symbol>`/`tu:<module>` name that"
                    " resolves against the GameCube symbol import"
                )
        if kind == "evidence":
            table = "claim" if record.get("claim") else "edge"
            target = record.get("claim") or record.get("edge")
            row = connection.execute(
                f"SELECT 1 FROM {table} WHERE record_id=?", (target,)
            ).fetchone()
            if row is None:
                raise MemoryGraphError(
                    f"proposal references unknown {table} record {target!r}"
                )


def stage_record_proposal(
    record: dict[str, Any],
    *,
    root: Path = REPO_ROOT,
) -> Path:
    """Atomically stage one validated record in the review-required inbox."""
    if not isinstance(record, dict):
        raise MemoryGraphError("proposed record must be a JSON object")
    now = datetime.now(timezone.utc)
    # Freshness stamps: valid_from is the author's semantic date (defaulted to
    # today), recorded_at is always the actual staging moment.
    record.setdefault("valid_from", now.strftime("%Y-%m-%d"))
    record["recorded_at"] = now.strftime("%Y-%m-%dT%H:%M:%SZ")
    if record.get("kind") == "attempt":
        attributes = record.get("attributes")
        law_screen = (
            attributes.get("law_screen") if isinstance(attributes, dict) else None
        )
        if not isinstance(law_screen, str) or not law_screen.strip():
            raise MemoryGraphError(
                "attempt proposals must carry attributes.law_screen: name the"
                " law records screened for this pass and whether each applied"
                " (run `gdlmem.py laws` for the current corpus); an explicit"
                " 'none applicable: <why>' is acceptable"
            )
    _validate_record(record, Path("<proposal>"))
    _probe_record_references(record, root)
    record_id = record["id"]
    for relative in (Path("memory_graph/records"), Path("memory_graph/inbox")):
        directory = root / relative
        if not directory.exists():
            continue
        for path in directory.rglob("*.json"):
            try:
                existing = json.loads(path.read_text(encoding="utf-8-sig"))
            except (OSError, json.JSONDecodeError):
                continue
            if isinstance(existing, dict) and existing.get("id") == record_id:
                raise MemoryGraphError(f"record id {record_id!r} already exists at {path}")
    slug = re.sub(r"[^a-zA-Z0-9._-]+", "-", record_id).strip(".-") or "record"
    destination_dir = root / "memory_graph" / "inbox"
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


def _accepted_attempts_by_function(root: Path) -> dict[str, list[dict[str, Any]]]:
    """Group accepted attempt records (records/, not inbox/) by anchor function."""
    attempts_dir = root / "memory_graph" / "records"
    grouped: dict[str, list[dict[str, Any]]] = {}
    if not attempts_dir.exists():
        return grouped
    for path in sorted(attempts_dir.rglob("*.json")):
        try:
            record = json.loads(path.read_text(encoding="utf-8-sig"))
        except (OSError, json.JSONDecodeError):
            continue
        if not isinstance(record, dict) or record.get("kind") != "attempt":
            continue
        function = record.get("function")
        if not isinstance(function, str):
            continue
        grouped.setdefault(function, []).append(
            {
                "id": record.get("id"),
                "path": path,
                "valid_from": record.get("valid_from") or "",
                "outcome": record.get("outcome"),
                "supersedes": record.get("supersedes"),
            }
        )
    return grouped


def prune_attempts(
    root: Path = REPO_ROOT,
    *,
    limit: int = ATTEMPT_LIMIT_PER_FUNCTION,
    apply: bool = False,
) -> dict[str, Any]:
    """Enforce the per-function attempt cap by ejecting the oldest records.

    For every anchor function holding more than ``limit`` accepted attempt
    records, keep the newest ``limit`` (superseded records are ejected before
    live ones regardless of age) and eject the rest. Dry-run by default:
    the report lists each ejection's id/outcome so the integrator can fold a
    still-live do-not-retry cap into the surviving record before ``apply``.
    Ejected files are deleted from the working tree only — git history keeps
    them recoverable. Rebuild the graph after an applied prune.
    """
    if limit < 1:
        raise MemoryGraphError(f"attempt limit must be >= 1, got {limit}")
    grouped = _accepted_attempts_by_function(root)
    superseded_ids = {
        row["supersedes"]
        for rows in grouped.values()
        for row in rows
        if row["supersedes"]
    }
    ejected: list[dict[str, Any]] = []
    kept: dict[str, list[str]] = {}
    for function, rows in sorted(grouped.items()):
        if len(rows) <= limit:
            continue
        # Newest first; superseded records sort behind live ones.
        rows.sort(
            key=lambda row: (
                row["id"] not in superseded_ids,
                row["valid_from"],
                row["id"] or "",
            ),
            reverse=True,
        )
        kept[function] = [row["id"] for row in rows[:limit]]
        for row in rows[limit:]:
            ejected.append(
                {
                    "function": function,
                    "id": row["id"],
                    "outcome": row["outcome"],
                    "valid_from": row["valid_from"],
                    "superseded": row["id"] in superseded_ids,
                    "file": str(row["path"].relative_to(root)),
                }
            )
            if apply:
                row["path"].unlink()
    result: dict[str, Any] = {
        "limit": limit,
        "functions_over_limit": len(kept),
        "kept": kept,
        "ejected": ejected,
        "applied": apply,
    }
    if ejected and not apply:
        result["next"] = (
            "review each ejection (fold any still-live cap into the newest"
            " record), then rerun with --apply, commit the deletions, and"
            " rebuild the graph"
        )
    elif ejected and apply:
        result["next"] = "commit the deletions and run gdlmem.py build"
    return result


_KIND_DIRS = {
    "attempt": "attempts",
    "claim": "claims",
    "evidence": "evidence",
    "entity": "entities",
    "edge": "entities",
    "tool": "tools",
}


def accept_records(
    record_ids: Iterable[str],
    *,
    release: Iterable[str] = (),
    root: Path = REPO_ROOT,
) -> dict[str, Any]:
    """Integrator acceptance: move inbox records into records/, delete released
    claims, and stage everything with git (pathspec-limited by construction).

    Moves each named record from memory_graph/inbox/ to the records/ directory
    for its kind, deletes each released work_claim's inbox file, stages exactly
    the touched paths when the root is a git checkout, rebuilds the graph, and
    returns the paths plus a ready-made pathspec-limited commit command. Fails
    closed: unknown ids, non-inbox records, and destination collisions abort
    before anything is moved.
    """
    record_ids = list(record_ids)
    release = list(release)
    if not record_ids and not release:
        raise MemoryGraphError("nothing to accept: pass record ids or --release")
    inbox = root / "memory_graph" / "inbox"
    index: dict[str, tuple[Path, dict[str, Any]]] = {}
    if inbox.exists():
        for path in sorted(inbox.glob("*.json")):
            try:
                record = json.loads(path.read_text(encoding="utf-8-sig"))
            except (OSError, json.JSONDecodeError):
                continue
            if isinstance(record, dict) and isinstance(record.get("id"), str):
                index[record["id"]] = (path, record)
    moves: list[tuple[Path, Path]] = []
    for record_id in record_ids:
        if record_id not in index:
            raise MemoryGraphError(
                f"record {record_id!r} not found in memory_graph/inbox"
            )
        path, record = index[record_id]
        kind = record.get("kind")
        if kind == "work_claim":
            raise MemoryGraphError(
                f"{record_id!r} is a work_claim: release it with --release,"
                " never accept it into records/"
            )
        subdir = _KIND_DIRS.get(kind)
        if subdir is None:
            raise MemoryGraphError(f"{record_id!r} has unsupported kind {kind!r}")
        destination = root / "memory_graph" / "records" / subdir / path.name
        if destination.exists():
            raise MemoryGraphError(f"destination already exists: {destination}")
        moves.append((path, destination))
    releases: list[Path] = []
    for claim_id in release:
        if claim_id not in index:
            raise MemoryGraphError(
                f"claim {claim_id!r} not found in memory_graph/inbox"
            )
        path, record = index[claim_id]
        if record.get("kind") != "work_claim":
            raise MemoryGraphError(
                f"{claim_id!r} is kind {record.get('kind')!r}, not a work_claim"
            )
        releases.append(path)
    touched: list[str] = []
    for source, destination in moves:
        destination.parent.mkdir(parents=True, exist_ok=True)
        os.replace(source, destination)
        touched.extend(
            [str(source.relative_to(root)), str(destination.relative_to(root))]
        )
    for path in releases:
        path.unlink()
        touched.append(str(path.relative_to(root)))
    staged = False
    if (root / ".git").exists():
        subprocess.run(
            ["git", "-C", str(root), "add", "--"] + touched,
            check=True, capture_output=True, text=True,
        )
        staged = True
    build_database(root)
    quoted = " ".join(f'"{path}"' for path in touched)
    return {
        "accepted": [record_id for record_id in record_ids],
        "released": release,
        "paths": touched,
        "staged": staged,
        "graph_rebuilt": True,
        "commit_command": f'git commit -m "<message>" -- {quoted}',
    }


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


@dataclass(frozen=True)
class SurfaceParam:
    """One parameter of a query-surface operation."""

    name: str
    annotation: type | object
    default: Any = None
    required: bool = False
    maximum: int | None = None
    help: str = ""


@dataclass(frozen=True)
class SurfaceOp:
    """One read operation exposed by every consumer (CLI, MCP, future hosts).

    `call(root, db_path, **params)` is the single implementation; consumers
    generate their argument surfaces from `params` instead of hand-mirroring
    each other.
    """

    name: str
    mcp_name: str
    doc: str
    call: Any
    params: tuple[SurfaceParam, ...] = ()

    def clamped(self, values: dict[str, Any]) -> dict[str, Any]:
        out: dict[str, Any] = {}
        for param in self.params:
            value = values.get(param.name, param.default)
            if param.maximum is not None and isinstance(value, int):
                value = max(1, min(value, param.maximum))
            out[param.name] = value
        return out


def _stats_surface(root: Path, db_path: Path | None) -> dict[str, Any]:
    path = ensure_database(root, db_path)
    return {"database": str(path), **memory_stats(root, path)}


def record_lookup(
    record_id: str, *, root: Path = REPO_ROOT, db_path: Path | None = None
) -> dict[str, Any]:
    """Return one record's full JSON by id (the on-demand detail fetch)."""
    ensure_database(root, db_path)
    with closing(open_database(root, db_path)) as connection:
        row = connection.execute(
            "SELECT record_state, source_path, raw_json FROM record_ingest"
            " WHERE record_id=?",
            (record_id,),
        ).fetchone()
    if row is None:
        raise MemoryGraphError(f"no record with id {record_id!r}")
    return {
        "record_state": row["record_state"],
        "source_path": row["source_path"],
        "record": json.loads(row["raw_json"]),
    }


def _record_age_days(valid_from: str | None, recorded_at: str | None) -> int | None:
    # recorded_at (the machine-stamped staging moment) supersedes the
    # author-claimed valid_from for freshness judgments.
    stamp = (recorded_at or "")[:10] or valid_from
    if not stamp:
        return None
    try:
        then = datetime.strptime(stamp, "%Y-%m-%d").date()
    except ValueError:
        return None
    return (datetime.now(timezone.utc).date() - then).days


def law_corpus(
    query: str | None = None,
    *,
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    limit: int = 100,
) -> dict[str, Any]:
    """List the codegen-law corpus, newest first, with freshness and supersession."""
    ensure_database(root, db_path)
    sql = """
        SELECT r.record_id, r.record_state, r.valid_from, r.recorded_at,
               c.epistemic_state, c.value_json,
               json_extract(r.raw_json, '$.attributes.scope') AS scope,
               COALESCE(
                   c.superseded_by,
                   (SELECT newer.record_id FROM record_ingest newer
                    WHERE json_extract(newer.raw_json, '$.supersedes') = r.record_id
                    LIMIT 1)
               ) AS superseded_by,
               (SELECT COUNT(*) FROM attempt_law_application ala
                WHERE ala.law_record_id = r.record_id) AS applied_count
        FROM claim c JOIN record_ingest r ON r.record_id = c.record_id
        WHERE (c.predicate = 'codegen_law' OR r.record_id LIKE '%law%')
    """
    params: list[Any] = []
    if query:
        sql += (
            " AND (r.record_id LIKE ? OR c.value_json LIKE ?"
            " OR json_extract(r.raw_json, '$.attributes.scope') LIKE ?)"
        )
        pattern = f"%{query}%"
        params.extend([pattern, pattern, pattern])
    sql += " ORDER BY COALESCE(r.valid_from, '') DESC, r.record_id LIMIT ?"
    params.append(limit)
    with closing(open_database(root, db_path)) as connection:
        rows = connection.execute(sql, params).fetchall()
    laws = []
    for row in rows:
        value = row["value_json"] or ""
        try:
            value = json.loads(value)
        except (TypeError, json.JSONDecodeError):
            pass
        if isinstance(value, str) and len(value) > 300:
            value = value[:300] + " …[gdlmem.py record <id> for full text]"
        laws.append(
            {
                "id": row["record_id"],
                "state": row["record_state"],
                "epistemic_state": row["epistemic_state"],
                "valid_from": row["valid_from"],
                "recorded_at": row["recorded_at"],
                "age_days": _record_age_days(row["valid_from"], row["recorded_at"]),
                "applied_count": row["applied_count"],
                "superseded_by": row["superseded_by"],
                "scope": row["scope"],
                "head": value,
            }
        )
    return {
        "laws": laws,
        "count": len(laws),
        "note": (
            "laws are compiler-scoped observations, not instructions:"
            " re-verify against your target bytes; a superseded_by entry means"
            " read the newer record instead"
        ),
    }


def work_claims(
    *,
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    stale_after: int = 2,
    include_released: int = 0,
) -> dict[str, Any]:
    """List work claims with owner, scope, age, and a stale flag.

    Active claims older than ``stale_after`` days are flagged stale per the
    AGENTS.md stale-claim rule; released/done claims are hidden unless
    ``include_released`` is nonzero.
    """
    ensure_database(root, db_path)
    with closing(open_database(root, db_path)) as connection:
        rows = connection.execute(
            """
            SELECT w.record_id, w.owner, w.state, w.claimed_at, w.released_at,
                   e.name AS function,
                   r.valid_from, r.recorded_at,
                   json_extract(r.raw_json, '$.attributes.scope') AS scope
            FROM work_claim w
            JOIN entity e ON e.id = w.function_entity_id
            JOIN record_ingest r ON r.record_id = w.record_id
            ORDER BY w.claimed_at, w.owner
            """
        ).fetchall()
    claims = []
    stale_count = 0
    for row in rows:
        released = row["released_at"] is not None or row["state"] in (
            "released", "done"
        )
        if released and not include_released:
            continue
        age = _record_age_days(row["claimed_at"], row["recorded_at"])
        stale = bool(not released and age is not None and age > stale_after)
        stale_count += stale
        claims.append(
            {
                "id": row["record_id"],
                "function": row["function"],
                "owner": row["owner"],
                "state": row["state"],
                "claimed_at": row["claimed_at"],
                "age_days": age,
                "stale": stale,
                "scope": row["scope"],
            }
        )
    return {
        "claims": claims,
        "count": len(claims),
        "stale_count": stale_count,
        "stale_after_days": stale_after,
        "note": (
            "a stale claim means the owner has gone quiet: per AGENTS.md,"
            " confirm the owner is really gone (branch activity, inbox files)"
            " before treating the scope as free"
        ),
    }


_DEBT_CAST_RE = re.compile(
    r"\*\s*\(\s*(?:[us](?:8|16|32|64)|f32|f64|int|char|short|long|float|double"
    r"|void\s*\*|\w+\s*\*)\s*\*?\s*\)\s*\("
)
_DEBT_PF_RE = re.compile(r"\bPF\s*\(")


def fakematch_debt(
    tu: str | None = None,
    *,
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    limit: int = 40,
) -> dict[str, Any]:
    """Census raw-offset fakematch debt per TU, heaviest first.

    Counts broad raw-cast sites (``*(T*)(...)``) and PF() macro sites in each
    C/C++ file under src/. Pass ``tu`` to filter to paths containing it. The
    scan runs against the working tree at query time, so the result is always
    current; the generated_at stamp makes saved copies comparable over time.
    """
    src = root / "src"
    if not src.exists():
        raise MemoryGraphError(f"no src/ directory under {root}")
    rows = []
    for path in sorted(src.rglob("*.c*")):
        if path.suffix.lower() not in (".c", ".cpp"):
            continue
        relative = str(path.relative_to(root)).replace("\\", "/")
        if tu and tu.lower() not in relative.lower():
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        casts = len(_DEBT_CAST_RE.findall(text))
        pf_sites = len(_DEBT_PF_RE.findall(text))
        if casts or pf_sites:
            rows.append(
                {"tu": relative, "cast_sites": casts, "pf_sites": pf_sites,
                 "total": casts + pf_sites}
            )
    rows.sort(key=lambda row: (-row["total"], row["tu"]))
    total = sum(row["total"] for row in rows)
    return {
        "tus": rows[:limit],
        "tu_count": len(rows),
        "site_total": total,
        "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "note": (
            "broad census: includes legitimate raw forms (protected webs,"
            " structless pools) — read the TU's attempt records before"
            " claiming; counts are for wave planning, not a to-zero target"
        ),
    }


def build_surface_ops() -> tuple[SurfaceOp, ...]:
    """The registry every query consumer derives its surface from."""
    return (
        SurfaceOp(
            name="stats", mcp_name="memory_graph_stats",
            doc="Show graph build metadata and row counts.",
            call=lambda root, db, **kw: _stats_surface(root, db),
        ),
        SurfaceOp(
            name="search", mcp_name="memory_search",
            doc="Search accepted records, legacy evidence, symbols, and entities.",
            call=lambda root, db, **kw: search_memory(
                kw["query"], root=root, db_path=db, limit=kw["limit"]),
            params=(
                SurfaceParam("query", str, required=True,
                             help="AND-combined search terms"),
                SurfaceParam("limit", int, default=20, maximum=100),
            ),
        ),
        SurfaceOp(
            name="laws", mcp_name="memory_law_corpus",
            doc=("List the codegen-law corpus (newest first) with scope, "
                 "age, application counts, and supersession flags."),
            call=lambda root, db, **kw: law_corpus(
                kw["query"], root=root, db_path=db, limit=kw["limit"]),
            params=(
                SurfaceParam("query", str, default=None,
                             help="optional filter over id/scope/law text"),
                SurfaceParam("limit", int, default=100, maximum=200),
            ),
        ),
        SurfaceOp(
            name="claims", mcp_name="memory_work_claims",
            doc=("List work claims with owner, scope, age, and stale flags "
                 "for cross-fleet coordination."),
            call=lambda root, db, **kw: work_claims(
                root=root, db_path=db, stale_after=kw["stale_after"],
                include_released=kw["include_released"]),
            params=(
                SurfaceParam("stale_after", int, default=2, maximum=30,
                             help="days before an active claim is stale"),
                SurfaceParam("include_released", int, default=0, maximum=1,
                             help="1 to include released/done claims"),
            ),
        ),
        SurfaceOp(
            name="debt", mcp_name="fakematch_debt_census",
            doc=("Census raw-offset fakematch debt per TU (heaviest first) "
                 "for wave planning."),
            call=lambda root, db, **kw: fakematch_debt(
                kw["tu"], root=root, db_path=db, limit=kw["limit"]),
            params=(
                SurfaceParam("tu", str, default=None,
                             help="optional path substring filter"),
                SurfaceParam("limit", int, default=40, maximum=200),
            ),
        ),
        SurfaceOp(
            name="context", mcp_name="memory_context",
            doc=("Assemble GameCube, Xbox, claim, attempt, and provenance "
                 "context for a symbol."),
            call=lambda root, db, **kw: symbol_context(
                kw["symbol"], root=root, db_path=db,
                document_limit=kw["document_limit"]),
            params=(
                SurfaceParam("symbol", str, required=True),
                SurfaceParam("document_limit", int, default=12, maximum=50),
            ),
        ),
        SurfaceOp(
            name="xbox", mcp_name="xbox_context",
            doc="Search Xbox PDB symbols/types and show module neighbors.",
            call=lambda root, db, **kw: xbox_symbol_context(
                kw["query"], root=root, db_path=db,
                limit=kw["limit"], radius=kw["radius"]),
            params=(
                SurfaceParam("query", str, required=True),
                SurfaceParam("limit", int, default=20, maximum=100),
                SurfaceParam("radius", int, default=4, maximum=20),
            ),
        ),
        SurfaceOp(
            name="struct", mcp_name="xbox_struct_layout",
            doc=("Show a PDB struct's field layout and pad gaps; optional "
                 "offset locates the covering field."),
            call=lambda root, db, **kw: xbox_struct_layout(
                kw["query"], root=root, db_path=db,
                offset=kw["offset"], limit=kw["limit"]),
            params=(
                SurfaceParam("query", str, required=True,
                             help="struct/union/enum name (LIKE match)"),
                SurfaceParam("offset", str, default=None,
                             help="byte offset to resolve (0x hex or decimal)"),
                SurfaceParam("limit", int, default=8, maximum=50),
            ),
        ),
        SurfaceOp(
            name="tool", mcp_name="memory_tool_context",
            doc="Return reviewed tool policy, discovered tools, and provenance.",
            call=lambda root, db, **kw: tool_context(
                kw["query"], root=root, db_path=db, limit=kw["limit"]),
            params=(
                SurfaceParam("query", str, required=True),
                SurfaceParam("limit", int, default=20, maximum=100),
            ),
        ),
        SurfaceOp(
            name="audit", mcp_name="memory_migration_audit",
            doc="Report duplicates and migration coverage without deleting anything.",
            call=lambda root, db, **kw: memory_audit(
                root=root, db_path=db, duplicate_limit=kw["duplicate_limit"]),
            params=(
                SurfaceParam("duplicate_limit", int, default=100, maximum=500),
            ),
        ),
        SurfaceOp(
            name="proposals", mcp_name="memory_pending_proposals",
            doc="List unreviewed migration proposals.",
            call=lambda root, db, **kw: migration_proposals(
                root=root, db_path=db, kind=kw["kind"], state=kw["state"],
                limit=kw["limit"]),
            params=(
                SurfaceParam("kind", str, default=None),
                SurfaceParam("state", str, default="pending"),
                SurfaceParam("limit", int, default=100, maximum=500),
            ),
        ),
        SurfaceOp(
            name="record", mcp_name="memory_record",
            doc="Fetch one record's full JSON by id (on-demand attempt detail).",
            call=lambda root, db, **kw: record_lookup(
                kw["record_id"], root=root, db_path=db),
            params=(
                SurfaceParam("record_id", str, required=True),
            ),
        ),
        SurfaceOp(
            name="stale", mcp_name="memory_stale",
            doc="Compare parked/capped attempts against the current objdiff report.",
            call=lambda root, db, **kw: attempt_staleness(root, db),
        ),
        SurfaceOp(
            name="validate", mcp_name="memory_validate",
            doc="Validate durable and inbox records, including reference resolution.",
            call=lambda root, db, **kw: validate_records(root),
        ),
    )


def attempt_staleness(
    root: Path = REPO_ROOT, db_path: Path | None = None
) -> dict[str, Any]:
    """Compare parked/capped attempts against the current objdiff report.

    A park is moot when its function measures fully matched WITHOUT a guarded
    postprocessor rule; such records should be removed or superseded. A park
    on a function far below the opcode-complete profile is suspect and should
    be re-triaged rather than trusted.
    """
    report_path = root / "build" / "GUNE5D" / "report.json"
    if not report_path.exists():
        raise MemoryGraphError(
            "build/GUNE5D/report.json not found; run"
            " `ninja build/GUNE5D/report.json` first"
        )
    report = json.loads(report_path.read_text(encoding="utf-8"))
    scores: dict[str, float] = {}
    address_suffixed_scores: dict[str, list[tuple[str, float]]] = {}
    for unit in report.get("units", []):
        for function in unit.get("functions", []):
            report_name = function["name"]
            score = float(function.get("fuzzy_match_percent", 0.0))
            scores[report_name] = score
            match = re.fullmatch(r"(.+)_([0-9A-Fa-f]{8})", report_name)
            if match:
                address_suffixed_scores.setdefault(match.group(1), []).append(
                    (report_name, score)
                )
    covered: set[str] = {"regFind"}  # P6Frank carrier
    webfrank = root / "config" / "GUNE5D" / "webfrank.json"
    if webfrank.exists():
        covered.update(
            re.findall(
                r'"function"\s*:\s*"([^"]+)"',
                webfrank.read_text(encoding="utf-8"),
            )
        )
    ensure_database(root, db_path)
    with closing(open_database(root, db_path)) as connection:
        rows = connection.execute(
            "SELECT a.record_id, e.name, a.attempted_axis, r.raw_json,"
            " (SELECT m.fuzzy_percent FROM measurement m"
            "  WHERE m.attempt_record_id = a.record_id"
            "  ORDER BY CASE m.phase WHEN 'after' THEN 0 ELSE 1 END"
            "  LIMIT 1) AS recorded_fuzzy"
            " FROM attempt a"
            " JOIN entity e ON e.id = a.function_entity_id"
            " JOIN record_ingest r ON r.record_id = a.record_id"
            " WHERE a.outcome IN ('parked', 'capped')"
            " AND NOT EXISTS (SELECT 1 FROM record_ingest newer"
            " WHERE json_extract(newer.raw_json, '$.supersedes') = a.record_id"
            " AND newer.record_state = 'accepted')"
        ).fetchall()
    with closing(open_database(root, db_path)) as connection:
        multi_rows = connection.execute(
            "SELECT e.name, COUNT(*) AS n, GROUP_CONCAT(a.record_id) AS ids"
            " FROM attempt a JOIN entity e ON e.id = a.function_entity_id"
            " WHERE NOT EXISTS (SELECT 1 FROM record_ingest newer"
            " WHERE json_extract(newer.raw_json, '$.supersedes') = a.record_id"
            " AND newer.record_state = 'accepted')"
            " GROUP BY a.function_entity_id HAVING COUNT(*) > 1"
        ).fetchall()
    multi = [
        {"function": row["name"], "records": row["ids"].split(",")}
        for row in multi_rows
    ]
    with closing(open_database(root, db_path)) as connection:
        claim_rows = connection.execute(
            "SELECT w.record_id, w.owner, w.state, w.claimed_at, e.name"
            " FROM work_claim w JOIN entity e ON e.id = w.function_entity_id"
            " WHERE w.released_at IS NULL AND w.state NOT IN"
            " ('released', 'done')"
            " ORDER BY w.claimed_at, w.owner"
        ).fetchall()
    today = datetime.now(timezone.utc).date()
    claims: list[dict[str, Any]] = []
    for row in claim_rows:
        age_days: int | None = None
        try:
            age_days = (today - datetime.strptime(
                row["claimed_at"][:10], "%Y-%m-%d").date()).days
        except (ValueError, TypeError):
            pass
        claims.append(
            {
                "function": row["name"],
                "record": row["record_id"],
                "owner": row["owner"],
                "state": row["state"],
                "claimed_at": row["claimed_at"],
                "age_days": age_days,
                "presumed_abandoned": age_days is not None and age_days > 1,
            }
        )
    stale: list[dict[str, Any]] = []
    walls: list[dict[str, Any]] = []
    suspect: list[dict[str, Any]] = []
    missing: list[dict[str, Any]] = []
    reopen: list[dict[str, Any]] = []
    valid = 0
    form_terms = ("offsetof", "typed alias", "repeated cast", "inline cast",
                  "declared alias")
    for row in rows:
        name = row["name"]
        score = scores.get(name)
        if score is None:
            aliases = address_suffixed_scores.get(name, [])
            if len(aliases) == 1:
                _, score = aliases[0]
        if score is None:
            missing.append({"function": name, "record": row["record_id"]})
            continue
        if score >= 100.0:
            entry = {"function": name, "record": row["record_id"], "fuzzy": score}
            (walls if name in covered else stale).append(entry)
            continue
        # Re-open heuristics for parks that are neither moot nor walls:
        # (a) the function's score moved since the park was measured — some
        #     later change disturbed it, so the wall may have shifted;
        # (b) the park's text never documents which source FORM failed —
        #     per claim.law.offsetof-overturns-typed-alias-caps such caps
        #     must be re-tried with offsetof-on-raw-pointer before trust.
        recorded = row["recorded_fuzzy"]
        if recorded is not None and abs(float(recorded) - score) > 0.05:
            reopen.append(
                {"function": name, "record": row["record_id"],
                 "reason": "score_moved_since_park",
                 "recorded_fuzzy": float(recorded), "current_fuzzy": score}
            )
        else:
            body = ((row["attempted_axis"] or "") + (row["raw_json"] or "")).lower()
            if not any(term in body for term in form_terms):
                reopen.append(
                    {"function": name, "record": row["record_id"],
                     "reason": "failing_form_undocumented",
                     "current_fuzzy": score}
                )
        if score < 70.0:
            suspect.append(
                {"function": name, "record": row["record_id"], "fuzzy": score}
            )
        else:
            valid += 1
    return {
        "stale_solved": stale,
        "postprocessor_walls": walls,
        "suspect_low_fuzzy": suspect,
        "missing_from_report": missing,
        "reopen_candidates": reopen,
        "multi_record_functions": multi,
        "active_work_claims": claims,
        "valid_count": valid,
        "note": (
            "stale_solved parks are moot (function fully matched without a"
            " postprocessor rule): remove or supersede them."
            " postprocessor_walls are valid source-level walls."
            " suspect_low_fuzzy and missing_from_report need re-triage."
            " multi_record_functions should be consolidated into one live"
            " attempt record per function (fold prior axes into axis_log)."
            " reopen_candidates: score_moved_since_park means later work"
            " disturbed the function (re-probe cheaply);"
            " failing_form_undocumented means the cap predates form-recording"
            " — per claim.law.offsetof-overturns-typed-alias-caps, re-try the"
            " offsetof-on-raw-pointer form before trusting it."
            " active_work_claims presumed_abandoned entries are older than"
            " one day: verify via git log against the claimed scope, then"
            " remove the claim in a standalone cleanup commit (AGENTS.md"
            " cross-fleet concurrency)."
        ),
    }


def validate_records(root: Path = REPO_ROOT) -> dict[str, Any]:
    paths: list[Path] = []
    for relative in (Path("memory_graph/records"), Path("memory_graph/inbox")):
        directory = root / relative
        if directory.exists():
            paths.extend(directory.rglob("*.json"))
    ids: set[str] = set()
    records: list[tuple[Path, dict[str, Any]]] = []
    for path in sorted(paths):
        record = json.loads(path.read_text(encoding="utf-8-sig"))
        if not isinstance(record, dict):
            raise MemoryGraphError(f"{path}: top-level JSON value must be an object")
        _validate_record(record, path)
        if record["id"] in ids:
            raise MemoryGraphError(f"duplicate record id {record['id']!r} at {path}")
        ids.add(record["id"])
        records.append((path, record))
    # Mirror the build's reference resolution so `validate` never blesses a
    # record the build would reject. Uses the existing database when present;
    # skipped (reported) when no graph has been built yet.
    reference_errors: list[dict[str, str]] = []
    references_checked = False
    database = default_database_path(root)
    if database.exists():
        references_checked = True
        for path, record in records:
            try:
                _probe_record_references(record, root, database)
            except MemoryGraphError as error:
                reference_errors.append(
                    {"path": _repo_relative(root, path), "error": str(error)}
                )
    if reference_errors:
        return {
            "valid": False,
            "record_count": len(paths),
            "schema_version": SCHEMA_VERSION,
            "references_checked": references_checked,
            "reference_errors": reference_errors,
        }
    return {
        "valid": True,
        "record_count": len(paths),
        "schema_version": SCHEMA_VERSION,
        "references_checked": references_checked,
    }
