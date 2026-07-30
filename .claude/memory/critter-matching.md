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
- `CritterNoHit` (`fn_80037D44` before the PDB rename)
- `CritterNoHitSub` (`fn_80037E80` before the PDB rename)
- `fn_80037ED0`
- `CritterLoadFile`

Reusable findings:

1. A scalar global loaded on every loop iteration in the target may need a TU-local
   `volatile` declaration. Declaring `sMusicFadeBase` as `extern volatile f32`
   prevented LICM and made `CritterNoHitSub` exact.
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
- `fn_80037D34` -> `CritterCollideStart`
- `fn_80037D44` -> `CritterNoHit`
- `fn_80037E80` -> `CritterNoHitSub`
- `fn_8003B1CC` -> `CritterMoveSetup`
- `fn_8003C8D4` -> `CritterGetDmove`

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

## Third pass (2026-07-29)

Verified exact additions:

- `CritterInsertTarget` (0xE4)
- `CritterDoKnockback` (0x15C; linked bytes exact, object diff is pool-name
  relocation noise only)
- `CritterAddHealthMeter` (0x150)
- `CritterLookForReady` (0x15C)

Reusable findings:

11. `CritterInsertTarget` is a four-entry insertion sort over a real 0x24-byte
    record, not byte-copy boilerplate. Modeling `CritterTargetRecord` and using
    whole-struct assignments makes MWCC emit the retail copy sequence. Hoist
    `target->distance` into a local before the scan; this moves the `lfs` to the
    retail position and closes the last scheduling residual.
12. In `CritterDoKnockback`, load the double clamp magnitude into one local after
    `NormalVector` and reuse it for all three vector components. Repeating the
    external constant expression makes MWCC reload it three times; the one local
    reproduces the retail single `lfd` and all three multiply/round stores.
13. An external string known to live in `.sdata2` needs its section stated on the
    declaration (`DECL_SECT(".sdata2") extern const char ...`). Merely making the
    declaration `const` still selects `lis/addi`; the section-qualified declaration
    produces the retail SDA21 `li`.
14. The first health-fill coordinate in `CritterAddHealthMeter` needs one named
    root-node pointer followed by a direct field RMW. Adding a second named `f32 *`
    does reproduce the desired address instruction but inflates the debug frame by
    eight bytes. One root local plus
    `*(f32 *)((u8 *)root + 0x30) = ...` gives both the retail `addi`/store shape and
    the 0x18-byte frame.
15. Recover signed byte fields from `lbz` followed by `extsb`. The critter child
    counters at 0x44E/0x44F are `s8`, which is needed by critical-move selection.
16. For a dense loop whose opcode stream is exact but five or more nonvolatile
    integer webs are rotated, list the desired colors from low to high and declare
    the corresponding locals in reverse order. `CritterLookForReady` needed
    `timeOffset, moveOffset, i, moves, result, moveCount` to produce retail
    `r25..r31` ownership around the fixed `c` parameter web.
17. A literal `0.0f` and a named zero-valued `.sdata2` float are not interchangeable
    at the allocator endgame. In `CritterLookForReady`, assigning
    `zeroFloat = lbl_80346470` forced the retail early `lfs`, which in turn colored
    `best/zeroFloat/zeroDouble` as `f29/f30/f31` and made all 87 instructions exact.
18. For nested array fields, keep a typed base and write both subscripts in the
    access (`patterns[patternIndex].sequence[step]`). An intermediate pointer to one
    pattern folds the `+0x22` field displacement into the index and selects `lhax`;
    the two-subscript form emits the retail base add followed by `lha 0x22(base)`.
19. Adjacent saved FPRs can follow reverse declaration order even in a 200+
    instruction function. Declaring the double `zero` before the float `best`
    changed `CritterChildCriticalMove` from `best=f31/zero=f30` to the retail
    `best=f30/zero=f31` without changing its instruction stream.

New fully translated near matches:

- `CritterLineNodeColSub` (formerly `fn_80037C08`) is 75/75 opcode-identical;
  the remaining 12 lines are one three-way saved-FPR color rotation.
- `CritterLookForCriticalMove` (formerly `fn_8003BAFC`) is 75/75
  opcode-identical; applying the reverse-declaration rule reduced the remaining
  nonvolatile GPR rotation from 28 to 24 real lines.
- `CritterMoveSetup` (`fn_8003B1CC`) is fully translated and one
  branch-shape instruction away from
  the 77-instruction retail body.
- `CritterProcessSafeRocks` is fully translated at 63/63 instructions; only three
  address-canonicalization sites remain (12 real diff lines).
- `CritterChildCriticalMove` (formerly `fn_8003B7D8`) now has its full two-pass
  201/201-instruction body and typed 0x50-byte `CritterPattern` records. Three
  second-loop initialization instructions remain structurally different; the
  rest of its residual is nonvolatile register coloring.

## Full-stub recovery pass (2026-07-29)

20. The Ghidra MCP REST endpoint can return a whole decompilation when the normal
    tool result is too large to iterate comfortably:
    `http://127.0.0.1:8089/decompile_function?address=0x...&program=main.dol`.
    Keep the address explicit and verify the recovered function against
    `tools/gdl/fnasm.py`/`fndiff.py`; Ghidra frequently mistakes a floating return
    for `void` around MWCC `_savefpr_*` prologues.
21. Rename a recovered function and every typed call site in the same change.
    Leaving `CritterLookForReady` calling the obsolete undeclared
    `fn_800372A0` made MWCC insert an integer-to-float conversion and regressed an
    exact 87-instruction function to 94 instructions. Calling the mapped
    `CritterCalcTarget` prototype restored the exact body.
22. Xbox symbol names are best assigned by combining behavior and relative size,
    not object order. Xbox and GameCube CRITTER.OBJ arrange several large regions
    in opposite order. For example, GC `0x80036B5C` rescans a populated target
    record (`CritterReCalcTarget`), while `0x800372A0` builds and scores a target
    record (`CritterCalcTarget`).
23. For serialized WAD headers, first express endian conversion as loops over
    record arrays and typed-width field swaps. This gives a reviewable native
    translation even though the retail GameCube compiler unrolled much of the
    conversion. Be especially careful at mixed-width boundaries: the damage
    record has `u32` fields only through `0x3C`, followed by `u16` fields at
    `0x40..0x46`.
24. Never pass the address of adjacent scalar locals to a vector helper. MWCC may
    place them in a different order, and native C does not guarantee adjacency.
    `CritterCollidePlayers` uses an explicit three-float separation vector before
    calling `NormalVector2D`.

The full recovery pass replaced every remaining empty CRITTER.OBJ skeleton with a
compiling behavioral translation and mapped the high-confidence Xbox names for
collision, target selection, movement collision, action/animation, instance, and
loader functions. `fndiff.py --classify` reports no target-only functions. The TU
must remain `NonMatching`: most recovered bodies still need ordinary compiler-match
iteration, but there are no longer missing C implementations blocking native-port
work.
