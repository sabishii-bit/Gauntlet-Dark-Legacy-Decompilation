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
 * NonMatching: structural skeletons only.
 */

#include "types.h"

extern u32 gErrorCode;           /* 0x80343EF0 (.sdata) */
extern u8  lbl_802C4DB8[0x28];   /* error scratch block (.bss) */
extern u32 lbl_80343F04;
extern u32 lbl_80343F08;
extern char lbl_801164C0[];      /* "PB_ERROR.C:__LINE__" */
extern char lbl_80120E98[];
extern u32 lbl_80344F94;

typedef struct WinGlobals { u8 _pad[64]; } WinGlobals;
extern WinGlobals* gWinGlobals;

extern void fn_800AF544(void);
extern void fn_800AF54C(void);
extern void fn_800AF564(void);
extern void fn_800AF568(void);
extern void fn_800AF56C(void*);
extern void fn_800AF570(int, int, int, int);
extern void fn_800AF1D8(void);
extern void fn_800AF1DC(void);
extern void fn_800C1148(void);          /* mb_window.c helper */
extern void fn_800C13CC(void);

/* Big error reporter: formats into the pb error block (8KB stack buffer),
 * references PB_ERROR.C:__LINE__ and gErrorCode. (One of PB_ERROR.OBJ's
 * pbErrorf / pbFatalErrorf; exact name unpinned -- kept fn_.) */
void fn_800C1174(char* fmt, ...)
{
    char buf[0x2000];
    (void)fmt;
    (void)buf;
    fn_800AF56C(lbl_802C4DB8);
    fn_800AF570(0, 0, 2, 1);
    fn_800C13CC();
    gErrorCode = lbl_80343F04;
    fn_800AF56C(lbl_80120E98);
    fn_800AF1D8();
    fn_800C1148();
}

/* Sub-report used by bulletproof_printf (references PB_ERROR.C:__LINE__). */
void fn_800C13CC(void)
{
    fn_800AF1D8();
    fn_800AF54C();
    fn_800AF544();
    fn_800C1148();
    (void)lbl_801164C0;
}

/* pbErrorf-family reporter. */
void fn_800C1498(void)
{
    fn_800AF1DC();
    (void)lbl_80344F94;
}

/* pb-module close stub (referenced from pb_global.c -- keep fn_) */
void fn_800C14F0(void)
{
    if (*(void**)((char*)gWinGlobals + 12)) {
        return;
    }
    *(void**)((char*)gWinGlobals + 12) = &lbl_80344F94;
}

/* pb-module reset stub (referenced from pb_global.c -- keep fn_) */
void fn_800C150C(void)
{
    *(void**)((char*)gWinGlobals + 12) = &lbl_80344F94;
}
