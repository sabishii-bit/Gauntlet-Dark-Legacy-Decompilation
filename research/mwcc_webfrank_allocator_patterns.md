# MWCC GC/1.2.5 WebFrank allocator-pattern audit

Date: 2026-08-25

This audit asks whether a name-independent, semantics-preserving compiler
policy can replace a useful class of GDL's WebFrank rules. It does not change
game source or a production compiler. All compiler observations use the exact
stock GC/1.2.5 executable with SHA-256
`0443b5c02b1aa7b575b61e0e24c4d5ad6bed8fd54cc42de5a2204a5216001914`.

## Static and dynamic allocator facts

The recovered GC/1.2.5 pipeline is:

1. `MWCC_SpillCode_BuildInterference` (`0x00530A00`) constructs liveness and
   interference, coalesces copies, and materializes graph nodes.
2. `MWCC_SpillCode_CoalesceCopies` (`0x00530E00`) uses lower-vreg union roots.
   Two nonphysical roots may merge only inside the class's half-open
   coalescing window and only if they do not interfere.
3. `MWCC_Coloring_SimplifyGraph` (`0x004CE400`) scans virtual-register IDs in
   ascending order for nodes below the color-count degree threshold and pushes
   removed nodes on a LIFO list. It uses spill cost/degree only when no
   low-degree node qualifies.
4. `MWCC_Coloring_SelectColors` (`0x004CE2D0`) pops that list, masks colors
   already used by colored neighbors, then scans the mask from bit zero upward.
   The exact decision at `0x004CE381` is therefore the lowest available
   physical color. `MWCC_Coloring_CommitAssignments` (`0x004CE1A0`) only
   rewrites operands and object metadata; it does not choose colors.

The captures came from `monde-lointain/mwcc-debugger` over retrowin32/GDB.
The raw and target function bytes were independently re-read through the
SHA-aware ELF parser used by `webfrank_audit.py`; "words/fields" below counts
changed instruction words and individual five-bit PPC register slots.

## Cross-function comparison

| Function | Raw proof | Current preallocation/web evidence | Required target allocation | First decisive mismatch and likely layer |
| --- | --- | --- | --- | --- |
| `CritterResolveMultipleTargets` | 111 instructions; raw `aa900ce7...a108cb`; target `c9a716df...f11458`; 62 words/93 fields; fuzzy 95.72072, real 126 | Windows GPR `43..71`, FPR `34..44`; every captured GPR `r32..r73` and FPR `f32..f43` parent is identity. Simplify/select lists are descending vreg. | Broad GPR/FPR recoloring; first FPR divergence is stock `f36->f3`, target `f4`. | At the first meaningful GPR pop, `r72` has degree 21 and available mask `0x1ff0`; stock correctly picks lowest bit `r4`, but target needs `r6`. Because no parent merged and `r4/r5` are free in the current state, the target must have different earlier-colored neighbors, interference, or simplify order. Not commit-time renaming and not a coalescing-parent failure. |
| `CritterGetTarget` (raw-exact negative control, same TU) | 64 instructions; raw/target `dc6889fd...cbb038`; zero changed words; no WebFrank rule | Windows GPR `37..46`, FPR `38..52`; GPR `v35/v36/v37` coalesce to fixed `r3`; all other GPR and every FPR root are identity. Surviving stack is descending and selection is lowest-free. | Stock allocation is the target allocation. | This directly vetoes global color-preference, simplify-scan, or coalescing disablement: the same mechanisms are required for an exact function in the same TU. Pressure/graph shape, not the selector alone, gates the failure. |
| `getSinCos` | 27 instructions; raw `2c0e88fa...f85dcc`; target `413c3aaf...c2e7cd`; 14 words/20 fields; raw real 28 | Current FPR roots include `f46` (first polynomial constant) `->f2`, `f38` (second constant/chain) `->f3`, and `f49` (second first-stage product) `->f2`. Current `f49` sees only colors `f0/f1` among its previously colored neighbors. | The constants/chains exchange `f2/f3`; `f49` requires `f8`; later products also change colors. | In selection order, `f49` is the earliest mapped disagreement: lowest-free gives `f2`, while the target needs `f8`. Reaching `f8` requires six additional lower colors to be occupied by earlier neighbors or a different graph. The visible constant swap is therefore a consequence of broad graph/stack state, not a two-register preference toggle. |
| `AllocMem32` | 56 instructions; raw `94bae096...8045e`; target `7896e680...46f44`; 8 words/9 fields; raw real 16 | Padding web `r38->r31`; total-size chain `r32->r30` (degree 34); result `r33->r29` (degree 19). Current pop order reaches `r38`, then `r32`, and `r33` last. | Padding remains `r31`; total size and result exchange `r30/r29`. | The target order must effectively be `r38`, `r33`, `r32`: `r33` must consume `r30` before `r32` can receive `r29`. This is a degree/simplify-order change between two interfering saved webs, not a general saved-register rotation. |
| `closest_enemy` | 105 instructions; raw `df7ab683...2ec114`; target `9bfc8703...b62fb`; 26 words/29 fields; raw real 52 | Dense saved GPR/FPR cliques. Current GPR order/colors are `r43->r30`, `r39->r29`, `r38->r28`, `r36->r27`; FPR tail is `f36->f26`, `f33->f25`, `f32->f24`. | GPR roles become `r38->r30`, `r43->r29`, `r36->r28`, `r39->r27`. FPR roles become `f33->f26`, `f32->f25`, `f36->f24`. | Both register classes need nonuniform web-order changes. The FPR three-cycle and GPR four-role permutation do not share a physical-color transform, although both are consistent with different simplify-stack ordering under high interference. |
| `PointLineDist2D` | 107 instructions; raw `ada2054b...f5fe9`; target `f38c09da...1902f`; 63 words/129 fields; prior `opt_lifetimes off` probe reduced raw real 124 to 90 | FPR `f42` (length) has degree 38 but is the first current pop, has no previously colored neighbor, and receives `f0`. Current `f52/f51` order gives the two initial direction loads `f0/f1`. | Length needs `f5`; the initial direction loads need `f1/f0`; the reciprocal-square-root and projection webs undergo a broad volatile-FPR permutation. | For lowest-free to give `f42` color `f5`, target state must have neighbors occupying `f0..f4` before `f42`; this is a major liveness/interference/simplify-order change. In addition, target offsets `+0x140/+0x150` reverse two independent loads from fixed ABI bases `r4/r5` before commutative adds. That subpattern is equivalent instruction/operand ordering, not virtual-register allocation, despite satisfying WebFrank's register-field-only proof. |

