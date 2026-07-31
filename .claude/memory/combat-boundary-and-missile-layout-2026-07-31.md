---
name: combat-boundary-and-missile-layout-2026-07-31
description: "Correct COMBAT/CONTROLS seam, exact missile data layouts, and two MWCC matching techniques"
metadata:
  node_type: memory
  type: project
---

# Combat boundary and missile-layout findings

## Correct TU seam

COMBAT.OBJ ends at `0x8003104C`, not `0x8003101C`.  The function at
`0x8003101C..0x8003104C` is Xbox PDB `ResetPlayerMissiles`; its GCN body is
exactly `memset(PlayerMissileTreeInfo, 0, 0x20)`.  CONTROLS.OBJ begins with
`active_player_edge` at `0x8003104C`.

Move the exception metadata with the text seam:

- combat extab `0x80005B00..0x80005C18`
- combat extabindex `0x800090D0..0x80009274`
- controls extab `0x80005C18..0x80005CE0`
- controls extabindex `0x80009274..0x800093A0`

This produces an exact `ResetPlayerMissiles` and preserves the full-DOL SHA-1.

## Proven COMBAT data layouts

Xbox PDB inter-symbol deltas and GCN indexing/strides agree on these partitions:

- `80118E28 PlayerMissileDesc` `0x140` = 16 `MISSILEDESC` records (`0x14`)
- `80118F68 PlayerMissileInfo` `0x180` = 8 `MISSILEINFO` records (`0x30`)
- `801190E8 DmgTypeDesc` `0x28` = five inline 8-byte names
- `80119110 EnemyMissileDesc` `0x18` = three inline 8-byte names
- `80119128 EnemyMissileInfo` `0xFC0` = 28 x 3 `MISSILEINFO` records
- `8011A0E8/8011A118/8011A148` = `BallistaMissileInfo`,
  `BossElecMissileInfo`, `BossAcidMissileInfo`, each `0x30`

PDB `MISSILEINFO` is exactly `0x30`: damage type, damage, speed, collision
radius, hit radius, angular velocity[3], weight, hit FX, audio, wall sound.
Do not append a guessed mode field: that makes the record `0x34`, breaks array
strides, and heavily degrades `InitPlayerMissiles`/`EnemyStartMissile`.

Runtime mappings:

- `80240560 pmissile_sfxidx[5]`
- `80240574 WeapThrowFx[4][5]`
- `802405C4 WeapHoldFxTree[4][5]`
- `80240614 FamiliarSpit[4]`, `80240624 PhoenixTree`
- `80240628 FamiliarTree[4][2]`
- `80240648 EnemyMissileTree[28][3]`
- `80240798 PlayerMissileTreeInfo[4]` (`throwHeader`, `throwFlags`)

## MWCC source-shape wins

`AverageCameraTargetPosition` reached zero real differences by declaring its
temporary pointer before the target-count local and putting both loop increments
in the `for` increment clause.  That yields the target register assignment
`q=r8`, `count=r9`, `i=r10`, `offset=r4`.

`init_stage_info` needs an otherwise-unused 8-byte local to recover the target
24-byte frame.  Updating width and height in place (`width += 60; height += 16`)
also recovers the target `r30/r31` allocation; only four scheduling-only lines
remain.
