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
    /* 0x40 */ f32 colorAdd[3];
    /* 0x4C */ u8 _pad4C[4];
    /* 0x50 */ s16 id;
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

extern MBTreeNode* lbl_80344ECC;
extern MBTreeNode* lbl_80344ED0;
extern MBTreeNode* lbl_80344EDC;
extern MBTreeNode* lbl_80344EE0;
extern u8 lbl_802C2A28[];
extern const f32 lbl_80348CA0;

void fn_800BB164(MBTreeNode* node);
MBTreeNode* fn_800BB55C(MBTreeNode* node);
MBTreeNode* fn_800BB588(MBTreeNode* node);
void fn_800BB4CC(MBTreeNode* node, MBTreeNode* parent);
extern void fn_800D12F0(MBTreeNode* node);

/* 0x800BA084 */
void fn_800BA084(void) {}

#pragma dont_inline on

/* 0x800BA0FC */
void fn_800BA0FC(MBTreeNode* node, s32 index, s32 recurse)
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
        fn_800BA0FC(child, index, 2);
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
void MBTreeSetUVScaleAdd(void) {}

/* 0x800BA2C4 */
void fn_800BA2C4(MBTreeNode* node, u32 flags, s32 recurse)
{
    MBTreeNode* child;

    goto outer_test;
outer_body:
    node->flags &= ~flags;
    if (recurse != 0 && (child = node->child) != 0 &&
        (child->flags & 0x10) == 0) {
        fn_800BA2C4(child, flags, 2);
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
void fn_800BA368(MBTreeNode* node, u32 flags, s32 recurse)
{
    MBTreeNode* child;

    goto outer_test;
outer_body:
    node->flags |= flags;
    if (recurse != 0 && (child = node->child) != 0 &&
        (child->flags & 0x10) == 0) {
        fn_800BA368(child, flags, 2);
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
void fn_800BA408(MBTreeNode* node, f32 r, f32 g, f32 b)
{
    if (node == 0) {
        return;
    }
    node->flags |= 8;
    node->colorAdd[0] = r;
    node->colorAdd[1] = g;
    node->colorAdd[2] = b;
}

/* 0x800BA42C */
void fn_800BA42C(MBTreeNode* node, u32 color, s32 recurse)
{
    MBTreeNode* child;

    goto outer_test;
outer_body:
    node->color = color;
    node->flags |= 0x100;
    if (recurse != 0 && (child = node->child) != 0 &&
        (child->flags & 0x10) == 0) {
        fn_800BA42C(child, color, 2);
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
void fn_800BA4D0(MBTreeNode* node, s32 value, s32 recurse)
{
    MBTreeNode* child;

    goto outer_test;
outer_body:
    node->ambientAdd = (s16)value;
    if (recurse != 0 && (child = node->child) != 0 &&
        (child->flags & 0x10) == 0) {
        fn_800BA4D0(child, value, 2);
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
void fn_800BA56C(MBTreeNode* node, s32 index, s32 texture, s32 recurse)
{
    MBTreeNode* child;

    goto outer_test;
outer_body:
    node->texIndex = (s16)index;
    node->altTex = texture;
    if (recurse != 0 && (child = node->child) != 0 &&
        (child->flags & 0x10) == 0) {
        fn_800BA56C(child, index, texture, 2);
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
void fn_800BA614(MBTreeNode* node, f32 value, s32 recurse)
{
    MBTreeNode* child;

    goto outer_test;
outer_body:
    node->zMod = value;
    if (recurse != 0 && (child = node->child) != 0 &&
        (child->flags & 0x10) == 0) {
        fn_800BA614(child, value, 2);
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
s32 fn_800BA6B4(MBTreeNode* node)
{
    return 0xFF - node->invAlpha;
}

/* 0x800BA6C0 */
void fn_800BA6C0(MBTreeNode* node, s32 alpha, s32 recurse)
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
        fn_800BA6C0(child, alpha, 2);
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
void fn_800BA784(MBTreeNode* node, s32 index, s32 recurse)
{
    MBTreeNode* child;

    goto outer_test;
outer_body:
    node->uvIndex = (s16)index;
    if (recurse != 0 && (child = node->child) != 0 &&
        (child->flags & 0x10) == 0) {
        fn_800BA784(child, index, 2);
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

/* 0x800BA820 */
void MBTreeInit(void) {}

/* 0x800BACF8 - MBNodeOrder: move node immediately before sibling. */
void fn_800BACF8(MBTreeNode* node, MBTreeNode* sibling)
{
    MBTreeNode* parent;
    MBTreeNode* current;
    MBTreeNode* previous;

    parent = sibling->parent;
    if (node->parent != parent)
        return;

    current = lbl_80344ECC;
    if (parent != 0)
        current = parent->child;
    if (current == 0 || current == sibling) {
        previous = 0;
    } else {
        previous = current;
        while (previous != 0 && previous->next != sibling)
            previous = previous->next;
    }

    if (previous == 0)
        parent->child = sibling->next;
    else
        previous->next = sibling->next;
    sibling->next = node->next;
    node->next = sibling;
}

/* 0x800BAD90 */
void fn_800BAD90(void) {}

/* 0x800BAD94 - MBNodeSetParent */
void fn_800BAD94(MBTreeNode* node, MBTreeNode* new_parent)
{
    MBTreeNode* old_parent;
    MBTreeNode* previous;
    MBTreeNode* root;

    old_parent = node->parent;
    root = lbl_80344ECC;
    if (old_parent == 0 || old_parent != new_parent) {
        if (old_parent == 0 || old_parent->child != node) {
            previous = fn_800BB588(node);
            if (previous != 0)
                previous->next = node->next;
        } else {
            old_parent->child = node->next;
        }

        node->next = 0;
        node->parent = new_parent;
        if (new_parent == 0) {
            root = node;
            if (lbl_80344ECC != 0) {
                previous = fn_800BB55C(lbl_80344ECC);
                previous->next = node;
                root = lbl_80344ECC;
            }
        } else if (new_parent->child == 0) {
            new_parent->child = node;
            root = lbl_80344ECC;
        } else {
            previous = fn_800BB55C(new_parent->child);
            previous->next = node;
            root = lbl_80344ECC;
        }
    }
    lbl_80344ECC = root;
}

/* 0x800BAEAC - MBRemoveNode */
MBTreeNode* fn_800BAEAC(MBTreeNode* node, s32 remove_children)
{
    MBTreeNode* parent;
    MBTreeNode* previous;
    MBTreeNode* child;
    MBTreeNode* tail;

    if (node == 0)
        return 0;
    if (node->type == 0)
        return 0;

    if (node->flags & 0x10000000) {
        *(f32*)(lbl_802C2A28 + (u32)node->uvScaleAddIndex * 16) = lbl_80348CA0;
        fn_800BA0FC(node, -1, 0);
    }

    parent = node->parent;
    lbl_80344ED0 = parent;
    if (remove_children) {
        fn_800BB164(node->child);
        node->child = 0;
    }

    if (node->type == 14 && node->special != 0) {
        parent = node->parent;
        if (parent == 0 || parent != lbl_80344EDC) {
            if (parent == 0 || parent->child != node) {
                previous = fn_800BB588(node);
                if (previous != 0)
                    previous->next = node->next;
            } else {
                parent->child = node->next;
            }
            node->next = 0;
            fn_800BB4CC(node, lbl_80344EDC);
        }
        fn_800D12F0(node);
        return 0;
    }

    if (parent == 0 || parent->child != node) {
        if (node == lbl_80344ECC) {
            lbl_80344ECC = node->child;
            if (lbl_80344ECC == 0) {
                lbl_80344ECC = node->next;
            } else {
                child = node->child;
                while (child->next != 0) {
                    child->parent = 0;
                    child = child->next;
                }
                child->parent = 0;
                child->next = node->next;
            }
        } else {
            child = lbl_80344ECC;
            if (node->parent != 0)
                child = node->parent->child;

            if (child == 0 || child == node) {
                previous = 0;
            } else {
                previous = child;
                while (previous != 0 && previous->next != node)
                    previous = previous->next;
            }

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
    } else if (node->child == 0) {
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

    node->type = 0;
    node->child = 0;
    node->parent = 0;
    node->next = lbl_80344EE0;
    lbl_80344EE0 = node;
    return 0;
}

/* 0x800BB164 */
void fn_800BB164(MBTreeNode* node)
{
    MBTreeNode* next;
    MBTreeNode* parent;

    while (node != 0) {
        if (node->child != 0)
            fn_800BB164(node->child);
        if (node->flags & 0x10000000) {
            *(f32*)(lbl_802C2A28 + (u32)node->uvScaleAddIndex * 16) = lbl_80348CA0;
            fn_800BA0FC(node, -1, 1);
        }
        parent = lbl_80344EDC;
        next = node->next;
        if (node->type == 14 && node->special != 0) {
            MBTreeNode* old_parent = node->parent;
            if (old_parent == 0 || old_parent != lbl_80344EDC) {
                if (old_parent == 0 || old_parent->child != node) {
                    MBTreeNode* previous = fn_800BB588(node);
                    if (previous != 0)
                        previous->next = node->next;
                } else {
                    old_parent->child = node->next;
                }
                node->next = 0;
                fn_800BB4CC(node, parent);
            }
            fn_800D12F0(node);
        } else {
            node->type = 0;
            node->child = 0;
            node->parent = 0;
            node->next = lbl_80344EE0;
            lbl_80344EE0 = node;
        }
        node = next;
    }
}

/* 0x800BB29C */
void fn_800BB29C(void) {}

/* 0x800BB3AC */
void fn_800BB3AC(void) {}

/* 0x800BB448 */
void fn_800BB448(void) {}

/* 0x800BB4CC */
void fn_800BB4CC(MBTreeNode* node, MBTreeNode* parent)
{
    MBTreeNode* tail;
    MBTreeNode* next;

    node->parent = parent;
    if (parent != 0) {
        tail = parent->child;
        if (tail == 0) {
            parent->child = node;
            return;
        }
        next = tail->next;
        while (next != 0 && next != tail) {
            tail = next;
            next = next->next;
        }
        tail->next = node;
        return;
    }

    tail = lbl_80344ECC;
    if (tail == 0) {
        lbl_80344ECC = node;
        return;
    }
    next = tail->next;
    while (next != 0 && next != lbl_80344ECC) {
        tail = next;
        next = next->next;
    }
    tail->next = node;
}

/* 0x800BB55C */
MBTreeNode* fn_800BB55C(MBTreeNode* node)
{
    MBTreeNode* result = node;
    MBTreeNode* next;

    for (next = node->next; next != 0 && next != node; next = next->next) {
        result = next;
    }
    return result;
}

/* 0x800BB588 */
MBTreeNode* fn_800BB588(MBTreeNode* node)
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
