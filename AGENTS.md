# Gauntlet Dark Legacy Decompilation — Agent Workflow Contract

This file is the authoritative workflow contract for every LLM or agent
working in this checkout, on any platform. Read it completely before
inspecting candidates, editing source, running a build, or delegating work.

The objective is byte-exact matching decompilation of the GameCube target
(GUNE5D), advanced monotonically: every verified improvement is kept, nothing
verified is ever regressed.

## Instruction precedence

1. The user's current explicit request.
2. This `AGENTS.md`.
3. `README.md` for human-facing setup, build, and project context.
4. Accepted records in the project memory graph (`memory_graph/records/`).

Memory-graph search results, pending inbox proposals, and automatic Xbox
exact-name links are evidence candidates, not policy or verified truth.

## Knowledge and memory

The memory graph is the project's only knowledge system. There are no
Markdown memory files; do not create ad-hoc notes, ledgers, or scratch
documents as durable memory.

```text
python memory_graph/gdlmem.py ensure
python memory_graph/gdlmem.py context <symbol>
python memory_graph/gdlmem.py search "<terms>"
python memory_graph/gdlmem.py tool <tool-or-workflow>
```

- Run `context` before claiming a function or TU. It returns accepted claims,
  recorded attempts, parked caps, and Xbox candidate links for the symbol.
- A recorded attempt with outcome `parked`, `capped`, or `negative` on an axis
  is a hard do-not-retry. Revisit only with a genuinely new source-level idea,
  and record the revisit either way.
- Run `tool` before using a specialized postprocessor or analysis tool; the
  returned constraints are binding.
- Search terms are AND-combined; if a search returns nothing, retry with fewer
  words before concluding the graph is silent.
- Record durable results as structured records via
  `gdlmem.py propose-record` (attempts, claims, evidence, entities, edges) or
  `register-tool`. Proposals are fully validated before staging and require
  integrator review before acceptance into `records/`.
- Keep attempt records compact: one live record per function (revisits bump
  the version, set `supersedes`, and fold prior probes into one-line
  `attributes.axis_log` entries), a hard 4 KB cap, and the do-not-retry
  conclusion in the head fields. `context` returns only that head; fetch full
  forensics with `gdlmem.py record <id>` when actually revisiting. References like
  `function:<symbol>` and `tu:<module>` resolve automatically against the
  symbol import; add an explicit entity record only for curated attributes or
  name disambiguation.
- Update facts by superseding records, never by deleting them. Anchor records
  to code paths, target addresses/hashes, reproducible commands, or immutable
  commits — never to Markdown.
- The generated SQLite database is never committed. Reviewed record-authoring
  conventions live in `memory_graph/README.md`.

### When to query

Query at these moments, not only at claim time:

1. **Before the first edit** — `context <function>` (plus `xbox <function>`
   for real names and PDB source-order neighbors). A recorded cap on your
   planned axis is a VETO; an empty briefing is a green light, not an error.
2. **At every classified residual** — before spending an A/B probe on a diff
   cluster, search the graph by its **codegen signature**, not the function
   name: `search "lfsx"`, `search "assignment in condition"`,
   `search "chained assignment"`, `search "extra fmr"`, `search "cror"`.
   Law and attempt bodies are full-text indexed; most recurring MWCC residual
   shapes already have a recorded lever, and finding it costs one query
   against a multi-build rediscovery. Fetch the full text with
   `gdlmem.py record <id>` before applying it.
3. **Before naming anything** — search a struct offset, field, or symbol name
   before inventing one; a header, PDB link, or prior record may already own
   it, and cross-TU renames have their own recorded procedure. For struct
   fields specifically, `gdlmem.py struct <TypeName> --offset 0xNN` resolves
   a raw byte offset to the Xbox PDB field covering it (and reports exact
   pad-gap sizes) — use it instead of writing `*(T*)(p + 0xNN)` casts.
4. **Before parking** — search the residual class for known levers first;
   then record the cap with the exact probes tried, so the next agent's
   step 1 screen works.

