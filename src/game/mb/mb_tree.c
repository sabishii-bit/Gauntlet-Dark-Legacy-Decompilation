#include "types.h"

/*
 * mb_tree.c - MB scene-graph node tree (MB_TREE.OBJ).
 *
 * The MB render tree: node alloc/insert/remove, per-node attribute setters
 * (ZMod/Alpha/Color/Flags/Scale/AltTex/AmbientAdd/UVIndex/UVScaleAdd/Texscroll),
 * fog, and the top-level MBTreeInit that also boots the object/blit/poly lists.
 * Node struct: +0x30 pos vec3, +0x60 flags, +0x74 child/next link.
 *
 * Range 0x800BA084..0x800BB5F4 (25 fns). Owns .sbss globals 0x80344EA8..0x80344EE0
 * and its own .sdata2 pool 0x80348CA0..0x80348CC4. Anchored names from the Xbox
 * shell3D PDB (MB_TREE.OBJ): MBTreeInit (biggest fn, inits subsystems),
 * MBTreeSetUVScaleAdd ('Too many UV Scale Add nodes active'). GC emits this module
 * in a scrambled (near-reverse) order; the remaining setter/alloc/remove helpers
 * are left fn_ pending per-field verification. cflags_demo, C++ exceptions on.
 *
 * Status: NonMatching translation.  The node attribute/flag propagation
 * helpers below are reconstructed against the retail call graph.
 */

typedef struct MBTreeNode {
    /* 0x00 */ u8 _pad00[0x40];
    /* 0x40 */ f32 scale[3];
    /* 0x4C */ u8 _pad4C[4];
    /* 0x50 */ u16 id;
    /* 0x52 */ s8 type;
    /* 0x53 */ u8 invAlpha;
    /* 0x54 */ f32 zMod;
    /* 0x58 */ s32 altTex;
    /* 0x5C */ s16 texIndex;
    /* 0x5E */ u8 uvScaleAddIndex;
    /* 0x5F */ u8 _pad5F;
    /* 0x60 */ u32 flags;
    /* 0x64 */ u32 color;
    /* 0x68 */ s16 uvIndex;
    /* 0x6A */ s16 ambientAdd;
    /* 0x6C */ u8 _pad6C[4];
    /* 0x70 */ void* special;
    /* 0x74 */ struct MBTreeNode* parent;
    /* 0x78 */ struct MBTreeNode* child;
    /* 0x7C */ struct MBTreeNode* next;
} MBTreeNode;

typedef struct MBUVScaleAdd {
    f32 uScale;
    f32 uAdd;
    f32 vScale;
    f32 vAdd;
} MBUVScaleAdd;

extern MBTreeNode* lbl_80344ECC;
extern MBTreeNode* lbl_80344ED0;
extern MBTreeNode* lbl_80344EDC;
extern MBTreeNode* lbl_80344EE0;
extern s32 lbl_80344EC8;
extern MBTreeNode* lbl_80344EA8;
extern MBTreeNode* lbl_80344EAC;
extern MBTreeNode* lbl_80344EB0;
extern MBTreeNode* lbl_80344EB4;
extern MBTreeNode* gSceneRoot;
extern MBTreeNode* lbl_80344EBC;
extern MBTreeNode* defaultBlitList;
extern MBTreeNode* gDiag_DE8;
extern MBTreeNode* gDiag_DEC;
extern MBTreeNode* gPolyCtx;
extern MBTreeNode* lbl_80344ED4;
extern MBTreeNode* lbl_80344ED8;
extern u8 lbl_802C2A28[];
extern const f32 lbl_80348CA0;
extern f32 gIdentityMatrix[16];
extern f32 light_color[4];
extern void* gWinGlobals;

const char lbl_801160B0[] = "Too many UV Scale Add nodes active\n";

static f32 MBTreePoolHugeFloat(void)
{
    return 1e37f;
}

