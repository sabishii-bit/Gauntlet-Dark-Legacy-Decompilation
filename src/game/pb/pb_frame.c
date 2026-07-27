/* pb_frame.c -- Midway "pb" library frame / display-mode TU.
 *
 * Xbox counterpart: PB_FRAME.OBJ (shell3D.pdb: pbCloseFrame / MBInitFrameMode
 * / MBScreenWidth / MBScreenHeight / MBSetFrameMode / pbSetupFrameSize /
 * pbFrameDebugGrab / pbFrameDebugSet / pbFrameMode / pbFrameClear / ... /
 * pbSetupFrame). Confirmed by the assert literal "PB_FRAME.C:__LINE__"
 * (0x8011656C) referenced by fn_800C151C.
 *
 * The GameCube build drops the PC-only debug helpers and reorders the rest;
 * the surviving static/count split does not line up with the Xbox source
 * order, so only the size/behaviour-anchored functions are named:
 *   pbFrameMode   (0x800C16F8, the 0xEF8 mode machine),
 *   MBScreenHeight/MBScreenWidth (0x800C31A4/0x800C31B4).
 * fn_800C31C4/32D0/33D0/33EC are referenced by name from the Matching
 * pb_global.c; fn_800C1624/2F50 from mb_main.c -- all must stay fn_.
 *
 * NOTE: pbInitGlobal/pbCloseGlobal/pbSetupPBGPtrs live in the already-wired
 * pb_global.c ABOVE 0x800C33FC -- this TU stops at 0x800C33FC.
 *
 * .text 0x800C151C-0x800C33FC. Compiled -Cpp_exceptions on (cflags_demo).
 * NonMatching: structural skeletons only.
 */

#include "types.h"

typedef struct MBScreen {
    s32 m00;           /* 0x00 */
    s32 m04;           /* 0x04 */
    s32 m08;           /* 0x08 */
    u8* frames;        /* 0x0C : frame-buffer base */
    s32 m10;           /* 0x10 : pending mode */
    s32 m14;           /* 0x14 : requested mode */
    s32 m18;           /* 0x18 : default mode */
    u8  _pad1c[4];
    s32 width;         /* 0x20 */
    s32 height;        /* 0x24 */
    s32 m28;           /* 0x28 */
    s32 m2c;           /* 0x2C */
    u8  _pad30[8];
    f32 f38;           /* 0x38 */
    f32 f3c;           /* 0x3C */
    s32 m40;           /* 0x40 */
    u8  _pad44[4];
    s32 m48;           /* 0x48 : force-refresh flag */
} MBScreen;

/* projection block (gWinGlobals->proj; shared with pb_winglobals) */
typedef struct MBProj {
    f32 f00;           /* 0x00 */
    f32 f04;           /* 0x04 */
    s32 m08;           /* 0x08 */
    s32 m0c;           /* 0x0C */
    s32 m10;           /* 0x10 */
    s32 m14;           /* 0x14 */
} MBProj;

typedef struct WinGlobals {
    u8       _pad00[8];
    u8*      frame;    /* 0x08 : current frame pointer */
    u8       _pad0c[4];
    MBScreen* screen;  /* 0x10 */
    u8       _pad14[0x24];
    MBProj*  proj;     /* 0x38 */
    void* volatile m60; /* 0x3C */
} WinGlobals;

extern WinGlobals* gWinGlobals;

/* frame-control block (*lbl_80343EFC) */
typedef struct PbFrameCtl {
    s32 m00;           /* 0x00 */
    u8  _pad04[0xc];
    s32 m10;           /* 0x10 */
    u8  _pad14[4];
    s32 m18;           /* 0x18 : current frame index */
    u8  _pad1c[0xc];
    u8* m28;           /* 0x28 */
    u8* m2c;           /* 0x2C */
} PbFrameCtl;

