# ITEMS symbol-recovery technique

For mixed GameCube/Xbox TUs, do not align names by function address order alone:
the retail GC linker reordered large portions of `ITEMS.OBJ`.

The reliable sequence used for `items.c` was:

1. Extract the complete module roster and globals from
   `research/xbox_symbols/functions_by_module.txt`.
2. Match small functions by semantic fingerprints (teardown, player-distance
   scan, generator gate, safe-rock state transitions), then validate every
   candidate from all callers in Ghidra.
3. Read the DOL bytes for string/constant labels. This immediately identified
   `KEYRING`, `CHESTSG`, `SEETHRU`, camera formats, and the lighting constants.
4. Use full data xref sets for BSS/SBSS. Writers identify table counts and
   singleton slots much more reliably than consumers.
5. Keep exact PDB names distinct from GC-only behavioral names. The latter are
   still useful, but should be explicitly documented as descriptive.

The complete old-to-new map and evidence are in
`research/items_symbol_map.md`.

## Matching continuation (2026-07-28)

- Put recovered functions at their target position in the source. Adding
  `DistanceToClosestPlayer` before `generate_now` disturbed later TU-local
  constant allocation; its real slot is between `generate_now` and
  `did_generate`.
- Reuse an already-matched `frsqrte` Newton sequence rather than translating
  Ghidra's `SQRT` expression literally. Three refinement steps plus the final
  single-precision spill/reload reproduce both distance paths.
- A signed comparison with `0x8008` emits MWCC's target
  `addis ...,0; cmplwi ...,32776`. Keep the canonical `u32 gGameMode`
  declaration and cast only at that comparison so other functions retain
  their established ABI.
- Ghidra can infer a zero-argument function when the real input survives in
  `r3`. `update_player_milestone` immediately copies `r3` to a saved register,
  proving that it takes the active player pointer despite the old header.
- Recover stack layout from live offsets, not frame size alone. A local overlay
  with the absolute-Y scratch at `sp+24`, twelve dead bytes, and the player
  position at `sp+40` produced the target 96-byte frame.
- Keep both milestone tolerance doubles live across the loop. This selects
  `f30/f31` and MWCC's `_savefpr_30`/`_restfpr_30` helpers, bringing the
  reconstructed 0x1EC-byte routine to the target instruction count.
- Preserve a raw signed-byte value separately from its adjusted copy.
  `ItemVisible` needs the raw `minplayers` value for the `> 10` test and the
  copied value for subtracting ten; that recovers the target `r3 -> r5`
  materialization and leaves only boolean-return coloring.

## Item-instance / WOBJ continuation (2026-07-28)

- When a BSS blob is accessed at fixed 600-byte intervals, test a
  struct-of-arrays interpretation before accepting anonymous byte offsets.
  `RegisterItemWobj` exposed five consecutive 150-float arrays at offsets
  `0x000`, `0x258`, `0x4B0`, `0x708`, and `0x960`, plus the target-pointer
  array at `0x7220`. Typed fields recovered the target's `add` + `lfsu` /
  `stfsx` addressing pattern.
- Pay attention to parameter widths even when the caller already extends the
  register. Declaring the WOBJ trigger type as `s16` removed MWCC's otherwise
  redundant `extsh`; writing the count test as
  `if (++sNumItemWobjs >= 150)` recovered the target compare-before-store
  schedule. Together these brought the routine to 139/139 instructions.
- Use the Xbox PDB for serialized input records, then verify every field against
  the GC loads before adopting it. The 0x3C `iteminst` layout explains all
  `AddItemInstList` offsets and constrains `SetItem`'s real second argument to
  an instance pointer rather than the old integer “flag” placeholder.
- A target opcode sequence can expose dead retail logic. `LinkItemTriggers`
  contains five instructions that compare candidate `next_id` with current
  `id`, then immediately overwrites CR0 with the loop comparison. Keep this
  documented, but do not add PPC inline assembly merely to reproduce a
  semantically dead comparison when native portability is the project goal.

## Generator placement continuation (2026-07-28)

- Caller register lifetime can recover a missing static function signature.
  The large item-processing routine preserves the new enemy index in `r4`
  while replacing only `r3` with the generator payload before calling
  `place_logic12`; this proves the real two-argument ABI despite Ghidra's
  decompiler losing both parameters around `_savefpr_29`.