void MBRemoveNodeChild(MBTreeNode* node);
MBTreeNode* MBNodeLastSibling(MBTreeNode* node);
MBTreeNode* MBNodePrevNode(MBTreeNode* node);
void MBNodeInsert(MBTreeNode* node, MBTreeNode* parent);
extern void MBRemovePsys(MBTreeNode* node);
extern void* AllocMem(u32 size);
extern void FatalError(const char* text, s32 errorCode);
extern void ErrorPrintf(const char* format, ...);
extern void CopyMat4(const f32* src, f32* dst);
extern void MBInitObjects(s32 enable);
extern void MBInitBlits(s32 makeNodes);
extern void MBInitPolys(s32 useHash);
extern void fn_800C0AA4(s32 layer);
void MBNodeInit(MBTreeNode* node, s32 type);
MBTreeNode* MBCreateNode(void);

/* 0x800BA084 */
typedef struct MBClearVertex {
    u16 _pad0;
    u16 flags;
    u8 _pad4[12];
} MBClearVertex;

typedef struct MBClearModel {
    u8 _pad0[0x48];
    u32 vertexCount;
    u8 _pad4C[0x0C];
    MBClearVertex* vertices;
} MBClearModel;

typedef struct MBClearRecord {
    s32 state;
    MBClearModel* model;
    u8 _pad8[8];
} MBClearRecord;

typedef struct MBClearGlobals {
    u8 _pad0[0x30];
    MBClearRecord* records;
} MBClearGlobals;

void MBClearTexscroll(void)
{
    MBClearGlobals* globals = gWinGlobals;
    MBClearModel** model;
    s32 i;

    for (i = 0; i < globals->records[0].state; i++) {
        MBClearRecord* record = &globals->records[i];

        model = &record->model;
        if (record[1].state == 0) {
            u32 j;

            for (j = 0; j < (*model)->vertexCount; j++) {
                (*model)->vertices[j].flags &= ~0x40;
            }
        }
    }
}

#pragma dont_inline on

/* 0x800BA0FC */
void MBTreeClearUVScaleAdd(MBTreeNode* node, s32 index, s32 recurse)
{
    MBTreeNode* child;

    goto outer_test;
outer_body:
    if (index == -1) {
        node->flags &= ~0x10000000;
    } else {
        node->uvScaleAddIndex = (u8)index;
        node->flags |= 0x10000000;
    }
    if (recurse != 0 && (child = node->child) != 0 &&
        (child->flags & 0x10) == 0) {
        MBTreeClearUVScaleAdd(child, index, 2);
    }
    if (recurse != 2) {
        goto done;
    }
    goto sibling_test;
sibling_body:
    node = node->next;
    if (node == 0 || (node->flags & 0x10) == 0) {
        goto outer_test;
    }
sibling_test:
    if (node != 0) {
        goto sibling_body;
    }
outer_test:
    if (node != 0) {
        goto outer_body;
    }
done:
    return;
}

/* 0x800BA1BC */
void MBTreeSetUVScaleAdd(f32 uScale, f32 uAdd, f32 vScale, f32 vAdd,
                         MBTreeNode* node, s32 recurse)
{
    MBUVScaleAdd* entries;
    MBUVScaleAdd* entry;

    entries = (MBUVScaleAdd*)lbl_802C2A28;
    if (1.0 == uScale && 0.0 == uAdd && 1.0 == vScale && 0.0 == vAdd) {
        if (node->flags & 0x10000000) {
            if (node->flags & 0x10000000) {
                entries[node->uvScaleAddIndex].uScale = lbl_80348CA0;
                MBTreeClearUVScaleAdd(node, -1, recurse);
            }
        }
        return;
    }

    if (node->flags & 0x10000000)
        entries[node->uvScaleAddIndex].uScale = lbl_80348CA0;

    entry = &entries[63];
    while (entry-- != entries) {
        if (entry->uScale > 1e36) {
            entry->uScale = uScale;
            entry->uAdd = uAdd;
            entry->vScale = vScale;
            entry->vAdd = vAdd;
            if (entry->uScale > 1e36)
                entry->uScale = 1e36f;
            MBTreeClearUVScaleAdd(node, entry - entries, recurse);
            return;
        }
    }

    ErrorPrintf(lbl_801160B0);
}

