"""Build and query the GDL project-memory graph.

The SQLite file is a disposable materialized view. Durable reviewed facts live
as JSON records under memory_graph/records; legacy notes are preserved and
indexed with provenance but never promoted to verified facts automatically.
"""

from __future__ import annotations

import hashlib
import json
import math
import os
import re
import sqlite3
import subprocess
import sys
import tempfile
import time
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

# The record ids that a LATER accepted record supersedes — the "is this
# record still live?" screen, in the ONE spelling every caller must use.
#
# NON-CORRELATED ON PURPOSE. The natural spelling is
#   NOT EXISTS (SELECT 1 FROM record_ingest newer
#               WHERE json_extract(newer.raw_json,'$.supersedes')
#                     = <outer>.record_id
#                 AND newer.record_state = 'accepted')
# and it re-runs a json_extract over the WHOLE record_ingest table once per
# candidate row: quadratic in the corpus, with a JSON parse in the inner
# loop. Measured run 36 on the live corpus (1,637 records): 14.352s for one
# unconstrained call against 0.029s for the form below, returning the
# identical 649 rows — a 495x cut, and 54.7s of the memory_graph suite's
# 72.9s. Same shape as the run-33 `validate` quadratic. SQLite materializes
# a non-correlated IN subquery once into an ephemeral index; a correlated
# one it cannot. An expression index on json_extract was measured and does
# NOT fix it (the planner still scans: 14.8s -> 18.1s).
#
# The `IS NOT NULL` guard is LOAD-BEARING, not tidiness: `x NOT IN (list)`
# evaluates to NULL — which filters as false — for EVERY row as soon as one
# NULL is in the list, and most records carry no `supersedes` key at all.
# Dropping it silently returns zero rows everywhere.
SUPERSEDED_RECORD_IDS = (
    "SELECT json_extract(newer.raw_json, '$.supersedes')"
    " FROM record_ingest newer"
    " WHERE newer.record_state = 'accepted'"
    "   AND json_extract(newer.raw_json, '$.supersedes') IS NOT NULL"
)

# The companion LOOKUP: for each superseded record, WHICH record replaced
# it. Joined as a derived table for the same reason the screen above is a
# flat IN — a correlated scalar subquery re-parses the corpus per row.
# Note the deliberate difference from SUPERSEDED_RECORD_IDS: no
# `record_state` filter, because the `laws` surface reports a proposed
# successor too, and narrowing it here would silently change what the law
# browse says about supersession. MIN() replaces a bare `LIMIT 1` with no
# ORDER BY: same arbitrary pick when there is one successor, deterministic
# when a record was superseded twice.
SUPERSEDING_RECORD_BY_TARGET = (
    "SELECT json_extract(newer.raw_json, '$.supersedes') AS old_id,"
    " MIN(newer.record_id) AS newer_id"
    " FROM record_ingest newer"
    " WHERE json_extract(newer.raw_json, '$.supersedes') IS NOT NULL"
    " GROUP BY 1"
)

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

# --- run-29 retrieval schema (integrator-decided contract) -----------------
# Three optional additions, all TOP-LEVEL record keys. `attributes.residual`
# already exists as free prose in the authoring template, so the structured
# object had to take a non-colliding home; `residual` also sits beside the
# `residual_class` it refines. Readers below accept an `attributes.<field>`
# spelling as a fallback so a record authored either way is still found, but
# only the top-level spelling is templated and documented.
RESIDUAL_FIELDS = ("signature", "family", "capability_needed", "measured_at")

# Provenance fields the BF backfill added alongside the contract four, kept
# because they carry the QUARANTINE: family_candidate holds extractor
# guesses measured at only ~30-50% precision, and merging those into
# `family` would poison every roster built on the facet. Unknown keys beyond
# these are tolerated rather than refused — an additive schema must not let
# one lane's extension break another lane's import, which is exactly what a
# strict unknown-key check did to the merged corpus on 2026-09-01.
RESIDUAL_EXTENSION_FIELDS = (
    "confidence", "extraction_status", "signature_source",
    "family_candidate", "family_candidate_confidence",
)

# Sentinels: NOT residual families, but legitimate `family` values meaning
# "no family assigned" and "this function has no residual at all".
# Excluded from --family results unless asked for by name.
RESIDUAL_FAMILY_SENTINELS = frozenset({"unclassified", "no-residual"})

# Starter family vocabulary from the contract. Extensible ONLY by recorded
# proposal to the integrator — a silently-grown vocabulary makes
# `find --family` an unreliable negative screen, which is the failure mode
# claim.find-subcommand-caps-at-100-and-silently-falsifies-park-screens
# already measured once on this surface.
RESIDUAL_FAMILY_VOCABULARY = frozenset({
    "live-zero-remat", "copy-form", "branch-pair", "frame-slot", "save-area",
    "pool-order", "reloc-naming", "schedule-window", "regalloc-web",
    "addressing-mode", "constant-hoist", "cse-share", "inline-boundary",
    "eh-scaffold", "prologue-form",
})

# --- run-32 evidence layer (integrator-decided contract) -------------------
# The corpus grew to 366 laws with 1016 attempts citing 250 of them. Every
# consumer surface ranked laws by DATE, so a law refuted the day after it was
# written outranked one that had paid off forty times. The layer below turns
# the citations the corpus already carries into a score. Three rules, all
# deliberate:
#
# (1) SUCCESS is narrow. A law counts a success only when an attempt naming
#     it in `laws_applied` actually landed. The literal contract wording is
#     "exact/improved"; the two extra spellings below are the same outcome
#     under merge/retention bookkeeping (1 record each in the 2026-09-02
#     corpus) and are included so a lane's word choice does not silently
#     erase its own evidence.
# (2) A CAP IS NOT A FAILURE. 486 parked and 336 capped citations sit in the
#     corpus, and a law that correctly predicts a park did its job perfectly.
#     Counting those as failures would rank the project's best negative
#     screens — the ones whose whole purpose is to stop work — at the bottom.
#     They are reported as `neutral_citations`, never in the denominator.
# (3) FAILURE is a strong signal only: someone measured the law false and
#     wrote a `refutes` edge (or, from this run on, an explicit
#     `laws_failed` citation). 8 laws carry one today.
LAW_SUCCESS_OUTCOMES = frozenset({
    "exact", "improved", "merged_improved", "retained_improved",
})

# Wilson lower-bound z for a 95% one-sided interval, and the Beta(1,1)
# (Laplace) prior used for the smoothed mean. Both are reported: the Wilson
# bound is the RANKING key because it penalises small samples, while the Beta
# mean is the more readable point estimate.
WILSON_Z = 1.96

# Status tiers, in ranking order. The tier is the PRIMARY sort key and the
# Wilson score only orders within a tier — measured reason: the run-32 canary
# found claim.law.offsetof-rename-isolated-site-outlier.20260830.v1 at 11
# successes against 1 refutation (Wilson 0.646), which outscores the
# known-winner merged-disjunction law at 1 success and 0 refutations (Wilson
# 0.207). Ranking on the bare score would therefore have floated a REFUTED
# law above a verified one, which is exactly the outcome the canary exists to
# forbid. Tiering fixes it without discarding the score.
LAW_STATUS_ORDER = ("established", "contested", "provisional", "refuted")
LAW_STATUS_NOTES = {
    "established": "at least one attempt citing this law landed exact/improved"
                   " and nothing refutes it",
    "contested": "has verified successes AND a standing refutation — read the"
                 " refuting record before applying; the refutation may be"
                 " scope-limited rather than total",
    "provisional": "NO verified success yet. Hidden from the default view:"
                   " an unverified law that reads as authoritative is how a"
                   " confident hallucination propagates fleet-wide. Pass"
                   " --include-provisional 1 to see these.",
    "refuted": "measured FALSE by a refuting record and carrying no verified"
               " success — do not apply; re-deriving it is wasted work",
}

# --- run-32 typed prose objects --------------------------------------------
# A DENIAL is a record telling a future lane not to do something. In prose it
# is unfalsifiable and immortal: "do-not-retry", "NOT a candidate",
# "ineligible" carry no scope, no measurement, and no way to ever be cleared,
# and the corpus has already been measured mistaking a tool's SILENCE for a
# verdict of ineligibility (claim.law.RQ_webfrank-audit-silence-is-not-
# ineligibility). The typed form forces the four things that make a denial
# re-checkable by somebody else.
DENIAL_FIELDS = ("scope", "premise_measurement", "expiry_check", "falsifier")

# A HYPOTHESIS is discipline 10b's payload: the untried idea a record ends on,
# which becomes the next lane's MANDATORY step 1. Prose hypotheses are
# extracted by phrase match today, which is a guess; the typed form is exact
# and additionally records what it would cost to kill the idea.
HYPOTHESIS_FIELDS = ("statement", "cheapest_refuting_observation",
                     "screened_against_target")

# Words that carry no MECHANISM. Two groups: ordinary function words, and
# this project's generic measurement/process vocabulary — "probe", "score",
# "build", "fuzzy", "real", "match". A refuter that shares only those with
# its statement has not named anything it could observe ABOUT the idea, which
# is the shape run 34's CI lane shipped: a mandatory-step-1 hypothesis whose
# cheapest_refuting_observation could never refute it. Stripping them from
# the STATEMENT is the safe direction — a statement left with no mechanism
# terms is not judged at all rather than warned about.
_HYPOTHESIS_STOPWORDS = frozenset("""
about after again against also although always another around because
been before being below best better between both build builds built
cannot change changed changes check could current currently
does doing done down during each either else even ever every
first from further give given goes going
have having here improve improved improves improving into itself
just keep kept know known
less like likely little look looking
made make makes making many maybe might more most much must
near needs never next nothing
occur occurs only other others over
part past probe probed probes
rather real really result results right
same score scored scores seems shall should side since some still such
take taken than that their them then there these they thing think this
those three thus time times together
under until upon used uses using
very want well were what when where whether which while will with within
without would
match matched matches matching fuzzy insns instruction instructions
function functions target ours source
""".split())

# Gate D vocabulary: denial phrasing that closes an axis for everyone after
# you. Kept narrow and literal — these are the phrases that actually appear
# in the corpus's parks, not a general negativity detector.
_DENIAL_PHRASE_RE = re.compile(
    r"do[- ]not[- ]retry|don't retry|never retry"
    r"|not a candidate|no[t]? a valid candidate"
    r"|ineligible|not eligible"
    r"|permanently parked|closed for good|do not revisit",
    re.I,
)

# Gate A vocabulary: a law asserting one of these makes an unconditional
# claim, and an unconditional claim with no stated falsifier cannot be
# screened OUT by a later lane — it can only be re-derived.
NECESSITY_TERMS = ("must", "requires", "require", "cannot", "only")
_NECESSITY_RE = re.compile(
    r"\b(" + "|".join(NECESSITY_TERMS) + r")\b", re.I)

# Gate B: a record moving a function into the postprocessor work class.
_POSTPROCESSOR_CLASS_RE = re.compile(
    r"postprocessor[- ]class|webfrank[- ]class|postprocessor path"
    r"|eligible for (?:the )?webfrank|webfrank candidacy"
    r"|reclassif\w+ (?:it |the function )?(?:as |to )?postprocessor",
    re.I,
)
# A quoted instruction count, e.g. "27/27" or "582 / 582".
_INSNS_QUOTE_RE = re.compile(r"\b\d{1,5}\s*/\s*\d{1,5}\b")

# Gate C: probed_form describing MORE THAN ONE edit. Deliberately narrow —
# it fires only on an EXPLICIT enumeration, because a loose trigger would
# make the gate a nuisance rather than a check.
_MULTI_EDIT_COUNT_RE = re.compile(
    r"\b(two|three|four|five|six|seven|eight|nine|ten|[2-9]|\d\d+)\s+"
    r"(?:\w+\s+){0,2}"
    r"(forms?|shapes?|edits?|axes|axis|probes?|spellings?|variants?"
    r"|changes?|rewrites?|respellings?)\b",
    re.I,
)
_MULTI_EDIT_ENUM_RE = re.compile(r"(?:^|[\s;])\(?[2-9][.)]\s")

# Gate E (run 36, from the run-35 T6 queue): a residual claim CONFINED TO A
# NAMED WINDOW and SIZED IN WORDS must quote a raw differing-word count.
#
# `fndiff --ops` clusters only where the OPCODE stream diverges and is
# structurally blind to pure register-field words, so a function can print
# "4 tokens in 5 clusters" while more than half its words differ. Run 35
# found a recorded "4-word residual" was 122 of 215 words
# (game/movie/movieplayer::fn_800D8BCC). The word count, not the --ops
# cluster count, is what decides postprocessor candidacy, and a rule sized
# against the cluster count is sized against a residual that does not exist.
#
# `\bwindows?\b` does NOT match `pb_window`: `_` is a word character, so
# there is no word boundary before "window" there — the pb_window TU's own
# records are excluded by construction rather than by a name list.
_WINDOW_TOKEN_RE = re.compile(
    r"\bwindows?\b"
    r"|@0x[0-9a-fA-F]+\s*-\s*0x[0-9a-fA-F]+"
    r"|\+0x[0-9a-fA-F]+\s*(?:\.\.|-|to)\s*\+?0x[0-9a-fA-F]+",
    re.I,
)
# The claim the count has to back: a residual SIZED in words.
_WORD_SIZED_RESIDUAL_RE = re.compile(
    r"\b\d{1,5}[-\s]word\b"
    r"|\b\d{1,5}\s+words?\s+(?:differ|of\s+\d{1,5})",
    re.I,
)
# The measurement that discharges it — the tool, or its output line.
_WORD_DIFF_EVIDENCE_RE = re.compile(
    r"\bwf_word_diff\b"
    r"|differing[-\s]words?\s*[:=]\s*\d{1,5}"
    r"|\bdiffering[-\s]word count\b",
    re.I,
)

# Gate F (run 36): a work_claim scope asserting that its premise is already
# recorded. Dispatch reads the scope as the lane's briefing, so an unnamed
# "banked in the graph" sends a worker after evidence it cannot find — see
# claim.law.MT_a-banked-in-the-graph-premise-is-not-a-citation.
_BANKED_EVIDENCE_RE = re.compile(
    r"banked (?:in|into) the (?:memory )?graph"
    r"|\bbanked in the graph\b"
    r"|(?:already |previously )?recorded in the (?:memory )?graph"
    r"|(?:is|are|sits?) already in the (?:memory )?graph"
    r"|per the (?:memory )?graph"
    r"|the graph (?:already )?(?:has|holds|carries|records)",
    re.I,
)
# A record id, as the corpus spells them: kind, then dotted slug segments.
_RECORD_ID_RE = re.compile(
    r"\b(?:attempt|claim|evidence|entity|edge|work_claim|event)"
    r"\.[A-Za-z0-9_][A-Za-z0-9_.-]{3,}",
)
# Outcomes for which a multi-edit probe must name what it held fixed: only the
# ones that VETO an axis. See gate C.
HELD_FIXED_OUTCOMES = frozenset({"negative", "parked", "capped"})

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
        # core.py COMPUTES the derived tables (law_evidence,
        # residual_signature), so it is a build input like the schema is. It
        # was missing here: a change to a derivation left every existing
        # database serving rows built by the OLD code, and `ensure_database`
        # had no way to know. Added run 33 (RG) alongside the reorder index.
        root / "memory_graph" / "core.py",
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
    """Cache key over every build input: relative path + size + mtime.

    RESOLVE THE ROOT ONCE. `_repo_relative` resolves BOTH sides on every
    call, and on Windows each `Path.resolve()` is two `_getfinalpathname`
    syscalls: 1,779 inputs cost 7,120 of them, ~1.0s per call — and
    `ensure_database` calls this on essentially every gdlmem entry point.
    Measured run 36.

    `_iter_input_paths` composes every path it yields as `root / ...`, so
    `relative_to` against the root is pure string arithmetic with no
    syscall at all; the resolving form is kept as the fallback for a root
    the caller passed in some other shape.
    """
    digest = hashlib.sha256()
    root_resolved = root.resolve()
    for path in _iter_input_paths(root):
        stat = path.stat()
        try:
            key = path.relative_to(root).as_posix()
        except ValueError:
            try:
                key = path.relative_to(root_resolved).as_posix()
            except ValueError:
                key = _repo_relative(root, path)
        digest.update(key.encode("utf-8"))
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


def _record_field(record: dict[str, Any], name: str) -> Any:
    """Read a run-29 schema field, top-level first, attributes as fallback.

    Tolerant on READ so a record authored either way is still retrievable;
    only the top-level spelling is templated and documented.
    """
    if name in record:
        return record[name]
    attributes = record.get("attributes")
    if isinstance(attributes, dict) and name in attributes:
        return attributes[name]
    return None


def _law_id_list(record: dict[str, Any], name: str) -> list[str]:
    """Read a citation list field tolerantly: list, or JSON-encoded string.

    `laws_applied` is authored both ways across the corpus (the template
    documents a "JSON array string"), and a reader that understood only one
    spelling would drop half the evidence on the floor without saying so.
    """
    value = _record_field(record, name)
    if isinstance(value, str):
        try:
            value = json.loads(value)
        except json.JSONDecodeError:
            return []
    if not isinstance(value, list):
        return []
    return [item for item in value if isinstance(item, str) and item]


def _refuted_ids(record: dict[str, Any]) -> list[str]:
    """Every record id this record declares it REFUTES.

    Accepts a single id or a list, top-level or under attributes, because all
    four spellings occur in the corpus.
    """
    value = _record_field(record, "refutes")
    if isinstance(value, str):
        return [value] if value else []
    if isinstance(value, list):
        return [item for item in value if isinstance(item, str) and item]
    return []


def _is_law_record(record: dict[str, Any]) -> bool:
    """A claim record carrying a codegen/workflow law."""
    if record.get("kind") != "claim":
        return False
    predicate = record.get("predicate")
    if predicate in ("codegen_law", "law", "workflow_law"):
        return True
    return ".law." in str(record.get("id", ""))


def _record_text(record: dict[str, Any]) -> str:
    """All human prose on a record, flattened for language screening."""
    parts: list[str] = []

    def walk(node: Any) -> None:
        if isinstance(node, str):
            parts.append(node)
        elif isinstance(node, dict):
            for value in node.values():
                walk(value)
        elif isinstance(node, list):
            for value in node:
                walk(value)

    walk(record)
    return "\n".join(parts)


