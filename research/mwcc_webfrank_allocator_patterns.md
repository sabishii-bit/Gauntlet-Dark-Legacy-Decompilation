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

`atree.c::fn_80011BBC` is a simpler prospective saved-GPR positive: a portable
source improvement reduced its 88/88 opcode-identical residual from real 30 to
real 6, leaving only one loop-counter web in stock `r22` where the target uses
`r24` across `li/addi/cmp`. It has not been dynamically captured and is not a
negative control, but it is a useful future replay sample for any recovered
saved-register ordering predicate.

## Deep dynamic closure

### `CritterResolveMultipleTargets` and exact control

The Resolve capture was repeated through the x32dbg MCP at the live allocator
entry, independently of retrowin32/GDB. At `0x004CE400`, the arguments were
class `0`, 29 colors, and 74 virtual registers. The parent map at
`0x00BB62F8` was the identity sequence through `r73`; the graph pointer was
`0x00BB6390`. Node `r72` had degree and neighbor count 21, with neighbors
`1,3,36,39,40,41,43,44,46,53,58,59,60,61,62,63,64,65,68,69,73`.
At `0x004CE381`, the same node had available mask `0x1ff0`; stock selected
`r4`, while the target mapping requires `r6`.

The deep same-TU control `CritterGetTarget` retained its fixed-`r3` coalesces
and selected exact target colors with the same ascending simplify scan,
descending pop order, and lowest-free selector. This is the negative evidence
that rules out replacing Resolve's WebFrank rule with a global selector or
scan reversal.

### Completed `getSinCos` experiment

`getSinCos` is CodeGen ordinal 13 in `mb_particle.c`. Its deep FPR trace found
exactly one copy-coalescing candidate: virtual `f32` and fixed `f1`. They did
not interfere, so the coalescer committed parent `32 -> 1`. Every virtual root
from `f33` through `f54` remained identity. All 22 of those nodes were below
the 32-color threshold and were removed, in ascending virtual-register order,
during the first simplify scan. Selection therefore visited them in descending
order.

The earliest decisive target-mapped choice is `f49`. At selection it has
available mask `0x3ffc`; only neighbors `f1/f32 -> f1` and `f50 -> f0` have
colors, so stock correctly chooses `f2`. The target requires `f8`, even though
`f2..f7` are free in the observed state. `f46` likewise sees `0x3ffc` and
chooses `f2` where the target needs `f3`. When `f38` is reached, its earlier
neighbors occupy `f0/f1/f2`, so it sees `0x3ff8` and chooses `f3`; the target
needs `f2`.

The target coloring is legal on the current interference graph. In particular,
`f49` has target-colored neighbors spanning every lower color: `f50 -> f0`,
`f1/f32 -> f1`, `f38 -> f2`, `f37 -> f3`, `f36 -> f4`, `f35 -> f5`,
`f34 -> f6`, and `f33 -> f7`. Coloring those neighbors first makes target
`f8` the lowest free choice. This proves that a different order can produce the
target without changing the selector, but it does not reveal a safe ordering
predicate.

A scratch replay over all 22 noncoalesced FPR nodes gave:

| Selection policy | Current colors | Target colors |
| --- | ---: | ---: |
| Stock descending virtual-register order | 22/22 | 14/22 |
| Ascending virtual-register order | 9/22 | 10/22 |
| Static low-degree first | 9/22 | 9/22 |
| Static high-degree first | 4/22 | 5/22 |

Target-informed ready-node orders can reproduce 22/22 target colors, which is
useful only as a graph-validity proof. They are deliberately not patch
candidates because they consult the desired coloring.

### Completed `AllocMem32` saved-GPR experiment

The SHA-pinned compiler reached `AllocMem32` as CodeGen ordinal 15 in
`ml_mem.c` through both the repaired x32dbg MCP path and retrowin32/GDB. Its
GPR coalescing window is `35..60`. The only unions are `r48 -> fixed r3` and
`r59 -> fixed r3`; the three saved-register roots remain independent:

