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
 * Matching: the two large raster loops retain recovered C semantics in the
 * comments but pin allocator-resistant schedules inside normal C functions,
 * preserving the compiler's original extab/extabindex entries.
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

extern int fn_800AF544();
extern int fn_800AF54C();
extern int fn_800AF564();
extern int fn_800AF568();
extern int fn_800AF56C();
extern int fn_800AF570();
extern int fn_800AF1D8();
extern int fn_800AF1DC();
extern void fn_800C1148();              /* mb_window.c helper */
extern void fn_800C13CC(void);

/* Draw the diagnostic texture repeatedly while the error display is active. */
void fn_800C13CC(void)
{
    u32 pixels[1024];
    u32 zero;
    u8 image[164];
    int x;
    u32 i;

    for (zero = 0; zero < 1024; zero++) {
        pixels[zero] = 0;
    }
    fn_800AF1D8(0);

    x = 0;
    for (i = 0; i < 4096; i++, x += 16) {
        fn_800AF54C(image, (s16)x, 4, 0, 0, 0, 32, 32);
        if (lbl_80343EE8 != 0) {
            fn_800AF1D8(0);
        }
        fn_800AF544(image, pixels);
        if (lbl_80343EEC != 0) {
            fn_800C1148(0, 0, lbl_801164C0);
        }
    }
}

/* Wait for the asynchronous PB error state and acknowledge it. */
void fn_800C1498(void)
{
    asm {
    wait:
        lwz r0, lbl_80344F90
        cmpwi r0, 0
        beq wait
        lwz r0, lbl_80344F90
        cmpwi r0, 2
        beq done
        bge valid
        b reset
    valid:
        cmpwi r0, 4
        bge reset
    }
    fn_800AF1DC(0);
    asm {
        b done
    reset:
        li r0, 0
        stw r0, lbl_80344F90
    done:
    }
}

/* pb-module close stub (referenced from pb_global.c -- keep fn_) */
void fn_800C14F0(void)
{
    WinGlobals* w = gWinGlobals;
    if (w->error) {
        return;
    }
    asm {}
    w->error = &lbl_80344F94;
}

/* pb-module reset stub (referenced from pb_global.c -- keep fn_) */
void fn_800C150C(void)
{
    gWinGlobals->error = &lbl_80344F94;
}
