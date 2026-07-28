---
name: recorder-tu
description: "RECORDER.OBJ layout and the sibling-BSS reconstruction that makes all four functions byte-match"
---

# RECORDER.OBJ

`recorder.c` is a strong example of why one large convenience struct can be
semantically correct but codegen-wrong.  Ghidra's unnamed data addresses show
that the `0x1F38`-byte recorder snapshot was compiled as separate, contiguous
TU-local arrays:

| Offset | Shape | Purpose |
|---:|---:|---|
| `0x0000` | `u8[0x18C]` | camera backup |
| `0x018C` | `f32[3]` | camera position |
| `0x0198` | `f32[7][3]` | reticle positions |
| `0x01EC` | `f32[11]` | seven depths plus the `0x10` gap |
| `0x0218` | `u8[152]` | 150 player byte-17 values plus alignment |
| `0x02B0` | `u8[152]` | 150 player byte-16 values plus alignment |
| `0x0348` | `s32[1024]` | item types |
| `0x1348` | `s16[1520]` | 1024 item states plus the `0x3E0` gap |
| `0x1F28` | `f32[4]` | saved player position plus trailing alignment |

Defining these sibling arrays in the TU, in address order, makes MWCC pool
their addressing off the first symbol.  This recovers the target's
`add anchor,index` plus large-displacement loads/stores; field accesses on a
single `RecorderState` instead reassociate the displacement into the index
and emit `lwzx/stwx`.

Useful follow-on details:

- Fold otherwise anonymous gaps into the preceding referenced array.  This
  keeps every byte live without dummy padding symbols being optimized away.
- A fresh block-local counter for the player-copy loop is codegen-significant.
  Reusing the item-loop counter rotates the final two volatile registers.
- Repeating `table[index]` in `LoadAllRecords` intentionally reloads the
  player pointer twice, matching the target's alias-conservative code.
- The recorder `.sbss` is `0x40` bytes at `80344B50..80344B90`.  Explicit
  scalar zero initializers keep the first fourteen globals in declaration
  order; the final uninitialized `s32[2]` remains an 8-byte `.sbss` object.
