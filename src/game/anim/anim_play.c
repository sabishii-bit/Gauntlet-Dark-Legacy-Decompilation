/*
 * anim_play.c -- GCN anim_play.obj scaffold.
 *
 * Function identity is pinned by the Xbox PDB roster and direct call shape:
 * CalcAnimData is the wrapper around GetAnimAngXYZVal, while the final
 * function constructs the 256-entry inverse-delta table.
 *
 * .text       0x8000F628..0x80010A4C
 * extab       0x80005580..0x80005590
 * extabindex  0x80008890..0x800088A8
 */

#define STUB(address, name) void name(void) {}

STUB(0x8000F628, ZeroAnimData)
STUB(0x8000F678, InitAnimData)
STUB(0x8000F72C, CalcAnimData)
STUB(0x8000F74C, InterpXYZ)
STUB(0x8000F788, InterpPYR)
STUB(0x8000F7F4, GetAnimAngXYZVal)
STUB(0x80010850, fn_80010850)
STUB(0x80010904, fn_80010904)
STUB(0x800109E4, InitAnimInvDeltaTable)

#undef STUB
