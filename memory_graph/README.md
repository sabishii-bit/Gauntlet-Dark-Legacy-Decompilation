# GDL Memory Graph

This folder is the project's single knowledge home: the durable record of
what has been proven, attempted, parked, and policy-bound across the matching
campaign, structured so any agent or contributor can query it deterministically
instead of re-deriving (or re-breaking) the same ground.

## Design overview

The system is three layers with strictly one-way data flow:

```text
  sources (tracked, reviewable)          builder (pure function)      view (disposable)
 ┌──────────────────────────────┐      ┌──────────────────────┐     ┌──────────────────┐
 │ memory_graph/records/*.json  │      │                      │     │ .git/gdl-memory/ │
 │ memory_graph/inbox/*.json    │ ───▶ │  core.py build       │ ──▶ │ memory.sqlite    │
 │ config/GUNE5D/symbols.txt    │      │  (validate, resolve, │     │ (FTS + relational│
 │ config/GUNE5D/splits.txt     │      │   index, verify,     │     │  indexes; never  │
 │ research/xbox_symbols/*      │      │   atomic swap)       │     │  committed)      │
 │ tools/gdl/*.py (tool scan)   │      │                      │     │                  │
 └──────────────────────────────┘      └──────────────────────┘     └──────────────────┘
```

- **Sources are the only truth.** Durable facts live as one-JSON-object-per-file
  records, versioned in git, reviewed like code. Symbol tables and the Xbox
  reference index are deterministic inputs, not memory.
- **The builder is a pure function of the sources.** Same checkout, same
  database — byte-for-byte reproducible, which is what makes the view safe to
  throw away and safe to race.
- **The SQLite file is a materialized view.** It exists for fast queries
  (joins + FTS5 full-text). Deleting it loses nothing; the next query rebuilds
  it in a few seconds.

## Layout

- `records/`: accepted structured records. Subfolders (`entities/`, `claims/`,
  `evidence/`, `attempts/`, `tools/`) are organizational only; the builder
  reads the whole tree recursively.
- `inbox/`: proposed records awaiting integrator review.
- `schema/record.schema.json`: the portable record contract (JSON Schema).
- `core.py` + `schema.sql`: the stdlib-only builder library and SQL schema.
- `gdlmem.py`: the CLI over the library.
- `mcp/`: optional MCP adapter exposing the same functions as host tools; its
  `uv`-locked environment keeps the `mcp` dependency out of the build.

## Build pipeline

`build` assembles the database in a fixed order inside one transaction:

1. **GameCube symbols/modules** from `config/GUNE5D/symbols.txt` and
   `splits.txt` — imported first so record references can resolve against
   them.
2. **Records** from `records/` then `inbox/` (see the record model below).
3. **Discovered tools**: a docstring scan of `tools/gdl/*.py` catalogs every
   script as `source_kind: source_scan` — an inventory, never invented policy.
   Reviewed `tool` records override with `source_kind: reviewed_record`.
4. **Optional legacy documents** from `memory_graph/legacy/` if that directory
   exists (it normally does not; see history note).
5. **Xbox symbols** from `research/xbox_symbols/functions_by_module.txt` and
   **PDB types/fields** from `xbox_structs.tsv`.
6. **Exact-name candidate links**: every GameCube/Xbox raw-name collision
   becomes a `cross_platform_symbol_link` at `verification: candidate`,
   `confidence: 0.5`, `method: exact_name`. Nothing is auto-verified.
7. **Integrity gates**: `PRAGMA foreign_key_check` and `quick_check` must be
   clean, then the temp file is atomically `os.replace`d over the target.
   Readers can never observe a half-built database.

## Freshness and location

The database lives at `<git common dir>/gdl-memory/memory.sqlite` (resolved
via `git rev-parse --git-common-dir`), so all worktrees of one repository
share a single view and git can never track it. Outside a git checkout it
falls back to `.gdl-memory/` under the root.

Every read command calls `ensure` first, which rebuilds when the database is
missing, corrupt, schema-outdated, or when the **source fingerprint** — a
SHA-256 over the path, size, and mtime of every input file — no longer
matches. Editing a record, symbol table, or the Xbox index therefore refreshes
the view automatically on the next query. A database built from a *different*
checkout root is left in place (worktrees share honestly rather than fighting
over whose sources win); force `build` if you need to repoint it.

