---
name: near-match-batch-2026-07-29
description: "Five exact functions across pmotion, player, and audio; proven source-shape fixes"
metadata:
  node_type: memory
  type: project
---

# Near-match batch: 2026-07-29

Verified exact/link-equivalent functions:

- `PlayerMotion_FindClosestPlayer`
- `heal_player`
- `change_player`
- `PlayerSelecting`
- `AudioDeferSlot`

Project report delta: 1969 -> 1974 exact functions and 350392 -> 351172
matched code bytes. Full DOL SHA-1 remained green.

Proven source-shape findings:

1. A typed parameter can eliminate an otherwise unexplained eight-byte frame
   reservation. `heal_player(f32, void*)` plus `Player* p = vp` produced a
   32-byte frame; declaring the ABI-equivalent parameter as `Player* p`
   produced the target 24-byte frame and exact code.
2. For an expression with two commutative dot-product terms, MWCC may schedule
   the right term first. Swapping the first two source terms in
   `PlayerMotion_FindClosestPlayer` reproduced the target FPR/load order.
3. Equivalent boolean forms are not layout-equivalent. Writing
   `state == 2 || state == 3` produced the target shared success return in
   `PlayerSelecting`; the inverted `state != 2 && state != 3` put the failure
   return first.
4. Target `cmpwi 8; blt` came from `value >= 8`, not the equivalent
   `value > 7`. Together with an eight-byte unused frame pad, this made
   `change_player` exact.
5. A one-case `switch` is a reliable way to retain the target's unfolded
   `beq case; b end` pair. It made `AudioDeferSlot` exact where positive and
   negative `if` forms both folded to one `bne`.

Parked/restored during this pass:

- `MBPsysSetEVolume`: frame remains eight bytes too large across named-local,
  parameter-reuse, explicit-cast, and implicit-cast forms.
- `fn_8009DB24` / `AudioExplodeWall`: pan/flags evaluation order remains an
  allocator/inlining-source residual.
- `fn_80055CB8`, `AudioGetSoundVol`, `sysClearFlags`: pure destination-register
  choice; source-equivalent attempts were neutral.

## Enemy / gamemain / pmotion continuation

Verified exact gains:

- `do_ai` (192 instructions / 768 bytes)
- `LevelLetter` (18 instructions / 72 bytes)

The project moved from 1,992 / 353,288 exact functions/bytes to
1,994 / 354,128. The full DOL SHA-1 remained green.

New source-shape findings:

1. **Combine assignment with the first comparison when an FP value must stay
   in its arithmetic register.** In `do_ai`, `f64 a = e->ang; if (a > PI)`
   loaded through `f0` and inserted `fmr f1,f0`. Splitting the declaration and
   writing `if ((a = e->ang) > PI)` loaded directly into target `f1`, removing
   the copy and the four-byte branch-offset cascade across the dispatcher.
2. **Model interior bytes as typed fields, but preserve signedness at each use
   site.** Adding unsigned `WorldDataType::letter` at offset `0x0F` gave
   `LevelLetter` the target `mulli r0` / base-in-`r4` address shape and made it
   exact. The parser needs `(s8)table[i].letter` explicitly; retaining that
   cast also reduced `fn_80057B30` from 12 to five residual lines.
3. **A deliberately indexed inline scratch array can place an inlined scalar
   home without changing instructions.** Replacing `fabsf_`'s scalar home with
   `f32 slots[3]` and using `slots[2]`, while shrinking the caller's dead pad
   from 24 to eight bytes, moved `PlayerNewFloor`'s bit-clear scratch from
   stack offset 28 to target offset 40. The function is now register-only
   with four commutative-operand lines remaining.
