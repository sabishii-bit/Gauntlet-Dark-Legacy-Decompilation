#include "types.h"

/* Midway "MB" model-buffer library main file (GCN MB_MAIN.CPP TU,
 * .text 0x800B6ED8-0x800B7758). This is the top of the MB graphics
 * library: the per-frame present/flush entry point (MBEndFrame), the
 * one-time library init/teardown driver (MBInit), and a fast 1/sqrt
 * table used by the MB perspective / distance-scale code.
 *
 * TU identity is pinned by the assert-watermark string "MB_MAIN.CPP:__LINE__"
 * (0x80115D90), referenced only by MBEndFrame. The neighbouring TUs in this
 * address run are separate objects (MB_FONT below at ~0x800B53B4, mb_lights
 * ending at 0x800B6ED8, and the objects.ngc/textures.ngc model loader above
 * at 0x800B7758) and are NOT part of this file.
 *
 * cflags_demo (-O4 no-peephole, -Cpp_exceptions on, -str reuse,readonly):
 * dtk attributes two extabindex entries here (MBEndFrame + MBInit), the
 * only two functions in this TU that save LR, confirming exceptions-on.
 *
 * Status: 3 of 4 functions byte-exact (MBEndFrame, MBInit, mbInvSqrtLookup).
 * The 1/sqrt table builder (mbInitInvSqrtTable) is parked NonMatching: the
 * target inlines the software square root (per-entry frsqrte + a 4-5 step
 * Newton-Raphson refinement, plus NaN/Inf domain guards and the int->double
 * magic-constant conversion), whereas this source emits a library sqrt() call.
 * Matching it byte-for-byte would require reproducing MWCC's inline-sqrt
 * intrinsic expansion (a numeric giant) and is left for a later pass.
 */

/* ---- MB_MAIN globals (resolved via symbols.txt / auto data objects) ---- */
extern u8* gWinGlobals;          /* 0x80344FC0 : window/model-mgr context ptr */
extern u32 sSeconds_80345150;    /* pbutils frame-time counter (owned by pbutils) */

extern u32 lbl_80344E80;         /* LastFrame : sSeconds latch for the frame gate */
extern s32 lbl_80344E6C;         /* CPU-time history write cursor (0..0x7F) */
extern u32 lbl_802A4B48[128];    /* per-frame pbGetCPUTime history ring */
extern vs32 lbl_80344E70;        /* render-thread busy flag (spun on) */
extern s32 lbl_80343EB8;         /* frame-budget threshold (0 = disabled) */
extern s32 lbl_802C45CC[16];     /* profiler block; +0x30 = elapsed CPU time */
extern s32 lbl_80344E7C;         /* slow-frame (over-budget) counter */
extern u8* lbl_80344EE8;         /* MB world context ptr; +0x84 = face-yaw state */
extern const char lbl_80115D90[]; /* "MB_MAIN.CPP:__LINE__" assert watermark */

extern s8 lbl_80344E78;          /* firsttime flag (signed char) */
extern s32 lbl_80344E74;         /* pending-(re)init flag */
extern s32 lbl_80344EC0;         /* cleared each MBInit */
extern u8 lbl_80344DA8;          /* MBInit registration blob (passed by address) */
extern u32 lbl_802A4B30[6];      /* current blit page block */
extern u32 lbl_80344E68;         /* MBInit CPU-time base */

extern f32 lbl_802A4D48[1001];   /* MB 1/sqrt lookup table (built by mbInitInvSqrtTable) */

/* ---- externs: functions in sibling TUs (resolved via symbols.txt) ---- */
extern void DEMOSwapBuffers(void);
extern void DEMODoneRender(void);
extern void pbPulseTime(void);
extern void pbSetTime(int t);
extern u32  pbGetCPUTime(void);
extern void pbWinSetup(void);
extern void pbInitGlobal(void);
extern void pbCloseGlobal(void);
extern void MBPsysStartFrame(void);       /* 0x800D1074 */
extern void MBResetBlits(void);           /* 0x800B4028 */
extern void InitFrontFaceYaw(void* p);    /* 0x800B9898 */
extern void GXSetZMode(int enable, int func, int update);

extern void fn_800C0FE4(void);
extern void fn_800C3674(void);
extern void fn_800C1624(void);
extern void fn_800AF1D8(int a);
extern void fn_800C1148(int a, int b, const char* s);
extern void fn_800C702C(void);
extern void fn_800C79E4(void);
extern void fn_800C5B1C(void);
extern void fn_800C0AA4(int layer);
extern void fn_800C1170(int a, void* b, int c);
extern void fn_800C2F50(f32 w, f32 h);
extern void fn_800B9E4C(void);
extern void fn_800B8AB8(void);
extern void fn_800BA820(void);
extern void fn_800BC23C(void);
extern void fn_800B6D38(int a, int b, int c);   /* mb_font/mb_lights reset */
extern void fn_800B6D64(void);
extern void fn_800B6C94(void);
extern void fn_800B5D90(void);

extern f64 sqrt(f64 x);

/* forward decls for this TU */
void mbInitInvSqrtTable(void);
f32 mbInvSqrtLookup(f64 x);

