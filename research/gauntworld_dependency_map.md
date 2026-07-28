# Gauntworld dependency map

This map comes from the GameCube `main.dol` call graph for
`0x80058078..0x800631AC`, checked against the current DTK unit boundaries.  It
is intended to guide native reconstruction, not to claim that the entire
address range originated in one source file.

## Important boundary finding

The range currently assigned to `game/world/gauntworld.c` is a linker
composite.  The Xbox PDB order cannot describe the GameCube order: for example,
the PDB's gauntworld function `GetEnemyTypes` is already mapped at `0x8005773C`,
before `ResolveWorldData` at `0x80058078`.  Keep unproven functions named
`fn_XXXXXXXX` until their identity is anchored by behavior, cross-references,
or another symbol source.

The useful architectural role of this range is nevertheless clear.  It owns
world-WAD resolution, model/animation acquisition, world-object transforms,
object activation and collision dispatch, and the high-level world update
path.  Its outward calls form the following subsystem map.

## Direct dependencies

| Dependency TU | Observed calls from the gauntworld range | Architectural role |
| --- | --- | --- |
| `game/world/items` | `0x800631AC`, `0x80063444`, `0x800635B4`, `0x8006366C`, `0x80063854`, `0x80065D98`, `0x800674F4` | Item creation, lookup, world-object binding, and progression state. This is the tightest adjacent boundary. |
| `game/mb/mb_tree` | Multiple `0x800BAxxx` calls from the object and update paths | Scene-node creation, parenting, traversal, and lifecycle. |
| `game/mb/mb_util` | `GetWorldMat` and related transform helpers | Converts MB scene state into world collision/attention positions. |
| `game/mb/mb_objects` | `MBSetObject` | Instantiates or updates renderable world objects. |
| `game/mb/mb_model` | `MBOX_LoadModel`, `MBOX_AllocModel`, `ReallyFindObject` | Model acquisition and object lookup. |
| `game/mb/mb_camera` | `MBWorldSphereVisible3` | Visibility decisions in the world update path. |
| `game/enemy/enemy` | `generate_enemy`, `find_enemy_slot` | Enemy spawning and capacity management. |
| `game/sound/sounds_evt` | Dense set of calls in `0x8009Cxxx..0x8009FBxx` | Object-, item-, and state-driven event sound dispatch. |
| `game/sound/sounds` | `AudioStopAll`, `AudioSecretProc`, `AudioPlayEvt101` | Global and special-case audio transitions. |
| `game/world/tower` | `0x800A1C28..0x800A27DC` | Rune/shard and tower progression gates. |
| `game/boss/bosscam` | `TriggerCameraActivate` | Boss-triggered camera transitions. |
| `game/world/camera` | `ShakeCamera` | World-event camera feedback. |
| `game/ui/message` | `msgPost` | Player-facing world and progression messages. |
| `game/ps2/ml_fmath` | Several calls in `0x800BCB44..0x800BE920` | Vector, matrix, distance, and collision math. |
| `game/game/gamemain` | `0x8005412C` | High-level game-state integration. |
| `game/crt/vsprintf` | `sprintf` | WAD paths, ambient sound names, and diagnostics. |

The large update/dispatch function at `0x800606FC` crosses almost every row in
the table.  It is therefore a good integration oracle: translating a callee
with the wrong signature is likely to become visible there as a bad argument
register, wrong field width, or inconsistent ownership assumption.

## Unresolved callees that may reveal new TU boundaries

The range also calls functions in the current auto-split units around:

- `0x80011104`, `0x800115D0`
- `0x8002C49C`, `0x8002C53C`
- `0x80076684`, `0x800784E0`, `0x800785CC`
- `0x8007F580`, `0x800801EC` (`add_got_it`), `0x80080270` (`init_got_it`)
- `0x8008C0F4`
- `0x80091AC0`, `0x80091F34`, `0x800933BC`, `0x80094440`, `0x800948E8`

These should be investigated from their gauntworld call sites first.  The
argument values and surrounding state often provide a better naming anchor
than decompiling an auto unit in isolation.

## Recommended translation order

1. Continue the small and medium gauntworld functions to establish typed
   world-object and trigger records.
2. Deepen `game/world/items`; it is adjacent, bidirectional, and has the most
   direct calls from this range.
3. Finish the relevant `mb_tree` and `mb_util` seams so node ownership and
   transforms are reliable on a native backend.
4. Reconstruct the `enemy` spawn boundary and the `sounds_evt` dispatch
   boundary using the proven world-object types.
