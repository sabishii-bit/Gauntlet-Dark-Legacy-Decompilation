# SOUNDS event matching pass (2026-07-29)

## Verified exact gains

`sounds_evt.c` moved from 95/118 to 102/118 exact functions:

- `AudioExplodeWall`
- `AudioBridgeOpen`
- `AudioBridgeClose`
- `AudioWorldObjectMotion`
- `AudioClick`
- `AudioClick2`
- `AudioPlayerSeverePain`

The full build moved from 1,985 / 352,508 exact functions/bytes to
1,992 / 353,288. `build/GUNE5D/main.dol` still passes the configured SHA-1
check.

## CodeWarrior techniques

- **Materialize complex call arguments in source order.** MWCC evaluates call
  arguments right-to-left. In `AudioExplodeWall`, assigning the two conditional
  halfword arguments to `atten` and `priority` before the call gives the
  target load/branch order. The equivalent inline ternaries in the call swap
  the two code blocks.
- **A one-case switch preserves an otherwise folded branch pair.** The three
  D100/D16C/D1D8 gates need `cmpwi; beq body; b exit`. A nested
  `switch (gate) { case 0: <real body>; }` emits that shape; the equivalent
  chained `if` folds to one `bne`.
- **Assignment in the condition pins a call-crossing value before compare.**
  `if ((id = table[index]) >= 0)` makes MWCC load the sound ID directly into
  its nonvolatile register and compare that register. A declaration
  initializer followed by `if (id >= 0)` loaded through `r0` and inserted an
  extra `mr` after the compare. This made `AudioPlayerSeverePain` exact.
- **Use a truly typed pointer, not a cast at the access site, to preserve
  field displacements.** The local table overlays in `AudioPlayerTurbo` and
  `AudioPlayerEatFood` make MWCC emit separate row/column shifts followed by
  `lwz field_disp(base)`. Flat expressions such as
  `table[row * 4 + col + K]` canonicalize to a combined index and `lwzx`.
- **Delete a derived player pointer when the target keeps the base temporary
  separate from its extracted fields.** Repeating the player-array
  expressions lets dominance CSE retain one address while giving the field
  and position values independent volatile registers.
- **Cast the array base to an integer when a final commutative `add` is
  reversed.** In `AudioTurboDefense`, materializing the byte offset and writing
  `(u32)gPlayers + offset + field` preserves the target's base-plus-offset
  operand order. Pointer indexing canonicalized the same address as
  offset-plus-base. The integer form made the full 128-byte function exact.

## Remaining close residuals

- `AudioPlayerTurbo`: instruction-identical; one commutative `add` operand pair
  remains (2 real diff lines).
- `AudioPlayerEatFood`: instruction-identical and operand-identical; only a
  three-register coloring rotation remains (32 real diff lines).
- `AudioEnterNextStage`: reduced from 30 to 13 real diff lines. Its loads and call
  argument schedule now match; the compiler still folds one target
  `blt`/unconditional-`b` pair into a single inverted branch.

## Recovered identities

The Xbox PDB names, GameCube callers, item subtype enum, and adjacent sound-ID
tables jointly identify the remaining generic symbols:

- `8009C8F0` -> `AudioExplodeWall` (called for `SUB_WALL`)
- `8009D100` / `8009D16C` -> `AudioBridgeOpen` / `AudioBridgeClose`
  (bridge pad/switch state transitions)
- `8009D1D8` -> `AudioWorldObjectMotion` (shared rotator/elevator motion table;
  kept conservative because the Xbox binary splits related behaviors)
- `8009EEBC` / `8009EF04` -> `AudioClick` / `AudioClick2`
- `8009F550` -> `AudioPlayerTurbo`
- `8009F860` -> `AudioPlayerEatFood`
- `8009FD84` -> `AudioEnterNextStage`
- `8009F638` -> `AudioPlayerEatSFX` (player-motion caller and food/eating
  effect selection)
- `8009FBD4` -> `AudioNumRunesFound` (the item dispatcher passes the
  population count of the 13-bit rune mask)
- `8009F4D0` -> `AudioTurboDefense` (player defensive-power flags select the
  three turbo-defense sound IDs)

## Additional source-shape progress

- Replacing `AudioPlayerEatSFX`'s long-lived derived player pointer with an
  explicit byte offset and re-derived pointer restored the target's complete
  68-instruction structure. Its residual is now register coloring only.
- Materializing `AudioTurboDefense`'s offset once and using integer base
  arithmetic for both field addresses resolved the final commutative-register
  residual; the function is now byte-exact.
- Laying out the invalid-entry arm explicitly in `AudioEnterNextStage` reduced
  that structural residual from 13 to eight real lines.
