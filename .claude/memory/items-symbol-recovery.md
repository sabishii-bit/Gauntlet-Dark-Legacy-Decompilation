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