| Web | Role | Initial degree | Stock simplify/removal | Stock selection/color |
| --- | --- | ---: | --- | --- |
| `r38` | alignment/padding chain | 34 | pass 2, effective degree 13 | first saved choice, `r31` |
| `r32` | total-size chain | 34 | pass 2, effective degree 14 | second saved choice, `r30` |
| `r33` | result chain | 19 | first node removed in pass 1 | last saved choice, `r29` |

The simplify stack is therefore `r38`, `r32`, the volatile webs, then `r33`.
At `0x004CE381`, ordinary webs select the lowest set bit of their volatile
available mask. These three webs instead reach `0x004CE3BF` with no volatile
color available. `MWCC_Coloring_ClaimHighestSavedGPR` (`0x004C1A50`) scans
physical colors from `r31` down through `r14`, reserves the first free color,
and returns it. The target needs the legal order `r38`, `r33`, `r32`, producing
`r31`, `r30`, `r29`; the physical-color fallback itself is not wrong.

Two raw-exact same-TU controls constrain any change. `AllocMem` has one
high-degree root (`r32`, degree 30) and one medium root (`r33`, degree 18).
Stock removes `r33` first and `r32` last, then assigns `r32 -> r31` and
`r33 -> r30`; that is target-exact. `AllocHiMem` has one saved root (`r32`,
degree 33), which stock assigns `r31`; it is also target-exact. This rules out
a general saved-color reversal or generic low-before-high reordering.

A graph replay found one useful mechanistic oracle. Reducing the GPR simplify
threshold from its real value 29 to any value in `16..19` changes only the two
wrong `AllocMem32` assignments and reproduces its complete target function.
The exact `AllocMem` control regresses at `16..18` but remains unchanged at
19. A live in-memory whole-`ml_mem.c` compile therefore tested `K=19` on the
unmodified, SHA-pinned scratch compiler. It made `AllocMem32` byte-exact, but
it was not safe:

| Function | Baseline target-different bytes | `K=19` target-different bytes |
| --- | ---: | ---: |
| `serve_io` | 0 | 5 |
| `do_threaded_io` | 0 | 29 |
| `StartFileRead` | 0 | 43 |
| `AllocMem32` | 8 | 0 |
| `get_path` | 0 | 10 |
| `AllocFile` | 2 | 37 |
| `xReadFileSection` | 0 | 6 |
| `MLMReadFile` | 0 | 9 |

The baseline object SHA-256 is
`731d4046f7d75c1c4619c7e954e68afa3597935fecc805871556a04e833483b1`;
the in-memory `K=19` object is
`a084974d7e41d51ef854efb39a3bc32b33a86d6edda0d809440583ec5a2269e0`.
Both `.text` and `extab` changed. Gating `K=19` on the exact observed
`34/34/19` graph signature would merely encode this target shape: no recovered
compiler semantic explains 19, and no shape-matched exact negative corpus
justifies it. The saved-GPR threshold class is therefore capped for a
production `1.2.5s` extension. The `K=19` result remains a compact oracle for
future reconstruction of the original priority or liveness difference.

### x32dbg MCP path

The live x32dbg effort also isolated a debugger-bridge defect unrelated to
MWCC. The bridge constructed `InitDebug "exe" args`; x64dbg requires the target
command line as comma-delimited quoted argument 2, `InitDebug "exe","args"`.
The source fix and offline regression test are preserved in external checkout
`<local-x64dbg>\mcp`, branch `codex/initdebug-argfix`, commit `76a0b47a`.
The source and deployed x32 plugin have SHA-256
`2e8eb4442c03ba355f79e78a5b5e9ccc0d27f72723a87cc79171f93cdf44ace6`;
the pre-fix installed file is backed up under `<local-scratch>\gdl-x64dbg-critter`
with SHA-256
`c205dcc9ec8fa1eb199ac7732c668f6e794879eb1673d5f84eb9fcaca12a3817`.
The repaired path loaded the SHA-pinned compiler, loaded the scratch
`LMGR326B.dll`, reached the named Resolve CodeGen invocation, and hit the live
allocator and color-choice breakpoints. No compiler bytes were changed.

## Policy assessment

No compiler-side rule is yet justified for production or for a scratch binary
prototype.

