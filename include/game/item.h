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
struct Player;
struct Item;
struct worldobj;

/* Camera/lookout waypoint record == the Xbox PDB record LOOKOUT (misc.h,
 * 0x6C): an OBJGRP-shaped prefix {worldmat/attn_pos/coll_pos/node/flags}
 * followed by {s16 next; s16 param}.  The former opaque data[0x30]/pos/pad
 * run is the world matrix -- add_arrow writes it via its matrix-out arg, and
 * the old pos[3]@0x30 was worldmat[3][0..2], its translation row.  The
 * OBJGRP correspondence is spelled with plain scalar arrays rather than an
 * embedded OBJGRP member: an aggregate-typed member in a widely-included
 * header regresses unrelated functions TU-wide (embedded-cascade law). */
typedef struct LookoutParam {
    /* 0x00 */ f32 worldmat[4][4]; /* == OBJGRP.worldmat; [3][0..2] = position */
    /* 0x40 */ f32 attn_pos[4];    /* == OBJGRP.attn_pos */
    /* 0x50 */ f32 coll_pos[4];    /* == OBJGRP.coll_pos */
    /* 0x60 */ s32 node;           /* == OBJGRP.node (mbnode handle) */
    /* 0x64 */ s32 flags;          /* == OBJGRP.flags */
    /* 0x68 */ s16 next;
    /* 0x6A */ s16 param;
} LookoutParam; /* 0x6C */

/* An OBJGRP is the transform / node handle shared by every placed object.   */
#ifndef GAME_OBJGRP_DEFINED
#define GAME_OBJGRP_DEFINED
typedef struct OBJGRP {
    /* 0x00 */ f32 worldmat[4][4];
    /* 0x40 */ f32 attn_pos[4];
    /* 0x50 */ f32 coll_pos[4];
    /* 0x60 */ struct mbnode* node;
    /* 0x64 */ s32 flags;
} OBJGRP; /* 0x68 */
#endif

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

/* A placed item record embedded in WORLDINFO before it is expanded into an
 * Item.  This is the Xbox PDB layout and matches the GC loader byte-for-byte. */
typedef struct iteminst {
    /* 0x00 */ s16 index;
    /* 0x02 */ s8  minplayers;
    /* 0x03 */ s8  flags;
    /* 0x04 */ s16 ctriidx;
    /* 0x06 */ s16 nctris;
    /* 0x08 */ char desc[16];
    /* 0x18 */ f32 pos[3];
    /* 0x24 */ f32 pyr[3];
    /* 0x30 */ u8  params[12];
} iteminst; /* 0x3C */

/* Compact world-locator record consumed by AddLocatorInstList. */
typedef struct locator {
    /* 0x00 */ u8  type;
    /* 0x01 */ u8  subtype;
    /* 0x02 */ s16 index;
    /* 0x04 */ f32 pos[3];
    /* 0x10 */ f32 pyr[3];
} locator; /* 0x1C */


/* ==========================================================================
 * Item.data -- the per-item-type union
 *
 * The Xbox PDB declares Item.data as `union __unnamed data; // Offset=0xdc
 * Size=0x14` (research/xbox_symbols/misc.h line 27771, Id=3251) with eleven
 * variants, every one at offset 0x00 of the 0x14-byte member.  The variant is
 * selected by Item.info->type, and the GameCube code proves that selection
 * independently: fn_800606FC (src/game/world/gauntworld.c) dispatches on
 * `type = it->info->type` and the case labels it uses -- 1, 2, 3, 5, 9, 0xA,
 * 0xC, 0xD -- reach exactly the field sets that the PDB's powerupdata,
 * containerdata, gendata, triggerdata, exitdata, obsticledata, rotdata and
 * sounddata name, matching the PDB's own `enum item_type` values
 * (ITEM_POWERUP=1, ITEM_CONTAINER=2, ITEM_GENERATOR=3, ITEM_TRIGGER=5,
 * ITEM_EXIT=9, ITEM_OBSTICLE=10, ITEM_TRANSPORTER=11, ITEM_ROTATOR=12,
 * ITEM_SOUND=13).
 *
 * A field below is TYPED only where a GameCube access proves its width and
 * signedness; the citation is on the member.  Everything else stays a sized
 * raw array naming the PDB field it stands for -- an unproven name is not a
 * recovery.  Byte offsets are relative to the start of the union (Item+0xDC).
 * ========================================================================== */