5. Use tower, camera, and message calls to recover high-level trigger/state
   meanings.
6. Assign the unresolved auto units only after their callers establish
   signatures and responsibilities.

## Matching progress in this pass

The first exact slice now includes two model/animation loader helpers, five
world-object transform helpers, a constant-result helper, an active-object
scan, and an item-state query.  These functions deliberately reuse the shared
`OBJGRP` and `Item` layouts from `include/game/item.h`; no duplicate anonymous
layout was added.

The next slice extends that boundary with the per-player world-name
initializer, the texture-mod update fan-out, a type-4 item relinker, and a
nearest-live-item query over `StartEnemyGrid`/`NextGridEnemy`.  The last helper
establishes the mixed-register ABI
`(f32 radius, f32* position, Item** result) -> f32 distance` and confirms that
the queried position is the first three floats of `OBJGRP.coll_pos`.

## Items follow-up

Following the first dependency edge into `game/world/items` established the
shared 0x6C-byte `LookoutParam` record.  Its next-link is at `0x68` and its
lookup id is at `0x6A`; both `FindLookoutParam` and `NextWaypoint` return record
pointers.  The older integer-return declarations were ABI-inaccurate.

The same pass recovered the milestone-position accessor, the four scripted
camera activation paths, the two item resource names, and the item runtime
reset.  This confirms that the item subsystem bridges three important
gauntworld responsibilities:

- level and boss-specific model/animation resources;
- linked lookout/waypoint navigation;
- crystal, Sumner, window, and rune camera triggers.

`LoadItems` is fully translated and one instruction longer than the target
because MWCC materializes a redundant address copy before saving the two
composite-data bases.  Its calls, fields, branches, relocations, and resource
ownership are otherwise reconstructed; retain the native-readable structure
unless a general scheduling technique removes that residual.

The item boundary now also includes the complete item-info/atree
initialization pass, the floor-probe and scene-parenting helper at
`0x80064154`, and the exact nearest-open start-position query.  These establish
that placed items are snapped with a `(height, radius, position, mode)` ground
probe, may be parented under the current scene root when its `0x1000` flag is
set, and use the same 14-entry world-open table as tower progression.

## MB scene-tree follow-up

The second dependency edge established the shared 0x80-byte MB tree-node tail:
render attributes at `0x40..0x6A`, parent at `0x74`, first child at `0x78`,
and next sibling at `0x7C`.  The translated propagation family now covers:

- flag set and clear;
- color-add and packed color;
- Z modification and inverse alpha;
- alternate texture, ambient-add, and UV indices;
- UV scale/add slot ownership;
- last-sibling and previous-sibling navigation.

Propagation mode `2` means “walk this sibling chain,” while a nonzero mode also
recurses into a first child unless that child has flag `0x10`.  This is the
scene-graph rule gauntworld and items rely on when hiding, showing, recoloring,
or retargeting an object subtree.

The lifecycle half of the same TU is now mapped from the Xbox PDB and
semantically translated:

- `fn_800BACF8` = `MBNodeOrder`
- `fn_800BAD94` = `MBNodeSetParent`
- `fn_800BAEAC` = `MBRemoveNode`
- `fn_800BB164` = recursive child removal (`MBRemoveNodeChild`)
- `fn_800BB4CC` = node insertion/attachment

`MBRemoveNode` is a particularly broad dependency: gauntworld, items, enemy,
player, sfx, particles, and several UI paths all use it.  Type-14 nodes with a
non-null field at `0x70` are not returned directly to the normal free list;
they are first moved under the special root at `lbl_80344EDC` and released
through `fn_800D12F0`.  Ordinary nodes are detached while preserving/promoting
their children, cleared, and pushed onto the free list at `lbl_80344EE0`.

## Grid dependency correction

The fixed candidate grid at `0x800433EC..0x80043A78` has misleading retail API
names.  Caller and link-field verification establishes:

- `StartEnemyGrid` / `NextGridEnemy` iterate `sItems` (stride `0xF0`);
- `StartItemGrid` / `NextGridItem` iterate `gEnemies` (stride `0x394`).

The shared declaration is now `include/game/dyngrid.h`, with the pool contract
documented explicitly.  This resolves the apparent contradiction between
gauntworld's nearest-item query and enemy's nearest-enemy query.

The adjacent dynamic world-object grid also no longer has a one-cell traversal
stub.  `NextDynGrid` is fully translated and opcode-identical to the 255-insn
retail function (relocation-label spelling aside).  It performs the swept DDA
used by `WorldDynCollide`, so native collision queries can now advance across
the complete set of crossed grid cells.
