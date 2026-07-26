/*
 * ml_error.c - Midway error/log subsystem (ML_ERROR.OBJ).
 *
 * The game-level error logging + fatal-error path: bulletproof_printf (formats
 * via vsprintf, echoes to stdout AND appends to a logfile via sce* file API),
 * ErrorPrintf (accumulates '--> ERROR:' lines), FatalErrorf (formats + prints the
 * '========== FATAL ERROR ===========' banner then halts via pbFatalErrorf), and
 * the on-screen error dump (MBSetFontColor/MBDrawText). Strings: 'MATH ERROR',
 * '>>> BREAKPOINT! <<<', ' VERSION %s'.
 *
 * Range 0x800BC2EC..0x800BC8D8 (8 fns). Owns .sbss globals 0x80344EF0..0x80344F04.
 * FatalError/bulletproof_printf/ErrorPrintf were named by prior sessions; sits
 * ABOVE the low-level pb error TU (PB_ERROR.OBJ @0x800C1174 which it calls into).
 * Names from Xbox shell3D PDB (ML_ERROR.OBJ). cflags_demo, C++ exceptions on.
 *
 * Status: NonMatching wired skeleton (stubs). Full bodies not reconstructed.
 */

/* 0x800BC2EC */
void bulletproof_printf(void) {}

/* 0x800BC418 */
void fn_800BC418(void) {}

/* 0x800BC4E4 */
void fn_800BC4E4(void) {}

/* 0x800BC52C */
void fn_800BC52C(void) {}

/* 0x800BC568 */
void FatalError(void) {}

/* 0x800BC590 */
void FatalErrorf(void) {}

/* 0x800BC6E0 */
void ErrorPrintf(void) {}

/* 0x800BC7FC */
void fn_800BC7FC(void) {}