/* 0x800BA2C4 */
void MBTreeClearFlags(MBTreeNode* node, u32 flags, s32 recurse)
{
    MBTreeNode* child;

    goto outer_test;
outer_body:
    node->flags &= ~flags;
    if (recurse != 0 && (child = node->child) != 0 &&
        (child->flags & 0x10) == 0) {
        MBTreeClearFlags(child, flags, 2);
    }
    if (recurse != 2) {
        goto done;
    }
    goto sibling_test;
sibling_body:
    node = node->next;
    if (node == 0 || (node->flags & 0x10) == 0) {
        goto outer_test;
    }
sibling_test:
    if (node != 0) {
        goto sibling_body;
    }
outer_test:
    if (node != 0) {
        goto outer_body;
    }
done:
    return;
}

/* 0x800BA368 */
void MBTreeSetFlags(MBTreeNode* node, u32 flags, s32 recurse)
{
    MBTreeNode* child;

    goto outer_test;
outer_body:
    node->flags |= flags;
    if (recurse != 0 && (child = node->child) != 0 &&
        (child->flags & 0x10) == 0) {
        MBTreeSetFlags(child, flags, 2);
    }
    if (recurse != 2) {
        goto done;
    }
    goto sibling_test;
sibling_body:
    node = node->next;
    if (node == 0 || (node->flags & 0x10) == 0) {
        goto outer_test;
    }
sibling_test:
    if (node != 0) {
        goto sibling_body;
    }
outer_test:
    if (node != 0) {
        goto outer_body;
    }
done:
    return;
}

/* 0x800BA408 */
void MBTreeSetScale(MBTreeNode* node, f32 r, f32 g, f32 b)
{
    if (node == 0) {
        return;
    }
    node->flags |= 8;
    node->scale[0] = r;
    node->scale[1] = g;
    node->scale[2] = b;
}

/* 0x800BA42C */
void MBTreeSetColor(MBTreeNode* node, u32 color, s32 recurse)
{
    MBTreeNode* child;

    goto outer_test;
outer_body:
    node->color = color;
    node->flags |= 0x100;
    if (recurse != 0 && (child = node->child) != 0 &&
        (child->flags & 0x10) == 0) {
        MBTreeSetColor(child, color, 2);
    }
    if (recurse != 2) {
        goto done;
    }
    goto sibling_test;
sibling_body:
    node = node->next;
    if (node == 0 || (node->flags & 0x10) == 0) {
        goto outer_test;
    }
sibling_test:
    if (node != 0) {
        goto sibling_body;
    }
outer_test:
    if (node != 0) {
        goto outer_body;
    }
done:
    return;
}

/* 0x800BA4D0 */
void MBTreeSetAmbientAdd(MBTreeNode* node, s32 value, s32 recurse)
{
    MBTreeNode* child;

    goto outer_test;
outer_body:
    node->ambientAdd = (s16)value;
    if (recurse != 0 && (child = node->child) != 0 &&
        (child->flags & 0x10) == 0) {
        MBTreeSetAmbientAdd(child, value, 2);
    }
    if (recurse != 2) {
        goto done;
    }
    goto sibling_test;
sibling_body:
    node = node->next;
    if (node == 0 || (node->flags & 0x10) == 0) {
        goto outer_test;
    }
sibling_test:
    if (node != 0) {
        goto sibling_body;
    }
outer_test:
    if (node != 0) {
        goto outer_body;
    }
done:
    return;
}

/* 0x800BA56C */
void MBTreeSetAltTex(MBTreeNode* node, s32 index, s32 texture, s32 recurse)
{
    MBTreeNode* child;

    goto outer_test;
outer_body:
    node->texIndex = (s16)index;
    node->altTex = texture;
    if (recurse != 0 && (child = node->child) != 0 &&
        (child->flags & 0x10) == 0) {
        MBTreeSetAltTex(child, index, texture, 2);
    }
    if (recurse != 2) {
        goto done;
    }
    goto sibling_test;
sibling_body:
    node = node->next;
    if (node == 0 || (node->flags & 0x10) == 0) {
        goto outer_test;
    }
sibling_test:
    if (node != 0) {
        goto sibling_body;
    }
outer_test:
    if (node != 0) {
        goto outer_body;
    }
done:
    return;
}