/* MBEndFrame @0x800B6ED8 : end-of-frame present + window-layer flush. */
void MBEndFrame(void) {
    u8* wg = gWinGlobals;
    u8 unused[8];

    DEMOSwapBuffers();
    /* pace to the frame boundary (~0x22 ticks since the last present) */
    while ((u32)(sSeconds_80345150 - lbl_80344E80) < 0x22) {
        pbPulseTime();
    }
    lbl_80344E80 = sSeconds_80345150;

    if (*(s32*)(*(u8**)(wg + 0x10) + 0x10) == 1) {
        fn_800B5D90();
        return;
    }

    fn_800C0FE4();
    lbl_80344E6C = lbl_80344E6C + 1;
    lbl_80344E6C = lbl_80344E6C & 0x7F;
    lbl_802A4B48[lbl_80344E6C] = pbGetCPUTime();

    while (lbl_80344E70 != 0) {
        /* wait for the render thread */
    }
    fn_800C3674();

    if (lbl_80343EB8 != 0 && lbl_802C45CC[12] > lbl_80343EB8) {
        lbl_80344E7C++;
    }

    pbSetTime(0);
    fn_800C0AA4(5);
    fn_800C0AA4(4);
    fn_800C0AA4(3);

    (*(s32*)(*(u8**)(wg + 0x10) + 0x4))++;

    fn_800C1624();
    fn_800AF1D8(0);
    fn_800C1148(0, 0, lbl_80115D90);
    pbWinSetup();
    fn_800C1148(0, 0, lbl_80115D90);
    pbGetCPUTime();
    MBPsysStartFrame();

    fn_800C0AA4(0);
    fn_800C0AA4(1);
    fn_800C0AA4(2);
    fn_800C0AA4(8);
    fn_800C0AA4(0xa);
    fn_800C0AA4(0xd);
    fn_800C0AA4(0xe);
    fn_800C0AA4(0xb);
    fn_800C0AA4(0xc);
    fn_800C0AA4(0xf);
    fn_800C0AA4(0x10);
    fn_800C0AA4(0x11);
    fn_800C0AA4(0x12);
    fn_800C0AA4(6);
    fn_800C0AA4(9);
    fn_800C0AA4(0x13);
    fn_800C0AA4(7);
    fn_800C0AA4(0x14);
    fn_800C0AA4(0x15);

    fn_800C702C();
    InitFrontFaceYaw(lbl_80344EE8 + 0x84);
    fn_800C79E4();
    fn_800B5D90();
    MBResetBlits();
    pbGetCPUTime();
    DEMODoneRender();
    GXSetZMode(1, 6, 1);
}

/* MBInit @0x800B70EC : bring the MB library up (or tear it down) and reset
 * the per-subsystem frame state.  The firsttime latch selects init vs close. */
void MBInit(void) {
    if (!lbl_80344E78) {
        lbl_80344E74 = 1;
        lbl_80344E78 = 1;
    }
    lbl_80344EC0 = 0;

    if (lbl_80344E74) {
        pbInitGlobal();
        fn_800C1170(0x145, &lbl_80344DA8, 0);
        fn_800B6D38(0, 0, 0);
        lbl_80344E74 = 0;
    } else {
        pbCloseGlobal();
    }

    lbl_802A4B30[0] = 0;
    lbl_80344E68 = pbGetCPUTime();
    fn_800B9E4C();
    mbInitInvSqrtTable();
    fn_800C2F50(512.0f, 384.0f);
    fn_800B8AB8();
    fn_800BA820();
    fn_800BC23C();
    fn_800B6D64();
    fn_800B6C94();
    fn_800C5B1C();
}

/* mbInvSqrtLookup @0x800B71AC : fast 1/sqrt(x) via the piecewise-scaled
 * lookup table built by mbInitInvSqrtTable.  The magnitude of x selects a
 * 200-entry block; the fractional position within the block indexes it. */
f32 mbInvSqrtLookup(f64 x) {
    f32* t = lbl_802A4D48;
    int idx;

    if (x >= 2000000.0) {
        idx = 1000;
    } else if (x >= 100000.0) {
        idx = (int)(x * 0.0001) + 800;
    } else if (x >= 4000.0) {
        idx = (int)(x * 0.002) + 600;
    } else if (x >= 200.0) {
        idx = (int)(x * 0.05) + 400;
    } else if (x >= 10.0) {
        idx = (int)x + 200;
    } else {
        idx = (int)(x * 20.0);
    }
    return t[idx];
}

/* mbInitInvSqrtTable @0x800B729C : precompute lbl_802A4D48 as 1/sqrt over
 * five decades of scale (block k = 0.05, 1, 20, 500, 10000), each 200
 * entries, plus a clamp value at index 1000.  NonMatching: relies on the
 * compiler's inline sqrt/divide sequence. */
void mbInitInvSqrtTable(void) {
    f32* p = lbl_802A4D48;
    int i;

    for (i = 1; i <= 200; i++) {
        *p++ = (f32)(1.0 / sqrt(0.05 * (f64)i));
    }
    for (i = 1; i <= 200; i++) {
        *p++ = (f32)(1.0 / sqrt((f64)i));
    }
    for (i = 1; i <= 200; i++) {
        *p++ = (f32)(1.0 / sqrt(20.0 * (f64)i));
    }
    for (i = 1; i <= 200; i++) {
        *p++ = (f32)(1.0 / sqrt(500.0 * (f64)i));
    }
    for (i = 1; i <= 200; i++) {
        *p++ = (f32)(1.0 / sqrt(10000.0 * (f64)i));
    }
    *p = 1e-5f;
}
