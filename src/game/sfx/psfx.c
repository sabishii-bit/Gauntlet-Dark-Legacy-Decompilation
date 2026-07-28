/*
 * psfx.c -- GCN PSFX.OBJ scaffold.
 *
 * Player-specific effects, damage, weapon trails, and player-data loading.
 * The last body scans for the player-data files; the following one-instruction
 * body is PSX2.OBJ's LoadVU1GameLogic.
 *
 * .text       0x80089120..0x8008BC50
 * extab       0x80006BE8..0x80006C50
 * extabindex  0x8000AA2C..0x8000AAC8
 */

#define STUB(address, name) void name(void) {}

STUB(0x80089120, PlayerDoWeapTrail)
STUB(0x80089350, fn_80089350)
STUB(0x800898DC, fn_800898DC)
STUB(0x80089EA8, fn_80089EA8)
STUB(0x8008A0E4, fn_8008A0E4)
STUB(0x8008A34C, fn_8008A34C)
STUB(0x8008A584, PlayerSfxClearData)
STUB(0x8008A5F4, PlayerSfxInitData)
STUB(0x8008A678, fn_8008A678)
STUB(0x8008A82C, ClearAllPlyrData)
STUB(0x8008A898, ClearPlyrData)
STUB(0x8008A928, LoadPlyrData)
STUB(0x8008BAF0, LoadPdataFile)

#undef STUB
