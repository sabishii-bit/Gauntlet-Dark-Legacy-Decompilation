---
description: "atree.c node-helper Xbox name map and exact-match source-shape techniques"
---

# atree.c node pass (2026-07-29)

The reverse-ordered Xbox `ATREE.OBJ` inventory provides a one-to-one semantic
map for the GCN node teardown and sibling helpers:

- `AtreeNodeSetParent` @ `0x80011628`
- `AtreeKillPsys` @ `0x800116EC`
- `AtreeRemovePsysSub` @ `0x80011750`
- `AtreeRemoveNode` @ `0x800117EC`
- `AtreeRemoveNodeChild` @ `0x800119DC`
- `AtreeRemoveNodeSub` @ `0x80011A74`
- `AtreeModel` @ `0x80011B6C`
- `AtreeNodeInsert` @ `0x8001326C`
- `AtreeNodeLastSibling` @ `0x800132F0`
- `AtreeNodePrevNode` @ `0x8001331C`

The pool/list names immediately after `AtreeFindSeq` also map confidently:
`AtreeInitLists`, `AtreeListLock`, `AtreeAlloc`, and `AtreeSetEmpty`.

Exact-match source shapes:

- `AtreeDelete`: `root = node = *proot;` emits the retail direct load into the
  loop argument followed by one root copy; two separate assignments introduce
  an extra temporary and copy.
- `AtreeRemoveNodeChild`: put child assignments in their null tests
  (`if ((child = node->child) != NULL)`) so MWCC loads directly into the saved
  register instead of loading through `r0` and copying.
- `AtreeNodePrevNode`: avoid a named `next` local in the root-list walk.
  Repeating `list->next` in the condition/body lets CSE produce the retail
  rotated loop and is byte-exact.
- `AtreeNodeInsert`: preserve the child anchor as a separate local, then reuse
  the dead incoming `parent` and `root` parameters as the second sibling
  loop's cursor and tail.  Their ABI registers become the retail `r4`/`r5`
  pair while the anchor stays in `r6`; this removes the final 14-line
  register-only residual and makes the function byte-exact.  Merely changing
  declaration or first-definition order did not affect the coloring.
