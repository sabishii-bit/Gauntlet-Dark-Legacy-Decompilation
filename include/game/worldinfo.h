#ifndef GAME_WORLDINFO_H
#define GAME_WORLDINFO_H

#include "types.h"

/* WorldInfo - the loaded world/level scene descriptor.
 *
 * GameCube global: gWorldInfo @ 0x8028CA8C, size 0xA4 (see config symbols.txt).
 *
 * Source of truth for the field names and offsets is the Xbox debug PDB:
 *   research/xbox_symbols/misc.h  ->  struct WORLDINFO  // Size=0xa4 (Id=2207)
 * The GameCube build uses this layout byte-for-byte; every offset below that
 * could be reached in the DOL was cross-checked against the GameCube
 * disassembly of the world/collision/item/camera consumers and matched the
 * Xbox layout exactly (no GC deltas).  Verified field offsets (base gWorldInfo):
 *   +0x04 wobjs   (obj array, stride 0x3C)   world.o, items.o
 *   +0x08 ctris                              auto_fn_8000DFEC (collision)
 *   +0x0C gridrow                            auto_fn_8000D578 (collision)
 *   +0x18 worldmin[3] (lfs)                  camera.o, dyngrid.o, dynobjgrid.o
 *   +0x24 worldmax[3] (lfs)                  dyngrid.o, dynobjgrid.o
 *   +0x48 gridsize / +0x4C invgridsize (lfs) auto_fn_8000DD00 (collision)
 *   +0x50 gridnumx / +0x54 gridnumz          auto_fn_8000DD00
 *   +0x58 checknum / +0x5C coltrichecked     auto_fn_8000D578 / 8000E3B8
 *   +0x60 nwobjs                             items.o
 *   +0x68 iteminfo +0x6C iteminst +0x70 locators
 *   +0x74 niteminfos +0x78 niteminsts +0x7C nlocators   items.o
 *   +0x80 atreelist +0x84 model              world.o
 *
 * NOTE: the Xbox PDB struct tag is WORLDINFO; it is exposed here as WorldInfo
 * to match the GameCube global name gWorldInfo.  The world.c NonMatching model
 * treats a larger struct starting 0xE4 (228) bytes earlier at 0x8028C9A8 (a
 * separate preceding name/state buffer) so its documented offsets (obj@232,
 * count@324, ...) are (this struct's offset + 228): 232->+0x04, 324->+0x60.
 */

struct worldobj;   /* Xbox worldobj   0x3C */
struct coltri;     /* Xbox coltri     0x28 */
struct GRIDROW;    /* Xbox GRIDROW    0x08 */
struct GRIDENTRY;  /* Xbox GRIDENTRY  0x04 */
struct iteminfo;   /* Xbox iteminfo   0x50 */
struct iteminst;   /* Xbox iteminst   0x3C */
struct locator;    /* Xbox locator    0x1C */
struct atreelist;  /* Xbox atreelist  0x18 */
struct worldanim;  /* Xbox worldanim  0x10 */
struct animheader; /* Xbox animheader 0x1C */
struct animdata;   /* Xbox animdata   0xA0 */
struct WORLDPSYS;  /* Xbox WORLDPSYS  0x138 */

typedef struct WorldInfo {
    /* 0x00 */ s32                inited;
    /* 0x04 */ struct worldobj*   wobjs;
    /* 0x08 */ struct coltri*     ctris;
    /* 0x0C */ struct GRIDROW*    gridrow;
    /* 0x10 */ struct GRIDENTRY*  grid;
    /* 0x14 */ char*              gridobjlist;
    /* 0x18 */ f32                worldmin[3];
    /* 0x24 */ f32                worldmax[3];
    /* 0x30 */ f32                worldsize[3];
    /* 0x3C */ f32                worldcenter[3];
    /* 0x48 */ f32                gridsize;
    /* 0x4C */ f32                invgridsize;
    /* 0x50 */ s32                gridnumx;
    /* 0x54 */ s32                gridnumz;
    /* 0x58 */ s32                checknum;
    /* 0x5C */ char*              coltrichecked;
    /* 0x60 */ s32                nwobjs;
    /* 0x64 */ s32                nctris;
    /* 0x68 */ struct iteminfo*   iteminfo;
    /* 0x6C */ struct iteminst*   iteminst;
    /* 0x70 */ struct locator*    locators;
    /* 0x74 */ s32                niteminfos;
    /* 0x78 */ s32                niteminsts;
    /* 0x7C */ s32                nlocators;
    /* 0x80 */ struct atreelist*  atreelist;
    /* 0x84 */ s32                model;
    /* 0x88 */ s32                whitetex;
    /* 0x8C */ struct worldanim*  worldanims;
    /* 0x90 */ s32                nworldanims;
    /* 0x94 */ struct animheader* animheader;
    /* 0x98 */ struct animdata*   animdata;
    /* 0x9C */ struct WORLDPSYS*  worldpsys;
    /* 0xA0 */ s32                nworldpsys;
} WorldInfo; /* size 0xA4 */

extern WorldInfo gWorldInfo; /* 0x8028CA8C */

#endif /* GAME_WORLDINFO_H */