/* ITEM_POWERUP (info->type 1).  PDB powerupdata, misc.h Id=3317. */
typedef struct powerupdata {
    /* 0x00 */ u8  _pad00[4];   /* PDB: unsigned int properties - unproven   */
    /* 0x04 */ s32 value;       /* gold amount: gauntworld fn_8005DE50 reads
                                 * *(s32*)&b->data[4] for subtype ITEM_GOLD  */
    /* 0x08 */ u8  _pad08[4];   /* PDB: float duration - unproven            */
    /* 0x0C */ struct Item* container;  /* gauntworld fn_8005C1DC: guarded by
                                 * info->type == 1, passed to DeleteItem     */
    /* 0x10 */ s16 nograb;      /* gauntworld fn_800606FC case 1: `> 0` then
                                 * `-= gFrameTicks`, a s16 countdown         */
    /* 0x12 */ u8  _pad12[2];   /* PDB: short dummy - unproven               */
} powerupdata; /* 0x14 */

/* ITEM_CONTAINER (info->type 2).  PDB containerdata, misc.h Id=3327. */
typedef struct containerdata {
    /* 0x00 */ s16 index;       /* gauntworld fn_8005C1DC case 2: indexes
                                 * gWorldInfo.iteminfo[] after a `>= 0` test */
    /* 0x02 */ s16 timer;       /* fn_800606FC case 2: `- gFrameTicks`,
                                 * clamped at 0, then tested `& 8`           */
    /* 0x04 */ f32 yaw;         /* fn_800606FC case 2: loaded into the yaw
                                 * slot of the CreateYPRMatrix argument      */
    /* 0x08 */ struct mbnode* attatch;  /* gauntworld fn_8005D0C4 region:
                                 * passed to MBNodeSetParent as the parent   */
    /* 0x0C */ struct Item* contents;   /* same site stores the item it just
                                 * created here; fn_800606FC case 2 walks it */
    /* 0x10 */ u8  _pad10[2];   /* PDB: short value - unproven               */
    /* 0x12 */ u8  _pad12[2];   /* PDB: short temp - unproven                */
} containerdata; /* 0x14 */

/* ITEM_GENERATOR (info->type 3).  PDB gendata, misc.h Id=3328.  Every field
 * is proven by fn_800606FC's generator case in src/game/world/gauntworld.c. */
typedef struct gendata {
    /* 0x00 */ s16 etype;         /* `== -1` idle test; generate_enemy type  */
    /* 0x02 */ s8  numenemies;    /* compared against maxenemies, ++ per spawn*/
    /* 0x03 */ s8  maxenemies;
    /* 0x04 */ s8  num_generated; /* `& 1` alternation (ai 0x0E), `>= 3` (0x0C)*/
    /* 0x05 */ s8  tail;          /* previous spawn's gEnemies slot, linked
                                   * through Enemy.prev_enemy/next_enemy     */
    /* 0x06 */ s8  strength;      /* generate_enemy argument 3, sign-extended*/
    /* 0x07 */ s8  ai;            /* generate_enemy argument 5; 0x0C..0x0F   */
    /* 0x08 */ s16 counter;       /* stored from the wob scale as (s16)      */
    /* 0x0A */ u8  flags;         /* byte store of 0 on the ai 0x0C path     */
    /* 0x0B */ u8  interval;      /* read zero-extended into the wob scale   */
    /* 0x0C */ f32 genratio;      /* accumulated per spawn, wrapped at radius*/
    /* 0x10 */ f32 ang;           /* added to Enemy.genang_offset            */
} gendata; /* 0x14 */

