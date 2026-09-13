# Handoff — lane `claude/cowork-lane-01`, commit 3

Report format per `AGENTS.md` § Closeout. Payload:
`0003-Fix-flagsweep-on-hosts-where-ninja-wraps-the-compiler.patch`.

## Closeout report

```text
FUNCTION/TU and owned files: no TU matched. Files touched:
  tools/gdl/flagsweep.py                        (+14 -4)
  tools/gdl/tests/test_flagsweep_wrapper.py     (new, 111 lines)
  Nothing under src/ or config/ was modified.

STATUS: RECLASSIFIED — a tool portability fix, plus a measured-dead axis for
  game/g3d/gcontrolpads::G3DReadControlPadStates. Matches nothing.

THE BUG: ninja_base_cmd() found the `mwcceppc.exe` token and kept only what
  FOLLOWED it, which lost two things on any host where ninja wraps the compiler:
  1. Every token BEFORE the compiler was dropped. On this project's Linux edge
     that prefix is `build/tools/wibo build/tools/sjiswrap.exe` — the wrapper
     that actually executes the PE. Without it every variant exec'd the PE
     directly and died `PermissionError: .../mwcceppc.exe`, so the entire
     battery reported "compile failed" and NO flag axis could be measured
     off-Windows at all.
  2. Ninja chains the dep transform with `&&`, and those tokens were kept as
     mwcc flags, so mwcc aborted with
     `Usage Error: Specified file '&&' not found`.
  Fix: return the wrapper prefix alongside the flags and prepend it to both
  compile invocations, and stop collecting flags at a shell operator. On Windows
  native the prefix is empty, so behaviour there is unchanged.

WHAT THE NOW-WORKING SWEEP MEASURED (game/g3d/gcontrolpads.c,
  G3DReadControlPadStates, target object build/GUNE5D/obj/.../gcontrolpads.o):
    target 49 lines. control (project flags) 51 lines, diff 13.
    NOT ONE of the 24 battery variants beats the control. Best equals it at 13
    (-opt nostrength, -opt space, -opt noprop, -opt nolifetimes, PROC=603e,
    -sym on, all three -inline forms, -schedule on, CC=1.2.5); everything else
    is worse (-opt speed 96, -opt noschedule 22, -schedule off 22,
    -opt nopeephole 22, CC=1.3.2 38, PROC=750 16, -opt nocse 15, CC=1.1p1 15).
    Four archive compilers are absent here (1.2.5e, 2.0p1, 2.5, 2.6, 2.7).
  `-opt noprop` is the flag form of the `#pragma opt_propagation off` already
  guarding that function, and it moves nothing. The flag/pragma axis for this
  function is therefore MEASURED-DEAD rather than untried.

TRIAGE RESULT for the two TUs that looked closest to done:

  game/g3d/gcontrolpads.c — ONE function away. 7 functions: 4 OK; 2
    (G3DGetControlPadAnalog, G3DControlPadButtonPressed) are
    MATCH-MODULO-RELOC-NAMING with ZERO non-reloc rows, i.e. `real` 0 —
    instruction words and relocation types all agree and the only difference is
    that the target names the switch jump tables (jumptable_80128670 size 0x20,
    jumptable_80128690 size 0x40, both scope:local) where our object emits
    anonymous locals. fndiff calls that link-equal. The remaining function is
    G3DReadControlPadStates at real 20, and it emits 51 instructions against the
    target's 49 — a genuine source-shape difference, not just colour.
    Its shape: wherever we emit `li rX,0` the target emits `mr rX,r29` /
    `addi rX,r29,0`, i.e. MWCC reuses a register already known to hold zero
    instead of materializing a fresh constant. The source already chains
    (`maskA = 0; maskB = maskA; gPadManager.count = maskA;`) and is already
    guarded by `#pragma opt_propagation off`, yet still propagates. Next
    evidence-backed action: test whether that pragma is INERT here by removing
    it and measuring — if the object does not move, the scaffold is dead weight
    and misleading, which is the re-audit AGENTS.md asks for.

  game/g3d/g3dpad.c — DEPRIORITIZE, and its source comment is stale. One DIFF
    function (G3DUpdatePadStatus) with a 72-line REGISTER_ONLY residual on
    VOLATILE temporaries around an inline branchless abs()
    (extsb/srawi/xor/subf): target uses r9/r10 where ours uses r7/r10. The
    file's header comment states that residual "is handled by the audited,
    hash-guarded WebFrank rule" — but AGENTS.md § "Native-only reconstruction
    campaign" records that the user retired all postprocessing on 2026-09-07 and
    forbids re-enabling WebFrank. That retirement is why this TU is NonMatching:
    its residual was never closed in source. The comment should be corrected so
    it stops implying a live mechanism.

