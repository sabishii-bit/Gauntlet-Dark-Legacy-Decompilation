# SOUNDS event matching pass (2026-07-29)

## Verified exact gains

`sounds_evt.c` moved from 95/118 to 102/118 exact functions:

- `fn_8009C8F0`
- `fn_8009D100`
- `fn_8009D16C`
- `fn_8009D1D8`
- `fn_8009EEBC`
- `fn_8009EF04`
- `AudioPlayerSeverePain`

The full build moved from 1,985 / 352,508 exact functions/bytes to
1,992 / 353,288. `build/GUNE5D/main.dol` still passes the configured SHA-1
check.

## CodeWarrior techniques

- **Materialize complex call arguments in source order.** MWCC evaluates call
  arguments right-to-left. In `fn_8009C8F0`, assigning the two conditional
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
  field displacements.** The local table overlays in `fn_8009F550` and
  `fn_8009F860` make MWCC emit separate row/column shifts followed by
  `lwz field_disp(base)`. Flat expressions such as
  `table[row * 4 + col + K]` canonicalize to a combined index and `lwzx`.
- **Delete a derived player pointer when the target keeps the base temporary
  separate from its extracted fields.** Repeating the player-array
  expressions lets dominance CSE retain one address while giving the field
  and position values independent volatile registers.

## Remaining close residuals

- `fn_8009F550`: instruction-identical; one commutative `add` operand pair
  remains (2 real diff lines).
- `fn_8009F860`: instruction-identical and operand-identical; only a
  three-register coloring rotation remains (32 real diff lines).
- `fn_8009FD84`: reduced from 30 to 13 real diff lines. Its loads and call
  argument schedule now match; the compiler still folds one target
  `blt`/unconditional-`b` pair into a single inverted branch.