- For `generate_single`, the target position is `OBJGRP.coll_pos`, not the
  world-matrix translation. Its direction is `worldmat[2][0..2]`, and the
  final floating argument to `generate_enemy` is `iteminfo.radius`. Typing
  those fields and loading the radius into a local before filling the vectors
  produced an exact 92/92-instruction match.
- Stack-array declaration order is effectively reversed by this MWCC build.
  `place_logic12` needed matrix, 12 dead bytes, transformed vector, angles,
  input vector, then four dead bytes in source order to recover target stack
  offsets `+68`, `+44`, `+32`, and `+20` and the 176-byte frame.
- A compound double update can remove an unwanted floating move:
  `angle = half_pi; angle += generator_angle;` emits the target
  `fadd f2,f2,f1`, whereas one expression selected `f1` and inserted
  `fmr f2,f1`. Together with branch-specific X/Z temporary coloring this
  brought `place_logic12` to an exact 166/166 instructions.

## Locator expansion continuation (2026-07-28)

- `AddLocatorInstList` is zero-argument. It reads `gWorldInfo.locators`
  (`+0x70`) and `gWorldInfo.nlocators` (`+0x7C`) directly; the old
  list/count prototype was an ABI placeholder.
- The 0x1C serialized `locator` record is type/subtype/index followed by
  `pos[3]` and `pyr[3]`. The type switch maps 1/2/3/4/9 to trigger-camera
  variants, 5 to milestones, 6 to boss matrices, 7 to player starts, and
  8/10 to lookout waypoints.
- The formerly opaque middle of `sItemRuntime` is one contiguous typed arena:
  a 32-byte load path, 14 start yaws, 14 start positions, 20 lookouts,
  42 Sumner-camera pointers, 17 rune/transmitter pointers, 256
  `TriggerCamera` records, and 128 milestone records. The final twelve-byte
  pad places the existing WOBJ target table back at `+0x7220`.
- Three source bodies that look duplicated are the Xbox PDB's
  `AddTransmitter` logic inlined by the GC optimizer. A C preprocessor macro
  is the portable way to reproduce the repeated body; a `static` helper was
  not inlined by MWCC and left an unwanted base function.
- Express bounded counter updates as `if (++count > limit)`: MWCC emits the
  target compare-before-store schedule and avoids a store/reload pair. Keep
  cases 9, 5, 6, and 7 in target code order after the merged 1/2 and 3/4
  fallthrough blocks, because switch body order materially affects diff
  alignment even when the jump table is equivalent.

## SetItem constructor continuation (2026-07-28)

- A decompiler may misidentify a helper from the surrounding arithmetic.
  SetItem's negative door-delay path doubles and negates the delay, calls
  `0x800BCCA8`, then adds half the range. The symbol map proves that callee is
  `RandInt`, not a square-root or absolute-value helper.
- Recover hidden floating arguments from the call-site registers. The
  `FindWorldAnimNode` call loads `r3 = &matrix[12]` and `f1 = 10.0f`, matching
  the translated world.c API even though Ghidra rendered it as a zero-argument
  call.
- Treat decompiler booleans that survive a large switch as control-flow state.
  SetItem's geometry flag begins true, is cleared for ordinary critter and
  world-animation items, and is re-enabled only when a special statue animation
  tree is found. Leaving it true produced plausible code but attached the wrong
  geometry at runtime.
- Follow nested loads literally. A loaded critter's animation-tree header is
  `*(loaded + 0x120)` followed by `*(header + 0x28)`; collapsing the offsets
  into `*(loaded + 0x148)` crosses a pointer indirection and is semantically
  wrong.
- Recover the original lexical case order from the jump table's destination
  addresses, not from Ghidra's numerically sorted switch. SetItem's type table
  maps 1..13 to bodies whose ascending-address order is
  `4,2,5,12,3,8,9,11,13,10,1,7`. Reordering the C cases to that sequence
  changed no behavior, but raised SetItem from 21.83% to 72.24% fuzzy and the
  whole TU from 64.90% to 78.95%. This is a high-leverage first step for every
  large MWCC switch.