## Record model

Records share an envelope (`schema_version`, `id`, `kind`, optional
`record_state`/`valid_from`/`valid_to`/`supersedes`/`attributes`) plus
kind-specific required fields:

| kind         | required fields                              | purpose |
| ------------ | -------------------------------------------- | ------- |
| `entity`     | `entity_type`, `key`, `name`                 | named things: functions, TUs, compilers, the project |
| `edge`       | `source`, `relation`, `target`               | typed relations between entities |
| `claim`      | `subject`, `predicate`, `epistemic_state`, and `object` or `value` | facts and laws with explicit confidence |
| `evidence`   | `evidence_kind`, `locator`, `detail`, and `claim` or `edge` | reproducible backing for a claim/edge |
| `attempt`    | `function`, `attempted_axis`, `outcome`      | what was tried and how it ended (incl. parked caps) |
| `work_claim` | `function`, `owner`, `state`, `claimed_at`   | live ownership during campaigns |
| `tool`       | `tool_key`, `name`, `tool_kind`, `status`, `purpose` | reviewed usage policy and constraints |

Conventions:

- IDs are dotted and dated (`claim.law.iteration-limits.20260829.v1`) and stay
  stable; revisions are **new records** whose `supersedes` names the old ID.
  History is preserved, never erased.
- Claims carry epistemic states: `proposed`, `observed`, `verified`,
  `refuted`, `superseded`. Only target-backed work earns `verified`.
- Evidence locators must be reproducible anchors: source/config/test paths,
  target addresses and hashes, build commands, immutable commits. Markdown is
  rejected as a truth anchor by the validator.

## Reference resolution

References like `function:<symbol>` and `tu:<module>` resolve automatically
against the imported GameCube symbol/module tables, materializing a minimal
entity tagged `auto_resolved_from`. Write an explicit entity record only to
attach curated attributes or to disambiguate a symbol name that appears more
than once. Unknown names fail closed — a typo cannot silently create a node —
and ambiguous names demand an explicit entity.

## Write path and review boundary

Agents never edit the database or `records/` directly:

```text
propose-record ──▶ full validation ──▶ inbox/ ──(integrator review)──▶ records/
                   (schema + reference
                    resolution, upfront)
```

- `propose-record` runs the *same* validation the build applies — schema and
  reference resolution — before staging, so a proposal the build would reject
  never reaches the inbox.
- Defense in depth: if a malformed file lands in `inbox/` anyway, the build is
  **fail-soft** for inbox records only — the bad record is skipped inside a
  savepoint and reported as `inbox_rejected` in `build`/`stats`. Accepted
  `records/` remain fail-closed: an invalid accepted record stops the build,
  because accepted truth must never be silently partial.
- Inbox records are ingested with `record_state: proposed` so queries can rank
  accepted facts above them; acceptance is a human/integrator moving the file
  into `records/` and rebuilding.

## Query surface

```sh
python memory_graph/gdlmem.py context <symbol>   # the briefing command
python memory_graph/gdlmem.py search "<terms>"   # FTS over records/symbols/entities
python memory_graph/gdlmem.py tool <name>        # reviewed tool policy
python memory_graph/gdlmem.py xbox <query>       # Xbox symbols + module neighbors
python memory_graph/gdlmem.py stats | validate | audit | proposals
python memory_graph/gdlmem.py build | ensure
python memory_graph/gdlmem.py propose-record <file> | register-tool <name> ...
```

`context` is the workhorse: for one symbol it joins the GameCube symbol row,
accepted claims (match state), recorded attempts (including parked caps — the
do-not-retry list), candidate Xbox links with module neighbors, and ranked
full-text hits, under an explicit authority note. Search terms are
AND-combined; retry with fewer words before concluding the graph is silent.

The query surface is defined **once**, as a registry
(`core.build_surface_ops()`): the CLI generates its subcommands from it and
the MCP adapter generates its tools from it, so new operations appear in both
surfaces by construction. Other consumers: `tools/gdl/nearmiss.py` and
`lowmatch.py` read parked caps from attempt records; the MCP adapter is
stateless and optional — nothing depends on it.

## MCP server