extern PbFrameCtl* lbl_80343EFC;
extern u32* lbl_80343F20;
extern u32* lbl_80343E7C;
extern u32* lbl_80343E80;
extern u32* lbl_80343E84;
extern u32* lbl_80343E88;
extern u32* lbl_80343E8C;
extern u32* lbl_80343E90;
extern u32* lbl_80343E94;
extern u32* lbl_80343E98;
extern u32* lbl_80343E9C;
extern s32 lbl_80343F08;
extern s32 lbl_80343F0C;
extern volatile s32 lbl_80344F98;
extern volatile u32 lbl_80344F9C;
extern u32 lbl_80344FAC, lbl_80344FB0, lbl_80344FB8;
extern s32 lbl_80344FC4;
extern u8 lbl_802C51E0[];    /* default screen block */
extern u8 lbl_802C4DE0[];    /* default frame buffers (0x450) */
extern char lbl_8011656C[]; /* "PB_FRAME.C:__LINE__" */

extern void pbFrameMode(s32 mode, s32 flag);
extern void MBBlitUpdateWindow(f32 sx, f32 sy);
extern void fn_800B69DC(f32 sx, f32 sy);
extern void fn_800C116C(s32 code, char* file);
extern void fn_800C2618(void);
extern void fn_800C2C74(void);
extern void fn_800AF1E0(void);
extern void fn_800AF1E8(void);
extern u32 fn_800AF55C(void);

/* Latch a frame buffer + publish its packet pointers
 * (references PB_FRAME.C:__LINE__). */
void fn_800C151C(s32 which)
{
    WinGlobals* g = gWinGlobals;
    u8* frame;

    lbl_80344F98 = 1;
    if (lbl_80344F9C == 0) {
        lbl_80344F98 = 0;
        return;
    }
    lbl_80343EFC->m18 = which;
    g->frame = g->screen->frames + which * 0x200;
    *lbl_80343F20 = (u32)(g->screen->frames + which * 0x200);
    frame = g->frame;
    *lbl_80343E8C = *(u32*)(frame + 0x1D4);
    *lbl_80343E90 = *(u32*)(frame + 0x1E4);
    *lbl_80343E94 = *(u32*)(frame + 0x1EC);
    *lbl_80343E98 = *(u32*)(frame + 0x1F4);
    *lbl_80343E9C = *(u32*)(frame + 0x1FC);
    fn_800C116C(0x100, lbl_8011656C);
    *lbl_80343E88 = 4;
    *lbl_80343E80 = 0;
    *lbl_80343E84 = (u32)frame;
    *lbl_80343E7C = 0x105;
    fn_800C116C(0x100, lbl_8011656C);
    lbl_80344F98 = 0;
}

/* Frame orchestration entry (referenced from mb_main.c -- keep fn_). */
void fn_800C1624(void)
{
    WinGlobals* g = gWinGlobals;
    MBScreen* s = g->screen;
    s32* q = &s->m14;
    s32 v;

    if (s->m10 == 0 && *q == 0) {
        *q = s->m18;
    }
    s = g->screen;
    q = &s->m14;
    v = *q;
    if (v != 0) {
        pbFrameMode(v, 1);
        g->screen->m14 = 0;
        if (lbl_80343EFC->m10 != 0) {
            fn_800C2C74();
        }
        fn_800AF1E0();
        fn_800C151C(1);
        fn_800C151C(0);
        fn_800AF1E8();
    } else if (s->m48 != 0) {
        pbFrameMode(s->m10, 0);
    }
    if (lbl_80343EFC->m00 != 0) {
        fn_800C2618();
    }
}

/* Display frame-mode state machine (0xEF8; the PB_FRAME namesake). */
void pbFrameMode(s32 mode, s32 flag)
{
    (void)mode;
    (void)flag;
    fn_800AF55C();
}

/* Queue a debug-grab request at (x, y). */
void fn_800C25F0(u32 x, u32 y)
{
    if (lbl_80344FC4 == 0) {
        return;
    }
    lbl_80344FAC = x;
    lbl_80344FB0 = y;
    gWinGlobals->screen->m48 = 1;
}

void fn_800C2618(void)
{
    (void)lbl_80343EFC;
}

