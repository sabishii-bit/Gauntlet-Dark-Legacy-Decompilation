---
name: mb-tree-api-map
description: "MB_TREE real-name map and node layout anchors"
---

# MB_TREE API map

`game/mb/mb_tree.c` is `0x800BA084..0x800BB5F4`. The Xbox
`MB_TREE.OBJ` roster and GC field writes/callers establish these names:

- `800BA084 MBClearTexscroll`
- `800BA0FC MBTreeClearUVScaleAdd`
- `800BA1BC MBTreeSetUVScaleAdd`
- `800BA2C4 MBTreeClearFlags`
- `800BA368 MBTreeSetFlags`
- `800BA408 MBTreeSetScale`
- `800BA42C MBTreeSetColor`
- `800BA4D0 MBTreeSetAmbientAdd`
- `800BA56C MBTreeSetAltTex`
- `800BA614 MBTreeSetZMod`
- `800BA6B4 MBTreeGetAlpha`
- `800BA6C0 MBTreeSetAlpha`
- `800BA784 MBTreeSetZsortAdd`
- `800BACF8 MBNodeOrder`
- `800BAD94 MBNodeSetParent`
- `800BAEAC MBRemoveNode`
- `800BB164 MBRemoveNodeChild`
- `800BB29C MBNewNode`
- `800BB3AC MBNodeInit`
- `800BB448 MBCreateNode`
- `800BB4CC MBNodeInsert`
- `800BB55C MBNodeLastSibling`
- `800BB588 MBNodePrevNode`

Node layout anchors: matrix `+0x00`, scale `+0x40`, id `+0x50` (`u16`,
not `s16`—signed assignment adds an unwanted `extsh`), type `+0x52`,
inverse alpha `+0x53`, z-mod `+0x54`, alt texture `+0x58`, flags `+0x60`,
color `+0x64`, z-sort add `+0x68`, ambient add `+0x6A`, parent/child/next
at `+0x74/+0x78/+0x7C`.

`MBNodeInit` and `MBCreateNode` are exact. `MBNewNode` is fully translated
but still differs in nonvolatile allocation and one instruction. The free-list
head is `lbl_80344EE0`; the root list is `lbl_80344ECC`; the monotonic node id
counter is `lbl_80344EC8`.
