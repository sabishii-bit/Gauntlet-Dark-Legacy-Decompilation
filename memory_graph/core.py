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

# Controlled applicability vocabulary for law records (attributes.tags).
# `laws` reports live per-tag counts; proposals with tags outside this set
# fail closed so the vocabulary cannot drift silently. Extend it here, in one
# reviewed change, when a genuinely new pattern class emerges.
LAW_TAG_VOCABULARY = frozenset({
    "core-screen",
    # pattern classes
    "walked-pointer", "alias-form", "offsetof-form", "lwzu-fusion",
    "index-form", "entry-schedule", "register-web", "sda-global",
    "param-retype", "cse", "decl-order", "store-placement", "switch-form",
    "symbol-identity", "relocation", "inline", "peephole", "pool-layout",
    # context classes
    "defake", "matching", "metrics", "batch-gating", "postprocessor",
    "workflow", "build-hygiene",
})

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
    """Return a per-checkout database path.

    The DB is a derived cache of records/ (the source of truth), so a linked
    worktree gets its OWN copy under build/ — the old worktree-shared path
    caused sqlite rename collisions between parallel fleet workers, and the
    `git rev-parse --git-common-dir` it relied on returns a POSIX-form path
    under MSYS git that Windows Path arithmetic mangles into a bogus
    location (both failure modes observed in the field, 2026-08-31).
    """
    git_marker = root / ".git"
    if git_marker.is_file():  # linked worktree: isolated derived cache
        return root / "build" / "gdlmem" / "memory.sqlite"
    if git_marker.is_dir():  # main checkout: same location as before
        return git_marker / "gdl-memory" / "memory.sqlite"
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
    # generous: concurrent fleet workers share this DB when git common-dir
    # resolution succeeds; a build's atomic replace can hold it for seconds
    connection.execute("PRAGMA busy_timeout = 20000")
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
    flat = json.dumps(record)
    if "<REQUIRED" in flat or "<OPTIONAL" in flat:
        raise MemoryGraphError(
            f"{source}: template placeholders remain — fill every <REQUIRED:...>"
            " field and delete unused <OPTIONAL:...> keys before proposing"
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
        # Acceptance is by LOCATION: a file under records/ is accepted no
        # matter what a stale record_state field claims (superseded v1 files
        # were found still carrying "proposed" after their inbox move).
        state = "proposed" if is_inbox else "accepted"
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
    result_hint = None
    if not matches and not type_rows:
        result_hint = (
            "no PDB symbol/type matched this name — the struct authority may"
            " be project-local: try `search \"<name>\"` (headers, prior"
            " attempt records, and sibling-TU conversions are indexed there)"
            " before concluding nothing exists or inventing a name"
        )
    return {
        "query": query,
        "matches": matches,
        "neighborhoods": neighborhoods,
        "types": type_rows,
        "hint": result_hint,
        "authority_note": "Candidate evidence only until verified against the GameCube target.",
    }


_LOCAL_OFFSET_COMMENT_RE = re.compile(r"/\*\s*(0[xX][0-9A-Fa-f]+)")


def _local_header_structs(
    root: Path, query: str, off_val: int | None
) -> list[dict[str, Any]]:
    """GC-verified project headers are the FIRST authority for struct names.

    Scans include/ for a struct/typedef block matching ``query`` exactly
    (case-insensitive) and parses the project's `/* 0xNN */` offset-comment
    convention; with an offset, reports the covering field line. This is
    what resolves `struct Player --offset 0x956` to include/game/player.h
    instead of an unrelated PDB substring match.
    """
    include = root / "include"
    results: list[dict[str, Any]] = []
    if not include.exists():
        return results
    name = re.escape(query)
    open_re = re.compile(rf"(?:struct|union)\s+{name}\s*\{{", re.I)
    close_re = re.compile(rf"\}}\s*{name}\s*;", re.I)
    for path in sorted(include.rglob("*.h")):
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if not re.search(rf"\b{name}\b", text, re.I):
            continue
        block_start = block_end = None
        open_match = open_re.search(text)
        if open_match:
            depth = 0
            for index in range(open_match.end() - 1, len(text)):
                if text[index] == "{":
                    depth += 1
                elif text[index] == "}":
                    depth -= 1
                    if depth == 0:
                        block_start, block_end = open_match.start(), index
                        break
        else:
            close_match = close_re.search(text)
            if close_match:
                depth = 0
                for index in range(close_match.start(), -1, -1):
                    if text[index] == "}":
                        depth += 1
                    elif text[index] == "{":
                        depth -= 1
                        if depth == 0:
                            block_start, block_end = index, close_match.start()
                            break
        if block_start is None:
            continue
        entry: dict[str, Any] = {
            "file": str(path.relative_to(root)).replace("\\", "/"),
            "line": text.count("\n", 0, block_start) + 1,
            "authority": "GC project header (primary — PDB is reference only)",
        }
        if off_val is not None:
            best_line = None
            best_off = -1
            block = text[block_start:block_end]
            base_line = text.count("\n", 0, block_start)
            for line_index, line in enumerate(block.splitlines()):
                comment = _LOCAL_OFFSET_COMMENT_RE.search(line)
                if not comment:
                    continue
                field_off = int(comment.group(1), 16)
                if best_off < field_off <= off_val:
                    best_off = field_off
                    best_line = (base_line + line_index + 1, line.strip())
            if best_line:
                entry["covering_field"] = {
                    "line": best_line[0],
                    "text": best_line[1][:160],
                    "field_offset": hex(best_off),
                    "delta_into_field": hex(off_val - best_off),
                }
            else:
                entry["covering_field"] = None
        results.append(entry)
        if len(results) >= 3:
            break
    return results


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
    bytes of padding fill the hole between two known fields?". Project-local
    headers under include/ are checked FIRST and reported as the primary
    authority; the Xbox PDB layout follows as cross-platform reference.
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
                "match_kind": ("exact" if trow["name"].lower() == query.lower()
                               else "substring"),
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
    local_headers = _local_header_structs(root, query, off_val)
    if local_headers:
        hint = ("a GC project header defines this struct — that is the"
                " authority; the PDB entries below are reference only")
    elif not types:
        hint = (
            "no PDB struct matched this name — PDB lookup needs the Xbox"
            " type name, which often differs from the GC symbol; grep"
            " research/xbox_symbols/misc.h for the struct NAME (the tsv"
            " index is incomplete) and try `search \"<name>\"` for"
            " project-local authority before leaving sites raw"
        )
    elif not any(entry["match_kind"] == "exact" for entry in types):
        hint = (
            f"NO type is named exactly {query!r} — every entry below is a"
            " SUBSTRING match and may be unrelated (a past worker was misled"
            " by exactly this). Project-local structs (include/game headers,"
            " e.g. the GC Player) are not in the PDB: verify via"
            " `search \"<name>\"` before trusting any of these"
        )
    else:
        hint = None
    return {
        "query": query,
        "offset": hex(off_val) if off_val is not None else None,
        "local_headers": local_headers,
        "types": types,
        "hint": hint,
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
    # Record-id citations must resolve at proposal time. Handoff quality
    # depends on a successor being able to fetch every cited record in one
    # `gdlmem record` call; a typoed or stale id rots silently otherwise
    # (an unresolvable law-id citation shipped in a run brief before a
    # worker caught it by hand). `supersedes` and the structured
    # `attributes.laws_applied` list are both checked; free-text mentions
    # in law_screen stay advisory.
    cited: list[str] = []
    for citing_key in ("supersedes", "refutes"):
        if isinstance(record.get(citing_key), str):
            cited.append(record[citing_key])
    laws_applied = (
        record.get("attributes", {}).get("laws_applied")
        if isinstance(record.get("attributes"), dict) else None
    )
    if isinstance(laws_applied, str):
        try:
            laws_applied = json.loads(laws_applied)
        except json.JSONDecodeError:
            raise MemoryGraphError(
                "attributes.laws_applied must be a JSON list of record ids"
                " (or a JSON-encoded string of one)"
            )
    if laws_applied is not None:
        if (not isinstance(laws_applied, list)
                or not all(isinstance(law, str) for law in laws_applied)):
            raise MemoryGraphError(
                "attributes.laws_applied must be a JSON list of record ids"
            )
        cited.extend(laws_applied)
    ensure_database(root, db_path)
    with closing(open_database(root, db_path)) as connection:
        for key in entity_refs:
            if not _reference_resolvable(connection, key):
                raise MemoryGraphError(
                    f"proposal references unknown entity {key!r}; use an existing"
                    " entity key, or a `function:<symbol>`/`tu:<module>` name that"
                    " resolves against the GameCube symbol import"
                )
        for cited_id in cited:
            if cited_id == record.get("id"):
                raise MemoryGraphError("a record cannot cite itself")
            row = connection.execute(
                "SELECT 1 FROM record_ingest WHERE record_id=?", (cited_id,)
            ).fetchone()
            if row is None:
                raise MemoryGraphError(
                    f"cited record id {cited_id!r} does not resolve (check"
                    " supersedes / attributes.laws_applied for typos, or"
                    " rebuild the graph if the record is new)"
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


def record_template(kind: str) -> dict[str, Any]:
    """Emit a correctly-shaped skeleton for a proposable record kind.

    Placeholders are self-describing; staging rejects any record still
    carrying a ``<REQUIRED:...>`` or ``<OPTIONAL:...>`` marker, so a
    half-filled template cannot slip into the inbox. ``valid_from`` and
    ``recorded_at`` are stamped automatically at staging — never author them.
    """
    templates: dict[str, dict[str, Any]] = {
        "attempt": {
            "function": "<REQUIRED: function:symbol_name>",
            "attempted_axis": "<REQUIRED: one-line description of the axis tried>",
            "outcome": "<REQUIRED: improved|neutral|negative|parked|capped>",
            "residual_class": "<OPTIONAL: NONE|REGISTER_ONLY|SCHEDULE|STRUCTURAL|MIXED>",
            "supersedes": "<OPTIONAL: id of the prior attempt record this replaces>",
            "refutes": "<OPTIONAL: id of a record whose mechanism/framing this"
                       " attempt DISPROVED by measurement — distinct from"
                       " supersedes: the refuted record may belong to another"
                       " function or be a law>",
            "attributes": {
                "law_screen": "<REQUIRED: laws screened and whether each applied;"
                              " 'none applicable: <why>' is acceptable>",
                "laws_applied": "<OPTIONAL: JSON array string of applied law claim ids>",
                "scope": "<OPTIONAL: files touched and change class>",
                "verification": "<OPTIONAL: gates run and their verdicts>",
                "residual": "<OPTIONAL: what remains and why it was left>",
            },
        },
        "claim": {
            "subject": "<REQUIRED: entity key, e.g. function:name or tu:module>",
            "predicate": "<REQUIRED: e.g. law|symbol_naming|opportunity>",
            "epistemic_state": "<REQUIRED: e.g. verified|proposed>",
            "value": "<REQUIRED unless 'object' is used: the claim statement>",
            "attributes": {
                "tags": ["<OPTIONAL: controlled-vocabulary tags; see laws"
                         " tags_available — delete this key if unused>"],
            },
        },
        "evidence": {
            "evidence_kind": "<REQUIRED: e.g. disasm|build|source-trace>",
            "locator": "<REQUIRED: src path:line or tool invocation — never .md>",
            "detail": "<REQUIRED: what the evidence shows>",
            "claim": "<REQUIRED unless 'edge' is used: the claim record id>",
        },
        "entity": {
            "entity_type": "<REQUIRED: e.g. function|tu|struct>",
            "key": "<REQUIRED: e.g. function:name>",
            "name": "<REQUIRED: display name>",
        },
        "edge": {
            "source": "<REQUIRED: entity key>",
            "relation": "<REQUIRED: relation verb>",
            "target": "<REQUIRED: entity key>",
        },
        "work_claim": {
            "function": "<REQUIRED: function:symbol_name anchor>",
            "owner": "<REQUIRED: worker identity>",
            "state": "active",
            "claimed_at": datetime.now(timezone.utc).strftime("%Y-%m-%d"),
            "attributes": {"scope": "<REQUIRED: exclusive file/TU scope>"},
        },
        "tool": {
            "tool_key": "<REQUIRED: stable key>",
            "name": "<REQUIRED: display name>",
            "tool_kind": "<REQUIRED: e.g. external|analysis>",
            "status": "active",
            "purpose": "<REQUIRED: one-line purpose>",
        },
    }
    if kind not in templates:
        raise MemoryGraphError(
            f"no template for kind {kind!r}; choose from "
            + ", ".join(sorted(templates))
        )
    skeleton: dict[str, Any] = {
        "schema_version": SCHEMA_VERSION,
        "id": f"<REQUIRED: {kind}.short-slug."
              f"{datetime.now(timezone.utc).strftime('%Y%m%d')}.v1>",
        "kind": kind,
    }
    skeleton.update(templates[kind])
    return skeleton


def stage_record_proposal(
    record: dict[str, Any],
    *,
    root: Path = REPO_ROOT,
    in_place: Path | None = None,
    dry_run: bool = False,
) -> Path:
    """Atomically stage one validated record in the review-required inbox.

    ``in_place`` supports re-proposing a file that already sits in the inbox
    (a natural authoring flow): the record is validated, stamped, and
    rewritten at its existing path instead of being rejected as a duplicate
    of itself. ``dry_run`` runs the full validation (schema, law_screen,
    tags, references, duplicates) but writes nothing — for iterating on a
    draft without producing throwaway inbox files.
    """
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
    proposed_tags = (
        record.get("attributes", {}).get("tags")
        if isinstance(record.get("attributes"), dict) else None
    )
    if proposed_tags is not None:
        unknown = [tag for tag in proposed_tags
                   if tag not in LAW_TAG_VOCABULARY]
        if unknown:
            raise MemoryGraphError(
                f"unknown tag(s) {unknown}: attributes.tags must come from"
                " the controlled vocabulary — "
                + ", ".join(sorted(LAW_TAG_VOCABULARY))
                + " (extend LAW_TAG_VOCABULARY in memory_graph/core.py via a"
                " reviewed change if a new pattern class is real)"
            )
    _validate_record(record, Path("<proposal>"))
    _probe_record_references(record, root)
    record_id = record["id"]
    in_place_resolved = in_place.resolve() if in_place is not None else None
    for relative in (Path("memory_graph/records"), Path("memory_graph/inbox")):
        directory = root / relative
        if not directory.exists():
            continue
        for path in directory.rglob("*.json"):
            if in_place_resolved is not None and path.resolve() == in_place_resolved:
                continue  # re-proposing the same inbox file is not a duplicate
            try:
                existing = json.loads(path.read_text(encoding="utf-8-sig"))
            except (OSError, json.JSONDecodeError):
                continue
            if isinstance(existing, dict) and existing.get("id") == record_id:
                raise MemoryGraphError(f"record id {record_id!r} already exists at {path}")
    if in_place_resolved is not None:
        destination = in_place_resolved
    else:
        slug = re.sub(r"[^a-zA-Z0-9._-]+", "-", record_id).strip(".-") or "record"
        destination_dir = root / "memory_graph" / "inbox"
        destination_dir.mkdir(parents=True, exist_ok=True)
        destination = destination_dir / f"{slug}.json"
        if destination.exists():
            raise MemoryGraphError(f"proposal destination already exists: {destination}")
    if dry_run:
        return destination
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
    allow_any_branch: bool = False,
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
    if (root / ".git").exists() and not allow_any_branch:
        head = subprocess.run(
            ["git", "-C", str(root), "rev-parse", "--abbrev-ref", "HEAD"],
            capture_output=True, text=True,
        ).stdout.strip()
        if head and head != "main":
            raise MemoryGraphError(
                f"accept is integrator-only and runs from the main checkout"
                f" (current branch: {head}). Workers: commit your inbox"
                " proposals on your branch and leave acceptance to the"
                " integrator merge. Pass allow_any_branch/--any-branch only"
                " for a deliberate exception."
            )
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
        touched.extend([
            str(source.relative_to(root)).replace("\\", "/"),
            str(destination.relative_to(root)).replace("\\", "/"),
        ])
    for path in releases:
        path.unlink()
        touched.append(str(path.relative_to(root)).replace("\\", "/"))
    staged = False
    staging_error = None
    stageable = touched
    if (root / ".git").exists():
        # A moved/deleted source that was never git-tracked matches no
        # pathspec and would abort the whole `git add` AFTER the filesystem
        # mutations — stage only paths that exist on disk or are tracked.
        # Any residual git failure must NOT raise: the moves have already
        # happened, so report precisely and hand the caller a recovery path.
        try:
            tracked = set(
                subprocess.run(
                    ["git", "-C", str(root), "ls-files", "--"] + touched,
                    capture_output=True, text=True,
                ).stdout.splitlines()
            )
            stageable = [
                path for path in touched
                if (root / path).exists() or path in tracked
            ]
            if stageable:
                subprocess.run(
                    ["git", "-C", str(root), "add", "--"] + stageable,
                    check=True, capture_output=True, text=True,
                )
            staged = True
        except (OSError, subprocess.CalledProcessError) as error:
            detail = getattr(error, "stderr", "") or str(error)
            staging_error = (
                "file moves/deletions COMPLETED but git staging failed"
                f" ({detail.strip()[:300]}). Do NOT re-run accept or"
                " propose-record — the records are already in place."
                " Recover by staging manually (PowerShell, not the POSIX"
                " shell): git add -- " + " ".join(stageable or touched)
            )
    build_database(root)
    quoted = " ".join(f'"{path}"' for path in stageable)
    result = {
        "accepted": [record_id for record_id in record_ids],
        "released": release,
        "paths": touched,
        "staged_paths": stageable,
        "staged": staged,
        "graph_rebuilt": True,
        "commit_command": f'git commit -m "<message>" -- {quoted}',
    }
    if staging_error:
        result["staging_error"] = staging_error
    return result


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
                # maximum==1 marks a 0/1 flag; 0 must stay off. Larger
                # maxima are result limits, floored at 1.
                floor = 0 if param.maximum == 1 else 1
                value = max(floor, min(value, param.maximum))
            out[param.name] = value
        return out


def _stats_surface(root: Path, db_path: Path | None) -> dict[str, Any]:
    path = ensure_database(root, db_path)
    return {"database": str(path), **memory_stats(root, path)}


def record_lookup(
    record_id: str, *, root: Path = REPO_ROOT, db_path: Path | None = None
) -> dict[str, Any]:
    """Return full record JSON by id (the on-demand detail fetch).

    Accepts a comma-separated id list so a law-corpus screen is one round
    trip instead of a dozen subprocess spawns.
    """
    ids = [part.strip() for part in record_id.split(",") if part.strip()]
    if not ids:
        raise MemoryGraphError("no record id given")
    ensure_database(root, db_path)
    results = []
    missing = []
    with closing(open_database(root, db_path)) as connection:
        for one_id in ids:
            row = connection.execute(
                "SELECT record_id, record_state, source_path, raw_json"
                " FROM record_ingest WHERE record_id=?",
                (one_id,),
            ).fetchone()
            resolved_from = None
            if row is None:
                # Citations frequently arrive without the .<date>.vN version
                # suffix (four fleet lanes hit this in one run). Resolve a
                # base id to its NEWEST version — explicitly, never silently:
                # the result carries resolved_from so the caller can see the
                # id it asked for was not the id it got. Anything looser
                # (relevance ranking, fuzzy match) is forbidden here: a
                # detail fetch must never substitute unrelated records.
                row = connection.execute(
                    "SELECT record_id, record_state, source_path, raw_json"
                    " FROM record_ingest WHERE record_id LIKE ? ESCAPE '\\'"
                    " ORDER BY COALESCE(recorded_at, valid_from, record_id)"
                    " DESC, record_id DESC LIMIT 1",
                    (one_id.replace("\\", "\\\\").replace("%", "\\%")
                        .replace("_", "\\_") + ".%",),
                ).fetchone()
                if row is not None:
                    resolved_from = one_id
            if row is None:
                missing.append(one_id)
                continue
            entry = {
                "record_id": row["record_id"],
                "record_state": row["record_state"],
                "source_path": row["source_path"],
                "record": json.loads(row["raw_json"]),
            }
            if resolved_from is not None:
                entry["resolved_from"] = resolved_from
                entry["note"] = ("cited without version suffix; resolved to"
                                 " the newest version — cite the full id")
            results.append(entry)
    if not results:
        raise MemoryGraphError(f"no record with id {ids[0]!r}"
                               if len(ids) == 1 else
                               f"none of the {len(ids)} ids matched a record")
    if len(ids) == 1:
        return results[0]
    return {"records": results, "missing": missing, "count": len(results),
            "requested": len(ids)}


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
    tag: str | None = None,
    full: int = 0,
    limit: int = 100,
) -> dict[str, Any]:
    """List the codegen-law corpus, newest first, with freshness and supersession.

    ``tag`` filters on the structured ``attributes.tags`` array (e.g.
    ``core-screen``, ``alias-form``, ``entry-schedule``) — the curated
    applicability vocabulary, cheaper and more precise than prose search.
    ``full=1`` inlines each law's complete text (one round trip for a whole
    tagged screen set instead of a `record <id>` call per law).
    """
    ensure_database(root, db_path)
    sql = """
        SELECT r.record_id, r.record_state, r.valid_from, r.recorded_at,
               c.epistemic_state, c.value_json,
               json_extract(r.raw_json, '$.attributes.scope') AS scope,
               json_extract(r.raw_json, '$.attributes.tags') AS tags,
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
    if tag:
        sql += " AND json_extract(r.raw_json, '$.attributes.tags') LIKE ?"
        params.append(f'%"{tag}"%')
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
        if not full and isinstance(value, str) and len(value) > 300:
            value = value[:300] + " …[--full or gdlmem.py record <id> for full text]"
        try:
            tags = json.loads(row["tags"]) if row["tags"] else []
        except (TypeError, json.JSONDecodeError):
            tags = []
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
                "tags": tags,
                "scope": row["scope"],
                "head": value,
            }
        )
    with closing(open_database(root, db_path)) as connection:
        tag_rows = connection.execute(
            """
            SELECT json_extract(r.raw_json, '$.attributes.tags') AS tags
            FROM claim c JOIN record_ingest r ON r.record_id = c.record_id
            WHERE (c.predicate = 'codegen_law' OR r.record_id LIKE '%law%')
              AND tags IS NOT NULL
            """
        ).fetchall()
    tag_counts: dict[str, int] = {}
    for row in tag_rows:
        try:
            for tag_name in json.loads(row["tags"]):
                tag_counts[tag_name] = tag_counts.get(tag_name, 0) + 1
        except (TypeError, json.JSONDecodeError):
            continue
    return {
        "laws": laws,
        "count": len(laws),
        "tags_available": dict(sorted(tag_counts.items())),
        "note": (
            "laws are compiler-scoped observations, not instructions:"
            " re-verify against your target bytes; a superseded_by entry means"
            " read the newer record instead; filter with --tag <name> using"
            " tags_available (core-screen = the mandatory de-fakematch screen)"
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
_C_COMMENT_RE = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)
_DEBT_KEYWORDS = frozenset((
    "return", "case", "else", "do", "if", "while", "for", "switch",
    "sizeof", "offsetof", "goto", "default", "break", "continue",
))
_TYPEISH_RE = re.compile(
    r"^\s*(?:const\s+|volatile\s+|unsigned\s+|signed\s+|struct\s+|union\s+)*"
    r"[A-Za-z_]\w*\s*\**\s*$")


def _match_open_paren(text: str, close_idx: int) -> int:
    depth = 0
    for i in range(close_idx, -1, -1):
        if text[i] == ")":
            depth += 1
        elif text[i] == "(":
            depth -= 1
            if depth == 0:
                return i
    return -1


def _is_binary_multiply(text: str, star_idx: int) -> bool:
    """True when this `*` is arithmetic, not a dereference.

    `rate * (f32)(u32)gFrameTicks` matches the census regex but is a
    multiply; counting it inflated the census ~3.6% repo-wide (133/3685,
    2026-08-31) and drifted wave-over-wave comparisons on float-heavy TUs.
    Ported from tools/gdl/structdraft.py (the validated authority).
    """
    j = star_idx - 1
    while j >= 0 and text[j] in " \t\r\n":
        j -= 1
    if j < 0:
        return False
    ch = text[j]
    if ch == "]" or ch.isdigit():
        return True
    if ch.isalpha() or ch == "_":
        k = j
        while k >= 0 and (text[k].isalnum() or text[k] == "_"):
            k -= 1
        return text[k + 1:j + 1] not in _DEBT_KEYWORDS
    if ch == ")":
        open_idx = _match_open_paren(text, j)
        if open_idx < 0:
            return False
        if _TYPEISH_RE.match(text[open_idx + 1:j]):
            return False  # a cast -> the `*` is a deref
        k = open_idx - 1
        while k >= 0 and text[k] in " \t\r\n":
            k -= 1
        m = k
        while m >= 0 and (text[m].isalnum() or text[m] == "_"):
            m -= 1
        return text[m + 1:k + 1] not in _DEBT_KEYWORDS
    return False


def _strip_c_comments(text: str) -> str:
    """Blank out comments, preserving newlines so line numbers stay true."""
    return _C_COMMENT_RE.sub(
        lambda match: re.sub(r"[^\n]", " ", match.group(0)), text
    )


_OFFSETOF_MACRO_RE = re.compile(
    r"^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\b.*?"
    r"\b(?:offsetof|sizeof)\b", re.M)
_FN_START_RE = re.compile(
    r"^(?:static\s+)?\w[\w\s\*]*?\b(\w+)\s*\([^;{]*?\)\s*\{", re.M)


def _cast_site_is_named(text: str, start: int,
                        named_macros: frozenset[str] = frozenset()) -> bool:
    """True when THIS cast's displacement is offsetof/sizeof-named, including
    via a file-local macro whose #define body contains offsetof (world.c's
    IOFF() pattern once inflated the bare count by ~25).

    The window ends at this cast's own expression boundary — a top-level
    comma or any semicolon — not the whole statement: scanning to the
    semicolon once credited three bare casts because a LATER cast in the
    same multi-cast statement used offsetof (pb_diag finding). offsetof's
    own internal comma sits inside parens, so depth tracking keeps it.
    """
    window = text[start:start + 240]
    depth = 0
    end = len(window)
    for index, char in enumerate(window):
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth < 0:
                end = index
                break
        elif char == ";" or (char == "," and depth <= 0):
            end = index
            break
    window = window[:end]
    if "offsetof" in window or "sizeof" in window:
        return True
    return any(macro + "(" in window for macro in named_macros)


def _enclosing_function(marks: list[tuple[int, str]], position: int) -> str:
    owner = "<file-scope>"
    for mark_pos, name in marks:
        if mark_pos <= position:
            owner = name
        else:
            break
    return owner


def fakematch_debt(
    tu: str | None = None,
    *,
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    limit: int = 40,
    show_lines: int = 0,
    by_function: int = 0,
) -> dict[str, Any]:
    """Census raw-offset fakematch debt per TU, heaviest first.

    Counts raw-cast sites (``*(T*)(...)``) and PF() macro sites in each C/C++
    file under src/, with comments stripped, splitting cast sites into
    ``bare_sites`` (numeric displacement — the actual remaining debt) and
    ``named_sites`` (already offsetof/sizeof-spelled — converted, kept for
    the total so old censuses stay comparable). Pass ``tu`` to filter;
    ``show_lines=1`` (with a tu filter) lists each bare site as file:line;
    ``by_function=1`` (with a tu filter) aggregates bare sites per enclosing
    function so a pass can be prioritized without an ad-hoc script.
    """
    src = root / "src"
    if not src.exists():
        raise MemoryGraphError(f"no src/ directory under {root}")
    rows = []
    sites: list[str] = []
    owner_counts: dict[str, int] = {}
    for path in sorted(src.rglob("*.c*")):
        if path.suffix.lower() not in (".c", ".cpp"):
            continue
        relative = str(path.relative_to(root)).replace("\\", "/")
        if tu and tu.lower() not in relative.lower():
            continue
        try:
            text = _strip_c_comments(
                path.read_text(encoding="utf-8", errors="replace"))
        except OSError:
            continue
        named_macros = frozenset(_OFFSETOF_MACRO_RE.findall(text))
        want_owners = bool((show_lines or by_function) and tu)
        marks = ([(m.start(), m.group(1)) for m in _FN_START_RE.finditer(text)]
                 if want_owners else [])
        bare = named = 0
        for match in _DEBT_CAST_RE.finditer(text):
            if _is_binary_multiply(text, match.start()):
                continue  # arithmetic `x * (f32)(...)`, not a dereference
            if _cast_site_is_named(text, match.start(), named_macros):
                named += 1
            else:
                bare += 1
                if want_owners:
                    owner = _enclosing_function(marks, match.start())
                    if by_function:
                        owner_counts[owner] = owner_counts.get(owner, 0) + 1
                    if show_lines:
                        line = text.count("\n", 0, match.start()) + 1
                        sites.append(f"{relative}:{line} ({owner})")
        pf_sites = pf_named = 0
        for pf in _DEBT_PF_RE.finditer(text):
            depth = 0
            for i in range(pf.end() - 1, min(pf.end() + 400, len(text))):
                if text[i] == "(":
                    depth += 1
                elif text[i] == ")":
                    depth -= 1
                    if depth == 0:
                        break
            args = text[pf.end():i]
            if "offsetof" in args or "sizeof" in args:
                pf_named += 1  # already converted in place, not debt
            else:
                pf_sites += 1
        if bare or named or pf_sites or pf_named:
            rows.append(
                {"tu": relative, "cast_sites": bare + named,
                 "bare_sites": bare, "named_sites": named,
                 "pf_sites": pf_sites, "pf_named": pf_named,
                 "total": bare + named + pf_sites + pf_named}
            )
    rows.sort(key=lambda row: (-(row["bare_sites"] + row["pf_sites"]),
                               row["tu"]))
    result = {
        "tus": rows[:limit],
        "tu_count": len(rows),
        "site_total": sum(row["total"] for row in rows),
        "bare_total": sum(row["bare_sites"] for row in rows),
        "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "note": (
            "bare_sites (+pf_sites) is the remaining debt; named_sites and"
            " pf_named are already offsetof/sizeof-converted. The census is"
            " a floor — cast-then-index, cast-assign, and macro-bodied"
            " access shapes (WSWAP-style swap macros) escape the regex —"
            " and includes legitimate raw forms (protected webs, structless"
            " pools): read the TU's attempt records before claiming."
            " Binary-multiply lookalikes are excluded since 2026-08-31."
            " A site converted to FULL member form leaves the census"
            " entirely — a shrinking total is progress, not loss."
        ),
    }
    if show_lines and tu:
        result["bare_site_lines"] = sites[:1500]
        if len(sites) > 1500:
            result["bare_site_lines_truncated"] = len(sites) - 1500
    if by_function and tu:
        result["functions"] = [
            {"function": name, "bare_sites": count}
            for name, count in sorted(
                owner_counts.items(), key=lambda item: (-item[1], item[0]))
        ]
    return result


def _record_head(record: dict[str, Any]) -> str:
    for key in ("attempted_axis", "value", "scope", "detail", "name", "purpose"):
        value = record.get(key)
        if not value and isinstance(record.get("attributes"), dict):
            value = record["attributes"].get(key)
        if isinstance(value, str) and value.strip():
            text = " ".join(value.split())
            return text[:200] + (" …" if len(text) > 200 else "")
    return ""


def find_records(
    query: str | None = None,
    *,
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    kind: str | None = None,
    function: str | None = None,
    tu: str | None = None,
    outcome: str | None = None,
    residual: str | None = None,
    law: str | None = None,
    limit: int = 25,
) -> dict[str, Any]:
    """Faceted record search: filter by kind, anchor function, TU, attempt
    outcome, residual class, and associated law, plus optional FTS terms.

    The TU facet is derived through the symbol import (function -> module),
    so historical records are TU-searchable without carrying a `tu` field.
    The law facet matches structured `laws_applied` links and prose mentions
    (law_screen text) alike.
    """
    if not any((query, kind, function, tu, outcome, residual, law)):
        raise MemoryGraphError(
            "find needs at least one facet or search term"
            " (--kind/--function/--tu/--outcome/--residual/--law or query)"
        )
    ensure_database(root, db_path)
    sql = """
        SELECT r.record_id, r.record_kind, r.record_state,
               r.valid_from, r.recorded_at, r.raw_json,
               MIN(fe.entity_key) AS fn_key,
               MIN(bm.object_name) AS tu_name,
               MIN(at.outcome) AS outcome
        FROM record_ingest r
        LEFT JOIN (
            SELECT record_id, function_entity_id FROM attempt
            UNION ALL
            SELECT record_id, function_entity_id FROM work_claim
        ) fx ON fx.record_id = r.record_id
        LEFT JOIN entity fe ON fe.id = fx.function_entity_id
        LEFT JOIN binary_symbol bs
            ON fe.entity_key LIKE 'function:%'
            AND bs.raw_name = substr(fe.entity_key, 10)
            AND bs.platform = 'gamecube' AND bs.symbol_kind = 'function'
        LEFT JOIN binary_module bm ON bm.id = bs.module_id
        LEFT JOIN attempt at ON at.record_id = r.record_id
        WHERE 1=1
    """
    params: list[Any] = []
    if kind:
        sql += " AND r.record_kind = ?"
        params.append(kind)
    if function:
        name = function.split(":", 1)[-1]
        sql += " AND fe.entity_key = ?"
        params.append(f"function:{name}")
    if tu:
        sql += " AND bm.object_name LIKE ?"
        params.append(f"%{tu}%")
    if outcome:
        sql += " AND at.outcome = ?"
        params.append(outcome)
    if residual:
        sql += " AND at.residual_class LIKE ?"
        params.append(f"%{residual}%")
    if law:
        sql += (
            " AND (EXISTS (SELECT 1 FROM attempt_law_application ala"
            "      WHERE ala.attempt_record_id = r.record_id"
            "        AND ala.law_record_id LIKE ?)"
            "   OR r.raw_json LIKE ?)"
        )
        params.extend([f"%{law}%", f"%{law}%"])
    if query:
        sql += (
            " AND r.record_id IN"
            " (SELECT record_id FROM record_fts WHERE record_fts MATCH ?)"
        )
        params.append(_fts_query(query))
    sql += """
        GROUP BY r.record_id
        ORDER BY CASE r.record_state WHEN 'accepted' THEN 0 ELSE 1 END,
                 COALESCE(r.recorded_at, r.valid_from, '') DESC,
                 r.record_id
        LIMIT ?
    """
    params.append(limit)
    with closing(open_database(root, db_path)) as connection:
        rows = connection.execute(sql, params).fetchall()
    results = []
    for row in rows:
        try:
            record = json.loads(row["raw_json"])
        except json.JSONDecodeError:
            record = {}
        fn_key = row["fn_key"] or ""
        results.append(
            {
                "id": row["record_id"],
                "kind": row["record_kind"],
                "state": row["record_state"],
                "function": fn_key.split(":", 1)[-1] if fn_key else None,
                "tu": row["tu_name"],
                "outcome": row["outcome"],
                "age_days": _record_age_days(row["valid_from"], row["recorded_at"]),
                "head": _record_head(record),
            }
        )
    return {
        "results": results,
        "count": len(results),
        "note": "heads only; fetch full detail with gdlmem.py record <id>",
    }


def tu_briefing(
    tu: str,
    *,
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    limit: int = 100,
) -> dict[str, Any]:
    """One-call spawn briefing for a TU-scoped pass.

    Assembles what a fresh worker needs before the first edit: the TU's
    function roster with current fuzzy scores, every live attempt record
    (parks and caps first), active claims touching the TU, the core-screen
    law list plus laws that mention this TU, and the raw-offset debt count.
    Heads only — fetch forensics per record id.
    """
    tu = tu.replace("\\", "/").strip("/")
    if tu.startswith("src/"):
        tu = tu[len("src/"):]
    ensure_database(root, db_path)
    with closing(open_database(root, db_path)) as connection:
        modules = connection.execute(
            "SELECT id, object_name FROM binary_module"
            " WHERE platform='gamecube' AND object_name LIKE ?"
            " ORDER BY object_name",
            (f"%{tu}%",),
        ).fetchall()
        if not modules:
            raise MemoryGraphError(
                f"no GameCube module matches {tu!r}; try a path fragment like"
                " game/enemy/enemy"
            )
        module_ids = [row["id"] for row in modules]
        marks = ",".join("?" * len(module_ids))
        functions = connection.execute(
            f"SELECT raw_name, address, size FROM binary_symbol"
            f" WHERE module_id IN ({marks}) AND symbol_kind='function'"
            f" ORDER BY address LIMIT ?",
            module_ids + [limit],
        ).fetchall()
        fn_names = [row["raw_name"] for row in functions]
        attempts: list[dict[str, Any]] = []
        claims: list[dict[str, Any]] = []
        if fn_names:
            fn_marks = ",".join("?" * len(fn_names))
            keys = [f"function:{name}" for name in fn_names]
            attempt_rows = connection.execute(
                f"""
                SELECT r.record_id, r.valid_from, r.recorded_at, r.raw_json,
                       a.outcome, e.entity_key,
                       (SELECT m.fuzzy_percent FROM measurement m
                        WHERE m.attempt_record_id = a.record_id
                        ORDER BY CASE m.phase WHEN 'after' THEN 0 ELSE 1 END
                        LIMIT 1) AS recorded_fuzzy
                FROM attempt a
                JOIN entity e ON e.id = a.function_entity_id
                JOIN record_ingest r ON r.record_id = a.record_id
                WHERE e.entity_key IN ({fn_marks})
                  AND NOT EXISTS (SELECT 1 FROM record_ingest newer
                      WHERE json_extract(newer.raw_json, '$.supersedes')
                            = a.record_id
                        AND newer.record_state = 'accepted')
                ORDER BY CASE WHEN a.outcome IN ('parked', 'capped')
                         THEN 0 ELSE 1 END,
                         COALESCE(r.recorded_at, '') DESC
                """,
                keys,
            ).fetchall()
            for row in attempt_rows:
                try:
                    record = json.loads(row["raw_json"])
                except json.JSONDecodeError:
                    record = {}
                attempts.append(
                    {
                        "id": row["record_id"],
                        "function": row["entity_key"].split(":", 1)[-1],
                        "outcome": row["outcome"],
                        "residual_class": record.get("residual_class"),
                        "age_days": _record_age_days(
                            row["valid_from"], row["recorded_at"]),
                        "head": _record_head(record),
                        "recorded_fuzzy": row["recorded_fuzzy"],
                    }
                )
            claim_rows = connection.execute(
                f"""
                SELECT w.record_id, w.owner, w.state, w.claimed_at,
                       e.entity_key,
                       json_extract(r.raw_json, '$.attributes.scope') AS scope
                FROM work_claim w
                JOIN entity e ON e.id = w.function_entity_id
                JOIN record_ingest r ON r.record_id = w.record_id
                WHERE e.entity_key IN ({fn_marks})
                  AND w.released_at IS NULL
                  AND w.state NOT IN ('released', 'done')
                """,
                keys,
            ).fetchall()
            claims = [
                {
                    "id": row["record_id"], "owner": row["owner"],
                    "state": row["state"], "claimed_at": row["claimed_at"],
                    "function": row["entity_key"].split(":", 1)[-1],
                    "scope": row["scope"],
                }
                for row in claim_rows
            ]
    # fuzzy scores from the current objdiff report, when built
    scores: dict[str, float] = {}
    report_path = root / "build" / "GUNE5D" / "report.json"
    stems = {row["object_name"].rsplit(".", 1)[0] for row in modules}
    if report_path.exists():
        report = json.loads(report_path.read_text(encoding="utf-8"))
        for unit in report.get("units", []):
            if any(unit.get("name", "").endswith(stem) for stem in stems):
                for function in unit.get("functions", []):
                    scores[function["name"]] = float(
                        function.get("fuzzy_match_percent", 0.0))
    roster = [
        {
            "function": row["raw_name"],
            "size": row["size"],
            "fuzzy": scores.get(row["raw_name"]),
        }
        for row in functions
    ]
    # RE-VERIFY banner: a parked/capped attempt whose recorded score no
    # longer matches the live report is stale evidence — four workers
    # burned probes trusting such parks before this was surfaced here.
    for attempt in attempts:
        if attempt["outcome"] not in ("parked", "capped"):
            continue
        current = scores.get(attempt["function"])
        recorded = attempt.get("recorded_fuzzy")
        if current is not None and recorded is not None \
                and abs(float(recorded) - current) > 0.01:
            attempt["REVERIFY"] = (
                f"score moved since park: recorded {recorded} vs current"
                f" {round(current, 4)} — re-measure before trusting this"
                " cap's classification or axis list")
        elif recorded is None:
            attempt["REVERIFY"] = (
                "no measurement recorded (likely a bulk-import park):"
                " forensics may be thin or polarity-incomplete —"
                " re-derive the baseline before spending probes")
    core_laws = law_corpus(root=root, db_path=db_path, tag="core-screen",
                           limit=50)["laws"]
    # Matching sessions need the schedule/register/entry levers too —
    # core-screen is the de-fakematch set, not the whole toolbox.
    matching_laws = []
    seen_matching: set[str] = set()
    for match_tag in ("matching", "entry-schedule", "register-web",
                      "store-placement"):
        for row in law_corpus(root=root, db_path=db_path, tag=match_tag,
                              limit=20)["laws"]:
            if row["id"] not in seen_matching and not row["superseded_by"]:
                seen_matching.add(row["id"])
                matching_laws.append({"id": row["id"], "tags": row["tags"],
                                      "age_days": row["age_days"]})
    matching_laws.sort(key=lambda row: row["age_days"] or 0)
    mentioned = law_corpus(tu, root=root, db_path=db_path, limit=20)["laws"]
    mentioned_ids = {row["id"] for row in core_laws}
    try:
        debt_rows = fakematch_debt(tu, root=root, db_path=db_path,
                                   limit=10)["tus"]
    except MemoryGraphError:
        debt_rows = []
    return {
        "tu": [row["object_name"] for row in modules],
        "functions": roster,
        "scores_note": (None if scores else
                        "fuzzy is null because build/GUNE5D/report.json does"
                        " not exist yet in this checkout — run a full ninja"
                        " first, then re-run brief for scores"),
        "live_attempts": attempts,
        "active_claims": claims,
        "core_screen_laws": [
            {"id": row["id"], "tags": row["tags"]} for row in core_laws
        ],
        "matching_laws": matching_laws,
        "tu_mentioned_laws": [
            {"id": row["id"], "scope": row["scope"]}
            for row in mentioned if row["id"] not in mentioned_ids
        ],
        "raw_offset_debt": debt_rows,
        "note": (
            "briefing heads only: fetch full law/attempt text in ONE call"
            " with gdlmem.py record <id1>,<id2>,... or laws --tag X --full;"
            " parked/capped attempts are VETOes on their axes; run"
            " tools/gdl/defake_gate.py baseline before the first edit and"
            " honor active_claims from other owners"
        ),
    }


_GC_ADDR_SUFFIX_RE = re.compile(r"_8[0-9A-Fa-f]{7}$")


def _lis_anchors(functions, xbox_names, normalize):
    """Order-consistent shared-name anchors via longest increasing
    subsequence over all (gc_index, xbox_index) name-match pairs.

    Greedy first-match anchoring locks in a bad early match and threw away
    most real anchors on the first field trial; LIS finds the maximal
    mutually-consistent set.
    """
    import bisect
    positions: dict[str, list[int]] = {}
    for j, name in enumerate(xbox_names):
        positions.setdefault(name.lower(), []).append(j)
    pairs: list[tuple[int, int]] = []
    for index, (name, _) in enumerate(functions):
        if name.startswith("fn_"):
            continue
        for j in positions.get(normalize(name), []):
            pairs.append((index, j))
    # same-i pairs sort by DESCENDING j so one GC function cannot chain
    # with itself (standard LIS-with-duplicates trick)
    pairs.sort(key=lambda pair: (pair[0], -pair[1]))
    tails: list[int] = []
    tail_refs: list[int] = []
    back: list[tuple[int, int, int]] = []  # (i, j, predecessor back-index)
    for i, j in pairs:
        position = bisect.bisect_left(tails, j)
        predecessor = tail_refs[position - 1] if position > 0 else -1
        if position == len(tails):
            tails.append(j)
            tail_refs.append(len(back))
        else:
            tails[position] = j
            tail_refs[position] = len(back)
        back.append((i, j, predecessor))
    anchors: list[tuple[int, int]] = []
    cursor = tail_refs[-1] if tail_refs else -1
    while cursor != -1:
        i, j, cursor = back[cursor]
        anchors.append((i, j))
    anchors.reverse()
    seen_i: set[int] = set()
    return [(i, j) for i, j in anchors
            if not (i in seen_i or seen_i.add(i))]


def symbol_naming_audit(
    tu: str | None = None,
    *,
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    limit: int = 60,
) -> dict[str, Any]:
    """Audit fn_*/lbl_* placeholders against Xbox PDB names by POSITION.

    Placeholders carry no name to match on, so candidates come from module
    alignment: the PDB lists each Xbox TU's functions in source order and the
    GC symbol map lists each GC TU's functions in address order; functions
    already sharing a name pin the two sequences together, and a gap between
    consecutive anchors that holds the SAME number of functions on both
    sides yields one-to-one candidates ("exact-gap"). Unequal gaps are
    reported as ambiguous candidate pools; placeholders with no candidate at
    all are the revisit set (record them with predicate `symbol_naming` so a
    future session can ponder names — see AGENTS.md).

    Candidates are cross-platform EVIDENCE, not authority: adopting one is a
    rename with the full cross-TU procedure and gates, never automatic.
    """
    ensure_database(root, db_path)
    with closing(open_database(root, db_path)) as connection:
        gc_rows = connection.execute(
            "SELECT m.object_name AS module, s.raw_name, s.address"
            " FROM binary_symbol s JOIN binary_module m ON m.id=s.module_id"
            " WHERE s.platform='gamecube' AND s.symbol_kind='function'"
            " ORDER BY m.object_name, s.address"
        ).fetchall()
        xbox_rows = connection.execute(
            "SELECT m.object_name AS module, s.raw_name"
            " FROM binary_symbol s JOIN binary_module m ON m.id=s.module_id"
            " WHERE s.platform='xbox' AND s.symbol_kind='function'"
            " ORDER BY m.object_name, s.source_ordinal"
        ).fetchall()
        lbl_rows = connection.execute(
            "SELECT m.object_name AS module, COUNT(*) AS n"
            " FROM binary_symbol s JOIN binary_module m ON m.id=s.module_id"
            " WHERE s.platform='gamecube' AND s.raw_name LIKE 'lbl\\_%' ESCAPE '\\'"
            " GROUP BY m.object_name"
        ).fetchall()
        taken_rows = connection.execute(
            "SELECT raw_name FROM binary_symbol WHERE platform='gamecube'"
        ).fetchall()
    lbl_counts = {row["module"]: row["n"] for row in lbl_rows}
    # A candidate whose name is ALREADY a live GC symbol cannot be adopted —
    # it links multiply-defined. Behavioral verification can't catch this
    # (the name may fit perfectly); only a namespace check can, and the
    # adoption field test hit it twice before this flag existed.
    gc_taken = {row["raw_name"].lower() for row in taken_rows}

    gc_modules: dict[str, list[tuple[str, int]]] = {}
    for row in gc_rows:
        gc_modules.setdefault(row["module"], []).append(
            (row["raw_name"], row["address"]))
    xbox_modules: dict[str, list[str]] = {}
    for row in xbox_rows:
        xbox_modules.setdefault(row["module"], []).append(row["raw_name"])
    xbox_by_stem: dict[str, list[str]] = {}
    for module in xbox_modules:
        stem = re.sub(r"\.obj$", "", module, flags=re.I).lower()
        xbox_by_stem.setdefault(stem, []).append(module)

    def normalize(name: str) -> str:
        return _GC_ADDR_SUFFIX_RE.sub("", name).lower()

    audited = []
    totals = {"placeholders": 0, "exact_gap": 0, "ambiguous": 0,
              "no_candidate": 0}
    for module in sorted(gc_modules):
        if tu and tu.lower() not in module.lower():
            continue
        functions = gc_modules[module]
        placeholders = [name for name, _ in functions
                        if name.startswith("fn_")]
        if not placeholders:
            continue
        totals["placeholders"] += len(placeholders)
        stem = re.sub(r"\.(c|cpp)$", "", module.rsplit("/", 1)[-1]).lower()
        xbox_names = None
        pair_note = None
        stems = xbox_by_stem.get(stem, [])
        if not stems:
            # fallback: unique substring pairing (moviePlayer vs movie etc.)
            close_stems = [xs for xs in xbox_by_stem
                           if stem in xs or xs in stem]
            if len(close_stems) == 1:
                stems = xbox_by_stem[close_stems[0]]
                pair_note = f"paired by substring stem {close_stems[0]!r}"
        if len(stems) == 1:
            xbox_names = xbox_modules[stems[0]]
        elif len(stems) > 1:
            pair_note = f"ambiguous xbox module stem: {stems}"
        elif pair_note is None:
            pair_note = "no xbox module with this stem"
        entry: dict[str, Any] = {
            "gc_module": module,
            "xbox_module": stems[0] if len(stems) == 1 else None,
            "placeholder_functions": len(placeholders),
            "lbl_data_symbols": lbl_counts.get(module, 0),
        }
        if xbox_names is None:
            entry["note"] = pair_note
            entry["no_candidate"] = placeholders
            totals["no_candidate"] += len(placeholders)
            audited.append(entry)
            continue
        # GC link order can be the REVERSE of PDB source order (newcam's
        # whole roster descends) — align in both orientations, keep the
        # better chain, and run the gap logic against that orientation.
        forward = _lis_anchors(functions, xbox_names, normalize)
        reversed_names = list(reversed(xbox_names))
        backward = _lis_anchors(functions, reversed_names, normalize)
        if len(backward) > len(forward):
            anchors = backward
            xbox_names = reversed_names
            entry["orientation"] = "reversed"
        else:
            anchors = forward
            entry["orientation"] = "forward"
        proposals = []
        mismatches = []
        ambiguous = []
        no_candidate = []
        bounds = ([(-1, -1)] + anchors
                  + [(len(functions), len(xbox_names))])
        for (i1, j1), (i2, j2) in zip(bounds, bounds[1:]):
            gc_gap = functions[i1 + 1:i2]
            xbox_gap = xbox_names[j1 + 1:j2]
            gap_placeholders = [(name, addr) for name, addr in gc_gap
                                if name.startswith("fn_")]
            if not gc_gap:
                continue
            if len(gc_gap) == len(xbox_gap):
                # 1:1 positional correspondence: placeholders get candidates;
                # named GC functions with a DIFFERENT xbox name at the same
                # slot are probable invented-name spelling mismatches — the
                # audit's second deliverable (UpdateCam vs CamUpdate class).
                for (name, addr), candidate in zip(gc_gap, xbox_gap):
                    if name.startswith("fn_"):
                        proposal = {
                            "gc": name, "address": hex(addr),
                            "xbox_candidate": candidate,
                            "confidence": "exact-gap",
                        }
                        if candidate.lower() in gc_taken:
                            proposal["confidence"] = "NAME-TAKEN"
                            proposal["warning"] = (
                                "a live GC symbol already uses this name —"
                                " adopting it links multiply-defined; either"
                                " the existing holder is the misnamed one or"
                                " the alignment is off here")
                        else:
                            totals["exact_gap"] += 1
                        proposals.append(proposal)
                    elif normalize(name) != candidate.lower():
                        mismatches.append({
                            "gc": name, "xbox": candidate,
                            "address": hex(addr),
                        })
            elif xbox_gap and gap_placeholders:
                ambiguous.append({
                    "gc_span": [name for name, _ in gap_placeholders],
                    "xbox_candidates": xbox_gap[:20],
                })
                totals["ambiguous"] += len(gap_placeholders)
            elif gap_placeholders:
                no_candidate.extend(name for name, _ in gap_placeholders)
                totals["no_candidate"] += len(gap_placeholders)
        entry.update({
            "anchors": len(anchors),
            "proposals": proposals,
            "spelling_mismatches": mismatches,
            "ambiguous_spans": ambiguous,
            "no_candidate": no_candidate,
        })
        audited.append(entry)
        if len(audited) >= limit:
            break
    return {
        "modules": audited,
        "totals": totals,
        "note": (
            "exact-gap candidates are positional EVIDENCE from the Xbox PDB"
            " — adopt only via the recorded cross-TU rename procedure with"
            " full gates; record no-candidate placeholders per TU as claim"
            " records with predicate symbol_naming for future revisits;"
            " lbl_* data alignment is not attempted (inventory only)"
        ),
    }


def rename_symbol(
    old: str,
    new: str,
    *,
    root: Path = REPO_ROOT,
    apply: bool = False,
) -> dict[str, Any]:
    """Atomic project-wide symbol rename with every invariant held at once.

    The adoption field test proved that hand-rolling these five steps is how
    one gets missed: (1) same-namespace collision pre-check; (2) whole-word
    rename across symbols.txt + src/ + include/; (3) graph-record anchor
    patch (a rename otherwise orphans every record whose `function:`/
    `subject:` field names the old symbol, breaking gdlmem build
    project-wide); (4) stale generated .s/.o cleanup so the build
    regenerates them; (5) a printed gate reminder. Dry-run by default.
    """
    word_re = re.compile(rf"\b{re.escape(old)}\b")
    symbols_path = root / "config" / "GUNE5D" / "symbols.txt"
    symbols_text = symbols_path.read_text(encoding="utf-8", errors="replace")
    if not re.search(rf"^{re.escape(old)}\s*=", symbols_text, re.M):
        raise MemoryGraphError(f"{old!r} is not a symbol in symbols.txt")
    if re.search(rf"\b{re.escape(new)}\b", symbols_text):
        raise MemoryGraphError(
            f"{new!r} already exists in symbols.txt — adopting it would link"
            " multiply-defined (the field-tested failure); the existing"
            " holder must be resolved first")
    touched: dict[str, list[str]] = {"source": [], "records": [],
                                     "stale_objects": []}
    edits: list[tuple[Path, str]] = [(symbols_path,
                                      word_re.sub(new, symbols_text))]
    for base in ("src", "include"):
        directory = root / base
        if not directory.exists():
            continue
        for path in sorted(directory.rglob("*")):
            if not path.is_file() or path.suffix.lower() not in (
                    ".c", ".cpp", ".h"):
                continue
            # Bytes round-trip: read_text/write_text translate newlines and
            # rewrote whole files CRLF, which reads as cross-TU damage in
            # diffs (field report, 2026-08-31). Preserve endings exactly.
            text = path.read_bytes().decode("utf-8", errors="replace")
            if word_re.search(text):
                edits.append((path, word_re.sub(new, text)))
                touched["source"].append(
                    str(path.relative_to(root)).replace("\\", "/"))
    marker = f"function:{old}"
    for base in ("memory_graph/records", "memory_graph/inbox"):
        directory = root / base
        if not directory.exists():
            continue
        for path in sorted(directory.rglob("*.json")):
            text = path.read_bytes().decode("utf-8-sig", errors="replace")
            if f'"{marker}"' in text:
                edits.append(
                    (path, text.replace(f'"{marker}"', f'"function:{new}"')))
                touched["records"].append(
                    str(path.relative_to(root)).replace("\\", "/"))
    build_dir = root / "build" / "GUNE5D"
    if build_dir.exists():
        for path in sorted(build_dir.rglob("*.s")):
            try:
                if word_re.search(path.read_text(encoding="utf-8",
                                                 errors="replace")):
                    touched["stale_objects"].append(
                        str(path.relative_to(root)).replace("\\", "/"))
            except OSError:
                continue
    if apply:
        for path, text in edits:
            path.write_bytes(text.encode("utf-8"))
        for relative in touched["stale_objects"]:
            stale = root / relative
            stale.unlink(missing_ok=True)
            sibling = stale.with_suffix(".o")
            sibling.unlink(missing_ok=True)
    return {
        "old": old,
        "new": new,
        "applied": apply,
        "symbols_txt": True,
        "source_files": touched["source"],
        "record_files_patched": touched["records"],
        "stale_generated_deleted": touched["stale_objects"],
        "next": (
            "python configure.py; ninja -j2 (must be green, main.dol OK);"
            " fndiff --clean the renamed function; gdlmem.py build to"
            " confirm no orphaned record anchors"
            if apply else "re-run with --apply / apply=True to execute"
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
                 "age, tags, application counts, and supersession flags."),
            call=lambda root, db, **kw: law_corpus(
                kw["query"], root=root, db_path=db, tag=kw["tag"],
                full=kw["full"], limit=kw["limit"]),
            params=(
                SurfaceParam("query", str, default=None,
                             help="optional filter over id/scope/law text"),
                SurfaceParam("tag", str, default=None,
                             help="filter by structured applicability tag"),
                SurfaceParam("full", int, default=0, maximum=1,
                             help="1 = inline complete law text"),
                # Default must exceed the corpus: at 100, 36 of 136 laws
                # were silently invisible to enumeration (audit, 2026-08-31).
                SurfaceParam("limit", int, default=400, maximum=500),
            ),
        ),
        SurfaceOp(
            name="find", mcp_name="memory_find_records",
            doc=("Faceted record search: by kind, function, TU, attempt "
                 "outcome, associated law, plus optional FTS terms."),
            call=lambda root, db, **kw: find_records(
                kw["query"], root=root, db_path=db, kind=kw["kind"],
                function=kw["function"], tu=kw["tu"], outcome=kw["outcome"],
                residual=kw["residual"], law=kw["law"], limit=kw["limit"]),
            params=(
                SurfaceParam("query", str, default=None,
                             help="optional FTS terms"),
                SurfaceParam("kind", str, default=None,
                             help="attempt|claim|work_claim|evidence|entity"),
                SurfaceParam("function", str, default=None,
                             help="anchor function name"),
                SurfaceParam("tu", str, default=None,
                             help="TU path fragment (derived via symbol map)"),
                SurfaceParam("outcome", str, default=None,
                             help="attempt outcome, e.g. parked|capped|improved"),
                SurfaceParam("residual", str, default=None,
                             help="residual class fragment, e.g. SCHEDULE"),
                SurfaceParam("law", str, default=None,
                             help="law id fragment (structured links + prose)"),
                SurfaceParam("limit", int, default=25, maximum=100),
            ),
        ),
        SurfaceOp(
            name="symaudit", mcp_name="symbol_naming_audit",
            doc=("Audit fn_*/lbl_* placeholders against Xbox PDB names via "
                 "anchor-based module alignment; reports candidates and the "
                 "no-candidate revisit set."),
            call=lambda root, db, **kw: symbol_naming_audit(
                kw["tu"], root=root, db_path=db, limit=kw["limit"]),
            params=(
                SurfaceParam("tu", str, default=None,
                             help="optional GC module path fragment"),
                SurfaceParam("limit", int, default=60, maximum=300),
            ),
        ),
        SurfaceOp(
            name="brief", mcp_name="memory_tu_briefing",
            doc=("One-call spawn briefing for a TU: function roster with "
                 "scores, live attempts, active claims, screened laws, and "
                 "raw-offset debt."),
            call=lambda root, db, **kw: tu_briefing(
                kw["tu"], root=root, db_path=db, limit=kw["limit"]),
            params=(
                SurfaceParam("tu", str, required=True,
                             help="TU path fragment, e.g. game/enemy/enemy"),
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
                kw["tu"], root=root, db_path=db, limit=kw["limit"],
                show_lines=kw["show_lines"], by_function=kw["by_function"]),
            params=(
                SurfaceParam("tu", str, default=None,
                             help="optional path substring filter"),
                SurfaceParam("limit", int, default=40, maximum=200),
                SurfaceParam("show_lines", int, default=0, maximum=1,
                             help="1 = list bare sites as file:line"
                                  " (requires --tu)"),
                SurfaceParam("by_function", int, default=0, maximum=1,
                             help="1 = aggregate bare sites per enclosing"
                                  " function (requires --tu)"),
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
    # form-undocumented only applies to parks about FIELD-CONVERSION work —
    # scheduler/regalloc parks document different axes and re-probing them
    # with offsetof forms would be noise, not signal.
    conversion_terms = ("raw offset", "raw-offset", "fakematch", "defake",
                        "field conversion", "member conversion", "cast",
                        "member-displacement", "struct field")
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
            if (any(term in body for term in conversion_terms)
                    and not any(term in body for term in form_terms)):
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
