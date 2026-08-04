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
extern volatile u32 lbl_80344F98;
extern volatile u32 lbl_80344F9C;
extern u32 lbl_80344FAC, lbl_80344FB0, lbl_80344FB8;
extern s32 lbl_80344FC4;
extern u8 lbl_802C51E0[];    /* default screen block */
extern u8 lbl_802C4DE0[];    /* default frame buffers (0x450) */
extern char lbl_8011656C[]; /* "PB_FRAME.C:__LINE__" */

extern void pbFrameMode(s32 mode, s32 flag);

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

/* pbFrameMode support */
typedef struct P8 { u32 a; u32 b; } P8;
typedef struct GsFldB5b1 { u8 a : 5; u8 b : 1; u8 c : 2; } GsFldB5b1;
extern s32 lbl_80343F00;
extern f64 lbl_80348F18;
extern u32 lbl_80343EF8;
extern s32 lbl_80343F04;
extern u32 lbl_80128088[];
extern s32 lbl_80344FA0;
extern s32 lbl_80344FA4;
extern s32 lbl_80344FA8;
extern u8 lbl_80344FB4;
extern void fn_800C2F88(void);
extern void sceGsResetGraph(s32 a, s16 b, s16 c, s16 d);
extern void sceGsResetPath(void);
extern void sceMtapPortClose(void* env, s16 cnt, s16 w, s16 h, s32 psm,
                             s16 chan, s32 e);
extern void FlushCache(s32 mode);

extern void MBBlitUpdateWindow(f32 sx, f32 sy);
extern void MBFontUpdateWindow(f32 sx, f32 sy);
extern void fn_800C116C(s32 code, char* file);
extern void fn_800C2618(void);
extern void fn_800C2C74(void);
extern void DIntr(void);
extern void EIntr(void);
extern u32 sceGsSyncV(s32 mode);

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

/* Display frame-mode state machine (0xEF8; the PB_FRAME namesake).
 * Modes 3-6 reprogram the GS display (width/height/interlace) and rebuild
 * both 512-byte GIF A+D register packets from the sceGs display-env
 * templates on the stack; mode 1 just latches the pending mode. */
