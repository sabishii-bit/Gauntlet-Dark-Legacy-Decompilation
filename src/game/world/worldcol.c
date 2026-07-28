/*
 * worldcol.c -- GCN WORLDCOL.OBJ scaffold.
 *
 * The Xbox PDB roster has exactly 15 functions and the GCN object contains
 * exactly 15 functions in reverse source order.  The tail is additionally
 * pinned by behavior: CreateMat3Norm builds a basis and PointLineColl performs
 * the point/segment test.
 *
 * .text       0x8000CF40..0x8000E8E8
 * extab       0x800054E0..0x80005548
 * extabindex  0x800087A0..0x8000883C
 */

#define STUB(address, name) void name(void) {}

STUB(0x8000CF40, CameraCollide)
STUB(0x8000CFA0, WeaponWallCollide)
STUB(0x8000D034, SlideAlongWall)
STUB(0x8000D1E0, EnemyWallCollide)
STUB(0x8000D274, PlayerWallCollide)
STUB(0x8000D308, FastWallCollide)
STUB(0x8000D3C4, FloorPos)
STUB(0x8000D4B8, FloorCollide)
STUB(0x8000D578, WorldCollide)
STUB(0x8000DCD8, ExitCollisionEarly)
STUB(0x8000DD00, NextGrid)
STUB(0x8000DFEC, WorldObjCollide)
STUB(0x8000E3B8, CTriListCollide)
STUB(0x8000E674, CreateMat3Norm)
STUB(0x8000E73C, PointLineColl)

#undef STUB