- Preserve compiler-visible pointer bases when the target hoists them into
  callee-saved registers. Keeping locals for `&gWorldInfo.iteminfo`,
  `sItemRuntime`, and the arrow-name table, and using `instance->params`
  directly instead of a nullable convenience pointer, removed substantial
  preamble and cross-case register drift.
- The KEYRING replacement search uses the generic item-info predicate
  `candidate.type == type && (subtype < 1 || candidate.subtype == subtype)`.
  Its not-found path is a fallthrough assignment to `-1`; expressing the
  successful exit with a label avoids an extra post-loop comparison and
  matches the retail control flow.
- For MWCC's inlined distance calculations, split the squared length into two
  assignments instead of one three-term expression. In `FindClosestWaypoint`,
  declaring `d2, dx, dy, dz` before assigning Y/X/Z, then evaluating
  `d2 = dx*dx + dy*dy; d2 = dz*dz + d2;` recovered the target's exact
  65-instruction size and reduced the meaningful diff to 18 lines. Dead stack
  arrays still matter: an additional eight-byte local recovered the target
  48-byte frame even though it is never referenced.
- Stack padding can recover both frame size and array placement independently.
  `AddItemInstList` needed four dead bytes declared after its matrix to move
  the matrix from `sp+8` to the target `sp+12`, then eight dead bytes declared
  before the matrix to grow the frame from 120 to 128 without moving it again.
  This reduced that function from 96 to 71 meaningful diff lines.
- Do not “clean up” adjacent-small-data aliases without checking the complete
  opcode sequence. `gControllerButtons` at `0x803445C8` is intentionally
  declared as 64-bit in `items.c`: its low word aliases `sFlags` at
  `0x803445CC`, and MWCC emits the retail pair of word loads plus 64-bit
  boolean operations. Replacing the expression with a direct `sFlags` test
  looks semantically obvious but removes seven target instructions.
- Squared distances formed from `f32` components should generally stay `f32`
  until the reciprocal-square-root refinement. In `DistanceToClosestPlayer`,
  changing the accumulator from `f64` to `f32`, declaring it before X/Y/Z,
  and splitting the sum after two terms removed two register moves, recovered
  the exact 122-instruction size, and cut the meaningful diff from 108 to 84.
- A dead array declared after a compound stack local shifts the whole compound
  upward. An eight-byte array after `update_player_milestone`'s position/abs-Y
  struct moved it from `sp+16` to the target `sp+24` while growing the frame
  from 88 to the target 96 bytes; meaningful diff fell from 82 to 58.
- Sparse inner decision trees should also be tested as switches. Replacing
  SetItem's subtype 2/12/15 chain and its statue-mode 0..3 chain with switches
  recovered MWCC's target branch topology and fallthrough sharing.
- Split dead stack padding across independent lexical blocks when both high and
  low temporary slots need alignment. SetItem uses one pad near its name arrays
  and another beside its absolute-value temporaries; the combined frame size is
  unchanged, but the compiler places both conversion spills at their retail
  offsets.
- Do not hoist duplicated calls merely because the decompiler merges their
  results. SetItem's subtype-29 tree performs separate `AtreeMatch` paths for a
  global header and a loaded critter header. Preserving that duplication
  substantially improves code generation and reflects the original source.
- Serialized fields must retain their load width until the destination narrows
  them. A type-2 parameter is loaded with `lwz` and only then truncated by
  `extsh`; spelling it as an `s16` parameter load silently loses both the
  original access semantics and target instructions.
- Pointer-shaped data tables can still contain packed integer defaults.
  SetItem's arrow table is typed as `char**`, but offsets `+0x30` and `+0x3c`
  are word-stride signed/unsigned byte defaults. Indexing it as an `s32` table
  recovered the target `slwi`/`lwz` sequence; byte-pointer indexing produced
  incorrect `lbz` accesses.
- Apparently dead accumulators can expose omitted source behavior. The tower
  trigger loop ORs a per-player halfword at `player + 8738 + index*240` in
  addition to calling `towerGetLevelFlag`. Restoring that read recovered the
  target loop exactly enough to remove a large structural diff, even though
  the accumulated value is not subsequently tested in retail code.
