#ifndef GAME_WORLDCOL_H
#define GAME_WORLDCOL_H

/* Deliberately dependency-free: worldcol.c predates types.h and declares
 * its own primitives, and porting it onto types.h moves bytes (`s32` as
 * `signed long` instead of `signed int` rewrites 96 words of WorldCollide
 * with no size change).  So this header only names f32/s32 and takes the
 * surface object as an opaque `struct worldobj*`; every other includer
 * already pulls types.h and game/worldobj.h for the complete type. */
struct worldobj;

/*
 * worldcol.h -- the world-collision probe result record.
 *
 * FloorCollide()/FloorPos() (src/game/world/worldcol.c) fill this record and
 * every mover reads it out of the shared instance gFloorCollisionResult
 * (0x8023CAE0).  The layout below is established entirely from GameCube
 * accesses; no positional PDB match was found for it, so no PDB name is
 * claimed for the record or its fields.
 *
 * Proving accesses, owning TU first:
 *   worldcol.c FloorPos      passes `(f32*)&gFloorCollisionResult` straight to
 *                            WorldCollide as the output matrix, and returns
 *                            the f32 at +0x34 as the floor height.
 *   worldcol.c FloorCollide  clears the word at +0x44 before probing, and
 *                            takes the record by pointer (`FloorCollisionResult*
 *                            result`), so it is one record and not a bare array.
 *   sfx.c                    CopyMat4(gFloorCollisionResult, mat) -- a full
 *                            0x40-byte 4x4 matrix starts at +0x00.
 *   critter.c                CopyMat3((f32*)gFloorCollisionResult, c->shadow)
 *                            copies the first three rows into a shadow node's
 *                            mat, then overwrites row 3 with the critter
 *                            position -- so rows 0..2 are the basis and row 3
 *                            is the contact point.
 *   pmotion.c                reads +0x30 as x and +0x38 as z beside +0x34 as y
 *                            (the same row 3), and dereferences +0x44 as an
 *                            object pointer: `*(u32*)(*(u32*)(base + 68) + 16)`
 *                            is worldobj.flags at +0x10.
 *   critter.c                reads +0x44 then `surface[0x16]` (s8) and
 *                            `*(u8**)(surface + 0x18)` -- worldobj.triggerstate
 *                            and worldobj.parent, at the right width and sign.
 *   pmotion.c                compares +0x44 against `(WorldObj*)motion->floor_obj`,
 *                            which fixes the pointee type independently.
 *
 * So the floor height is mtx[3][1]; `floorY` is kept as the spelling the movers
 * already use, via the row-3 member, rather than as a separate field.
 */

typedef struct FloorCollisionResult {
    /* 0x00 */ f32 mtx[4][4]; /* rows 0..2 surface basis, row 3 contact point */
    /* 0x40 */ s32 _unk40;    /* never read on GC                            */
    /* 0x44 */ struct worldobj* obj; /* surface owner; NULL == no floor     */
} FloorCollisionResult;       /* size 0x48                                   */

#endif /* GAME_WORLDCOL_H */
