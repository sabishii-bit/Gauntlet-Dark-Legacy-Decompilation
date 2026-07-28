# Repository placeholder-symbol audit

Audit date: 2026-07-28

This pass audited `fn_XXXXXXXX` and `lbl_XXXXXXXX` names across `src/`,
`include/`, and `config/GUNE5D/symbols.txt`. It deliberately promotes only
names supported by one of these evidence classes:

1. the same address already has a canonical name in `symbols.txt`;
2. Xbox `shell3D.pdb` module order plus a GameCube behavioral anchor;
3. a unique PDB name confirmed from GameCube callers or decompilation.

Descriptive guesses and platform-specific names without a GameCube anchor stay
as placeholders.

## Results

- 268 source placeholders were replaced:
  - 193 functions;
  - 75 data symbols.
- 231 were stale source/config spellings: the canonical symbol was already
  assigned to the same address in `symbols.txt`.
- 37 additional functions were recovered from the PDB and checked against the
  GameCube binary.
- Remaining in source and headers: 540 unique `fn_*` and 1,067 unique `lbl_*`
  references.
- Remaining in the symbol file: 565 `fn_*` definitions. The much larger raw
  `lbl_*` definition count includes linker-generated and unreferenced labels,
  so source-reference counts are the more useful prioritization metric.

## Newly recovered function clusters

### ML_FMATH.OBJ

The GameCube TU reverses most of the Xbox PDB order. Matrix formulas and Ghidra
decompilation independently confirm:

| Address | Name |
|---|---|
| `0x800BCD68` | `ExtractYPR` |
| `0x800BCED8` | `ExtractPYR` |
| `0x800BD154` | `CreateRYPMatrix` |
| `0x800BD254` | `CreatePYRMatrix` |
| `0x800BD360` | `AddAngle` |
| `0x800BD3A4` | `SubAngle` |
| `0x800BD428` | `GetYawPitch` |
| `0x800BD488` | `CreateDirMatrix` |

These names improve shared dependencies used by world, camera, UI, effects,
and item code.

### MESSAGE.OBJ

The inventory/fire-scroll block is a clean reverse-order match to the PDB:

| Address | Name |
|---|---|
| `0x8006C3A0` | `draw_panels` |
| `0x8006C4D0` | `end_inventory_panel` |
| `0x8006C5DC` | `init_inventory_panel` |
| `0x8006C60C` | `init_panel_blits` |
| `0x8006CCB8` | `animate_panel_piece` |
| `0x8006D0A4` | `disp_piece` |
| `0x8006D18C` | `print_n_of_m` |
| `0x8006D29C` | `ServeFireScroll` |
| `0x8006D458` | `EndFireScroll` |
| `0x8006D4E8` | `StartFireScroll` |
| `0x8006D7BC` | `FireScrollActive` |
| `0x8006D7D8` | `FireScrollReset` |
| `0x8006D7E4` | `ticks_for_firescroll` |
| `0x8006D7EC` | `ControllerMessageBox` |

### SELECT.OBJ

PDB names were matched to unique behavior and call graphs:

| Address | Name |
|---|---|
| `0x8008E3BC` | `init_player_change` |
| `0x8008F768` | `setup_file_entries` |
| `0x8008F914` | `verify_vmu_file_ok` |
| `0x8008F984` | `setup_vmu_entries` |
| `0x8008FA70` | `setup_sel_menu` |
| `0x8008FC5C` | `sel_set_inactive` |
| `0x8008FC78` | `sel_set_choice` |
| `0x8008FE70` | `other_players_next_level` |
| `0x8008FED4` | `check_active_players` |
| `0x8008FFF0` | `update_class_attr` |
| `0x80090450` | `update_class_spec` |
| `0x80090B6C` | `hide_select_blits` |
| `0x80090C34` | `setup_tex` |
| `0x80090D6C` | `serve_blits` |

`0x800C79E4` was also promoted to `pbTreeTraverse`, as established by the
`pb_tree` PDB roster and its traversal body.

## Highest-value remaining source concentrations

| File | Functions | Data | Total |
|---|---:|---:|---:|
| `src/game/sound/sounds_evt.c` | 114 | 51 | 165 |
| `src/game/game/player.c` | 46 | 119 | 165 |
| `src/game/game/gamemain.c` | 44 | 74 | 118 |
| `src/game/ui/attract.c` | 13 | 71 | 84 |
| `src/game/enemy/enemy.c` | 39 | 32 | 71 |
| `src/game/game/controls.c` | 8 | 62 | 70 |
| `src/game/enemy/critter.c` | 50 | 9 | 59 |
| `src/game/sys/main.c` | 18 | 40 | 58 |
| `src/game/movie/movieplayer.c` | 43 | 14 | 57 |
| `src/game/game/combat.c` | 43 | 12 | 55 |

The next productive passes should be module-scoped rather than address-wide:

- `PLAYER.OBJ`, `ENEMY.OBJ`, and `GAMEMAIN.OBJ` provide the largest
  architectural payoff.
- `SOUNDS.OBJ` has many functions, but the GameCube order diverges from Xbox
  and sound IDs alone do not prove names; caller-based event identification is
  required.
- Data labels should be promoted in ownership groups after the containing
  struct/table is established, avoiding attractive but weak per-use guesses.