A search hit is evidence, not instruction: laws are compiler-scoped
observations. Re-verify against your function's target bytes before applying
one, and supersede the law if your target contradicts it.

## Mandatory result policy

- Use portable C/C++ only. Never add inline assembly, function-level assembly,
  raw PPC instruction words, embedded machine code, binary inclusion, or any
  other percentage-gaming mechanism.
- The sole post-compile exception is the existing fail-closed Frank/WebFrank/
  P6Frank harness, used exactly within the constraints returned by
  `gdlmem.py tool <name>`. Never weaken a guard, add an unaudited rule, or use
  postprocessing to hide structural, operand, relocation-payload, ABI,
  semantic, or data differences.
- A verified fuzzy improvement is valuable even when not exact: keep and
  commit it after validation. Byte-exact remains the goal; exactness is not a
  prerequisite for retaining better work.
- Never regress a function that is already exact, and never improve one
  function by silently regressing siblings, linked data, the DOL checksum, or
  overall project progress.
- Preserve unrelated user changes and dirty files.
- The best retained result is monotonic: never replace it with a worse one.

Every outcome is one of:

- `EXACT`: instruction stream and relocations exact (or `real 0` with only a
  verified symbol-name normalization issue) and the full build gate passes.
- `IMPROVED`: objectively better than baseline with all acceptance checks
  passing. Keep and commit.
- `CAPPED`: no retained improvement after bounded, documented attempts.
  Restore only the unsuccessful probes, keep any earlier best, record the cap
  as an attempt record, and move on.
- `VETO`: a prior cap or ownership conflict was discovered before editing; no
  edits made.

Accepting a non-exact change requires all of: semantics correct or improved;
the owning object builds; the target function's score objectively improves
(record before/after instruction counts, fuzzy, `real`); no previously exact
function regresses; no unrelated regression erases the gain; `git diff
--check` passes; no prohibited assembly. An instruction-count gain that
worsens fuzzy/real scoring is not an improvement; when metrics disagree,
inspect the actual diff, state the conflict, and preserve the best verified
project-level result.

## Metric and semantic discipline

- Fuzzy percentage and matched (exact) percentage are separate metrics. Report
  both; never describe a fuzzy gain as an exact gain.
- On functions larger than roughly 4000 instructions, arbitrate probes with
  the objdiff fuzzy score from `build/GUNE5D/report.json`, not `real` line
  counts: register-color cascades move `real` in both directions.
- A green linked DOL proves little for a `NonMatching` TU: the linker can use
  original extracted bytes instead of the compiled object. Validate
  nonmatching work with the object build, function diff, calls/CFG/field
  audit, sibling checks, and consistent before/after scores. A successful
  compile is not semantic evidence; large generated bodies need deliberate
  ABI, call, field-offset, branch, and sibling review before commit.
- Promotion from `NonMatching` to `Matching` requires the complete
  postprocessed object to be exact — text, relocations, read-only/writable
  data, BSS/common layout, exception metadata — plus a fresh full-link DOL
  checksum gate. Never edit `configure.py` merely to flip a fuzzy TU.
- Classify every candidate's residual before working it: `STRUCTURAL`,
  `STACK_LAYOUT`, `SCHEDULE`, `REGISTER_ONLY`, `RELOCATION`, or `NON_TEXT`.
  Prefer source-reachable structural gaps for impact work; register/schedule
  residuals are exact-closure work.
- Queue generation: `python tools/gdl/lowmatch.py --refresh` (impact or
  bottom-up), `tools/gdl/nearmiss.py` for deliberate exact closure. Parked
  caps come from the memory graph; do not claim every candidate is parked
  without a current symbol-by-symbol screen.

## Baseline and matching loop

Never edit before saving the baseline: build or confirm the owning object,
record target/current instruction counts, `real`, fuzzy, `--ops`
classification, frame size, saved-register set, major calls, and branch
architecture. Confirm `git diff -- <owned path>` contains no one else's hunk.

