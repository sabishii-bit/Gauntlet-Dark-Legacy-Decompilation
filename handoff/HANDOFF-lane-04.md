# Handoff — lane `claude/cowork-lane-01`, commit 4

Report format per `AGENTS.md` § Closeout. Payload:
`0004-Add-volregs-volatile-register-residual-classifier.patch`.

## Closeout report

```text
FUNCTION/TU and owned files: no TU matched. Files added:
  tools/gdl/volregs.py                  (365 lines)
  tools/gdl/tests/test_volregs.py       (142 lines)
  Nothing existing was modified; nothing under src/ or config/ was touched.

STATUS: RECLASSIFIED — new measurement capability, plus a project-wide census
  that changes where the remaining work is believed to be. Matches nothing.

WHY IT EXISTS: `savedregs` states outright that it "never reads" volatile
  registers, and three stuck residuals sat in that blind spot. volregs aligns
  the two streams on the same opcode-sequence alignment savedregs uses (its
  `aligned_rows` is imported, not reimplemented) and classifies every differing
  row COLOUR / SAVED-COLOUR / MATERIALIZE / STRUCTURAL / UNPAIRED, then reports
  whether one relabeling explains the volatile rows (CONSISTENT) or none does
  (INCONSISTENT).

WHAT IT FOUND ON THE THREE MOTIVATING FUNCTIONS:

  game/g3d/g3dpad::G3DUpdatePadStatus
    250 identical, 32 COLOUR, and ZERO of everything else. Correspondence
    r7->r9, r9->r7. The entire 72-line residual that a prior campaign closed
    with the now-retired WebFrank rule is ONE TRANSPOSITION of two volatile
    registers. This is the cleanest colour-only function found in the project
    and the best candidate for characterizing MWCC's volatile numbering,
    because nothing else is mixed into it.

  game/g3d/gcontrolpads::G3DReadControlPadStates
    38 identical, 5 COLOUR (r6<->r8), 2 MATERIALIZE, 2 STRUCTURAL, 2 UNPAIRED.
    The STRUCTURAL and UNPAIRED rows are the dtk relocation-reconstruction
    artifact around gPadManager already documented in HANDOFF-lane-03.

  game/ui/message::msgDraw
    301 identical, 0 COLOUR, 5 SAVED-COLOUR, 8 STRUCTURAL. AND THE EIGHT ARE
    NOT COLOUR AT ALL — every one is
      T: lfs f1,0(0)  @lbl_80348618(EMB_SDA21)
      O: lfs f1,0(0)  @@187(EMB_SDA21)
    identical instruction words against a NAMED target pool symbol
    (lbl_80348618 / lbl_8034861C) versus our ANONYMOUS pool entry (@187 /
    @236). That is float-literal pool NAMING, the same class fndiff calls
    MATCH-MODULO-RELOC-NAMING and treats as link-equal on two gcontrolpads
    functions. An earlier note in claude/findings-message-c.md called these
    eight "a volatile-register residual" on savedregs' say-so; that reading was
    WRONG and is corrected by this measurement. msgDraw's real residual is its
    5 callee-saved permutation rows, which savedregs already had.

THE CENSUS, and the reason it matters more than the tool:
  336 non-exact functions across the 57 NonMatching TUs.
    CONSISTENT              95
    INCONSISTENT           155
    NO-COLOUR               86
    unreadable               0
    with MATERIALIZE rows    16
    with STRUCTURAL rows    286
    with SAVED-COLOUR rows  227
  286 of 336 functions carry STRUCTURAL rows — real differences, not colour —
  against only 16 with the constant-vs-copy MATERIALIZE shape. So the bulk of
  what remains is ordinary reconstruction, and the copy-vs-materialize pattern
  I spent probes on is rare rather than systemic. The three functions this lane
  chose are the unusual nearly-done colour-bound ones; they are NOT
  representative of the remaining work.

  Most STRUCTURAL rows (read these first):
    1690 game/game/pmotion::PlayerMotion            [INCONSISTENT]
     537 game/sfx/sfx::ProcessEffects               [INCONSISTENT]
     515 game/world/items::fn_800606FC              [INCONSISTENT]
     423 game/ui/select::do_player_select           [INCONSISTENT]
     344 game/game/player::do_players               [INCONSISTENT]
     342 game/pb/pb_objregs::sDrawGeom              [INCONSISTENT]
     327 game/sound/sounds_evt::AudioSetupBossStreams [CONSISTENT]
     323 game/ui/options::DoOptions                 [INCONSISTENT]
     237 game/shop/shop::do_shopping_8009AA48       [INCONSISTENT]
     209 game/sys/memcard::loadGauntletSave         [INCONSISTENT]

REMAINING RESIDUAL: unchanged everywhere. No source file was edited.

VERIFICATION: exact commands and results.
  python -m unittest tools.gdl.tests.test_volregs -v -> Ran 12 tests, OK
  python -m unittest discover tools/gdl/tests -b -> Ran 3855 tests, OK
      (skipped=37), exit 0   [3843 before this commit; +12 is this file]
  python -m flake8 tools/gdl/volregs.py tools/gdl/tests/test_volregs.py
      -> exit 0 both
  rm -f build/GUNE5D/ok && ninja -j2 -> exit 0, "build/GUNE5D/main.dol: OK"
  sha1sum -c config/GUNE5D/build.sha1 -> exit 0
  git diff --check HEAD / --cached --check -> exit 0 both

  TWO BUGS THE TESTS CAUGHT DURING DEVELOPMENT, both now pinned:
  1. Registers live INSIDE operands. Comparing whole operand strings read
     `lwz r4,0(r8)` against `lwz r4,0(r6)` as STRUCTURAL — a plain colour row
     mislabelled a real difference, found on G3DReadControlPadStates.
  2. ABI registers were folded into SAVED-COLOUR. `addi r3,r1,8` against
     `addi r3,r13,8` swaps the stack pointer for the small-data base, which is
     a real difference, not a colour question. Only callee-saved and volatile
     now count as a relabeling; r1/r2/r13 fall through to STRUCTURAL. This one
     was caught by a test written before the code was right.

  TWO-SIDED. Positive: volatile swaps are COLOUR including inside memory
  operands; a bijection is CONSISTENT; constant-vs-copy is MATERIALIZE in
  either direction. Negative: a differing immediate is STRUCTURAL and never
  colour; a callee-saved swap is SAVED-COLOUR and never STRUCTURAL; a
  many-to-one map is INCONSISTENT in EITHER direction (checking only forward
  would call a many-to-one map consistent); an empty map is NO-COLOUR, not
  CONSISTENT; an ABI-register difference is STRUCTURAL.

  LIMITS AND UNRUN GATES — not claimed as passing:
  - CONSISTENT is evidence, NOT proof, and the tool says so in its own output.
    One relabeling covering every row is what pure allocator colour looks like
    AND what one upstream decision that renamed everything downstream looks
    like. It identifies no decision and never proves a function unreachable
    from source.
  - Classification is per-row and syntactic. It does not model liveness, so a
    MATERIALIZE row is a shape match, not proof that the copied register
    actually held the constant.
  - UNPAIRED rows mean the alignment could not pair them; they are reported,
    never folded into another class.
  - reconstruction_preflight.py not run. fakematch-lint not run (no cargo); no
    source under src/ changed, so its input is unchanged.

ENVIRONMENT: Linux, CodeWarrior under wibo 1.0.3 + sjiswrap, compilers tag
  20251118, dtk v1.8.3, ninja 1.13.2, configured with
  `--compilers build/compilers`. claimcheck.py's hardcoded
  powerpc-eabi-objdump.exe still worked around by a symlink in the ignored
  build/binutils.

COMMIT / BRANCH / WORKTREE:
  commit b935a333 "Add volregs volatile-register residual classifier"
  (2 files, +507 -0), on claude/cowork-lane-01, parent 54e14df4
  worktree READY: clean tree, one unique commit ahead of the fork, not pushed

PRIVATE ARTIFACTS preserved: build/GUNE5D/volregs.json (all 336 rows),
  build/GUNE5D/web_reuse_census.json, build/lane-index-modules.py,
  build/lane-module-map*.json, build/lane-extract-dol.py.

UNRELATED DIRTY FILES preserved: none. LANE_LOCK untracked via
  .git/info/exclude.
```

## Applying

```sh
cd "D:\Gauntlet Decomp\pr"
git am "..\handoff\0004-Add-volregs-volatile-register-residual-classifier.patch"
git push
python -m unittest tools.gdl.tests.test_volregs      # expect 12 tests, OK
python tools/gdl/volregs.py game/g3d/g3dpad G3DUpdatePadStatus
python tools/gdl/volregs.py --all --out build/GUNE5D/volregs.json
```

Needs a full build first: it reads both objects through `fnasm.parse_fn`, and
`--all` also reads `build/GUNE5D/report.json`.
