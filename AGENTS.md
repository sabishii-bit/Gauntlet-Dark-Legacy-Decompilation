# Gauntlet Dark Legacy Decompilation — Agent Workflow Contract

> **FIRST-FIVE-MINUTES TRAPS (read before anything else):**
> 1. In a linked worktree, `git` through the Bash/MSYS tool fails with
>    `fatal: not a git repository` — it cannot resolve `W:/`-form gitdir
>    links. Run **ALL git commands through PowerShell**. If PowerShell git
>    ALSO fails, the worktree's `.git` file carries an MSYS-form gitdir
>    (`/w/...`) that native git can't resolve either — run
>    `python tools/gdl/provision_worktree.py` FIRST (it repairs the gitdir
>    before anything else); do not hand-diagnose.
> 2. `gdlmem.py` global flags (`--out`, `--root`, `--compact`) go
>    **BEFORE** the subcommand: `gdlmem.py --out r.json brief <tu>`.
> 3. Large `gdlmem.py` results auto-spill to `build/gdlmem_out/` and print
>    the path — Read that file; don't re-run the query.
> 4. Before editing ANY function, check `config/GUNE5D/webfrank.json` for
>    your TUs — a pinned function's source is FROZEN (the postprocessor
>    hash-asserts its body and the build aborts on drift). Screen the
>    pinned list first; do not discover it via a failed ninja.
> 5. Your shell's DEFAULT working directory is the SHARED main checkout,
>    which is read-only to workers — and **`Set-Location` does NOT persist
>    between tool calls** (run 36: a worker's provision script ran in the
>    shared checkout on its very first call despite a prior cd). PREFIX
>    EVERY COMMAND with the cd, in the same call:
>    `Set-Location X; if (-not $?) { throw "cd failed" }; <command>` —
>    configure.py especially regenerates build.ninja into whatever CWD it
>    runs from (a worker regenerated the shared checkout's build graph
>    this way; absolute script paths protect reads, not a script's own
>    output).
> 6a. `... | Select-Object -First N` on a PYTHON pipe makes the command
>    report FAILURE. PowerShell stops the pipeline as soon as it has N
>    objects, python dies on the broken pipe, and `$LASTEXITCODE` reads
>    **-1** (255 to anything reading a byte) for a run that was fine.
>    Reproduce with any repo tool that prints a long list:
>    `python tools/gdl/nearmiss.py --min 90 | Select-Object -First 2;
>    $LASTEXITCODE` -> **-1**; the same command with `-Last 2` -> 0.
>    `-Last` buffers the whole stream and is safe; so is capturing first
>    (`$o = python ...; $o | Select-Object -First 2`). It is also
>    INTERMITTENT — a command that finishes writing before the pipeline
>    stops exits 0 — so one clean observation does not mean you are safe.
>    NEVER read an exit code through a `-First` pipe: that is a gate
>    reporting a failure that did not happen.
>    **THE RULE IS NOW CAPTURE-FORM-ONLY**: never pipe a python tool
>    into `Select-Object` at all. Assign first (`$o = python ...;
>    $ec = $LASTEXITCODE`), then slice `$o`. Two more sightings in run 46,
>    and the second cost a whole diagnosis: `probe.py <unit> <fn> --fuzzy`
>    captured exits 0 and prints `FUZZY (fresh report): 100.0000%`, while
>    the SAME command through `| Select-Object -First 2` exits -1 AND the
>    FUZZY line is gone, because it is printed last. That pair —
>    "255 with no number" — was carried into a work order as a probe
>    defect; probe was fine both times. `-First` truncates the OUTPUT as
>    well as corrupting the exit code, so the evidence for the bug it
>    appears to show is exactly what it eats.
> 6b. `Select-String -Pattern 'A|B' -SimpleMatch` matches NOTHING.
>    `-SimpleMatch` takes the pattern literally, so the alternation is
>    searched for as the six characters `A|B` and the result is an empty
>    set that reads exactly like "no hits, all clear". Measured on the
>    suite-summary screen this file recommends: `'OK|FAILED' -SimpleMatch`
>    = 0 rows, the same pattern as a REGEX = 2 rows, and `'FAILED'
>    -SimpleMatch` = 1. Drop `-SimpleMatch` whenever the pattern contains
>    `|`; keep it only for literals with regex metacharacters in them
>    (`fn_800DACD8(+0x4)`), where it is the right tool.
> 6. Adding a TU's FIRST rule to `config/GUNE5D/webfrank.json` does NOT
>    create its WEBFRANK build edge — a plain `ninja` runs green with the
>    rule silently unapplied (looks exactly like "the rule didn't work").
>    Re-run `python configure.py` after editing webfrank.json, then ninja,
>    and confirm the `WEBFRANK <your TU>` line appears. Also: edit that
>    file with surgical text inserts only — a json.dump round-trip
>    reformats every other lane's rules.

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
python memory_graph/gdlmem.py find [--kind K] [--function F] [--tu T] [--outcome O] [--residual R] [--law L] [--family F] [--capability C] [--query "terms"]
python memory_graph/gdlmem.py search "<terms>"
python memory_graph/gdlmem.py laws [--query <term>] [--tag <tag>] [--full 1]
python memory_graph/gdlmem.py record <id1>,<id2>,...   # batch detail fetch
python memory_graph/gdlmem.py tool <tool-or-workflow>
```

### Residual-first retrieval (run 29)

Search by what the diff LOOKS LIKE, not by function name. A residual you can
describe is a residual someone may already have closed:

```text
gdlmem.py laws --residual "+1 addi -1 li"   # laws + sibling records sharing
                                            # the signature, + webfrank pins
                                            # (QUOTES REQUIRED: unquoted, the
                                            # shell splits it and argparse
                                            # reparses "+1" as a subcommand,
                                            # dumping usage — not a broken
                                            # tool)
gdlmem.py find --family live-zero-remat     # the whole residual family
gdlmem.py find --capability dataflow-equivalence  # which parks a capability
                                            # would unpark = its payoff
gdlmem.py laws --query "live zero remat"    # matches id SLUG WORDS and pin
                                            # `mechanism` prose, not just text
