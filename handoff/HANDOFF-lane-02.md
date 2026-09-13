# Handoff — lane `claude/cowork-lane-01`, commit 2

Report format per `AGENTS.md` § Closeout. Payload:
`0002-Add-web_reuse_census-liveness-positive-control-finder.patch`.

## Closeout report

```text
FUNCTION/TU and owned files: no TU. New tool only. Files added:
  tools/gdl/composed_census/web_reuse_census.py (245 lines)
  tools/gdl/tests/test_web_reuse_census.py      (165 lines)
  Nothing existing was modified; no file under src/ or config/ was touched.

STATUS: RECLASSIFIED — this commit adds measurement capability and a population
  result. It matches nothing and changes no output bytes.

BASELINE / BEST: unchanged. build/GUNE5D/main.dol still verifies against
  config/GUNE5D/build.sha1. Progress measures untouched: complete_units 207/401,
  Game Code 53/111 linked.

SEMANTIC / SOURCE-SHAPE CHANGES: none.

ATTEMPTED AXES and held-fixed variables: not a matching experiment. Held fixed:
  all source, all config, all existing tools.

WHY IT EXISTS: `shapegrep` slices the target by opcode shape and
  lowmatch/nearmiss slice by score, so no tool could express a LIVENESS
  property. Hunting game/ui/message::msgDraw's residual (2 ranges PERMUTED
  r29->r25 per `savedregs --per-web`) with `shapegrep addi,srawi --exact-only`
  returns ml_mem::AllocFile, whose two hits come from three unrelated `>> 10`
  shifts on separate variables — a false control. web_reuse_census groups
  DEFINITIONS by callee-saved register instead, so a hit is a byte-exact
  function where one register genuinely holds the same computation twice.

RESULTS MEASURED AT 1e91f9b1:
  - 1309 byte-exact functions scanned; 487 hits at the default gap of 0x20.
  - Targeted query for msgDraw's role (`--role 'addi %0,0x2' --gap 0x40`)
    returns two positive controls: dolphin/os/OSContext::OSDumpContext (r25,
    offsets 0x194/0x1F8) and game/audio/dcsdrv::dcsHandleRequest (r31, four
    definitions across 0x238..0x2E8).
  - OSDumpContext reads as the clean control: two separate
    `for (i = 0; i < 32; i += 2)` loops share one `u32 i`, and MWCC kept both
    disjoint ranges in r25. Single-register reuse across disjoint ranges is
    therefore reachable from ordinary source.
  - POPULATION RESULT, and the one that matters: of (role, function)
    occurrences in byte-exact code, 249 keep a repeated role in ONE
    callee-saved register against 608 that spread it over two or more —
    **70.9% of repeated roles in byte-exact output are SPLIT across
    registers.** 163 exact functions show the same-register form, 334 the
    split form.

WHAT THAT CHANGES: our msgDraw form (first `lineHeight` range r29, second r25)
  is an ORDINARY MWCC outcome, not a symptom of wrong source. The residual is
  not "why did it split" but "why r25 rather than r29" — a numbering question,
  governed by class and declaration order
  (claim.law.MV_callee-saved-numbering-has-a-width-class-ahead-of-declaration-
  order.20260902.v1), and one already measured immovable there: r29 and r25 are
  four positions apart and any reorder large enough would pass through the 25
  ranges already in place. This retires the "abnormal split" reading of that
  residual rather than closing it.

REMAINING RESIDUAL and next evidence-backed action: msgPost real 8, msgDraw
  real 10, both unchanged and both still pure register colour. See
  claude/findings-message-c.md for six refuted axes. The tool's
  `--distinct-roles` mode (registers hosting two different roles) is built and
  tested but has not been used for a campaign.

VERIFICATION: exact commands and results.
  python -m unittest tools.gdl.tests.test_web_reuse_census -v -> Ran 9 tests, OK
  python -m unittest discover tools/gdl/tests -b -> Ran 3839 tests, OK
      (skipped=37), exit 0   [was 3830 before this commit; +9 is this file]
  python -m flake8 <both new files> -> exit 0 (repo .flake8, E203/E501 ignored)
  rm -f build/GUNE5D/ok && ninja -j2 -> exit 0, "build/GUNE5D/main.dol: OK"
  sha1sum -c config/GUNE5D/build.sha1 -> exit 0
  git diff --check HEAD / --cached --check -> exit 0 both

  TWO-SIDED TESTS, as AGENTS.md requires for a tool change. Positive: a distant
  same-role pair in a callee-saved register is reported, and the role signature
  pairs across differing destination registers (the target's r29 vs our r25).
  Negative, each asserted to report NOTHING: stores and `stmw` are not
  definitions (first operand is a source), branches/compares/`mtlr`/`lmw` are
  not definitions, volatile registers are out of scope, a pair closer than
  --gap is adjacent redefinition rather than two ranges, a function below the
  fuzzy floor is not a positive control, and two DIFFERENT roles in one
  register belong to --distinct-roles rather than the default mode.

  LIMITS AND UNRUN GATES — not claimed as passing:
  - Update-form stores (`stwu rS,d(rA)`) also write rA; this does not count
    that. Documented in the module docstring.
  - Definitions are identified by "first operand is a callee-saved GPR" with a
    prefix exclusion list, not a full PPC opcode table. Sound for the opcodes
    MWCC emits here; not a general disassembler.
  - The role signature is a proxy for variable identity. Two different
    variables computing the same expression pair as one role, so a hit is a
    candidate to read, never a proof of variable reuse.
  - reconstruction_preflight.py not run. fakematch-lint not run (no cargo
    here); no source under src/ changed, so its input is unchanged.

ENVIRONMENT: Linux, CodeWarrior under wibo 1.0.3, compilers tag 20251118,
  dtk v1.8.3, ninja 1.13.2, configured with
  `--compilers build/compilers`. claimcheck.py's hardcoded
  powerpc-eabi-objdump.exe worked around by a symlink inside the ignored
  build/binutils; no tool source edited.

COMMIT / BRANCH / WORKTREE:
  commit cded87af "Add web_reuse_census liveness-shaped positive-control finder"
  (2 files, +410 -0), on claude/cowork-lane-01, parent 1e91f9b1
  git rev-list --left-right --count fork/claude/cowork-lane-01...HEAD -> 0  1
  worktree READY: clean tree, one unique commit, not pushed

PRIVATE ARTIFACTS preserved: build/GUNE5D/web_reuse_census.json (the full 487
  rows), build/lane-index-modules.py and build/lane-module-map*.json (the
  TU->Xbox module map), build/lane-extract-dol.py.

UNRELATED DIRTY FILES preserved: none. LANE_LOCK stays untracked, locally
  excluded via .git/info/exclude rather than committed.
```

## Applying

```sh
git checkout claude/cowork-lane-01          # or create it at 1e91f9b1
git am 0002-Add-web_reuse_census-liveness-positive-control-finder.patch
python -m unittest tools.gdl.tests.test_web_reuse_census   # expect 9 tests, OK
python tools/gdl/composed_census/web_reuse_census.py --role 'addi %0,0x2' --gap 0x40
```

The census needs `build/GUNE5D/asm/**/*.s` and `build/GUNE5D/report.json`, so run
it after a full build.
