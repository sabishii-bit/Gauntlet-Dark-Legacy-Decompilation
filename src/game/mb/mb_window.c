/*
 * mb_window.c - MB viewport/window setup (mb_window.obj).
 *
 * MB's window/viewport layer built on the pb projection primitives: sets view
 * angle/zoom (tan-based FOV, calls MBWindowProjection), region/viewport
 * (MBWindowViewport), and projects points (MBCameraUpdate). fn_800BBCA8 is a
 * shared sin/cos rotation helper used by several of these.
 *
 * Range 0x800BB804..0x800BC2EC (7 fns). Owns .sbss global 0x80344EE8 and .sdata2
 * pool 0x80348CC8..0x80348D50. Distinct from pb_window.obj (already wired at
 * 0x800C8294). Anchored: MBWindowSetRegion (biggest fn, MBWindowViewport call).
 * Remaining SetAng/SetHang/Zoom/Project left fn_. cflags_demo, C++ exceptions on.
 *
 * Status: NonMatching wired skeleton (stubs). Full bodies not reconstructed.
 */

/* 0x800BB804 */
void fn_800BB804(void) {}

/* 0x800BB8E8 */
void fn_800BB8E8(void) {}

/* 0x800BBA34 */
void fn_800BBA34(void) {}

/* 0x800BBB70 */
void fn_800BBB70(void) {}

/* 0x800BBCA8 */
void fn_800BBCA8(void) {}

/* 0x800BBE88 */
void MBWindowSetRegion(void) {}

/* 0x800BC23C */
void fn_800BC23C(void) {}