void pbFrameMode(s32 mode, s32 flag)
{
    WinGlobals* g;
    s32 sync0;
    MBScreen* scr;
    u8 dispenv[96];                       /* 88..183: two 40B + slack */
    u8 tplA2[88];                         /* 184..271 */
    u8 tplB1[88];                         /* 272..359 */
    u8 tplC1[80];                         /* 360..439 */
    u8 tplA1[88];                         /* 440..527 */
    u8 tplB2[88];                         /* 528..615 */
    u8 tplC2[64];                         /* 616..679 */
    u32 gifTag2;                          /* 684 */
    u32 colorMask;                        /* 688 */
    u32 kTest1;                           /* 692.. spare consts */
    u8 _spare[24];
    u32 kAlpha1;                          /* 736 */
    u32 kBig;                             /* 740 */
    u32 kTexFlush;                        /* 744 */
    u32 kNop;                             /* 748 */
    u8* tplA2p;                           /* 752 */
    s32 loopW;                            /* r27 */
    s32 loopH;                            /* r31 */
    s32 loopCnt;                          /* r28 */
    s32 loopChan;                         /* r29 */
    s32 fieldA;                           /* r25 */
    s32 fieldB;                           /* r26 */
    s32 blocks;                           /* r21 */
    s32 halfBlocks;                       /* r4-ish */
    s32 dblBlocks;                        /* r20 */
    s32 k;                                /* r23 */
    s32 envOff;                           /* r29 reuse */
    s32 bufOff;                           /* r28 reuse */
    u8* buf;
    u8* env;
    u8* pA1;
    u8* pB1;
    u8* pB2;
    u8* pC1;
    u8* pC2;
    u8* tA;
    u8* tB;
    u8* tC;
    s32 smode;
    s32 aR;
    s32 aG;
    s32 noBlend;
    s32 selA;
    s32 selB;
    s32 selC;

    g = gWinGlobals;
    lbl_80344F9C = 0;
    sync0 = lbl_80343F00;
    while (lbl_80344F98 != 0) {
    }
    if ((u32)mode > 6) {
        return;
    }
    switch (mode) {
    case 3:
        g->screen->width = (s32)(f32)lbl_80343F04;
        g->screen->height = (s32)(f32)(lbl_80343F0C / 2);
        *(s32*)((u8*)g->screen + 48) = 1;
        *(s32*)((u8*)g->screen + 52) = 0x1000000 - 1;
        if (g->screen->m28 == 0 || g->screen->m2c == 0) {
            gWinGlobals->screen->m28 = 512;
            gWinGlobals->screen->m2c = 384;
        }
        fn_800C2F88();
        loopW = lbl_80343F04;
        loopH = lbl_80343F08 / 2;
        g->screen->m00 = 2;
        loopCnt = 10;
        loopChan = 49;
        fieldA = 0;
        fieldB = 1;
        break;
    case 4:
        g->screen->width = (s32)(f32)lbl_80343F04;
        g->screen->height = (s32)(f32)lbl_80343F0C;
        *(s32*)((u8*)g->screen + 48) = 1;
        *(s32*)((u8*)g->screen + 52) = 0x1000000 - 1;
        if (g->screen->m28 == 0 || g->screen->m2c == 0) {
            gWinGlobals->screen->m28 = 512;
            gWinGlobals->screen->m2c = 384;
        }
        fn_800C2F88();
        loopW = lbl_80343F04;
        loopH = lbl_80343F08;
        g->screen->m00 = 1;
        loopCnt = 10;
        loopChan = 49;
        fieldA = 1;
        fieldB = 0;
        break;
    case 5:
        g->screen->width = (s32)(f32)lbl_80343F04;
        g->screen->height = (s32)(f32)(lbl_80343F0C / 2);
        *(s32*)((u8*)g->screen + 48) = 1;
        *(s32*)((u8*)g->screen + 52) = 0x10000 - 1;
        if (g->screen->m28 == 0 || g->screen->m2c == 0) {
            gWinGlobals->screen->m28 = 512;
            gWinGlobals->screen->m2c = 384;
        }
        fn_800C2F88();
        loopW = lbl_80343F04;
        loopH = lbl_80343F08 / 2;
        g->screen->m00 = 2;
        loopCnt = 2;
        loopChan = 50;
        fieldA = 0;
        fieldB = 1;
        break;
    case 6:
        g->screen->width = (s32)(f32)lbl_80343F04;
        g->screen->height = (s32)(f32)lbl_80343F0C;
        *(s32*)((u8*)g->screen + 48) = 1;
        *(s32*)((u8*)g->screen + 52) = 0x10000 - 1;
        if (g->screen->m28 == 0 || g->screen->m2c == 0) {
            gWinGlobals->screen->m28 = 512;
            gWinGlobals->screen->m2c = 384;
        }
        fn_800C2F88();
        loopW = lbl_80343F04;
        loopH = lbl_80343F08;
        g->screen->m00 = 1;
        loopCnt = 2;
        loopChan = 50;
        fieldA = 1;
        fieldB = 0;
        break;
    case 1:
        g->screen->m10 = mode;
        lbl_80343EF8 = lbl_80128088[mode];
        return;
    case 0:
    case 2:
        return;
    }

    blocks = ((loopW + 63) >> 6) * loopH + 31 >> 5;
    halfBlocks = blocks;
    lbl_80343EF8 = lbl_80128088[mode];
    if (loopCnt == 2 || loopCnt == 10) {
        blocks = (blocks + 1) >> 1;
    }
    if (loopChan == 50 || loopChan == 58) {
        halfBlocks = (halfBlocks + 1) >> 1;
    }
    dblBlocks = blocks * 2;
    *(s32*)((u8*)g->screen + 68) = halfBlocks + dblBlocks;
    g->screen->m08 = !sceGsSyncV(0);
    if (flag != 0) {
        sceGsResetGraph(0, (s16)fieldA, (s16)sync0, (s16)fieldB);
        sceGsResetPath();
    }
    sceMtapPortClose(dispenv, (s16)loopCnt, (s16)loopW, (s16)loopH, 3,
                     (s16)loopChan, 1);

    tplA2p = tplA2;
    pA1 = tplA1;
    pB1 = tplB1;
    pB2 = tplB2;
    pC1 = tplC1;
    pC2 = tplC2;
    gifTag2 = 0x60712435;
    colorMask = 0x1000000 - 1;
    kAlpha1 = 0x10000 - 32743;
    kNop = 0x7000001A;
    kBig = 0x10000000;
    envOff = 0;
    bufOff = 0;
    for (k = 0; k < 2; k++) {
        buf = g->screen->frames + bufOff;
        env = dispenv + envOff;
        tA = (k != 0) ? tplA2p : pA1;
        tB = (k != 0) ? pB1 : pB2;
        tC = (k != 0) ? pC1 : pC2;
        *(P8*)(buf + 448) = *(P8*)env;
        *(P8*)(buf + 456) = *(P8*)env;
        *(P8*)(buf + 464) = *(P8*)(env + 8);
        *(P8*)(buf + 472) = *(P8*)(env + 32);
        *(P8*)(buf + 496) = *(P8*)(env + 16);
        *(P8*)(buf + 504) = *(P8*)(env + 24);
        *(P8*)(buf + 480) = *(P8*)(env + 16);
        *(P8*)(buf + 488) = *(P8*)(env + 24);

        smode = lbl_80344FA0;
        if (smode == 0) {
            s32 xoff = lbl_80344FB0;
            s32 one = 1;
            s32 zero = 0;

            ((GsFldB1a*)(buf + 448))->a = one;
            *(u8*)(buf + 449) = 128;
            ((GsFldB1b*)(buf + 448))->b = one;
            *(f64*)(buf + 456) = *(f64*)(buf + 448);
            ((GsFldH11*)(buf + 484))->hi = one;
            ((GsFldW11a*)(buf + 484))->b = zero;
            ((GsFldH4*)(buf + 490))->b = *(u16*)(env + 26);
            ((GsFldB2*)(buf + 491))->b = *(u8*)(env + 27);
            ((GsFldH12*)(buf + 488))->hi =
                ((u32)*(u16*)(env + 24) >> 4) + (lbl_80344FA4 + lbl_80344FAC);
            ((GsFldW11b*)(buf + 488))->b =
                (*(u32*)(env + 24) >> 9 & 0x7FF) + (lbl_80344FA8 + xoff);
            ((GsFldH12*)(buf + 492))->hi =
                ((u32)*(u16*)(env + 28) >> 4) - 4 - lbl_80344FA4 * 2;
            ((GsFldW11b*)(buf + 492))->b =
                (*(u32*)(env + 28) >> 9 & 0x7FF) - lbl_80344FA8 * 2;
            ((GsFldH11*)(buf + 500))->hi = one;
            ((GsFldW11a*)(buf + 500))->b = one;
            ((GsFldH4*)(buf + 506))->b = *(u16*)(env + 26);
            ((GsFldB2*)(buf + 507))->b = *(u8*)(env + 27);
            ((GsFldH12*)(buf + 504))->hi =
                ((u32)*(u16*)(env + 24) >> 4) + ((u32)*(u16*)(env + 26) >> 5 & 0xF) +
                lbl_80344FA4 + lbl_80344FAC + 1;
            ((GsFldW11b*)(buf + 504))->b =
                (*(u32*)(env + 24) >> 9 & 0x7FF) + (lbl_80344FA8 + xoff);
            ((GsFldH12*)(buf + 508))->hi =
                ((u32)*(u16*)(env + 28) >> 4) - 4 - lbl_80344FA4 * 2;
            ((GsFldW11b*)(buf + 508))->b =
                (*(u32*)(env + 28) >> 9 & 0x7FF) - 1 - lbl_80344FA8 * 2;
        } else {
            s32 one = 1;

            aR = 128;
            aG = 128;
            noBlend = 1;
            selA = 0;
            selB = 0;
            selC = 0;
            if (smode <= 5) {
                aR = (smode - 1) << 6;
                aG = 256 - aR;
                noBlend = 0;
                if (aR > 255) {
                    aR = 255;
                }
                if (aG > 255) {
                    aG = 255;
                }
            } else if (smode == 6) {
                noBlend = 0;
                selA = 1;
            } else if (smode == 7) {
                noBlend = 0;
                selA = 0;
                selB = 0;
                selC = 1;
            } else if (smode == 10) {
                aR = 64;
                aG = 192;
            } else if (smode == 11) {
                aR = 192;
                aG = 64;
            }
            if (smode == 8) {
                ((GsFldB1a*)(buf + 448))->a = one;
                ((GsFldB1b*)(buf + 448))->b = one;
                ((GsFldB5b1*)(buf + 448))->b = one;
                *(u8*)(buf + 449) = 128;
                *(f64*)(buf + 456) = *(f64*)(buf + 448);
                ((GsFldW11b*)(buf + 508))->b =
                    (*(u32*)(env + 28) >> 9 & 0x7FF) - 1;
            } else if (smode == 9) {
                ((GsFldH11*)(buf + 500))->hi = 0;
                ((GsFldW11a*)(buf + 500))->b = 1;
                ((GsFldW11b*)(buf + 508))->b =
                    (*(u32*)(env + 28) >> 9 & 0x7FF) - 1;
            } else {
            ((GsFldB1a*)(buf + 448))->a = one;
            *(u8*)(buf + 449) = (u8)aR;
            ((GsFldB1b*)(buf + 448))->b = one;
            ((GsFldB1a*)(buf + 456))->a = one;
            ((GsFldB1b*)(buf + 456))->b = one;
            *(u8*)(buf + 457) = (u8)aG;
            ((GsFldH11*)(buf + 484))->hi = 0;
            ((GsFldW11a*)(buf + 484))->b = 0;
            ((GsFldH4*)(buf + 490))->b = *(u16*)(env + 26);
            ((GsFldB2*)(buf + 491))->b = *(u8*)(env + 27);
            ((GsFldH12*)(buf + 488))->hi =
                ((u32)*(u16*)(env + 24) >> 4) +
                noBlend * (((u32)*(u16*)(env + 26) >> 5 & 0xF) + 1);
            ((GsFldW11b*)(buf + 488))->b =
                (*(u32*)(env + 24) >> 9 & 0x7FF) +
                selB * (((u32)*(u8*)(env + 27) >> 3 & 3) + 1);
            ((GsFldH12*)(buf + 492))->hi = (u32)*(u16*)(env + 28) >> 4;
            ((GsFldW11b*)(buf + 492))->b = *(u32*)(env + 28) >> 9 & 0x7FF;
            ((GsFldH11*)(buf + 500))->hi = 0;
            ((GsFldW11a*)(buf + 500))->b = one;
            ((GsFldH4*)(buf + 506))->b = *(u16*)(env + 26);
            ((GsFldB2*)(buf + 507))->b = *(u8*)(env + 27);
            ((GsFldH12*)(buf + 504))->hi =
                ((u32)*(u16*)(env + 24) >> 4) +
                selA * (((u32)*(u16*)(env + 26) >> 5 & 0xF) + 1);
            ((GsFldW11b*)(buf + 504))->b =
                (*(u32*)(env + 24) >> 9 & 0x7FF) +
                selC * (((u32)*(u8*)(env + 27) >> 3 & 3) + 1);
            ((GsFldH12*)(buf + 508))->hi = (u32)*(u16*)(env + 28) >> 4;
            ((GsFldW11b*)(buf + 508))->b =
                (*(u32*)(env + 28) >> 9 & 0x7FF) - 1;
            }
        }

        *(u32*)(buf + 32) = *(u32*)tA;
        *(u32*)(buf + 36) = *(u32*)(tA + 4);
        *(u32*)(buf + 48) = *(u32*)(tA + 12);
        *(u32*)(buf + 52) = *(u32*)(tA + 16);
        *(u32*)(buf + 64) = *(u32*)(tA + 24);
        *(u32*)(buf + 68) = *(u32*)(tA + 28);
        *(u32*)(buf + 80) = *(u32*)(tA + 36);
        *(u32*)(buf + 84) = *(u32*)(tA + 40);
        *(u32*)(buf + 400) = *(u32*)(tA + 76);
        *(u32*)(buf + 404) = *(u32*)(tA + 80);
        *(u32*)(buf + 112) = *(u32*)tB;
        *(u32*)(buf + 116) = *(u32*)(tB + 4);
        *(u32*)(buf + 128) = *(u32*)(tB + 12);
        *(u32*)(buf + 132) = *(u32*)(tB + 16);
        *(u32*)(buf + 144) = *(u32*)(tB + 24);
        *(u32*)(buf + 148) = *(u32*)(tB + 28);
        *(u32*)(buf + 160) = *(u32*)(tB + 36);
        *(u32*)(buf + 164) = *(u32*)(tB + 40);
        *(u32*)(buf + 416) = *(u32*)(tB + 76);
        *(u32*)(buf + 420) = *(u32*)(tB + 80);
        *(u32*)(buf + 192) = *(u32*)(tA + 48);
        *(u32*)(buf + 196) = *(u32*)(tA + 52);
        *(u32*)(buf + 208) = *(u32*)(tA + 56);
        *(u32*)(buf + 212) = *(u32*)(tA + 60);
        lbl_80344FB4 = (*(u64*)(buf + 208) & 1) == 0;
        *(u32*)(buf + 224) = *(u32*)(tA + 68);
        *(u32*)(buf + 228) = *(u32*)(tA + 72);
        *(u32*)(buf + 100) = 0;
        *(u32*)(buf + 96) = 0;
        *(u32*)(buf + 180) = 0;
        *(u32*)(buf + 176) = 0;
        *(u32*)(buf + 240) = 0x71603524;
        *(u32*)(buf + 244) = gifTag2;
        *(u32*)(buf + 260) = 0;
        *(u32*)(buf + 256) = 0;
        *(u32*)(buf + 276) = colorMask;
        *(u32*)(buf + 272) = 0;
        *(u32*)(buf + 292) = 0;
        *(u32*)(buf + 288) = 0;
        *(u32*)(buf + 304) = 0;
        *(u32*)(buf + 308) = 128;
        *(u32*)(buf + 320) = *(u32*)tC;
        *(u32*)(buf + 324) = *(u32*)(tC + 4);
        *(u32*)(buf + 336) = *(u32*)(tC + 8);
        *(u32*)(buf + 340) = *(u32*)(tC + 12);
        *(u32*)(buf + 352) = *(u32*)(tC + 16);
        *(u32*)(buf + 356) = *(u32*)(tC + 20);
        *(u32*)(buf + 368) = *(u32*)(tC + 28);
        *(u32*)(buf + 372) = *(u32*)(tC + 32);
        *(u32*)(buf + 384) = *(u32*)(tC + 40);
        *(u32*)(buf + 388) = *(u32*)(tC + 44);
        *(u32*)(buf + 44) = 76;
        *(u32*)(buf + 40) = 0;
        *(u32*)(buf + 60) = 78;
        *(u32*)(buf + 56) = 0;
        *(u32*)(buf + 76) = 24;
        *(u32*)(buf + 72) = 0;
        *(u32*)(buf + 92) = 64;
        *(u32*)(buf + 88) = 0;
        *(u32*)(buf + 108) = 74;
        *(u32*)(buf + 104) = 0;
        *(u32*)(buf + 412) = 71;
        *(u32*)(buf + 408) = 0;
        *(u32*)(buf + 124) = 77;
        *(u32*)(buf + 120) = 0;
        *(u32*)(buf + 140) = 79;
        *(u32*)(buf + 136) = 0;
        *(u32*)(buf + 156) = 25;
        *(u32*)(buf + 152) = 0;
        *(u32*)(buf + 172) = 65;
        *(u32*)(buf + 168) = 0;
        *(u32*)(buf + 188) = 75;
        *(u32*)(buf + 184) = 0;
        *(u32*)(buf + 428) = 72;
        *(u32*)(buf + 424) = 0;
        *(u32*)(buf + 204) = 26;
        *(u32*)(buf + 200) = 0;
        *(u32*)(buf + 220) = 70;
        *(u32*)(buf + 216) = 0;
        *(u32*)(buf + 236) = 69;
        *(u32*)(buf + 232) = 0;
        *(u32*)(buf + 252) = 68;
        *(u32*)(buf + 248) = 0;
        *(u32*)(buf + 268) = 34;
        *(u32*)(buf + 264) = 0;
        *(u32*)(buf + 284) = 61;
        *(u32*)(buf + 280) = 0;
        *(u32*)(buf + 300) = 73;
        *(u32*)(buf + 296) = 0;
        *(u32*)(buf + 316) = 59;
        *(u32*)(buf + 312) = 0;
        *(u32*)(buf + 332) = 71;
        *(u32*)(buf + 328) = 0;
        *(u32*)(buf + 348) = 0;
        *(u32*)(buf + 344) = 0;
        *(u32*)(buf + 364) = 1;
        *(u32*)(buf + 360) = 0;
        *(u32*)(buf + 380) = 5;
        *(u32*)(buf + 376) = 0;
        *(u32*)(buf + 396) = 5;
        *(u32*)(buf + 392) = 0;
        *(u32*)(buf + 12) = 0;
        *(u32*)(buf + 8) = 0;
        *(u32*)(buf + 16) = 0x8019;
        *(u32*)(buf + 20) = kBig;
        *(u32*)(buf + 28) = 14;
        *(u32*)(buf + 24) = 0;
        *(u32*)(buf + 0) = kNop;
        *(u32*)(buf + 4) = 0;
        envOff += 40;
        bufOff += 512;
    }

    scr = g->screen;
    {
        u8* pkt = scr->frames;

        ((GsFldH9*)(pkt + 480))->hi = (u16)blocks;
        ((GsFldH9*)(pkt + 496))->hi = (u16)blocks;
        ((GsFldH9*)(pkt + 32))->hi = 0;
        ((GsFldH9*)(pkt + 112))->hi = 0;
        ((GsFldH9*)(pkt + 48))->hi = (u16)dblBlocks;
        ((GsFldH9*)(pkt + 128))->hi = (u16)dblBlocks;
        pkt = g->screen->frames + 512;
        ((GsFldH9*)(pkt + 480))->hi = 0;
        ((GsFldH9*)(pkt + 496))->hi = 0;
        ((GsFldH9*)(pkt + 32))->hi = (u16)blocks;
        ((GsFldH9*)(pkt + 112))->hi = (u16)blocks;
        ((GsFldH9*)(pkt + 48))->hi = (u16)dblBlocks;
        ((GsFldH9*)(pkt + 128))->hi = (u16)dblBlocks;
    }
    g->screen->m10 = mode;
    FlushCache(0);
    lbl_80344F9C = 1;
    g->screen->m48 = 0;
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
    s32 zero = 0;

    if (gg->screen == 0) {
        gg->screen = (MBScreen*)(fb + 0x400);
    }
    g = gWinGlobals;
    g->screen->frames = fb;
    g->frame = g->screen->frames;
    *lbl_80343F20 = (u32)g->screen->frames;
    g->screen->f38 = 2048.0f;
    g->screen->f3c = 2048.0 + (lbl_80343F0C - lbl_80343F08) / 2;
    g->screen->m04 = zero;
    g->screen->m08 = !sceGsSyncV(0);
    g->screen->m10 = zero;
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
    s32 zero = 0;

    gWinGlobals->screen = (MBScreen*)(fb + 0x400);
    g = gWinGlobals;
    g->screen->frames = fb;
    g->frame = g->screen->frames;
    *lbl_80343F20 = (u32)g->screen->frames;
    g->screen->f38 = 2048.0f;
    g->screen->f3c = 2048.0 + (lbl_80343F0C - lbl_80343F08) / 2;
    g->screen->m04 = zero;
    g->screen->m08 = !sceGsSyncV(0);
    g->screen->m10 = zero;
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
