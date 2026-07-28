/*
 * mb_util.c - MB scene-graph matrix utilities (MB_UTIL.OBJ).
 *
 * PointVisible (thin wrapper over MBWorldSphereVisible), GetWorldMat (walk a
 * node's children accumulating world position/transform), and UnparentMatrix
 * (recursive tree walk that transposes + multiplies out the parent transform).
 * All operate on the MB node struct (+0x30 pos, +0x60 flags, +0x74 link) via the
 * mat44 helpers in ml_fmath (MulMat4 MulMat4, fn_800BE7E4 transpose,
 * CopyMat4 CopyMat4).
 *
 * Range 0x800BB5F4..0x800BB804 (3 fns), between mb_tree.c and mb_window.c. Uses
 * neither module's sdata2 pool (pure matrix math). Names from Xbox shell3D PDB
 * (MB_UTIL.OBJ; the GetScreen/safe_expf/fog helpers appear dead-stripped on GC).
 * cflags_demo, C++ exceptions on.
 *
 * Status: NonMatching wired skeleton (stubs). Full bodies not reconstructed.
 */
#include "types.h"

extern int MBWorldSphereVisible(f32* a, f32* b, f32* c);
extern void CopyMat4(void* src, f32* dst);
extern void MulMat4(void* a, f32* b, f32* dst);
extern void InvertMat4(void* src, f32* dst);

typedef struct MBUtilNode {
    /* 0x00 */ f32 matrix[16];
    /* 0x40 */ u8 _pad40[0x20];
    /* 0x60 */ u32 flags;
    /* 0x64 */ u8 _pad64[0x10];
    /* 0x74 */ struct MBUtilNode* next;
} MBUtilNode;

/* 0x800BB5F4 */
int PointVisible(f32* a, f32* b, f32* c) {
    return MBWorldSphereVisible(a, b, c);
}

/* 0x800BB614 */
int GetWorldMat(MBUtilNode* node, f32* matrix, f32* offset)
{
    MBUtilNode* parent;
    int translationOnly;

    CopyMat4(node, matrix);
    if (offset != NULL) {
        matrix[12] += offset[0];
        matrix[13] += offset[1];
        matrix[14] += offset[2];
    }

    translationOnly = (node->flags & 4) ? 1 : 0;
    parent = node->next;
    while (parent != NULL) {
        if ((parent->flags & 4) == 0) {
            MulMat4(parent, matrix, matrix);
            translationOnly = 0;
        } else {
            matrix[12] = parent->matrix[12] + matrix[12];
            matrix[13] = parent->matrix[13] + matrix[13];
            matrix[14] = parent->matrix[14] + matrix[14];
        }
        parent = parent->next;
    }
    return translationOnly;
}

/* 0x800BB704 */
void UnparentMatrix(f32* matrix, MBUtilNode* node)
{
    f32 inverseNode[16];
    f32 inverseParent[16];
    MBUtilNode* parent;

    if (node != NULL) {
        if ((parent = node->next) != NULL) {
            UnparentMatrix(matrix, parent->next);
            if ((parent->flags & 4) == 0) {
                InvertMat4(parent, inverseParent);
                MulMat4(inverseParent, matrix, matrix);
            } else {
                matrix[12] -= parent->matrix[12];
                matrix[13] -= parent->matrix[13];
                matrix[14] -= parent->matrix[14];
            }
        }

        if ((node->flags & 4) == 0) {
            InvertMat4(node, inverseNode);
            MulMat4(inverseNode, matrix, matrix);
        } else {
            matrix[12] -= node->matrix[12];
            matrix[13] -= node->matrix[13];
            matrix[14] -= node->matrix[14];
        }
    }
}
