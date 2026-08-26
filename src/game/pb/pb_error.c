/* pb_error.c -- Midway "pb" ("bulletproof") library error/assert reporting TU.
 *
 * Xbox counterpart: PB_ERROR.OBJ (shell3D.pdb: pbInitError / pbResetError /
 * pbCloseError / pbErrorf / pbErrorDie / fb_all_clear / bulletproof_printf /
 * pbFatalErrorf). Confirmed by the assert literal "PB_ERROR.C:__LINE__"
 * (0x801164C0) referenced by fn_800C1174 and fn_800C13CC.
 *
 * The GameCube build keeps a reduced set (5 functions). The real
 * bulletproof_printf lives OUTSIDE this TU (0x800BC2EC, the caller-family), so
 * the exact PB_ERROR name of each surviving function here cannot be pinned by
 * size alone; all are left fn_. The two module init/close stubs (fn_800C14F0 /
 * fn_800C150C) are referenced by name from the Matching pb_global.c and must
 * stay fn_.
 *
 * .text 0x800C1174-0x800C151C. Compiled -Cpp_exceptions on (cflags_demo).
 * Reconstructed in plain C (2026-08): the allocator-resistant schedules that
 * used to be pinned by hand-written instruction blocks are now expressed as
 * ordinary C statements; the residual codegen difference is accepted.
 */

#include "types.h"

extern u32 gErrorCode;           /* 0x80343EF0 (.sdata) */
extern u8  lbl_802C4DB8[0x28];   /* error scratch block (.bss) */
extern u32 lbl_80343F04;
extern s32 lbl_80343F08;
extern s32 lbl_80343EE8;
extern s32 lbl_80343EEC;
extern s32 lbl_80344F90;
extern char lbl_801164C0[];      /* "PB_ERROR.C:__LINE__" */
extern s8 lbl_80120E98[];
extern u32 lbl_80344F94;

typedef struct WinGlobals {
    u8 _pad[12];
    void* volatile error;
} WinGlobals;
extern WinGlobals* gWinGlobals;

extern int sceGsExecLoadImage();
extern int sceGsSetDefLoadImage();
extern int sceGsSwapDBuff();
extern int sceGsSetDefDBuff();
extern int sceGsResetPath();
extern int sceGsResetGraph();
extern int FlushCache();
extern int sceGsSyncPath();
extern void fn_800C1148();              /* mb_window.c helper */
extern void fn_800C13CC(void);

typedef struct PBErrorBlock {
    u8 _pad0[16];
    u16 high : 9;
    u16 low : 7;
    u8 _pad12[14];
    u8 red;
    u8 green;
    u8 blue;
} PBErrorBlock;

/* Big error reporter: rasterizes the message through a 256-wide 1-bit glyph
 * atlas into an 8 KiB stack bitmap, one 21-character line at a time. Each
 * glyph is 5 bytes wide x 7 rows in the atlas (35 bytes); every set cell
 * plots a two-word white pixel pair. The second parameter is retained for the
 * original ABI; the scratch block itself is addressed through its symbol. */
void fn_800C1174(register s8* text, register u32 errorHigh)
{
    u8 image[80];
    u8 unused[8];
    u32 pixels[2048];
    PBErrorBlock* blk;
    u8* cursor;
    s8* glyph;
    u32 ec;
    s32 y;
    s32 clr;
    s32 idx;
    s32 x;
    s32 row;
    s32 col;
    s8 c8;
    u8 c;
    s32 plot;

    blk = (PBErrorBlock*)lbl_802C4DB8;
    cursor = (u8*)text;
    sceGsResetPath();
    sceGsResetGraph(0, 0, 2, 1);
    fn_800C13CC();

    sceGsSetDefDBuff(blk, 0, (s16)lbl_80343F04, (s16)(lbl_80343F08 / 2), 0, 0);
    ec = gErrorCode;
    blk->red = (ec >> 17) & 0x7F;
    blk->green = (ec >> 9) & 0x7F;
    blk->blue = (ec >> 1) & 0x7F;
    blk->high = 0;
    FlushCache(0, 0);
    sceGsSwapDBuff(blk);
    sceGsResetPath();
    sceGsSwapDBuff(blk);
    fn_800C13CC();

    y = 50;
    while ((s8)*cursor != 0) {
        for (clr = 0; clr < 2048; clr++) {
            pixels[clr] = 0;
        }
        idx = 0;
        x = 0;
        while ((s8)(c = *cursor) != 0 && idx < 21) {
            if ((s8)c >= 97 && (s8)c <= 122) {
                c -= 32;
            }
            if ((s8)c == 92) {
                c = 37;
            }
            if ((s8)c == 91) {
                c = 40;
            }
            if ((s8)c == 93) {
                c = 41;
            }
            if ((s8)c < 33 || (s8)c > 90) {
                c = 46;
            }
            c8 = (s8)c;
            if (c8 != ' ') {
                glyph = (s8*)lbl_80120E98 + (c8 - 33) * 35;
                for (row = 0; row < 7; row++) {
                    for (col = 0; col < 5; col++) {
                        if (*glyph != 0) {
                            plot = x + col * 2 + row * 256;
                            pixels[plot] = 0x00FFFFFF;
                            plot++;
                            pixels[plot] = 0x00FFFFFF;
                        }
                        glyph++;
                    }
                }
            }
            idx++;
            x += 12;
            cursor++;
        }
        sceGsSetDefLoadImage(image, 0, 10, 0, 50, (s16)y, 256, 8);
        FlushCache(0);
        sceGsExecLoadImage(image, pixels);
        fn_800C1148(0, 0, lbl_801164C0);
        y += 12;
    }
}

/* Draw the diagnostic texture repeatedly while the error display is active. */
void fn_800C13CC(void)
{
    u32 pixels[1024];
    u32 zero;
    u8 image[164];
    s32 off;
    char* file;
    u32 i;

    for (zero = 0; zero < 1024; zero++) {
        pixels[zero] = 0;
    }
    FlushCache(0);

    i = 0;
    off = 0;
    file = lbl_801164C0;
    do {
        sceGsSetDefLoadImage(image, (s16)off, 4, 0, 0, 0, 32, 32);
        if (lbl_80343EE8 != 0) {
            FlushCache(0);
        }
        sceGsExecLoadImage(image, pixels);
        if (lbl_80343EEC != 0) {
            fn_800C1148(0, 0, file);
        }
        i++;
        off += 16;
    } while (i < 4096);
}

/* Wait for the asynchronous PB error state and acknowledge it. */
void fn_800C1498(void)
{
    s32 v;

    while (*(volatile s32*)&lbl_80344F90 == 0) {
    }
    v = *(volatile s32*)&lbl_80344F90;
    switch (v) {
    case 2:
        break;
    case 3:
        sceGsSyncPath(0);
        break;
    case 0:
    case 1:
    default:
        lbl_80344F90 = 0;
        break;
    }
}

/* pb-module close stub (referenced from pb_global.c -- keep fn_) */
void fn_800C14F0(void)
{
    WinGlobals* w = gWinGlobals;
    if (w->error) {
        return;
    }
    w->error = &lbl_80344F94;
}

/* pb-module reset stub (referenced from pb_global.c -- keep fn_) */
void fn_800C150C(void)
{
    gWinGlobals->error = &lbl_80344F94;
}