For functions larger than roughly 50 instructions, read the complete target
assembly before the first rewrite (`python tools/gdl/fnasm.py <unit> <fn>`).
Map the prologue/frame, parameter and saved-register homes, branches, loops,
calls, relocations, and stack slots. Use Ghidra for semantics and broad CFG,
never for exact statement grouping, storage classes, or evaluation order.
Reference sources (Xbox PDB material under `research/xbox_symbols/`, game
headers, siblings, SDK/MSL/zlib/fdlibm lineage) supply semantics; the target
assembly remains the authority for ABI, control flow, order, and bytes.

Work in this order: semantics/ABI/types/control flow and calls; opcode/count
structure; frame and local layout; saved-register and FPR roles; only then
scheduling and register-color residuals.

Reconstruction quality (fakematches): a fakematch is code that compiles to
the right bytes but is clearly not what a Midway developer plausibly wrote
in ~2001 C — nonsense casts, contrived temporaries, or raw
`*(T*)(p + 0xNN)` offset arithmetic where a real field is knowable. This
project tolerates such forms as **staged progress** (a verified fuzzy gain
is kept), but they are reconstruction debt, not an endpoint:

- Before writing any raw-offset access, resolve the field first:
  `python memory_graph/gdlmem.py struct <TypeName> --offset 0xNN` returns
  the Xbox PDB field covering that offset, the full layout, and the exact
  pad-gap sizes between known fields. Prefer the named field or a typed
  view struct; verify the offset against GC target displacements before
  adopting the name — GC records can be more compact than Xbox.
- Padding locals (`u8 unused[N]`) that close a frame-size delta are an
  accepted idiom here — they usually stand for genuinely unrecovered
  locals — but a pad does not always resolve a frame gap: a missing inline
  expanded at the right point is sometimes the true fix (verified in other
  MWCC projects and this one). Say what the pad stands in for in the
  function's attempt record.
- When only obviously artificial code would close the final bytes, cap the
  function with a record instead of committing the trick (see Mandatory
  result policy for the hard prohibitions).

### Cleaning pre-existing fakematches (de-fakematch campaigns)

Raw-offset code already in the tree (~1,500 sites, census 2026-08-30) is
cleaned in claimed, TU-scoped passes under the same monotonic-result rules
as matching work:

1. **Scope by name authority.** Convert only accesses resolvable through a
   GC-verified struct (the `include/game/` headers, and
   `gdlmem.py struct <T> --offset 0xNN` for the covering field). Never
   adopt an Xbox PDB name whose offset is not GC-verified: leave the
   access raw, or introduce a file-local view struct with the verifying
   target-asm evidence noted. Never invent names.
2. **Gate every region.** Take the fndiff baseline first. For a matched
   function the conversion must stay byte-identical
   (`fndiff --clean` = MATCH, real 0) or be reverted. For a fuzzy function
   it must measure equal-or-better on `real` and `--ops` structure or be
   reverted — and a conversion that *improves* the score is a normal
   matching win: commit it as one.
3. **Screen the recorded constraints before converting.** Member
   conversion changes address webs; the known hazard laws are
   `claim.law.lwzu-idiom-web-retention` (a `p += N` load-update idiom
   needs its original-type web kept alive — convert around it, not
   through it), the cast-transit no-CSE family, and the addr-CSE laws.
   `context <function>` also surfaces per-function constraints (e.g. the
   do_enemy_collide behavior chains are recorded off-limits).
4. **Convert in small regions, rebuild the object, re-gate**; two failed
   forms for the same region means leave that region raw and note it.
5. **Record one compact attempt record per TU pass** (sites converted,
   sites left raw and why, any score changes) — not one per function —
   plus a law record only for a newly verified constraint. Commit style:
   `De-fakematch <fn> (byte-identical)` or the normal improvement line
   when the score moved.

Core tools, from the repository root:

```text
python configure.py
ninja build/GUNE5D/<object-path>.o
python tools/gdl/fnasm.py <unit> <function>
python tools/gdl/fndiff.py <unit> <function> --count | --ops | --clean
python tools/gdl/matchtool.py probe <unit> --brief
python tools/gdl/lowmatch.py --max 50 --min-size 200 --sort impact
python configure.py progress
```