/* 0x800BA614 */
void MBTreeSetZMod(MBTreeNode* node, f32 value, s32 recurse)
{
    MBTreeNode* child;

    goto outer_test;
outer_body:
    node->zMod = value;
    if (recurse != 0 && (child = node->child) != 0 &&
        (child->flags & 0x10) == 0) {
        MBTreeSetZMod(child, value, 2);
    }
    if (recurse != 2) {
        goto done;
    }
    goto sibling_test;
sibling_body:
    node = node->next;
    if (node == 0 || (node->flags & 0x10) == 0) {
        goto outer_test;
    }
sibling_test:
    if (node != 0) {
        goto sibling_body;
    }
outer_test:
    if (node != 0) {
        goto outer_body;
    }
done:
    return;
}

/* 0x800BA6B4 */
s32 MBTreeGetAlpha(MBTreeNode* node)
{
    return 0xFF - node->invAlpha;
}

/* 0x800BA6C0 */
void MBTreeSetAlpha(MBTreeNode* node, s32 alpha, s32 recurse)
{
    MBTreeNode* child;

    goto outer_test;
outer_body:
    node->invAlpha = (u8)(0xFF - alpha);
    if (alpha > 0) {
        node->flags |= 0x200;
    } else {
        node->flags &= ~0x200;
    }
    if (recurse != 0 && (child = node->child) != 0 &&
        (child->flags & 0x10) == 0) {
        MBTreeSetAlpha(child, alpha, 2);
    }
    if (recurse != 2) {
        goto done;
    }
    goto sibling_test;
sibling_body:
    node = node->next;
    if (node == 0 || (node->flags & 0x10) == 0) {
        goto outer_test;
    }
sibling_test:
    if (node != 0) {
        goto sibling_body;
    }
outer_test:
    if (node != 0) {
        goto outer_body;
    }
done:
    return;
}

/* 0x800BA784 */
void MBTreeSetZsortAdd(MBTreeNode* node, s32 index, s32 recurse)
{
    MBTreeNode* child;

    goto outer_test;
outer_body:
    node->uvIndex = (s16)index;
    if (recurse != 0 && (child = node->child) != 0 &&
        (child->flags & 0x10) == 0) {
        MBTreeSetZsortAdd(child, index, 2);
    }
    if (recurse != 2) {
        goto done;
    }
    goto sibling_test;
sibling_body:
    node = node->next;
    if (node == 0 || (node->flags & 0x10) == 0) {
        goto outer_test;
    }
sibling_test:
    if (node != 0) {
        goto sibling_body;
    }
outer_test:
    if (node != 0) {
        goto outer_body;
    }
done:
    return;
}

#pragma dont_inline off

static inline void MBTreeMoveAfter(MBTreeNode* node, MBTreeNode* after)
{
    MBTreeNode* previous;

    if (after->parent == node->parent) {
        previous = MBNodePrevNode(node);
        if (previous != 0)
            previous->next = node->next;
        else
            node->parent->child = node->next;
        node->next = after->next;
        after->next = node;
    }
}

static inline MBTreeNode* MBTreeCreateInitializedNode(s32 type)
{
    const f32* matrix = gIdentityMatrix;
    MBTreeNode* node = MBCreateNode();

    if (node != 0) {
        MBNodeInit(node, type);
        CopyMat4(matrix, (f32*)node);
        MBNodeInsert(node, 0);
    }
    return node;
}

