#ifndef GAME_ITEM_H
#define GAME_ITEM_H

#include "types.h"

/* ================================================================== *
 *  Gauntlet Dark Legacy -- world item / pickup records (ITEMS.OBJ)
 * ================================================================== *
 *
 * Reconstructed from the Xbox debug build (shell3D.pdb) symbols:
 *   research/xbox_symbols/misc.h
 *     struct item        Id=3252  Size=0xf0   (the live world-item record)
 *     struct iteminfo    Id=2265  Size=0x50   (an item *definition*)
 *     struct iteminfodata Id=3277 Size=0x4c   (definition payload)
 *     struct randominfo  Id=3278  Size=0x24   (random-item definition payload)
 *     struct iteminst    Id=3314  Size=0x3c   (placed-item instance record)
 *     enum item_type / enum item_subtype
 *
 * GameCube (GUNE5D) verification -- the retail item pool lives behind a base
 * pointer at lbl_80344950 (.sbss) and the tiny accessors in game/world/items.c
 * index it at STRIDE 0xF0, exactly matching the Xbox struct size.  Offsets
 * confirmed byte-for-byte against the GC DOL:
 *
 *   fn_80063C44:  item[i] = *lbl_80344950 + i*240; return item->objgrp.node   (+0x64)
 *   fn_80063D0C:  test item->health(+0xD0) > 0 && item->gridnext?(+0xDE) > 0
 *   fn_80063D40:  health(+0xD0) = info->hitpoints*3; armor(+0xCF) = info->armor
 *   NewItemPtr:   memset(slot, 0, 240); ctriidx(+0xC0) = -1; scans active(+0xC4)==-1
 *                 for a free slot; preserves gridnext(+0xD2) across the memset.
 *
 * GC vs Xbox: NO layout deltas found.  Every accessor-touched offset
 * (info@0x00, objgrp.node@0x64, ctriidx@0xC0, active@0xC4, armor@0xCF,
 * health@0xD0, gridnext@0xD2) matches the Xbox struct, and the whole slot is
 * a 0xF0-byte memset.
 *
 * This header is inert until a .c #includes it.
 */

/* ------------------------------------------------------------------ *
 * Forward declarations (pointer members only -- no layout pulled in). *
 * ------------------------------------------------------------------ */
struct mbnode;
struct animdata;
struct objanim;
struct texmod;
struct atreeheader;

/* ------------------------------------------------------------------ *
 * enum item_type -- discriminator for iteminfo.info (Xbox misc.h)     *
 * ------------------------------------------------------------------ */
typedef enum item_type {
    ITEM_RANDOM      = -1,
    ITEM_POWERUP     = 1,
    ITEM_CONTAINER   = 2,
    ITEM_GENERATOR   = 3,
    ITEM_ENEMYINFO   = 4,
    ITEM_TRIGGER     = 5,
    ITEM_TRAP        = 6,
    ITEM_DOOR        = 7,
    ITEM_DAMAGETILE  = 8,
    ITEM_EXIT        = 9,
    ITEM_OBSTICLE    = 10,   /* [sic] -- original Midway spelling */
    ITEM_TRANSPORTER = 11,
    ITEM_ROTATOR     = 12,
    ITEM_SOUND       = 13
} item_type;

/* ------------------------------------------------------------------ *
 * enum item_subtype -- iteminfodata.subtype (Xbox misc.h)             *
 * ------------------------------------------------------------------ */
typedef enum item_subtype {
    ITEM_GOLD        = 1,
    ITEM_KEY         = 2,
    ITEM_FOOD        = 3,
    ITEM_POTION      = 4,
    ITEM_WEAPON      = 5,
    ITEM_ARMOR       = 6,
    ITEM_SPEED       = 7,
    ITEM_MAGIC       = 8,
    ITEM_SPECIAL     = 9,
    ITEM_RUNESTONE   = 10,
    ITEM_BOSSKEY     = 11,
    ITEM_OBELISK     = 12,
    ITEM_QUEST       = 13,
    ITEM_SCROLL      = 14,
    ITEM_GEMSTONE    = 15,
    ITEM_FEATHER     = 16,
    SUB_BRIDGEPAD    = 20,
    SUB_DOORPAD      = 21,
    SUB_BRIDGESWITCH = 22,
    SUB_DOORSWITCH   = 23,
    SUB_ACTIVESWITCH = 24,
    SUB_ELEVPAD      = 25,
    SUB_ELEVSWITCH   = 26,
    SUB_LIFTPAD      = 27,
    SUB_LIFTSTART    = 28,
    SUB_LIFTEND      = 29,
    SUB_NOWEAPCOL    = 30,
    SUB_SHOOTTRIG    = 31,
    SUB_ROCKFALL     = 40,
    SUB_SAFEROCK     = 41,
    SUB_WALL         = 42,
    SUB_BARREL       = 43,
    SUB_BARREL_EXP   = 44,
    SUB_BARREL_POI   = 45,
    SUB_CHEST        = 46,
    SUB_CHEST_GOLD   = 47,
    SUB_CHEST_SILVER = 48,
    SUB_LEAFFALL     = 49,
    SUB_SECRET       = 50,
    SUB_ROCKFLY      = 51,
    SUB_SHOOTFALL    = 52,
    SUB_ROCKSINK     = 53
} item_subtype;

/* ------------------------------------------------------------------ *
 * struct randominfo  (Size=0x24) -- iteminfo payload for ITEM_RANDOM  *
 * ------------------------------------------------------------------ */