/* ITEM_ENEMYINFO (info->type 4).  PDB enemydata, game.h Id=3325.  Proven by
 * fn_80060114 in src/game/world/gauntworld.c. */
typedef struct enemydata {
    /* 0x00 */ s16 etype;       /* switch selector and generate_enemy type   */
    /* 0x02 */ s8  strength;    /* generate_enemy argument 3, sign-extended  */
    /* 0x03 */ s8  ai;          /* generate_enemy argument 5, sign-extended  */
    /* 0x04 */ u8  _pad04[4];   /* PDB: float ang - not read by any GC site  */
    /* 0x08 */ u32 flags;       /* the `& 1` spawn gate                      */
    /* 0x0C */ f32 rad;         /* scaled by gCurLevel->ene_visrad into
                                 * Enemy.sight and crit_inst+0xAD0           */
    /* 0x10 */ s16 interval;    /* scaled by sItemFloorYOffset into idle_time*/
    /* 0x12 */ s16 pickup;      /* sItems element index -> Enemy.gotitem     */
} enemydata; /* 0x14 */

/* ITEM_TRIGGER (info->type 5).  PDB triggerdata, misc.h Id=3294. */
typedef struct triggerdata {
    /* 0x00 */ struct worldobj* target;  /* ActivateSpecialTrigger drives its
                                 * flags/triggerstate/ptriggerstate/nodeptr  */
    /* 0x04 */ s16 flags;       /* fn_800606FC case 5 tests 0x40/0x100/0x400 */
    /* 0x06 */ s8  id;          /* ActivateSpecialTrigger's `type` key; also
                                 * the tower level / garg argument           */
    /* 0x07 */ s8  nextid;      /* items.c LinkItemTriggers reads it as s8   */
    /* 0x08 */ struct Item* next;       /* items.c LinkItemTriggers stores the
                                 * successor here; case 5 fans playermask    */
    /* 0x0C */ u8  _pad0C[4];   /* PDB: float rad - unproven                 */
    /* 0x10 */ s16 idletime;    /* case 5: `> 0` then `-= gFrameTicks`       */
    /* 0x12 */ s16 camid;       /* case 5: `>= 0`, then indexes the camera
                                 * record array with stride 0x28             */
} triggerdata; /* 0x14 */

/* ITEM_TRAP (info->type 6).  PDB trapdata, misc.h Id=3333, size 0x8. No GC
 * access in the current source proves either field, so both stay raw. */
typedef struct trapdata {
    /* 0x00 */ u8 _pad00[4];    /* PDB: float damage                         */
    /* 0x04 */ u8 _pad04[2];    /* PDB: short interval                       */
} trapdata; /* 0x8 */

/* ITEM_EXIT (info->type 9).  PDB exitdata, misc.h Id=3329, size 0x8. */
typedef struct exitdata {
    /* 0x00 */ s16 wave;        /* fn_800606FC case 9 reads it as s16 and
                                 * narrows it to u8 for a table index        */
    /* 0x02 */ u8  _align0[2];  /* PDB: unsigned char __align0[2]            */
    /* 0x04 */ s32 onexit;      /* case 9: a player bitmask, compared against
                                 * pmask and cleared with 0                  */
} exitdata; /* 0x8 */

/* ITEM_OBSTICLE (info->type 10).  PDB obsticledata, misc.h Id=3332. */
typedef struct obsticledata {
    /* 0x00 */ s16 subtype;    /* items.c ShowSafeRocks, guarded by
                                * info->type == 10, compares it as an s16
                                * against 0x29                              */
    /* 0x02 */ s16 strength;    /* fn_8005D3D8 case 10 gates on `> 0`        */
    /* 0x04 */ s16 flash;       /* case 0xA: decremented, `== 1`, cleared    */
    /* 0x06 */ s16 timer;       /* case 0xA: `+= gFrameTicks`, wrapped 0x3C  */
    /* 0x08 */ f32 vel[3];      /* case 0xA writes three consecutive f32 from
                                 * dir[0..2] and integrates by gClockFrameStep*/
} obsticledata; /* 0x14 */