`GetAnimAngXYZVal` is a non-WebFrank diagnostic only: its remaining raw
1047/1047, frame-920, fuzzy-99.58835/real-174 residual contains another
`f29/f31` and `f1/f0` web exchange. It was not used as a positive or negative
policy test and its source was not changed.

`pbDiagDrawTexture` is another unprocessed prospective positive, not a control:
its source-shaped raw result is 385/385, frame `0xb8`, raw real 30/clean 34,
with saved `r29/r28`, cursor `r23/r24`, and several volatile GPR role changes.
It has no WebFrank rule yet. It should join a later replay corpus, but no claim
in this audit depends on it.

## Policy assessment

No compiler-side rule is yet justified for production or for a scratch binary
prototype.

* Reversing or biasing the physical-color scan is contradicted by the
  raw-exact `CritterGetTarget` control and by target permutations that are not
  one common color mapping.
* Reversing the vreg scan globally is also contradicted by the exact control,
  and the positive cases require selective pressure-dependent reorderings
  rather than one reversal.
* Disabling or changing copy coalescing cannot explain
  `CritterResolveMultipleTargets`: all of its observed parents are identity.
  It would also remove valid fixed-`r3` coalesces from the exact control.
* An allocator-only patch cannot retire every WebFrank rule because
  `PointLineDist2D` includes fixed ABI operand/load-order changes which never
  pass through virtual-register coloring.
* Function-name or desired-register special cases are rejected. They do not
  generalize to edited source and would merely move WebFrank into the compiler.

The shared positive pattern is narrower: under enough interference, target
code behaves as if different neighbors were colored before the first divergent
web. A pressure-gated simplify tie-break or an upstream lifetime/interference
change remains plausible, but the present captures do not identify a safe
predicate. It must not be patched until it predicts target order on positives
and preserves raw-exact controls.

## Smallest next experiment

Capture `getSinCos` deeply because it is the smallest high-information positive:

1. At entry/exit of `MWCC_SpillCode_CoalesceCopies` (`0x00530E00`), record each
   copy pair, both roots, window membership, interference result, and resulting
   parent.
2. During `MWCC_Coloring_SimplifyGraph` (`0x004CE400`), record every actual
   removal with effective degree and scan pass, not just the final stack.
3. At `0x004CE381`, retain the node, neighbor colors, available mask, and chosen
   color for `f49`, `f46`, and `f38`.
4. Replay candidate orderings without patching the executable. A candidate may
   advance only if it produces the desired colors on `getSinCos`,
   `AllocMem32`, `closest_enemy`, and `CritterResolveMultipleTargets` while
   preserving `CritterGetTarget` and a wider raw-exact corpus.

Treat `PointLineDist2D` separately with scheduler/operand-order capture. A
successful allocator experiment may retire an allocator subset of WebFrank;
it cannot by itself justify deprecating the whole mechanism.

## Durable capture references

* `<local-scratch>\gdl-x64dbg-critter\trace-critter-colors.txt`, SHA-256
  `19B1FA51E054BDA81641D995741634222AF0085275D59C9C9860EC93F1D4D6B0`
* `<local-scratch>\gdl-x64dbg-critter\trace-critter-gettarget.txt`, SHA-256
  `53E37A9CB728623F3E4C3495A8C248C24F2D9FA3440886D9012A256DAEDE95FB`
* `<local-scratch>\gdl-webfrank-patterns\capture-getSinCos`
* `<local-scratch>\gdl-webfrank-patterns\capture-AllocMem32`
* `<local-scratch>\gdl-webfrank-patterns\capture-closest_enemy`
* `<local-scratch>\gdl-webfrank-patterns\capture-PointLineDist2D`

The temporary paths are evidence locations, not repository inputs. Recovered
addresses, decision semantics, and the policy vetoes are preserved in this
note and in `tools/gdl/mwcc_p6/ghidra_import.py`.
