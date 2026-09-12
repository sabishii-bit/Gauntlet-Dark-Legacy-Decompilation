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
 * its referenced sdata2 run (0x80348B20..0x80348B44, before mb_font's 0x80348B48+)
 * and by callee signature (projection math, no font/message globals). Only
 * 0x80348B30..0x80348B38 is currently claimed by this TU. Names come
 * from an earlier Xbox shell3D PDB (mb_camera.obj) correspondence. Some legacy
 * GC names disagree with the actual direction of projection; see below.
 *
 * Stock GC 1.2.5, cflags_demo (-O4, -Cpp_exceptions on, -str reuse,readonly).
 *
 * Status: NonMatching - five native bodies are exact. The remaining inverse
 * projection residual has not yet been resolved through source reconstruction.
 */
#include "types.h"
#include "game/pbwindow.h"

#ifndef offsetof
#define offsetof(type, memb) ((u32) & ((type*)0)->memb)
#endif

/* File-local mirror of pb_window.c's PBSCREEN (no Xbox PDB counterpart;
 * layout re-verified against this TU's own MBWorldToScreen[3D] target asm,
 * consistent with pb_window.c's reconstruction). The old pad2@0x28 is split
 * into the two dwords the GC actually reads: MBWorldToScreen3D forms the
 * per-axis scale as w/+0x28 and h/+0x2C, and MBWorldToScreen scales the
 * projected point by +0x28/w and +0x2C/h, so the pair is the reference
 * extent that w/h are expressed against. That is a GC-verified partial view
 * of their ROLE, not a recovered original field name; pb_window.c never
 * reads either dword. */
typedef struct PBSCREEN {
    /* 0x00 */ u32 flags;
    /* 0x04 */ u8 pad[0x1C];
    /* 0x20 */ s32 w;
    /* 0x24 */ s32 h;
    /* 0x28 */ s32 wref;
    /* 0x2C */ s32 href;
    /* 0x30 */ s32 w2;
    /* 0x34 */ s32 h2;
    /* 0x38 */ f32 xoff;
    /* 0x3C */ f32 yoff;
    /* 0x40 */ s32 dirty;
} PBSCREEN;

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

/* 0x800B53B4 - legacy GC name retained: this is screen-to-world using the
 * supplied view-space depth (PS2/Xbox MBScreenToWorld), not forward projection.
 * The four-component view matches vec4ApplyTrans's input and the Xbox view[4]
 * local; its fourth component is not initialized because that helper reads
 * only X/Y/Z. The pre-existing eight-byte reservation remains unrecovered:
 * removing it with this view extent changes the frame from 136 to 128 bytes. */
void MBWorldToScreen3D(f32* dst, f32* world)
{
    PBWINGLOBALS* globals = gWinGlobals;
    PBSCREEN* viewport;
    PBWINDOW* camera;
    f32 xScale;
    f32 yScale;
    f32 xNumerator;
    f32 yNumerator;
    f32 yDenomA;
    f32 yDepthScale;
    f32 yDenomB;
    f64 centeredX;
    f64 centeredY;
    u8 unused[8];
    f32 projected[4];

    if (globals->current->proj_dirty != 0 ||
        ((PBSCREEN*)globals->screen)->dirty != 0) {
        pbProjCalc();
    }
    if (globals->current->cam_dirty != 0) {
        pbCameraCalc();
    }

    projected[2] = world[2];
    viewport = (PBSCREEN*)globals->screen;
    camera = globals->current;
    xScale = (f32)viewport->w / (f32)viewport->wref;
    yScale = (f32)viewport->h / (f32)viewport->href;
    centeredX =
        (f64)(world[0] * xScale) -
        lbl_80348B28 * (f64)viewport->w;
    centeredY =
        (f64)(world[1] * yScale) -
        lbl_80348B28 * (f64)viewport->h;
    yDenomB = camera->viewport[1][1];
    yDenomA = camera->projection[1][1];
    yDepthScale = camera->viewport[3][1];
    yNumerator =
        (f32)((f64)viewport->yoff + centeredY);
    xNumerator =
        (f32)((f64)viewport->xoff + centeredX);
    xNumerator *= projected[2];
    yNumerator *= projected[2];
    xNumerator -= projected[2] * camera->viewport[3][0];
    projected[0] = xNumerator /
        (camera->projection[0][0] * camera->viewport[0][0]);
    yNumerator -= projected[2] * yDepthScale;
    projected[1] = yNumerator / (yDenomA * yDenomB);

    vec4ApplyTrans__FR4vec4R4vec4R5mat44(
        dst, projected, (f32*)globals->current->icamera);
}

/* 0x800B5554 - MBWorldToScreen : project a world point to screen space. */
void MBWorldToScreen(f32* dst, f32* world)
{
    f32 invW;
    f32 portWidth;
    f32 portHeight;
    PBWINGLOBALS* globals = gWinGlobals;

    if (globals->current->proj_dirty != 0 ||
        ((PBSCREEN*)globals->screen)->dirty != 0) {
        pbProjCalc();
    }
    if (globals->current->cam_dirty != 0) {
        pbCameraCalc();
    }
    vec4ApplyTrans__FR4vec4R4vec4R5mat44(
        dst, world, (f32*)globals->current->world_screen);

    invW = lbl_80348B20 / dst[3];
    portWidth = (f32)((PBSCREEN*)globals->screen)->w;
    portHeight = (f32)((PBSCREEN*)globals->screen)->h;
    dst[0] = (lbl_80348B38 * portWidth + dst[0] * invW) -
             ((PBSCREEN*)globals->screen)->xoff;
    dst[1] = (lbl_80348B38 * portHeight + dst[1] * invW) -
             ((PBSCREEN*)globals->screen)->yoff;
    dst[0] *= (f32)((PBSCREEN*)globals->screen)->wref / portWidth;
    dst[1] *= (f32)((PBSCREEN*)globals->screen)->href / portHeight;
    dst[2] = dst[3];
    dst[3] = lbl_80348B20;
}

/* 0x800B56B4 - MBWorldSphereVisible : sphere visibility (calls clip).
 * Xbox MBWorldSphereVisible3 records center_w as float[4]. Only X/Y/Z are
 * copied and consumed by the transform path; the fourth component is unused. */
int MBWorldSphereVisible(f32* sphere, f32 radius)
{
    f32 copy[4];

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

/* 0x800B5738 - MBWorldSphereClip : transform a sphere centre and test it
 * against the view frustum. vec4ApplyTrans writes all four components into
 * center_v (also float[4] in the Xbox PDB), though clipping reads only X/Y/Z. */
int MBWorldSphereClip(f32* sphere, f32 radius)
{
    f32 transformed[4];
    f32 bound;
    f32 scaled;
    PBWINGLOBALS* globals = gWinGlobals;
    PBWINDOW* camera;

    vec4ApplyTrans__FR4vec4R4vec4R5mat44(
        transformed, sphere, (f32*)globals->current->camera);
    camera = globals->current;
    if (transformed[2] < camera->near_z - radius) {
        return 6;
    }
    if (transformed[2] > camera->far_z + radius) {
        return 5;
    }
    bound = transformed[2] * camera->hva_sin_x + radius;
    scaled = transformed[0] * camera->hva_cos_x;
    if (scaled > bound) {
        return 2;
    }
    if (scaled < -bound) {
        return 1;
    }
    bound = transformed[2] * camera->hva_sin_y + radius;
    scaled = transformed[1] * camera->hva_cos_y;
    if (scaled > bound) {
        return 3;
    }
    if (scaled < -bound) {
        return 4;
    }
    return 0;
}

/* 0x800B582C - MBCameraUpdate : per-frame MB camera / projection setup. */
void MBCameraUpdate(f32* position, f32* matrix)
{
    f32* saved = lbl_8029E378;
    int row;
    f32* camera = (f32*)lbl_80344EE8;
    f32* copied = &camera[25];
    f32* view3 = &camera[41];
    f32* inverse = &camera[57];
    f32 y;
    f32 x;
    f32 z;
    int col;
    f32 (*inputRows)[4];
    f32 (*outputRows)[4];

    z = lbl_80348B3C;
    for (row = 0; row < 3; row++) {
        matrix[row * 4 + 3] = z;
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
        if (lbl_80348B3C == position[0] &&
            lbl_80348B3C == position[1] &&
            lbl_80348B3C == position[2]) {
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

    row = 0;
    z = lbl_80348B3C;
    inputRows = (f32 (*)[4])matrix;
    outputRows = (f32 (*)[4])view3;
    do {
        for (col = 0; col < 3; col++) {
            outputRows[row][col] = inputRows[col][row];
        }
        outputRows[row][3] = z;
        outputRows[3][row] = position[row];
        row++;
    } while (row < 3);
    view3[15] = lbl_80348B20;

    x = -view3[13];
    y = -view3[12];
    z = -view3[14];
    view3[12] = view3[0] * y + view3[4] * x + view3[8] * z;
    view3[13] = view3[1] * y + view3[5] * x + view3[9] * z;
    view3[14] = view3[2] * y + view3[6] * x + view3[10] * z;
}
