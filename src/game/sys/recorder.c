/*
 * recorder.c -- GCN RECORDER.OBJ scaffold.
 *
 * Demo-stage snapshot storage.  The two small tail bodies save and restore
 * camera/global stage state; the two front bodies save and restore the full
 * item/player/world snapshot.
 *
 * .text       0x8008BF88..0x8008C52C
 * extab       0x80006C60..0x80006C80
 * extabindex  0x8000AAE0..0x8000AB10
 */

#define STUB(address, name) void name(void) {}

STUB(0x8008BF88, LoadAllRecords)
STUB(0x8008C0F4, SaveAllRecords)
STUB(0x8008C2AC, LoadStage)
STUB(0x8008C430, SaveStage)

#undef STUB
