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
