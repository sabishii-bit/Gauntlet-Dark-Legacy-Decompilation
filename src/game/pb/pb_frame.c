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
    s32 m14;           /* 0x14 : apply-decoded-registers flag */
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
extern void MBFontUpdateWindow(f32 sx, f32 sy);
extern void fn_800C116C(s32 code, char* file);
extern void fn_800C2618(void);
extern void fn_800C2C74(void);
extern void DIntr(void);
extern void EIntr(void);
extern u32 sceGsSyncV(void);

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
    s32* q;

    if (s->m10 == 0) {
        if (*(q = &s->m14) == 0) {
            *q = s->m18;
        }
    }
    s = g->screen;
    q = &s->m14;
    if (*q != 0) {
        pbFrameMode(*q, 1);
        g->screen->m14 = 0;
        if (lbl_80343EFC->m10 != 0) {
            fn_800C2C74();
        }
        DIntr();
        fn_800C151C(1);
        fn_800C151C(0);
        EIntr();
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
    sceGsSyncV();
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

typedef struct GsPmodeView {
    u8  _pad0[0x1D0];
    s64 pmode1;        /* 0x1D0 */
    u8  _pad1d8[0x1F8];
    s64 pmode2;        /* 0x3D0 */
} GsPmodeView;

/* GS display-register decode block, viewed at lbl_80343EFC+0x18. */
typedef struct PbFrameDecode {
    /* +0x18 */ s32 frameIdx;
    /* +0x1C */ u8  m1C;
    u8 _pad1d[3];
    /* +0x20 */ u32 m20;
    /* +0x24 */ u32 m24;
    /* +0x28 */ GsPmodeView* m28;
    /* +0x2C */ u8* regs;
    /* +0x30 */ u32 m30;
    /* +0x34 */ u32 m34;
    u32 m38, m3C, m40, m44, m48, m4C, m50, m54, m58, m5C, m60;
    /* +0x64 */ f32 f64;
    /* +0x68 */ f32 f68;
    /* +0x6C */ u32 m6C;
    /* +0x70 */ u32 m70;
    u32 m74, m78, m7C, m80, m84, m88, m8C, m90, m94, m98, m9C;
    /* +0xA0 */ f32 fA0;
    /* +0xA4 */ f32 fA4;
} PbFrameDecode;

/* GS register bitfield overlays (write shapes for fn_800C2618). */
typedef struct GsFldH9 { u16 hi : 9; u16 lo : 7; } GsFldH9;
typedef struct GsFldB6 { u8 top : 1; u8 mid : 6; u8 low : 1; } GsFldB6;
typedef struct GsFldW5 { u32 a : 15; u32 b : 5; u32 c : 12; } GsFldW5;
typedef struct GsFldH11 { u16 hi : 11; u16 lo : 5; } GsFldH11;
typedef struct GsFldW11a { u32 a : 11; u32 b : 11; u32 c : 10; } GsFldW11a;
typedef struct GsFldH12 { u16 hi : 12; u16 lo : 4; } GsFldH12;
typedef struct GsFldW11b { u32 a : 12; u32 b : 11; u32 c : 9; } GsFldW11b;
typedef struct GsFldH4 { u16 a : 7; u16 b : 4; u16 c : 5; } GsFldH4;
typedef struct GsFldB2 { u8 a : 3; u8 b : 2; u8 c : 3; } GsFldB2;
typedef struct GsFldB1a { u8 a : 1; u8 rest : 7; } GsFldB1a;
typedef struct GsFldB1b { u8 a : 1; u8 b : 1; u8 rest : 6; } GsFldB1b;

/* Re-pack the decode block back into the GS DISPLAY/DISPFB register shadows
 * (both field mirrors), refresh the scale ratios, and rebuild PMODE. */
void fn_800C2618(void)
{
    PbFrameDecode* s;
    u8 unused[8];

    if (lbl_80343EFC->m14 != 0) {
        s = (PbFrameDecode*)&lbl_80343EFC->m18;
        if (s->m28 != 0) {
            s->f64 = (f32)(s32)s->m5C / (f32)(s32)s->m54;
            s->f68 = (f32)(s32)s->m60 / (f32)(s32)s->m58;
            s->fA0 = (f32)(s32)s->m98 / (f32)(s32)s->m90;
            s->fA4 = (f32)(s32)s->m9C / (f32)(s32)s->m94;

            ((GsFldH9*)(s->regs + 0x1E0))->hi = (u16)s->m34;
            ((GsFldB6*)(s->regs + 0x1E1))->mid = (u8)s->m3C;
            ((GsFldW5*)(s->regs + 0x1E0))->b = s->m40;
            ((GsFldH11*)(s->regs + 0x1E4))->hi = (u16)s->m44;
            ((GsFldW11a*)(s->regs + 0x1E4))->b = s->m48;
            ((GsFldH12*)(s->regs + 0x1E8))->hi = (u16)s->m4C;
            ((GsFldW11b*)(s->regs + 0x1E8))->b = s->m50;
            ((GsFldH4*)(s->regs + 0x1EA))->b = (u16)s->m54;
            ((GsFldB2*)(s->regs + 0x1EB))->b = (u8)s->m58;
            ((GsFldH12*)(s->regs + 0x1EC))->hi = (u16)s->m5C;
            ((GsFldW11b*)(s->regs + 0x1EC))->b = s->m60;
            ((GsFldH9*)(s->regs + 0x1F0))->hi = (u16)s->m70;
            ((GsFldB6*)(s->regs + 0x1F1))->mid = (u8)s->m78;
            ((GsFldW5*)(s->regs + 0x1F0))->b = s->m7C;
            ((GsFldH11*)(s->regs + 0x1F4))->hi = (u16)s->m80;
            ((GsFldW11a*)(s->regs + 0x1F4))->b = s->m84;
            ((GsFldH12*)(s->regs + 0x1F8))->hi = (u16)s->m88;
            ((GsFldW11b*)(s->regs + 0x1F8))->b = s->m8C;
            ((GsFldH4*)(s->regs + 0x1FA))->b = (u16)s->m90;
            ((GsFldB2*)(s->regs + 0x1FB))->b = (u8)s->m94;
            ((GsFldH12*)(s->regs + 0x1FC))->hi = (u16)s->m98;
            ((GsFldW11b*)(s->regs + 0x1FC))->b = s->m9C;

            ((GsFldH9*)(s->regs + 0x3E0))->hi = (u16)s->m38;
            ((GsFldB6*)(s->regs + 0x3E1))->mid = (u8)s->m3C;
            ((GsFldW5*)(s->regs + 0x3E0))->b = s->m40;
            ((GsFldH11*)(s->regs + 0x3E4))->hi = (u16)s->m44;
            ((GsFldW11a*)(s->regs + 0x3E4))->b = s->m48;
            ((GsFldH12*)(s->regs + 0x3E8))->hi = (u16)s->m4C;
            ((GsFldW11b*)(s->regs + 0x3E8))->b = s->m50;
            ((GsFldH4*)(s->regs + 0x3EA))->b = (u16)s->m54;
            ((GsFldB2*)(s->regs + 0x3EB))->b = (u8)s->m58;
            ((GsFldH12*)(s->regs + 0x3EC))->hi = (u16)s->m5C;
            ((GsFldW11b*)(s->regs + 0x3EC))->b = s->m60;
            ((GsFldH9*)(s->regs + 0x3F0))->hi = (u16)s->m74;
            ((GsFldB6*)(s->regs + 0x3F1))->mid = (u8)s->m78;
            ((GsFldW5*)(s->regs + 0x3F0))->b = s->m7C;
            ((GsFldH11*)(s->regs + 0x3F4))->hi = (u16)s->m80;
            ((GsFldW11a*)(s->regs + 0x3F4))->b = s->m84;
            ((GsFldH12*)(s->regs + 0x3F8))->hi = (u16)s->m88;
            ((GsFldW11b*)(s->regs + 0x3F8))->b = s->m8C;
            ((GsFldH4*)(s->regs + 0x3FA))->b = (u16)s->m90;
            ((GsFldB2*)(s->regs + 0x3FB))->b = (u8)s->m94;
            ((GsFldH12*)(s->regs + 0x3FC))->hi = (u16)s->m98;
            ((GsFldW11b*)(s->regs + 0x3FC))->b = s->m9C;

            ((GsFldB1a*)(s->regs + 0x1C0))->a = (u8)s->m30;
            ((GsFldB1b*)(s->regs + 0x1C0))->b = (u8)s->m6C;
            *(u8*)(s->regs + 0x1C1) = s->m1C;
            *(f64*)(s->regs + 0x1C8) = *(f64*)(s->regs + 0x1C0);
            ((GsFldB1a*)(s->regs + 0x3C0))->a = (u8)s->m30;
            ((GsFldB1b*)(s->regs + 0x3C0))->b = (u8)s->m6C;
            *(u8*)(s->regs + 0x3C1) = s->m1C;
            *(f64*)(s->regs + 0x3C8) = *(f64*)(s->regs + 0x3C0);

            s->m28->pmode1 &= ~3;
            s->m28->pmode1 |= (s32)s->m20;
            s->m28->pmode1 |= (s32)(s->m24 << 1);
            s->m28->pmode2 &= ~3;
            s->m28->pmode2 |= (s32)s->m20;
            s->m28->pmode2 |= (s32)(s->m24 << 1);
        }
    }
}



/* Unpack the latched GS DISPLAY/DISPFB register shadows into the decode
 * block (widths, positions, magnifications) and derive the scale ratios. */
void fn_800C2C74(void)
{
    PbFrameDecode* s = (PbFrameDecode*)&lbl_80343EFC->m18;
    u8 unused[8];

    if (s->m28 != 0) {
        s->m34 = (*(u16*)(s->regs + 0x1E0) >> 7) & 0x1FF;
        s->m38 = (*(u16*)(s->regs + 0x3E0) >> 7) & 0x1FF;
        s->m3C = (*(u8*)(s->regs + 0x1E1) >> 1) & 0x3F;
        s->m40 = (*(u32*)(s->regs + 0x1E0) >> 12) & 0x1F;
        s->m44 = (*(u16*)(s->regs + 0x1E4) >> 5) & 0x7FF;
        s->m48 = (*(u32*)(s->regs + 0x1E4) >> 10) & 0x7FF;
        s->m4C = (*(u16*)(s->regs + 0x1E8) >> 4) & 0xFFF;
        s->m50 = (*(u32*)(s->regs + 0x1E8) >> 9) & 0x7FF;
        s->m54 = (*(u16*)(s->regs + 0x1EA) >> 5) & 0xF;
        s->m58 = (*(u8*)(s->regs + 0x1EB) >> 3) & 3;
        s->m5C = (*(u16*)(s->regs + 0x1EC) >> 4) & 0xFFF;
        s->m60 = (*(u32*)(s->regs + 0x1EC) >> 9) & 0x7FF;
        s->m70 = (*(u16*)(s->regs + 0x1F0) >> 7) & 0x1FF;
        s->m74 = (*(u16*)(s->regs + 0x3F0) >> 7) & 0x1FF;
        s->m78 = (*(u8*)(s->regs + 0x1F1) >> 1) & 0x3F;
        s->m7C = (*(u32*)(s->regs + 0x1F0) >> 12) & 0x1F;
        s->m80 = (*(u16*)(s->regs + 0x1F4) >> 5) & 0x7FF;
        s->m84 = (*(u32*)(s->regs + 0x1F4) >> 10) & 0x7FF;
        s->m88 = (*(u16*)(s->regs + 0x1F8) >> 4) & 0xFFF;
        s->m8C = (*(u32*)(s->regs + 0x1F8) >> 9) & 0x7FF;
        s->m90 = (*(u16*)(s->regs + 0x1FA) >> 5) & 0xF;
        s->m94 = (*(u8*)(s->regs + 0x1FB) >> 3) & 3;
        s->m98 = (*(u16*)(s->regs + 0x1FC) >> 4) & 0xFFF;
        s->m9C = (*(u32*)(s->regs + 0x1FC) >> 9) & 0x7FF;
        s->m30 = (*(u8*)(s->regs + 0x1C0) >> 7) & 1;
        s->m6C = (*(u8*)(s->regs + 0x1C0) >> 6) & 1;
        s->m1C = *(u8*)(s->regs + 0x1C1);
        s->m20 = (u32)(*(u64*)((u8*)s->m28 + 0x1D0) & 1);
        s->m24 = (u32)(*(u64*)((u8*)s->m28 + 0x1D0) & 2);
        s->f64 = (f32)(s32)s->m5C / (f32)(s32)s->m54;
        s->f68 = (f32)(s32)s->m60 / (f32)(s32)s->m58;
        s->fA0 = (f32)(s32)s->m98 / (f32)(s32)s->m90;
        s->fA4 = (f32)(s32)s->m9C / (f32)(s32)s->m94;
    }
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
        MBFontUpdateWindow(g->proj->f00 / ox, g->proj->f04 / oy);
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
    g->screen->m08 = sceGsSyncV() == 0;
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
    g->screen->m08 = sceGsSyncV() == 0;
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