void fn_800C2C74(void)
{
    (void)lbl_80343EFC;
}

/* Set the render size in pixels (referenced from mb_main.c -- keep fn_). */
void fn_800C2F50(f32 w, f32 h)
{
    WinGlobals* g = gWinGlobals;
    g->screen->m28 = (s32)w;
    g->screen->m2c = (s32)h;
}

/* Recompute the 16x fixed-point projection block from the screen size;
 * notifies the blitter when the scale changed. */
void fn_800C2F88(void)
{
    WinGlobals* g = gWinGlobals;
    f32 ox = g->proj->f00;
    f32 oy = g->proj->f04;

    g->proj->f00 = (f32)(16.0 * g->screen->width / g->screen->m28);
    g->proj->f04 = (f32)(16.0 * g->screen->height / g->screen->m2c);
    g->proj->m08 = 16.0 * (g->screen->f38 - (f32)(g->screen->width / 2));
    g->proj->m0c = 16.0 * (g->screen->f3c - (f32)(g->screen->height / 2));
    g->proj->m10 = 16.0 * g->screen->width;
    g->proj->m14 = 16.0 * g->screen->height;
    if (g->proj->f00 != ox || g->proj->f04 != oy) {
        MBBlitUpdateWindow(g->proj->f00 / ox, g->proj->f04 / oy);
        fn_800B69DC(g->proj->f00 / ox, g->proj->f04 / oy);
    }
    g->screen->m40 = 1;
}

/* return current screen height */
s32 MBScreenHeight(void)
{
    return gWinGlobals->screen->height;
}

/* return current screen width */
s32 MBScreenWidth(void)
{
    return gWinGlobals->screen->width;
}

/* Install the default screen block if unset + reset it
 * (referenced from pb_global.c -- keep fn_). */
void fn_800C31C4(void)
{
    u8* fb = lbl_802C4DE0;
    WinGlobals* gg = gWinGlobals;
    WinGlobals* g;

    if (gg->screen == 0) {
        gg->screen = (MBScreen*)(fb + 0x400);
    }
    g = gWinGlobals;
    g->screen->frames = fb;
    g->frame = g->screen->frames;
    *lbl_80343F20 = (u32)g->screen->frames;
    g->screen->f38 = 2048.0f;
    g->screen->f3c = 2048.0 + (lbl_80343F0C - lbl_80343F08) / 2;
    g->screen->m04 = 0;
    g->screen->m08 = fn_800AF55C() == 0;
    g->screen->m10 = 0;
    g->screen->m14 = 4;
    g->screen->m18 = 4;
    lbl_80343EFC->m28 = fb;
    lbl_80343EFC->m2c = fb;
}

/* Force the default screen block + reset it
 * (referenced from pb_global.c -- keep fn_). */
void fn_800C32D0(void)
{
    u8* fb = lbl_802C4DE0;
    WinGlobals* g;

    gWinGlobals->screen = (MBScreen*)(fb + 0x400);
    g = gWinGlobals;
    g->screen->frames = fb;
    g->frame = g->screen->frames;
    *lbl_80343F20 = (u32)g->screen->frames;
    g->screen->f38 = 2048.0f;
    g->screen->f3c = 2048.0 + (lbl_80343F0C - lbl_80343F08) / 2;
    g->screen->m04 = 0;
    g->screen->m08 = fn_800AF55C() == 0;
    g->screen->m10 = 0;
    g->screen->m14 = 4;
    g->screen->m18 = 4;
    lbl_80343EFC->m28 = fb;
    lbl_80343EFC->m2c = fb;
}

/* Install the default m60 hook if unset
 * (referenced from pb_global.c -- keep fn_). */
void fn_800C33D0(void)
{
    WinGlobals* g = gWinGlobals;
    if (g->m60) {
        return;
    }
    asm {}
    g->m60 = &lbl_80344FB8;
}

/* pb-module init stub (referenced from pb_global.c -- keep fn_) */
void fn_800C33EC(void)
{
    gWinGlobals->m60 = &lbl_80344FB8;
}