typedef struct randominfo {
    /* 0x00 */ s32 numentries;
    /* 0x04 */ s16 infoidx[16];
} randominfo;                     /* 0x24 */

/* ------------------------------------------------------------------ *
 * struct iteminfodata  (Size=0x4c) -- iteminfo payload for non-random *
 * item types; this is the authored item *definition*.                *
 * ------------------------------------------------------------------ */
typedef struct iteminfodata {
    /* 0x00 */ item_subtype subtype;   /* enum (4 bytes) */
    /* 0x04 */ s16 coltype;
    /* 0x06 */ s16 colflags;
    /* 0x08 */ f32 radius;
    /* 0x0C */ f32 height;
    /* 0x10 */ f32 xdim;
    /* 0x14 */ f32 zdim;
    /* 0x18 */ f32 coloffset[3];
    /* 0x24 */ char desc[16];
    /* 0x34 */ u32 mbflags;
    /* 0x38 */ u32 properties;
    /* 0x3C */ s16 value;
    /* 0x3E */ s16 armor;
    /* 0x40 */ s16 hitpoints;
    /* 0x42 */ s16 activetype;
    /* 0x44 */ s16 activeoff;
    /* 0x46 */ s16 activeon;
    /* 0x48 */ struct atreeheader* atreeheader;
} iteminfodata;                   /* 0x4c */

/* ------------------------------------------------------------------ *
 * struct iteminfo  (Size=0x50) -- an ITEM DEFINITION.  info is a      *
 * union discriminated by `type`: iteminfodata for authored items,     *
 * randominfo for ITEM_RANDOM tables.                                  *
 * ------------------------------------------------------------------ */
typedef struct iteminfo {
    /* 0x00 */ item_type type;         /* enum (4 bytes) */
    /* 0x04 */ union {
        iteminfodata item;             /* 0x4c */
        randominfo   random;           /* 0x24 */
    } info;
} iteminfo;                       /* 0x50 */

/* ------------------------------------------------------------------ *
 * OBJGRP / atree are large shared structs owned by other modules.     *
 * They are embedded here by size only so `struct item` stays          *
 * offset-exact and dependency-free.  Real Xbox layouts (misc.h):      *
 *                                                                     *
 *   struct OBJGRP  Size=0x68:                                         *
 *     0x00 f32 worldmat[4][4]                                         *
 *     0x40 f32 attn_pos[4]                                            *
 *     0x50 f32 coll_pos[4]                                            *
 *     0x60 struct mbnode* node    <- item accessor fn_80063C44 (+0x64)*
 *     0x64 int flags                                                  *
 *                                                                     *
 *   struct atree   Size=0x48:                                         *
 *     0x00 struct anode* root                                         *
 *     0x04 struct animinfo animinfo (0x38)                            *
 *     0x3C int nanodes                                                *
 *     0x40 struct anode* firstanode                                   *
 *     0x44 struct anodeinfo* anodeinfo                                *
 * ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ *
 * struct item  (Size=0xf0) -- the live world-item / pickup record.    *
 * GC pool: (*lbl_80344950)[i], stride 0xF0.                           *
 * ------------------------------------------------------------------ */
typedef struct item {
    /* 0x00 */ iteminfo* info;         /* item definition this instance uses */
    /* 0x04 */ u8 objgrp[0x68];        /* struct OBJGRP (node@+0x60, flags@+0x64) */
    /* 0x6C */ u8 atree[0x48];         /* struct atree */
    /* 0xB4 */ f32 coll_offset[3];
    /* 0xC0 */ s16 ctriidx;            /* collision-tri index (NewItemPtr sets -1) */
    /* 0xC2 */ s16 nctris;
    /* 0xC4 */ s16 active;             /* -1 == free slot (NewItemPtr scan key) */
    /* 0xC6 */ s16 activetime;
    /* 0xC8 */ s8  action;
    /* 0xC9 */ s8  paction;
    /* 0xCA */ s8  daction;
    /* 0xCB */ s8  opener;
    /* 0xCC */ s8  minplayers;
    /* 0xCD */ s8  minoff;
    /* 0xCE */ u8  playermask;
    /* 0xCF */ s8  armor;              /* fn_80063D40: = info->hitpoints armor */
    /* 0xD0 */ s16 health;             /* fn_80063D40: = info->hitpoints * 3 */
    /* 0xD2 */ s16 gridnext;           /* NewItemPtr preserves this across memset */
    /* 0xD4 */ f32 visrad;
    /* 0xD8 */ f32 fxhittime;
    /* 0xDC */ union {
        struct animdata* anim;
        struct objanim*  oanim;
        struct texmod*   texmod;
        struct mbnode*   psys;
        u8 _pad[0x14];                 /* GC allocates 0x14 bytes here (0xDC-0xF0) */
    } data;
} item;                           /* 0xf0 */

/* ------------------------------------------------------------------ *
 * struct iteminst  (Size=0x3c) -- a placed-item instance record       *
 * (world-editor placement data, parsed into `item` slots).            *
 * ------------------------------------------------------------------ */
typedef struct iteminst {
    /* 0x00 */ s16  index;
    /* 0x02 */ s8   minplayers;
    /* 0x03 */ s8   flags;
    /* 0x04 */ s16  ctriidx;
    /* 0x06 */ s16  nctris;
    /* 0x08 */ char desc[16];
    /* 0x18 */ f32  pos[3];
    /* 0x24 */ f32  pyr[3];
    /* 0x30 */ u8   params[0xc];       /* union __unnamed params (type-specific) */
} iteminst;                       /* 0x3c */

#endif /* GAME_ITEM_H */
