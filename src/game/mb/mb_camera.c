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
 * Status: NonMatching - reconstructed from the GCN implementation; remaining
 * differences are compiler scheduling/register allocation.
 */
#include "types.h"

extern u8* gWinGlobals;    /* 0x80344FC0 : window/model-mgr context */
extern u8* lbl_80344EE8;
extern s32 lbl_80344E08;
extern s32 lbl_80344E0C;
extern f32 lbl_8029E378[20];

extern void pbProjCalc(void);
extern void pbCameraCalc(void);
extern void pbInitCamera(f32* position, f32* look);
extern void vec4ApplyTrans__FR4vec4R4vec4R5mat44(f32* dst, f32* src, f32* m);
extern void CopyMat3(f32* src, f32* dst);

extern const f32 lbl_80348B20;
extern const f64 lbl_80348B28;
extern const f64 lbl_80348B30;
extern const f32 lbl_80348B38;
extern const f32 lbl_80348B3C;
extern const f32 lbl_80348B40;

int MBWorldSphereClip(f32* sphere, f32 radius);

/* 0x800B582C - MBCameraUpdate : per-frame MB camera / projection setup. */
void MBCameraUpdate(f32* position, f32* matrix)
{
    f32* saved = lbl_8029E378;
    int row;
    f32* camera = (f32*)lbl_80344EE8;
    f32* copied = &camera[25];
    f32* view3 = &camera[41];
    f32* inverse = &camera[57];
    f32 x;
    f32 y;
    f32 z;

    for (row = 0; row < 3; row++) {
        matrix[row * 4 + 3] = lbl_80348B3C;
    }
    matrix[15] = lbl_80348B20;
    pbInitCamera(position, &matrix[8]);

    lbl_80344E08 = 1;
    saved[16] = position[0];
    saved[17] = -position[1];
    saved[18] = position[2];
    saved[0] = matrix[0];
    saved[1] = matrix[1];
    saved[2] = matrix[2];
    saved[4] = matrix[4];
    saved[5] = matrix[5];
    saved[6] = matrix[6];
    saved[8] = matrix[8];
    saved[9] = matrix[9];
    saved[10] = matrix[10];
    saved[12] = matrix[12];
    saved[13] = matrix[13];
    saved[14] = matrix[14];

    if (lbl_80344E0C != 0) {
        if (position[0] == lbl_80348B3C &&
            position[1] == lbl_80348B3C &&
            position[2] == lbl_80348B3C) {
            position[2] = lbl_80348B40;
        } else {
            lbl_80344E0C = 0;
        }
    }

    inverse[0] = lbl_80348B20;
    inverse[1] = lbl_80348B3C;
    inverse[2] = lbl_80348B3C;
    inverse[4] = lbl_80348B3C;
    inverse[5] = lbl_80348B20;
    inverse[6] = lbl_80348B3C;
    inverse[8] = lbl_80348B3C;
    inverse[9] = lbl_80348B3C;
    inverse[10] = lbl_80348B20;
    inverse[12] = lbl_80348B3C;
    inverse[13] = lbl_80348B3C;
    inverse[14] = lbl_80348B3C;

    CopyMat3(matrix, copied);
    copied[12] = position[0];
    copied[13] = position[1];
    copied[14] = position[2];
    copied[15] = lbl_80348B20;

    {
        int dstOffset = 0;
        int srcOffset = 0;

        row = 0;
        do {
            f32* src = (f32*)((u8*)matrix + srcOffset);
            f32* dst = (f32*)((u8*)view3 + dstOffset);
            f32 z = lbl_80348B3C;
            int col;

            for (col = 0; col < 3; col++) {
                dst[col] = src[col * 4];
            }
            dst[3] = z;
            row++;
            *(f32*)((u8*)view3 + srcOffset + 48) =
                *(f32*)((u8*)position + srcOffset);
            srcOffset += 4;
            dstOffset += 16;
        } while (row < 3);
    }
    view3[15] = lbl_80348B20;

    x = -view3[13];
    y = -view3[12];
    z = -view3[14];
    view3[12] = view3[4] * x + view3[0] * y + view3[8] * z;
    view3[13] = view3[5] * x + view3[1] * y + view3[9] * z;
    view3[14] = view3[6] * x + view3[2] * y + view3[10] * z;
}