```

`laws --query` indexes record-id slug words (date/version suffixes are not
content — count citations by slug) and the `mechanism` notes on
`config/GUNE5D/webfrank.json` pins, which carry the densest derivation of a
closed residual anywhere in the project. Each law row reports `match` (why it
matched) plus `falsifier`/`asserted_by`.

Three optional record fields, all TOP-LEVEL (`propose-record --template`
prints the shapes; readers also accept an `attributes.` spelling):

- attempt: `residual` = `{signature, family, capability_needed, measured_at}`,
  **top-level only** — `attributes.residual` is legacy free prose on 654
  records and is never read as structure. `signature` is the `fndiff --ops`
  delta VERBATIM (only the `+N`/`-N` mnemonics are indexed, so the framing
  words in the real format do not create false overlaps). `family` comes from
  a controlled 15-term vocabulary plus the sentinels `unclassified` /
  `no-residual`; a typo is refused, because an empty result on a negative
  screen reads as a false all-clear. Naming `capability_needed` is what makes
  a park findable by the lane that could build the capability. Extra
  provenance keys (`confidence`, `extraction_status`, `signature_source`,
  `family_candidate`, `family_candidate_confidence`) are tolerated — the
  schema is additive, and refusing unknown keys once broke the whole corpus
  import when a second lane extended the object.

`find --family` returns **three labelled tiers — never total them**:
`match: family` is the verified classification and the only tier usable as a
screen; `match: family_candidate` appears only with `--include-candidates 1`
and holds extractor guesses measured at ~30-50% precision; `match:
residual_class-fallback` bridges the 941 legacy `residual_class` values the
family is defined against, a coarse *widening* that says "right
neighbourhood", not "this family". Verified hits always rank first.
- claim (law): `falsifier` (what evidence would DISPROVE this, and where) and
  `asserted_by` (tool/test paths that mechanically assert it).
- attempt: `held_fixed` — the variable a multi-edit park held CONSTANT.
- attempt: `reproductions` — a LIST of `{at, alignment, command, result}`,
  one per re-probe of a cap at a NEW alignment. The alignment-sensitivity
  rule above is written for the case where a re-probed negative FLIPS; when
  it REPRODUCES instead, that is repeated evidence and the stronger kind,
  and it had nowhere to live but prose. `alignment` (what MOVED since the
  cap) is required — re-running the same probe on the same tree measures
  nothing. `brief` marks such a row `veto_strength: STRENGTHENED`, so a
  twice-measured veto outranks a same-age veto measured once.

Proposal gates run on **new proposals only**; the full set is A-I in
`memory_graph/core.py` — beyond the three corpus-critical ones below:
dedup-at-propose, windowed-residual word counts (E), banked-evidence
citation resolution (F), `verifiers_run` on postprocessor closures (G),
`addressing_modes_covered` on region-untouched claims (H), and a
register-naming `hypothesis` must cite that register inside a quoted
instruction beside its stream offset (I — the dispatch screen below, moved
to record-authoring time where the misreading starts; ABI-fixed registers
and save-set RANGES are exempt); the gate code is the authority, this list
summarizes. (Accepted records are never
retroactively invalidated; field SHAPE is checked corpus-wide, so in-place
annotation of an accepted record is still validated by `validate`/`build`):

1. A law asserting necessity (must/requires/cannot/only) **requires
   `falsifier`**. An unconditional law with none cannot be screened out by a
   later lane, only re-derived at full cost.
2. A record reclassifying a function **postprocessor-class must quote
   instruction counts as N/N** — a count-asymmetric residual is provably
   outside every postprocessor class, so the count is the deciding fact.
3. A `probed_form` enumerating more than one edit **requires `held_fixed`**.
   Two correct-alone negative parks that each failed to say what they held
   constant jointly hid a 7-function TU flip.

`brief <tu>` leads with **OPEN 10b HYPOTHESES** — rebalanced run 41: a
record's DIAGNOSIS (which words, which registers, which offsets) transfers
near-perfectly and verifying it is mandatory step 1; the record's named
CURE is only the FIRST CANDIDATE, not a mandate (both one-day-old run-40
cures were wrong while both diagnoses were exact; five mandated levers
across runs 38-41 regressed or folded). Execute the diagnosis check, then
treat the lever as one hypothesis among the levers the diagnosis suggests, then
`vetoed_axes` (with `has_probed_form`: false = an unreproducible, weak veto),
`refutations`, and `webfrank_pins` with a `provenance` class against the
Mandatory-policy source-exhaustion bar. **Every number it prints is read from
disk and carries a staleness banner — remeasure before quoting one.**

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

## Exact-match mandate (USER DIRECTIVE, 2026-08-31)

At ~98% project fuzzy, marginal fuzzy gains are no longer the product.
**A matching session's required outcome is EXACT (real 0, byte-identical).**
A park is acceptable ONLY when the residual has been reduced to a single
NAMED mechanism (the exact instructions, registers, and the MWCC-internal
choice behind them) with the complete current law corpus screened and the
screen recorded — "regalloc-ish, 3 tries, park" no longer clears the bar,
and a session whose only result is a fuzzy improvement is an INCOMPLETE
session, not a win. The 3-attempt cap is a PER-AXIS dead-end detector
(two identical A/Bs kill an axis; three probes on ONE cluster with no
movement end THAT cluster) — it is NOT a per-function stopping rule:
PlayerCollidePlayers went exact on axis ten after a first-probe
REGISTER_ONLY classification, so exhaust distinct axes before parking.
Machine-proven REGISTER_ONLY / SCHEDULE mechanisms
are eligible for the WebFrank postprocessor path (hardened guards:
form-aware masks, renaming bisimulation, permutation dependence audit) —
authoring a sound rule that closes such a residual IS full matching.
This supersedes the older timebox-and-accept-regalloc-residuals policy.

A search hit is evidence, not instruction: laws are compiler-scoped
observations. Re-verify against your function's target bytes before applying
one, and supersede the law if your target contradicts it.

### Residual-work discipline (consolidated from runs 17-18, all measured)

1. **Re-derive the mechanism from the aligned view before probing.** A
   record's residual LABEL is usually trustworthy after the regnorm wave;
   its prescribed CURE usually is not (3 of 6 cures on one measured roster
   were wrong). Diagnosis transfers; prescriptions rot.
2. **Census before probing for any residual seen twice.** Three sessions
   probed a mechanism whose population (24, not 4) was never counted; the
   20-minute census closed the class. If the same shape appears in two
   functions, count it image-wide before touching source.
3. **Arbiters by residual class:** slot/frame work -> `slotdiff.py` (real
   actively fights the right answer); recolor claims -> `regnorm.py`
   (STRUCTURAL rows need aligned-view confirmation — positional pairing
   fabricates rows near unpaired insns); staleness -> `gdlmem stale`,
   NEVER fndiff (fndiff scores the POSTPROCESSED object, so every
   webfrank-pinned function reads real 0 by construction); metric
   disagreements -> fuzzy from a FRESH successful report build
   (`probe.py ... --fuzzy` does build+readout in one call; NEVER read
   fuzzy from a report generated mid-sequence by another tool — a
   0.04-off number was nearly recorded that way); gate real > 0 while
   `fndiff --clean` says MATCH -> pool-name noise, the function is
   FINISHED (confirm with --clean before treating a gate row as open);
   wrong-symbol/pool defects -> dump the function's RELOCATION SYMBOLS
   (no score sees them; three real bugs found that way in one session —
   for a named-pool TU, "no @NN remains" is a complete decision
   procedure). **Never compare two pool entries by their FULL byte
   arrays**: dtk names a whole contiguous `.rodata` run with ONE `lbl_ADDR`
   symbol — `lbl_80116BD8` is 0xB4 bytes covering four string literals —
   while our compiler emits each literal as its own `@N` object of 0x28
   bytes, so the two entries legitimately differ in LENGTH at the same
   datum. The relocation points at the START of both, so the decidable
   question is whether the shorter is a PREFIX of the longer. Measured
   (attempt.T11_tool-queue-11-ten-items-with-three-calibration-narrowings
   .20260903.v1): full-array comparison called 337 rows in 123 functions
   wrong, including byte-identical functions inside the 100%-matched SDK
   (DEMOInit::LoadMemInfo at real 0); the prefix comparison `fndiff` now
   ships (`_datum_prefix_equal`) takes it to 177 rows in 58 functions.
4. **Free evidence first:** the TU's own header comments (one carried the
   correct diagnosis two passes missed); the target's function address
   order (= source order); callee prototypes; `nm`/UND tables for link
   claims. Read these before any build is spent.
5. **A refuted premise is a deliverable.** When a lane's stated goal
   evaporates under measurement, convert it into the refutation record
   plus whatever the evidence actually supports — never execute a
   premise you have measured false.
6. **Two-variable axes need joint probes.** Two correct-alone negative
   parks jointly hid a 7-function TU flip (btricol: extern-ghost +
   volatile-scaffold were ONE lever). When two parked axes touch the
   same instructions, one joint probe is mandatory before treating the
   pair as closed; a park that held another variable fixed must SAY so.
7. **Shell file-writing:** never write a source file from PowerShell
   (`Out-File`/`Set-Content -Encoding utf8` inject a BOM that MWCC
   rejects as Shift-JIS; `git show HEAD:x > file` corrupts too). Use
   the Write/Edit tools; for byte-level ops use [System.IO.File] — but
   pass it ABSOLUTE paths on BOTH sides (source AND destination):
   .NET resolves relative paths against the process working directory,
   NOT PowerShell's `$PWD`, and the two diverge in this harness, so a
   relative `[System.IO.File]::Copy('a','b')` reads or writes the wrong
   directory silently (measured twice). `Resolve-Path`/`Join-Path` the
   arguments first, or spell out `W:\...` literals.
   `git stash` is UNUSABLE here (shared stash, 31+ foreign entries) —
   A/B via a scratch copy or `probe --discard`. `Copy-Item` PRESERVES
   LastWriteTime, so restoring a file by copy can leave ninja thinking
   nothing changed and serve you a STALE measurement as if live —
   touch the timestamp after any copy-restore. Under a concurrent
   fleet, re-run a failed link ONCE before diagnosing (a transient
   exception-fixup-stamp race produced a false link failure).
8. **Write records and commit messages FROM tool output, never before
   it.** Two workers drafted metrics into records/messages before the
   measuring build ran; one shipped fabricated slotdiff numbers. Run
   the tool, paste from its output, then write. Corollary: REMEASURE
   is the default — every number quoted from a brief or record is
   stale until a live tool run confirms it.
9. **Order of operations for a TU roster or any IMPROVED bank:**
   (a) regnorm census FIRST across the whole roster — `real` INVERTS
   tractability (a real-232 function was 1 word from a pure recolor
   while the real-30 "cheap" one was a schedule rewrite); (b)
   `gdlmem laws --query` on the residual signature BEFORE the first
   probe (an existing law predicted a win a worker nearly re-derived);
   (c) a parity-held real improvement (counts equal and unchanged) is
   arbitrated on FRESH fuzzy BEFORE `--update-improved` — probe+gate
   both passed a real 30->24 that was a fuzzy 81->72 regression.
10. **Sweep/roster hygiene:** screen `config/GUNE5D/webfrank.json`
   pins before ranking anything by measured real (pinned functions
   read real 0 by construction); "no source change retained" in a
   record does NOT mean never-probed — probes may have run and been
   reverted, and the corpus predates the `probed_form` field, so
   absence of the field is not absence of probing. Outcomes must say
   which: `measured-dead` (probed, axis vetoed) vs `never-attempted`.
   Any "closing X flips the file" claim must carry a live
   `datadiff --sections` result, not an assumption.
10b. **A record ending in a concrete untried hypothesis makes that
   hypothesis MANDATORY STEP 1** for the next lane on the function —
   ranked above fresh analysis. One such hypothesis, written down and
   then skipped by its own author, was worth −235 real in a single
   build when finally executed a run later. Remeasure the record's
   NEGATIVE findings too, not just its cure: a "nets to zero" claim
   hid the exact block where the next win lived.
11. **Per-function step zero: `gdlmem context <function>` BEFORE the
   first edit.** `laws --query` (discipline 9b) is NOT a substitute —
   it finds laws by signature, not this function's own attempt
   history. A sweep lane spent 5 of 11 probes re-running axes whose
   caps and vetoes were sitting in per-function records the whole
   time, because a roster's "never-probed" label was trusted over the
   function's context. Roster/label claims about a function are
   remeasured like any other number.
12. **Two hashes, two meanings.** The build gate is `ninja` printing
   `build/GUNE5D/main.dol: OK` — dtk verifying against
   `config/GUNE5D/build.sha1` (540bed0b...). The `7cba77aa...` sha1 is
   the ORIGINAL retail DOL in `orig/` (what provision verifies).
   Hashing the built DOL and comparing to 7cba77aa yields a FALSE
   failure — state gates as commands ("ninja prints main.dol: OK"),
   never as raw hashes.
13. **Running the test suite:** `python -m unittest discover
   tools/gdl/tests -b` from the repo root. There is no pytest. Module
   counts drift — state test gates as the command plus "all green",
   not as a number.
   **`-b` IS PART OF THE COMMAND.** Without it the suite's stdout is
   the tools' own chatter from PASSING tests, and the verdict is not
   in it: measured run 44 on a green run, the whole stdout stream was
   12 lines — 10 `WEBFRANK <fn>: ...` rule reports, one
   `[arbitration log NOT written to ...]` and one `[transient pin bank
   CONSUMED ...]` — while `Ran N tests` and `OK` go to STDERR, so a
   PowerShell `2>&1` puts the verdict ABOVE the noise and a tail reads
   as a webfrank failure. That is where the `-SimpleMatch` trap (trap
   6b) bites: lanes grep for the verdict and the pattern they reach
   for contains `|`. `-b` is not a mute switch — unittest BUFFERS
   per-test output and REPLAYS it under a `Stdout:` header for any
   test that fails (verified both ways: a passing test's print is
   swallowed, a failing test's is replayed), so it is strictly better
   than routing tool prints to stderr or adding a `--quiet`, both of
   which lose the text exactly when it is wanted. `python -m
   memory_graph.test_graph -b` takes it too. For the memory graph, `gdlmem build` is the
   write-path gate (~30s) and `gdlmem validate` is the whole-corpus
   check — it now completes in under a second (the old "never block
   on it" advice described a quadratic bug, fixed run 33 at 3,400x).
   validate reports dangling citations from pruned records as DEBT,
   not failure; staging stays strict. The memory_graph suite
   (`python -m memory_graph.test_graph`) runs 66-88s vs 3-4s for
   tools/gdl (measured run 34) — between items of a multi-item lane,
   run only the test class your change touches; the FULL suite is
   required once per commit, not once per edit. (Run-36 note: the
   suite now runs ~17s, but editing any `tools/gdl/*.py` invalidates
   the graph DB fingerprint, so the NEXT graph-suite run pays a ~19s
   rebuild — a timing swing, not flakiness.) **`python -m
   memory_graph.test_graph --changed` decides that per-commit run from
   the paths git reports** (`--since <ref>` compares a range instead of
   the working tree): it runs the full suite when anything under
   `memory_graph/`, `tools/gdl/`, `config/GUNE5D/` or
   `research/xbox_symbols/` changed and skips otherwise, printing both
   sides of the comparison. Those four roots are the MEASURED input set
   (instrumented run: 2,164 repo paths read, nothing under `src/`,
   `include/`, `configure.py` or `AGENTS.md`) — the intuitive "did I
   touch a memory_graph file?" reading is NOT the discriminant and would
   have wrongly skipped 17 of the last 60 commits, because every
   `tools/gdl` source is a graph build input and `config/GUNE5D/
   webfrank.json` feeds law/pin queries. Over those 60 commits the
   sound gate skips 30, worth ~20 minutes.
14. **A guard's refusal is a measurement of the guard, not only of the
   function.** Two coarse guards each refused a provable function
   while failing correctly by their own logic (blanket relocation
   distrust; pre-recolor-only permutation), and a
   verify_consistent_recolor refusal was twice read as a fact about
   the function when it was a fact about the checker. Before recording
   a refusal as a cap, instrument it: WHAT check fired, on WHICH
   word, and would a sound-but-finer check pass? Corollary of
   discipline 1 for guards instead of cures.
15. **Pass anything non-trivial to a shell via a FILE, never argv.**
   Hard rules, each measured more than once: **NO `python -c` AT ALL**
   — the old "none containing a newline" phrasing invited a judgment
   call that failed twice more (run 36: a worker who KNEW the trap
   reached for a 5-line `-c` anyway and got the cmd-shim `goto :error`
   injection); write a scratch script, every time. Commit messages
   ALWAYS via `git commit -F <file>` (the first `->` arrow breaks
   argv); PowerShell also mangles `%` format strings and backtick
   escapes, and pipes into python inject a BOM. Write a scratch
   script / message file and run it. A gate or parity check must
   PRINT the values it compared — one passed by comparing two empty
   dicts and printed OK.
15b. **Pin scope after the name-bound hash migration:** a permutation
   pin no longer freezes its TU against symbol-COUNT changes (indices
   are not hashed). It STILL invalidates — correctly — on edits that
   renumber the anonymous pool (@NNNN names change) or alter a window
   relocation. The pin screen question remains "does my edit change
   the TU's anonymous-pool population or a pinned window's relocs?";
   if yes, re-derive with
   `tools/gdl/composed_census/wf_rederive_pin.py` (FULL path — one lane
   reported the tool "does not exist" after checking only tools/gdl/;
   body hashes must return byte-identical, that IS the audit). ALSO
   position-sensitivity, measured: an instruction-COUNT change in any
   function PRECEDING a pinned function in source order can shift the
   pinned window's relocation addends and abort the build. That is NOT
   a reason to park the upstream function — make the edit, re-derive
   the downstream pin, and verify its body hashes held. The upstream
   freeze is a re-derivation chore, not a wall. THIRD abort cause
   (run 41, measured): editing the BODY of a shared static inline
   helper changes the body hash of every pinned function that INLINES
   it — the abort names the pinned inliner, not your helper. And a pool
   renumber is not a wall either (run 40: one crossed five pinned
   siblings and every name-bound pin replayed).
16. **Prototype/extern disagreements: run `tools/gdl/abicheck.py`,
   not positional comparison.** PPC EABI assigns GPR and FPR args as
   independent sequences, so most positional "conflicts" are
   register-identical and faithful; the corrupting direction (the
   definition reads a register no caller writes) numbered THREE rows
   image-wide when last measured. externcheck ranks type-class
   disagreements but cannot model this — screen through abicheck
   before queueing any prototype work.
17. **Lane scratch directories stay UNTRACKED.** Never `git add` your
   `XX_scratch/` — committed scratch rides merges onto main's repo
   root and has had to be relocated three times. A script worth
   keeping is promoted deliberately: move it under
   `tools/gdl/composed_census/` (lane-prefixed filename, repo-root-
   relative paths) in its own commit, and say so in your report.
   A promoted script must be RUN ONCE from the repo root before the
   promoting commit lands — one promoted census tool was unrunnable
   for two runs because nobody executed it after the move, while its
   law records were still being cited.
   Record DRAFTS go through `gdlmem propose-record`, not into git.

   **Every scratch and generated filename is LANE-PREFIXED, and no tool
   hardcodes another lane's scratch path.** Three rules, each measured:

   (a) *Prefix the basename, not just the directory.* `XX_scratch/`
   isolates nothing once a file is promoted or a path is hardcoded, and
   the basename is what collides. Census 2026-09-01 over
   `tools/gdl/composed_census/`: 97 promoted files, 70 lane-prefixed
   (ch, cn, cs, cv, eh, gw, ha, hv, pw, wf, ws) and **27 generic** —
   `rule.json`, `rec1.json`…`rec6.json`, `rec_law.json`,
   `rec_roster.json`, `readlaws.py`, `poolrefs.py`. Those are names a
   second lane picks independently, in a directory every lane shares.
   `wf_mkrule.py` writes plain `rule.json` beside itself; the next lane
   to write a rule draft overwrites it silently.

   (b) *Never hardcode a foreign lane's scratch directory.* Measured
   collision: `pb_window_rules.json` is written by
   `composed_census/build_rule_pw.py` into `PW_scratch/` while
   `tools/gdl/splice_rules.py` reads it from `WF_scratch/` — one
   basename, two lane directories, and neither path exists in a third
   lane's worktree. `tools/gdl/build_rule.py` carried the same
   `WF_scratch/` hardcode and hard-crashed with `FileNotFoundError` in
   every checkout but its author's (fixed run 31: output is `--out=PATH`
   with a `build/GUNE5D/` default).

   (c) *Generated artifacts go under `build/`, never beside the script.*
   `os.path.join(HERE, "x.json")` inside `tools/gdl/composed_census/`
   writes an untracked artifact into a TRACKED directory — 31 such JSONs
   are already committed there. A tool that emits a roster or a report
   takes an `--out` argument defaulting under `build/GUNE5D/`; that
   directory is already gitignored and per-worktree, so two lanes running
   the same tool cannot overwrite each other at all.

Header edits (include/game/*.h): allowed ONLY to the lane whose work_claim
names it as that header's owner this run — one owner per header per run.
The owner follows the isolation protocol without exception: exact byte
accounting for every pad split, scalars/arrays-of-scalars by default
(array-of-STRUCT members need their own dedicated isolation pass per the
embedded-cascade law), gate the header edit ALONE across the heavy
includers before any consumer edit, and finish with a full ninja
main.dol: OK. Evidence bar is unchanged: PDB + GC access patterns +
sibling consumers; never invent names.

## Mandatory result policy

- Use portable C/C++ only. Never add inline assembly, function-level assembly,
  raw PPC instruction words, embedded machine code, binary inclusion, or any
  other percentage-gaming mechanism.
- The sole post-compile exception is the existing fail-closed Frank/WebFrank/
  P6Frank harness, used exactly within the constraints returned by
  `gdlmem.py tool <name>`. Never weaken a guard, add an unaudited rule, or use
  postprocessing to hide structural, operand, relocation-payload, ABI,
  semantic, or data differences.
- **A new rule additionally requires SOURCE-EXHAUSTION provenance**: the
  function must carry a parked/capped attempt record with literal
  `probed_form` axes (or a law proving its residual class source-
  unreachable), and the rule's attempt record must cite it. Mechanical
  closability alone is not sufficient — a provenance audit found 11 rules
  authored without any source-work trail, which is the "wanton use"
  failure mode. Functions with no such record get a source-first pass
  BEFORE any rule.
- **The rule-authoring path, as commands.** Run 44: WS found
  `webfrank_audit.py` by listing `tools/gdl/`, because nothing named it —
  the tool that decides ELIGIBILITY was the one step with no pointer.

  ```text
  python tools/gdl/webfrank_audit.py [--min-insns N] [--grep <prefix>]
      # ELIGIBILITY, no build and no config written: proves identical
      # instruction/relocation shape, then that every raw difference sits
      # in one of PowerPC's four five-bit register slots. Emits the
      # register-field edits plus whole-function hashes as reviewable JSON.
      # Its SILENCE is not a verdict of ineligibility
      # (claim.law.RQ_webfrank-audit-silence-is-not-ineligibility).
  python tools/gdl/composed_census/wr_try_rule.py <unit> <fn> <fragment.json>
      # RUN a hand-written rule body through the real apply_patch and its
      # guards: BYTE-EQUAL / APPLIED-NOT-EQUAL / the guard's verbatim
      # refusal. It passes the retail DOL, so the datum screen runs at L3
      # rather than degrading to the pool correspondence, and REFUSES when
      # the image is missing.
  python tools/gdl/composed_census/wf_rederive_pin.py <unit> <fn> --apply
      # re-derive a pin whose window relocations moved (FULL path; body
      # hashes must return byte-identical — that IS the audit).
  python tools/gdl/composed_census/t16_rederive_body.py <unit> <fn> --apply
      # re-derive a pin whose BODY hash moved — the case the tool above
      # refuses. NEVER paste the hash out of webfrank's abort message
      # ("input hash X != expected Y"): before_sha256 hashes OUR body, so
      # pasting it re-blesses the new codegen without re-running a single
      # guard. This derives every slot fresh, re-runs the rule through the
      # real apply_patch with the retail image, and pastes ONLY on
      # BYTE-EQUAL; APPLIED-NOT-EQUAL means rule_derive.py, not a hash. A
      # moved after_sha256 (the TARGET's body) is always a refusal — the
      # rule is bound to a different symbol now.
  python configure.py    # a TU's FIRST rule has no WEBFRANK build edge
                         # until this runs (first-five-minutes trap 6)
  ```
- **Commissioning a postprocessor CAPABILITY requires a DEMAND CENSUS
  FIRST** (WF, run 34: a sound, correctly-guarded capability was built and
  shipped with ZERO customers — no parked function in the corpus was
  waiting on it). Soundness is necessary and not sufficient; a capability
  with no customer is a guard surface to maintain forever in exchange for
  nothing. Before writing one, COUNT the functions it would unpark and
  name them in the proposing record:

  ```text
  gdlmem.py find --capability <name>       # the parks that named this
                                           # capability_needed = its payoff
  gdlmem.py laws --residual "<signature>"  # who else has this residual
  gdlmem.py find --residual <signature> --outcome parked
  ```

  Report the census as a NUMBER with the record ids, and treat a count of
  zero as a refutation of the premise, not as a reason to build it and
  hope (discipline 5: a refuted premise is a deliverable). If the demand
  is real but unrecorded, the fix is to record the parks first — a
  capability justified only by the builder's belief that someone will
  want it is the "wanton use" failure mode in a different costume.
  Corollary of the source-exhaustion rule above, at the class level
  rather than the rule level.
- **Class ceiling**: every postprocessor class must be attributable to
  allocator/scheduler variance under a proven compiler. The relational
  value-equality mode is the outer boundary — no class may cross into
  "any semantically equivalent stream". Proposals for new classes go to
  the integrator as records, never shipped unilaterally.
- Progress reporting always publishes the STRICT/EQUIVALENT split; never
  quote the combined matched% alone in a record or report.
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
- `REFUTED`: the target's standing premise (a recorded diagnosis, a law's
  prescription, a brief's claim) was measured false this session. Zero score
  movement with a refutation is a DELIVERABLE, not a failure — record what
  the evidence actually supports, with `refutes`/`supersedes` citations, so
  `stale`/reopen tooling can distinguish it from an ordinary dead end.
- `RECLASSIFIED`: the function moved between work classes (e.g. source-class
  to postprocessor-class, or a park family to a served rule class) without a
  score change. Name the old and new class and what reassignment it implies.
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
- **Measure fresh fuzzy BEFORE banking any keep, even when real and multiset
  agree** (run 35, measured): probe banked an "IMPROVED" state whose fresh
  fuzzy was a 0.46 regression, and the next probe — measured on that
  poisoned base — read the run's best edit as a loss (re-applied from the
  last commit it was +0.33). Corollary: re-run any negative verdict from
  the last COMMITTED state before recording it. `probe --arbitrate` prints
  the (real, fuzzy) pair for both states in one call.
- **A windowed residual claim requires a raw differing-word count**
  (`tools/gdl/composed_census/wf_word_diff.py`): run 35 found a recorded
  "4-word residual" was 122 words — `--ops` clusters only where the opcode
  stream diverges and is blind to pure register-field words. The word
  count, not the `--ops` cluster count, decides postprocessor candidacy.
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
   evidence noted. Never invent names. Proven authority moves in
   priority order: (a0) FIRST grep `include/game/*.h` for an existing
   project struct of the same size and field shape — two of critter.c's
   multi-session "unresolved blockers" (MBObject, WorldObj) were fully
   documented project headers all along, and select.c coined a fourth
   name for a record mb_blit.c already typed; a lookup failure is not a
   knowledge gap; (a) an opaque forward-declared struct with a known
   size deserves a `struct <name>` PDB-body lookup BEFORE per-site work —
   completing the body converts whole loops at once; (b) the tsv index is
   regenerated from the FULL PDB dump (1,958 records incl. `__unnamed_<Id>`
   anonymous ones) since 2026-08-31 — a `struct` miss now usually means the
   record really isn't in the PDB; grep `research/xbox_symbols/*.h` only as
   a last-resort double check, and rerun `tools/gdl/gen_xbox_structs.py` +
   `gdlmem.py build` if the dump headers ever change; (c) before inventing
   ANY file-local view,
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
python tools/gdl/probe.py <unit> <fn> [--ops | --revert]  # MATCHING loop: build+score+verdict, one call
python tools/gdl/defake_gate.py check <unit> --rebuild  # DEFAKE loop: build+gate, one call
python tools/gdl/defake_gate.py baseline <unit> --at-head
    # THE ANCHORED BASELINE. Gate baselines are keyed to the COMMIT, not
    # committed to git, so a lane that finds none (or finds one taken at a
    # different HEAD) does not have to re-take it by hand off whatever the
    # working tree happens to hold: --at-head sets working-tree edits aside,
    # rebuilds the baseline THIS COMMIT implies, and restores them. It
    # prints the commit and source sha1 it anchored to, and that line is
    # what a record quotes (attempt.NM_init-enemy-setenemyobj-takes-three-
    # args-and-closes-exact.20260903.v1 quotes exactly it). The flag existed
    # since run 37 and was reachable only from defake_gate's own docstring.
python tools/gdl/fnasm.py <unit> <fn> [0xA:0xB | i:j] [--ours | --diff]
python tools/gdl/fndiff.py <unit> <function> --count | --ops | --clean
python tools/gdl/savedregs.py <unit> <fn> [--uses]  # callee-saved
    # correspondence, BOTH streams, zero builds: which local lands in which
    # saved register. `--ops` compares opcode MULTISETS and is blind to a
    # save-register permutation (MV spent three lanes on one that reads
    # "multiset: IDENTICAL"); this prints the assignment and reports
    # emission ORDER separately, because they are different questions.
python tools/gdl/defake_rewrite.py <file> --base X --type T --map off=field,...
python tools/gdl/fuzzy.py <unit> [<fn>]        # fuzzy from last report, no regen
python tools/gdl/xrefnum.py <const...> [--cast-only]  # who else uses this offset
python tools/gdl/externcheck.py                # cross-TU extern type conflicts
python tools/gdl/aritycheck.py [--verdict PHANTOM-CANDIDATE]  # parameter
    # COUNT: which definitions declare a trailing parameter the body never
    # reads, and which call sites pass fewer arguments than declared.
    # externcheck ranks type CLASSES and abicheck models GPR/FPR sequence
    # assignment (discipline 16) — neither reads count, which is why
    # init_enemy's phantom 4th argument had to be found by hand. Zero
    # builds. THE CENSUS IS THE COMMAND, NOT A NUMBER: run it (its second
    # line is the per-verdict tally) and quote THAT, because the population
    # moves as its own rows are worked — 4/15/42 at run 42 became 1/14/42
    # once the arity work landed, and both figures have been quoted into
    # orders as if current. Stability is not the point either: measured at
    # 0fd3bca5a the tally still reads 1/14/42, i.e. unchanged since
    # ca4074cb1 — a live run is the only way to know which of those two
    # things is true today. A verdict is a place to look: both
    # governing laws (NM_an-unread-trailing-parameter..., knr-extern-arity-
    # can-be-faithful-not-a-defect) are settled against the TARGET BYTES at
    # the call site. Every site now prints its CALLER, a WEBFRANK-PIN
    # marker (a pinned caller reads real 0 by construction, so its aligned
    # view cannot decide the row — read the target half) and a SHORT/FULL
    # split (only FULL sites pay for a phantom parameter, so a row with
    # 0 FULL sites has no payer).
python tools/gdl/matchtool.py probe <unit> --brief
python tools/gdl/lowmatch.py --max 50 --min-size 200 --sort impact
python configure.py progress
```

Loop discipline: the edit loop is ONE command now — `probe.py` (matching:
prints BASELINE/IMPROVED/REGRESSED/NEUTRAL/CONFLICT against a remembered
best, with the opcode-multiset token count on every verdict; CONFLICT =
real regressed but structure improved — arbitrate, never auto-revert.
Every BASELINE/IMPROVED/NEUTRAL banks a TU-source snapshot and `--revert`
restores it AND re-scores in the same call — never hand-retype a revert;
to DISCARD a neutral edit you dislike, use git, since neutral states
bank too; probe a BASELINE before your first edit so the revert point
exists.
FIRST-BASELINE TRAP: probe banks whatever state it FIRST sees per
function — an edit made before a function's first probe gets banked as
its "baseline" and --revert then restores the BAD state; run a
clean-state probe on every function you touch, including secondaries,
BEFORE its first edit) or
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
`missing: build/...` form). Since run 43 the `composed_census/` family
accepts the same spellings as the core tools (`game/x/y`, `game/x/y.c`,
`src/game/x/y.c`, backslashes) — `fndiff.unit_key` is the one normalizer
and the census tools route through it. Before that, 16 of the 18 census
tools that take a unit built `build/GUNE5D/obj/{unit}.o` from raw argv, so
a `.c` spelling produced `...y.c.o` and a MISSING OBJECT, which reads as
"this function is not in the census" rather than as a spelling.

**IMPORTABLE CORE (run-43 item 10).** A tool whose module docstring
carries a line beginning `IMPORTABLE CORE:` names functions you may call
IN-PROCESS: they are pure over parsed data, they never build, and
importing the module has no side effects. Use them for any sweep — a
per-function subprocess is the wrong shape when two object parses would
do (`fndiff`, `slotdiff`, `savedregs`, `defake_gate`, `nearmiss` carry
the line today; `tools/gdl/tests/test_importable_core.py` fails if a
marked module stops importing silently or renames a function it
advertises). Measured over all 62 tools/gdl modules: 51 import silently,
9 do work at import (abicheck, build_rule and the addr16/addrlo/
add_remat census family) and 2 fail outright (pdb20_dump, splice_rules
each open a file at import) — those 11 are library-hostile and are the
debt this convention makes visible.

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
to the user or another worker. **`git branch --show-current` printing nothing
means DETACHED HEAD — fix it before any commit or merge**: a host reboot left
the shared checkout detached once (run 39) and six merges landed off-branch,
recovered only because the chain was fast-forwardable.

- Never use destructive cleanup (`git reset --hard`, `git checkout -- <file>`)
  on shared work, and never rewrite another worker's uncommitted changes.
- Stage only files you own and have audited; do not fold unrelated changes
  into a matching commit.
- Do not push unless you are the integrator and the user has authorized it.
- Never commit machine-local paths, credentials, or personal environment
  details, and never commit the generated memory database. If private content
  lands in history, tell the user immediately rather than papering over it.
- Inspect diffs with `git diff --numstat` or `--name-status`, not
  `--stat`: with this repo's line-ending config, `--stat` emits a
  LF-to-CRLF warning per tracked file (~317KB of noise before any
  content, measured run 34) and buries the signal.

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
- **Every work_claim carries `attributes.owned_units`** (run 46): a LIST of
  repo-relative unit paths or directory prefixes
  (`["game/ps2/ml_fmath.c", "tools/gdl"]`; any spelling the tools accept is
  normalized on read). The scope PROSE is for the worker; this list is for
  the tools, and it is the only channel they screen. Measured over all 250
  `src/` units against run-46's six claims, the prose substring screen fires
  on 20 units and **17 of those (85%) are units no scope names** —
  `game/sys/main.c` matches five of six claims on the word "main" (from the
  `main.dol: OK` gate line) and `game/ui/select.c` matches the tool lane on
  "Select" (from `Select-Object`) — and it cannot read a negation, so a lane
  that names another lane's TUs *in order to exclude them* is reported as
  their co-owner. `probe.py` and `defake_gate.py` refuse (exit 3) when the
  cwd worktree's lane edits a unit another ACTIVE claim lists; the lane id
  comes from `LANE_LOCK`'s first line, then `$GDL_LANE`, then the branch.
  `--ignore-claim` / `GDL_CLAIM_OVERRIDE=1` is the integrator's escape;
  `GDL_CLAIM_SCREEN=off` disables it entirely. A claim with NO list makes
  every unit **undecidable, never free** — the tools warn and proceed, so an
  order that omits the list silently disarms the protection for the whole
  fleet. `python tools/gdl/claimscope.py --index` prints the live unit→owner
  map and any two-lane conflicts; `--self` prints the detected lane id.
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

Single-writer enforcement (run 34, measured): two fleets dispatched twin
workers onto the same worktrees and branches, and three lanes collided.
Measured costs: one complete redundant implementation written and then
removed (T4); ~15 reconciliation tool calls plus ~4 wasted builds chasing
edits a twin kept reverting (PC); one false record written, committed, and
superseded; roughly doubled wall-clock fleet-wide from doubled build
contention on one machine. Rules:

- **One integrator per repository at a time.** Before dispatching a fleet,
  the integrator checks for live peer sessions (`ListAgents` or platform
  equivalent) and for foreign `state: active` work_claims whose
  `attributes.integrator` is someone else. Either finding: coordinate with
  the user before spawning anything. Two integrators merging one trunk is
  never acceptable, and competing git operations in the shared checkout
  corrupt each other's index mid-merge (observed run-34 closeout).
- **Lane lock.** A worker's first action inside its worktree is writing an
  untracked `LANE_LOCK` file at the worktree root with its worker id and a
  random nonce. If the file already exists with a different id: do not
  work — report the collision and stop. Re-read it before every commit; a
  changed nonce means a second writer is present — commit nothing, report,
  stop. Remember the PC law: a lost edit fails nothing — every green gate
  describes HEAD, so the commit, not the gate, is the first evidence an
  edit exists.
- **Author records in lane scratch, never directly in the inbox.**
  `propose-record` refuses duplicate ids, but a direct file write into
  `memory_graph/inbox/` bypasses that guard entirely (measured: a twin's
  detailed record was silently overwritten by a same-id write). Write the
  JSON in your scratch directory and let `propose-record` place it.

Dispatch screens (run 35: 2 of 6 lanes were sent to already-complete
regions, and a third inherited a residual claim that was 30x wrong —
each screen below costs one command and would have caught its lane):

- **Fresh-TU work orders**: the integrator runs
  `tools/gdl/composed_census/mt_region_census.py` over the target prefix at
  DISPATCH time and quotes the target TUs' current `Object()` states from
  configure.py in the order. An `Object(Matching, ...)` line means the TU
  is done and the lane is over before it starts.
- **Citations resolve to record ids.** "Banked in the graph" prose is not
  a citation; `gdlmem search` must return the record, and the order names
  its id. (A memory-index summary can advertise a plan its own note body
  records as superseded — dispatch reads indexes.)
- **ANY brief inheriting a residual signature** — not only a
  postprocessor-lane one: quote the raw differing-word count next to the
  inherited description. Run 38: a SOURCE-lane brief skipped this because
  the rule named postprocessor lanes, and its "25-word permutation"
  framing died on contact with the real count. `brief` now attaches
  `current_differing_words` to every `vetoed_axes` row that quotes a
  signature (and to every roster row), so the screen costs nothing and
  cannot be skipped by whoever writes the brief; `wf_word_diff.py <unit>
  <fn>` remains the one-function form. The RAW count decides
  postprocessor candidacy — `unabsorbed` is the strictly smaller
  register-field-stage number (do_exit: 18 raw, 8 unabsorbed) and is not
  a substitute for it.
- **Worker step 0 is `python tools/gdl/provision_worktree.py`** from the
  worker's own worktree — a fresh worktree lacks build.ninja/orig/build
  and the failure mode is a confusing smoke-test error minutes in.
- **Multi-item lanes commit each item BEFORE starting the next.** Three
  items interleaved in one file cost 9 tool calls and two full suite runs
  to disentangle (git stash is banned here and `git add -p` is unusable
  non-interactively).
- **Close-lane rosters get one `fndiff --clean` per DIFF row at dispatch**
  — run 36: 4 of ~58 assigned functions were already finished (pool-name
  noise reads as DIFF in every census), and the lane's "matched" counts
  disagreed with `defake_gate baseline`'s. Rank candidate functions by
  records-per-unmatched-function, not fuzzy — the nearest-by-fuzzy TU held
  5 thorough caps while the genuinely unexplored functions ranked last.
- **A tool-queue item's brief must include the one-command symptom
  reproduction** — run 36: an item's stated symptom no longer existed and
  masked a defect 15x larger. Run 37 re-measured the rule's value: 3 of 11
  briefs contradicted their stated cause in three different ways.
- **A brief naming a specific register must quote that register's
  definition site** — run 37: a mandated hypothesis identified the wrong
  register (a defs census refutable in zero builds cost two).
- **One `gdlmem context` per function the order NAMES** (not just the
  roster) — records land mid-run; run 37 dispatched "no record at all"
  about a function whose record had been accepted the same day.
- **One `gdlmem claims` cross-check per named calibration target** —
  run 37 nominated a calibration case inside another live lane's claimed
  TU.
- **A work order quoting a `real` delta must name the arbiter that decides
  the keep** — run 37: a `real`-denominated bar was unsatisfiable against
  a compensating-error baseline while fuzzy (the actual arbiter) said keep.
- **A record asserting "X is the blocker" must quote X's count in BOTH
  streams** — run 37: a named blocker existed identically in both.
- **Capability briefs quote the two differing WORDS, never the family
  label** — run 37: built to its label, the lane would have duplicated a
  shipped mode and still missed both real customers; pair the word count
  with the aligned `fnasm --diff` view (offset-pairing lies across
  permutations).
- **Any lane editing `webfrank.py` must run the forced full-replay gate**:
  touch `webfrank.py`, full ninja, confirm every shipped rule replays and
  the DOL is OK — the only proof that "every existing rule replays," one
  build, previously undocumented.
- **Quote a record's residual decomposition VERBATIM, never paraphrased** —
  six paraphrase-vs-record divergences in three runs; the record is the
  authority and the order is a pointer to it.
- **"Measured run N" in a brief or queue item must name the record id** —
  run 39: an item's cited measurement had no record behind it; items citing
  ids shipped verbatim, prose rotted.
- **A capability census asks UNPARK PAYOFF, not site population** — the two
  came apart 2-vs-0 on the first class where both were measured; a
  downstream "close X" step is CONDITIONAL on the census gate above it, and
  the order must say so.
- **Grep for already-applied source levers before dispatching one** — a
  banked pragma lever was already in the tree bracketing its function
  (run 39); one grep at dispatch.
- **A published register-correspondence table must state the byte range it
  covers, and an order may not quote one covering less than the whole
  function** — a table from the first 0xe0 of a 0x330 body was wrong three
  ways and steered two lanes.
- **Calibrate every new gate/classifier against the live corpus before
  shipping it** (hard rule): a run-39 gate would have shipped at a 76%
  false-positive rate; the census cost ~2 minutes. Run 41: calibration
  changed four tool-queue items, and a prose screen keyed on record
  BODIES was defeated by record IDS (every claim.law.* contains "claim")
  — calibrate against ids too.
- **Calibration is TWO-SIDED: count the POSITIVES and the NEGATIVES, and
  report both numbers.** A trigger measured only on the cases it should
  catch is half a measurement, and the missing half decides whether the
  thing ships as a REFUSAL or as an advisory. Both halves paid in run 44:
  (a) the slot-arbiter gate's positives looked clean until the negative
  side was counted — 92 anchored corpus records fire its trigger and 40
  (43%) quote no arbiter, 16 of 58 (28%) among records since 2026-09-01,
  so it shipped ADVISORY rather than refusing a third of the corpus's slot
  work; (b) that same gate's first EVIDENCE predicate looked only for the
  literal tool name and scored eight records as violations that quote the
  tool's OUTPUT verbatim — a false-positive class visible only by reading
  the would-be-refused set, not its size. Two intermediate trigger drafts
  were also killed by the negative side alone (a bare `frame N` mention
  took the population 92 -> 208 and the miss rate to 72%; `save set`
  belongs to `savedregs.py` and accounted for 20 of one draft's 67 hits).
  Name the exclusions in the shipped code with the number each removed.
- **An order proposing a LEVER must cite the last sweep of that lever**
  — run 41: a "virgin territory" sweep premise was governed by seven
  records from three days earlier that one search would have surfaced.
- **An order naming a postprocessor class carries the two differing
  words AND the decode verdict** — run 41: a class line inherited from a
  family label was wrong for both customers, refutable in zero builds.
- **Queue items state the OBSERVATION, never the cure AND never the
  cause** — run 41: three of ten items stated cures that smuggled
  untested discriminants; run 42: two stated causes were both wrong
  (one had ZERO live firings while the unnamed cause drove 29 of 33).
  A number in a brief needs a record id exactly like a "measured run N".
- **A queue derived from a positional census ships with a datum-multiset
  column and each row's `real`** — run 42: 22 of 30 rows were order
  artifacts ("a list of candidates advertised as a list of bugs"), and
  the row-alignment trustworthiness was decided by `real` values the
  order never quoted.
- **Cite the TOOL SURFACES a record shipped, not only the record id** —
  run 42: a lane rebuilt a value-comparison layer that already existed
  inside fndiff because the order cited the records but not the code.
- **Never write files the toolchain parses with PowerShell redirection
  or Set-Content** (widened from source files): `-Encoding utf8` injects
  a BOM that json.load rejects. Python scripts write records.
- **A failed PowerShell git commit can leave a stale `index.lock`** in
  the worktree gitdir — on "Another git process seems to be running",
  check for and remove the lock before diagnosing anything else.
- **An order that asserts an ABSENCE quotes the query and its empty
  result** (run 46). "there is no tool for X", "no record covers Y", "no
  lane owns this TU" are the cheapest claims to write and the most
  expensive to act on, because a worker cannot distinguish "measured
  absent" from "did not look". Every one of them is one command:
  `gdlmem.py search "<terms>"` / `find --function <f>` /
  `claims --owns <path>` / `Get-ChildItem tools/gdl -Filter "*x*"`, and
  the order pastes the command AND the empty output beside the claim.
  Measured in run 46's own queue: item 5 asserted "6 image passes at 2-4
  min each"; the seven heaviest passes in the tree measured 0.4s to
  13.2s, so the lane built the cheap fix and reported the refutation
  instead of a cache nothing needed. Item 8 asserted a probe defect
  ("--fuzzy on CONFLICT exits 255 with no number") that was trap 6a in
  the reporter's own shell. A present-tense number in an order needs a
  record id (already a rule above); an ABSENCE needs its query, which is
  the same rule pointed at the other kind of claim.
- **A tool item's fix belongs to the lane that owns the tool, but the
  MEASUREMENT belongs to the reporter** — quote the failing command and
  its verbatim output in the item, not a paraphrase of what it seemed to
  do. Both refuted run-46 items above were paraphrases of a real
  symptom whose cause the paraphrase had already discarded.

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
