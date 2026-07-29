---
name: critter-matching
description: "critter.c exact-match recipes, ABI recovery, and parked near-match residuals"
metadata:
  node_type: memory
  type: project
---

# critter.c matching notes

Verified exact functions from the first implementation batch:

- `fn_80036740`
- `fn_80037D44`
- `fn_80037E80`
- `fn_80037ED0`
- `CritterLoadFile`

Reusable findings:

1. A scalar global loaded on every loop iteration in the target may need a TU-local
   `volatile` declaration. Declaring `sMusicFadeBase` as `extern volatile f32`
   prevented LICM and made `fn_80037E80` exact.
2. When the target emits both a standalone helper and multiple inlined copies, define
   a `static inline` implementation for callers but retain a direct standalone body.
   A wrapper that only returns the inline helper can perturb the standalone function's
   register allocation.
3. Recover call signatures before trying register-allocation tricks. `CritterLoadFile`
   actually takes `(wad, name)` and calls `AllocFile(wad, name)`. The missing first
   argument shifted the load counter from target `r5` to `r4`; correcting the ABI made
   the function exact.
4. Local declaration order controlled the nonvolatile allocation in `fn_80037ED0`.
   The exact order is `i`, `oldest`, `oldest_time`, `offset`.
5. Reusing the input index as the induction variable, paired with a separately
   incremented `Player *`, reproduced the target register webs in `fn_80036740`.
6. `gBig` is an overlay containing four scratch floats, `0x224` bytes of intervening
   state, then the 16-entry critter pool at offset `0x234`. Model this layout instead
   of treating the scratch array and pool as unrelated objects when target addressing
   is relative to `gBig`.

Near-match status:

- `fn_80036958` is fully translated at 64/64 instructions and differs only by an
  FP-register allocation swap (`real 8`). Its target-like control flow requires the
  explicit waypoint-search labels; ordinary structured loops regress substantially.
- `CritterDelInst` is 80/80 with a four-line `lwzu`/displacement peephole residual.
  Typed-node, array-index, scoped-temp, and peephole experiments did not improve it.
- `CritterEmptyInst` has the correct pool overlay and semantics but remains a larger
  allocation/addressing residual. Do not discard the three-argument overflow
  `ErrorPrintf(format, index, active_count)` recovered from Ghidra.
