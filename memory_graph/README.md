# GDL Memory Graph

This folder is the project's single knowledge home. Durable facts live as
reviewable JSON records; the SQLite database is a disposable materialized view
kept under Git's common directory (`.git/gdl-memory/memory.sqlite`) so every
worktree queries the same index without committing a binary.

## Layout

- `records/`: accepted structured records, one JSON object per file.
  Subfolders (`entities/`, `claims/`, `evidence/`, `attempts/`, `tools/`) are
  organizational only; the builder reads the whole tree.
- `inbox/`: agent- or tool-proposed records awaiting integrator review.
- `schema/record.schema.json`: the portable record contract.
- `core.py`, `schema.sql`, `gdlmem.py`: the stdlib-only builder and CLI.
- `mcp/`: optional local MCP adapter (its `uv`-locked environment keeps the
  `mcp` dependency out of the build).

## Commands

```sh
python memory_graph/gdlmem.py build
python memory_graph/gdlmem.py ensure
python memory_graph/gdlmem.py stats
python memory_graph/gdlmem.py validate
python memory_graph/gdlmem.py search "register web topology"
python memory_graph/gdlmem.py context PlayerMotion
python memory_graph/gdlmem.py xbox pool_garbage_collect
python memory_graph/gdlmem.py tool Frank
python memory_graph/gdlmem.py propose-record path/to/record.json
python memory_graph/gdlmem.py register-tool <name> --purpose <p> --kind <k>
```

`context` returns a function's GameCube symbol, accepted claims, recorded
attempts (including parked caps), Xbox candidate links, and search hits.
`tool` returns reviewed usage policy and constraints.

## Authoring records

- Records referencing `function:<symbol>` or `tu:<module>` resolve
  automatically against the deterministic GameCube symbol/module import.
  Write an explicit entity record only to attach curated attributes or to
  disambiguate a duplicated symbol name. Unknown names fail closed.
- `propose-record` validates fully — schema and reference resolution — before
  staging into `inbox/`. A malformed inbox file cannot break the build: it is
  skipped and reported as `inbox_rejected` in `build`/`stats`.
- An integrator accepts a proposal by moving it into `records/` and
  rebuilding. Update facts with a new record whose `supersedes` names the
  prior record ID; do not erase history.
- Anchor records to source/config/test paths, target addresses and hashes,
  reproducible commands, or immutable commits. Markdown is not a truth anchor.

## Authority and confidence

Claims carry explicit epistemic states (`proposed`, `observed`, `verified`,
`refuted`, `superseded`). Xbox PDB symbols are cross-platform reference
evidence: exact-name matches are indexed automatically as candidates at 0.5
confidence and become verified only with target-backed behavioral, ordering,
signature, call-graph, or layout evidence. GameCube target bytes remain
authoritative over everything.

The legacy Markdown memory was terminally migrated on 2026-08-29: parked caps
became per-function `attempt` records, the compiler-law corpus became
`claim` records, and the remaining prose was retired. The builder still
indexes an optional `memory_graph/legacy/` directory if one ever reappears,
but nothing depends on it.