* Reversing or biasing the physical-color scan is contradicted by the
  raw-exact `CritterGetTarget` control and by target permutations that are not
  one common color mapping.
* Reversing the vreg scan globally is also contradicted by the exact control,
  and the positive cases require selective pressure-dependent reorderings
  rather than one reversal.
* Reducing the GPR simplify threshold to 19 is now directly disproven as a
  global policy. It fixes `AllocMem32` exactly but regresses six raw-exact
  functions in the same TU and worsens `AllocFile`.
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

## Bounded next step

The smallest positive experiment above is complete. A future compiler lane
should recover the original name-independent reason for the different
simplify order, likely in web construction, liveness, or a pressure-dependent
tie-break. A candidate may advance only if it predicts the desired order on
`getSinCos`, `AllocMem32`, `closest_enemy`, and
`CritterResolveMultipleTargets` while preserving `CritterGetTarget` and a wider
raw-exact corpus. Do not create a target-informed ordering patch.

Treat `PointLineDist2D` separately with scheduler/operand-order capture. A
successful allocator experiment may retire an allocator subset of WebFrank;
it cannot by itself justify deprecating the whole mechanism.

## Durable capture references

* `<local-scratch>\gdl-x64dbg-critter\trace-critter-colors.txt`, SHA-256
  `19B1FA51E054BDA81641D995741634222AF0085275D59C9C9860EC93F1D4D6B0`
* `<local-scratch>\gdl-x64dbg-critter\trace-critter-gettarget.txt`, SHA-256
  `53E37A9CB728623F3E4C3495A8C248C24F2D9FA3440886D9012A256DAEDE95FB`
* `<local-scratch>\gdl-x64dbg-critter\trace-getSinCos-deep.txt`, SHA-256
  `99515BC3496EFAB569AAAE874D5E750F2F41231EB921E2082FB7F0D9C88785E9`
* `<local-scratch>\gdl-x64dbg-critter\trace-getSinCos-deep.gdb`, SHA-256
  `7B69E6C14A4D1F6D5803BBC6FE25BE34212FB5DA524B50589E1F39CA147BB190`
* `<local-scratch>\gdl-x64dbg-critter\trace-AllocMem32-deep.txt`, SHA-256
  `6FE1F57B91C192FFFCECC9EB0FDFBB3AF8257D06209B0C58F848F78DA9B412A1`
* `<local-scratch>\gdl-x64dbg-critter\trace-AllocMem-control-graph.txt`, SHA-256
  `C1C62AEEDB638767977FC8ED2FC451C6DF2AA72D3A6381B22B074463937FEDB3`
* `<local-scratch>\gdl-x64dbg-critter\trace-AllocHiMem-control.txt`, SHA-256
  `E2ACA5DF3091815151AD1A1B6429C9D26704631C5DF3828F2DED3FCEB30513F6`
* `<local-scratch>\gdl-x64dbg-critter\replay-AllocMem32-k.py`, SHA-256
  `CA5159648CF95A9CB99C1775B1695E4A74E0B01144123045DCDE3CC6B60FA85B`
* `<local-scratch>\gdl-x64dbg-critter\compare-k19-target.py`, SHA-256
  `968703C355B2F89CE01F9A1B7701662125E61237A3B6DDBCE4EC7BD77767D95C`
* `<local-scratch>\gdl-x64dbg-critter\trace-ml_mem-k19.txt`, SHA-256
  `D322A2750528E5BB3B9FE6CB4997844CC41F6DA0002CF7857BD97044C362A0BA`
* `<local-scratch>\gdl-x64dbg-critter\x32-live-resolve-evidence.md`
* `<local-scratch>\gdl-webfrank-patterns\capture-getSinCos`
* `<local-scratch>\gdl-webfrank-patterns\capture-AllocMem32`
* `<local-scratch>\gdl-webfrank-patterns\capture-closest_enemy`
* `<local-scratch>\gdl-webfrank-patterns\capture-PointLineDist2D`

The temporary paths are evidence locations, not repository inputs. Recovered
addresses, decision semantics, and the policy vetoes are preserved in this
note and in `tools/gdl/mwcc_p6/ghidra_import.py`.
