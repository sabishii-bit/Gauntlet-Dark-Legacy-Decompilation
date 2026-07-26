/*
 * ml_fmath.c - Midway float matrix/vector math library (ML_FMATH.OBJ).
 *
 * The big Midway 3D math library: 3x3/4x4 matrix build/copy/mul/invert, yaw/pitch/
 * roll rotation-matrix builders (sin/cos), body<->world vector transforms,
 * PYR/YPR/RYP euler<->matrix (Create.../Extract... helpers), angle helpers, and the RNG
 * (Random/RandInt/Randomize over pbRand/srand).
 *
 * Range 0x800BCAAC..0x800BE964 (44 fns). Anchored via unique shape/callees:
 * RandInt/Random/Randomize (pbRand/srand), MulMat4 (mat44Mult), CopyMat3
 * (sceSamp0CopyMatrix34) / CopyMat4 (mat44::operator=). GC emits this module in
 * near-reverse PDB order with heavy inlining; the rotation-matrix and euler
 * helpers are left fn_ pending exact per-function verification. Calls the g3dMath3D
 * C++ mat44 primitives. cflags_demo, C++ exceptions on.
 *
 * Status: NonMatching wired skeleton (stubs). Full bodies not reconstructed.
 */

/* 0x800BCAAC */
void fn_800BCAAC(void) {}

/* 0x800BCB44 */
void fn_800BCB44(void) {}

/* 0x800BCCA8 */
void RandInt(void) {}

/* 0x800BCCE8 */
void Random(void) {}

/* 0x800BCD48 */
void Randomize(void) {}

/* 0x800BCD68 */
void fn_800BCD68(void) {}

/* 0x800BCED8 */
void fn_800BCED8(void) {}

/* 0x800BD050 */
void fn_800BD050(void) {}

/* 0x800BD154 */
void fn_800BD154(void) {}

/* 0x800BD254 */
void fn_800BD254(void) {}

/* 0x800BD360 */
void fn_800BD360(void) {}

/* 0x800BD3A4 */
void fn_800BD3A4(void) {}

/* 0x800BD3E8 */
void fn_800BD3E8(void) {}

/* 0x800BD428 */
void fn_800BD428(void) {}

/* 0x800BD488 */
void fn_800BD488(void) {}

/* 0x800BD7C4 */
void fn_800BD7C4(void) {}

/* 0x800BD804 */
void fn_800BD804(void) {}

/* 0x800BD860 */
void fn_800BD860(void) {}

/* 0x800BD938 */
void fn_800BD938(void) {}

/* 0x800BD9B0 */
void fn_800BD9B0(void) {}

/* 0x800BDA98 */
void fn_800BDA98(void) {}

/* 0x800BDB1C */
void fn_800BDB1C(void) {}

/* 0x800BDB98 */
void fn_800BDB98(void) {}

/* 0x800BDBFC */
void fn_800BDBFC(void) {}

/* 0x800BDC60 */
void fn_800BDC60(void) {}

/* 0x800BDD00 */
void fn_800BDD00(void) {}

/* 0x800BDD7C */
void fn_800BDD7C(void) {}

/* 0x800BDE08 */
void fn_800BDE08(void) {}

/* 0x800BDE80 */
void fn_800BDE80(void) {}

/* 0x800BDEE4 */
void fn_800BDEE4(void) {}

/* 0x800BDF48 */
void fn_800BDF48(void) {}

/* 0x800BE030 */
void fn_800BE030(void) {}

/* 0x800BE1E0 */
void fn_800BE1E0(void) {}

/* 0x800BE360 */
void MulMat4(void) {}

/* 0x800BE3A0 */
void fn_800BE3A0(void) {}

/* 0x800BE448 */
void fn_800BE448(void) {}

/* 0x800BE4F4 */
void fn_800BE4F4(void) {}

/* 0x800BE5A0 */
void fn_800BE5A0(void) {}

/* 0x800BE648 */
void fn_800BE648(void) {}

/* 0x800BE6F4 */
void fn_800BE6F4(void) {}

/* 0x800BE79C */
void fn_800BE79C(void) {}

/* 0x800BE7E4 */
void fn_800BE7E4(void) {}

/* 0x800BE8C8 */
void CopyMat3(void) {}

/* 0x800BE8F4 */
void CopyMat4(void) {}