/* ITEM_TRANSPORTER (info->type 11).  PDB transdata, misc.h Id=3330, size 0xC.
 * All three fields are proven by MatchTransporters in src/game/world/items.c,
 * which is guarded by `info->type == 11` and addresses them as word indices
 * 0x37/0x38/0x39 off the Item (bytes 0xDC/0xE0/0xE4). */
typedef struct transdata {
    /* 0x00 */ s32 id;
    /* 0x04 */ s32 destid;
    /* 0x08 */ struct Item* dest;
} transdata; /* 0xC */

/* ITEM_ROTATOR (info->type 12).  PDB rotdata, misc.h Id=3290, size 0x10.
 * All four fields are proven by fn_800606FC case 0xC. */
typedef struct rotdata {
    /* 0x00 */ struct worldobj* target;  /* YawMat3 is applied to its nodeptr*/
    /* 0x04 */ f32 speed;       /* multiplied by gFrameTicks into an angle   */
    /* 0x08 */ f32 delta;       /* the +/- limit curang is compared against  */
    /* 0x0C */ f32 curang;      /* accumulated by the per-frame angle        */
} rotdata; /* 0x10 */

/* ITEM_SOUND (info->type 13).  PDB sounddata, audio.h Id=3331. */
typedef struct sounddata {
    /* 0x00 */ f32 radius;      /* case 0xD: the attenuation range           */
    /* 0x04 */ s32 sound;       /* case 0xD: AudioSecretProc's sound argument*/
    /* 0x08 */ struct mbnode* parent;   /* case 0xD: GetWorldMat's node      */
    /* 0x0C */ s16 musicarea;   /* case 0xD: read as s16 into the sub select */
    /* 0x0E */ s16 active;      /* case 0xD: holds vmask / -vinst / 0 and is
                                 * tested both `< 0` and `> 0`               */
    /* 0x10 */ s16 fade;        /* case 0xD: AudioSecretProc's state argument*/
    /* 0x12 */ u8  _pad12[2];   /* PDB: short flags - unproven               */
} sounddata; /* 0x14 */

/* The union itself.  `raw` is NOT a PDB member: it is the byte view that the
 * not-yet-converted accesses in gauntworld.c, items.c and recorder.c still
 * use, kept so those sites stay byte-identical while their variants are
 * recovered one measured region at a time.  It is staged reconstruction debt,
 * not recovered source. */
typedef union itemdata {
    powerupdata   powerup;
    containerdata container;
    gendata       gen;
    enemydata     enemy;
    triggerdata   trigger;
    trapdata      trap;
    exitdata      exit;
    obsticledata  obsticle;
    transdata     trans;
    rotdata       rot;
    sounddata     sound;
    u8            raw[0x14];
} itemdata; /* 0x14 */

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
    /* 0xDC */ itemdata data;   /* per-info->type union; see above */
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
extern u32   sItemRandSeed;
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
Item* PlaceItem(s32 type, s32 level, char* name, void* matrix);
void  SetItem(Item* item, iteminst* instance, iteminfo* info, f32* matrix);
void  MatchTransporters(void);
void  AddLocatorInstList(void);
LookoutParam* FindLookoutParam(s32 id);
LookoutParam* FindClosestWaypoint(f32 maxDist, f32* pos, s32 all);
LookoutParam* NextWaypoint(LookoutParam* waypoint);
s32   ClosestStartPos(f32* pos);
s32   ShowCameras(s32 idx);
s32   ShowMilestones(s32 idx);
void  GetMilestonePos(s32 idx, f32* out);
void  update_player_milestone(struct Player* player);
void  RuneCamActivate(s32 idx);
void  WindowCamActivate(s32 idx);
void  SumnerCamActivate(s32 idx, s32 sub);
void  CrystalCamActivate(void);

#endif /* GAME_ITEM_H */
