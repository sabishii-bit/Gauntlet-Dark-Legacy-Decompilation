# PLAYER.OBJ symbol and dependency audit

Date: 2026-07-28

## Result

`player.c` already owned a complete, named 92-function GameCube text range
(`0x800745D0..0x8008091C`).  Its unresolved surface was instead dominated by
imports and by the TU's pooled data block.  This pass mapped 39 identifiers
from that surface and propagated the names repository-wide.

After the pass, `src/game/game/player.c` contains 23 unique `fn_` imports and
106 unique `lbl_` references, down from 46 and 119 respectively.  The
remaining names are deliberately unresolved rather than inferred from Xbox
ordinal position.

## High-confidence dependency mappings

| GameCube address | Name | Evidence |
| --- | --- | --- |
| `0x8002F5D8` | `ModifyDamage` | COMBAT.OBJ roster plus armor/damage/flags/shield call contract |
| `0x8002F818` | `DamageColor` | COMBAT.OBJ roster plus low-nibble color-to-player mapping |
| `0x80089120` | `PlayerDoWeapTrail` | PSFX.OBJ roster, TU boundary, player-record weapon-trail fields, Ghidra body |
| `0x80091F34` | `StartGemFX` | rune/garg/gem effect dispatch plus SFX.OBJ roster |
| `0x80092794` | `StartShieldFX` | shield effect table, light setup, and SFX.OBJ roster |
| `0x80092B58` | `StartThrowMagicFX` | launched velocity/yaw magic body and SFX.OBJ roster |
| `0x80092DF4` | `StartMagicFX` | non-thrown magic effect body and SFX.OBJ roster |
| `0x8009CADC` | `AudioExp` | level delta caller, named-voice events, SOUNDS.OBJ roster |
| `0x8009F028` | `AudioPotion` | potion color/mode sound table and SOUNDS.OBJ roster |
| `0x8009F118` | `AudioPlayerXray` | see-through transition caller and player-position sound body |
| `0x8009F198` | `AudioHeartBeat` | health-dependent cadence/volume body |
| `0x8009F250` | `AudioPlayerDies` | death-only caller and death SFX/voice pair |
| `0x8009F2DC` | `AudioPlayerHit` | damage-kind table and player-position impact body |
| `0x8009F7D8` | `AudioPlayerSeverePain` | severe-pain per-class voice table |
| `0x8009F960` | `AudioPlayerPoison` | poison damage caller and per-class poison sound table |
| `0x8009F9E8` | `AudioPlayerPain` | randomized per-class pain voice table |
| `0x8009FE4C` | `AudioPlayerBreath` | idle cadence caller and named breath event |
| `0x8009D3A8` | `AudioMenuExit` | back/cancel callers |
| `0x8009D3D4` | `AudioCursorSelect` | accept/use callers |
| `0x8009D42C` | `AudioCursorH` | left/right navigation callers |
| `0x8009D458` | `AudioCursorV` | menu selection movement callers |
| `0x800B28EC` | `MBBlitSetColor4` | exact MB_BLIT.OBJ roster and four-corner color body |
| `0x800B290C` | `MBBlitSetAlpha` | exact MB_BLIT.OBJ roster and fade body |
| `0x800B2940` | `MBBlitSetColor` | exact MB_BLIT.OBJ roster and brightness body |
| `0x800B2980` | `MBBlitGetTex` | exact MB_BLIT.OBJ roster and texture accessor body |

The SOUNDS mappings use caller behavior and table shape, not function order.
The GameCube and Xbox SOUNDS layouts diverge enough that ordinal mapping is
not evidence.

## PLAYER.OBJ data mappings

The TU's BSS/SBSS anchors now use the PLAYER.OBJ names:

- `potionicon_tab`, `player_multiple_models`, `tb_info`, and `frame_blit`
- `key_blit_idx`, `alpha`, `it_blit`, and `got_max_player_sizes`
- `firstgetidx`, `randpottype`, and `welcome_timer`
- `Hidden` and `Cheats` for the hidden-character and powerup-cheat tables

`player_multiple_models` is also used as the C static name, eliminating the
old `model_slot` alias.

## Automation

Two small tools now cover the mechanical parts of future symbol audits:

- `tools/gdl/audit_placeholders.py` finds `fn_`/`lbl_` references whose exact
  address already has a canonical name in `symbols.txt`.
- `tools/gdl/rename_symbols.py` previews or applies an exact-identifier rename
  manifest across tracked text files.

The reviewed manifest for this pass is
`research/player_dependency_symbols.json`.  Typical use:

```text
python tools/gdl/rename_symbols.py research/player_dependency_symbols.json
python tools/gdl/rename_symbols.py research/player_dependency_symbols.json --apply
```

The first command is intentionally a dry run.  This separates semantic
research from repository-wide editing and makes the exact rename batch
reviewable.

## Verification

`python configure.py` followed by `ninja` completed successfully, including
the repository SHA check:

```text
build/GUNE5D/main.dol: OK
```
