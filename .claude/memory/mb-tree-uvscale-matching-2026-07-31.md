---
name: mb-tree-uvscale-matching-2026-07-31
description: "Exact MBTreeSetUVScaleAdd reconstruction and MB_TREE data-pool ownership"
metadata:
  node_type: memory
  type: project
---

# MB tree UV-scale matching

`MBTreeSetUVScaleAdd` (`0x800BA1BC`, 66 instructions) is reconstructed exactly
apart from normalized relocation labels. Its real ABI is four float values,
then `MBTreeNode *node` in `r3`, then `s32 recurse` in `r4`; the independent
integer/FPR argument banks make the order easy to misread in Ghidra.

The 0x400-byte table at `0x802C2A28` is 64 records of four floats. Allocation
starts at record 62 and scans backward with `while (entry-- != entries)`, using
`1e37f` as the free marker and `1e36` as the availability threshold. Record 63
is deliberately skipped. A neutral `(1, 0, 1, 0)` request clears the mapping.

Two MWCC source-shape details closed the function:

- The target's consecutive identical `beq` instructions come from nesting the
  same `node->flags & 0x10000000` guard twice. MWCC retains both branches while
  reusing the first condition-register result.
- An uncalled local helper returning `1e37f` places the marker first in sdata2;
  dead stripping removes the helper at link time but preserves pool ordering.
  This produces the exact MB_TREE pool `0x80348CA0..0x80348CC8`, including the
  final `0.0f` used by `MBNodeInit::zMod`.

MB_TREE owns rodata `0x801160B0..0x801160E4` (one trailing zero claim byte) and
sdata2 `0x80348CA0..0x80348CC8`; `datadiff.py` verifies both.

## Init and ordering follow-up

`MBTreeInit` is a 310-instruction subsystem initializer, not an empty platform
hook. It clears the node allocator, initializes objects/blits/polys, creates
three root nodes (types 9, 15, and 1), reorders twelve global render roots,
marks all 64 UV records free, and enables debug layer 3. A small inlined
`MBTreeMoveAfter(node, after)` helper reproduces every repeated ordering block.
The semantic translation currently emits 311 instructions; its residual is
almost entirely the nonvolatile-register permutation plus one matrix-pointer
copy and the final loop schedule.

`MBNodeOrder` became exact by preserving the standalone `MBNodePrevNode`
control shape and writing the parent capture inside the comparison:

```c
if (node->parent != (parent = sibling->parent))
    return;
```

That assignment-in-condition changes both evaluation order and the r5/r6 web
allocation, eliminating a 34-line register-only residual. The 4-byte no-op at
`0x800BAD90` is Xbox's `MBCompVertScaleAddUV`, not an unknown `fn_*`.