/* 0x800B5554 - MBWorldToScreen : project a world point to screen space. */
void MBWorldToScreen(f32* dst, f32* world)
{
    f32 invW;
    f32 portWidth;
    f32 portHeight;
    u8* globals = gWinGlobals;

    if ((*(u8**)(globals + 4))[3] != 0 ||
        *(s32*)(*(u8**)(globals + 0x10) + 0x40) != 0) {
        pbProjCalc();
    }
    if ((*(u8**)(globals + 4))[2] != 0) {
        pbCameraCalc();
    }
    vec4ApplyTrans__FR4vec4R4vec4R5mat44(
        dst, world, (f32*)(*(u8**)(globals + 4) + 0x2C0));

    invW = lbl_80348B20 / dst[3];
    portWidth = (f32)*(s32*)(*(u8**)(globals + 0x10) + 0x20);
    portHeight = (f32)*(s32*)(*(u8**)(globals + 0x10) + 0x24);
    dst[0] = (lbl_80348B38 * portWidth + dst[0] * invW) -
             *(f32*)(*(u8**)(globals + 0x10) + 0x38);
    dst[1] = (lbl_80348B38 * portHeight + dst[1] * invW) -
             *(f32*)(*(u8**)(globals + 0x10) + 0x3C);
    dst[0] *= (f32)*(s32*)(*(u8**)(globals + 0x10) + 0x28) / portWidth;
    dst[1] *= (f32)*(s32*)(*(u8**)(globals + 0x10) + 0x2C) / portHeight;
    dst[2] = dst[3];
    dst[3] = lbl_80348B20;
}

/* 0x800B53B4 - MBWorldToScreen3D : project a world point (with depth). */
void MBWorldToScreen3D(f32* dst, f32* world)
{
    u8* globals = gWinGlobals;
    u8* camera;
    u8* viewport;
    f32 projected[3];
    f32 z;
    f32 width;
    f32 height;
    f32 outputWidth;
    f32 outputHeight;

    if ((*(u8**)(globals + 4))[3] != 0 ||
        *(s32*)(*(u8**)(globals + 0x10) + 0x40) != 0) {
        pbProjCalc();
    }
    if ((*(u8**)(globals + 4))[2] != 0) {
        pbCameraCalc();
    }

    camera = *(u8**)(globals + 4);
    viewport = *(u8**)(globals + 0x10);
    z = world[2];
    width = (f32)*(s32*)(viewport + 0x20);
    height = (f32)*(s32*)(viewport + 0x24);
    outputWidth = (f32)*(s32*)(viewport + 0x28);
    outputHeight = (f32)*(s32*)(viewport + 0x2C);

    projected[0] =
        -(z * *(f32*)(camera + 0xF0) -
          (*(f32*)(viewport + 0x38) -
           (lbl_80348B28 * width - world[0] * (width / outputWidth))) *
              z) /
        (*(f32*)(camera + 0x80) * *(f32*)(camera + 0xC0));
    projected[1] =
        -(z * *(f32*)(camera + 0xF4) -
          (*(f32*)(viewport + 0x3C) -
           (lbl_80348B28 * height - world[1] * (height / outputHeight))) *
              z) /
        (*(f32*)(camera + 0x94) * *(f32*)(camera + 0xD4));
    projected[2] = z;

    vec4ApplyTrans__FR4vec4R4vec4R5mat44(
        dst, projected, (f32*)(camera + 0x240));
}

/* 0x800B5738 - MBWorldSphereClip : transform a sphere centre and test it
 * against the view frustum. NonMatching stub. */
int MBWorldSphereClip(f32* sphere, f32 radius)
{
    f32 transformed[3];
    f32 bound;
    f32 scaled;
    u8* globals = gWinGlobals;
    u8* camera;

    vec4ApplyTrans__FR4vec4R4vec4R5mat44(
        transformed, sphere, (f32*)(*(u8**)(globals + 4) + 0x200));
    camera = *(u8**)(globals + 4);
    if (transformed[2] < *(f32*)(camera + 0x58) - radius) {
        return 6;
    }
    if (transformed[2] > *(f32*)(camera + 0x5C) + radius) {
        return 5;
    }
    bound = transformed[2] * *(f32*)(camera + 0x60) + radius;
    scaled = transformed[0] * *(f32*)(camera + 0x64);
    if (scaled > bound) {
        return 2;
    }
    if (scaled < -bound) {
        return 1;
    }
    bound = transformed[2] * *(f32*)(camera + 0x68) + radius;
    scaled = transformed[1] * *(f32*)(camera + 0x6C);
    if (scaled > bound) {
        return 3;
    }
    if (scaled < -bound) {
        return 4;
    }
    return 0;
}

/* 0x800B56B4 - MBWorldSphereVisible : sphere visibility (calls clip). */
int MBWorldSphereVisible(f32* sphere, f32 radius)
{
    f32 copy[3];

    copy[0] = sphere[0];
    copy[1] = sphere[1];
    copy[2] = sphere[2];
    if (MBWorldSphereClip(copy, radius) != 0) {
        return 0;
    }
    return 1;
}

/* 0x800B5704 - MBWorldSphereVisible3 : sphere visibility variant (calls clip). */
int MBWorldSphereVisible3(f32* sphere, f32 radius)
{
    if (MBWorldSphereClip(sphere, radius) != 0) {
        return 0;
    }
    return 1;
}
