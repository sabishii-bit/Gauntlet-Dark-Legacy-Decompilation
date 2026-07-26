/*
 * mb_camera.c - MB camera / world<->screen projection (mb_camera.obj).
 *
 * The MB library's camera-space helpers: per-frame camera/projection setup
 * (MBCameraUpdate, called from main + camera/newcam/bosscam), world->screen
 * point projection used all over the HUD and menu code (get_screen_pos et al.
 * call these), and bounding-sphere frustum visibility tests. Built on the pb
 * projection primitives (pbProjCalc/pbCameraCalc/pbInitCamera) and vec4ApplyTrans.
 *
 * Address range 0x800B53B4..0x800B5AA8 (6 functions), sitting between mb_blit
 * (ends 0x800B53B0) and mb_font (starts 0x800B5AA8). Delimited from mb_font by
 * its own sdata2 pool (0x80348B20..0x80348B44, disjoint from mb_font's 0x80348B48+)
 * and by callee signature (projection math, no font/message globals). Names come
 * from the Xbox shell3D PDB (mb_camera.obj); the world/screen-projection pairing
 * to Xbox names is by callee + caller behaviour (MBScreenToWorld/3D appear to be
 * inlined on GC, so only 6 of the 8 PDB functions are present).
 *
 * cflags_demo (-O4 no-peephole, -Cpp_exceptions on, -str reuse,readonly).
 *
 * Status: NonMatching - documented stubs (call/flow shape only); the FP
 * projection math is not reconstructed.
 */
#include "types.h"

extern void* gWinGlobals;    /* 0x80344FC0 : window/model-mgr context */

extern void pbProjCalc();
extern void pbCameraCalc();
extern void pbInitCamera();
extern void vec4ApplyTrans__FR4vec4R4vec4R5mat44(f32* dst, f32* src, f32* m);
extern void* fn_800BE8C8();

/* 0x800B582C - MBCameraUpdate : per-frame MB camera / projection setup. */
void MBCameraUpdate(void)
{
    pbInitCamera();
    fn_800BE8C8();
}

/* 0x800B5554 - MBWorldToScreen : project a world point to screen space. */
void MBWorldToScreen(f32* dst, f32* world)
{
    pbProjCalc();
    pbCameraCalc();
    vec4ApplyTrans__FR4vec4R4vec4R5mat44(dst, world, (f32*)0);
}

/* 0x800B53B4 - MBWorldToScreen3D : project a world point (with depth). */
void MBWorldToScreen3D(f32* dst, f32* world)
{
    pbProjCalc();
    pbCameraCalc();
    vec4ApplyTrans__FR4vec4R4vec4R5mat44(dst, world, (f32*)0);
}

/* 0x800B5738 - MBWorldSphereClip : transform a sphere centre and test it
 * against the view frustum. NonMatching stub. */
int MBWorldSphereClip(f32* sphere)
{
    vec4ApplyTrans__FR4vec4R4vec4R5mat44((f32*)0, sphere, (f32*)0);
    return 0;
}

/* 0x800B56B4 - MBWorldSphereVisible : sphere visibility (calls clip). */
int MBWorldSphereVisible(f32* sphere)
{
    return MBWorldSphereClip(sphere);
}

/* 0x800B5704 - MBWorldSphereVisible3 : sphere visibility variant (calls clip). */
int MBWorldSphereVisible3(f32* sphere)
{
    return MBWorldSphereClip(sphere);
}