/* 0x800BA820 */
void MBTreeInit(void)
{
    MBTreeNode* node1;
    f32 default_scale;
    s32 i;

    lbl_80344EC8 = 0;
    lbl_80344ECC = 0;
    lbl_80344EE0 = 0;
    MBInitObjects(1);
    MBInitBlits(1);
    MBInitPolys(1);

    node1 = MBTreeCreateInitializedNode(9);
    lbl_80344ED8 = node1;
    node1->flags |= 4;

    node1 = MBTreeCreateInitializedNode(15);
    lbl_80344ED4 = node1;
    node1->flags |= 4;

    node1 = MBTreeCreateInitializedNode(1);
    lbl_80344EDC = node1;

    MBTreeMoveAfter(gDiag_DEC, lbl_80344EBC);
    MBTreeMoveAfter(gSceneRoot, gDiag_DEC);
    MBTreeMoveAfter(lbl_80344EDC, gSceneRoot);
    MBTreeMoveAfter(lbl_80344EB4, lbl_80344EDC);
    MBTreeMoveAfter(lbl_80344EB0, lbl_80344EB4);
    MBTreeMoveAfter(gPolyCtx, lbl_80344EB0);
    MBTreeMoveAfter(lbl_80344EAC, gPolyCtx);
    MBTreeMoveAfter(gPolyCtx, lbl_80344ED8);
    MBTreeMoveAfter(defaultBlitList, lbl_80344EAC);
    MBTreeMoveAfter(lbl_80344ED4, defaultBlitList);
    MBTreeMoveAfter(gDiag_DE8, lbl_80344ED4);
    MBTreeMoveAfter(lbl_80344EA8, gDiag_DE8);

    default_scale = lbl_80348CA0;
    for (i = 0; i < 64; i++)
        ((MBUVScaleAdd*)lbl_802C2A28)[i].uScale = default_scale;

    fn_800C0AA4(3);
}

/* 0x800BACF8 - MBNodeOrder: move node immediately before sibling. */
void MBNodeOrder(MBTreeNode* node, MBTreeNode* sibling)
{
    MBTreeNode* previous;
    MBTreeNode* parent;

    if (node->parent != (parent = sibling->parent))
        return;

    if (parent == 0)
        previous = lbl_80344ECC;
    else
        previous = parent->child;

    if (previous == 0) {
        previous = 0;
    } else if (previous == sibling) {
        previous = 0;
    } else {
        while (previous != 0 && previous->next != sibling)
            previous = previous->next;
        if (previous == 0)
            previous = 0;
    }

    if (previous != 0)
        previous->next = sibling->next;
    else
        parent->child = sibling->next;
    sibling->next = node->next;
    node->next = sibling;
}

/* 0x800BAD90 */
void MBCompVertScaleAddUV(void) {}

/* 0x800BAD94 - MBNodeSetParent */
void MBNodeSetParent(MBTreeNode* node, MBTreeNode* new_parent)
{
    u8 unused[8];
    MBTreeNode* old_parent;
    MBTreeNode* previous;

    old_parent = node->parent;
    if (old_parent == 0 || old_parent != new_parent) {
        if (old_parent != 0 && old_parent->child == node) {
            old_parent->child = node->next;
        } else {
            MBTreeNode* current;

            if (old_parent == 0)
                current = lbl_80344ECC;
            else
                current = old_parent->child;
            if (current == 0) {
                previous = 0;
            } else if (current == node) {
                previous = 0;
            } else {
                while (current != 0 && current->next != node)
                    current = current->next;
                if (current == 0)
                    previous = 0;
                else
                    previous = current;
            }
            if (previous != 0)
                previous->next = node->next;
        }

        node->next = 0;
        node->parent = new_parent;
        if (new_parent == 0) {
            if (lbl_80344ECC == 0) {
                lbl_80344ECC = node;
            } else {
                previous = MBNodeLastSibling(lbl_80344ECC);
                previous->next = node;
            }
        } else if (new_parent->child == 0) {
            new_parent->child = node;
        } else {
            previous = MBNodeLastSibling(new_parent->child);
            previous->next = node;
        }
    }
}

static inline MBTreeNode* MBNodePrevNodeInline(MBTreeNode* node)
{
    MBTreeNode* current;

    if (node->parent == 0) {
        current = lbl_80344ECC;
    } else {
        current = node->parent->child;
    }
    if (current == 0) {
        return 0;
    }
    if (current == node) {
        return 0;
    }
    while (current != 0 && current->next != node) {
        current = current->next;
    }
    if (current == 0) {
        return 0;
    }
    return current;
}

