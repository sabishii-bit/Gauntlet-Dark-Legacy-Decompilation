# GDL Memory Graph

This directory contains the durable, reviewable inputs for the GDL project
memory graph. The generated SQLite database is a disposable materialized view;
it lives under Git's common directory (`.git/gdl-memory/memory.sqlite`) so all
worktrees can query the same index without committing a binary database.

The existing `.claude/memory/` files are preserved. The builder indexes them as
provenance-bearing legacy documents and stages possible parking/law records for
review. It never upgrades prose to a verified fact, edits an old document, or
deletes a duplicate.

## Layout

- `records/`: accepted structured records. One JSON object per file.
- `inbox/`: agent- or tool-proposed records awaiting integrator review.
- `schema/record.schema.json`: the portable record contract.

Migration bookkeeping (coverage/duplication audit notes) lives outside the
tracked tree in the private local memory area, alongside the legacy notes it
audits. Tracked records must never contain machine-local paths or other
environment details; use repo-relative paths, target addresses, hashes, and
immutable commits as anchors.

## Commands

```sh
python memory_graph/gdlmem.py build
python memory_graph/gdlmem.py ensure
python memory_graph/gdlmem.py stats
python memory_graph/gdlmem.py validate
python memory_graph/gdlmem.py audit
python memory_graph/gdlmem.py search "register web topology"
python memory_graph/gdlmem.py context PlayerMotion
python memory_graph/gdlmem.py xbox pool_garbage_collect
python memory_graph/gdlmem.py tool Frank
python memory_graph/gdlmem.py proposals --kind parking_legacy
```

Register a new tool as a review-required proposal:

```sh
python memory_graph/gdlmem.py register-tool Ghidra \
  --kind external \
  --purpose "Inspect target control flow and data types" \
  --constraint "GameCube target bytes remain authoritative"
```

Review the resulting JSON under `inbox/`. An integrator accepts it by moving it
to the appropriate `records/` subdirectory and rebuilding. Updates use a new
record with `supersedes` pointing at the prior record ID; old revisions remain.

Other entity, edge, claim, evidence, attempt, and work-claim records can be
staged through the same review boundary:

```sh
python memory_graph/gdlmem.py propose-record path/to/record.json
```

Records that reference `function:<symbol>` or `tu:<module>` resolve
automatically against the deterministic GameCube symbol/module import — an
explicit entity record is only needed to attach curated attributes or to
disambiguate a duplicated symbol name. Unknown names still fail the build,
so typos cannot slip through.

Accepted records must not depend on Markdown for current truth. Anchor them to
source/config/test paths, target addresses and hashes, build commands and
outputs, or immutable commits. Legacy Markdown paths may appear in search
results as migration provenance, but they are not authoritative dependencies.

## Authority and confidence

Accepted records are not all equally strong. Claims use explicit epistemic
states such as `proposed`, `observed`, `verified`, `refuted`, and `superseded`.
Evidence locators should identify a commit, source path and line, target address,
object hash, or reproducible command.

Xbox PDB symbols are cross-platform reference evidence. Exact name matches are
automatically indexed only as candidates. A GameCube-to-Xbox link becomes
verified only after target-backed behavioral, ordering, signature, call-graph,
or layout evidence is recorded.

## Migration policy

Migration is intentionally incremental:

1. Index every legacy memory document and deterministic symbol/type source.
2. Inventory exact and normalized duplicate chunks without deleting anything.
3. Stage legacy parking entries and law sections as unasserted proposals.
4. Review current, high-value operational knowledge first: active claims,
   function/TU outcomes, compiler laws, postprocessor policy, and tool usage.
5. Generate concise views for agents after structured coverage is proven.

The graph augments the original records during this transition. If it is ever
abandoned, deleting the generated SQLite file loses no source knowledge.
