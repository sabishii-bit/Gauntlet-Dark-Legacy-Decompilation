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
 * ordinary C statements. fn_800C1174 still has native code differences;
 * this TU remains NonMatching until those and its link obligations close.
 */

#include "types.h"

extern u32 gErrorCode;           /* 0x80343EF0 (.sdata) */
extern u32 lbl_80343F04;
extern s32 lbl_80343F08;
extern s32 lbl_80343EE8;
extern s32 lbl_80343EEC;
const char lbl_801164C0[] = "PB_ERROR.C:__LINE__";
extern s8 ramfont[2030]; /* RFONT.OBJ: 58 glyphs, seven rows of five cells */
u32 lbl_80344F94; /* four-byte PBGLOBAL_ERROR storage; only its address escapes */

typedef struct WinGlobals {
    u8 _pad[12];
    void* volatile error;
} WinGlobals;
extern WinGlobals* gWinGlobals;

extern int sceGsExecLoadImage();
/* GC callers retain the PS2 load-image API's seven signed-short arguments. */
extern int sceGsSetDefLoadImage(void* image, s16 dbp, s16 dbw, s16 dpsm,
                              s16 x, s16 y, s16 w, s16 h);
extern int sceGsSwapDBuff();
extern int sceGsSetDefDBuff();
extern int sceGsResetPath();
extern int sceGsResetGraph();
extern int FlushCache();
extern int sceGsSyncPath();
extern void fn_800C1148();              /* mb_window.c helper */
extern void fn_800C13CC(void);

/* Five PS2 GS register records, retained by the GC compatibility layer.
 * Xbox sceGsDispEnv/tGS_* declarations corroborate the names and 8-byte
 * records. GC's halfword FBP update at +0x10 and RGB stores at +0x20..0x22
 * verify the accessed fields under MWCC's big-endian bitfield allocation;
 * do not copy the Xbox PDB's little-endian bit offsets. p0/p1 are reserved
 * register bits, not artificial object padding. */
typedef struct GsPmode {
    u32 EN1 : 1;
    u32 EN2 : 1;
    u32 CRTMD : 3;
    u32 MMOD : 1;
    u32 AMOD : 1;
    u32 SLBG : 1;
    u32 ALP : 8;
    u32 p0 : 16;
    u32 p1;
} GsPmode;

typedef struct GsSmode2 {
    u32 INT : 1;
    u32 FFMD : 1;
    u32 DPMS : 2;
    u32 p0 : 28;
    u32 p1;
} GsSmode2;

typedef struct GsDispFb {
    u32 FBP : 9;
    u32 FBW : 6;
    u32 PSM : 5;
    u32 p0 : 12;
    u32 DBX : 11;
    u32 DBY : 11;
    u32 p1 : 10;
} GsDispFb;

typedef struct GsDisplay {
    u32 DX : 12;
    u32 DY : 11;
    u32 MAGH : 4;
    u32 MAGV : 2;
    u32 p0 : 3;
    u32 DW : 12;
    u32 DH : 11;
    u32 p1 : 9;
} GsDisplay;

typedef struct GsBgColor {
    u32 R : 8;
    u32 G : 8;
    u32 B : 8;
    u32 p0 : 8;
    u32 p1;
} GsBgColor;

typedef struct PBErrorDispEnv {
    GsPmode pmode;
    GsSmode2 smode2;
    GsDispFb dispfb;
    GsDisplay display;
    GsBgColor bgcolor;
} PBErrorDispEnv;

PBErrorDispEnv lbl_802C4DB8; /* sceGsDispEnv dispenv, 0x28 bytes at 0x802C4DB8 */

/* Big error reporter: rasterizes the message through a 256-wide 1-bit glyph
 * atlas into an 8 KiB stack bitmap, one 21-character line at a time. Each
 * glyph is 5 bytes wide x 7 rows in the atlas (35 bytes); every set cell
 * plots a two-word white pixel pair. The scratch block itself is addressed
 * through its symbol. */
void fn_800C1174(register s8* text)
{
    u8 image[80];
    u8 unused[8];             /* unrecovered local between image and pixels */
    u32 pixels[2048];
    PBErrorDispEnv* blk;
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

    blk = &lbl_802C4DB8;
    sceGsResetPath();
    sceGsResetGraph(0, 0, 2, 1);
    fn_800C13CC();

    sceGsSetDefDBuff(blk, 0, (s16)lbl_80343F04, (s16)(lbl_80343F08 / 2), 0, 0);
    ec = gErrorCode;
    blk->bgcolor.R = (ec >> 17) & 0x7F;
    blk->bgcolor.G = (ec >> 9) & 0x7F;
    blk->bgcolor.B = (ec >> 1) & 0x7F;
    blk->dispfb.FBP = 0;
    FlushCache(0);
    sceGsSwapDBuff(blk);
    sceGsResetPath();
    sceGsSwapDBuff(blk);
    fn_800C13CC();

    y = 50;
    while (*text != 0) {
        idx = 0;
        x = 0;
        for (clr = 0; clr < 2048; clr++) {
            pixels[clr] = 0;
        }
        while ((s8)(c = *text) != 0 && idx < 21) {
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
                glyph = (s8*)ramfont + (c8 - 33) * 35;
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
            text++;
        }
        sceGsSetDefLoadImage(image, 0, 10, 0, 50, y, 256, 8);
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
    u32 i;

    for (zero = 0; zero < 1024; zero++) {
        pixels[zero] = 0;
    }
    FlushCache(0);

    i = 0;
    do {
        sceGsSetDefLoadImage(image, i * 16, 4, 0, 0, 0, 32, 32);
        if (lbl_80343EE8 != 0) {
            FlushCache(0);
        }
        sceGsExecLoadImage(image, pixels);
        if (lbl_80343EEC != 0) {
            fn_800C1148(0, 0, lbl_801164C0);
        }
        i++;
    } while (i < 4096);
}

/* Wait for the asynchronous PB error state and acknowledge it. */
void fn_800C1498(void)
{
    /* PS2 pbErrorDie's local h.18; GC polls this four-byte state at 80344F90. */
    static volatile s32 lbl_80344F90;
    s32 v;

    while (lbl_80344F90 == 0) {
    }
    v = lbl_80344F90;
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