/* 0x800BAEAC - MBRemoveNode */
MBTreeNode* MBRemoveNode(MBTreeNode* node, s32 remove_children)
{
    u8 unused[8];
    MBTreeNode* parent;
    MBTreeNode* previous;
    MBTreeNode* child;
    MBTreeNode* tail;

    if (node == 0)
        return 0;
    if (node->type == 0)
        return 0;

    if (node->flags & 0x10000000) {
        if (node->flags & 0x10000000) {
            *(f32*)(lbl_802C2A28 + (u32)node->uvScaleAddIndex * 16) = lbl_80348CA0;
            MBTreeClearUVScaleAdd(node, -1, 0);
        }
    }

    parent = node->parent;
    lbl_80344ED0 = parent;
    if (remove_children) {
        MBRemoveNodeChild(node->child);
        node->child = 0;
    }

    if (node->type == 14 && node->special != 0) {
        MBTreeNode* psys_root = lbl_80344EDC;

        parent = node->parent;
        if (parent == 0 || parent != psys_root) {
            if (parent != 0 && parent->child == node) {
                parent->child = node->next;
            } else {
                previous = MBNodePrevNode(node);
                if (previous != 0)
                    previous->next = node->next;
            }
            node->next = 0;
            MBNodeInsert(node, psys_root);
        }
        MBRemovePsys(node);
        return 0;
    }

    if (parent != 0 && parent->child == node) {
        if (node->child == 0) {
            parent->child = node->next;
        } else {
            parent->child = node->child;
            child = node->child;
            while (child->next != 0) {
                child->parent = parent;
                child = child->next;
            }
            child->parent = parent;
            child->next = node->next;
        }
    } else if (node == lbl_80344ECC) {
        child = node->child;
        if (child == 0) {
            lbl_80344ECC = node->next;
        } else {
            lbl_80344ECC = child;
            child = node->child;
            while (child->next != 0) {
                child->parent = 0;
                child = child->next;
            }
            child->parent = 0;
            child->next = node->next;
        }
    } else {
        previous = MBNodePrevNodeInline(node);
        if (previous != 0) {
            if (node->child == 0) {
                previous->next = node->next;
            } else {
                previous->next = node->child;
                parent = previous->parent;
                tail = node->child;
                while (tail->next != 0) {
                    tail->parent = parent;
                    tail = tail->next;
                }
                tail->parent = parent;
                tail->next = node->next;
            }
        }
    }

    node->type = 0;
    node->child = 0;
    node->parent = 0;
    node->next = lbl_80344EE0;
    lbl_80344EE0 = node;
    return 0;
}

/* 0x800BB164 */
void MBRemoveNodeChild(MBTreeNode* node)
{
    u8 unused[8];
    MBTreeNode* current = node;
    u8* entries = lbl_802C2A28;
    u8* entry;
    f32 default_scale = lbl_80348CA0;

    while (current != 0) {
        node = current;

        if (node->child != 0)
            MBRemoveNodeChild(node->child);
        if (node->flags & 0x10000000) {
            if (node->flags & 0x10000000) {
                *(f32*)(entry = entries + (u32)node->uvScaleAddIndex * 16) = default_scale;
                MBTreeClearUVScaleAdd(node, -1, 1);
            }
        }
        current = node->next;
        if (node->type == 14 && node->special != 0) {
            MBTreeNode* psys_root = lbl_80344EDC;
            MBTreeNode* old_parent = node->parent;

            if (old_parent == 0 || old_parent != psys_root) {
                if (old_parent != 0 && old_parent->child == node) {
                    old_parent->child = node->next;
                } else {
                    MBTreeNode* previous = MBNodePrevNode(node);
                    if (previous != 0)
                        previous->next = node->next;
                }
                node->next = 0;
                MBNodeInsert(node, psys_root);
            }
            MBRemovePsys(node);
        } else {
            node->type = 0;
            node->child = 0;
            node->parent = 0;
            node->next = lbl_80344EE0;
            lbl_80344EE0 = node;
        }
    }
}

