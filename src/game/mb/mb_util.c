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

/* 0x800BB5F4 */
int PointVisible(f32* a, f32* b, f32* c) {
    return MBWorldSphereVisible(a, b, c);
}

/* 0x800BB614 */
void GetWorldMat(void) {}

/* 0x800BB704 */
void UnparentMatrix(void) {}
