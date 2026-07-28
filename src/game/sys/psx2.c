/*
 * psx2.c -- GCN PSX2.OBJ scaffold.
 *
 * Retained PS2/IOP compatibility bootstrap.  Xbox PDB source order is
 * reversed by the GCN object: LoadVU1GameLogic, init_psx2, then the merged
 * IRX argument/name helper.
 *
 * .text       0x8008BC50..0x8008BF88
 * extab       0x80006C50..0x80006C60
 * extabindex  0x8000AAC8..0x8000AAE0
 */

#define STUB(address, name) void name(void) {}

STUB(0x8008BC50, LoadVU1GameLogic)
STUB(0x8008BC54, init_psx2)
STUB(0x8008BE44, load_irx_args)

#undef STUB
