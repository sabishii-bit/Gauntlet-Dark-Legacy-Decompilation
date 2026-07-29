---
name: critter-matching
description: "critter.c exact-match recipes, ABI recovery, and parked near-match residuals"
metadata:
  node_type: memory
  type: project
---

# critter.c matching notes

Verified exact functions from the first implementation batch:

- `CritterAwardExp` (`fn_80036740` before the PDB rename)
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
7. A complete typed overlay can suppress an unwanted update-form field load. For
   `CritterDelInst`, changing the opaque subnode pointer into a struct with fields at
   `0x00`, `0x48`, and `0x50` made MWCC emit the target `lwz 72(r29)` plus
   `stw 72(r29)` instead of `lwzu 72(r29)` plus `stw 0(r29)`. Earlier partial
   typed-node experiments had missed the exact layout/lifetime combination.
8. `CritterSetFxHitTime` needs two simultaneous source shapes: keep `&gBig` in a local
   `CritterBigState *big`, use a temporary `Critter *c` while scanning, then perform
   the final indexed writes through `big->pool[i]`. This makes MWCC retain and reuse
   the target `gBig` base register while still forming the scan pointer at offset
   `0x234`; the function then matches all 31 instructions.
9. Small scheduling residuals can depend on expression-level dataflow, not just
   statement order. In `CritterUpdateCounters`, the second expiry timestamp had to be
   loaded with `current = *(counterTime = (f32 *)(base + 0x1C8));`. The value-producing
   pointer assignment forces the target `addi` before `lfs` schedule and closes the
   final two-line diff.
10. For counter loops, explicitly keep long-lived constants (`zero`, `timeout`,
    `clear`) and a short-lived `current` value. This reproduces the target FPR web
    (`f3`, `f2`, `f1`, then `f4`) much more reliably than repeating literal
    expressions. The recovered thresholds in `CritterUpdateCounters` are 3.0 seconds
    for the move/event counter and 15.0 seconds for each effect-counter timestamp.

Verified exact functions added in the second pass:

- `CritterSetFxHitTime` (0x7C)
- `CritterUpdateCounters` (0xDC)

High-confidence Xbox-PDB/Ghidra symbol cleanup from this pass:

- `fn_80036740` -> `CritterAwardExp`
- `fn_800367CC` -> `CritterDamagePlayer`
- `fn_800371BC` -> `CritterInsertTarget`
- `fn_8003A73C` -> `CritterProcessSafeRocks`
- `fn_8003DE70` -> `CritterDoParticle`
- `fn_8003FF98` -> `CritterInitSfx`
- `0x8003DC64`, previously mislabeled `CritterDoParticle`, -> `CritterDoSfxSub`

Near-match status:

- `CritterGetTarget` is fully translated at 64/64 instructions and differs only by an
  FP-register allocation swap (`real 8`). Its target-like control flow requires the
  explicit waypoint-search labels; ordinary structured loops regress substantially.
- `ProcessCritterList` is now 49/50 instructions with 29 real diff lines. Iterating a
  local `Player *` instead of indexing `gPlayers[i]` removes one induction web and is
  materially closer, but the remaining prologue/register initialization does not yet
  match.
- `CritterDelInst` is exact after the complete typed-subnode overlay described above.
- `CritterEmptyInst` has the correct pool overlay and semantics but remains a larger
  allocation/addressing residual. Do not discard the three-argument overflow
  `ErrorPrintf(format, index, active_count)` recovered from Ghidra.