REMAINING RESIDUAL: unchanged everywhere. No source file was edited.

VERIFICATION: exact commands and results.
  python -m unittest tools.gdl.tests.test_flagsweep_wrapper -v -> Ran 4, OK
  python -m unittest discover tools/gdl/tests -b -> Ran 3843 tests, OK
      (skipped=37), exit 0   [3839 before this commit; +4 is this file]
  python -m flake8 tools/gdl/tests/test_flagsweep_wrapper.py -> exit 0
  python -m flake8 tools/gdl/flagsweep.py -> 4 findings, ALL PRE-EXISTING and
      none in an edited region; verified by running flake8 against
      `git show HEAD:tools/gdl/flagsweep.py`, which reports the same four
      (E302 line 44, F841 `bm`, two E741 `l`). Not introduced, not fixed.
  rm -f build/GUNE5D/ok && ninja -j2 -> exit 0, "build/GUNE5D/main.dol: OK"
  sha1sum -c config/GUNE5D/build.sha1 -> exit 0
  git diff --check HEAD / --cached --check -> exit 0 both

  TWO-SIDED TESTS. Positive: a wrapped Linux-style edge yields the wrapper as
  its own list and flags that stop at the operator, with -MMD/-c/-o/-lang still
  stripped and the real flags surviving. Negative: an unwrapped Windows-style
  edge yields an EMPTY wrapper (the fix must not invent one); a source with no
  mwcc edge still raises SystemExit rather than silently sweeping nothing; and
  the returned tuple's arity is asserted, since a stale two-value caller would
  raise ValueError. Fixtures are the REAL ninja command line for
  gcontrolpads.o, not an invented one — an invented fixture initially failed
  these tests by mis-modelling `-c`, which consumes the following token.

  LIMITS AND UNRUN GATES — not claimed as passing:
  - The sweep covered the built-in battery only, under GC/1.2.5n, on one
    function. Four archive compilers are not present in this checkout.
  - A `diff` count from flagsweep is a normalized-line count, not objdiff
    `real`; it ranks variants and is not comparable to probe.py's numbers.
  - reconstruction_preflight.py not run. fakematch-lint not run (no cargo); no
    source under src/ changed, so its input is unchanged.
  - The four pre-existing flake8 findings in flagsweep.py were left alone
    deliberately, to keep this diff to the portability fix.

ENVIRONMENT: Linux, CodeWarrior under wibo 1.0.3 (+ sjiswrap), compilers tag
  20251118, dtk v1.8.3, ninja 1.13.2, configured with
  `--compilers build/compilers`. claimcheck.py's hardcoded
  powerpc-eabi-objdump.exe still worked around by a symlink inside the ignored
  build/binutils; no tool source edited for that.

COMMIT / BRANCH / WORKTREE:
  commit 092332ac "Fix flagsweep on hosts where ninja wraps the compiler"
  (2 files, +125 -4), on claude/cowork-lane-01, parent 5f1787d3
  worktree READY: clean tree, one unique commit ahead of the fork, not pushed

PRIVATE ARTIFACTS preserved: build/GUNE5D/web_reuse_census.json,
  build/lane-index-modules.py, build/lane-module-map*.json,
  build/lane-extract-dol.py, the sweep scratch under the system temp dir.

UNRELATED DIRTY FILES preserved: none. LANE_LOCK stays untracked via
  .git/info/exclude.
```

## Applying

```sh
cd "D:\Gauntlet Decomp\pr"
git am "..\handoff\0003-Fix-flagsweep-on-hosts-where-ninja-wraps-the-compiler.patch"
git push
```