def _validate_schema_fields(record: dict[str, Any], source: Path) -> None:
    """SHAPE-check the run-29 fields on EVERY record (import and propose).

    Deliberately separate from the requirement gates in
    ``stage_record_proposal``: shape runs corpus-wide so the BF lane's
    IN-PLACE annotations of already-accepted records are caught by
    ``gdlmem validate``/``build``, while the requirement gates bind new
    proposals only and never retroactively invalidate accepted records.
    """
    # TOP-LEVEL ONLY. `attributes.residual` is legacy free prose ("what
    # remains and why it was left") on 654 accepted records; conflating the
    # two would read prose as structure.
    residual = record.get("residual")
    if isinstance(residual, dict):
        # Unknown keys are TOLERATED, not refused: the schema is additive and
        # a strict check here broke the whole corpus import the moment a
        # second lane extended the object.
        for key in RESIDUAL_FIELDS + RESIDUAL_EXTENSION_FIELDS:
            value = residual.get(key)
            if value is not None and not isinstance(value, str):
                raise MemoryGraphError(
                    f"{source}: residual.{key} must be a string or null,"
                    f" got {type(value).__name__}"
                )
        candidate = residual.get("family_candidate")
        if candidate and candidate not in RESIDUAL_FAMILY_VOCABULARY:
            raise MemoryGraphError(
                f"{source}: residual.family_candidate {candidate!r} is"
                " outside the contract vocabulary — "
                + ", ".join(sorted(RESIDUAL_FAMILY_VOCABULARY))
            )
        family = residual.get("family")
        if family and family in RESIDUAL_FAMILY_SENTINELS:
            family = None  # sentinel, not a family
        if family and family not in RESIDUAL_FAMILY_VOCABULARY:
            raise MemoryGraphError(
                f"{source}: residual.family {family!r} is outside the"
                " contract vocabulary — "
                + ", ".join(sorted(RESIDUAL_FAMILY_VOCABULARY))
                + " (extend it by recorded proposal to the integrator, never"
                " in passing: find --family is used as a NEGATIVE screen)"
            )
        measured_at = residual.get("measured_at")
        if measured_at and not re.fullmatch(r"\d{4}-\d{2}-\d{2}",
                                            measured_at):
            raise MemoryGraphError(
                f"{source}: residual.measured_at must be YYYY-MM-DD, got"
                f" {measured_at!r} — an undated signature cannot be aged"
                " against the law wave"
            )
    elif residual is not None and not isinstance(residual, str):
        raise MemoryGraphError(
            f"{source}: residual must be the structured object or legacy"
            f" prose, got {type(residual).__name__}"
        )
    asserted_by = _record_field(record, "asserted_by")
    if asserted_by is not None:
        if not isinstance(asserted_by, list) or not all(
                isinstance(item, str) for item in asserted_by):
            raise MemoryGraphError(
                f"{source}: asserted_by must be an array of tool/test paths"
                " that mechanically assert this law"
            )
    for name in ("falsifier", "held_fixed"):
        value = _record_field(record, name)
        if value is not None and (not isinstance(value, str)
                                  or not value.strip()):
            raise MemoryGraphError(
                f"{source}: {name} must be a non-empty string"
            )
    # run-32 typed prose objects. Shape-checked corpus-wide like the residual
    # object; the REQUIREMENT to have one is a propose-time gate, so no
    # accepted record is retroactively invalidated by the field existing.
    for name, fields, purpose in (
        ("denial", DENIAL_FIELDS,
         "a denial must say WHERE it applies (scope), WHAT was measured to"
         " support it (premise_measurement), the COMMAND that would show it"
         " no longer holds (expiry_check), and what evidence would DISPROVE"
         " it (falsifier)"),
        ("hypothesis", HYPOTHESIS_FIELDS,
         "a hypothesis must state the idea (statement), the cheapest"
         " observation that would KILL it (cheapest_refuting_observation),"
         " and whether it was already screened against the target"
         " (screened_against_target)"),
    ):
        value = _record_field(record, name)
        if value is None:
            continue
        if not isinstance(value, dict):
            raise MemoryGraphError(
                f"{source}: {name} must be the structured object"
                f" {{{', '.join(fields)}}}, got {type(value).__name__}."
                f" {purpose}"
            )
        # Unknown keys TOLERATED — additive schema, same rule as `residual`.
        missing = [field for field in fields
                   if not str(value.get(field) or "").strip()]
        if missing:
            raise MemoryGraphError(
                f"{source}: {name} is missing {', '.join(missing)}."
                f" {purpose}"
            )
        for field, item in value.items():
            if item is not None and not isinstance(item, str):
                raise MemoryGraphError(
                    f"{source}: {name}.{field} must be a string or null,"
                    f" got {type(item).__name__}"
                )


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
        "entity", "edge", "claim", "evidence", "attempt", "work_claim", "tool",
        # run-32: a regime-change marker. Its own kind rather than a claim
        # predicate because it is read by DATE against other records' evidence
        # dates, which is a different question from anything `claim` answers.
        "event",
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
        "event": {"slug", "scope"},
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
    _validate_schema_fields(record, source)


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
        # Match with OR without the source extension, in both directions: a TU
        # that is renamed between .c and .cpp (movieplayer.c -> movieplayer.cpp,
        # 2026-08-31) must not strand records anchored to its former spelling.
        base = name
        for suffix in (".cpp", ".c"):
            if base.endswith(suffix):
                base = base[: -len(suffix)]
                break
        candidates = [name, name + ".c", name + ".cpp", base, base + ".c", base + ".cpp"]
        candidates = list(dict.fromkeys(candidates))
        placeholders = ",".join("?" for _ in candidates)
        rows = connection.execute(
            "SELECT object_name FROM binary_module"
            f" WHERE platform='gamecube' AND object_name IN ({placeholders})",
            candidates,
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
                # MEASURED BUG, fixed run 32: this used to read
                # `attributes.laws_applied` and accept only a JSON LIST. The
                # authoring template documents the field as a "JSON array
                # string", and most lanes wrote it that way — so the importer
                # silently dropped them. Live count on the 2026-09-02 corpus:
                # 142 citations imported out of 1912 present, i.e. 92.6% of
                # the corpus's law-application evidence was invisible to
                # every query built on this table. _law_id_list() reads both
                # spellings, and both homes (top-level and attributes).
                for law_id in _law_id_list(record, "laws_applied"):
                    connection.execute(
                        "INSERT OR IGNORE INTO attempt_law_application"
                        " (attempt_record_id, law_record_id) VALUES (?, ?)",
                        (record["id"], law_id),
                    )
                # laws_failed: the explicit failure citation. Same shape and
                # same tolerance as laws_applied (JSON list, or a
                # JSON-encoded string of one) so a lane that writes one
                # spelling is never silently dropped.
                for law_id in _law_id_list(record, "laws_failed"):
                    connection.execute(
                        "INSERT OR IGNORE INTO attempt_law_failure"
                        " (attempt_record_id, law_record_id) VALUES (?, ?)",
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

    # Refutation edges and regime events, from EVERY surviving record kind.
    # Done in its own pass over `loaded` rather than inside the per-kind
    # branches: `refutes` is a top-level field on attempts, claims and
    # evidence alike, and hanging it off one branch is how the corpus ended
    # up with 79 refutation citations that no query could see.
    for _, record, _ in loaded:
        for refuted in _refuted_ids(record):
            connection.execute(
                "INSERT OR IGNORE INTO record_refutation"
                " (refuting_record_id, refuted_record_id) VALUES (?, ?)",
                (record["id"], refuted),
            )
        if record["kind"] == "event":
            connection.execute(
                "INSERT OR REPLACE INTO regime_event"
                " (record_id, slug, scope, occurred_at, note)"
                " VALUES (?, ?, ?, ?, ?)",
                (
                    record["id"], record["slug"], record["scope"],
                    record.get("occurred_at") or record.get("valid_from") or "",
                    record.get("note"),
                ),
            )

    _derive_law_evidence(connection)
    _derive_residual_index(connection)
    remaining = connection.execute(
        "SELECT COUNT(*) FROM record_ingest"
    ).fetchone()[0]
    return int(remaining), rejected


def _derive_law_evidence(connection: sqlite3.Connection) -> None:
    """Rebuild `law_evidence` from scratch. Called once per build, never else.

    Everything here is a projection of rows already imported — attempt
    outcomes, application citations, failure citations, refutation edges. The
    table is TRUNCATED first so a rebuild is idempotent and a stale row from
    a deleted record cannot survive: this is the property that makes the
    layer auditable, and it is why no other code path may write to it.
    """
    connection.execute("DELETE FROM law_evidence")
    tallies: dict[str, dict[str, Any]] = {}

    def slot(law_id: str) -> dict[str, Any]:
        return tallies.setdefault(law_id, {
            "successes": 0, "failures": 0, "neutral": 0, "cited": 0,
            "latest": None, "success_records": [], "failure_records": [],
        })

    for row in connection.execute(
        """
        SELECT ala.law_record_id AS law_id, ala.attempt_record_id AS rid,
               a.outcome AS outcome,
               COALESCE(r.recorded_at, r.valid_from) AS stamp
        FROM attempt_law_application ala
        JOIN attempt a ON a.record_id = ala.attempt_record_id
        JOIN record_ingest r ON r.record_id = ala.attempt_record_id
        """
    ).fetchall():
        entry = slot(row["law_id"])
        entry["cited"] += 1
        if str(row["outcome"]).lower() in LAW_SUCCESS_OUTCOMES:
            entry["successes"] += 1
            entry["success_records"].append(row["rid"])
            stamp = row["stamp"]
            if stamp and (entry["latest"] is None or stamp > entry["latest"]):
                entry["latest"] = stamp
        else:
            # A park or cap the law correctly predicted. Counted, reported,
            # and deliberately kept OUT of the denominator.
            entry["neutral"] += 1

    for table, column in (("attempt_law_failure", "attempt_record_id"),
                          ("record_refutation", "refuting_record_id")):
        target = ("law_record_id" if table == "attempt_law_failure"
                  else "refuted_record_id")
        for row in connection.execute(
            f"SELECT {target} AS law_id, {column} AS rid FROM {table}"
        ).fetchall():
            entry = slot(row["law_id"])
            if row["rid"] not in entry["failure_records"]:
                entry["failures"] += 1
                entry["failure_records"].append(row["rid"])

    for law_id, entry in tallies.items():
        connection.execute(
            "INSERT INTO law_evidence(law_record_id, successes, failures,"
            " neutral_citations, cited_total, latest_evidence_at,"
            " success_records, failure_records) VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
            (
                law_id, entry["successes"], entry["failures"],
                entry["neutral"], entry["cited"], entry["latest"],
                json.dumps(sorted(entry["success_records"])),
                json.dumps(sorted(entry["failure_records"])),
            ),
        )


# --- RG lane (run 33): independent recounts for every derived table --------
#
# WHY. The run-32 evidence layer shipped with a canary that recomputes
# law_evidence straight from the raw JSON, and that check is what caught the
# importer accepting only the LIST spelling of laws_applied — 142 citations
# imported out of 1912 present, 92.6% of the corpus's law-application evidence
# invisible to every query built on the table. Every OTHER derived table was
# unguarded and would have failed exactly as silently.
#
# INDEPENDENCE IS THE WHOLE POINT, so the field readers below are deliberately
# re-implemented here rather than imported: the 92.6% defect lived in the field
# READER, and a check that calls `_law_id_list` cannot see a bug inside
# `_law_id_list`. (The run-32 canary shares those helpers — that is the gap
# this closes.) These readers accept both documented spellings, a JSON array
# and a JSON-encoded string of one, in both homes, top-level and `attributes.`.


def _recount_id_list(record: Mapping[str, Any], field: str) -> list[str]:
    """Independent re-implementation of the citation field reader."""
    out: list[str] = []
    for holder in (record, record.get("attributes")):
        if not isinstance(holder, Mapping):
            continue
        value = holder.get(field)
        if isinstance(value, str):
            try:
                value = json.loads(value)
            except json.JSONDecodeError:
                value = [value] if value.startswith(("claim.", "attempt.",
                                                     "evidence.")) else []
        if isinstance(value, list):
            out.extend(item for item in value if isinstance(item, str))
    return out


def _recount_refuted(record: Mapping[str, Any]) -> list[str]:
    out: list[str] = []
    for holder in (record, record.get("attributes")):
        if not isinstance(holder, Mapping):
            continue
        value = holder.get("refutes")
        if isinstance(value, str):
            out.append(value)
        elif isinstance(value, list):
            out.extend(item for item in value if isinstance(item, str))
    return out


def recount_derived_tables(
    root: Path = REPO_ROOT, db_path: Path | None = None
) -> dict[str, Any]:
    """Recount every derived table straight from the record JSON.

    A projection table is only trustworthy if something outside its own import
    path reproduces it. Each row reports the SHIPPED count, the INDEPENDENT
    count and the delta — printed with values, never as a bare OK, because a
    parity check that prints nothing once passed by comparing two empty dicts.
    """
    ensure_database(root, db_path)
    with closing(open_database(root, db_path)) as connection:
        imported = {
            row["record_id"]: row["raw_json"]
            for row in connection.execute(
                "SELECT record_id, raw_json FROM record_ingest").fetchall()
        }
        shipped = {
            table: int(connection.execute(
                f"SELECT COUNT(*) FROM {table}").fetchone()[0])
            for table in ("attempt", "claim", "work_claim",
                          "attempt_law_application", "attempt_law_failure",
                          "record_refutation", "law_evidence", "measurement",
                          "regime_event", "residual_signature")
        }
        shipped_evidence = {
            row["law_record_id"]: (int(row["successes"]), int(row["failures"]))
            for row in connection.execute(
                "SELECT law_record_id, successes, failures"
                " FROM law_evidence").fetchall()
        }
        shipped_facets = {
            row["record_id"]: (row["kind"], row["facets_json"])
            for row in connection.execute(
                "SELECT record_id, kind, facets_json"
                " FROM residual_signature").fetchall()
        }

    # Re-parse the SAME record set from the ingest column, which is the only
    # honest comparison: reading the directories instead would count inbox
    # proposals the build legitimately rejected and report a phantom delta.
    records: dict[str, dict[str, Any]] = {}
    unparseable = 0
    for record_id, raw in imported.items():
        try:
            parsed = json.loads(raw)
        except (TypeError, json.JSONDecodeError):
            unparseable += 1
            continue
        if isinstance(parsed, dict):
            records[record_id] = parsed

    counts = {name: 0 for name in
              ("attempt", "claim", "work_claim", "attempt_law_application",
               "attempt_law_failure", "record_refutation", "measurement",
               "regime_event", "residual_signature")}
    successes: dict[str, int] = {}
    failure_sets: dict[str, set[str]] = {}
    facet_rows: dict[str, tuple[str, str]] = {}
    law_ids: set[str] = set()
    for record_id, record in records.items():
        kind = record.get("kind")
        if kind in counts:
            counts[kind] += 1
        if kind == "attempt":
            landed = str(record.get("outcome", "")).lower() \
                in LAW_SUCCESS_OUTCOMES
            applied = {law for law in _recount_id_list(record, "laws_applied")}
            counts["attempt_law_application"] += len(applied)
            law_ids |= applied
            for law in applied:
                if landed:
                    successes[law] = successes.get(law, 0) + 1
            failed = {law for law in _recount_id_list(record, "laws_failed")}
            counts["attempt_law_failure"] += len(failed)
            law_ids |= failed
            for law in failed:
                failure_sets.setdefault(law, set()).add(record_id)
            for phase in ("before", "after"):
                if record.get(phase):
                    counts["measurement"] += 1
        for refuted in set(_recount_refuted(record)):
            counts["record_refutation"] += 1
            failure_sets.setdefault(refuted, set()).add(record_id)
            law_ids.add(refuted)
        residual = record.get("residual")
        if isinstance(residual, dict):
            counts["residual_signature"] += 1
            parsed_sig = parse_residual_signature(residual.get("signature"))
            facet_rows[record_id] = (parsed_sig["kind"],
                                     json.dumps(parsed_sig["facets"]))

    independent_evidence = {
        law: (successes.get(law, 0), len(failure_sets.get(law, ())))
        for law in law_ids
    }

    tables: list[dict[str, Any]] = []
    for name in ("attempt", "claim", "work_claim",
                 "attempt_law_application", "attempt_law_failure",
                 "record_refutation", "measurement", "regime_event",
                 "residual_signature"):
        got, want = shipped[name], counts[name]
        tables.append({"table": name, "shipped": got, "independent": want,
                       "delta": got - want, "ok": got == want})
    tables.append({
        "table": "law_evidence",
        "shipped": shipped["law_evidence"],
        "independent": len(independent_evidence),
        "delta": shipped["law_evidence"] - len(independent_evidence),
        "ok": shipped_evidence == independent_evidence,
        "note": "compared PER LAW (successes, failures), not only by row count",
    })
    mismatched_rows = sorted(
        law for law, pair in independent_evidence.items()
        if shipped_evidence.get(law) != pair)
    facet_mismatch = sorted(
        rid for rid, pair in facet_rows.items()
        if shipped_facets.get(rid) != pair)
    for row in tables:
        if row["table"] == "residual_signature":
            row["ok"] = row["ok"] and not facet_mismatch
            row["note"] = ("compared PER RECORD (kind, facet list), not only"
                           " by row count")
    return {
        "ok": all(row["ok"] for row in tables),
        "tables": tables,
        "records_compared": len(records),
        "records_unparseable": unparseable,
        "law_evidence_mismatches": mismatched_rows[:20],
        "law_evidence_mismatch_count": len(mismatched_rows),
        "residual_signature_mismatches": facet_mismatch[:20],
        "residual_signature_mismatch_count": len(facet_mismatch),
        "method": (
            "every count is recomputed from record_ingest.raw_json by field"
            " readers written independently of the importer's. The importer's"
            " own readers are NOT reused: the 92.6% law-citation defect lived"
            " inside the field reader, and a check that calls it cannot see a"
            " bug inside it."
        ),
    }


def wilson_lower_bound(successes: int, failures: int,
                       z: float = WILSON_Z) -> float:
    """Wilson score interval lower bound for successes/(successes+failures).

    Chosen over the raw ratio because the raw ratio cannot tell 1/1 from
    40/40 — and the corpus is full of 1/1 laws. Returns 0.0 for an empty
    sample, which is what puts an uncited law below every cited one.
    """
    n = successes + failures
    if n <= 0:
        return 0.0
    p = successes / n
    denominator = 1.0 + z * z / n
    centre = p + z * z / (2.0 * n)
    margin = z * ((p * (1.0 - p) / n + z * z / (4.0 * n * n)) ** 0.5)
    return max(0.0, min(1.0, (centre - margin) / denominator))


def beta_mean(successes: int, failures: int,
              prior_success: float = 1.0, prior_failure: float = 1.0) -> float:
    """Posterior mean of Beta(prior + s, prior + f) — the Laplace estimate.

    Reported alongside the Wilson bound as the readable point estimate: an
    uncited law reads 0.50 (no information) rather than the Wilson 0.00,
    which is the honest distinction between "unknown" and "known bad".
    """
    return (prior_success + successes) / (
        prior_success + prior_failure + successes + failures)


def law_evidence_score(successes: int, failures: int, *,
                       refuted: bool | None = None) -> dict[str, Any]:
    """The full score row for one law: tier, Wilson bound, Beta mean, n."""
    if refuted is None:
        refuted = failures > 0
    if refuted:
        status = "contested" if successes > 0 else "refuted"
    elif successes > 0:
        status = "established"
    else:
        status = "provisional"
    return {
        "status": status,
        "successes": successes,
        "failures": failures,
        "n": successes + failures,
        "wilson": round(wilson_lower_bound(successes, failures), 4),
        "beta_mean": round(beta_mean(successes, failures), 4),
    }


def law_score_sort_key(score: Mapping[str, Any]) -> tuple[Any, ...]:
    """Ranking key: tier FIRST, then Wilson, then sample size, then id.

    Tier before score is load-bearing, not stylistic — see LAW_STATUS_ORDER.
    """
    try:
        tier = LAW_STATUS_ORDER.index(str(score.get("status")))
    except ValueError:
        tier = len(LAW_STATUS_ORDER)
    return (tier, -float(score.get("wilson") or 0.0),
            -int(score.get("n") or 0), str(score.get("id") or ""))


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
            "attempt_law_application", "attempt_law_failure",
            "record_refutation", "law_evidence", "regime_event",
            "residual_signature",
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
    residual: str | None = None,
    similar_limit: int = 8,
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
                f"""
                SELECT a.record_id, r.record_state, a.attempted_axis, a.outcome,
                       a.residual_class, a.commit_hash, r.raw_json
                FROM attempt a JOIN record_ingest r ON r.record_id=a.record_id
                WHERE a.function_entity_id=?
                  AND a.record_id NOT IN ({SUPERSEDED_RECORD_IDS})
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
                typed_denial = _record_field(raw, "denial")
                if typed_denial:
                    attempt["denial"] = typed_denial
                quarantine = _ungated_prose_denial(raw)
                if quarantine:
                    attempt["quarantine"] = quarantine
                attempts.append(attempt)
    try:
        documents = search_memory(
            symbol_name, root=root, db_path=db_path, limit=document_limit
        )["documents"]
    except MemoryGraphError:
        documents = []
    similar = similar_residuals(
        root=root, db_path=db_path, function=symbol_name, signature=residual,
        limit=similar_limit)
    return {
        "query": symbol_name,
        "gamecube_symbol": dict(gcn) if gcn is not None else None,
        "xbox_links": links,
        "xbox_neighbors": xbox_neighbors,
        "claims": claims,
        "attempts": attempts,
        # RUN-33 (RG): the transferability section. The RS pilot measured
        # `context` returning zero transferable items on 4 of 4 closable
        # functions; `attempts` above is this function's OWN history, which is
        # a park/veto screen, not a source of cures.
        "similar_residuals": similar,
        "migration_proposals": proposals,
        "legacy_provenance": documents,
        "authority_note": (
            "Xbox symbols and legacy notes are reference evidence. GameCube target "
            "assembly/object data remains authoritative until a link or claim is verified."
        ),
        "denial_note": (
            "An attempt carrying `denial` states a TYPED denial: scope,"
            " premise_measurement, an expiry_check command you can run, and a"
            " falsifier. An attempt carrying `quarantine`"
            " (UNGATED-PROSE-DENIAL) denies work in prose only and predates"
            " the typed-denial gate — it has no scope and no way to expire,"
            " so treat it as a WEAK veto, re-measure its premise, and"
            " supersede it with a typed denial if you re-probe. The prose is"
            " flagged, never auto-extracted: phrase extraction measured"
            " 30-50% precision on this corpus."
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


def entity_key_namespaces(connection: sqlite3.Connection) -> list[tuple]:
    """[(prefix, count, example_key)] over the entity table, biggest first.

    The `entity` table is the FIRST thing `_reference_resolvable` consults,
    so any key already in it resolves — including the non-code namespaces
    (`project:`, `tool:`, `workflow:`) that no error message named. Two run-35
    lanes burned records discovering `project:gdl` by counting ~1,600 record
    files by hand, because the refusal listed only `function:` and `tu:`.
    Read the namespaces out of the corpus instead of hardcoding a list that
    would go stale the first time a lane coins one.
    """
    return [
        (row[0], row[1], row[2])
        for row in connection.execute(
            "SELECT substr(entity_key, 1, instr(entity_key, ':') - 1)"
            "         AS prefix,"
            "       COUNT(*) AS n, MIN(entity_key) AS example"
            " FROM entity WHERE instr(entity_key, ':') > 1"
            " GROUP BY prefix ORDER BY n DESC, prefix"
        ).fetchall()
    ]


def _entity_key_suggestions(connection: sqlite3.Connection, key: str,
                            limit: int = 5) -> list[str]:
    """Existing entity keys that look like the one that failed to resolve."""
    name = key.split(":", 1)[-1]
    prefix = key.split(":", 1)[0] if ":" in key else ""
    rows = connection.execute(
        "SELECT entity_key FROM entity"
        " WHERE lower(entity_key) LIKE lower(?)"
        "    OR lower(entity_key) LIKE lower(?)"
        "    OR lower(name) = lower(?)"
        " ORDER BY length(entity_key) LIMIT ?",
        (f"%:{name}", f"{prefix}:%{name}%", name, limit),
    ).fetchall()
    return [row[0] for row in rows]


def unknown_entity_message(key: str, namespaces, suggestions) -> str:
    """The refusal, as a DIRECTORY of what would have worked.

    Run-35 item 7. The old text named two forms out of the several that
    resolve and offered no way to enumerate the rest, so the only route to a
    valid non-code key was to grep the corpus. Everything below is derived
    from the live database, so it cannot drift away from what actually
    resolves.
    """
    lines = [
        f"proposal references unknown entity {key!r}. THREE things resolve:",
        "  1. any entity_key ALREADY IN the graph — including the non-code"
        " namespaces, which is the half no error used to mention;",
        "  2. `function:<symbol>` naming exactly one GameCube function;",
        "  3. `tu:<module>` naming a GameCube object (with or without a"
        " .c/.cpp suffix).",
    ]
    if namespaces:
        lines.append("Namespaces live in this corpus right now"
                     " (prefix, count, example):")
        for prefix, count, example in namespaces[:12]:
            lines.append(f"  {prefix}: {count} — e.g. {example}")
    if suggestions:
        lines.append("Did you mean: " + ", ".join(suggestions) + "?")
    lines.append(
        "List them yourself with"
        " `python memory_graph/gdlmem.py find --query <term>` or"
        " `gdlmem.py search <term>`; do NOT go counting record files.")
    return "\n".join(lines)


def _probe_record_references(
    record: dict[str, Any], root: Path, db_path: Path | None = None,
    connection: sqlite3.Connection | None = None,
    strict_citations: bool = True,
) -> list[str]:
    """Run the same reference resolution the build applies, before staging.

    A proposal that the build would reject must never reach the inbox: the
    build is fail-soft about inbox errors, but the proposer should learn about
    a bad reference immediately, with the build's own error text.

    ``connection`` lets a CALLER validating many records at once supply one
    open database instead of paying for a fresh `ensure_database` per record.
    That per-record call is what made `validate` unusable: `ensure_database`
    recomputes `source_fingerprint`, which stats every record file, the symbol
    tables, the PDB dump and every tools/gdl source — roughly 1,600 stats — so
    validating 1,568 records performed ~2.5 MILLION file stats plus 1,568
    connection opens. Measured run 33: the whole call did not finish in 600 s.
    Single-record callers (`stage_record_proposal`) are unaffected and still
    pass nothing.
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
        return []
    # Record-id citations must resolve at proposal time. Handoff quality
    # depends on a successor being able to fetch every cited record in one
    # `gdlmem record` call; a typoed or stale id rots silently otherwise
    # (an unresolvable law-id citation shipped in a run brief before a
    # worker caught it by hand). `supersedes` and the structured
    # `attributes.laws_applied` list are both checked; free-text mentions
    # in law_screen stay advisory.
    cited: list[str] = []
    # `describes_denial_of` joins these (run-36 item 9). Gate D has always
    # DESCRIBED it as "a CITATION, not a free-text opt-out ... which is
    # checkable" — but nothing checked it, so any string at all released the
    # typed-denial gate. Resolving it is what makes the documented promise
    # true and keeps the escape from becoming the opt-out it disclaims.
    for citing_key in ("supersedes", "refutes"):
        if isinstance(record.get(citing_key), str):
            cited.append(record[citing_key])
    # Read through _record_field, because gate D releases on EITHER spelling
    # (top-level or attributes.) — checking only the top-level one would
    # leave the same hole one level down.
    describes = _record_field(record, "describes_denial_of")
    if isinstance(describes, str) and describes.strip():
        cited.append(describes)
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
    if connection is not None:
        return _probe_references_with(connection, record, kind, entity_refs,
                                      cited, root,
                                      strict_citations=strict_citations)
    ensure_database(root, db_path)
    with closing(open_database(root, db_path)) as owned:
        return _probe_references_with(owned, record, kind, entity_refs, cited,
                                      root,
                                      strict_citations=strict_citations)


def _cited_ids_that_resolve(connection: sqlite3.Connection,
                            record: dict[str, Any], root: Path) -> list[str]:
    """Record ids mentioned ANYWHERE in this record that actually resolve.

    Free prose counts here on purpose: a work_claim's scope is written as
    English, and requiring the citation to live in a structured field would
    reject the correct habit (naming the record inline) along with the wrong
    one. What is NOT accepted is a mention that resolves to nothing.
    Same-batch inbox files count, exactly as `supersedes` does.
    """
    inbox = root / "memory_graph" / "inbox"
    inbox_ids = ({path.stem for path in inbox.glob("*.json")}
                 if inbox.exists() else set())
    resolved = []
    for candidate in set(_RECORD_ID_RE.findall(_record_text(record))):
        if candidate == record.get("id"):
            continue  # a claim cannot cite itself into existence
        if candidate in inbox_ids:
            resolved.append(candidate)
            continue
        row = connection.execute(
            "SELECT 1 FROM record_ingest WHERE record_id=?", (candidate,)
        ).fetchone()
        if row is not None:
            resolved.append(candidate)
    return sorted(resolved)


def _probe_references_with(
    connection: sqlite3.Connection, record: dict[str, Any], kind: Any,
    entity_refs: list[str], cited: list[str], root: Path,
    strict_citations: bool = True,
) -> list[str]:
    """The reference checks themselves, against an already-open connection.

    Split out of `_probe_record_references` so a bulk caller can hold ONE
    connection across every record; the checks are byte-for-byte the ones the
    build applies.
    """
    for key in entity_refs:
        if not _reference_resolvable(connection, key):
            raise MemoryGraphError(unknown_entity_message(
                key,
                entity_key_namespaces(connection),
                _entity_key_suggestions(connection, key),
            ))
    dangling: list[str] = []
    for cited_id in cited:
        if cited_id == record.get("id"):
            raise MemoryGraphError("a record cannot cite itself")
        row = connection.execute(
            "SELECT 1 FROM record_ingest WHERE record_id=?", (cited_id,)
        ).fetchone()
        if row is None:
            # Same-batch proposals cite each other before any rebuild
            # ingests them — resolve against the inbox files too, so a
            # correct citation is never reported as a typo (a worker
            # burned a round trip hunting a misspelling that wasn't
            # there).
            inbox = root / "memory_graph" / "inbox"
            in_inbox = inbox.exists() and any(
                p.stem == cited_id for p in inbox.glob("*.json")
            )
            if in_inbox:
                continue
            if not strict_citations:
                # BULK VALIDATION of the ACCEPTED corpus. A dangling citation
                # here is usually not a typo: `prune-attempts` DELETES records
                # ejected past the per-function cap, by design, and every
                # `supersedes` pointing at one is stranded by that deletion.
                # Reporting those as hard errors would make `validate` fail on
                # the documented workflow's own output — the gate refusing the
                # records that document it. Collected as debt instead.
                dangling.append(cited_id)
                continue
            raise MemoryGraphError(
                f"cited record id {cited_id!r} does not resolve (check"
                " supersedes / attributes.laws_applied for typos; if the"
                " record was accepted moments ago, rebuild the graph)"
            )
    # Gate F (run 36, from the run-35 T6 queue). A work_claim whose scope
    # says its premise is "banked in the graph" must NAME the record.
    # claim.law.MT_a-banked-in-the-graph-premise-is-not-a-citation: dispatch
    # reads a claim's scope as the lane's briefing, so unnamed banked
    # evidence sends a worker to re-derive something it cannot find — or,
    # worse, to execute a premise that was superseded. AGENTS.md's dispatch
    # screen already says citations must resolve to record ids; this is that
    # screen, enforced where the claim is written rather than where it is
    # read.
    if kind == "work_claim":
        banked = _BANKED_EVIDENCE_RE.search(_record_text(record))
        if banked and not _cited_ids_that_resolve(connection, record, root):
            raise MemoryGraphError(
                "this work_claim's scope claims banked evidence (matched"
                f" {' '.join(banked.group(0).split())!r}) but names no record"
                " id that resolves. \"Banked in the graph\" is not a"
                " citation: dispatch reads the scope as the lane's briefing,"
                " so an unnamed premise sends a worker to re-derive evidence"
                " it cannot find, or to execute a premise that has since been"
                " superseded — see"
                " claim.law.MT_a-banked-in-the-graph-premise-is-not-a-citation"
                ". Name the record id in the scope text (or in"
                " attributes.laws_applied / supersedes) so"
                " `gdlmem.py record <id>` returns it. If the evidence is real"
                " but unrecorded, RECORD IT FIRST and cite that."
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
    return dangling


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
            "residual": {
                "signature": "<OPTIONAL: the fndiff --ops token delta"
                             " VERBATIM, e.g. '+1 addi -1 li'>",
                "family": "<OPTIONAL: one of live-zero-remat, copy-form,"
                          " branch-pair, frame-slot, save-area, pool-order,"
                          " reloc-naming, schedule-window, regalloc-web,"
                          " addressing-mode, constant-hoist, cse-share,"
                          " inline-boundary, eh-scaffold, prologue-form>",
                "capability_needed": "<OPTIONAL: the postprocessor capability"
                                     " that would unpark this, or null —"
                                     " naming it makes the park findable by"
                                     " `find --capability`>",
                "measured_at": "<OPTIONAL: YYYY-MM-DD the signature was"
                               " measured>",
            },
            "held_fixed": "<OPTIONAL, REQUIRED when probed_form enumerates"
                          " more than one edit: the variable this park held"
                          " CONSTANT while varying the others>",
            "denial": {
                "scope": "<REQUIRED if present, and REQUIRED on any record"
                         " using do-not-retry / not-a-candidate / ineligible"
                         " phrasing: exactly what this denial covers — one"
                         " axis on one function, not 'this class'>",
                "premise_measurement": "<REQUIRED if present: the measurement"
                                       " the denial rests on, with the"
                                       " command and its numbers>",
                "expiry_check": "<REQUIRED if present: the COMMAND a later"
                                " lane runs to see whether this still holds."
                                " A denial with no expiry check is immortal>",
                "falsifier": "<REQUIRED if present: what evidence would"
                             " DISPROVE the denial>",
            },
            "hypothesis": {
                "statement": "<REQUIRED if present: the concrete untried idea."
                             " AGENTS.md discipline 10b makes this the next"
                             " lane's MANDATORY step 1>",
                "cheapest_refuting_observation": "<REQUIRED if present: the"
                                                 " cheapest observation that"
                                                 " would KILL this idea>",
                "screened_against_target": "<REQUIRED if present: was this"
                                           " already checked against the"
                                           " target bytes? yes/no + what was"
                                           " seen>",
            },
            "supersedes": "<OPTIONAL: id of the prior attempt record this replaces>",
            "refutes": "<OPTIONAL: id of a record whose mechanism/framing this"
                       " attempt DISPROVED by measurement — distinct from"
                       " supersedes: the refuted record may belong to another"
                       " function or be a law>",
            "attributes": {
                "law_screen": "<REQUIRED: laws screened and whether each applied;"
                              " 'none applicable: <why>' is acceptable>",
                "probed_form": "<OPTIONAL but STRONGLY preferred for any"
                               " negative/capped probe: the LITERAL edited"
                               " source text, not a paraphrase — a"
                               " paraphrased form cost two probes to fail"
                               " to reproduce>",
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
            "falsifier": "<REQUIRED for a law asserting must/requires/cannot/"
                         "only: what evidence would DISPROVE this, and where"
                         " that evidence lives. Delete this key on a"
                         " non-law claim.>",
            "asserted_by": ["<OPTIONAL: tool/test paths that mechanically"
                            " assert this law — delete if unused>"],
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
        "event": {
            "slug": "<REQUIRED: kebab-case name of the regime change>",
            "scope": "<REQUIRED: path fragment, law tag, id slug word, or"
                     " '*' for the whole corpus>",
            "occurred_at": "<OPTIONAL: YYYY-MM-DD, defaults to today>",
            "note": "<OPTIONAL: one line on what changed>",
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


_TOKEN_RE = re.compile(r"[A-Za-z0-9_][A-Za-z0-9_]*")
# Below this length a term is matched exactly; at or above it, on a prefix,
# so `register`/`registers` and `reload`/`reloads` agree while `reload` and
# `relocation` (which share only four characters) do not.
_STEM_LENGTH = 6


def _mechanism_terms(text: str) -> set[str]:
    """Content words in ``text`` that could name a mechanism."""
    return {token.lower() for token in _TOKEN_RE.findall(text or "")
            if len(token) >= 4 and token.lower() not in _HYPOTHESIS_STOPWORDS}


def hypothesis_refuter_warning(hypothesis: Any) -> str | None:
    """Warn when a refuting observation shares no mechanism with its idea.

    Run-34 criticism (CI): a MANDATORY-STEP-1 hypothesis shipped with a
    `cheapest_refuting_observation` that could never refute it. Discipline
    10b makes such a hypothesis the next lane's first action, so a refuter
    that names nothing about the idea does not merely fail to help — it
    certifies an unkillable hypothesis as screenable and hands the next lane
    a step 1 with no exit.

    This is a WARNING, never a refusal. Vocabulary overlap is a heuristic:
    a refuter can legitimately be phrased in the language of an INSTRUMENT
    ("regnorm reports zero genuine rows") rather than of the mechanism, and
    refusing those would tax correct records to catch sloppy ones — the
    failure mode this corpus has already measured twice (the retired
    failing_form_undocumented heuristic, and the 43/43 false-positive reopen
    queue). Returns None when there is nothing to say, including when the
    statement carries no mechanism terms at all to compare against.
    """
    if not isinstance(hypothesis, dict):
        return None
    statement = hypothesis.get("statement")
    refuter = hypothesis.get("cheapest_refuting_observation")
    if not isinstance(statement, str) or not isinstance(refuter, str):
        return None
    terms = _mechanism_terms(statement)
    if not terms:
        return None
    refuter_terms = _mechanism_terms(refuter)
    for term in terms:
        if term in refuter_terms:
            return None
        if len(term) >= _STEM_LENGTH:
            stem = term[:_STEM_LENGTH]
            if any(other.startswith(stem) for other in refuter_terms):
                return None
        for other in refuter_terms:
            if len(other) >= _STEM_LENGTH and term.startswith(
                    other[:_STEM_LENGTH]):
                return None
    sample = ", ".join(sorted(terms)[:8])
    return (
        "WARNING: this hypothesis' cheapest_refuting_observation names none"
        f" of the mechanism terms in its own statement ({sample}). Discipline"
        " 10b makes a recorded hypothesis the next lane's MANDATORY STEP 1,"
        " so a refuter that does not mention what the idea is ABOUT cannot"
        " kill it — the lane runs the observation, learns nothing either way,"
        " and the hypothesis survives forever as an unfalsifiable"
        " instruction. Run 34's CI lane shipped exactly that. State an"
        " observation whose OUTCOME differs depending on whether the"
        " statement is true, in the statement's own terms. (If your refuter"
        " is correctly phrased in an INSTRUMENT's vocabulary rather than the"
        " mechanism's, this warning is a false positive — it does not block"
        " the proposal.)"
    )


def _apply_proposal_gates(record: dict[str, Any]) -> list[str]:
    """The three run-29 validation gates, binding on NEW proposals only.

    Each gate exists because a specific burned-probe criticism was recorded;
    the error text names that record so the author can read WHY rather than
    just satisfying a checker. These run in ``stage_record_proposal`` and NOT
    in ``_validate_record``, so accepted records — which predate the fields —
    are never retroactively invalidated.

    Returns the list of non-blocking WARNINGS raised (run 34 item 7); a
    blocking gate still raises MemoryGraphError.
    """
    text = _record_text(record)
    warnings: list[str] = []
    refuter_warning = hypothesis_refuter_warning(
        _record_field(record, "hypothesis"))
    if refuter_warning:
        warnings.append(refuter_warning)

    # Gate A. A law asserting necessity (must/requires/cannot/only) that
    # states no falsifier can never be screened OUT by a later lane; it can
    # only be re-derived at full cost. RQ measured the general shape: a
    # tool's SILENCE was read as a verdict of ineligibility for want of any
    # stated way to disprove it.
    if _is_law_record(record):
        value = record.get("value")
        law_text = value if isinstance(value, str) else text
        hit = _NECESSITY_RE.search(law_text)
        if hit and not _record_field(record, "falsifier"):
            raise MemoryGraphError(
                f"necessity-language law (matched {hit.group(0)!r}) requires a"
                " `falsifier`: state what evidence would DISPROVE this law and"
                " where that evidence lives. An unconditional law with no"
                " falsifier cannot be screened out by a later lane, only"
                " re-derived — see"
                " claim.law.RQ_webfrank-audit-silence-is-not-ineligibility"
                ".20260901.v1 (absence of output read as a verdict) and"
                " claim.RC_stale-reopen-queue-is-a-classifier-artifact"
                ".20260901.v1 (a record-mining heuristic nobody could"
                " disprove until it was measured, and all 43 of its hits were"
                " false). If the claim is genuinely conditional, soften the"
                " language instead of adding a token falsifier."
                " THE NORM YOU ARE JOINING: the BF backfill measured 258 of"
                " 324 existing necessity-laws carrying NO falsifier — see"
                " claim.BF_necessity-laws-without-falsifiers-report"
                ".20260901.v1. This gate binds new records only, so that"
                " backlog is not yours to fix, but it is why the field is"
                " required going forward."
            )

    # Gate B. Moving a function into the postprocessor work class is only
    # meaningful against its instruction counts: a count-ASYMMETRIC residual
    # is provably outside every postprocessor class, so a reclassification
    # with no quoted N/N is unverifiable on its face.
    #
    # TWO SOUNDNESS NARROWINGS, both required, both learned by this gate
    # firing on the very record that DESCRIBED it:
    # (a) only FUNCTION-ANCHORED records can reclassify a function, so a
    #     project-level methodology claim is excluded by construction;
    # (b) scan a SUBSTANCE projection, not the whole record —
    #     claim.RC_stale-reopen-queue-is-a-classifier-artifact.20260901.v1
    #     measured that matching a record's citation and verification prose
    #     as evidence about its SUBJECT makes every well-run session look
    #     like the thing being searched for (43/43 false positives there).
    anchored = bool(record.get("function")) or \
        str(record.get("subject", "")).startswith("function:")
    substance = _record_text({
        key: value for key, value in record.items()
        if key != "attributes"
    } | {"attributes": {
        key: value
        for key, value in (record.get("attributes") or {}).items()
        if key not in _PARK_CITATION_KEYS
    }})
    reclassifies = anchored and (
        str(record.get("outcome", "")).lower() == "reclassified"
        or bool(_POSTPROCESSOR_CLASS_RE.search(substance))
    )
    if reclassifies and not _INSNS_QUOTE_RE.search(text):
        raise MemoryGraphError(
            "a record reclassifying a function into the postprocessor work"
            " class must QUOTE the instruction counts as N/N (ours/target)."
            " claim.law.webfrank-cannot-close-instruction-count-deltas"
            ".20260831.v1 and"
            " claim.law.webfrank-cannot-close-a-count-asymmetric-residual"
            ".20260831.v1 make the count the DECIDING fact: if the counts"
            " differ, no postprocessor class can reach the function and the"
            " reclassification is wrong. Quote the counts you measured."
        )

    # Gate D (run 32). A record that tells every future lane not to try
    # something must say so in the TYPED form. Prose denials are the corpus's
    # most expensive artifact: they are unfalsifiable, they never expire, and
    # `find`/`brief` render them as vetoes forever. The typed object costs
    # four sentences and makes the denial re-checkable by somebody who was
    # not there.
    #
    # THE GS SELF-REFUSAL LESSON IS APPLIED HERE, both halves, because gate B
    # learned it the hard way by refusing the very record that documented it:
    # (a) ANCHOR SCOPE — only a record anchored to a FUNCTION can deny work on
    #     one, so this law record, the run-32 methodology claims, and every
    #     project-level record are excluded by construction rather than by
    #     wording; and
    # (b) SUBSTANCE PROJECTION — scan `substance`, which already excludes
    #     _PARK_CITATION_KEYS, so a record whose law_screen or verification
    #     prose QUOTES the denial vocabulary (as every record about this gate
    #     must) is not caught by its own citation apparatus.
    # The practical corollary from that law was followed: this gate was run
    # against the records documenting it before the commit landed.
    # (c) RUN 33, from the MB lane via the integrator: the gate also fired on
    #     records whose substance QUOTES a prior denial in order to screen,
    #     re-measure or overturn it — exactly the behaviour AGENTS.md asks for
    #     (discipline 1: re-derive the mechanism; discipline 10b: remeasure the
    #     NEGATIVE findings). Demanding a typed `denial` from a record that is
    #     REPORTING someone else's is the self-refusal defect one level out, so
    #     `describes_denial_of: <record-id>` is the explicit escape. It is a
    #     CITATION, not a free-text opt-out: it names the record being
    #     described, which is checkable, and it does not suppress the gate for
    #     a record that also issues a denial of its own — that still needs the
    #     typed object.
    describes = _record_field(record, "describes_denial_of")
    if anchored and not _record_field(record, "denial") and not describes:
        denial_hit = _DENIAL_PHRASE_RE.search(substance)
        if denial_hit:
            # THE ESCAPE GOES FIRST (run-35 item 9). This text already
            # implemented `describes_denial_of`, but named it in the last
            # sentence of a nine-sentence paragraph — and CL, whose record
            # was DESCRIBING a prior park exactly as discipline 1 and 10b
            # ask, read the refusal as "you must invent a denial". An
            # escape a reader does not reach is an escape that does not
            # exist. Two branches, in the order a reader needs them.
            raise MemoryGraphError(
                f"denial language (matched {denial_hit.group(0)!r}) on a"
                " function-anchored record.\n"
                "\nARE YOU DESCRIBING SOMEONE ELSE'S DENIAL — screening it,"
                " re-measuring it, or overturning it? That is what AGENTS.md"
                " disciplines 1 and 10b ask for, and this gate is not aimed"
                " at you. Add:\n"
                '    "describes_denial_of": "<the record id you are'
                ' describing>"\n'
                "It is a CITATION, not a free-text opt-out: the id must"
                " resolve. It does not suppress the gate for a record that"
                " ALSO issues a denial of its own — that still needs the"
                " typed object below.\n"
                "\nARE YOU ISSUING THE DENIAL? Then it needs the typed"
                " `denial` object: {scope, premise_measurement,"
                " expiry_check, falsifier} — the SCOPE it covers, the"
                " MEASUREMENT behind it, an EXPIRY_CHECK command a later"
                " lane can run to see whether it still holds, and the"
                " FALSIFIER. A prose denial cannot be screened out, cannot"
                " expire, and renders as a permanent veto in every brief;"
                " see claim.law.RQ_webfrank-audit-silence-is-not-"
                "ineligibility.20260901.v1, where a tool's SILENCE was read"
                " as a verdict of ineligibility."
            )

    # Gate E (run 36). A residual claim confined to a named window and sized
    # in WORDS has to quote the raw differing-word count that backs it. See
    # the regex block for the measured case: a recorded "4-word residual"
    # was 122 of 215 words, because --ops is blind to register-field words.
    # Anchored like gates B and D, and scanned over `substance`, so a record
    # CITING somebody else's windowed claim is not caught by its citation.
    if (anchored
            and _WINDOW_TOKEN_RE.search(substance)
            and not _record_field(record, "differing_words")):
        sized = _WORD_SIZED_RESIDUAL_RE.search(substance)
        if sized and not _WORD_DIFF_EVIDENCE_RE.search(substance):
            raise MemoryGraphError(
                "a residual claim confined to a named window and sized in"
                f" words (matched {' '.join(sized.group(0).split())!r})"
                " must quote the RAW DIFFERING-WORD COUNT that backs it."
                " `fndiff --ops` clusters only where the OPCODE stream"
                " diverges and is structurally blind to pure register-field"
                " words, so it under-reports the residual it is asked to"
                " size: run 35 found a recorded \"4-word residual\" was 122"
                " of 215 words on game/movie/movieplayer::fn_800D8BCC (see"
                " claim.law.identical-multiset-is-blind-to-displacements"
                ".20260831.v1). The word count, not the --ops cluster count,"
                " decides postprocessor candidacy, and a rule sized against"
                " the cluster count is sized against a residual that is not"
                " there. MEASURE IT:"
                "\n    python tools/gdl/composed_census/wf_word_diff.py"
                " <unit> <function>"
                "\nthen quote its `DIFFERING WORDS = N` line in the record,"
                " or set the `differing_words` field to the number."
            )

    # Gate C. A park that changed several things at once and does not say
    # which variable it held fixed reads as a veto on every axis it touched.
    # Discipline 6's measured case: two correct-alone negative parks jointly
    # hid a 7-function TU flip because neither said what it held constant.
    #
    # RUN-33 NARROWING, from the MB lane via the integrator: the gate is keyed
    # on OUTCOME. Its whole rationale is that a NEGATIVE result reads as a veto
    # on every axis it touched, so the record must say which variable it held
    # constant. A record whose edits were all RETAINED (exact/improved/
    # neutral/reclassified) vetoes nothing — demanding held_fixed there taxes
    # the successes to protect against a failure mode only the failures have.
    if record.get("kind") == "attempt" and \
            str(record.get("outcome", "")).lower() in HELD_FIXED_OUTCOMES:
        probed = _record_field(record, "probed_form")
        if isinstance(probed, str) and probed.strip():
            multi = _MULTI_EDIT_COUNT_RE.search(probed) \
                or _MULTI_EDIT_ENUM_RE.search(probed)
            if multi and not _record_field(record, "held_fixed"):
                raise MemoryGraphError(
                    "multi-edit probed_form (matched"
                    f" {' '.join(multi.group(0).split())!r}) requires"
                    " `held_fixed`: name the variable this park held CONSTANT"
                    " while it varied the others. Without it the record reads"
                    " as a veto on every axis it touched — AGENTS.md"
                    " discipline 6's measured case is btricol, where two"
                    " correct-alone negative parks (extern-ghost and"
                    " volatile-scaffold) were ONE lever and jointly hid a"
                    " 7-function TU flip; see"
                    " attempt.br-btricol-ghosts-unlock-and-btrilinecol-"
                    "closure.20260901.v1. If the probe really varied one"
                    " thing, say so in held_fixed."
                )
    return warnings


def _duplicate_claim_candidates(
    record: dict[str, Any], root: Path, db_path: Path | None = None,
) -> list[dict[str, Any]]:
    """Existing claims that look like the one being proposed.

    Two signals, deliberately cheap and deliberately conservative:
      * SLUG OVERLAP — Jaccard over the id's meaningful words, reusing the
        same `_slug_words` tokenizer the retrieval surface already indexes on,
        so "what the author named it" is compared the same way a searcher
        would find it.
      * FTS OVERLAP — the proposed claim's own value text, run through the
        record index, to catch a re-derivation that chose different words for
        the id.

    A hit is NOT an error and must never be reported as one. The corpus grew
    366 laws with 131 of them uncited, and the likeliest reason a lane writes
    a near-duplicate is that it re-derived something it could not find. The
    right response to that is to attach the new evidence to the record that
    already exists, which is a strictly better outcome than a second record:
    two half-evidenced claims score worse under the evidence layer than one
    fully-evidenced claim, and they compete for the same reader.
    """
    # A supersession or a refutation is SUPPOSED to resemble its target —
    # that is what a v2 is. Screening those would make the gate fire hardest
    # on exactly the records the corpus most wants written.
    declared = set(_refuted_ids(record))
    supersedes = record.get("supersedes")
    if isinstance(supersedes, str):
        declared.add(supersedes)
    elif isinstance(supersedes, list):
        declared.update(item for item in supersedes if isinstance(item, str))

    new_words = set(_slug_words(record.get("id", "")))
    if not new_words:
        return []
    value = record.get("value")
    text = value if isinstance(value, str) else ""

    ensure_database(root, db_path)
    hits: dict[str, dict[str, Any]] = {}
    with closing(open_database(root, db_path)) as connection:
        for row in connection.execute(
            "SELECT record_id FROM record_ingest WHERE record_kind='claim'"
        ).fetchall():
            existing_id = row["record_id"]
            if existing_id == record.get("id") or existing_id in declared:
                continue
            other = set(_slug_words(existing_id))
            if not other:
                continue
            overlap = len(new_words & other) / len(new_words | other)
            if overlap >= _DEDUP_SLUG_JACCARD:
                hits[existing_id] = {
                    "id": existing_id, "signal": "slug",
                    "similarity": round(overlap, 3),
                    "shared_words": sorted(new_words & other),
                }
        if text and len(text.split()) >= 8:
            try:
                query = _fts_query(" ".join(text.split()[:24]))
            except MemoryGraphError:
                query = None
            if query:
                # OR the tokens: an AND query over 24 words matches nothing,
                # and a dedup screen that can never fire is worse than none.
                query = query.replace(" AND ", " OR ")
                try:
                    rows = connection.execute(
                        "SELECT record_id, rank FROM record_fts"
                        " WHERE record_fts MATCH ? AND record_kind='claim'"
                        " ORDER BY rank LIMIT 5", (query,)
                    ).fetchall()
                except sqlite3.OperationalError:
                    rows = []
                for row in rows:
                    existing_id = row["record_id"]
                    if (existing_id == record.get("id")
                            or existing_id in declared
                            or existing_id in hits):
                        continue
                    other = set(_slug_words(existing_id))
                    overlap = (len(new_words & other) / len(new_words | other)
                               if other else 0.0)
                    if overlap >= _DEDUP_FTS_SLUG_FLOOR:
                        hits[existing_id] = {
                            "id": existing_id, "signal": "text+slug",
                            "similarity": round(overlap, 3),
                            "shared_words": sorted(new_words & other),
                        }
    return sorted(hits.values(), key=lambda hit: -hit["similarity"])[:5]


# Tuned against the live corpus: at 0.60 the screen fires on genuine
# re-derivations while leaving distinct laws in the same family alone. The
# FTS path needs only a weak slug agreement because the text already carried
# the evidence.
_DEDUP_SLUG_JACCARD = 0.60
_DEDUP_FTS_SLUG_FLOOR = 0.34


def stage_record_proposal(
    record: dict[str, Any],
    *,
    root: Path = REPO_ROOT,
    in_place: Path | None = None,
    dry_run: bool = False,
    confirm_new: bool = False,
    warnings: list[str] | None = None,
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
    gate_warnings = _apply_proposal_gates(record)
    if warnings is not None:
        warnings.extend(gate_warnings)
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
    # DEDUP-AT-PROPOSE, attach-not-error. Claims only: an attempt record is
    # per-function forensics and is SUPPOSED to resemble its siblings.
    if record.get("kind") == "claim" and not confirm_new and in_place is None:
        near = _duplicate_claim_candidates(record, root)
        if near:
            listing = "; ".join(
                f"{hit['id']} ({hit['signal']}, similarity"
                f" {hit['similarity']}, shared: {', '.join(hit['shared_words'])})"
                for hit in near)
            raise MemoryGraphError(
                "this claim closely resembles a record that already exists:"
                f" {listing}."
                " PREFER ATTACHING over duplicating: add your measurement to"
                " that record as an `evidence` record citing it, or supersede"
                " it with a v-next that carries both derivations (set"
                " `supersedes`, which exempts you from this screen). One"
                " fully-evidenced claim outranks two half-evidenced ones"
                " under the evidence layer, and a near-duplicate splits the"
                " citation history that layer scores on — the live corpus"
                " already carries 131 laws with zero verified successes,"
                " and a re-derivation nobody could find is how they got"
                " there. If yours really is a DIFFERENT claim, re-run with"
                " --confirm-new and say in the record how it differs.")
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
    foreign_staged = []
    if (root / ".git").exists():
        # A no-pathspec `git commit` after accept has three times reverted
        # merge-landed SOURCE files: the shared-checkout index can hold
        # pre-merge content for paths accept never touched. Surface any
        # foreign staged path loudly so the caller commits with the
        # pathspec below instead of the whole index.
        try:
            cached = subprocess.run(
                ["git", "-C", str(root), "diff", "--cached",
                 "--name-only"],
                capture_output=True, text=True,
            ).stdout.splitlines()
            foreign_staged = [
                path for path in cached
                if path and not path.startswith("memory_graph/")
            ]
        except OSError:
            pass
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
    if foreign_staged:
        result["WARNING_foreign_staged_paths"] = (
            "the index holds staged changes OUTSIDE memory_graph — a"
            " no-pathspec commit will include (or REVERT) them: "
            + ", ".join(foreign_staged[:8])
            + ". Commit with the pathspec in commit_command.")
    if staging_error:
        result["staging_error"] = staging_error
    return result


def stage_event_proposal(
    slug: str,
    *,
    scope: str,
    occurred_at: str | None = None,
    note: str | None = None,
    root: Path = REPO_ROOT,
) -> Path:
    """Stage a regime-change event as a reviewable inbox record.

    An event says "the ground under this scope moved on this date". Every
    claim whose newest supporting measurement predates it, and whose scope the
    event covers, then renders with a needs-revalidation banner.

    This is the deliberate alternative to calendar decay. Ageing claims by
    time punishes old laws that are still perfectly true and says nothing
    about a law measured yesterday under a tool that changed this morning.
    Tying revalidation to a RECORDED regime change means someone has to name
    the change, which is also the only way the banner can ever be cleared:
    re-measure, and the fresh evidence postdates the event.
    """
    slug = slug.strip()
    if not re.fullmatch(r"[a-z0-9][a-z0-9-]{2,80}", slug):
        raise MemoryGraphError(
            f"event slug {slug!r} must be lowercase kebab-case, 3-81 chars"
            " (e.g. 'regnorm-v2-migration', 'webfrank-value-equality-class')"
        )
    if not scope or not scope.strip():
        raise MemoryGraphError(
            "an event needs --scope: a path fragment (e.g."
            " 'tools/gdl/regnorm.py'), a law tag (e.g. 'postprocessor'), or"
            " '*' for the whole corpus. An unscoped event would banner every"
            " claim in the graph and teach every reader to ignore the banner"
        )
    occurred = occurred_at or datetime.now(timezone.utc).strftime("%Y-%m-%d")
    if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", occurred):
        raise MemoryGraphError(
            f"--occurred-at must be YYYY-MM-DD, got {occurred!r}")
    record = {
        "schema_version": SCHEMA_VERSION,
        "id": f"event.{slug}.{occurred.replace('-', '')}.v1",
        "kind": "event",
        "slug": slug,
        "scope": scope.strip(),
        "occurred_at": occurred,
        "valid_from": occurred,
    }
    if note:
        record["note"] = note
    return stage_record_proposal(record, root=root)


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


# residual_class predates `family` by the whole campaign: it sits on 941 of
# 971 attempts but has drifted to 35 spellings, many compound ("STRUCTURAL(1
# cluster, precisely rooted) + REGISTER_ONLY/SCHEDULE(2 clusters)"). Families
# are defined AGAINST these coarse classes, so --family can fall back to the
# legacy field instead of leaving 941 records invisible to the new facet.
_RESIDUAL_CLASS_CANON = (
    "REGISTER_ONLY", "STACK_LAYOUT", "STRUCTURAL", "SCHEDULE", "RELOCATION",
    "NON_TEXT", "OPERAND_DIFF", "MIXED", "EXACT", "NONE",
)

FAMILY_TO_RESIDUAL_CLASS = {
    "live-zero-remat": "REGISTER_ONLY",
    "copy-form": "REGISTER_ONLY",
    "regalloc-web": "REGISTER_ONLY",
    "cse-share": "REGISTER_ONLY",
    "schedule-window": "SCHEDULE",
    "branch-pair": "STRUCTURAL",
    "pool-order": "STRUCTURAL",
    "addressing-mode": "STRUCTURAL",
    "constant-hoist": "STRUCTURAL",
    "inline-boundary": "STRUCTURAL",
    "frame-slot": "STACK_LAYOUT",
    "save-area": "STACK_LAYOUT",
    "prologue-form": "STACK_LAYOUT",
    "reloc-naming": "RELOCATION",
    "eh-scaffold": "NON_TEXT",
}


def normalize_residual_class(raw: str | None) -> list[str]:
    """Canonical class tokens present in a free-form residual_class string."""
    if not isinstance(raw, str):
        return []
    upper = raw.upper()
    found = []
    for token in _RESIDUAL_CLASS_CANON:
        if token in upper and token not in found:
            found.append(token)
    # "REGISTER" alone is a live spelling; don't lose it to REGISTER_ONLY.
    if not found and "REGISTER" in upper:
        found.append("REGISTER_ONLY")
    return found


_SLUG_SPLIT_RE = re.compile(r"[^a-z0-9]+")
# Version and date suffixes are NOT content: RC measured that counting law
# citations by date suffix misreads a record that cites laws by slug alone
# ("CTriListCollide's screen cites four wave laws without version suffixes,
# which is why a naive suffix count reads it as unscreened — COUNT CITATIONS
# BY SLUG, NOT BY DATE SUFFIX").
_SLUG_NOISE_RE = re.compile(r"^(v\d+|\d{6,8}|claim|law|attempt|evidence)$")


def _slug_words(record_id: str) -> list[str]:
    """Content words of a record id, with kind/date/version noise dropped."""
    words = [w for w in _SLUG_SPLIT_RE.split(record_id.lower()) if w]
    return [w for w in words if not _SLUG_NOISE_RE.match(w)]


def _query_tokens(query: str) -> list[str]:
    return [t for t in _SLUG_SPLIT_RE.split(query.lower()) if t]


def _slug_match(record_id: str, tokens: list[str]) -> bool:
    """True when EVERY query token appears in the id's slug words.

    This is the index RC/MB found missing: `laws --query` matched only
    prose and the raw id substring, so a hyphenated family name typed as
    separate words ("live zero remat") returned nothing on a family whose
    every member carries those words in its id.
    """
    if not tokens:
        return False
    words = _slug_words(record_id)
    if not words:
        return False
    return all(any(token in word for word in words) for token in tokens)


# The measured shape `fndiff --ops` actually emits, which is what 624 of the
# backfilled signatures carry:
#   "DIFFERS target-only: +1 add ours-only: -1 mr; insns T71/O71; 6 ops
#    clusters"
#   "0t opcode multiset IDENTICAL (155/155); insns T155/O155; 0 ops clusters"
# Word-splitting that string yields {differs, target, only, ours, insns,
# ops, clusters, ...}, so ANY two signatures overlap on framing words and
# the facet degenerates. Take only the mnemonics carried by a +N/-N marker.
_SIGNATURE_OPCODE_RE = re.compile(r"[+-]\s*\d+\s+([a-z][a-z0-9_.]*)")
_SIGNATURE_STOPWORDS = frozenset({
    "differs", "target", "only", "ours", "insns", "ops", "clusters",
    "cluster", "opcode", "multiset", "identical", "none", "and", "or",
    "vs", "real", "fuzzy", "t", "o",
})


def _signature_tokens(signature: str) -> set[str]:
    """Bare opcode mnemonics of a `--ops` token delta.

    Primary path reads the +N/-N markers, which is exact on the measured
    fndiff format. Bare hand-written forms ('+1 addi -1 li') match the same
    pattern. Only when neither yields anything does this fall back to word
    splitting, with the framing vocabulary filtered out.
    """
    lowered = signature.lower()
    tokens = set(_SIGNATURE_OPCODE_RE.findall(lowered))
    if tokens:
        return tokens
    for raw in _SLUG_SPLIT_RE.split(lowered):
        # A PPC mnemonic starts with a letter, so anything leading with a
        # digit is a count artifact ("0t", "155", "2t") rather than an
        # opcode.
        if not raw or not raw[0].isalpha() or raw in _SIGNATURE_STOPWORDS:
            continue
        # drop count artifacts like t71 / o421
        if re.fullmatch(r"[to]\d+", raw):
            continue
        tokens.add(raw)
    return tokens


def _iter_attempt_residuals(root: Path) -> Iterator[tuple[dict[str, Any], dict[str, Any]]]:
    """Yield (record, residual-object) for every record carrying one."""
    for relative in ("records", "inbox"):
        directory = root / "memory_graph" / relative
        if not directory.exists():
            continue
        for path in sorted(directory.rglob("*.json")):
            try:
                record = json.loads(path.read_text(encoding="utf-8-sig"))
            except (OSError, json.JSONDecodeError):
                continue
            if not isinstance(record, dict):
                continue
            # TOP-LEVEL ONLY: attributes.residual is prose on 654 records.
            residual = record.get("residual")
            if isinstance(residual, dict):
                yield record, residual


# --- RG lane (run 33): the PURE-REORDER index axis -------------------------
#
# MEASURED PROBLEM (claim.law.RS_residual-retrieval-is-blind-to-pure-reorder-
# residuals): `_signature_tokens` indexes ONLY the +N/-N opcode mnemonics of a
# `--ops` delta. When the opcode multiset is IDENTICAL there are no such
# markers, so every pure-reorder signature presents the SAME empty term set and
# `laws --residual` degenerates to a default listing. The RS pilot proved it:
# four distinct pilot signatures returned byte-identical 71992-byte payloads,
# sha1 88dc32585711 for all four.
#
# MEASURED CORPUS SHAPE (RG census, 2026-09-02, 1001 records carrying a
# top-level residual object): 521 signatures yield zero index tokens. Of those,
# 346 are the EMPTY STRING — genuinely unindexable, and no parser fixes that —
# and ~175 are pure-reorder signatures that carry real content the old tokenizer
# threw away.
#
# WHAT A REORDER SIGNATURE ACTUALLY CARRIES. The measured emitted form is
#   "0t opcode multiset IDENTICAL (155/155); insns T155/O155; 0 ops clusters"
# optionally followed by lane prose such as
#   "; real 8 = 1 immediate @0x17c + a 3-atom epilogue permutation
#    ['li','stw','lwz'] at [0x1a4,0x1b4)"
# So the indexable facts are: the identical-multiset SENTINEL, the instruction
# count, the cluster count, and — when a lane wrote them — differing-word and
# atom counts, an opcode list, and shape flags. Those are typed into FACET
# TOKENS here, which is what makes two reorder signatures comparable at all.
#
# HONEST BOUND, stated because the ranking downstream depends on it: a bare
# reorder signature's whole content is (insns, clusters). Two functions sharing
# those two numbers have little else in common, so facet similarity over bare
# reorder signatures is a WEAK predictor of which source-level edit closes the
# residual — it discriminates the query (which is the fix) without claiming to
# rank cures. The strong descriptor is regnorm's genuine/unpaired/crossing
# tuple, which CANNOT be backfilled: a closed function no longer has the
# residual to measure. That is why the richer facets below are read when a lane
# recorded them and never invented when it did not.
# NOTE THE PUNCTUATION CLASS, and it is not cosmetic. The STORED corpus
# signatures read "opcode multiset IDENTICAL (155/155)" while LIVE
# `fndiff --ops` emits "opcode multiset: IDENTICAL (33/33)" with a colon. A
# regex written from the stored form alone parses every live signature as
# `empty` — measured here: all 13 pure-reorder acceptance rows classified
# `empty` on the first run, which is the same class of defect this whole lane
# exists to remove (a query silently asking nothing).
_REORDER_SENTINEL_RE = re.compile(
    r"multiset[:\s]+IDENTICAL\s*\(\s*(\d+)\s*/\s*(\d+)\s*\)", re.I)
_SIG_INSNS_RE = re.compile(r"insns\s+T\s*(\d+)\s*/\s*O\s*(\d+)", re.I)
_SIG_CLUSTERS_RE = re.compile(r"(\d+)\s+ops\s+clusters", re.I)
# The STORED form says "N ops clusters"; the LIVE form never does — it prints
# the clusters themselves and, when any is flagged, a "1 of 2 clusters
# flagged" line. Both are read so a lane pasting live output gets the same
# facets as one quoting a recorded signature.
_SIG_CLUSTERS_FLAGGED_RE = re.compile(r"\d+\s+of\s+(\d+)\s+clusters", re.I)
_SIG_CLUSTER_LINE_RE = re.compile(
    r"^\s*(delete|insert|replace)\s+T\[", re.I | re.M)
_SIG_WORDS_RE = re.compile(r"(\d+)\s+differing\s+words", re.I)
_SIG_ATOMS_RE = re.compile(r"(\d+)[-\s]atom", re.I)
# RUN 33, from the MB lane via the integrator, and it is the sharpest thing
# known about this population: a signature recording ONLY the multiset delta
# serialises a function whose entire residual is same-opcode IMMEDIATES as
# "0t IDENTICAL" — INDISTINGUISHABLE from a genuinely closed function.
# Measured: DrawPsysSub's stored signature read `0t (290/290)` while a live
# fndiff showed 49 IMMEDIATE rows (frame-slot displacements), and the stale
# label sent a whole charter down the wrong class. So "IDENTICAL u0 i49" and
# "IDENTICAL u4 i0" are DIFFERENT FAMILIES and the index must separate them.
# Both the canonical short form (`u4 i49 g3`) and the long prose lanes
# actually write ("49 immediate rows", "4 unpaired", "3 genuine") are read.
_SIG_UNPAIRED_RE = re.compile(
    r"(?:\bu(\d+)\b|(\d+)\s+unpaired)", re.I)
_SIG_IMMEDIATE_RE = re.compile(
    r"(?:\bi(\d+)\b|(\d+)\s+immediate(?:\s+rows?)?)", re.I)
_SIG_GENUINE_RE = re.compile(
    r"(?:\bg(\d+)\b|(\d+)\s+genuine)", re.I)
_SIG_REAL_RE = re.compile(r"\breal\s+(\d+)", re.I)
_SIG_OPLIST_RE = re.compile(r"\[([^\[\]]*?)\]")
_SIG_MNEMONIC_RE = re.compile(r"'([a-z][a-z0-9_.]*)'")
# Shape words a lane writes about a reorder residual. Kept SMALL and literal:
# an open-ended prose vocabulary is the 30-50%-precision extraction the BF lane
# measured and the RS lane's discarded keyword scorer, both recorded failures.
_SIG_SHAPE_FLAGS = (
    ("immediate", "immediate"),
    ("permutation", "permutation"),
    ("shiftable", "shiftable"),
    ("balanced", "balanced"),
    ("epilogue", "epilogue"),
    ("prologue", "prologue"),
)
REORDER_INSN_BANDS = (15, 31, 63, 127, 255, 511, 1023)

# Facet weights. A facet's weight is how much SHARING it should count for, and
# they are deliberately ordered: a shared opcode is real evidence, a shared
# instruction band is a coincidence two hundred records also share.
RESIDUAL_FACET_WEIGHTS: dict[str, float] = {
    # The MB row-shape facets outrank everything else in the reorder class:
    # they are the only facts that separate "IDENTICAL u0 i49" from
    # "IDENTICAL u4 i0", which are different families entirely.
    "immediates": 1.00,
    "unpaired": 0.95,
    "genuine": 0.95,
    "op": 0.90,
    "insns": 0.60,
    "words": 0.35,
    "atoms": 0.35,
    "insnband": 0.45,
    "clusters": 0.35,
    "flag": 0.25,
    "kind": 0.00,      # sentinel: gates the comparison, scores nothing
    "resolution": 0.00,  # honesty metadata, reported not scored
    "parity": 0.10,
}


def _insn_band(count: int) -> int:
    for high in REORDER_INSN_BANDS:
        if count <= high:
            return high
    return 9999


def _facet_weight(facet: str) -> float:
    return RESIDUAL_FACET_WEIGHTS.get(facet.split(":", 1)[0], 0.20)


def parse_residual_signature(signature: str | None) -> dict[str, Any]:
    """Typed facets of a recorded `fndiff --ops` residual signature.

    Returns a dict with a ``kind`` of ``reorder`` (opcode multiset IDENTICAL —
    the schedule/recolor class the exact-match mandate concentrates on),
    ``asymmetric`` (carries +N/-N opcode markers) or ``empty`` (nothing
    indexable — 346 corpus records are in this state and a parser cannot
    rescue them), plus the ``facets`` token list the index is built on.
    """
    text = signature if isinstance(signature, str) else ""
    out: dict[str, Any] = {
        "kind": "empty", "insns_target": None, "insns_ours": None,
        "clusters": None, "real": None, "words": None, "atoms": None,
        "unpaired": None, "immediates": None, "genuine": None,
        "opcodes": [], "flags": [], "facets": [], "resolution": None,
    }
    if not text.strip():
        return out
    lowered = text.lower()
    facets: list[str] = []

    sentinel = _REORDER_SENTINEL_RE.search(text)
    opcode_markers = sorted(_SIGNATURE_OPCODE_RE.findall(lowered))
    if sentinel:
        out["insns_target"] = int(sentinel.group(1))
        out["insns_ours"] = int(sentinel.group(2))
        # THE MB SEPARATION, and `fndiff --ops` already emits it live:
        #   "IDENTICAL (52/52) -- but 4 IMMEDIATE word(s) differ at aligned
        #    same-opcode positions: NOT pure reorder, NOT schedule-class"
        # An identical multiset with differing immediates is a DIFFERENT
        # FAMILY from a true reorder — frame-slot displacements, not a
        # schedule — so it gets its own kind and can never be paired with one.
        # The stored corpus signatures lost this: DrawPsysSub serialised as
        # `0t (290/290)` while carrying 49 immediate rows.
        out["kind"] = ("immediate-aligned"
                       if ("not pure reorder" in lowered
                           or "immediate word" in lowered)
                       else "reorder")
    elif opcode_markers:
        out["kind"] = "asymmetric"
    facets.append(f"kind:{out['kind']}")

    insns = _SIG_INSNS_RE.search(text)
    if insns:
        out["insns_target"] = int(insns.group(1))
        out["insns_ours"] = int(insns.group(2))
    if out["insns_target"] is not None:
        facets.append(f"insns:{out['insns_target']}")
        facets.append(f"insnband:{_insn_band(out['insns_target'])}")
        # Count parity is informative ONLY for a count-asymmetric signature.
        # Under an IDENTICAL multiset it is implied by the sentinel — T==O
        # always — so emitting it there gives every reorder record a facet in
        # common with every reorder query, which re-creates the constant list
        # this index exists to break. Measured: with `parity:held` emitted for
        # reorder signatures, 7 of the 13 acceptance rows still selected one
        # identical 183-record set.
        if (out["insns_ours"] == out["insns_target"]
                and out["kind"] == "asymmetric"):
            facets.append("parity:held")

    clusters = _SIG_CLUSTERS_RE.search(text)
    flagged = _SIG_CLUSTERS_FLAGGED_RE.search(text)
    cluster_lines = len(_SIG_CLUSTER_LINE_RE.findall(text))
    if clusters:
        out["clusters"] = int(clusters.group(1))
    elif flagged:
        out["clusters"] = int(flagged.group(1))
    elif cluster_lines:
        out["clusters"] = cluster_lines
    if out["clusters"] is not None:
        # Exact for the small counts that discriminate; banded above, where the
        # exact number is noise.
        facets.append(f"clusters:{out['clusters']}"
                      if out["clusters"] <= 4 else "clusters:5+")

    for attr, pattern in (("words", _SIG_WORDS_RE), ("atoms", _SIG_ATOMS_RE),
                          ("real", _SIG_REAL_RE)):
        found = pattern.search(text)
        if found:
            out[attr] = int(found.group(1))
    if out["words"] is not None:
        facets.append(f"words:{out['words']}")
    if out["atoms"] is not None:
        facets.append(f"atoms:{out['atoms']}")

    # The MB separation: unpaired / immediate / genuine row counts.
    for attr, pattern in (("unpaired", _SIG_UNPAIRED_RE),
                          ("immediates", _SIG_IMMEDIATE_RE),
                          ("genuine", _SIG_GENUINE_RE)):
        found = pattern.search(text)
        if found:
            value = found.group(1) or found.group(2)
            if value is not None:
                out[attr] = int(value)
                facets.append(f"{attr}:{out[attr]}")
    if out["kind"] in ("reorder", "immediate-aligned"):
        # RESOLUTION is an honesty facet, not a similarity facet: it records
        # whether this signature can tell a CLOSED function from one carrying
        # only same-opcode immediate rows. A `multiset-only` reorder signature
        # cannot — that is the DrawPsysSub defect — so it must never be read
        # as evidence of a closed or nearly-closed residual, and it must not
        # be treated as a confident neighbour of a resolved one.
        resolved = any(out[key] is not None
                       for key in ("unpaired", "immediates", "genuine"))
        out["resolution"] = "row-resolved" if resolved else "multiset-only"
        facets.append(f"resolution:{out['resolution']}")

    mnemonics: set[str] = set(opcode_markers)
    for group in _SIG_OPLIST_RE.findall(text):
        mnemonics.update(_SIG_MNEMONIC_RE.findall(group.lower()))
    out["opcodes"] = sorted(mnemonics)
    facets.extend(f"op:{name}" for name in out["opcodes"])

    for word, flag in _SIG_SHAPE_FLAGS:
        if word in lowered:
            out["flags"].append(flag)
            facets.append(f"flag:{flag}")

    out["facets"] = sorted(set(facets))
    return out


def residual_facets(signature: str | None) -> set[str]:
    """The indexable facet tokens of a residual signature."""
    return set(parse_residual_signature(signature)["facets"])


def residual_facet_similarity(
    query: Iterable[str], candidate: Iterable[str]
) -> tuple[float, list[str]]:
    """Weighted overlap of two facet sets, in [0, 1], plus the shared facets.

    Normalised by the QUERY's own weight so the score answers "how much of what
    I asked about does this row share", not "how similar are two rows" — an
    unshared facet on the candidate is not evidence against it.
    """
    # ONLY POSITIVE-WEIGHT FACETS PARTICIPATE. `kind:` and `resolution:` are
    # metadata — a sentinel that gates the comparison and an honesty label —
    # and both weigh 0. Leaving them in the set made every reorder record
    # share a facet with every reorder query, so the SELECTED SET was constant
    # across all 13 pure-reorder acceptance rows and only the ORDER varied.
    # That is precisely the PX failure mode ("any retrieval A/B that counts
    # constant lists is insensitive by construction"), reproduced inside the
    # fix for it, and caught by the acceptance measurement rather than by
    # reasoning.
    query_set = {f for f in query if _facet_weight(f) > 0}
    candidate_set = set(candidate)
    total = sum(_facet_weight(f) for f in query_set)
    if total <= 0:
        return 0.0, []
    shared = sorted(query_set & candidate_set)
    got = sum(_facet_weight(f) for f in shared)
    return min(1.0, got / total), shared


def _derive_residual_index(connection: sqlite3.Connection) -> None:
    """Rebuild `residual_signature` from scratch. Called once per build.

    A pure projection of rows already imported, TRUNCATED first so a rebuild is
    idempotent and a stale row from a deleted record cannot survive — the same
    property that makes `law_evidence` auditable, and the reason
    `recount_derived_tables` can check it against the raw JSON.
    """
    connection.execute("DELETE FROM residual_signature")
    rows = connection.execute(
        "SELECT record_id, raw_json FROM record_ingest").fetchall()
    for row in rows:
        try:
            record = json.loads(row["raw_json"])
        except (TypeError, json.JSONDecodeError):
            continue
        if not isinstance(record, dict):
            continue
        # TOP-LEVEL ONLY, exactly as `_iter_attempt_residuals` reads it:
        # attributes.residual is legacy free prose on 654 records.
        residual = record.get("residual")
        if not isinstance(residual, dict):
            continue
        parsed = parse_residual_signature(residual.get("signature"))
        function_key = record.get("function")
        connection.execute(
            "INSERT OR REPLACE INTO residual_signature(record_id,"
            " function_key, outcome, family, capability_needed, kind,"
            " insns_target, insns_ours, clusters, signature, facets_json,"
            " measured_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            (
                row["record_id"],
                str(function_key) if function_key else None,
                str(record.get("outcome")).lower() if record.get("outcome")
                else None,
                residual.get("family"),
                residual.get("capability_needed"),
                parsed["kind"],
                parsed["insns_target"], parsed["insns_ours"],
                parsed["clusters"],
                residual.get("signature") or "",
                json.dumps(parsed["facets"]),
                residual.get("measured_at"),
            ),
        )


def webfrank_pin_mechanisms(
    root: Path = REPO_ROOT, query: str | None = None
) -> list[dict[str, Any]]:
    """Search the `mechanism` prose on config/GUNE5D/webfrank.json pins.

    Requested by the GW lane: a pin's mechanism note carries the full
    derivation of a closed residual — the single densest description of that
    residual class anywhere in the project — but it lived only in a config
    file that no query surface read, so a lane hunting the same signature
    could not find the derivation that already closed it.
    """
    path = root / "config" / "GUNE5D" / "webfrank.json"
    if not path.exists():
        return []
    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError):
        return []
    tokens = _query_tokens(query) if query else []
    out: list[dict[str, Any]] = []
    for unit, rules in (data.get("units") or {}).items():
        if not isinstance(rules, list):
            continue
        for rule in rules:
            if not isinstance(rule, dict):
                continue
            mechanism = rule.get("mechanism")
            if not isinstance(mechanism, str):
                continue
            function = rule.get("function") or ""
            haystack = f"{unit} {function} {mechanism}".lower()
            if tokens and not all(token in haystack for token in tokens):
                continue
            stages = sorted(
                key for key in rule
                if key not in ("function", "before_sha256", "after_sha256",
                               "mechanism", "audit")
            )
            text = " ".join(mechanism.split())
            out.append({
                "unit": unit,
                "function": function,
                "stages": stages,
                "cites_records": sorted(set(re.findall(
                    r"\b(?:claim|attempt|evidence)\.[A-Za-z0-9._-]+", text))),
                "mechanism": text[:600] + (" …" if len(text) > 600 else ""),
                "source": "config/GUNE5D/webfrank.json",
                "note": "a PIN, not a law: its source is FROZEN — screen it"
                        " before editing the function (AGENTS.md trap 4)",
            })
    return out


def _load_law_evidence(connection: sqlite3.Connection) -> dict[str, dict[str, Any]]:
    """Read the derived table into score rows keyed by law id."""
    out: dict[str, dict[str, Any]] = {}
    for row in connection.execute(
        "SELECT law_record_id, successes, failures, neutral_citations,"
        " cited_total, latest_evidence_at, success_records, failure_records"
        " FROM law_evidence"
    ).fetchall():
        score = law_evidence_score(int(row["successes"]), int(row["failures"]))
        score.update({
            "neutral_citations": int(row["neutral_citations"]),
            "cited_total": int(row["cited_total"]),
            "latest_evidence_at": row["latest_evidence_at"],
        })
        for key, column in (("success_records", "success_records"),
                            ("failure_records", "failure_records")):
            try:
                score[key] = json.loads(row[column])
            except (TypeError, json.JSONDecodeError):
                score[key] = []
        out[row["law_record_id"]] = score
    return out


def regime_events(
    *,
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
) -> list[dict[str, Any]]:
    """Every recorded regime-change event, newest first."""
    ensure_database(root, db_path)
    with closing(open_database(root, db_path)) as connection:
        rows = connection.execute(
            "SELECT record_id, slug, scope, occurred_at, note FROM regime_event"
            " ORDER BY occurred_at DESC, slug"
        ).fetchall()
    return [dict(row) for row in rows]


def _event_matches(event: Mapping[str, Any], scope: str | None,
                   tags: Iterable[str], record_id: str = "") -> bool:
    """Does this event's scope cover a law with this scope prose, tags, or id?

    Matching is deliberately coarse and OR-shaped: a regime change that a
    lane bothered to record should over-flag rather than under-flag, because
    the cost of a spurious banner is one re-read and the cost of a missed one
    is a lane applying a law the ground truth no longer supports.

    THE ID SLUG IS A MATCH TARGET, and that is not a convenience. Measured on
    the 2026-09-02 corpus: only 124 of 357 laws carry an `attributes.scope`
    at all, and those that do carry compiler-family boilerplate ("MWCC GC
    1.2.5 family; ...") rather than the subject. A `--scope regnorm` event
    matched ZERO laws by scope prose while eight laws carry `regnorm` in
    their id. Scope-prose-only matching would have shipped a feature that
    silently never fires — the same failure mode the run-29 lane recorded
    when the prose index could not find a family name.
    """
    event_scope = str(event.get("scope") or "").strip()
    if not event_scope or event_scope in ("*", "all", "project"):
        return True
    lowered = event_scope.lower()
    if lowered in {str(tag).lower() for tag in tags}:
        return True
    if scope and lowered in scope.lower():
        return True
    # Match the id on WORD boundaries of the slug, so `--scope cse` does not
    # sweep in every law whose id contains the letters "cse" inside another
    # word. Path-form scopes are compared on their basename stem too.
    slug_words = set(_slug_words(record_id or ""))
    needles = {lowered}
    tail = lowered.replace("\\", "/").rsplit("/", 1)[-1]
    if tail:
        needles.add(tail.rsplit(".", 1)[0])
    return bool(needles & slug_words)


def _revalidation_banner(
    evidence_at: str | None,
    scope: str | None,
    tags: Iterable[str],
    events: Iterable[Mapping[str, Any]],
    record_id: str = "",
) -> dict[str, Any] | None:
    """Flag a law whose newest evidence predates a matching regime event.

    EVENT-BASED, never calendar decay. A law does not become less true
    because a month passed; it becomes unreliable because the tool, the
    compiler, or the class that produced its evidence changed underneath it.
    """
    tags = list(tags)
    stale_events = [
        event for event in events
        if _event_matches(event, scope, tags, record_id)
        and str(event.get("occurred_at") or "") >
        str((evidence_at or "")[:10] or "")
    ]
    if not stale_events:
        return None
    return {
        "banner": "NEEDS REVALIDATION",
        "evidence_predates": [
            {"slug": event.get("slug"), "scope": event.get("scope"),
             "occurred_at": event.get("occurred_at"),
             "record": event.get("record_id")}
            for event in stale_events
        ],
        "newest_evidence_at": evidence_at,
        "why": (
            "every measurement backing this law predates a recorded"
            " regime change covering its scope. The law is not refuted —"
            " it is UNVERIFIED under the current regime. Re-measure before"
            " citing it, and the re-measurement itself refreshes this flag."
        ),
    }


def law_corpus(
    query: str | None = None,
    *,
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    tag: str | None = None,
    full: int = 0,
    limit: int = 100,
    residual: str | None = None,
    include_provisional: int = 0,
    rank: int = 1,
) -> dict[str, Any]:
    """List the codegen-law corpus, newest first, with freshness and supersession.

    ``tag`` filters on the structured ``attributes.tags`` array (e.g.
    ``core-screen``, ``alias-form``, ``entry-schedule``) — the curated
    applicability vocabulary, cheaper and more precise than prose search.
    ``full=1`` inlines each law's complete text (one round trip for a whole
    tagged screen set instead of a `record <id>` call per law).

    ``query`` matches the law's prose AND its id's SLUG WORDS, so a family
    name typed as separate words finds the family (the prose-only index
    returned nothing for exactly that shape). It additionally searches the
    `mechanism` notes on webfrank.json pins, returned as ``pin_mechanisms``.

    ``residual`` takes a `--ops` token delta (e.g. ``"+1 addi -1 li"``) and
    finds the laws and sibling attempt records whose recorded
    ``residual.signature`` shares mnemonics with it — "who else had this
    exact residual", which no facet could ask before.
    """
    ensure_database(root, db_path)
    sql = f"""
        SELECT r.record_id, r.record_state, r.valid_from, r.recorded_at,
               c.epistemic_state, c.value_json,
               json_extract(r.raw_json, '$.attributes.scope') AS scope,
               json_extract(r.raw_json, '$.attributes.tags') AS tags,
               COALESCE(c.superseded_by, sup.newer_id) AS superseded_by,
               (SELECT COUNT(*) FROM attempt_law_application ala
                WHERE ala.law_record_id = r.record_id) AS applied_count
        FROM claim c JOIN record_ingest r ON r.record_id = c.record_id
        LEFT JOIN ({SUPERSEDING_RECORD_BY_TARGET}) sup
               ON sup.old_id = r.record_id
        WHERE (c.predicate = 'codegen_law' OR r.record_id LIKE '%law%')
    """
    params: list[Any] = []
    if tag:
        sql += " AND json_extract(r.raw_json, '$.attributes.tags') LIKE ?"
        params.append(f'%"{tag}"%')
    # The query filter is applied in PYTHON, not SQL: slug-word matching
    # cannot be expressed as a LIKE, and the corpus is small enough that
    # fetching then filtering is cheaper than a second index.
    # Always fetch wide and truncate in Python AFTER ranking: an SQL LIMIT
    # here is a date-ordered cut, which is exactly the wrong prefix to keep
    # once rows are ranked by evidence.
    sql += " ORDER BY COALESCE(r.valid_from, '') DESC, r.record_id LIMIT ?"
    params.append(5000)
    with closing(open_database(root, db_path)) as connection:
        rows = connection.execute(sql, params).fetchall()

    residual_matches: list[dict[str, Any]] = []
    residual_law_ids: set[str] = set()
    residual_parsed: dict[str, Any] | None = None
    if residual:
        residual_parsed = parse_residual_signature(residual)
        wanted_tokens = _signature_tokens(residual)
        wanted_facets = set(residual_parsed["facets"])
        for record, obj in _iter_attempt_residuals(root):
            signature = obj.get("signature") or ""
            # OPCODE path, unchanged: a count-asymmetric residual is matched on
            # its shared +N/-N mnemonics, which is exact where those exist.
            shared = _signature_tokens(signature) & wanted_tokens
            candidate = parse_residual_signature(signature)
            strength, shared_facets = residual_facet_similarity(
                wanted_facets, candidate["facets"])
            # REORDER path, added run 33: a pure-reorder query carries no
            # mnemonics, so it is matched on typed facets against the other
            # reorder rows. The KIND must agree — pairing a reorder signature
            # with a count-asymmetric one is the false neighbour that makes an
            # undiscriminated listing look like a result.
            same_kind = (residual_parsed["kind"] != "empty"
                         and candidate["kind"] == residual_parsed["kind"])
            if not shared and not (same_kind and shared_facets):
                continue
            applied = _law_id_list(record, "laws_applied")
            residual_law_ids.update(applied)
            fn_name = str(record.get("function", "")).split(":", 1)[-1]
            residual_matches.append({
                "record": record.get("id"),
                "function": fn_name or None,
                "outcome": record.get("outcome"),
                "signature": signature,
                "family": obj.get("family"),
                "capability_needed": obj.get("capability_needed"),
                "measured_at": obj.get("measured_at"),
                "shared_tokens": sorted(shared),
                "kind": candidate["kind"],
                "resolution": candidate.get("resolution"),
                "shared_facets": shared_facets,
                "match_strength": round(strength, 4),
                "laws_applied": applied,
            })
        residual_matches.sort(
            key=lambda row: (-len(row["shared_tokens"]),
                             -row["match_strength"], row["record"] or ""))

    tokens = _query_tokens(query) if query else []
    # Per-term OR diagnostics (run 34 item 6). Silent AND-combining returned
    # 0 on a spread query like "reloc blind real naming" — every term is
    # real, but no single law carries all four, so the whole-phrase and
    # all-tokens-slug tests both failed and the lane read the graph as
    # silent. Now each term is matched independently, a law is selected if it
    # carries ANY term, and the ranking below is by how many DISTINCT terms
    # it carries. Per-term corpus hit counts are returned so a zero-hit term
    # (a typo, or a vocabulary the corpus does not use) is visible.
    query_term_hits = {t: 0 for t in tokens}
    terms_by_id: dict[str, int] = {}
    selected = []
    for row in rows:
        if not (query or residual):
            selected.append((row, None))
            continue
        why: list[str] = []
        terms_matched: set[str] = set()
        if query:
            lowered = query.lower()
            rid = (row["record_id"] or "").lower()
            prose = (row["value_json"] or "").lower()
            scope_l = (row["scope"] or "").lower()
            slug_ws = _slug_words(row["record_id"])
            # Whole-phrase / all-tokens matches keep their precise labels.
            if lowered in rid:
                why.append("id")
            if lowered in prose:
                why.append("text")
            if lowered in scope_l:
                why.append("scope")
            if _slug_match(row["record_id"], tokens):
                why.append("slug")
            # Per-term OR matching across slug words, prose and scope. NOT the
            # raw id: matching a bare term against the whole id makes the
            # version/date suffix ("v1", "20260831") a content term, so every
            # `.v1` law would match "v1". _slug_words already strips those.
            for term in tokens:
                if (term in prose or term in scope_l
                        or any(term in word for word in slug_ws)):
                    terms_matched.add(term)
                    query_term_hits[term] += 1
            if terms_matched and not why:
                why.append("terms:" + "+".join(sorted(terms_matched)))
        if residual and row["record_id"] in residual_law_ids:
            why.append("residual-sibling")
        if why or terms_matched:
            terms_by_id[row["record_id"]] = len(terms_matched)
            selected.append((row, why))
    # NOTE: truncation to `limit` happens AFTER scoring below, not here. It
    # used to happen at this point, which meant a query matching more laws
    # than the limit kept the DATE-newest prefix and threw away the
    # best-evidenced rows before anything could rank them.

    laws = []
    for row, why in selected:
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
                "falsifier": None,
                "asserted_by": None,
                "match": why,
                "query_terms_matched": terms_by_id.get(row["record_id"], 0),
            }
        )

    # --- run-32 evidence layer -------------------------------------------
    # Scores are attached AFTER text filtering and BEFORE truncation, so the
    # ranking sees the whole matched set rather than an arbitrary prefix of
    # it (the limit used to slice by date, which silently hid the best-scored
    # rows whenever a query matched more laws than the limit).
    with closing(open_database(root, db_path)) as connection:
        evidence = _load_law_evidence(connection)
    events = regime_events(root=root, db_path=db_path)
    for row in laws:
        score = evidence.get(row["id"]) or law_evidence_score(0, 0)
        row["evidence"] = score
        row["score"] = score["wilson"]
        row["n"] = score["n"]
        row["status"] = score["status"]
        banner = _revalidation_banner(
            score.get("latest_evidence_at") or row["recorded_at"]
            or row["valid_from"],
            row["scope"], row["tags"], events, row["id"])
        if banner:
            row["needs_revalidation"] = banner
    # PROVISIONAL SUPPRESSION, and the one place it must NOT apply.
    #
    # Hiding unverified laws from the browse is the point of deliverable 3: a
    # law with no verified success that reads as authoritative is how one
    # lane's confident guess becomes fleet-wide doctrine. But suppression is a
    # DELETION, and deleting rows out of a set somebody asked for by name is a
    # different act from ranking a browse.
    #
    # MEASURED, and the reason for this branch: `--tag core-screen` is the
    # MANDATORY de-fakematch screen (AGENTS.md: "Fetch your law screen in ONE
    # call"). Of its 33 laws, 7 are provisional. Suppressing inside a tag
    # filter would have silently removed 21% of a mandatory screen — every
    # lane would have run a quieter screen and never known. A screen that
    # fails open is worse than an unranked one.
    #
    # The same argument covers --query and --residual, which are TARGETED
    # retrievals: a lane searching a residual signature for the one law that
    # explains it, finding nothing, concludes the graph is silent — and
    # AGENTS.md already names that false conclusion as a failure mode, with
    # claim.find-subcommand-caps-at-100-and-silently-falsifies-park-screens
    # as the recorded instance of a filter quietly falsifying a screen.
    #
    # So suppression applies to exactly one surface: the UNFILTERED ranked
    # browse, which is the "deterministic view" the contract names. Every
    # targeted request keeps its provisional matches, clearly labelled
    # `status: provisional` and counted in `provisional_retained`.
    hidden_provisional = 0
    provisional_retained = 0
    provisional_rows: list[dict[str, Any]] = []
    targeted = bool(tag or query or residual)
    if targeted:
        provisional_retained = sum(
            1 for row in laws if row["status"] == "provisional")
    elif not include_provisional:
        kept = [row for row in laws if row["status"] != "provisional"]
        provisional_rows = [row for row in laws
                            if row["status"] == "provisional"]
        hidden_provisional = len(provisional_rows)
        laws = kept
    # RESIDUAL RELEVANCE (run 33, RG, prompted by the PX 24-row baseline).
    # PX measured that `laws --residual` returned ONE constant 6-law payload
    # for all 13 pure-reorder rows and a second constant 5-law payload for 4
    # more — zero discriminating payloads across 24 queries — and that `brief`
    # ships a 59-law matching_laws list byte-identical for every TU. A
    # constant list selects nothing, so ranking it FOR THIS QUERY is the whole
    # value. Each law is scored by the best-matching cohort record that cites
    # it (similarity x outcome weight), damped by how ubiquitous the law is in
    # the cohort — a law every record cites carries no information about which
    # of them resembles you.
    if residual and residual_matches:
        cohort_size = len(residual_matches)
        support: dict[str, float] = {}
        document_freq: dict[str, int] = {}
        for match in residual_matches:
            for law_id in match.get("laws_applied") or []:
                document_freq[law_id] = document_freq.get(law_id, 0) + 1
                weight = (match["match_strength"]
                          * SIMILAR_OUTCOME_TIER.get(
                              str(match.get("outcome") or "").lower(), 0.2))
                if weight > support.get(law_id, 0.0):
                    support[law_id] = weight
        for row in laws:
            raw_support = support.get(row["id"], 0.0)
            freq = document_freq.get(row["id"], 0)
            damp = (math.log(max(cohort_size, 2) / freq)
                    if freq else 0.0)
            row["residual_relevance"] = round(raw_support * damp, 4)
            row["residual_cohort_citations"] = freq
    if rank:
        if residual and residual_matches:
            # Tier still outranks everything — a refuted law must not float on
            # relevance — but WITHIN a tier the query decides the order.
            laws.sort(key=lambda row: (
                LAW_STATUS_ORDER.index(row["status"])
                if row["status"] in LAW_STATUS_ORDER else len(LAW_STATUS_ORDER),
                -float(row.get("residual_relevance") or 0.0),
                -float(row["score"] or 0.0), -int(row["n"] or 0), row["id"]))
        elif query and tokens:
            # OR-rank (run 34 item 6): tier FIRST (a refuted law must not
            # float on term overlap, exactly as in the residual branch), then
            # the number of distinct query terms the law carries, then
            # evidence. A 3-of-4-term law outranks a 1-of-4 one instead of
            # both being dropped by the old AND filter.
            laws.sort(key=lambda row: (
                LAW_STATUS_ORDER.index(row["status"])
                if row["status"] in LAW_STATUS_ORDER else len(LAW_STATUS_ORDER),
                -int(row.get("query_terms_matched") or 0),
                -float(row["score"] or 0.0), -int(row["n"] or 0), row["id"]))
        else:
            laws.sort(key=lambda row: law_score_sort_key(
                {"status": row["status"], "wilson": row["score"],
                 "n": row["n"], "id": row["id"]}))
    truncated = max(0, len(laws) - limit)
    laws = laws[:limit]

    # falsifier/asserted_by come from the raw record, not the claim table.
    if laws:
        with closing(open_database(root, db_path)) as connection:
            marks = ",".join("?" * len(laws))
            for extra in connection.execute(
                f"SELECT record_id, raw_json FROM record_ingest"
                f" WHERE record_id IN ({marks})",
                [row["id"] for row in laws],
            ).fetchall():
                try:
                    record = json.loads(extra["raw_json"])
                except json.JSONDecodeError:
                    continue
                for row in laws:
                    if row["id"] != extra["record_id"]:
                        continue
                    row["falsifier"] = _record_field(record, "falsifier")
                    row["asserted_by"] = _record_field(record, "asserted_by")
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
    if rank and provisional_rows:
        provisional_rows.sort(key=lambda row: law_score_sort_key(
            {"status": row["status"], "wilson": row["score"], "n": row["n"],
             "id": row["id"]}))
    out: dict[str, Any] = {
        "laws": laws,
        "count": len(laws),
        "tags_available": dict(sorted(tag_counts.items())),
        "ranked_by": ("status tier, then Wilson lower bound (z=1.96), then"
                      " sample size" if rank else "valid_from (unranked)"),
        "hidden_provisional": hidden_provisional,
        # SEGREGATED, NOT DELETED. The contract excludes provisional laws from
        # the deterministic view, and they are excluded — from `laws`. They
        # are still returned here, in the same response, because AGENTS.md
        # makes the unfiltered `laws` call the corpus ENUMERATION surface
        # ("read it at the start of every pass") and the run-29 audit already
        # measured the cost of rows being invisible to enumeration: 36 of 136
        # laws silently unreachable behind a limit. Excluding a row from the
        # ranked view is a judgement about authority; making it unreachable
        # is data loss, and only the first was asked for.
        "provisional_laws": provisional_rows,
        "provisional_retained": provisional_retained,
        "provisional_policy": (
            "provisional laws are hidden from the UNFILTERED ranked browse"
            " only. Any targeted request — --tag, --query, --residual — keeps"
            " its provisional matches, labelled and counted in"
            " provisional_retained, because a filter that quietly drops"
            " matches turns 'not verified yet' into 'the graph is silent':"
            " 7 of the 33 core-screen laws are provisional, and that screen"
            " is mandatory. --include-provisional 1 shows them everywhere."
        ),
        "truncated": truncated,
        # Per-term corpus hit counts (run 34 item 6): a zero here is a term
        # the corpus does not use — a typo or the wrong vocabulary — which is
        # the diagnosis a silent AND-combined empty result never gave.
        "query_term_hits": (dict(sorted(query_term_hits.items(),
                                        key=lambda kv: (-kv[1], kv[0])))
                            if query else {}),
        "query_ranked_by": ("status tier, then distinct query terms matched,"
                            " then evidence (OR-ranked)")
                            if (query and tokens) else None,
        "status_legend": dict(LAW_STATUS_NOTES),
        "evidence_note": (
            "SCORES ARE DERIVED, never hand-set: `gdlmem build` recomputes"
            " every one from the citations already in the corpus."
            " successes = attempts naming the law in laws_applied whose"
            " outcome was exact/improved. failures = refutes edges pointing"
            " at the law, plus explicit laws_failed citations."
            " neutral_citations = parked/capped/negative/neutral citations,"
            " which are NOT failures — a law that correctly predicts a park"
            " did its job, and those are excluded from n on purpose."
            " READ n BEFORE READING wilson: n is successes+failures, NOT"
            " cited_total, so a law cited 147 times with 13 landings scores"
            " on n=13. A high wilson on n=1 means one success and no"
            " contradiction, not a proven law."
        ),
        "note": (
            "laws are compiler-scoped observations, not instructions:"
            " re-verify against your target bytes; a superseded_by entry means"
            " read the newer record instead; filter with --tag <name> using"
            " tags_available (core-screen = the mandatory de-fakematch screen)."
            " `match` says WHY a row matched: slug = the id's words, text ="
            " prose, residual-sibling = a recorded residual signature. A law"
            " with falsifier=null asserted before the run-29 gate: treat its"
            " necessity language as unscreened, not as proven."
        ),
    }
    if query:
        out["pin_mechanisms"] = webfrank_pin_mechanisms(root, query)
    if residual:
        out["residual_matches"] = residual_matches
        out["residual_parsed"] = residual_parsed
        out["residual_note"] = (
            "sibling records whose recorded residual.signature shares"
            " mnemonics OR typed facets with your --ops delta."
            " capability_needed names the postprocessor capability that would"
            " unpark the function; query it directly with"
            " `find --capability <name>`. Signatures are measured_at a DATE —"
            " remeasure before trusting one."
        )
        if residual_parsed and residual_parsed["kind"] == "reorder":
            out["residual_note"] += (
                " PURE-REORDER QUERY (opcode multiset IDENTICAL): matched on"
                " typed facets — instruction count/band, cluster count and any"
                " opcode, word, atom or shape facts the sibling's lane"
                " recorded — because such a signature carries no +N/-N"
                " mnemonics at all. READ match_strength BEFORE READING THE"
                " ORDER: a bare reorder signature's entire content is (insns,"
                " clusters), so a row matching on those alone is a WEAK"
                " neighbour, not a prescription. The strong descriptor for"
                " this class is regnorm's genuine/unpaired/crossing tuple,"
                " which cannot be backfilled onto closed functions — run"
                " `regnorm.py <unit> <fn> --map` on YOUR residual and screen"
                " the returned laws' scope lines against its crossing shape."
            )
            if residual_parsed.get("resolution") == "multiset-only":
                out["residual_warning"] = (
                    "MULTISET-ONLY SIGNATURE. Yours records the opcode-multiset"
                    " delta and nothing about ROWS, so it cannot distinguish a"
                    " CLOSED function from one whose entire residual is"
                    " same-opcode immediates: DrawPsysSub's stored signature"
                    " read `0t (290/290)` while a live fndiff showed 49"
                    " IMMEDIATE rows, and that stale label sent a whole"
                    " charter down the wrong class. `IDENTICAL u0 i49` and"
                    " `IDENTICAL u4 i0` are DIFFERENT FAMILIES. Re-measure and"
                    " record the row counts — the canonical short form is"
                    " `u<unpaired> i<immediates> g<genuine>` appended to the"
                    " signature — before trusting any neighbour returned here."
                )
        elif residual_parsed and residual_parsed["kind"] == "empty":
            out["residual_note"] += (
                " YOUR SIGNATURE PARSED AS EMPTY: it carries neither +N/-N"
                " mnemonics nor an identical-multiset sentinel, so nothing was"
                " asked and an empty result is NOT evidence the corpus is"
                " silent. Paste the `fndiff --ops` output verbatim."
            )
        if not query:
            # Pins whose mechanism prose names one of the delta's mnemonics:
            # a closed derivation for the same opcode shape is the cheapest
            # possible read on an open one.
            wanted = _signature_tokens(residual)
            out["pin_mechanisms"] = [
                pin for pin in webfrank_pin_mechanisms(root, None)
                if wanted & _signature_tokens(pin["mechanism"])
            ]
    return out


# --- RG lane (run 33): cross-function transferability ----------------------
#
# MEASURED PROBLEM (claim.measurement.RS_retrieval-pilot-four-functions): the
# per-function `context <fn>` call that AGENTS.md discipline 11 makes mandatory
# step zero returned ZERO transferable items on 4 of 4 closable functions —
# empty three times and self-referential once. Its value on that population was
# screening for parks and vetoes, not finding cures. Nothing in the surface
# answered "who else had a residual shaped like mine, and what closed it".
#
# The self-reference screen below is the RS protocol's, promoted from a scoring
# rule to a product rule: a record ANCHORED TO THE FUNCTION UNDER TEST is that
# function's own write-up, and returning it as transferable evidence is the
# graph remembering its own answer. Measured instance: `context
# PlayerCollidePlayers` returned exactly one record — the closing agent's own,
# naming the cure outright.
SIMILAR_OUTCOME_TIER: dict[str, float] = {
    "exact": 1.00, "improved": 0.85, "capped": 0.40, "parked": 0.35,
    "negative": 0.20, "neutral": 0.20,
}
SIMILAR_LAW_TIER: dict[str, float] = {
    "established": 1.0, "contested": 0.5, "provisional": 0.35, "refuted": 0.05,
}
# Fixed a priori and reported with the acceptance table, so the ranking cannot
# be quietly tuned to whichever functions were used to measure it.
SIMILAR_WEIGHTS: dict[str, float] = {
    "signature": 0.40, "family": 0.20, "tu": 0.15, "outcome": 0.15,
    "law_evidence": 0.10,
}


def _module_for_function(connection: sqlite3.Connection,
                         name: str) -> str | None:
    row = connection.execute(
        "SELECT m.object_name AS tu FROM binary_symbol s"
        " JOIN binary_module m ON m.id = s.module_id"
        " WHERE s.platform='gamecube' AND s.symbol_kind='function'"
        "   AND lower(s.raw_name)=lower(?) LIMIT 1",
        (name,),
    ).fetchone()
    return row["tu"] if row else None


def similar_residuals(
    *,
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    function: str | None = None,
    tu: str | None = None,
    signature: str | None = None,
    family: str | None = None,
    limit: int = 8,
    landed_only: int = 0,
    exclude_tu: str | None = None,
) -> dict[str, Any]:
    """"Similar residuals elsewhere": what closed a residual shaped like this.

    Three cohorts, each labelled on the row so a reader can tell WHY it is
    here: ``signature`` (typed facet overlap with your `--ops` delta, including
    the run-33 pure-reorder facets), ``family`` (the same residual family) and
    ``tu`` (a sibling in the same translation unit, resolved through the symbol
    map because records rarely carry a `tu` field). Ranked by the run-32
    evidence layer as well as by similarity, capped small, and every row
    carries the MECHANISM prose — a row without it is not transferable.
    """
    ensure_database(root, db_path)
    fn_name = (function or "").split(":", 1)[-1] or None
    query_facets: set[str] = set()
    query_kind = "empty"
    if signature:
        parsed = parse_residual_signature(signature)
        query_facets = set(parsed["facets"])
        query_kind = parsed["kind"]
    with closing(open_database(root, db_path)) as connection:
        if fn_name and not tu:
            tu = _module_for_function(connection, fn_name)
        # An agent who already recorded a residual on this function gets its
        # facets and family for free rather than having to retype them.
        if fn_name and (not query_facets or not family):
            for row in connection.execute(
                "SELECT rs.signature, rs.family FROM residual_signature rs"
                " WHERE lower(rs.function_key)=lower(?)"
                " ORDER BY rs.record_id DESC",
                (f"function:{fn_name}",),
            ).fetchall():
                if not query_facets and row["signature"]:
                    parsed = parse_residual_signature(row["signature"])
                    query_facets = set(parsed["facets"])
                    query_kind = parsed["kind"]
                if not family and row["family"] not in (None, "unclassified",
                                                        "no-residual"):
                    family = row["family"]
        evidence = _load_law_evidence(connection)
        # A derived table that is present but EMPTY returns "no similar
        # residuals" — indistinguishable from a genuine silence, which is the
        # precise failure this lane exists to remove. Caught once during
        # development by `recount` (shipped 0 vs 1001 independent), so the
        # condition is reported rather than trusted.
        indexed = int(connection.execute(
            "SELECT COUNT(*) FROM residual_signature").fetchone()[0])
        expected = int(connection.execute(
            "SELECT COUNT(*) FROM record_ingest"
            " WHERE json_extract(raw_json,'$.residual') IS NOT NULL"
        ).fetchone()[0])
        rows = connection.execute(
            f"""
            SELECT r.record_id, r.raw_json, r.valid_from, r.recorded_at,
                   a.outcome AS outcome, a.attempted_axis AS axis,
                   a.residual_class AS residual_class,
                   fe.entity_key AS fn_key,
                   bm.object_name AS tu_name,
                   rs.signature AS signature, rs.family AS family,
                   rs.kind AS sig_kind, rs.facets_json AS facets_json,
                   rs.capability_needed AS capability_needed
            FROM record_ingest r
            JOIN attempt a ON a.record_id = r.record_id
            LEFT JOIN entity fe ON fe.id = a.function_entity_id
            LEFT JOIN binary_symbol bs
                ON fe.entity_key LIKE 'function:%'
               AND bs.raw_name = substr(fe.entity_key, 10)
               AND bs.platform = 'gamecube' AND bs.symbol_kind = 'function'
            LEFT JOIN binary_module bm ON bm.id = bs.module_id
            LEFT JOIN residual_signature rs ON rs.record_id = r.record_id
            WHERE r.record_id NOT IN ({SUPERSEDED_RECORD_IDS})
            """
        ).fetchall()

    candidates: list[dict[str, Any]] = []
    excluded_self = 0
    for row in rows:
        row_fn = (row["fn_key"] or "").split(":", 1)[-1]
        if fn_name and row_fn.lower() == fn_name.lower():
            excluded_self += 1
            continue
        # TU-level self-reference: `brief` already lists this TU's own
        # history, so echoing it back as "transferable" is the same
        # self-reference defect one scope up.
        if exclude_tu and row["tu_name"] == exclude_tu:
            excluded_self += 1
            continue
        outcome = str(row["outcome"] or "").lower()
        tier = SIMILAR_OUTCOME_TIER.get(outcome, 0.0)
        if tier <= 0:
            continue
        if landed_only and outcome not in LAW_SUCCESS_OUTCOMES:
            continue
        try:
            facets = set(json.loads(row["facets_json"] or "[]"))
        except json.JSONDecodeError:
            facets = set()
        strength, shared = 0.0, []
        if query_facets and facets and row["sig_kind"] == query_kind:
            strength, shared = residual_facet_similarity(query_facets, facets)
        family_hit = bool(
            family and row["family"] == family
            and family not in ("unclassified", "no-residual"))
        tu_hit = bool(tu and row["tu_name"] and row["tu_name"] == tu)
        if not (shared or family_hit or tu_hit):
            continue
        try:
            record = json.loads(row["raw_json"])
        except json.JSONDecodeError:
            record = {}
        best_law: dict[str, Any] | None = None
        for law_id in _law_id_list(record, "laws_applied"):
            score = evidence.get(law_id) or law_evidence_score(0, 0)
            weight = (SIMILAR_LAW_TIER.get(score["status"], 0.3)
                      * (0.4 + score["wilson"]))
            if best_law is None or weight > best_law["_weight"]:
                best_law = {"id": law_id, "status": score["status"],
                            "wilson": score["wilson"], "n": score["n"],
                            "_weight": weight}
        law_component = best_law["_weight"] if best_law else 0.0
        why: list[str] = []
        if shared:
            why.append("signature")
        if family_hit:
            why.append("family")
        if tu_hit:
            why.append("tu")
        rank = (SIMILAR_WEIGHTS["signature"] * strength
                + SIMILAR_WEIGHTS["family"] * (1.0 if family_hit else 0.0)
                + SIMILAR_WEIGHTS["tu"] * (1.0 if tu_hit else 0.0)
                + SIMILAR_WEIGHTS["outcome"] * tier
                + SIMILAR_WEIGHTS["law_evidence"] * min(1.0, law_component))
        mechanism = " ".join(str(row["axis"] or "").split())
        candidates.append({
            "record": row["record_id"],
            "function": row_fn or None,
            "tu": row["tu_name"],
            "outcome": outcome,
            "residual_class": row["residual_class"],
            "family": row["family"],
            "capability_needed": row["capability_needed"],
            "match": why,
            "match_strength": round(strength, 4),
            "shared_facets": shared,
            "rank_score": round(rank, 4),
            "age_days": _record_age_days(row["valid_from"],
                                         row["recorded_at"]),
            "top_law": ({k: v for k, v in best_law.items() if k != "_weight"}
                        if best_law else None),
            "mechanism": (mechanism[:400] + " …") if len(mechanism) > 400
                         else mechanism,
            "detail": f"gdlmem.py record {row['record_id']}",
        })
    candidates.sort(key=lambda item: (-item["rank_score"], item["record"]))
    kept = candidates[:max(0, limit)]
    result: dict[str, Any] = {
        "query": {"function": fn_name, "tu": tu, "family": family,
                  "signature_kind": query_kind,
                  "facets": sorted(query_facets)},
        "rows": kept,
        "cohort_size": len(candidates),
        "truncated": max(0, len(candidates) - len(kept)),
        "self_records_excluded": excluded_self,
        "ranked_by": (
            "signature facet overlap %.2f, family %.2f, same-TU %.2f,"
            " outcome tier %.2f, best cited law's run-32 evidence %.2f"
            % (SIMILAR_WEIGHTS["signature"], SIMILAR_WEIGHTS["family"],
               SIMILAR_WEIGHTS["tu"], SIMILAR_WEIGHTS["outcome"],
               SIMILAR_WEIGHTS["law_evidence"])),
        "note": (
            "TRANSFERABLE EVIDENCE, NOT A PRESCRIPTION. Rows are other"
            " functions' residuals; `match` says which cohort put each one"
            " here and `match_strength` how much of your signature it shares."
            " Records anchored to the function you asked about are EXCLUDED"
            " (self_records_excluded) — a function's own write-up is not"
            " transfer. Diagnosis transfers, prescriptions rot (AGENTS.md"
            " residual-work discipline 1): re-derive the mechanism against"
            " your own aligned view before probing it. Pass --residual with"
            " your verbatim `fndiff --ops` output to rank on the signature"
            " cohort; without it only family and TU cohorts are available."
        ),
        # CHANNEL ROLES, stated because the PX lane measured agents misreading
        # them: `context.attempts` is a PARK/VETO screen (empty on 24 of 24
        # closable functions — an empty one is NOT corpus silence, it means
        # nobody has parked this function); THIS section is the cure channel;
        # `laws --residual` is the law channel and is only discriminating for
        # signatures that carry row counts.
        "channel_role": (
            "CURE CHANNEL. context.attempts answers 'has anyone parked or"
            " vetoed an axis on THIS function' and is empty for most closable"
            " functions — that emptiness is not evidence the corpus is silent."
            " This section answers the different question 'who else closed a"
            " residual shaped like mine'."
        ),
    }
    if expected and not indexed:
        result["index_warning"] = (
            f"THE DERIVED RESIDUAL INDEX IS EMPTY while {expected} records"
            " carry a residual object. Every row above is missing, and an"
            " empty result here would otherwise be indistinguishable from a"
            " genuine silence. Rebuild with `gdlmem.py build` and re-run"
            " `gdlmem.py recount` before trusting this section."
        )
    result["indexed_records"] = indexed
    result["indexable_records"] = expected
    return result


def work_claims(
    *,
    root: Path = REPO_ROOT,
    db_path: Path | None = None,
    stale_after: int = 2,
    include_released: int = 0,
    owns: str | None = None,
) -> dict[str, Any]:
    """List work claims with owner, scope, age, and a stale flag.

    Active claims older than ``stale_after`` days are flagged stale per the
    AGENTS.md stale-claim rule; released/done claims are hidden unless
    ``include_released`` is nonzero.

    ``owns`` answers the question a worker actually has before its first
    edit — "may I touch this file?" — instead of making it read every claim's
    scope prose and judge for itself. A foreign ACTIVE claim covering the path
    is a VETO on that whole scope. The screen is substring-based over the
    claim's scope prose and its anchor function name, and it deliberately
    OVER-matches: a spurious hit costs one conversation with the owner, while
    a missed hit costs two fleets editing one TU, which is the coupling
    AGENTS.md forbids outright.
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
        if owns:
            needle = owns.replace("\\", "/").strip().lower()
            haystack = " ".join(filter(None, (
                str(row["scope"] or ""), str(row["function"] or ""),
            ))).replace("\\", "/").lower()
            stem = needle.rsplit("/", 1)[-1].rsplit(".", 1)[0]
            if not (needle and needle in haystack) and not (
                    stem and len(stem) > 2 and stem in haystack):
                continue
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
    result = {
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
    if owns:
        result["owns_query"] = owns
        result["verdict"] = "CLAIMED" if claims else "no claim found"
        result["owns_note"] = (
            "every claim listed here covers the queried path. An ACTIVE claim"
            " owned by someone else is a VETO on its ENTIRE scope, not just"
            " this file — MWCC couples a whole TU through its constant pool,"
            " declaration order and register allocation, so two writers in one"
            " TU is a merge conflict by construction."
            " 'no claim found' is NOT a guarantee: an unpushed claim protects"
            " nothing and cannot be seen from here, and matching is over"
            " scope PROSE. Screen `git log -- <path>` too before a first edit."
        )
    return result


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
    family: str | None = None,
    capability: str | None = None,
    include_candidates: int = 0,
) -> dict[str, Any]:
    """Faceted record search: filter by kind, anchor function, TU, attempt
    outcome, residual class, associated law, residual FAMILY and needed
    CAPABILITY, plus optional FTS terms.

    The TU facet is derived through the symbol import (function -> module),
    so historical records are TU-searchable without carrying a `tu` field.
    The law facet matches structured `laws_applied` links and prose mentions
    (law_screen text) alike.

    ``family`` and ``capability`` read the run-29 ``residual`` object.
    ``capability`` closes the gap the WF lane named highest-leverage: a park
    record can NAME the postprocessor capability that would unpark it (as
    InitControls' did), but until now nothing could ask "which parks are
    waiting on the capability I am about to build", so the payoff of a
    capability could not be priced before building it. Structured hits are
    marked ``match: field``; prose-only hits are marked ``match: prose`` and
    are candidates, not a screen.
    """
    if not any((query, kind, function, tu, outcome, residual, law,
                family, capability)):
        raise MemoryGraphError(
            "find needs at least one facet or search term"
            " (--kind/--function/--tu/--outcome/--residual/--law/"
            "--family/--capability or --query)"
        )
    if family and family not in RESIDUAL_FAMILY_VOCABULARY:
        raise MemoryGraphError(
            f"unknown residual family {family!r}; the contract vocabulary is "
            + ", ".join(sorted(RESIDUAL_FAMILY_VOCABULARY))
            + " — a typo here returns zero rows, which reads as a false"
            " all-clear on a negative screen"
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
    if family:
        # Three tiers, kept SEPARABLE and labelled per row: the verified
        # `family`; the quarantined `family_candidate` (extractor guesses,
        # measured ~30-50% precise — opt-in only, never merged silently);
        # and the legacy coarse residual_class the family is defined against.
        clauses = ["json_extract(r.raw_json, '$.residual.family') = ?"]
        params.append(family)
        if include_candidates:
            clauses.append(
                "json_extract(r.raw_json, '$.residual.family_candidate') = ?")
            params.append(family)
        legacy_class = FAMILY_TO_RESIDUAL_CLASS.get(family)
        if legacy_class:
            clauses.append("UPPER(COALESCE(at.residual_class,'')) LIKE ?")
            params.append(f"%{legacy_class}%")
        sql += " AND (" + " OR ".join(clauses) + ")"
    if capability:
        # Structured OR prose: the structured field is the screen, but the
        # corpus predates it and the naming that motivated this facet lived
        # in prose, so prose hits are surfaced and LABELLED rather than
        # silently dropped.
        sql += (
            " AND (json_extract(r.raw_json, '$.residual.capability_needed')"
            "      LIKE ?"
            "   OR json_extract(r.raw_json,"
            "      '$.attributes.residual.capability_needed') LIKE ?"
            "   OR r.raw_json LIKE ?)"
        )
        pattern = f"%{capability}%"
        params.extend([pattern, pattern, pattern])
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
    # Fetch one past the limit so truncation is DETECTED, never silent: a
    # park screen is a negative test, and a silently-capped result set
    # manufactured false clearances (22 of 47 live vetoes missed in one
    # session — claim.find-subcommand-caps-at-100-and-silently-falsifies-
    # park-screens).
    # A --family query widens into the coarse legacy class, which can return
    # hundreds of rows and bury the handful of EXACT family hits behind
    # them. Fetch generously, rank exact > candidate > fallback in Python,
    # then apply the caller's limit — otherwise the precise answer is the
    # part that gets truncated away.
    fetch = 4000 if family else limit + 1
    params.append(fetch)
    with closing(open_database(root, db_path)) as connection:
        rows = connection.execute(sql, params).fetchall()
    if family:
        def rank(row):
            try:
                record = json.loads(row["raw_json"])
            except json.JSONDecodeError:
                return 3
            obj = record.get("residual")
            obj = obj if isinstance(obj, dict) else {}
            if obj.get("family") == family:
                return 0
            if include_candidates and obj.get("family_candidate") == family:
                return 1
            return 2
        rows = sorted(rows, key=rank)
    truncated = len(rows) > limit
    rows = rows[:limit]
    results = []
    for row in rows:
        try:
            record = json.loads(row["raw_json"])
        except json.JSONDecodeError:
            record = {}
        fn_key = row["fn_key"] or ""
        entry = {
            "id": row["record_id"],
            "kind": row["record_kind"],
            "state": row["record_state"],
            "function": fn_key.split(":", 1)[-1] if fn_key else None,
            "tu": row["tu_name"],
            "outcome": row["outcome"],
            "age_days": _record_age_days(row["valid_from"], row["recorded_at"]),
            "head": _record_head(record),
        }
        residual_obj = record.get("residual")
        if isinstance(residual_obj, dict):
            entry["residual"] = {
                key: residual_obj.get(key) for key in RESIDUAL_FIELDS
            }
        if family:
            obj = residual_obj if isinstance(residual_obj, dict) else {}
            if obj.get("family") == family:
                entry["match"] = "family"
            elif include_candidates and obj.get("family_candidate") == family:
                entry["match"] = "family_candidate"
                entry["candidate_warning"] = (
                    "UNVERIFIED extractor guess (~30-50% precision) — verify"
                    " against the function's aligned diff before relying on"
                    " it. This is NOT a family classification."
                )
            else:
                entry["match"] = "residual_class-fallback"
                entry["fallback_class"] = normalize_residual_class(
                    record.get("residual_class"))
        if capability:
            structured = isinstance(residual_obj, dict) and capability.lower() \
                in str(residual_obj.get("capability_needed") or "").lower()
            entry["match"] = "field" if structured else "prose"
        results.append(entry)
    out = {
        "results": results,
        "count": len(results),
        "truncated": truncated,
        "note": "heads only; fetch full detail with gdlmem.py record <id>",
    }
    if capability:
        out["capability_note"] = (
            "match=field is a structured hit and is screenable; match=prose"
            " is a CANDIDATE found by text and may be an incidental mention"
            " — read it before counting it. Prose hits exist because the"
            " corpus predates residual.capability_needed; annotate the ones"
            " that are real so the next screen is structured."
        )
    if family:
        counts: dict[str, int] = {}
        for row in results:
            counts[row.get("match", "?")] = counts.get(row.get("match"), 0) + 1
        out["family_match_counts"] = counts
        out["family_note"] = (
            "THREE TIERS, ranked and labelled per row — do not total them."
            " match=family is the verified classification and the only tier"
            " usable as a screen. match=family_candidate (only with"
            " --include-candidates) is an UNVERIFIED extractor guess measured"
            " at ~30-50% precision. match=residual_class-fallback is the"
            " legacy coarse class this family is defined against"
            f" ({FAMILY_TO_RESIDUAL_CLASS.get(family, 'n/a')}) — a WIDENING"
            " that says the record is in the right neighbourhood, not that it"
            " is in this family. An empty verified tier means 'nothing"
            " annotated', not 'no such residual exists'."
        )
    if truncated:
        out["warning"] = (
            f"RESULT SET TRUNCATED at limit={limit}: more records match."
            " NEVER use a truncated result as a negative screen (park/veto"
            " checks) — raise --limit or narrow the facets until"
            " truncated=false."
        )
    return out


# Discipline 10b markers. A record ending in a concrete untried hypothesis
# makes that hypothesis MANDATORY STEP 1 for the next lane — one such
# hypothesis, written down and then skipped by its own author, was worth
# -235 real in a single build when finally executed a run later. The brief
# could not surface them, so finding one depended on reading every record in
# full; these are the phrasings the corpus actually uses.
_HYPOTHESIS_MARKERS = (
    "unpark condition", "next hypothesis", "next-hypothesis",
    "untried", "not yet tried", "never tried", "remains to try",
    "the remaining path", "remaining path", "next step", "next lane",
    "worth pricing", "recommendation for the next",
    "mandatory step 1", "would close", "should be tried", "worth trying",
    "the one carrier", "not been tried",
)
_SENTENCE_SPLIT_RE = re.compile(r"(?<=[.;])\s+")


def _ungated_prose_denial(record: dict[str, Any]) -> dict[str, Any] | None:
    """RENDER-FLAG a pre-gate record whose prose denies work with no typed object.

    Flag only. This deliberately does NOT extract the prose into a `denial`
    object, and no future version of it should: the BF family backfill
    measured phrase-extraction precision at 30-50% on this corpus, so
    auto-populating a typed field from prose would manufacture authoritative-
    looking denials that are wrong half the time — strictly worse than the
    prose it replaced, because the typed form is trusted more.

    What the reader gets instead is an honest label: this record denies work
    in prose, it predates the typed-denial gate, and therefore it carries no
    scope, no expiry check, and no falsifier. Treat it as a WEAK veto and say
    so if you re-probe — the same treatment `has_probed_form: false` already
    gets.
    """
    if _record_field(record, "denial"):
        return None  # typed: gated, scoped, expirable — not legacy prose
    substance = _record_text({
        key: value for key, value in record.items() if key != "attributes"
    } | {"attributes": {
        key: value for key, value in (record.get("attributes") or {}).items()
        if key not in _PARK_CITATION_KEYS
    }})
    hit = _DENIAL_PHRASE_RE.search(substance)
    if not hit:
        return None
    return {
        "banner": "UNGATED-PROSE-DENIAL",
        "matched": hit.group(0),
        "why": (
            "this record denies future work in PROSE and carries no typed"
            " `denial` object, so it states no scope, no premise measurement,"
            " no expiry_check command, and no falsifier. It predates the"
            " run-32 gate and is not wrong for that — but it cannot be"
            " screened out, only re-derived. Treat it as a WEAK veto: read"
            " the record, re-measure its premise, and if you re-probe the"
            " axis, supersede it with a typed denial so the next lane"
            " inherits something checkable."
        ),
        "not_extracted": (
            "the phrase is FLAGGED, never parsed into fields: the family"
            " backfill measured prose extraction at 30-50% precision on this"
            " corpus, and a wrong typed denial outranks the prose it came"
            " from."
        ),
    }


def _open_hypotheses(record: dict[str, Any]) -> list[dict[str, str]]:
    """Concrete untried hypotheses, TYPED first and prose second.

    A typed hypothesis is exact and carries its own kill-cost; a prose one is
    a phrase-match guess. Ranking the guess alongside the exact statement is
    how a briefing's mandatory step 1 ends up being a sentence fragment.
    """
    found: list[dict[str, str]] = []
    seen: set[str] = set()
    typed = _record_field(record, "hypothesis")
    if isinstance(typed, dict) and str(typed.get("statement") or "").strip():
        statement = str(typed["statement"])
        seen.add(statement[:80])
        found.append({
            "marker": "TYPED",
            "field": "hypothesis",
            "text": statement,
            "cheapest_refuting_observation": str(
                typed.get("cheapest_refuting_observation") or ""),
            "screened_against_target": str(
                typed.get("screened_against_target") or ""),
        })
    for field, text in (("attempted_axis", record.get("attempted_axis")),
                        ("value", record.get("value")),
                        *(("attributes." + key, value)
                          for key, value in
                          (record.get("attributes") or {}).items()
                          if isinstance(value, str))):
        if not isinstance(text, str):
            continue
        for sentence in _SENTENCE_SPLIT_RE.split(" ".join(text.split())):
            lowered = sentence.lower()
            marker = next((m for m in _HYPOTHESIS_MARKERS if m in lowered),
                          None)
            if marker is None:
                continue
            key = sentence[:80]
            if key in seen:
                continue
            seen.add(key)
            found.append({
                "marker": marker,
                "field": field,
                "text": sentence[:400] + (" …" if len(sentence) > 400 else ""),
            })
    return found


def _pin_provenance(root: Path, tu: str) -> list[dict[str, Any]]:
    """webfrank.json pins for this TU, each with its SOURCE-EXHAUSTION class.

    The Mandatory-policy provenance rule requires a new rule's function to
    carry a parked/capped attempt record with literal probed_form axes; an
    audit found 11 rules authored with no source-work trail at all. Classing
    each pin here makes that debt visible at spawn instead of at audit.

    The scan covers the WHOLE record history, not the brief's live-attempt
    list: a rule's source-exhaustion evidence characteristically sits in the
    very park the rule then SUPERSEDED, so classing against live records
    alone reports "no trail" for exactly the best-evidenced pins.
    """
    pins = [pin for pin in webfrank_pin_mechanisms(root, None)
            if tu.rstrip("/") in pin["unit"] or pin["unit"] in tu]
    if not pins:
        return []
    wanted = {pin["function"] for pin in pins}
    trails: dict[str, dict[str, Any]] = {
        name: {"any": False, "parked": False, "probed": False, "laws": set()}
        for name in wanted
    }
    directory = root / "memory_graph" / "records"
    if directory.exists():
        for path in sorted(directory.rglob("*.json")):
            try:
                record = json.loads(path.read_text(encoding="utf-8-sig"))
            except (OSError, json.JSONDecodeError):
                continue
            if not isinstance(record, dict) or record.get("kind") != "attempt":
                continue
            name = str(record.get("function", "")).split(":", 1)[-1]
            trail = trails.get(name)
            if trail is None:
                continue
            trail["any"] = True
            applied = _record_field(record, "laws_applied")
            if isinstance(applied, str):
                try:
                    applied = json.loads(applied)
                except json.JSONDecodeError:
                    applied = []
            if isinstance(applied, list):
                trail["laws"].update(
                    item for item in applied if isinstance(item, str))
            if record.get("outcome") in ("parked", "capped"):
                trail["parked"] = True
                if _record_field(record, "probed_form"):
                    trail["probed"] = True
    for pin in pins:
        trail = trails[pin["function"]]
        # The Mandatory-policy bar is a DISJUNCTION: a parked record with
        # literal probed_form axes, OR a law proving the residual class
        # source-unreachable. Reporting only the first half would flag
        # law-backed rules — the best-evidenced kind — as unprovenanced.
        # Cited by the pin's own mechanism prose OR applied by any attempt
        # record on the function: for InitControls the unreachability law is
        # named in the closing record's laws_applied, not in the pin text.
        candidates = set(pin["cites_records"]) | trail["laws"]
        law_backed = sorted(
            cited for cited in candidates
            if cited.startswith("claim.law.")
            and any(mark in cited.lower() for mark in
                    ("source-unreachable", "allocator-not-source",
                     "unreachable-from-source", "is-not-source",
                     "source-unavailable"))
        )
        if law_backed:
            pin["provenance"] = "law-backed-source-unreachable"
            pin["provenance_laws"] = law_backed
        elif trail["probed"]:
            pin["provenance"] = "source-exhausted"
        elif trail["parked"]:
            pin["provenance"] = "parked-without-probed_form"
        elif trail["any"]:
            pin["provenance"] = "attempts-but-no-park"
        else:
            pin["provenance"] = "NO-SOURCE-TRAIL"
        pin["provenance_note"] = (
            "The Mandatory-policy bar is a disjunction."
            " law-backed-source-unreachable = the pin cites a law proving the"
            " residual class source-unreachable (bar met)."
            " source-exhausted = a parked/capped record with a literal"
            " probed_form backs this rule, superseded records included, since"
            " that is usually where the trail lives (bar met)."
            " parked-without-probed_form = a park exists but never wrote the"
            " failing form down, so the rule rests on an unreproducible veto."
            " NO-SOURCE-TRAIL = bar unmet; the function owes a source-first"
            " pass before any further rule work."
        )
    return pins


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
                  AND a.record_id NOT IN ({SUPERSEDED_RECORD_IDS})
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
                residual_obj = _record_field(record, "residual")
                attempts.append(
                    {
                        "id": row["record_id"],
                        "function": row["entity_key"].split(":", 1)[-1],
                        "outcome": row["outcome"],
                        "residual_class": record.get("residual_class"),
                        "residual": residual_obj
                        if isinstance(residual_obj, dict) else None,
                        "held_fixed": _record_field(record, "held_fixed"),
                        "has_probed_form": bool(
                            _record_field(record, "probed_form")),
                        "age_days": _record_age_days(
                            row["valid_from"], row["recorded_at"]),
                        "head": _record_head(record),
                        "recorded_fuzzy": row["recorded_fuzzy"],
                        "_record": record,
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
    report_stamp = None
    report_age_hours = None
    stems = {row["object_name"].rsplit(".", 1)[0] for row in modules}
    if report_path.exists():
        stat = report_path.stat()
        stamped = datetime.fromtimestamp(stat.st_mtime, timezone.utc)
        report_stamp = stamped.strftime("%Y-%m-%dT%H:%M:%SZ")
        report_age_hours = round(
            (datetime.now(timezone.utc) - stamped).total_seconds() / 3600, 1)
        report = json.loads(report_path.read_text(encoding="utf-8"))
        for unit in report.get("units", []):
            if any(unit.get("name", "").endswith(stem) for stem in stems):
                for function in unit.get("functions", []):
                    scores[function["name"]] = float(
                        function.get("fuzzy_match_percent", 0.0))
    # STALENESS BANNER. Every number below came from a file on disk, not from
    # this call: discipline 8's REMEASURE default says a brief's number is
    # stale until a live tool run confirms it, and workers repeatedly quoted
    # brief figures as current. The banner is attached to each number-bearing
    # row, not only to the envelope, because the envelope is what gets
    # skimmed past.
    if report_stamp is None:
        fuzzy_staleness = (
            "NO REPORT: build/GUNE5D/report.json does not exist in this"
            " checkout, so every fuzzy below is null. Run a full ninja."
        )
    else:
        fuzzy_staleness = (
            f"STALE BY CONSTRUCTION: read from build/GUNE5D/report.json"
            f" generated {report_stamp} ({report_age_hours}h ago), NOT"
            " measured now. REMEASURE before quoting: probe.py <unit> <fn>"
            " --fuzzy does build+readout in one call."
        )
    # UNABSORBED words: the CLOSABILITY column. fuzzy and size say how big
    # a residual is; `unabsorbed` says whether the WebFrank register-field
    # stage can reach it at all (tier A = 0 unabsorbed = the stage alone
    # reproduces the target). 32 differing equal-size functions image-wide
    # are tier A while a structural census calls them real work, so a
    # roster ranked without it is ranked blind. Lazy, fail-soft, and never
    # fabricated: a size mismatch or an unavailable backend reads null.
    unabsorbed_rows: dict[str, dict[str, Any]] = {}
    try:
        sys.path.insert(0, str(root / "tools" / "gdl"))
        import unabsorbed as _unabsorbed  # type: ignore

        for stem in stems:
            unabsorbed_rows.update(_unabsorbed.unit_rows(stem, root=root))
    except Exception:
        unabsorbed_rows = {}
    unabsorbed_staleness = (
        "READ FROM build/GUNE5D/{obj,src} OBJECTS ON DISK, not measured"
        " now: as stale as the last ninja for this TU. null means the"
        " metric is UNDEFINED here (unequal function sizes, which is"
        " itself outside every postprocessor class, or no built object) —"
        " it never means zero."
    )
    roster = [
        {
            "function": row["raw_name"],
            "size": row["size"],
            "fuzzy": scores.get(row["raw_name"]),
            "fuzzy_staleness": fuzzy_staleness,
            "unabsorbed": (unabsorbed_rows.get(row["raw_name"]) or {}).get(
                "unabsorbed"),
            "unabsorbed_tier": (
                unabsorbed_rows.get(row["raw_name"]) or {}).get("tier"),
            # THE COUNT IS NOT THE CLASS (run 34 item 5): an unchanged
            # unabsorbed count hid a residual moving from a class no
            # postprocessor can reach to one a permutation window can, so
            # the roster reported "no change" on a real change. Classes:
            # allocator / schedule / operand / source-structural /
            # count-asymmetric — only the first two are reachable at all —
            # plus compiler-exact and rule-served, which are already CLOSED
            # and are not work items. Those two used to be folded into
            # `allocator` because the census scored the POSTPROCESSED
            # object, which oversized one work order 24:1 (run-37 item 3,
            # claim.law.MC_the-unabsorbed-census-scores-the-postprocessed-
            # object...); it now reads the raw .postprocess/body.
            "unabsorbed_class": (
                unabsorbed_rows.get(row["raw_name"]) or {}).get(
                    "residual_class"),
            "unabsorbed_staleness": unabsorbed_staleness,
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
    for attempt in attempts:
        if attempt.get("recorded_fuzzy") is not None:
            attempt["recorded_fuzzy_staleness"] = (
                "the value the record BANKED at park time, not a current"
                " measurement — remeasure before comparing"
            )

    # 10b FIRST: open hypotheses outrank fresh analysis for the next lane.
    open_hypotheses: list[dict[str, Any]] = []
    for attempt in attempts:
        for hypothesis in _open_hypotheses(attempt["_record"]):
            open_hypotheses.append({
                "function": attempt["function"],
                "record": attempt["id"],
                "outcome": attempt["outcome"],
                "age_days": attempt["age_days"],
                **hypothesis,
            })
    # TYPED hypotheses first (exact, and each carrying its own kill-cost),
    # then parked/capped prose ones — those are the records whose author
    # stopped — then everything else by age.
    open_hypotheses.sort(
        key=lambda row: (0 if row["marker"] == "TYPED" else 1,
                         0 if row["outcome"] in ("parked", "capped") else 1,
                         row["age_days"] or 0))

    vetoed_axes = []
    for attempt in attempts:
        if attempt["outcome"] not in ("parked", "capped", "negative"):
            continue
        row = {
            "function": attempt["function"],
            "record": attempt["id"],
            "outcome": attempt["outcome"],
            "residual_class": attempt["residual_class"],
            "residual": attempt["residual"],
            "held_fixed": attempt["held_fixed"],
            "has_probed_form": attempt["has_probed_form"],
            "axis": attempt["head"],
            "age_days": attempt["age_days"],
        }
        typed_denial = _record_field(attempt["_record"], "denial")
        if typed_denial:
            row["denial"] = typed_denial
        quarantine = _ungated_prose_denial(attempt["_record"])
        if quarantine:
            row["quarantine"] = quarantine
        vetoed_axes.append(row)

    refutations: list[dict[str, Any]] = []
    for attempt in attempts:
        refuted = attempt["_record"].get("refutes")
        if refuted:
            refutations.append({
                "function": attempt["function"],
                "record": attempt["id"],
                "refutes": refuted,
                "head": attempt["head"],
            })

    scaffold_rows = [
        {"function": attempt["function"], "record": attempt["id"],
         "head": attempt["head"]}
        for attempt in attempts
        if "scaffold" in json.dumps(attempt["_record"]).lower()
    ]

    pins = _pin_provenance(root, tu)

    # RUN-33 (RG): seed the transferability cohort from what this TU's OWN
    # records already measured — the dominant verified residual family and the
    # newest recorded signature — then ask for closes from ELSEWHERE.
    tu_module = modules[0]["object_name"] if modules else None
    seed_family: str | None = None
    seed_signature: str | None = None
    family_counts: dict[str, int] = {}
    for attempt in attempts:
        obj = attempt["_record"].get("residual")
        if not isinstance(obj, dict):
            continue
        name = obj.get("family")
        if name and name not in ("unclassified", "no-residual"):
            family_counts[name] = family_counts.get(name, 0) + 1
        if not seed_signature and obj.get("signature"):
            seed_signature = obj["signature"]
    if family_counts:
        seed_family = max(sorted(family_counts), key=family_counts.get)
    similar = similar_residuals(
        root=root, db_path=db_path, tu=None, family=seed_family,
        signature=seed_signature, limit=8, exclude_tu=tu_module)
    similar["seeded_from"] = {
        "tu": tu_module, "family": seed_family,
        "signature_present": bool(seed_signature),
        "how": "dominant verified residual family and newest recorded"
               " signature among THIS TU's own attempt records; rows from"
               " this TU are excluded because the briefing already lists"
               " them",
    }

    for attempt in attempts:
        attempt.pop("_record", None)

    core_laws = law_corpus(root=root, db_path=db_path, tag="core-screen",
                           limit=50)["laws"]

    def _law_head(row: dict[str, Any]) -> dict[str, Any]:
        """The brief's law row: identity, tags, and the evidence columns.

        A brief that lists laws without their evidence is what let a refuted
        law sit in a spawn briefing looking exactly like a proven one.
        """
        head = {"id": row["id"], "tags": row["tags"],
                "status": row["status"], "score": row["score"], "n": row["n"],
                "successes": row["evidence"]["successes"],
                "failures": row["evidence"]["failures"]}
        if row.get("needs_revalidation"):
            head["needs_revalidation"] = row["needs_revalidation"]
        return head
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
                matching_laws.append(
                    _law_head(row) | {"age_days": row["age_days"]})
    # Ranked by evidence, not by age: the whole point of the run-32 layer is
    # that a lane reading a briefing top-down reads the best-evidenced lever
    # first. Age stays on the row as a tiebreaker the reader can see.
    matching_laws.sort(key=lambda row: law_score_sort_key(
        {"status": row["status"], "wilson": row["score"], "n": row["n"],
         "id": row["id"]}))
    mentioned = law_corpus(tu, root=root, db_path=db_path, limit=20)["laws"]
    mentioned_ids = {row["id"] for row in core_laws}
    try:
        debt_rows = fakematch_debt(tu, root=root, db_path=db_path,
                                   limit=10)["tus"]
    except MemoryGraphError:
        debt_rows = []
    return {
        "tu": [row["object_name"] for row in modules],
        # 10b comes FIRST, before the roster: a recorded untried hypothesis
        # outranks fresh analysis, and one skipped by its own author was
        # worth -235 real when a later run finally executed it.
        "open_hypotheses": open_hypotheses,
        "open_hypotheses_note": (
            "AGENTS.md discipline 10b: a record ending in a concrete untried"
            " hypothesis makes that hypothesis MANDATORY STEP 1 for the next"
            " lane on that function, ranked ABOVE fresh analysis. These are"
            " extracted by phrase match — read the cited record before acting,"
            " and remeasure the record's NEGATIVE findings too, not only its"
            " cure."
        ),
        "vetoed_axes": vetoed_axes,
        "vetoed_axes_note": (
            "parked/capped/negative records: each is a VETO on ITS axis."
            " has_probed_form=false means the failing form was never written"
            " down, so the veto cannot be reproduced — treat it as a weak"
            " veto and say so if you re-probe. held_fixed names what the park"
            " held CONSTANT; a null held_fixed on a multi-edit park is why"
            " two correct-alone parks once jointly hid a 7-function TU flip."
            " A `denial` block is the TYPED form — scoped, with an"
            " expiry_check you can run and a falsifier. A `quarantine` block"
            " (UNGATED-PROSE-DENIAL) means the opposite: the record denies"
            " work in prose only, predating the typed-denial gate, so it"
            " carries no scope and no way to expire. Prose denials are"
            " FLAGGED here, never auto-extracted into fields — phrase"
            " extraction measured 30-50% precision on this corpus, and a"
            " fabricated typed denial would be trusted more than the prose"
            " it replaced."
        ),
        "refutations": refutations,
        "scaffold_rows": scaffold_rows,
        # RUN-33 (RG): TU-scoped transferability. `brief` already lists this
        # TU's OWN history; these are closes from ELSEWHERE that share its
        # residual shape or family, so a spawn briefing carries at least one
        # worked example of the class before the first probe.
        "similar_residuals": similar,
        "webfrank_pins": pins,
        "webfrank_pins_note": (
            "a pinned function's SOURCE IS FROZEN (AGENTS.md trap 4): the"
            " postprocessor hash-asserts its body and the build aborts on"
            " drift. Screen this list before editing anything in the TU."
            " `provenance` classes each pin against the Mandatory-policy"
            " source-exhaustion bar."
        ),
        "staleness_banner": (
            "EVERY NUMBER IN THIS BRIEF IS READ FROM DISK, NOT MEASURED NOW."
            " Fuzzy comes from build/GUNE5D/report.json"
            + (f" (generated {report_stamp}, {report_age_hours}h old)"
               if report_stamp else " (ABSENT — all fuzzy is null)")
            + "; recorded_fuzzy on an attempt is what that record banked at"
            " park time. Discipline 8: REMEASURE is the default — a number"
            " quoted from a brief is stale until a live tool run confirms it."
            " Do not write a record or a commit message from these figures."
        ),
        "report_generated_at": report_stamp,
        "report_age_hours": report_age_hours,
        "functions": roster,
        "scores_note": (None if scores else
                        "fuzzy is null because build/GUNE5D/report.json does"
                        " not exist yet in this checkout — run a full ninja"
                        " first, then re-run brief for scores"),
        "live_attempts": attempts,
        "active_claims": claims,
        "core_screen_laws": [_law_head(row) for row in core_laws],
        "law_evidence_note": (
            "status/score/n are DERIVED at build from the corpus's own"
            " citations: status is the tier (established > contested >"
            " provisional > refuted), score is the Wilson lower bound"
            " (z=1.96) over successes+failures, and n is that denominator —"
            " NOT the citation count, because parks and caps a law correctly"
            " predicted are not failures and never enter it. PROVISIONAL laws"
            " (zero verified successes) are hidden from these lists; ask for"
            " them with `laws --include-provisional 1` when you want the"
            " unverified pool. A `needs_revalidation` banner means every"
            " measurement behind the law predates a recorded regime change"
            " for its scope."
        ),
        "matching_laws": matching_laws,
        "tu_mentioned_laws": [
            {"id": row["id"], "scope": row["scope"]}
            for row in mentioned if row["id"] not in mentioned_ids
        ],
        "raw_offset_debt": debt_rows,
        "note": (
            "READ open_hypotheses FIRST (discipline 10b), then vetoed_axes,"
            " then webfrank_pins — only then the roster. Briefing heads only:"
            " fetch full law/attempt text in ONE call with gdlmem.py record"
            " <id1>,<id2>,... or laws --tag X --full; parked/capped attempts"
            " are VETOes on their axes; run tools/gdl/defake_gate.py baseline"
            " before the first edit and honor active_claims from other"
            " owners. Every number here is stale — see staleness_banner."
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
                full=kw["full"], limit=kw["limit"],
                residual=kw["residual"],
                include_provisional=kw["include_provisional"],
                rank=kw["rank"]),
            params=(
                SurfaceParam("query", str, default=None,
                             help="filter over id SLUG WORDS, scope, law text,"
                                  " and webfrank pin mechanism prose"),
                SurfaceParam("residual", str, default=None,
                             help="`--ops` token delta, e.g. \"+1 addi -1 li\";"
                                  " returns laws plus sibling records sharing"
                                  " the residual signature"),
                SurfaceParam("tag", str, default=None,
                             help="filter by structured applicability tag"),
                SurfaceParam("full", int, default=0, maximum=1,
                             help="1 = inline complete law text"),
                # Default must exceed the corpus: at 100, 36 of 136 laws
                # were silently invisible to enumeration (audit, 2026-08-31).
                SurfaceParam("limit", int, default=400, maximum=500),
                SurfaceParam("include_provisional", int, default=0, maximum=1,
                             help="1 = also return PROVISIONAL laws (zero"
                                  " verified successes). Hidden by default so"
                                  " an unverified law cannot read as"
                                  " authoritative"),
                SurfaceParam("rank", int, default=1, maximum=1,
                             help="0 = restore the legacy date ordering"
                                  " instead of ranking by evidence"),
            ),
        ),
        SurfaceOp(
            name="events", mcp_name="memory_regime_events",
            doc=("List recorded regime-change events; claims whose evidence "
                 "predates a matching event carry a needs-revalidation "
                 "banner."),
            call=lambda root, db, **kw: {
                "events": regime_events(root=root, db_path=db),
                "note": (
                    "add one with `gdlmem.py event-add <slug> --scope"
                    " <path-or-tag>`. Revalidation is EVENT-based, never"
                    " calendar decay: a law goes stale when the regime that"
                    " produced its evidence changes, not when time passes."
                    " scope matches a law's attributes.scope substring or one"
                    " of its tags; '*' covers the whole corpus."),
            },
        ),
        SurfaceOp(
            name="find", mcp_name="memory_find_records",
            doc=("Faceted record search: by kind, function, TU, attempt "
                 "outcome, associated law, plus optional FTS terms."),
            call=lambda root, db, **kw: find_records(
                kw["query"], root=root, db_path=db, kind=kw["kind"],
                function=kw["function"], tu=kw["tu"], outcome=kw["outcome"],
                residual=kw["residual"], law=kw["law"], limit=kw["limit"],
                family=kw["family"], capability=kw["capability"],
                include_candidates=kw["include_candidates"]),
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
                SurfaceParam("family", str, default=None,
                             help="residual.family tag, e.g. live-zero-remat;"
                                  " rows are labelled family (verified) vs"
                                  " residual_class-fallback (coarse legacy"
                                  " widening) — never total the tiers"),
                SurfaceParam("include_candidates", int, default=0, maximum=1,
                             help="1 = also return quarantined"
                                  " family_candidate extractor guesses"
                                  " (~30-50%% precision), labelled as such"),
                SurfaceParam("capability", str, default=None,
                             help="residual.capability_needed — which parks"
                                  " are waiting on a capability"),
                SurfaceParam("limit", int, default=25, maximum=2000),
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
                include_released=kw["include_released"], owns=kw["owns"]),
            params=(
                SurfaceParam("stale_after", int, default=2, maximum=30,
                             help="days before an active claim is stale"),
                SurfaceParam("include_released", int, default=0, maximum=1,
                             help="1 to include released/done claims"),
                SurfaceParam("owns", str, default=None,
                             help="path or TU fragment: which claim owns it?"
                                  " An active foreign claim is a VETO on its"
                                  " whole scope — run this before a first"
                                  " edit"),
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
                document_limit=kw["document_limit"],
                residual=kw["residual"],
                similar_limit=kw["similar_limit"]),
            params=(
                SurfaceParam("symbol", str, required=True),
                SurfaceParam("document_limit", int, default=12, maximum=50),
                SurfaceParam("residual", str, default=None,
                             help="your verbatim `fndiff --ops` delta; ranks"
                                  " the similar_residuals section on the"
                                  " signature cohort. Pure-reorder deltas"
                                  " (multiset IDENTICAL) are supported since"
                                  " run 33"),
                SurfaceParam("similar_limit", int, default=8, maximum=50,
                             help="cap on the similar_residuals section"),
            ),
        ),
        SurfaceOp(
            name="similar", mcp_name="memory_similar_residuals",
            doc=("Similar residuals elsewhere: closes on other functions "
                 "sharing your residual signature, family, or TU, ranked by "
                 "the evidence layer."),
            call=lambda root, db, **kw: similar_residuals(
                root=root, db_path=db, function=kw["function"], tu=kw["tu"],
                signature=kw["residual"], family=kw["family"],
                limit=kw["limit"], landed_only=kw["landed_only"]),
            params=(
                SurfaceParam("function", str, default=None,
                             help="the function you are working; its own"
                                  " records are excluded as self-reference"),
                SurfaceParam("residual", str, default=None,
                             help="verbatim `fndiff --ops` delta"),
                SurfaceParam("tu", str, default=None,
                             help="TU object name (derived from --function"
                                  " when omitted)"),
                SurfaceParam("family", str, default=None,
                             help="residual family to match"),
                SurfaceParam("limit", int, default=8, maximum=50),
                SurfaceParam("landed_only", int, default=0, maximum=1,
                             help="1 = only exact/improved closes"),
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
            name="recount", mcp_name="memory_recount_derived",
            doc=("Recount every derived table straight from the record JSON "
                 "with independently written readers; prints shipped vs "
                 "independent vs delta per table."),
            call=lambda root, db, **kw: recount_derived_tables(root, db),
        ),
        SurfaceOp(
            name="validate", mcp_name="memory_validate",
            doc=("Validate durable and inbox records, including reference "
                 "resolution (incremental: unchanged records are served from "
                 "a content-hash cache)."),
            call=lambda root, db, **kw: validate_records(
                root, db_path=db, refresh=kw["refresh"]),
            params=(
                SurfaceParam("refresh", int, default=0, maximum=1,
                             help="1 = ignore the cache and revalidate every"
                                  " record from scratch"),
            ),
        ),
    )


_PARK_CITATION_KEYS = frozenset({
    "verification", "law_screen", "laws_applied", "anchors", "scope",
    "history_screen", "measurement_resolution", "provenance",
})


# Repo-relative paths under a TRACKED root only. `build/` and `orig/` are
# gitignored and legitimately absent in a fresh worktree, so their absence
# says nothing about a record; these five roots are committed, and a path
# under them that is gone really is gone.
_ANCHOR_PATH_RE = re.compile(
    r"(?:src|include|tools|config|memory_graph)"
    r"/[A-Za-z0-9_][A-Za-z0-9_./+-]*\.[A-Za-z0-9_]{1,6}"
)

# Extensions this check must NEVER read as a stranded anchor. Records
# routinely name an OBJECT by its source-tree path (`src/game/boss/boss.o`,
# `src/game/world/.postprocess/body/btricol.o`) — those live under `build/`
# and are absent from `src/` by construction, so treating them as missing
# files reports the build layout as record rot. Measured: a first cut of this
# check flagged 111 records, and 108 of them were exactly this shape.
_GENERATED_SUFFIXES = frozenset({
    "o", "s", "a", "d", "elf", "dol", "bin", "map", "obj", "lib",
})
# The one rename that has actually stranded records here (movieplayer.c ->
# movieplayer.cpp, 2026-08-31). Offered only for source spellings; a `.o`
# has no meaningful `.c` sibling.
_RENAME_SIBLINGS = {"c": "cpp", "cpp": "c"}

# The keys whose JOB is to point at where a record's truth lives. Prose
# fields that merely NARRATE the work (law_screen, attempted_axis,
# probed_form, axis_log, value, falsifier, denial, hypothesis, scope) are
# deliberately excluded: a path mentioned there is a story, not an anchor,
# and claim.RC_stale-reopen-queue-is-a-classifier-artifact.20260901.v1
# measured what scanning narrative prose does to a reopen queue (43/43 false
# positives). `verification`/`provenance`/`reproduction` ARE included despite
# being citation keys, because the question here is not "does this prose use
# the vocabulary" but "does this literal file still exist" — a reproduction
# command whose file is gone cannot be run, which is precisely a reopen
# signal rather than a classifier artifact.
_ANCHOR_ROLE_KEYS = (
    "anchor", "anchors", "evidence", "implementation_anchors",
    "reference_anchors", "provenance", "reproduction", "verification",
    "source_path", "entrypoint",
)


def _anchor_path_strings(record: Any) -> list[str]:
    """Repo-relative paths a record offers as its truth anchors."""
    if not isinstance(record, dict):
        return []
    found: list[str] = []
    scopes: list[Any] = [record.get("locator")]
    for container in (record, record.get("attributes")):
        if not isinstance(container, dict):
            continue
        for key in _ANCHOR_ROLE_KEYS:
            if key in container:
                scopes.append(container[key])
    for value in scopes:
        if value is None:
            continue
        text = value if isinstance(value, str) else json.dumps(value)
        found.extend(_ANCHOR_PATH_RE.findall(text))
    # Preserve first-seen order; a record repeating one anchor is one anchor.
    return list(dict.fromkeys(found))


_ANCHOR_INDEX_ROOTS = ("src", "include", "tools", "config", "memory_graph")


def anchor_basename_index(root: Path) -> dict[str, list[str]]:
    """{basename: [repo-relative paths]} across the tracked anchor roots.

    Built ONCE per `stale` run and passed in, because the alternative — an
    rglob per record — is the quadratic shape run 33 already had to fix once
    in `validate`.
    """
    index: dict[str, list[str]] = {}
    for top in _ANCHOR_INDEX_ROOTS:
        base = root / top
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if path.is_file():
                index.setdefault(path.name, []).append(
                    path.relative_to(root).as_posix())
    return index


def missing_anchor_paths(record: Any, root: Path,
                         basename_index: dict[str, list[str]] | None = None
                         ) -> list[dict[str, Any]]:
    """Anchor paths this record names that no longer exist in the tree.

    Run-34 criticism (MV): an ACCEPTED PlayVQMovie record was anchored to a
    deleted `movieplayer.c` and described a layout the tree no longer
    produces — an authoritative record pointing at a file that is not there.
    Nothing in `stale` could see it, because every heuristic there compares
    SCORES, and a record whose anchor evaporated has no score to move.

    The `.c`/`.cpp` sibling is reported when it exists, because that rename
    is the measured cause (movieplayer.c -> movieplayer.cpp, 2026-08-31) and
    it turns "this record is stranded" into a one-line repair. With a
    ``basename_index`` supplied, a file that MOVED DIRECTORY is reported the
    same way — the live corpus carries both shapes (`src/game/leveldata.h`
    now lives at `include/game/leveldata.h`, `src/game/sfx/sndfx.c` at
    `src/game/audio/sndfx.c`).
    """
    out: list[dict[str, Any]] = []
    for path in _anchor_path_strings(record):
        stem, _dot, suffix = path.rpartition(".")
        if suffix.lower() in _GENERATED_SUFFIXES:
            continue
        if (root / path).exists():
            continue
        entry: dict[str, Any] = {"path": path}
        sibling = _RENAME_SIBLINGS.get(suffix.lower())
        if sibling and stem and (root / f"{stem}.{sibling}").exists():
            entry["renamed_to"] = f"{stem}.{sibling}"
        elif basename_index:
            moved = basename_index.get(path.rsplit("/", 1)[-1]) or []
            # One unambiguous candidate is a repair; several is a guess, and
            # a guess in a reopen queue is how the last record-mining
            # heuristic here produced 43 false positives.
            if len(moved) == 1:
                entry["moved_to"] = moved[0]
            elif moved:
                entry["candidates"] = sorted(moved)[:5]
        out.append(entry)
    return out


def _mentions_unnegated(text: str, term: str) -> bool:
    """True if `term` occurs in `text` outside a negating phrase.

    Parks routinely rule a class OUT in prose ("none applicable: no
    raw-offset conversion here", "a non-fakematch source edit"). A plain
    substring test reads those as evidence FOR the class and flags the park
    for exactly the reason it said did not apply.
    """
    negators = ("no ", "non-", "non ", "not a ", "not ", "never ", "without ")
    start = 0
    while True:
        index = text.find(term, start)
        if index < 0:
            return False
        prefix = text[max(0, index - 12):index]
        if not any(prefix.endswith(negator) for negator in negators):
            return True
        start = index + len(term)


def _park_substance_text(attempted_axis: Any, raw_json: Any) -> str:
    """Prose describing what a park is ABOUT, for the conversion heuristic.

    Excludes attributes that record HOW the park was verified or WHICH laws
    were screened. Scanning those made every properly gated park self-flag as
    a field-conversion cap: `verification` quotes the mandatory
    `defake_gate.py` command (substring "defake") and `law_screen` cites law
    ids containing "cast"/"raw-offset", often in explicitly NEGATED prose
    ("none applicable: no raw-offset conversion here"). On 2026-09-01 that
    made 43 of 43 reopen_candidates false positives.
    """
    parts: list[str] = [str(attempted_axis or "")]
    try:
        record = json.loads(raw_json or "{}")
    except (TypeError, ValueError):
        record = {}
    attributes = record.get("attributes") if isinstance(record, dict) else None
    if isinstance(attributes, dict):
        for key, value in attributes.items():
            if key in _PARK_CITATION_KEYS:
                continue
            parts.append(value if isinstance(value, str) else json.dumps(value))
    return " ".join(parts).lower()


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
            f" AND a.record_id NOT IN ({SUPERSEDED_RECORD_IDS})"
        ).fetchall()
    with closing(open_database(root, db_path)) as connection:
        multi_rows = connection.execute(
            "SELECT e.name, COUNT(*) AS n, GROUP_CONCAT(a.record_id) AS ids"
            " FROM attempt a JOIN entity e ON e.id = a.function_entity_id"
            f" WHERE a.record_id NOT IN ({SUPERSEDED_RECORD_IDS})"
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
    # The failing_form_undocumented reopen reason (and its form_terms /
    # conversion_terms heuristics) is RETIRED (2026-09-01): the five-word
    # whitelist flagged parks that document their forms exhaustively in
    # domain vocabulary — both live field hits were false positives with
    # explicit do-not-retry conclusions, and two workers independently
    # recommended retirement over extension. The durable fix is the
    # attributes.probed_form field on attempt records (literal edited
    # source, not paraphrase). score_moved_since_park remains the sole
    # heuristic reopen signal.
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
        if score < 70.0:
            suspect.append(
                {"function": name, "record": row["record_id"], "fuzzy": score}
            )
        else:
            valid += 1
    # STRANDED ANCHORS (run 34 item 6). Every heuristic above compares
    # SCORES, and a record whose anchor file was deleted or renamed has no
    # score to move — MV found an ACCEPTED PlayVQMovie record anchored to a
    # `movieplayer.c` that no longer exists, describing a layout the tree no
    # longer produces. Scanned over EVERY accepted record, not just the
    # parked/capped rows above: the defect is orthogonal to outcome.
    basename_index = anchor_basename_index(root)
    with closing(open_database(root, db_path)) as connection:
        anchor_rows = connection.execute(
            "SELECT record_id, raw_json FROM record_ingest"
            " WHERE record_state = 'accepted'"
        ).fetchall()
    for row in anchor_rows:
        try:
            record = json.loads(row["raw_json"] or "{}")
        except (TypeError, ValueError):
            continue
        gone = missing_anchor_paths(record, root, basename_index)
        if not gone:
            continue
        entry = {
            "record": row["record_id"],
            "reason": "anchor_path_missing",
            "missing_anchors": gone,
        }
        subject = record.get("function") or record.get("subject")
        if isinstance(subject, str) and subject:
            entry["function"] = subject.split(":", 1)[-1]
        reopen.append(entry)
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
            " offsetof-on-raw-pointer form before trusting it;"
            " anchor_path_missing means the record cites a source path that"
            " is NOT in the tree, so its evidence cannot be reopened as"
            " written — this one is scanned over EVERY accepted record, not"
            " just parks, because it is orthogonal to outcome and to score."
            " `renamed_to`/`moved_to` name the surviving file when exactly"
            " one candidate exists (the repair is superseding the record"
            " with the corrected anchor, NOT deleting it); `candidates`"
            " means several files share the basename and a human must"
            " choose. Object paths under a source tree"
            " (src/.../foo.o, .postprocess/body/*.o) are EXCLUDED: they live"
            " under build/ by construction, and counting them reported the"
            " build layout as record rot (108 of a first cut's 111 hits)."
            " active_work_claims presumed_abandoned entries are older than"
            " one day: verify via git log against the claimed scope, then"
            " remove the claim in a standalone cleanup commit (AGENTS.md"
            " cross-fleet concurrency)."
        ),
    }


def _validate_cache_path(root: Path) -> Path:
    return default_database_path(root).parent / "validate_cache.json"


def validate_records(
    root: Path = REPO_ROOT, db_path: Path | None = None, refresh: int = 0
) -> dict[str, Any]:
    """Validate every durable and inbox record, incrementally.

    RUN 33 (RG): this call previously did not complete. AGENTS.md discipline 13
    told every lane to skip it ("`gdlmem validate` does not complete at current
    corpus size — never block on it"), which left the only whole-corpus schema
    check unusable and made `build` the de-facto integrity gate.

    Two defects, both measured: (1) `_probe_record_references` called
    `ensure_database` PER RECORD, and `ensure_database` recomputes
    `source_fingerprint` by stat-ing every record file, both symbol tables, the
    PDB dump and every tools/gdl source — about 1,600 stats each time, so 1,568
    records cost roughly 2.5 million file stats and 1,568 connection opens;
    (2) nothing was cached between runs, so an unchanged corpus paid the full
    price every time. The fix hoists the database out of the loop and memoises
    per-record results by CONTENT HASH.

    The cache is sound because the two checks have different dependencies and
    are keyed accordingly: schema validation is a pure function of the record
    bytes, so its key is the content hash alone; reference resolution depends
    on the whole graph, so its key additionally carries the database's
    `source_fingerprint`, which changes whenever any record does. A corpus that
    changed at all therefore revalidates its references from scratch. Pass
    ``refresh=1`` to ignore the cache entirely.
    """
    started = time.monotonic()
    paths: list[Path] = []
    for relative in (Path("memory_graph/records"), Path("memory_graph/inbox")):
        directory = root / relative
        if directory.exists():
            paths.extend(directory.rglob("*.json"))
    paths.sort()

    cache_path = _validate_cache_path(root)
    cache: dict[str, Any] = {"schema": {}, "references": {}}
    if not refresh and cache_path.exists():
        try:
            loaded = json.loads(cache_path.read_text(encoding="utf-8"))
            if (isinstance(loaded, dict)
                    and loaded.get("schema_version") == SCHEMA_VERSION):
                cache["schema"] = loaded.get("schema") or {}
                cache["references"] = loaded.get("references") or {}
        except (OSError, json.JSONDecodeError):
            cache = {"schema": {}, "references": {}}

    ids: set[str] = set()
    records: list[tuple[Path, dict[str, Any], str]] = []
    schema_cached = 0
    for path in paths:
        raw = path.read_bytes()
        digest = hashlib.sha256(raw).hexdigest()
        record = json.loads(raw.decode("utf-8-sig"))
        if not isinstance(record, dict):
            raise MemoryGraphError(
                f"{path}: top-level JSON value must be an object")
        if digest in cache["schema"]:
            schema_cached += 1
        else:
            _validate_record(record, path)
            cache["schema"][digest] = True
        # The duplicate-id check is a property of the SET, never of one
        # record, so it is always run and never cached.
        if record["id"] in ids:
            raise MemoryGraphError(
                f"duplicate record id {record['id']!r} at {path}")
        ids.add(record["id"])
        records.append((path, record, digest))

    # Mirror the build's reference resolution so `validate` never blesses a
    # record the build would reject. Uses the existing database when present;
    # skipped (reported) when no graph has been built yet.
    reference_errors: list[dict[str, str]] = []
    dangling_citations: list[dict[str, str]] = []
    references_checked = False
    references_cached = 0
    database = (db_path or default_database_path(root)).resolve()
    if database.exists():
        references_checked = True
        with closing(open_database(root, database)) as connection:
            fingerprint = dict(connection.execute(
                "SELECT key, value FROM meta").fetchall()
            ).get("source_fingerprint", "")
            for path, record, digest in records:
                key = f"{digest}:{fingerprint}"
                cached = cache["references"].get(key)
                if cached is not None:
                    references_cached += 1
                    for cited_id in cached:
                        dangling_citations.append(
                            {"path": _repo_relative(root, path),
                             "cited": cited_id})
                    continue
                try:
                    stranded = _probe_record_references(
                        record, root, database, connection=connection,
                        strict_citations=False)
                except MemoryGraphError as error:
                    reference_errors.append(
                        {"path": _repo_relative(root, path),
                         "error": str(error)})
                else:
                    cache["references"][key] = stranded
                    for cited_id in stranded:
                        dangling_citations.append(
                            {"path": _repo_relative(root, path),
                             "cited": cited_id})
    # The cache is written unconditionally. Skipping the write when ANY record
    # failed would have thrown away every good result alongside it, which is
    # how an incremental check silently stays non-incremental.
    try:
        cache_path.parent.mkdir(parents=True, exist_ok=True)
        cache_path.write_text(json.dumps({
            "schema_version": SCHEMA_VERSION,
            "schema": cache["schema"],
            "references": cache["references"],
        }, sort_keys=True), encoding="utf-8")
    except OSError:
        # A read-only or racing checkout must not fail the validation
        # itself; the next run simply pays full price.
        pass
    stranded_ids = sorted({row["cited"] for row in dangling_citations})
    result = {
        "valid": not reference_errors,
        "record_count": len(paths),
        "schema_version": SCHEMA_VERSION,
        "references_checked": references_checked,
        "schema_checks_cached": schema_cached,
        "reference_checks_cached": references_cached,
        "dangling_citation_count": len(dangling_citations),
        "dangling_citation_ids": stranded_ids,
        "dangling_citations": dangling_citations[:60],
        "dangling_note": (
            "CORPUS DEBT, not a validation failure: an accepted record cites a"
            " record id that no longer exists. Most are the documented"
            " `prune-attempts` workflow's own output — it DELETES records"
            " ejected past the per-function cap, stranding every `supersedes`"
            " that pointed at one — so failing the corpus on them would make"
            " the gate refuse the records that document it. A NEW proposal is"
            " still refused for a dangling citation (strict at staging,"
            " tolerant over history)."
        ),
        "elapsed_seconds": round(time.monotonic() - started, 3),
        "cache": _repo_relative(root, cache_path),
        "cache_note": (
            "schema results are keyed by record CONTENT HASH (a pure function"
            " of the bytes); reference results additionally carry the"
            " database's source_fingerprint, so any corpus change revalidates"
            " every reference. --refresh 1 ignores the cache."
        ),
    }
    if reference_errors:
        result["reference_errors"] = reference_errors
    return result
