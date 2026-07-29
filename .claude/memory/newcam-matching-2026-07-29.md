# NEWCAM matching: 2026-07-29

## Exact gain and recovered name

- `0x80070340` is the Xbox `NEWCAM.OBJ` function `CamLookInDir`.
- `CamLookInDir` is exact (428 bytes; only normalized private-pool names remain
  in the object comparison).

## Source-shape technique

The last mismatch was `mr r31,r4` in retail versus `addi r31,r4,0` in the
reconstruction. The function's second argument is an address, but declaring it
as `f32 *` makes MWCC treat the local copy as pointer arithmetic and select
`addi`. Declaring the ABI argument as `u32`, then casting it once to the local
`f32 *`, selects the retail integer-register move without changing any later
pointer arithmetic or register coloring.

Keep the local declaration order `m`, `up`, `fwd`: removing the `m` local or
using the parameter directly rotates all three nonvolatile registers and moves
the cross-product blocks.
