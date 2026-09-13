# Handoff — lane `claude/cowork-lane-01`

Report format per `AGENTS.md` § Closeout. One commit, ready for integration.

## Closeout report

```text
FUNCTION/TU and owned files: game/world/hiscore.c (whole TU). Files written: configure.py
  (one line). No files under src/ were modified. No shared headers touched.

STATUS: RECLASSIFIED — the TU was already byte-exact and already source-linked; the commit
  corrects its Object() state. This is not a matching result and no reconstruction was done.

BASELINE / BEST: unchanged by this commit, because there was no residual to close.
  Measured before the flip, from build/GUNE5D/report.json:
    game/world/hiscore.c  fuzzy 100.00%  matched_functions 3/3  estimated byte gap 0
    sections: .text 0x5a8, .data 0x60, .sdata 0x10, .sdata2 0xa2, extab 0x10, extabindex 0x18
  Project measures: complete_units 206/401 -> 207/401.
  Game Code linked: 52/111 (11.54%) -> 53/111 (11.71%).
  Whole-project fuzzy/matched unchanged at 98.14% / 61.82% — as expected for a state flip.

SEMANTIC / SOURCE-SHAPE CHANGES: none. No source, header, cflag, compiler, pragma or TU
  boundary was altered. No padding, scaffolding, stub, alias or fakematch introduced.

ATTEMPTED AXES and held-fixed variables: no experiments were run — none were needed. Held
  fixed: all source, all cflags, mw_version (GC/1.3.2 via cflags_demo default), splits.txt,
  symbols.txt, config.yml.

REMAINING RESIDUAL and next evidence-backed action: none for this TU. It now links from
  source with the whole-DOL checksum still exact. Next candidates by measured closeness to a
  whole-TU close (fns short / estimated gap): game/ui/message.c 2 / 3B, game/g3d/g3dpad.c
  1 / 8B, game/world/world.c 1 / 10B, game/g3d/gcontrolpads.c 1 / 13B, game/mb/mb_camera.c
  1 / 25B, game/crt/vsprintf.c 1 / 35B. game/sys/registry.c is 1 / 8B but needs the optional
  derived 1.2.5sn profile. Largest single gap in the project: do_sel_menu in
  game/ui/select.c, 0.00% at 4724 bytes.

VERIFICATION: exact commands and results.
  python tools/gdl/claimcheck.py game/world/hiscore.c
    -> [game/world/hiscore.c] ok (.data 0x60, .sdata 0x10, .sdata2 0xa2, .text 0x5a8,
       extab 0x10, extabindex 0x18)
  python tools/gdl/datadiff.py game/world/hiscore.c
    -> .data: OK 0x60 bytes compared, 20 reloc words skipped
    -> .sdata: OK 0x10 bytes compared
    -> .sdata2: DATA-DEBT 0xA2 compared, 0x6 claim slack (zero-filled; structural;
       tool states advisory, not a flip blocker; 0 shrinkable, 1 structural)
  python tools/gdl/textorder.py game/world/hiscore.c
    -> ORDER-OK  game/world/hiscore  (3 shared .text functions, 0 target-only, 0 ours-only)
  rm -f build/GUNE5D/ok && ninja -j2            -> exit 0, "build/GUNE5D/main.dol: OK"
  sha1sum -c config/GUNE5D/build.sha1           -> exit 0 (build/GUNE5D/main.dol: OK)
  ninja all_source                              -> exit 0, 0 FAILED
  git diff --check HEAD                         -> exit 0
  git diff --cached --check (after staging)     -> exit 0
  python -m unittest discover tools/gdl/tests -b -> Ran 3830 tests, OK (skipped=37), exit 0

  LIMITS AND UNRUN GATES — not run, and not claimed as passing:
  - fndiff / `real` instruction-word comparison: not run. There was no residual to measure;
    closeness came from report.json fuzzy + matched_functions, not from a word-level diff.
  - tools/gdl/reconstruction_preflight.py: not run.
  - fakematch-lint (Rust) source scan: not run — no cargo in this environment; CI's separate
    reconstruction-lint workflow owns it. No source changed, so its input is unchanged.
  - The optional --experimental-p6-compiler profile was not used, so gamemain.c and
    registry.c were measured in their fallback state.

  ENVIRONMENT CAVEAT for reproduction: built on Linux with the CodeWarrior compilers run
  under wibo 1.0.3, compilers archive tag 20251118, dtk v1.8.3, ninja 1.13.2, configured as
  `python3 configure.py -v GUNE5D --compilers build/compilers`. GC/1.2.5 and GC/1.2.5n
  hash to the pins in tools/gdl/mwcc_p6/README.md (0443b5c0…, ccf4b465…). claimcheck.py
  hardcodes powerpc-eabi-objdump.exe, so a symlink without the .exe suffix was created
  inside the ignored build/binutils directory; no tool source was edited.

COMMIT / BRANCH / WORKTREE:
  commit 35f75a5b "Match game/world/hiscore.c" (1 file, +1 -1)
  branch claude/cowork-lane-01, forked from d2fbb5a5 (origin/main at provision time)
  git rev-list --left-right --count main...claude/cowork-lane-01 -> 0  1
  worktree: standalone clone, classification READY (clean tree, one unique commit, unmerged)
  not pushed — worker lane, per AGENTS.md line 618

PRIVATE ARTIFACTS preserved: baseline report.json from before the flip; build logs
  (lane-cold-build.log, lane-flip-build.log, lane-allsource.log); build/lane-extract-dol.py,
  a standalone DOL extractor that reads the boot header's DOL offset and section table to
  pull sys/main.dol out of a disc image without unpacking the full filesystem.

UNRELATED DIRTY FILES preserved: none. The only untracked file is LANE_LOCK, which
  AGENTS.md line 597 requires stay untracked; it was added to .git/info/exclude locally
  rather than committed.
```

## Applying this handoff

The payload is `0001-Match-game-world-hiscore.c.patch` alongside this file.

```sh
git checkout -b claude/cowork-lane-01 d2fbb5a5
git am 0001-Match-game-world-hiscore.c.patch
python configure.py -v GUNE5D
rm -f build/GUNE5D/ok && ninja -j2      # expect: build/GUNE5D/main.dol: OK
```

Commit author is `Claude (Cowork worker) <noreply@anthropic.com>`; rewrite it on integration
if the history should carry a different identity. No attribution trailers, per AGENTS.md.