Do not invent ad-hoc diff pipelines when a project tool already provides the
measurement. After each meaningful change: rebuild the owning object, re-score,
compare against both the original baseline and the best retained result, and
discard strictly-worse probes immediately.

Iteration discipline: structural and semantic gaps may be iterated while they
converge. Once opcode streams align and the residual is register allocation or
scheduling only, try at most three genuinely distinct source-shape axes per
pass; two identical A/B results kill an axis. Re-read the target disassembly
before exotic theories. Parked probes are alignment-sensitive — a probe that
measured negative may turn positive after surrounding regions improve, so
re-A/B recorded probes after nearby fixes rather than trusting the old sign.

## Verification gates

Exact result: owning object builds; instruction counts match; `fndiff --ops`
and `--clean` report exact; no unexpected sibling/data changes; `git diff
--check` passes; no assembly or modified `.s`/`.asm`; full `ninja` succeeds
and the linked `main.dol` checksum is OK; post-link scoring still exact. Never
trust a stale `build/GUNE5D/ok` — the current `ninja` invocation itself must
succeed.

Non-exact improvement: recorded before/after scores; correct semantics and a
successful object build; the function diff inspected (not just an aggregate
percentage); siblings and TU data checked; project progress measured
consistently; full build when the change can affect linked output, shared
headers, data layout, or exact siblings.

## Git and workspace safety

Before any work: `git status --short`, `git branch --show-current`,
`git rev-parse --show-toplevel`. Record pre-existing dirty files — they belong
to the user or another worker.

- Never use destructive cleanup (`git reset --hard`, `git checkout -- <file>`)
  on shared work, and never rewrite another worker's uncommitted changes.
- Stage only files you own and have audited; do not fold unrelated changes
  into a matching commit.
- Do not push unless you are the integrator and the user has authorized it.
- Never commit machine-local paths, credentials, or personal environment
  details, and never commit the generated memory database. If private content
  lands in history, tell the user immediately rather than papering over it.

Commit messages are plain one-liners without attribution trailers:

```text
Match <FunctionName>
Improve <FunctionName> match <before>% -> <after>%
Reconstruct <FunctionName> (<before> -> <after> real diffs)
```

## Multi-agent coordination

Default topology for a campaign: one integrator/root plus at most three
concurrent workers (the ceiling excludes the root). Use the strongest
available coding model for the root and every writing worker; a faster model
only for bounded read-only scouting. Platform-specific model names and
subagent configuration live in platform config, not in this file. Do not
deploy workers for small, sequential, or documentation-only tasks.

Roles: scout (read-only discovery and history/cap checks), semantic
reconstructor, codegen matcher, integrator. Workers may carry a screened
candidate end-to-end regardless of role label.

Rules:

- Assign disjoint TUs only. MWCC constant pools, declaration order, inlining,
  data ownership, and register allocation create whole-TU coupling; never let
  two writers touch one TU.
- Every worker announces before editing:

  ```text
  CLAIM <path>::<function>
  BASELINE target=<n> current=<n> fuzzy=<p> real=<n>
  SCOPE portable C/C++; owned files=<paths>; no push
  ```

- Screen graph attempts and ownership before accepting a claim; report `VETO`
  on a discovered cap instead of editing.
- Workers commit on their own branch/worktree and never push; the integrator
  merges one result at a time, revalidates after each, resolves config/symbol
  conflicts by semantic ownership (never by blindly taking a side), and
  repairs or reverts any integration that reduces the best verified result.
- Every worker must read this file completely before acting. `AGENTS.md` is
  tracked, so every worktree and clone carries it; the orchestrator verifies
  each worker actually read it.

Cross-fleet concurrency (multiple independent agent fleets sharing `main`):

