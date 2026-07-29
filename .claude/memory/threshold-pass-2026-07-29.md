# 33.33% threshold pass (2026-07-29)

Additional exact-match source tells:

- `DoExit`: a one-case `switch` preserves MWCC's retail
  `beq case; b end` pair. An equivalent combined `&&` or `goto` condition
  folds to a single `bne`, leaving the function one instruction short.
- `WorldExplosion`: independent declaration-order swaps fixed both allocator
  cycles. Declaring the branch-specific float before the default float swaps
  `f30/f31`; declaring the call result before damage swaps `r30/r31`.
- `hide_rune_stones`: moving the inner-loop counter declaration before the
  accumulator swaps their volatile register webs without changing code shape.
- `player_can_be_damaged`: spelling the final success case as a nested test
  with a success label preserves the retail return-block order. Use `u32` for
  the zero test to select `cmplwi`, not signed `cmpwi`.

These complement the player/select declaration-order recipe: when an opcode
stream is already identical, permuting declarations is effective for both
volatile and nonvolatile web cycles, while preserving statement order.
