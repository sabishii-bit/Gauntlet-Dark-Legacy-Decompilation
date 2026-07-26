/*
 * mb_tree.c - MB scene-graph node tree (MB_TREE.OBJ).
 *
 * The MB render tree: node alloc/insert/remove, per-node attribute setters
 * (ZMod/Alpha/Color/Flags/Scale/AltTex/AmbientAdd/UVIndex/UVScaleAdd/Texscroll),
 * fog, and the top-level MBTreeInit that also boots the object/blit/poly lists.
 * Node struct: +0x30 pos vec3, +0x60 flags, +0x74 child/next link.
 *
 * Range 0x800BA084..0x800BB5F4 (25 fns). Owns .sbss globals 0x80344EA8..0x80344EE0
 * and its own .sdata2 pool 0x80348CA0..0x80348CC4. Anchored names from the Xbox
 * shell3D PDB (MB_TREE.OBJ): MBTreeInit (biggest fn, inits subsystems),
 * MBTreeSetUVScaleAdd ('Too many UV Scale Add nodes active'). GC emits this module
 * in a scrambled (near-reverse) order; the remaining setter/alloc/remove helpers
 * are left fn_ pending per-field verification. cflags_demo, C++ exceptions on.
 *
 * Status: NonMatching wired skeleton (stubs). Full bodies not reconstructed.
 */

/* 0x800BA084 */
void fn_800BA084(void) {}

/* 0x800BA0FC */
void fn_800BA0FC(void) {}

/* 0x800BA1BC */
void MBTreeSetUVScaleAdd(void) {}

/* 0x800BA2C4 */
void fn_800BA2C4(void) {}

/* 0x800BA368 */
void fn_800BA368(void) {}

/* 0x800BA408 */
void fn_800BA408(void) {}

/* 0x800BA42C */
void fn_800BA42C(void) {}

/* 0x800BA4D0 */
void fn_800BA4D0(void) {}

/* 0x800BA56C */
void fn_800BA56C(void) {}

/* 0x800BA614 */
void fn_800BA614(void) {}

/* 0x800BA6B4 */
void fn_800BA6B4(void) {}

/* 0x800BA6C0 */
void fn_800BA6C0(void) {}

/* 0x800BA784 */
void fn_800BA784(void) {}

/* 0x800BA820 */
void MBTreeInit(void) {}

/* 0x800BACF8 */
void fn_800BACF8(void) {}

/* 0x800BAD90 */
void fn_800BAD90(void) {}

/* 0x800BAD94 */
void fn_800BAD94(void) {}

/* 0x800BAEAC */
void fn_800BAEAC(void) {}

/* 0x800BB164 */
void fn_800BB164(void) {}

/* 0x800BB29C */
void fn_800BB29C(void) {}

/* 0x800BB3AC */
void fn_800BB3AC(void) {}

/* 0x800BB448 */
void fn_800BB448(void) {}

/* 0x800BB4CC */
void fn_800BB4CC(void) {}

/* 0x800BB55C */
void fn_800BB55C(void) {}

/* 0x800BB588 */
void fn_800BB588(void) {}
