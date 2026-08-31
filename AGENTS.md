# Gauntlet Dark Legacy Decompilation — Agent Workflow Contract

> **FIRST-FIVE-MINUTES TRAPS (read before anything else):**
> 1. In a linked worktree, `git` through the Bash/MSYS tool fails with
>    `fatal: not a git repository` — it cannot resolve `W:/`-form gitdir
>    links. Run **ALL git commands through PowerShell**. Do not diagnose
>    the git failure; switch shells.
> 2. `gdlmem.py` global flags (`--out`, `--root`, `--compact`) go
>    **BEFORE** the subcommand: `gdlmem.py --out r.json brief <tu>`.
> 3. Large `gdlmem.py` results auto-spill to `build/gdlmem_out/` and print
>    the path — Read that file; don't re-run the query.

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
python memory_graph/gdlmem.py brief <tu-path-fragment>
python memory_graph/gdlmem.py context <symbol>
python memory_graph/gdlmem.py find [--kind K] [--function F] [--tu T] [--outcome O] [--residual R] [--law L] [terms]
python memory_graph/gdlmem.py search "<terms>"
python memory_graph/gdlmem.py laws [--query <term>] [--tag <tag>] [--full 1]
python memory_graph/gdlmem.py record <id1>,<id2>,...   # batch detail fetch
python memory_graph/gdlmem.py tool <tool-or-workflow>
```

Fetch your law screen in ONE call: `laws --tag core-screen --full 1`
(de-fakematch) — `brief` also lists `matching_laws` (schedule/register/
entry levers) for matching sessions. `record` takes a comma-separated id
list. Do not loop single-id subprocess calls.

Law tags are a controlled vocabulary — `laws` reports `tags_available`
with live counts (don't guess names), and `--tag core-screen` returns the
mandatory de-fakematch screen set. Proposals using tags outside the
vocabulary are rejected at staging.

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
  integrator review before acceptance into `records/`. **Workers never run
  `gdlmem.py accept`** — propose, commit the inbox files on your branch, and
  stop; acceptance and claim release happen in the integrator's merge (the
  command enforces this by refusing to run off the main branch).
- `laws` lists the whole codegen-law corpus newest-first (scope, age_days,
  supersession); read it at the start of every pass instead of waiting to be
  handed record ids. Every attempt proposal MUST carry
  `attributes.law_screen` naming the laws screened and whether each applied
  (`none applicable: <why>` is acceptable) — `propose-record` rejects the
  proposal otherwise.
- Records are freshness-stamped at staging (`valid_from` + `recorded_at`),
  and query results carry `age_days`: prefer newer records when two disagree,
  and check `superseded_by` before applying an old law.
- Keep attempt records compact: one live record per function (revisits bump
  the version, set `supersedes`, and fold prior probes into one-line
  `attributes.axis_log` entries), a 16 KB cap, and the do-not-retry
  conclusion in the head fields. Each function keeps at most **5** attempt
  records: when acceptance pushes a function over, the integrator runs
  `gdlmem.py prune-attempts` (dry-run first — fold any still-live
  do-not-retry cap from an ejection into the surviving newest record), then
  `--apply`, commits the deletions, and rebuilds; `build` flags offenders as
  `attempt_overflow`. `context` returns only the record head; fetch full
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

0. **At spawn, for TU-scoped work** — `brief <tu>` (e.g.
   `gdlmem.py brief game/enemy/enemy`) is the one-call briefing: function
   roster with scores, every live attempt record (parks and caps first —
   each is a VETO on its axis), active claims from every fleet, the
   core-screen law list, and the TU's raw-offset debt. Start here; it
   replaces running `context` per function for the roster overview.
1. **Before the first edit of a specific function** — `context <function>`
   (plus `xbox <function>` for real names and PDB source-order neighbors).
   A recorded cap on your planned axis is a VETO; an empty briefing is a
   green light, not an error. `find` slices records by facet when you need
   a cross-section (`find --tu game/ui/select --outcome parked`,
   `find --law lwzu-idiom --kind attempt`).
2. **At every classified residual** — before spending an A/B probe on a diff
   cluster, search the graph by its **codegen signature**, not the function
   name: `laws --query "lfsx"`, `search "assignment in condition"`,
   `search "chained assignment"`, `search "extra fmr"`, `search "cror"`.
   `laws` (no filter) lists the entire current corpus with ages — cheaper
   than guessing which ids to fetch.
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
   `gdlmem.py struct <T> --offset 0xNN` — it now reports project headers
   FIRST as `local_headers`, then PDB reference). Never adopt an Xbox PDB
   name whose offset is not GC-verified: leave the access raw, or
   introduce a file-local view struct with the verifying target-asm
   evidence noted. Never invent names. Three proven authority moves in
   priority order: (a) an opaque forward-declared struct with a known
   size deserves a `struct <name>` PDB-body lookup BEFORE per-site work —
   completing the body converts whole loops at once; (b) after a `struct`
   miss, grep `research/xbox_symbols/misc.h` for the struct NAME (the tsv
   index is incomplete — PBFRAMEBUF's full 0x200 layout was found this
   way after the op missed); (c) before inventing ANY file-local view,
   grep the TU for an existing `typedef struct`/`struct <Name>` of the
   same purpose — critter.c accumulated two CONFLICTING partial
   reconstructions of one struct because nobody checked.
2. **Gate every region.** Take the fndiff baseline first. For a matched
   function the conversion must stay byte-identical
   (`fndiff --clean` = MATCH, real 0) or be reverted. For a fuzzy function
   it must measure equal-or-better on `real` and `--ops` structure or be
   reverted — and a conversion that *improves* the score is a normal
   matching win: commit it as one. The mechanical form of this gate is
   `python tools/gdl/defake_gate.py baseline <unit>` once at pass start,
   then `check <unit>` after every region (exit 1 = revert before
   committing; `--update-improved` banks a better score as the new
   baseline). It scores from the real ninja object via fndiff — trust it
   over matchtool presets, which have diverged from per-TU cflags.
3. **Screen the recorded constraints before converting.** Member
   conversion changes address webs; the known hazard laws are
   `claim.law.lwzu-idiom-web-retention` (a `p += N` load-update idiom
   needs its original-type web kept alive — convert around it, not
   through it), the cast-transit no-CSE family, and the addr-CSE laws.
   `context <function>` also surfaces per-function constraints (e.g. the
   do_enemy_collide behavior chains are recorded off-limits).
4. **Convert in small regions, rebuild the object, re-gate.** After a
   failed form, read the failure diff and name the cause before choosing
   the next form; the retry budget is generous — try at least three
   targeted counter-forms (the law corpus names one for every known
   failure class) before leaving a region raw, and re-test earlier
   left-raw regions after later conversions land: cleanup is
   alignment-sensitive too, and regions that failed early in a pass have
   repeatedly converted cleanly late in it. Leave a region raw only with
   the failure causes understood and recorded.
5. **Record one compact attempt record per TU pass** (sites converted,
   sites left raw and why, any score changes) — not one per function —
   plus a law record only for a newly verified constraint. Alongside the
   required `attributes.law_screen` prose, list the laws that actually
   governed your conversions in `attributes.laws_applied` (a JSON array of
   record ids): the build materializes these into per-law application
   counts, which is how the corpus learns which laws pay off. Commit
   style: `De-fakematch <fn> (byte-identical)` or the normal improvement
   line when the score moved.

Wave planning: `python memory_graph/gdlmem.py debt` is the census —
per-TU raw-cast + PF() counts, heaviest first, timestamped for
wave-over-wave comparison. Its counts include legitimate raw forms
(protected webs, structless pools), so read the TU's attempt records
before claiming. `gdlmem.py stale` now also emits `reopen_candidates`:
parks whose score moved since parking, and conversion caps that never
documented the failing form (re-try those with offsetof-on-raw-pointer
per `claim.law.offsetof-overturns-typed-alias-caps`).

Symbol naming (fn_*/lbl_* placeholders): `gdlmem.py symaudit [--tu X]`
aligns each GC TU's function roster against its Xbox PDB module by
position (LIS-anchored on shared names, both link orientations — some
TUs link in REVERSE source order) and reports three things:
`proposals` (exact-gap candidates — one-to-one positional evidence),
`spelling_mismatches` (a GC invented name sitting where the PDB has a
different real name, e.g. UpdateCam vs CamUpdate), and `no_candidate`
(the revisit set). A `NAME-TAKEN` confidence means a live GC symbol
already uses the candidate name — adopting it links multiply-defined;
resolve the existing holder first. Adopting a candidate is a RENAME:
verify it against the function's own behavior AND its CALLERS' context
(position is evidence, not proof — the first adoption pass rejected 8
of 38 this way, including two confirmed positional swaps), then execute
with `gdlmem.py rename-symbol <old> <new> --apply`, which holds all
five invariants at once (collision check, symbols.txt+src+include
rename, graph-record anchor patch, stale .s/.o cleanup) and prints the
gate steps; hand-rolling those steps is how one gets missed. Per-TU revisit records use claim predicate
`symbol_naming` (subject `tu:<module>`, value = the no-candidate list +
ambiguous spans + naming notes) so future sessions can ponder names
without re-deriving the audit; update them by superseding when symbols
get named.

Parked-record hygiene (integrator duty): parked/capped records live as
per-function attempt records under `records/attempts/parked/` — never as
entries appended to a bulk list (the legacy
`claim.parked-unresolved` blob was migrated 2026-08-30; its v2 residual
holds only genuinely non-per-function entries). After each integration
wave, run `gdlmem.py stale`: delete parks in `stale_solved` (the function
now measures fully matched — the park is moot; git history keeps the
text), and queue `reopen_candidates` for cheap re-probes. A park must be
anchored to its function or `context`/`brief`/`find` cannot surface it.

Core tools, from the repository root:

```text
python configure.py
ninja build/GUNE5D/<object-path>.o
python tools/gdl/probe.py <unit> <fn> [--ops]  # MATCHING loop: build+score+verdict, one call
python tools/gdl/defake_gate.py check <unit> --rebuild  # DEFAKE loop: build+gate, one call
python tools/gdl/fnasm.py <unit> <fn> [0xA:0xB | i:j] [--ours | --diff]
python tools/gdl/fndiff.py <unit> <function> --count | --ops | --clean
python tools/gdl/defake_rewrite.py <file> --base X --type T --map off=field,...
python tools/gdl/fuzzy.py <unit> [<fn>]        # fuzzy from last report, no regen
python tools/gdl/xrefnum.py <const...> [--cast-only]  # who else uses this offset
python tools/gdl/externcheck.py                # cross-TU extern type conflicts
python tools/gdl/matchtool.py probe <unit> --brief
python tools/gdl/lowmatch.py --max 50 --min-size 200 --sort impact
python configure.py progress
```

Loop discipline: the edit loop is ONE command now — `probe.py` (matching:
prints BASELINE/IMPROVED/REGRESSED/NEUTRAL against a remembered best) or
`defake_gate.py check --rebuild` (defake: builds first, prints each
regressing fn's --ops summary inline). Never hand-pair ninja+fndiff or
ninja+gate again. To read a residual: take `--ops`'s `@0xA-0xB` offsets
straight into `fnasm.py <unit> <fn> 0xA:0xB --diff` — the aligned view;
NEVER eyeball target and ours at the same absolute offset (instruction
drift makes that silently wrong). A `--ops` multiset DIFFERS line means
structure is hiding even if it "looks like regalloc noise" — and when a
small function's real improves while fuzzy dips, read the --ops diff and
apply claim.law.fuzzy-can-underweight-a-real-improvement: the finer
metric wins once the diff is read.

Reading `--ops`: each cluster carries `@0xA-0xB` function-relative byte
offsets — paste them straight into `fnasm.py <unit> <fn> 0xA:0xB` (target)
or `... --ours` (our object). The `opcode multiset:` line above the
clusters is the classifier: IDENTICAL = pure reorder (schedule-class);
DIFFERS = something STRUCTURAL is still hiding even when the diff "looks
like regalloc noise" — chase the named +/- opcodes before any register
theory (a cross-TU extern type conflict announced itself exactly this way:
fctiwz/stfd in one stream only).

Shell note: in worktrees run git through PowerShell only. The Bash tool
CANNOT run multi-line inline code of ANY kind on this platform (a cmd-shim
injects `goto :error` artifacts into `python -c`/here-docs — every fleet
worker rediscovers this): write a script file to the scratchpad and run
`python <file>`. Global gdlmem flags (`--out`, `--compact`) go BEFORE the
subcommand; use `--out <windows-path>` for any large JSON result instead
of shell pipes/redirects (PowerShell pipes re-encode with a BOM, and
Bash-side `/tmp` paths are invisible to Windows python).

First-build note (fresh worktree): `configure.py` alone emits a BOOTSTRAP
build.ninja (tool download + DOL split only, no object graph) — that is
not breakage. Run `ninja -j2` and let it bootstrap: it downloads tools,
splits the DOL, re-invokes `configure.py` itself, and only then builds
objects. Judge the setup by whether `ninja` finishes green, never by the
size of the first build.ninja. Unit paths for every tool above are
`game/.../file.c` forms — no `src/` prefix (the tools now strip a stray
`src/` themselves, but errors from older invocations show the bare
`missing: build/...` form).

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

Do not give up early. The 3-axis cap applies ONLY to residuals already
reduced to pure register/schedule noise — a structural gap, a score-visible
delta, or a near-matching opcode stream is NOT subject to it and deserves
sustained iteration. A failed probe is data, not a stop signal: before
parking any axis, read the failing `--clean` diff and state WHY it
regressed; the explanation usually names the next form to try (a
register-pressure cascade, a split web, a killed fusion each have known
counter-forms in the law corpus — search them). Capping without a stated
root cause is premature and will be sent back by the integrator. Most
"impossible" residuals in this project's history fell to a later form after
the first two failed.

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
concurrent workers (the ceiling excludes the root). The user may authorize
expanded capacity (currently authorized: up to nine concurrent workers,
2026-08-30); at expanded capacity, workers should use reduced build
parallelism (`ninja -j2`, falling back to `-j1` on "User break, cancelled"
contention) since concurrent fleet builds share one machine. Prefer copying
the few needed gitignored artifacts into a fresh worktree over junctioning
`orig/` — copies remove the reparse-point removal hazard entirely. Use the strongest
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
- **Acceptance is atomic in the shared checkout**: perform the inbox→records
  move, the claim deletion, and the commit as one uninterrupted step — never
  leave memory_graph files moved or staged uncommitted, because every other
  fleet's merges are blocked while your worktree state is dirty (three merge
  collisions on 2026-08-30 came from exactly this). The supported path is
  `python memory_graph/gdlmem.py accept <record-id...> --release <claim-id>`,
  which moves the files, stages exactly the touched paths, rebuilds the
  graph, and prints the pathspec-limited commit command — run that commit
  immediately.
- `gdlmem.py claims` lists every fleet's work claims with owner, scope, age,
  and stale flags — check it before claiming instead of reading inbox
  filenames.
- **Commit with explicit pathspecs in the shared checkout**
  (`git commit <paths> -m ...`), never a bare `git commit` after `git add`:
  the index is shared, and a bare commit sweeps in whatever another fleet
  has staged — this silently reverted a merged TU once (world.c,
  2026-08-30, repaired) when a stale foreign-staged copy rode along.
- **Never run `git clean` in the shared main checkout.** Every "mystery
  deletion" incident of 2026-08-30 (downloaded compilers/binutils/objdiff
  under `build/`, the retail `orig/GUNE5D/sys/main.dol`) matches `git
  clean`'s signature: it destroys gitignored files that every fleet's
  build depends on and that take real time to restore. Clean only your
  own worktree, never the shared checkout, and never with `-x`.
- **Never delete anything under `orig/`.** The README's note that "the disc
  image can be deleted" refers to the user's disc-image file (ISO/RVZ)
  only — `orig/GUNE5D/sys/main.dol` and the extracted files are shared,
  load-bearing input for every fleet's build (the file carries a
  read-only attribute as a guard; do not clear it). It has been deleted
  by an unidentified fleet process at least twice (2026-08-30). If it is
  missing, restore it from any verified `build/GUNE5D/main.retail.dol`
  (SHA-1 `7cba77aa496eb0fc5ffec60efd9680aa9635d679`, byte-identical to
  retail by construction), verify the hash, and report the incident.
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
  Additionally: a POSIX-shell (Bash-tool) git cannot resolve these
  worktrees' `.git` link files at all even when the plumbing is healthy
  (the gitdir round-trips through a `W:/` path the `/w/` mount can't
  follow) — run ALL git commands in a worktree through PowerShell;
  non-git tools (python, ninja) work fine from either shell.

Worktrees: writing workers use separate worktrees/branches; the shared
checkout is read-only to them. Reuse existing clean campaign worktrees before
creating new ones; never create ad-hoc repository clones when a worktree
suffices; never delete or copy over the shared `build/` or `orig/`
directories. Never junction or symlink a worker's ignored `orig/` or `build/`
path to the shared checkout: `git worktree remove` can follow the reparse point
and delete shared inputs. Provision only the exact required ignored files, and
reject/remove any reparse point before removing the worktree. At session end
classify every worktree/branch: `READY`,
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