/* 0x800BB29C */
MBTreeNode* MBNewNode(MBTreeNode* parent, const f32* matrix, s32 type)
{
    u8 unused[8];
    MBTreeNode* node;

    if (matrix == 0)
        matrix = gIdentityMatrix;
    if (type == 0)
        type = 1;

    if ((node = lbl_80344EE0) != 0) {
        lbl_80344EE0 = node->next;
    } else {
        node = AllocMem(0x80);
        node->id = lbl_80344EC8;
        lbl_80344EC8++;
        if (lbl_80344EC8 >= 0x3000)
            FatalError("Too many nodes", 0x804000);
    }

    if (node != 0) {
        MBNodeInit(node, type);
        CopyMat4(matrix, (f32*)node);
        node->parent = parent;
        if (parent == 0) {
            if (lbl_80344ECC == 0)
                lbl_80344ECC = node;
            else
                MBNodeLastSibling(lbl_80344ECC)->next = node;
        } else if (parent->child == 0) {
            parent->child = node;
        } else {
            MBNodeLastSibling(parent->child)->next = node;
        }
    }
    return node;
}

/* 0x800BB3AC */
void MBNodeInit(MBTreeNode* node, s32 type)
{
    node->type = type;
    CopyMat4(gIdentityMatrix, (f32*)node);
    node->scale[0] = light_color[0];
    node->scale[1] = light_color[1];
    node->scale[2] = light_color[2];
    node->flags = 0;
    node->color = 0;
    node->invAlpha = 0;
    node->uvIndex = 0;
    *(u32*)node->_pad6C = 0;
    node->parent = 0;
    node->child = 0;
    node->next = 0;
    node->zMod = 0.0f;
    node->ambientAdd = 0;
    node->texIndex = -1;
    node->altTex = 0;
}

/* 0x800BB448 */
MBTreeNode* MBCreateNode(void)
{
    MBTreeNode* node;

    if (lbl_80344EE0 != 0) {
        node = lbl_80344EE0;
        lbl_80344EE0 = node->next;
    } else {
        node = AllocMem(0x80);
        node->id = lbl_80344EC8;
        lbl_80344EC8++;
        if (lbl_80344EC8 >= 0x3000)
            FatalError("Too many nodes", 0x804000);
    }
    return node;
}

/* 0x800BB4CC */
static void MBNodeAppend(MBTreeNode* node, MBTreeNode* head)
{
    MBTreeNode* tail = head;
    MBTreeNode* next = head->next;

    while (next != 0 && next != head) {
        tail = next;
        next = next->next;
    }
    tail->next = node;
}

void MBNodeInsert(MBTreeNode* node, MBTreeNode* parent)
{
    node->parent = parent;
    if (parent == 0) {
        MBTreeNode* head;

        if ((head = lbl_80344ECC) == 0) {
            lbl_80344ECC = node;
            return;
        }
        MBNodeAppend(node, head);
        return;
    }

    {
        MBTreeNode* head = parent->child;

        if (head == 0) {
            parent->child = node;
            return;
        }
        MBNodeAppend(node, head);
    }
}

/* 0x800BB55C */
MBTreeNode* MBNodeLastSibling(MBTreeNode* node)
{
    MBTreeNode* result = node;
    MBTreeNode* next;

    for (next = node->next; next != 0 && next != node; next = next->next) {
        result = next;
    }
    return result;
}

/* 0x800BB588 */
MBTreeNode* MBNodePrevNode(MBTreeNode* node)
{
    MBTreeNode* current;

    if (node->parent == 0) {
        current = lbl_80344ECC;
    } else {
        current = node->parent->child;
    }
    if (current == 0) {
        return 0;
    }
    if (current == node) {
        return 0;
    }
    while (current != 0 && current->next != node) {
        current = current->next;
    }
    if (current == 0) {
        return 0;
    }
    return current;
}
