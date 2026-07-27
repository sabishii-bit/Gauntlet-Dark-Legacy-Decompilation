#ifndef GAME_ITEM_H
#define GAME_ITEM_H

#include "types.h"

/* Gauntlet Dark Legacy item / world-object system (Xbox ITEMS.OBJ).
 *
 * Struct layouts are taken from the Xbox shell3D.pdb debug build
 * (research/xbox_symbols/misc.h) and are byte-for-byte the retail GC layout
 * (item stride 0xF0 confirmed from the DOL: `mulli rX,idx,240`).
 */

struct iteminfo;
struct mbnode;

/* Camera/lookout waypoint record.  Only the two link fields at the tail have
 * been identified so far; the preceding camera parameters remain opaque. */
typedef struct LookoutParam {
    /* 0x00 */ u8  data[0x68];
    /* 0x68 */ s16 next;
    /* 0x6A */ s16 id;
} LookoutParam; /* 0x6C */

/* An OBJGRP is the transform / node handle shared by every placed object.   */
typedef struct OBJGRP {
    /* 0x00 */ f32 worldmat[4][4];
    /* 0x40 */ f32 attn_pos[4];
    /* 0x50 */ f32 coll_pos[4];
    /* 0x60 */ struct mbnode* node;
    /* 0x64 */ s32 flags;
} OBJGRP; /* 0x68 */

/* Per-type static item description (loaded from the item-info resource).     */
typedef struct iteminfodata {
    /* 0x00 */ s32 subtype;      /* enum item_subtype */
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
    /* 0x48 */ void* atreeheader;
} iteminfodata; /* 0x4C */

typedef struct iteminfo {
    /* 0x00 */ s32 type;        /* enum item_type */
    /* 0x04 */ iteminfodata item;
} iteminfo; /* 0x50 */

/* A live placed item instance.                                               */
typedef struct Item {
    /* 0x00 */ iteminfo* info;
    /* 0x04 */ OBJGRP objgrp;
    /* 0x6C */ u8 atree[0x48];
    /* 0xB4 */ f32 coll_offset[3];
    /* 0xC0 */ s16 ctriidx;
    /* 0xC2 */ s16 nctris;
    /* 0xC4 */ s16 active;
    /* 0xC6 */ s16 activetime;
    /* 0xC8 */ s8 action;
    /* 0xC9 */ s8 paction;
    /* 0xCA */ s8 daction;
    /* 0xCB */ s8 opener;
    /* 0xCC */ s8 minplayers;
    /* 0xCD */ s8 minoff;
    /* 0xCE */ u8 playermask;
    /* 0xCF */ s8 armor;
    /* 0xD0 */ s16 health;
    /* 0xD2 */ s16 gridnext;
    /* 0xD4 */ f32 visrad;
    /* 0xD8 */ f32 fxhittime;
    /* 0xDC */ u8 data[0x14];   /* union: anim/oanim/texmod/psys */
} Item; /* 0xF0 */

/* ---- items.c globals (r13 / SDA-relative in the DOL) --------------------- */
extern Item* sItems;             /* pool base, alloc'd by ResetItems         */
extern s32   sWeaponsHandle;
extern void* sWeaponsBuf;
extern s32   sPowerupsHandle;
extern void* sPowerupsBuf;
extern s32   sItemFile0Handle;
extern void* sGoodWizObj;        /* level-zero item resource buffer */
#define sItemFile0Buf sGoodWizObj
extern s32   sItemFile1Handle;
extern void* sItemFile1Buf;
extern s32   sItemRandSeed;
/* NB: the "weapons"/"powerups" resource names are string literals in
 * LoadWeapons/LoadPowerups (-str readonly: <=8 bytes -> .sdata2/r2). */

/* ---- items.c public API ------------------------------------------------- */
void  InitItems(void);
void  ResetItems(void);
void  LoadItems(void);
void  SetupItemTexMods(void);
void  UnloadWeaponsPowerups(void);
void  LoadWeapons(void);
void  LoadPowerups(char* name);
void  SetupWeaponPowerupTexMods(void);
s32   RandItemIdx(s32 n, s32 mod, s32 advance);
void  InitItemInfoData(void);
struct mbnode* ItemGetNode(s32 idx);
Item* NewItemPtr(void);
Item* AddItem(void* a, void* b);
void  AddItemInstList(void);
void* PlaceItem(char* name, void* a, void* b);
void  SetItem(Item* item, s32 flag, void* a, void* b);
void  MatchTransporters(void);
void  AddLocatorInstList(void* list, s32 count);
LookoutParam* FindLookoutParam(s32 id);
s32   FindClosestWaypoint(f32* pos);
LookoutParam* NextWaypoint(LookoutParam* waypoint);
s32   ClosestStartPos(f32* pos);
void  ShowCameras(s32 on);
void  ShowMilestones(s32 on);
void  GetMilestonePos(s32 idx, f32* out);
void  update_player_milestone(void);
void  RuneCamActivate(s32 idx);
void  WindowCamActivate(s32 idx);
void  SumnerCamActivate(s32 idx, s32 sub);
void  CrystalCamActivate(void);

#endif /* GAME_ITEM_H */
