/* mb_window.c -- Midway "MB" high-level window / 3D-projection TU.
 *
 * Xbox counterpart: mb_window.obj (shell3D.pdb: MBWindowSetAng / MBWindowInit
 * / MBWindowZoom / MBWindowProject / MBPointOnScreen / MBWindowTo3D /
 * MBInitWindows / MBWindowSetRegion). The GameCube build keeps a reduced,
 * reordered set plus the pb-module init/reset/close stubs; the function count
 * and static split differ from Xbox, so all internals keep fn_ names (several
 * are referenced by name from the Matching pb_global.c and must stay fn_).
 *
 * .text 0x800C0ADC-0x800C1174. Identified by the sdata2 pool seam
 * (0x80348EF8..0x80348F08 is this TU's; dbgtext ends at 0x80348EF4) and the
 * C++ vec4::operator= call (__as__4vec4FRC4vec4). Compiled -Cpp_exceptions on
 * (cflags_demo).
 *
 * NonMatching: structural skeletons only.
 */

#include "types.h"

/* current-window view (offsets from the disassembly) */
typedef struct MBWin {
    u8  _pad00[128];
    f32 f128; /* 0x80 */
    f32 f132; /* 0x84 */
    f32 f136; /* 0x88 */
    f32 f140; /* 0x8C */
} MBWin;

typedef struct WinGlobals {
    u8    _pad00[0x1C];
    MBWin* cur;    /* 0x1C : current window */
    u8    _pad20[0x08];
    void* m40;     /* 0x28 */
    u8    _pad2C[0x0C];
    void* m56;     /* 0x38 */
} WinGlobals;

extern WinGlobals* gWinGlobals;

extern u32 lbl_80343EE0;
extern u32 lbl_80343F04;
extern u32 lbl_80343F78;
extern u32 lbl_80343F7C;
extern u32 lbl_802C4750;
extern u32 lbl_802C4DA0;
extern u32 lbl_80344F88;

void __as__4vec4FRC4vec4(void* dst, const void* src); /* vec4::operator= */
void fn_800C0BD4(f32, f32);
void fn_800C0CF4(void);
void fn_800C0E0C(void);
s32  fn_800C1004(void);
void mbBlitStub51E0(void);
void* fn_800C5B3C(void);
void fn_800AF554(void);

/* MBWindowSetAng-family: sets the window orientation from two angles. */
void fn_800C0ADC(f32 a, f32 b, s32 mode)
{
    MBWin* w = gWinGlobals->cur;
    (void)a;
    (void)b;
    (void)mode;
    (void)w;
    fn_800C0BD4(a, b);
    __as__4vec4FRC4vec4(0, (const void*)&lbl_80343EE0);
}

void fn_800C0BD4(f32 a, f32 b)
{
    MBWin* w = gWinGlobals->cur;
    (void)a;
    (void)b;
    (void)w;
}

void fn_800C0CF4(void)
{
    MBWin* w = gWinGlobals->cur;
    (void)w;
}

/* set two floats on the current window */
void fn_800C0DDC(f32 x, f32 y)
{
    gWinGlobals->cur->f128 = x;
    gWinGlobals->cur->f132 = y;
}

void fn_800C0DF4(f32 x, f32 y)
{
    gWinGlobals->cur->f136 = x;
    gWinGlobals->cur->f140 = y;
}

void fn_800C0E0C(void)
{
    fn_800C0CF4();
}

/* pb-module close stub (referenced from pb_global.c -- keep fn_) */
void fn_800C0F04(void)
{
    fn_800C0E0C();
}

/* pb-module init stub (referenced from pb_global.c -- keep fn_) */
void fn_800C0F40(void)
{
    fn_800C0E0C();
}

/* pb-module close stub (referenced from pb_global.c -- keep fn_) */
void fn_800C0F70(void)
{
    if (gWinGlobals->m56) {
        return;
    }
    gWinGlobals->m56 = &lbl_802C4DA0;
}

/* pb-module init stub (referenced from pb_global.c -- keep fn_) */
void fn_800C0F90(void)
{
    gWinGlobals->m56 = &lbl_802C4DA0;
}

void fn_800C0FE4(void)
{
}

/* pb-module close stub (referenced from pb_global.c -- keep fn_) */
void fn_800C0FE8(void)
{
    if (gWinGlobals->m40) {
        return;
    }
    gWinGlobals->m40 = &lbl_80344F88;
}

s32 fn_800C1004(void)
{
    return (s32)gWinGlobals;
}

void fn_800C1120(void)
{
    mbBlitStub51E0();
    fn_800C5B3C();
    fn_800C1004();
}

void fn_800C1148(void)
{
    fn_800AF554();
}

void fn_800C116C(void)
{
}

void fn_800C1170(void)
{
}
