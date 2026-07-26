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
    u8  _pad00[32];
    s32 width;   /* 0x20 */
    s32 height;  /* 0x24 */
} MBScreen;

typedef struct WinGlobals {
    u8       _pad00[16];
    MBScreen* screen;  /* 0x10 */
    u8       _pad14[0x28];
    void*    m60;      /* 0x3C */
} WinGlobals;

extern WinGlobals* gWinGlobals;

extern u32 lbl_80343EFC;
extern u32 lbl_80343E7C, lbl_80343E80, lbl_80343E84, lbl_80343E88, lbl_80343E8C;
extern u32 lbl_80343E90, lbl_80343E94, lbl_80343E98, lbl_80343E9C;
extern u32 lbl_80344FAC, lbl_80344FB0, lbl_80344FB8, lbl_80344FC4;
extern char lbl_8011656C[]; /* "PB_FRAME.C:__LINE__" */

extern void pbFrameMode(s32 mode);
extern void MBBlitUpdateWindow(void);
extern void fn_800B69DC(void);
extern void fn_800C116C(void);
extern void fn_800C2618(void);
extern void fn_800C2C74(void);
extern void fn_800AF1E0(void);
extern void fn_800AF1E8(void);
extern void fn_800AF55C(void);

/* Set up per-frame state (references PB_FRAME.C:__LINE__). */
void fn_800C151C(void)
{
    fn_800C116C();
    (void)lbl_8011656C;
    (void)lbl_80343E7C;
}

/* Frame orchestration entry (referenced from mb_main.c -- keep fn_). */
void fn_800C1624(void)
{
    fn_800C2C74();
    fn_800C151C();
    fn_800AF1E0();
    pbFrameMode(0);
    fn_800AF1E8();
    fn_800C2618();
}

/* Display frame-mode state machine (0xEF8; the PB_FRAME namesake). */
void pbFrameMode(s32 mode)
{
    (void)mode;
    fn_800AF55C();
}

void fn_800C25F0(void)
{
    (void)lbl_80344FC4;
    (void)lbl_80344FAC;
    (void)lbl_80344FB0;
}

void fn_800C2618(void)
{
    (void)lbl_80343EFC;
}

void fn_800C2C74(void)
{
    (void)lbl_80343EFC;
}

/* referenced from mb_main.c -- keep fn_ */
void fn_800C2F50(f32 w, f32 h)
{
    (void)w;
    (void)h;
    (void)gWinGlobals;
}

void fn_800C2F88(void)
{
    MBBlitUpdateWindow();
    fn_800B69DC();
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

/* pb-module close stub (referenced from pb_global.c -- keep fn_) */
void fn_800C31C4(void)
{
    fn_800AF55C();
}

/* pb-module init stub (referenced from pb_global.c -- keep fn_) */
void fn_800C32D0(void)
{
    fn_800AF55C();
}

/* pb-module close stub (referenced from pb_global.c -- keep fn_)
 * (residual: MWCC emits lwzu base-advance vs target lwz+stw at fixed
 * offset 60 -- scheduler/addressing tie, parked) */
void fn_800C33D0(void)
{
    if (gWinGlobals->m60) {
        return;
    }
    gWinGlobals->m60 = &lbl_80344FB8;
}

/* pb-module init stub (referenced from pb_global.c -- keep fn_) */
void fn_800C33EC(void)
{
    gWinGlobals->m60 = &lbl_80344FB8;
}