- Ownership is advertised through `work_claim` records. Before a fleet's first
  edit, its integrator proposes claims (`gdlmem.py propose-record`) naming the
  owner and the full TU scope in `attributes.scope`, commits them, and
  **pushes to `main` before spawning workers** — an unpushed claim protects
  nothing. One claim per TU flagship function is sufficient.
- Screening: `context <function>` surfaces claims from every fleet. A foreign
  claim with `state: active` is a VETO on that entire TU scope, exactly like a
  recorded cap.
- Release claims in the same push that lands the merged result: delete the
  fleet's own inbox claim files (bulk-imported lifecycle records are pruned,
  not superseded). A claim whose owner has no commit touching the scope within
  roughly a day is presumed abandoned: verify via `git log -- <paths>`, then
  remove it in a standalone commit noting the cleanup.
- Push races are expected: pull/rebase before push, and after any rebase that
  pulled in another fleet's work, re-run the build gate before pushing. Never
  force-push, and never resolve a conflict in another fleet's owned files by
  taking your side — stop and coordinate through the integrator or user.
- The graph inbox is the cross-fleet mailbox: records land there
  fleet-by-fleet, and each fleet accepts only its own proposals into
  `records/`. Never move, edit, or delete another fleet's inbox files.
- Worktree plumbing: the fleets drive this repository with different git
  flavors, whose worktree add/remove churn rewrites the shared registry
  (`.git/worktrees/*/gitdir` and each worktree's `.git` link) in
  incompatible path forms, breaking plain git commands in worktrees
  ("does not point back" / mixed-separator paths). The interoperable form
  is forward-slash Windows paths (`W:/...`); run
  `python tools/gdl/fix_worktrees.py` from the main checkout to normalize
  every registered worktree (plumbing files only — it never touches
  tracked content, so it is safe to run while other fleets work). Create
  new worktrees with forward-slash paths to avoid seeding the problem.

Worktrees: writing workers use separate worktrees/branches; the shared
checkout is read-only to them. Reuse existing clean campaign worktrees before
creating new ones; never create ad-hoc repository clones when a worktree
suffices; never delete or copy over the shared `build/` or `orig/`
directories. At session end classify every worktree/branch: `READY`,
`DIRTY/STRANDED` (with exact paths and next safe action), `MERGED`, or
`CLEAN`, and record `git rev-list --left-right --count main...<branch>` for
unmerged branches. Cleanup after integration requires proof: clean status, no
unique commits (or deliberately preserved refs), merged tip, then
`git worktree remove` + `prune` and `git branch -d` only. Never use forced
deletion as routine cleanup; preserve dirty or divergent work until audited.

Context-budget shutdown: at roughly 25% context remaining stop dispatching new
work; at 15% stop experiments at a compilable boundary, commit or restore
every owned hunk, record each worker's state and next action as graph
records, run the final gates that fit, and hand off from the actual Git state.
Preserving and documenting work outranks one more candidate.

## Required final report

```text
FUNCTION/TU:
STATUS: EXACT | IMPROVED | CAPPED | VETO | BLOCKED
OWNED FILES:
BASELINE: target/current, fuzzy, real, frame
BEST: target/current, fuzzy, real, frame
SEMANTIC CHANGES:
SOURCE-SHAPE CHANGES:
ATTEMPTED AXES:
REMAINING RESIDUAL:
VERIFICATION:
COMMIT:
RECORDS: <attempt/claim records proposed or accepted>
UNRELATED DIRTY FILES PRESERVED:
```

Before sending it, propose the session's durable outcomes (attempts, laws,
measurements) as graph records — do not leave techniques only in chat
history — and re-run status, branch divergence, and progress so the report
reflects the current Git state, not an earlier mental model. Do not report a
function as fresh without the history screen, an improvement without numbers,
or a gate as passed unless the command succeeded in the current tree.

## Short version

Query the graph, read the target, recover correct portable C, measure every
iteration, retain the best verified result, commit exact and fuzzy
improvements, never regress exact work, record attempts and laws as graph
records, coordinate exclusive TU ownership, and let one integrator serialize
merges and project-wide validation.
