/*
 * ml_ffsincos.c - Midway fast float sin/cos (ml_ffsincos.obj).
 *
 * ffsin: polynomial sin over a reduced range with an input-range guard that
 * reports 'sin bad input: %f' via ErrorPrintf; ffcos(x) = ffsin(x + quarter turn).
 * (Xbox SIN_POLY is inlined into ffsin on GC.)
 *
 * Range 0x800BC8D8..0x800BCAAC (2 fns), between ml_error.c and ml_fmath.c. Owns
 * .sdata2 pool 0x80348D68..0x80348D98 (sin polynomial coefficients). Names from
 * Xbox shell3D PDB (ml_ffsincos.obj). cflags_demo, C++ exceptions on.
 *
 * Status: NonMatching wired skeleton (stubs). Full bodies not reconstructed.
 */

/* 0x800BC8D8 */
void ffcos(void) {}

/* 0x800BC904 */
void ffsin(void) {}