The MCP adapter exposes the same query surface as host tools for agents that
prefer tool calls over shelling out. It is **optional** — the CLI is fully
self-sufficient — and there is **no daemon to manage**: it speaks stdio, so
the MCP host launches it as a subprocess per session and the database
materializes itself on the first query.

Requirements: [`uv`](https://docs.astral.sh/uv/) on `PATH`. The server's only
dependency (`mcp`) lives in a locked project under `mcp/`, resolved
automatically on first launch — nothing is installed into the build's Python.

Register it with Claude Code from the repository root:

```sh
claude mcp add gdl-memory -- uv run --project memory_graph/mcp python memory_graph/mcp/server.py
```

then restart the session so the tools load. For other hosts (or JSON-file
configuration), see [`mcp/README.md`](mcp/README.md) for the stdio config
block; set `GDL_MEMORY_ROOT` to the checkout root if the host launches the
server from elsewhere. Host configuration is machine-local — never commit
absolute paths.

Once connected, the read tools mirror the CLI one-to-one (generated from the
same registry): `memory_context`, `memory_search`, `memory_record`,
`memory_graph_stats`, `xbox_context`, `memory_tool_context`, `memory_stale`,
`memory_validate`, `memory_pending_proposals`, `memory_migration_audit`. The
only write tools are `memory_propose_record` and `memory_register_tool`, and
they write **only** to `inbox/` — the review boundary applies to MCP clients
exactly as it does to the CLI.

To verify the environment end-to-end without an MCP host:

```sh
uv run --project memory_graph/mcp python memory_graph/mcp/test_server.py
```

## Concurrency and disposability

Connections are opened per command and closed immediately; builds go to a
temp file and swap atomically; content is deterministic, so concurrent
rebuilds from the same checkout converge on identical bytes and last-writer-
wins is harmless. The database is never committed and never transported by
`git clone` — a fresh clone materializes its own view on first query.

## Data lifecycle and staleness

Records are cheap to add, so the graph needs deliberate hygiene to stay
trustworthy and bounded:

- **Parks expire against ground truth.** A `parked`/`capped` attempt is only
  valid while its function is not fully matched. Run
  `python memory_graph/gdlmem.py stale` to compare every park against the
  current `build/GUNE5D/report.json`; it buckets records as `stale_solved`
  (moot — remove them), `postprocessor_walls` (function reaches 100% only via
  a guarded WebFrank/P6Frank rule, so the source-level wall is still real and
  the record stays, annotated), `suspect_low_fuzzy` (a park claiming an
  allocator residual on a function under 70% is dubious — re-triage), and
  `missing_from_report` (symbol/report drift — re-triage). Run it as part of
  integration waves; do not let solved functions sit hidden behind stale caps.
- **Bulk imports may be pruned; earned records are superseded.** Records from
  a mechanical import (like the 2026-08-29 parked-list conversion) carry no
  unique evidence, so a moot one is simply deleted. A hand-authored attempt
  with measurements and conclusions is history: when it becomes obsolete,
  supersede it rather than deleting it.
- **One live attempt record per function.** A revisit updates that record in
  place (bump the id version, set `supersedes`, fold the prior probe into a
  one-line `attributes.axis_log` entry); git history is the lineage. Do not
  accumulate parallel attempt files per function — `stale` reports
  `multi_record_functions` as the consolidation queue.
- **Compact head, on-demand detail.** Attempt records are hard-capped at
  4 KB by the validator. `context` returns only the do-not-retry head (axis,
  outcome, residual class); full forensic attributes are fetched explicitly
  with `gdlmem.py record <id>`, so briefings stay small no matter how much
  history a function accumulates. Deep forensics that exceed the cap belong
  in an evidence record or the commit message, not the attempt head.
- **Growth check.** `stats` row counts are the early-warning signal; if
  `attempt` growth outpaces actual matching work, the inbox review boundary
  is being skipped or axes are being re-recorded instead of superseded.

## Authority and confidence

GameCube target bytes outrank everything. Accepted, evidence-backed records
outrank proposals, search hits, and any legacy text. Xbox PDB material is
cross-platform *reference* evidence: exact-name links stay candidates until
target-backed behavioral, ordering, signature, call-graph, or layout evidence
verifies them. Every `context`/`xbox` response restates this so downstream
agents cannot mistake retrieval for truth.
