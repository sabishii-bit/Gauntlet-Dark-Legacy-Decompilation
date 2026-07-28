---
name: remaining-tu-map
description: "Final undeclared .text inventory and reliable TU-boundary method"
---

# Remaining `.text` TU inventory (2026-07-28)

The last auto-generated code regions were resolved into ten real source
objects. After wiring them, objdiff reports only
`main/auto_03_800B53B0_text` (4 bytes), which is the alignment gap between
`mb_blit.c` and `mb_camera.c`, not a translation unit.

| TU | `.text` | `extab` | `extabindex` | Key evidence |
|---|---|---|---|---|
| WORLDCOL.OBJ | `8000CF40..8000E8E8` | `800054E0..80005548` | `800087A0..8000883C` | 15 GCN functions = 15-function PDB roster in reverse order |
| ANIM.OBJ | `8000E8E8..8000F628` | `80005548..80005580` | `8000883C..80008890` | `AnimInit` calls anim_play inverse-delta init; next body is `ZeroAnimData` |
| anim_play.obj | `8000F628..80010A4C` | `80005580..80005590` | `80008890..800088A8` | `Zero/Init/CalcAnimData`, giant angle sampler, inverse-delta-table builder |
| ATREE.OBJ | `80010A4C..800137BC` | `80005590..80005678` | `800088A8..80008A04` | exact `DoTexMods` front and ATTRACT seam; PDB reverse-order anchors |
| COMBAT.OBJ | `8002951C..8003101C` | `80005B00..80005C10` | `800090D0..80009268` | link-order slot CAMERA → COMBAT → CONTROLS plus exception runs |
| CRITTER.OBJ | `80034CFC..8004229C` | `80005CE0..80005F28` | `800093A0..8000970C` | CONTROLS → CRITTER seam, full behavior/load roster, SOUND manager follows |
| PMOTION.OBJ | `8008091C..80089120` | `80006B30..80006BE8` | `8000A918..8000AA2C` | `get_player_pos` first; floor-collision tail; PSFX begins at `80089120` |
| PSFX.OBJ | `80089120..8008BC50` | `80006BE8..80006C50` | `8000AA2C..8000AAC8` | player-FX/data bodies; `LoadPdataFile` is the final body |
| PSX2.OBJ | `8008BC50..8008BF88` | `80006C50..80006C60` | `8000AAC8..8000AAE0` | `LoadVU1GameLogic` (`blr`), `init_psx2`, merged IRX helper |
| RECORDER.OBJ | `8008BF88..8008C52C` | `80006C60..80006C80` | `8000AAE0..8000AB10` | save/load world, player, item, and camera snapshot; SELECT follows |

## Boundary technique

1. List every auto-generated unit containing `.text` from
   `build/GUNE5D/report.json`, then merge adjacent function fragments into
   contiguous holes between already declared sources.
2. Use PDB module order as a hypothesis, never as proof. Confirm with two or
   more of: recognizable behavior/strings, exact roster count, neighboring
   modules, and exception-table ownership.
3. Dump `extabindex` from `main.elf`. Each 12-byte record gives function
   address, function size, and its `extab` pointer. Object seams become exact
   when the first function of the next semantic cluster also begins the next
   exception run.
4. Watch for functions without exception records. In particular,
   `LoadVU1GameLogic` at `8008BC50` is a one-instruction function between two
   indexed bodies; text semantics identify its TU even though extab cannot.
5. Treat tiny auto regions between declared objects as alignment only after
   both neighbors are established. The remaining `800B53B0..800B53B4` hole is
   exactly that case.

## Naming discipline

GC often emits a module in reverse PDB source order, but inlining and
platform-only bodies create insertions/deletions. Use reverse-order mapping
only while sizes and behavior continue to agree. Keep `fn_` names at the
first ambiguous point; a correct TU scaffold is more valuable than a
confident-looking wrong symbol map.
