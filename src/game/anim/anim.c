/*
 * anim.c -- GCN ANIM.OBJ scaffold.
 *
 * The front seam is AnimInit calling anim_play's InitAnimInvDeltaTable.  The
 * back seam is the ZeroAnimData body at 0x8000F628, which starts anim_play.
 *
 * .text       0x8000E8E8..0x8000F628
 * extab       0x80005548..0x80005580
 * extabindex  0x8000883C..0x80008890
 */

#define STUB(address, name) void name(void) {}

STUB(0x8000E8E8, AnimInit)
STUB(0x8000E910, InitAnimInfo)
STUB(0x8000E994, SetupAnimHeader)
STUB(0x8000EB54, fn_8000EB54)
STUB(0x8000EB70, InitAnim)
STUB(0x8000ED70, fn_8000ED70)
STUB(0x8000EF18, fn_8000EF18)
STUB(0x8000F184, fn_8000F184)
STUB(0x8000F2D8, fn_8000F2D8)
STUB(0x8000F534, fn_8000F534)

#undef STUB
