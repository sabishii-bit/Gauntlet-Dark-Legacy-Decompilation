/*
 * mb_struct.c - MB rom-texture descriptor management (mb_struct.obj).
 *
 * The MB library's ROM-texture table accessors: MBRomTexPtr returns a pointer
 * to a texture descriptor entry (indexed via the model-manager's texture table,
 * asserting texidx >= 0 with 'MBRomTexPtr: texidx < 0' via FatalError), and the
 * Set/Copy helpers move 16-byte descriptors between slots. fn_800B9E4C is a
 * small init that zeroes the descriptor context at 0x802C29B8.
 *
 * Range 0x800B9E4C..0x800BA084 (4 fns), between mb_objects.c (ends 0x800B9E4C)
 * and mb_tree.c (starts 0x800BA084). Names from the Xbox shell3D PDB
 * (mb_struct.obj: MBRomTexPtr/MBGetRomTexture/MBSetRomTexture/MBCopyTexture;
 * MBGetRomTexture appears inlined on GC). cflags_demo, C++ exceptions on.
 *
 * Status: NonMatching wired skeleton (stubs). Full bodies not reconstructed.
 */

/* 0x800B9E4C */
void fn_800B9E4C(void) {}

/* 0x800B9EBC */
void MBCopyTexture(void) {}

/* 0x800B9F88 */
void MBSetRomTexture(void) {}

/* 0x800BA024 */
void MBRomTexPtr(void) {}
