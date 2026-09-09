#include "types.h"
#include "game/options.h"
#include "game/item.h"
#include "game/enemy.h"
#include "game/gamemode.h"
#include "game/worldinfo.h"
#include "game/worldobj.h"
#include "game/player.h"
#include "game/camera.h"
#include "game/leveldata.h"

#ifndef offsetof
#define offsetof(type, memb) ((u32) & ((type*)0)->memb)
#endif

/*
 * dtk makes TU-local functions globally addressable in the extracted object
 * by appending their retail address.  Keep the recovered source names readable
 * while emitting symbols that objdiff can pair with those target functions.
 */
/* Gauntlet item / world-object system (Xbox ITEMS.OBJ), region
 * 0x800631AC-0x80067904 -- followed by the separate LIGHTS.OBJ module.
 *
 * This is the GameCube retail slice of ITEMS.OBJ.  The Xbox shell3D.pdb debug
 * build lists 103 functions in this module; the retail GC build keeps ~51.
 * The loader / init / camera / waypoint / milestone tail (0x80065B60-end)
 * links in the *reverse* of the Xbox source order, so those names below are
 * anchored 1:1 to the PDB.  The front half (the item-spawn / collision block)
 * was re-ordered in the GC source and is named behaviourally / by string.
 *
 * Wired NonMatching: the DOL links from the dtk-extracted asm object while
 * reconstruction continues.  The complete retail function range is mapped
 * below, including the large SetItem constructor and serialized file loaders.
 *
 * NAMED (real Xbox-PDB names; string / call-graph / struct anchored):
 *   front (behavioural):  PlaceItem, AddItem, NewItemPtr, MatchTransporters,
 *                         SetItem, SetItemGeo, AddItemWobj, ItemGetNode
 *   tail (reverse-order): AddLocatorInstList, LinkTriggerToCam, FindLookoutParam,
 *                         Crystal/Sumner/Window/RuneCamActivate, ShowCameras,
 *                         ShowMilestones, add_arrow, NextWaypoint,
 *                         FindClosestWaypoint, InitItemInfoData,
 *                         AtreeMatchAnyHeader, GetMilestonePos,
 *                         update_player_milestone, Closest/SetPlayerStartPos,
 *                         SetupItemTexMods, SetupWeaponPowerupTexMods,
 *                         RandItemIdx, LoadItems, ResetItems, InitItems,
 *                         Load/UnloadWeapons/Powerups
 */

/* ------------------------------------------------------------------ */
/* cross-file externs                                                 */
/* ------------------------------------------------------------------ */
extern int   LoadModel(const char* name, void** out, int a, int b); /* GLUE.OBJ model loader */
extern void  InitTexMods(void* buf, int handle);                       /* fn_8007xxxx */
extern void  DoPlayerTexMods(int idx);                                     /* per-slot texmod */
extern int   ErrorPrintf(const char* fmt, ...);
extern int   sprintf(char* dst, const char* fmt, ...);
extern int   FileExists(const char* dev, const char* path);
extern void* AllocMem(u32 size);
extern void  TriggerCameraActivate(s32 type, f32* eye, f32* target,
                                   s32 duration, s32 flags, s32 variant);
extern char* LevelItemDesc(void);
extern char* WorldItemDesc(void);
extern void* MBNewNode(void* parent, f32* matrix, s32 flags);
extern void  MBTreeSetFlags(void* node, s32 flags, s32 value);

typedef struct TriggerCamera {
    /* 0x00 */ u8  type;
    /* 0x01 */ u8  subtype;
    /* 0x02 */ s16 active;
    /* 0x04 */ f32 eye[4];
    /* 0x14 */ f32 target[3];
    /* 0x20 */ u8  _pad20[4];
    /* 0x24 */ void* handle;
} TriggerCamera; /* 0x28 */

typedef struct RuneCameraVariants {
    s32 value[3];
} RuneCameraVariants;

typedef struct MilestoneParam {
    f32 matrix[16];
    f32 pos[3];
    u8  _pad4C[4];
    f32 saved_pos[3];
    u8  _pad5C[4];
    void* handle;
    s32 active;
} MilestoneParam; /* 0x68 */

typedef struct PlayerStart {
    f32 x;
    f32 y;
    f32 z;
} PlayerStart;

typedef struct ItemRuntime {
    /* 0x0000 */ f32 wobjX[150];
    /* 0x0258 */ f32 wobjNodeY[150];
    /* 0x04B0 */ f32 wobjX2[150];
    /* 0x0708 */ f32 wobjZ[150];
    /* 0x0960 */ f32 wobjValue[150];
    char itemPath[0x20];
    f32 playerStartYaw[14];
    PlayerStart playerStartPositions[14];
    LookoutParam lookoutParams[20];
    TriggerCamera* sumnerCameras[3][14];
    TriggerCamera* runeCameras[3];
    TriggerCamera* startCameras[14];
    TriggerCamera triggerCameras[256];
    MilestoneParam milestones[128];
    u8 _pad7214[0xC];
    /* 0x7220 */ void* wobjTarget[150];
} ItemRuntime;

typedef struct ItemStrings {
    char objectsFile[0x554];
    char file0Format[0xC];
    char file1Format[1];
} ItemStrings;

typedef struct ItemSceneContext {
    u8    _pad00[0x44];
    void* current;
} ItemSceneContext;

extern ItemRuntime    sItemRuntime;
extern f32            sPlayerStartPositions[14][3];
extern LookoutParam   sLookoutParams[];
extern TriggerCamera* sSumnerCameras[3][14];
extern TriggerCamera* sRuneCameras[17];
extern RuneCameraVariants sRuneCameraVariants;
extern char           sMissingLookoutParamFmt[0x1C];
extern TriggerCamera* sCrystalCamera;
extern s32            sNumLookoutParams;
extern TriggerCamera* sWindowCameras[2];
extern s32            sWindowCameraVariant0;
extern s32            sWindowCameraVariant1;
extern char           sWeaponsName[8];
extern char           sPowerupsName[0x28];
extern ItemStrings    sObjectsFile_80112AB8;
extern s32            gBossType;
extern void*          gSceneRoot;
extern void*          sItemsRootNode;
extern f32            gIdentityMatrix[16];
extern f32            sItemZero;
extern f32            sItemFloorRadius;
extern f64            sItemFloorYOffset;
extern f32            sNoDistance;
extern f64            sZeroDouble;
extern s32            sNumItems;
extern s32            gMaxItems;
extern s32            gNextItemIdx;
extern s32            sNumItemWobjs;
extern s32            sVisibleSumCoinCount;
extern s32            sUnusedItemState;
extern s32            sUnusedResetState;
extern s32            sSpecialItem10;
extern s32            sSpecialItem13;
extern s32            sSafeRockCount;
extern s32            sPreviousSafeRockCount;
extern iteminfo*       sDeathItemInfo;
extern void*           sKeyringAtree;
extern void*           sDeathIconAtree;
extern void*           sChestAtree;
extern u32            pbLoad;
extern s32            gNumPlayers;
extern s32            gGameMode;
extern char           sMaxItemsError[];
extern s32            gNumType7Items;
extern s32            gDemoMode;
extern s32            gSumnerReady;
extern s32            sEnemyDefaultAlgorithm[];

extern s32            gGameBusy;
extern s32            gScriptedCameraState;
extern s32            gFrameTicks;
extern s32            default_gen_count;
extern double         ITEM_ACTIVE_DIST;
extern char           sSafeRockBoss41ObjectName[];
extern char           sSafeRockBoss44ObjectName[];

extern void  FatalError(const char* msg, s32 code);
extern void  SetPlayerVars(void);
extern void  fn_80062A00(void);
extern void  LinkItemTriggers(void);
extern void* AtreeInit(void* definition, void* tree, const char* objectPrefix, u32 treeFlags);
extern s32   AnimateATree(void* tree, s32 action, s32 mode);
extern s32   generate_enemy(f32* pos, s32 type, s32 level, f32* dir,
                            s32 spew, Item* generator, s32 important,
                            f32 angle);
extern s32   check_vacancy(s32 enemy_index, f32* position);
extern void  WorldVector(const f32* vector, f32* out, const f32* matrix);
extern void  AddBoss(f32* matrix);
extern s32   EnemyDescType(char* desc);
extern s32   GetEnemyType(s32 type, s32 level);
extern char* EnemyTypePrefix(s32 type);
extern void* CritterTypeLoaded(s32 type, s32 load);
extern s16*  FindWobjWanim(void* wobj);
extern u32   FindWave(const s8* name);
extern s32   towerGetLevelFlag(void* player, s32 flag);
extern s32   towerAllPlayersMetBossReq(s32 flag);
extern s32   AudioFindSound(char* name, s32 length, s32 load);
extern s32   FindWorldAnimNode(f32* point, f32 maxdist);
extern void* AtreeFindMbidxNode(void* tree, s32 mbidx);
extern u32   RandInt(u32 limit);
extern s32   stricmp(const char* a, const char* b);
extern u32   strlen(const char* text);
extern s32   toupper(s32 c);
extern char* strcpy(char* dst, const char* src);
extern char* strncpy(char* dst, const char* src, u32 count);
extern f32   atan2(f32 y, f32 x);
extern void* memset(void* dst, s32 val, u32 n);
extern WorldObj* FindWORLDOBJ(const char* name);
extern double DistanceToClosestPlayer(f32* pos);
extern void  AddItemSub(Item* item);
extern void  AtreeDelete(void* p);
extern void  MBRemoveNode(void* node, s32 flag);
extern s32   MBTreeClearFlags(void* node, s32 a, s32 b);
extern void  MBNodeSetParent(void* node, void* parent);
extern void  UpdateObjWorldMat(OBJGRP* group);
void AddItemWobj(Item* it);
extern s32   RegisterItemWobj(void* target_ptr, s16 type, s32 x_grid,
                              s32 z_grid, s32 value);
extern s32   PlayerSelecting(s32 idx);
extern Player gPlayers[];
extern ItemSceneContext gFloorCollisionResult;
extern s64   gControllerButtons;
extern char  sBadItemFloorPosFmt[];
extern char  sDeathIconName[0xB];
extern char  sKeyringName[8];
extern char  sGoodWizardChestName[8];
extern char  sSeeThroughObjectName[8];
extern s32   WorldOpen(s32 handle);
extern char  sTransporterNoDestFmt[];
extern char  sUnableToAddItemFmt[];
extern char  sTriggerCameraConflictFmt[];
extern s32   sMusicTrackHi;
extern int   strcmp(const char* a, const char* b);
extern f32   sCameraVisibilityRadius;
extern f32   sNoNearbyPlayerDistance;
extern f32   sItemSearchDistance;
extern f64   sNewtonThree;
extern f64   sCameraDistanceLimit;
extern f64   sMilestoneHeightTolerance;
extern f64   sMilestoneDistanceTolerance;
extern s32   MBWorldSphereVisible3(f32* position, f32 radius);
extern void  GetPlayerPos(s32 player, f32* position);
extern f32   fqdist(f32 x, f32 y);

void* AtreeMatchAnyHeader(char* name, s32 alsoWads);

extern s32     sLastPlayerStart;
extern double  sInvalidPlayerStartY;
extern level_data* gCurLevel;

extern f32 sMusicFadeBase;
extern s32     crystal_order[14];
extern f64   __frsqrte(f64 value);
extern void* add_arrow(s32 kind, s32 one, s32 alt, f32* a, f32* b, f32* pos);
extern void  CurTransmitterBlink(s32 idx);                        /* newcam hook */
extern void* AtreeMatch(void* tree, const char* name, s32 flag);
extern f32   FixAngle(f64 ang);                        /* angle wrap */
extern void  CreateYPRMatrix(f32* mtx, f32* angles);          /* mtx from angles */
extern f32   FloorPos(f32 y, f32 r, f32* pos, s32 mode); /* ground probe */
extern void* MBOX_NewObject(const char* name, void* matrix, void* parent, u32 flags);
extern void  MBTreeSetAlpha(void* node, s32 alpha, s32 b);
extern s32   MBOX_ReallyFindObject(char* name, s32 a, s32 b, s32 c);
extern void* MBNewObject(s32 object, f32* matrix, void* parent, u32 flags);
extern void  MBSetObject(void* node, s32 object);
extern char* strcat(char* dst, const char* src);
extern s32   StartFXNoLoop(s32 type, f32* pos);
extern void  CopyMat3(const f32* src, f32* dst);
extern void  CopyMat4(const f32* src, f32* dst);
extern void  MBTreeSetZMod(void* node, f32 value, s32 recurse);
extern void  WPitchMat3(f32* matrix, f32 angle);
extern void  WYawMat3(f32* matrix, f32 angle);
extern void  WRollMat3(f32* matrix, f32 angle);
extern char* sArrowObjectNames[];   /* arrow blit names by kind */
extern const char sLevelOneSuffix[3]; /* "L1" */
extern const char sRootSuffix[5];     /* "ROOT" */
extern char  sItemHealthTextureFmt[5];  /* "%s%d" health-tier fmt */
extern f64   sPi;     /* pi (rounded) */
extern f64   sTwoPi;
extern f64   sNegativePi;
extern f64   sHalfPi;
extern f32   sLogic12Distance;
extern f64   sArrowFloorYOffset;     /* 0.5 */
extern f32   sArrowFloorRadius;
extern s32   sShownMilestones;   /* milestone shown idx */
extern s32   lbl_80344800;       /* free-running frame counter (gamemain.c) */
extern s32   sShownCameras;   /* cameras shown idx */
extern s32   sNumMilestones;   /* milestone count */
extern s32   sNumTriggerCameras;   /* camera count */
extern u8    sMilestones[]; /* milestone table (stride 0x68: pos@0, handle@0x60) */
extern u8    sTriggerCameras[]; /* camera table (stride 0x28: type@0, a@4, b@0x14, handle@0x24) */
extern s32   gNumTransmitters;
extern s32   sLastTransmitter;
extern TriggerCamera* sSpecialTransmitter;
extern f32   sInvalidPlayerStartYFloat;
extern void* gWadAtreeHeaders[45]; /* wad atree headers */
extern f32     gDefaultPlayerPosition[3];
extern f32     gPlayerStartYaw;
extern TriggerCamera* CurTransmitter;
extern char    sNewItemBadIndex[];
extern char    sSetItemFailedFmt[];

s32 ItemVisible(Item* item);
void SetItemGeo(Item* item, void* atree_header, char* name, u32 flags);

/* ------------------------------------------------------------------ */
/* item pool                                                          */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* ITEMS.OBJ head, 0x8005ACE0..0x800631AC (previously carried by gauntworld.c). */
/* ------------------------------------------------------------------ */

#include "game/critter.h"
#include "game/effect.h"
#include "game/mbnode.h"
#include "game/newcam.h"
#include "game/mbobject.h"

/* Item.data's per-info->type variants (powerupdata / containerdata /
 * gendata / enemydata / triggerdata / trapdata / exitdata / obsticledata /
 * transdata / rotdata / sounddata, Xbox misc.h union Id=3251) now live in
 * include/game/item.h as the real `itemdata` union, with each member typed
 * only where a GameCube access proves its width and signedness.  Accesses
 * this TU has not yet converted go through the union's `raw` byte view. */

extern int   FileExists(const char* dev, const char* path);
/* fn_80057F44 (world-registry hook) is defined in this TU below; its
 * prototype sits with the 0x80055CB8 block's declarations. */
extern int   sprintf(char* buf, const char* fmt, ...);
extern int   ErrorPrintf(const char* fmt, ...);
extern int   strcmp(const char* lhs, const char* rhs);
extern int   GetWorldMat(void* node, f32* matrix, f32* offset);
extern int   msgPost(s32 code, s32 owner, char* data);
extern void  DoTexMods(void* data);
extern void  DoSpecialTexmods(void);
extern void  SetupPlayerTexMods(s32 player);
extern void  fn_800606FC(void);
extern s32   fn_8005D0C4(s32 id, f32* position);
extern f32   NormalVector(f32* vector);
extern s32   lbl_8034481C;
extern s32   gGameplayPauseTimer;
extern void  AtreeDelete(void* atree);
extern void  MBTreeSetFlags(void* node, s32 flags, s32 value);
extern s32   WorldOpen(s32 world);
extern s32   PlayerHasShard(s32 player, s32 shard);

extern s32   lbl_803448A0;
extern s32   lbl_803448A4;

extern s32   toupper(s32 c);
extern void  LoadItems(void);
extern void  InitItems(void);

extern void  LoadWeapons(void);
extern s32   lbl_80344768;
extern s32   lbl_803447B8;
extern char* lbl_8011B578[];       /* category name-string table */
extern void  AudioGeneratorDies(f32* pos, s32 gen);
extern void  add_got_it(s32 player, s32 subtype, s32 count);
extern s32   damage_player(s32 i, f32 dmg, s32 mode, u32 flags,
                           f32* direction);
extern void  TowerNeedGargItemsMsg(s32 who, s32 slot);
extern u8*   CritterNewInst(s32 type, s32 sub, void* mat);
extern s32   did_generate(void* w, s32 a);

u32   FindWave(const s8* s);

extern f32  lbl_80344880;
extern s32  lbl_8034488C;
extern char* strcpy(char* dst, const char* src);
extern f32  Random(f32 range);
extern void AddItemInstList(void);
extern void AddLocatorInstList(void);
extern void fn_8005D04C(void);
extern void SetupWeaponPowerupTexMods(void);
extern void SetupItemTexMods(void);
extern void InitItemInfoData(void);

extern s32  lbl_803447B4;
extern Effect Effects[]; /* live fx pool, stride 0xF0 (game/effect.h) */


/* gWorldInfo: game/worldinfo.h (WorldInfo, 0x8028CA8C, size 0xA4) */
extern f64 __fabs(f64 value);
extern void get_screen_pos(s32 camera, s32* x, s32* y, void* position);
extern void DrawText(s32 x, s32 y, s32 font, u32 color,
                     const char* format, ...);

void fn_8005AF98(u8* record, s32* typeOut, s32* valueOut, s32* fieldOut,
                 s32* stateOut, char** nameOut);
f32 fn_8005B198(f32 radius, f32* position, Item** result);

static inline u8* world_record_at(u8** records, s16 index)
{
    return *records + index * 80;
}

void fn_8005ACE0(f32* position)
{
    char** names = sArrowObjectNames;
    char* runtime = (char*)&sItemRuntime;
    Item* item;
    s32 type;
    s32 value;
    s32 field;
    s32 state;
    s32 x;
    s32 y;
    char* name;
    s32 screenX;
    s32 screenY;
    s32 displayType;

    fn_8005B198(20.0f, position, &item);
    if (item == 0) {
        return;
    }

    fn_8005AF98((u8*)item->info, &type, &value, &field, &state, &name);
    get_screen_pos(0, &x, &y, &item->objgrp.worldmat[3][0]);

    if (type == 2 && *(s16*)&item->data.raw[0] >= 0) {
        fn_8005AF98((u8*)gWorldInfo.iteminfo +
                        *(s16*)&item->data.raw[0] * 80,
                    &type, &value, &field, &state, &name);
    }

    screenX = x;
    if (screenX <= 32 || screenX >= 480 ||
        ((screenY = y), screenY < 0) || screenY >= 384) {
        return;
    }

    displayType = type;
    switch (displayType) {
    case 1:
        if ((u32)value >= 17) {
            return;
        }
        // lint-allow-next-line FM007: 0xFFFFFF is a packed RGB colour passed to the parameter this call's own prototype declares as `u32 rgb` - opaque white. A colour code is final-form source: there is nothing behind it to recover, and it is the only numeric literal on this statement.
        DrawText(-screenX, screenY, 0, 0xFFFFFF, "%s:%s:%d(%d)",
                 names[displayType + 43], names[value + 57], field,
                 item->minplayers);
        break;

    case 5:
    case 12:
        if (*(u32*)&item->data.raw[0] == 0) {
            return;
        }
        if (displayType == 12) {
            sprintf(runtime + 3000, "ROT:");
        } else if (value == 20 || value == 22) {
            sprintf(runtime + 3000, "BRID:");
        } else if (value == 21 || value == 23) {
            sprintf(runtime + 3000, "DOOR:");
        } else if ((u32)(value - 25) <= 1) {
            sprintf(runtime + 3000, "ELEV:");
        } else if (value >= 27 && value <= 29) {
            sprintf(runtime + 3000, "LIFT:");
        } else {
            sprintf(runtime + 3000, "TRIG:");
        }
        DrawText(-x, y, 0, 0xFFFFFF, "%s:%s(%d)",
                 runtime + 3000,
                 *(char**)&item->data.raw[0], item->minplayers);
        break;

    default:
        // lint-allow-next-line FM007: 0xFFFFFF is a packed RGB colour passed to the parameter this call's own prototype declares as `u32 rgb` - opaque white. A colour code is final-form source: there is nothing behind it to recover, and it is the only numeric literal on this statement.
        DrawText(-screenX, screenY, 0, 0xFFFFFF, "%s(%d)",
                 names[displayType + 43], item->minplayers);
        break;
    }
}

void fn_8005AF98(u8* record, s32* typeOut, s32* valueOut, s32* fieldOut,
                 s32* stateOut, char** nameOut)
{
    typedef struct WorldRecordView {
        s32 type;
        s32 valueOrCount;
        s16 links[16];
        char name[20];
        s32 state;
        s16 field;
    } WorldRecordView;
    WorldRecordView* view = (WorldRecordView*)record;
    u8 unusedHigh[8];
    s32 type;
    s32 value;
    s32 field;
    s32 state;
    s32 nextType;
    s32 nextValue;
    s32 nextField;
    s32 nextState;
    char* name;
    char* nextName;
    u8** worldRecords;
    s32 count;
    s32 i;
    u8 unusedLow[8];

    if (view->type == -1) {
        worldRecords = (u8**)&gWorldInfo.iteminfo;
        count = view->valueOrCount;
        fn_8005AF98(world_record_at(worldRecords, view->links[0]),
                    &type, &value, &field, &state, &name);
        for (i = 1; i < count; i++) {
            fn_8005AF98(*worldRecords +
                            view->links[i] * 80,
                        &nextType, &nextValue, &nextField, &nextState,
                        &nextName);
            if (nextType != type) {
                type = 0;
            }
            if (nextValue != value) {
                value = 0;
            }
            if (nextField != field) {
                field = 0;
            }
            if (nextState != state) {
                state = 0;
            }
            if (name != NULL && nextName != NULL) {
                if (strcmp(name, nextName) != 0) {
                    name = NULL;
                }
            } else {
                nextName = NULL;
                name = NULL;
            }
        }
        *typeOut = type;
        *valueOut = value;
        *fieldOut = field;
        *stateOut = state;
        *nameOut = name;
    } else {
        *typeOut = view->type;
        *valueOut = view->valueOrCount;
        *fieldOut = view->field;
        if (view->type != 1 || view->valueOrCount != 3) {
            *fieldOut = (s32)__fabs((f64)*fieldOut);
        }
        *stateOut = view->state;
        *nameOut = view->name;
    }
}

f32 fn_8005B198(f32 radius, f32* position, Item** result)
{
    f32 delta[3];
    f32 best_distance = -1.0f;
    Item* item;
    Item* best_item = 0;
    s32 index;

    StartEnemyGrid(position, radius);
    while ((index = NextGridEnemy()) >= 0) {
        item = &sItems[index];

        if (item->active != -1 && item->minoff == 0) {
            f32 distance;

            delta[0] = item->objgrp.coll_pos[0] - position[0];
            delta[1] = item->objgrp.coll_pos[1] - position[1];
            delta[2] = item->objgrp.coll_pos[2] - position[2];
            distance = NormalVector(delta);
            if (best_distance < 0.0f || distance < best_distance) {
                best_item = item;
                best_distance = distance;
            }
        }
    }

    *result = best_item;
    return best_distance;
}

extern f64 lbl_80346EE8;
extern f64 lbl_80346EF0;
extern f64 lbl_80346EF8;
extern f64 lbl_80346F00;
extern f64 lbl_80346F08;
extern f32 fqdist(f32 x, f32 y);

/* Find the closest eligible world item in front of a directed probe. */
f32 fn_8005B274(f32* position, f32 bias, f32 radius, f32* direction,
                f32* resultPosition, Item** resultItem)
{
    f32 delta[3];
    u8 unused[4];
    f32 dot;
    f32 weighted;
    f32 absY;
    f64 maxInset;
    f64 normalScale;
    f64 subtypeScale;
    f64 heightScale;
    f32 bestX;
    f32 bestY;
    f32 bestZ;
    f32 distance;
    f32 best;
    f32 scale;
    iteminfodata* sub;
    s32 index;
    s32 bestIndex;
    s32 type;
    s16 active;
    Item* item;
    iteminfo* info;

    scale = (f32)((lbl_80346EE8 - bias) / radius);
    best = radius;
    bestIndex = -1;
    StartEnemyGrid(position, radius);
    subtypeScale = lbl_80346EF8;
    normalScale = lbl_80346F00;
    maxInset = lbl_80346F08;
    heightScale = lbl_80346EF0;

    while ((index = NextGridEnemy()) >= 0) {
        item = &sItems[index];
        active = item->active;
        if (active == -1 || (active & 0x8100) != 0 ||
            item->armor == -1) {
            continue;
        }
        info = item->info;
        type = info->type;
        if (type == -1) {
            continue;
        }
        if (item->minoff != 0) {
            continue;
        }
        sub = &info->item;
        if ((active & 0x4000) == 0) {
            continue;
        }
        switch (type) {
        case 2:
        case 10:
            if (type == 2 && sub->subtype != 43 && sub->subtype != 44 &&
                sub->subtype != 45) {
                continue;
            }
            if (type == 10 && sub->subtype == 41) {
                continue;
            }
            if ((sub->subtype == 43 || sub->subtype == 44 ||
                 sub->subtype == 45) && item->action > 0) {
                continue;
            }
            goto check_type5;
        case 5:
check_type5:
            if (type == 5 && sub->subtype != 31) {
                continue;
            }
            break;
        case 3:
            break;
        default:
            continue;
        }

        delta[0] = item->objgrp.coll_pos[0] - position[0];
        delta[1] = item->objgrp.coll_pos[1] - position[1];
        delta[2] = item->objgrp.coll_pos[2] - position[2];
        absY = delta[1];
        *(u32*)&absY &= 0x7FFFFFFF;
        if (absY > heightScale * sub->height) {
            continue;
        }
        distance = NormalVector(delta);
        if (item->info->type != 3) {
            if (sub->subtype == 44) {
                distance *= subtypeScale;
            } else if (sub->subtype == 45) {
                distance *= subtypeScale;
            } else {
                distance *= normalScale;
            }
        }
        distance -=
            (((f64)sub->radius < maxInset) ? (f64)sub->radius : maxInset);
        if (distance > radius) {
            continue;
        }
        {
            f32 dotPart;
            f32 weightedBase;
            f32 q;

            q = fqdist(delta[0], delta[2]);
            weightedBase = distance * scale + bias;
            dotPart = delta[2] * direction[2];
            weighted = q * weightedBase;
            dot = delta[0] * direction[0] + dotPart;
            if (dot < weighted) {
                continue;
            }
        }
        if (distance < best) {
            best = distance;
            bestIndex = index;
            bestX = delta[0];
            bestY = delta[1];
            bestZ = delta[2];
        }
    }

    if (bestIndex >= 0) {
        resultPosition[0] = bestX;
        resultPosition[1] = bestY;
        resultPosition[2] = bestZ;
        if (resultItem != 0) {
            *resultItem = &sItems[bestIndex];
        }
    } else if (resultItem != 0) {
        *resultItem = 0;
    }
    return best;
}

/* Item-pool queries used by the world dispatcher and camera/UI code. */
Item* fn_8005B558(s32 id)
{
    s32 i;
    s32 count = sNumItems;

    for (i = 0; i < count; i++) {
        Item* item = &sItems[i];
        if (item->active != -1 && item->info->type == 9 &&
            id == *(s16*)&item->data.raw[0]) {
            return item;
        }
    }
    return 0;
}

void fn_8005B5B8(void)
{
    u32 level_flags[14];
    char name[32];
    f32 matrix[16];
    char* strings = (char*)&sObjectsFile_80112AB8;
    s32 player;
    s32 level;
    Item* item;
    s32 gate;
    s32 item_index;
    s32 world;
    s32 enable;
    s32 id;

    for (level = 0; level < 14; level++) {
        level_flags[level] = 0;
    }

    for (player = 0; player < 4; player++) {
        Player* record = &gPlayers[player];

        if (record->state != 0) {
            for (level = 0; level < 14; level++) {
                level_flags[level] |= towerGetLevelFlag((u8*)record, level);
            }
        }
    }

    for (item_index = 0; item_index < sNumItems; item_index++) {
        item = &sItems[item_index];
        enable = 0;

        if (item->active != -1 && item->info->type == 9) {
            id = *(s16*)&item->data.raw[0];
            world = id >> 8;
            gate = id & 0xFF;

            if (gDemoMode != 0) {
                enable = 1;
                switch (world) {
                case 7:
                    if (gate == 0) {
                        enable = 0;
                    }
                    break;
                case 11:
                    if (gate == 3) {
                        enable = 0;
                    }
                    break;
                case 10:
                    if (gate == 5) {
                        enable = 0;
                    }
                    break;
                }
            } else {
                switch (world) {
                default:
                    if (gate - 1 >= 0 &&
                        ((1 << (gate - 1)) & level_flags[world]) == 0) {
                        enable = 1;
                    }
                    break;
                case 5:
                case 6:
                    if (WorldOpen(world) == 0) {
                        enable = 1;
                    }
                    break;
                case 8:
                    if (WorldOpen(world) == 0) {
                        enable = 1;
                    } else if (gate == 3) {
                        if (PlayerHasShard(-1, 0x1FFF) == 0) {
                            enable = 1;
                        }
                    } else if (gate - 1 >= 0 &&
                               ((1 << (gate - 1)) & level_flags[world]) == 0) {
                        enable = 1;
                    }
                    break;
                }
            }

            if (enable != 0) {
                WorldObj* world_object;
                void* parent = ((MBObject*)item->objgrp.node)->parent;

                CopyMat4((f32*)item->objgrp.node, matrix);
                if (*(void**)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                    *(s32*)&item->atree[0] = 0;
                }
                if (item->objgrp.node != 0) {
                    MBRemoveNode(item->objgrp.node, 0);
                    item->objgrp.node = 0;
                }
                item->objgrp.node = MBOX_NewObject(
                    &strings[0x100], matrix, parent, 0);
                item->active = -0x8000;
                sprintf(name, &strings[0x10C], world + 0x40, gate + 1);
                world_object = FindWORLDOBJ(name);
                if (world_object != 0) {
                    MBTreeSetFlags(world_object->nodeptr, 2, 0);
                } else {
                    ErrorPrintf(&strings[0x120], name);
                }
            }
        }
    }
}

s32 fn_8005B8B0(void* owner)
{
    Item* item = (Item*)((Player*)owner)->special_collision_item;
    s32 result = 0;

    if (item != 0 && item->info->type == 0xB) {
        Item* linked = *(Item**)&item->data.raw[8];
        s16 active = linked->active;

        result = (s32)linked;
        if (active == -1 || linked->minoff != 0 || (active & 0x4000) == 0) {
            result = 0;
        }
    }
    return result;
}

s32 fn_8005B8FC(void* owner)
{
    s32 result = 0;
    Item* item;

    if (lbl_8034481C != 0) {
        result = -1;
    }

    item = (Item*)((Player*)owner)->special_collision_item;
    if (item != 0 && item->info->type == 9 &&
        item->info->item.subtype != 0x32) {
        result = *(s16*)&item->data.raw[0];
    }

    if ((gGameMode & MODE_GROUP_ATTRACT) != 0 && result != 0) {
        msgPost(0x61, *(s32*)owner, (char*)owner + 0x54);
    }
    return result;
}

void fn_8005B988(void)
{
    s32 i;

    if ((gGameBusy | gGameplayPauseTimer) == 0) {
        fn_800606FC();
        if (sGoodWizObj != 0) {
            DoTexMods(sGoodWizObj);
        }
        if (sItemFile1Buf != 0) {
            DoTexMods(sItemFile1Buf);
        }
        if (sWeaponsBuf != 0) {
            DoTexMods(sWeaponsBuf);
        }
        if (sPowerupsBuf != 0) {
            DoTexMods(sPowerupsBuf);
        }
        for (i = 0; i < 4; i++) {
            SetupPlayerTexMods(i);
        }
        DoSpecialTexmods();
    }
}

void fn_8005D04C(void)
{
    s32 i;
    Item* item;

    for (i = 0; i < sNumItems; i++) {
        item = &sItems[i];

        if (item->info != 0 && item->info->type == 4) {
            *(s16*)&item->data.raw[0x12] =
                fn_8005D0C4(*(s16*)&item->data.raw[0],
                             &item->objgrp.worldmat[3][0]);
        }
    }
}

/* forward decls for later-defined callees */
extern f32 fqdist(f32 x, f32 y);
extern void MBTreeSetFlags(void* node, s32 flags, s32 value);
extern int fn_8005EE18(Item* item, s32 arg);
extern f32 fn_8005F0F4(Item* item, f32* from, f32* pos, f32* out,
                       f32 radius, f32 height);
extern void fn_8009D91C(f32* pos);

s32 fn_8005D0C4(s32 id, f32* position)
{
    f32 dy;
    f32 best = sCameraVisibilityRadius;
    s32 best_idx = -1;
    s32 idx;
    u8 unused[16];
    struct {
        u8 pad[8];
        f32 value;
    } absWork;

    if (id != 33 && id != 32 && id != 29) {
        return -1;
    }

    StartEnemyGrid(position, sNoDistance);
    while ((idx = NextGridEnemy()) >= 0) {
        Item* item = &sItems[idx];
        f32 d;

        if (item->active == -1) {
            continue;
        }
        if (item->active & 0x8100) {
            continue;
        }
        if (item->minoff != 0) {
            continue;
        }
        if (item->info->type != 1) {
            continue;
        }

        {
            f32 dx = item->objgrp.worldmat[3][0] - position[0];
            f32 dz = item->objgrp.worldmat[3][2] - position[2];
            dy = item->objgrp.worldmat[3][1] - position[1];
            d = fqdist(dx, dz);
        }
        if (d < best) {
            absWork.value = dy;
            *(u32*)&absWork.value &= 0x7FFFFFFF;
            if (absWork.value < 3.0f) {
                best = d;
                best_idx = idx;
            }
        }
    }

    if (best_idx >= 0) {
        sItems[best_idx].minoff = 10;
        MBTreeSetFlags(sItems[best_idx].objgrp.node, 2, 0);
    }
    return best_idx;
}

extern void MBNodeSetParent(void* node, void* parent);
extern Item* NewItemPtr(void);
extern void AddItemSub(Item* item);
extern void fn_8009DAF8(void);

/* 0x8005E90C - resolve an item's world-record binding: reroll random slots,
 * spawn the linked pickup/chest contents, seed timers and child algorithm. */
/* Index of the keyring row in the level's item table, or -1 when the level has
 * no keyring. */
static inline s32 find_keyring_row(iteminfo* q, u8* world)
{
    s32 i;

    for (i = 0; i < *(s32*)(world + 116); i++, q++) {
        iteminfodata* qdata = &q->item;
        if (strcmp(sKeyringName, qdata->desc) != 0) {
            continue;
        }
        if (q->type != 1) {
            continue;
        }
        if (qdata->subtype != 2) {
            continue;
        }
        return i;
    }
    return -1;
}

void fn_8005E90C(Item* item, s32* inst)
{
    iteminfo** tblp;
    iteminfo* info;
    iteminfo* row;
    Item* child;
    Item* result;
    s32 idx;
    u32 fl;
    s32 t;
    u8 unused[24];

    idx = *(s16*)&item->data.raw[0];
    info = item->info;
    if ((s16)idx >= 0) {
        tblp = (iteminfo**)&gWorldInfo.iteminfo;
        row = *tblp + idx;
    } else {
        return;
    }
    while (row->type == -1) {
        s32 n = row->item.subtype;
        s32 r;
        t = item - sItems;
        if (n != 0) {
            r = ((sItemRandSeed >> 5) + t) % (u32)n;
        } else {
            r = 0;
        }
        sItemRandSeed = sItemRandSeed + 439;
        *(s16*)&item->data.raw[0] = *(s16*)((u8*)row + r * 2 + 8);
        row = *tblp + *(s16*)&item->data.raw[0];
    }
    if (info->item.subtype == 48 && row->type == 1 && row->item.subtype == 1) {
        fl = 0;
        if (sChestAtree != NULL) {
            if (*(u32*)&item->atree[0] != 0) {
                AtreeDelete(item->atree);
            }
            fl |= 0x800;
            fl |= info->item.mbflags;
            ((atree*)item->atree)->root = AtreeInit(sChestAtree, item->atree, 0, fl);
            MBNodeSetParent(*(void**)*(void**)&item->atree[0], item->objgrp.node);
        }
        item->info = (iteminfo*)sDeathItemInfo;
        *(s32*)&item->data.raw[4] = row->item.value;
        return;
    }
    if (info->item.subtype == 47) {
        *(s32*)&item->data.raw[4] = row->item.value;
        return;
    }
    if (info->item.subtype == 44) {
        item->active |= 64;
        fn_8009DAF8();
        return;
    }
    if (row->type == 1) {
        switch (row->item.subtype) {
        case 2:
            if (*(s16*)&item->data.raw[16] > 1) {
                u8* world = (u8*)&gWorldInfo;
                row = *tblp + find_keyring_row(*tblp, world);
            }
            break;
        }
    }
    if (row->type == 1 && *(u32*)&item->data.raw[8] != 0) {
        Item* newItem;
        f32 radius;

        newItem = NewItemPtr();
        SetItem(newItem, NULL, row, gIdentityMatrix);
        result = newItem;
        MBNodeSetParent(result->objgrp.node, *(void**)&item->data.raw[8]);
        *(Item**)&item->data.raw[12] = newItem;
        *(Item**)&newItem->data.raw[12] = item;
        newItem->opener = (s8)(inst != NULL ? *inst : -1);
        MBTreeSetFlags(result->objgrp.node, 8, 0);
        radius = sArrowFloorRadius;
        *(f32*)((u8*)newItem->objgrp.node + 64) = radius;
        *(f32*)((u8*)newItem->objgrp.node + 68) = radius;
        *(f32*)((u8*)newItem->objgrp.node + 72) = radius;
    } else {
        s32 subtype;

        child = NewItemPtr();
        if (&item->objgrp != NULL) {
            SetItem(child, NULL, row, &item->objgrp.worldmat[0][0]);
            AddItemSub(child);
        } else {
            SetItem(child, NULL, row, gIdentityMatrix);
        }
        subtype = info->item.subtype;
        result = child;
        if (subtype == 43) {
            child->opener = (s8)(inst != NULL ? *inst : -2);
        } else {
            if (row->type == 1) {
                *(Item**)&child->data.raw[12] = item;
            }
            child->opener = (s8)(inst != NULL ? *inst : -1);
        }
    }
    switch (row->type) {
    case 1:
        switch (row->item.subtype) {
        case 14:
            *(s32*)&result->data.raw[4] = *(s16*)&item->data.raw[16];
            break;
        case 2:
            *(s32*)&result->data.raw[4] = *(s16*)&item->data.raw[16];
            if (*(s32*)&result->data.raw[4] < 1) {
                *(s32*)&result->data.raw[4] = 1;
            }
            break;
        }
        *(s16*)&result->data.raw[16] = 30;
        break;
    case 4:
        *(u32*)&result->data.raw[8] |= 1;
        if (*(s16*)&result->data.raw[0] == 30 && *(u32*)((u8*)gWadAtreeHeaders + 120) != 0) {
            MBTreeSetFlags(*(void**)*(void**)&result->atree[0], 2, 0);
            if (*(s16*)&item->data.raw[16] != 0) {
                result->data.raw[2] = 2;
                *(s8*)&result->data.raw[3] = (s8)sEnemyDefaultAlgorithm[30];
            }
        }
        *(s16*)&result->data.raw[18] = -1;
        break;
    }
}

Item* fn_8005ED44(f32 radius, s32 a2, f32* position, s32 a4, s32 a5, s32 a6)
{
    Item* item;
    f32 best_dist = 100000.0f;
    s32 idx;
    Item* best = 0;

    StartEnemyGrid(position, radius);
    while ((idx = NextGridEnemy()) >= 0) {
        item = &sItems[idx];
        if (fn_8005EE18(item, a6) != 0) {
            f32 d = fn_8005F0F4(item, (f32*)a2, position, (f32*)a4,
                                radius, radius);
            if (d >= 0.0 && d < best_dist) {
                best_dist = d;
                best = item;
                if (a5 == 0) {
                    break;
                }
            }
        }
    }
    return best;
}

int fn_8005EE18(Item* item, s32 arg)
{
    int result = 0;
    iteminfo* info = item->info;
    s32* sub = (s32*)info + 1;
    u8 _pad[8];

    switch (info->type) {
    case -1:
        break;
    case 1:
        if (item->activetime > 0) {
            result = 0;
        } else if (*(u32*)&item->data.raw[0xC] != 0) {
            result = 0;
        } else {
            switch (*sub) {
            case 4:
                result = 1;
            }
        }
        break;
    case 2:
    case 7:
        result = 1;
        break;
    case 3: {
        s32 t;
        if (t = ((s8)item->data.raw[6] ? 0 : 1)) {
            break;
        }
        result = 1;
        break;
    }
    case 4:
        result = 1;
        break;
    case 10:
        switch (*sub) {
        case 0x1E:
            break;
        case 0x29:
            if (*(s16*)&item->data.raw[2] > 0) {
                result = 1;
            }
            break;
        case 0x34:
            if (arg >= 0 && (item->active & 1) == 0) {
                fn_8009D91C(&item->objgrp.worldmat[3][0]);
                item->active |= 1;
            }
            result = 0;
            break;
        case 0x28:
        case 0x31:
        case 0x33:
        case 0x35:
            result = 0;
            break;
        default:
            result = 1;
            break;
        }
        break;
    case 8:
        if (*sub == 5 &&
            (item->action == 1 || item->action == 2)) {
            item->daction = 3;
            result = 1;
        }
        break;
    case 5:
        if (*sub == 0x1F) {
            result = 1;
        }
        break;
    }
    return result;
}

Item* fn_8005EFAC(f32 radius, s32 a2, f32* position, s32 a4, s32 a5)
{
    Item* item;
    f32 best_dist = 100000.0f;
    f32 scaled = radius * 1.5;
    s32 idx;
    Item* best = 0;

    StartEnemyGrid(position, radius);
    while ((idx = NextGridEnemy()) >= 0) {
        s32 reject;
        item = &sItems[idx];
        reject = 0;
        switch (item->info->type) {
        case 13:
            reject = 1;
            break;
        case 1:
            if (*(u32*)&item->data.raw[0xC] != 0) {
                reject = 1;
            }
            break;
        case 8:
            if ((item->action == 2 || item->action == 4) &&
                (item->active & 1)) {
            } else {
                reject = 1;
            }
            break;
        }
        if (reject != 0) {
            continue;
        }
        {
            f32 d = fn_8005F0F4(item, (f32*)a2, position, (f32*)a4,
                                radius, scaled);
            if (d >= 0.0 && d < best_dist) {
                best_dist = d;
                best = item;
                if (a5 == 0) {
                    break;
                }
            }
        }
    }
    return best;
}

Item* fn_80062FF0(f32 radius, f32* position, s32 type, f32* out1, f32* out2)
{
    u8 unused[40];
    Item* item;
    f32 min_flagged = 100000.0f;
    f32 min_all = 100000.0f;
    Item* best = 0;
    s32 idx;

    StartEnemyGrid(position, radius);
    while ((idx = NextGridEnemy()) >= 0) {
        iteminfo* info;
        s32 reject;
        s32 item_type;
        f32 d;

        item = &sItems[idx];
        if (item->active == -1) {
            continue;
        }
        if (item->active & 0x8100) {
            continue;
        }
        info = item->info;
        if (info->item.coltype == 0) {
            continue;
        }
        if (type != 0) {
            if (info->type != type) {
                continue;
            }
            if (type == 4 && (*(u32*)&item->data.raw[8] & 1)) {
                continue;
            }
        }
        item_type = info->type;
        if (item_type == -1) {
            continue;
        }
        if (item->minoff != 0) {
            continue;
        }
        reject = 0;
        switch (item_type) {
        case 13:
            reject = 1;
            break;
        case 1:
            if (*(u32*)&item->data.raw[0xC] != 0) {
                reject = 1;
            }
            break;
        case 8:
            if ((item->action == 2 || item->action == 4) &&
                (item->active & 1)) {
            } else {
                reject = 1;
            }
            break;
        }
        if (reject != 0) {
            continue;
        }
        d = fqdist(item->objgrp.coll_pos[0] - position[0],
                   item->objgrp.coll_pos[2] - position[2]);
        d = d - item->info->item.radius;
        if (d < min_all) {
            min_all = d;
        }
        if ((item->active & 0x40) || (item->active & 0x4000)) {
            if (d < min_flagged) {
                min_flagged = d;
                best = item;
            }
        }
    }

    if (out1 != 0) {
        *out1 = min_flagged;
    }
    if (out2 != 0) {
        *out2 = min_all;
    }
    return best;
}

s32 fn_800629B0(void)
{
    s32 result = 0;
    Item* item = sItems;
    s32 count = sNumItems;
    s32 i;

    for (i = 0; i < count; i++) {
        if (item->active != -1 && item->info->type == 1 &&
            item->info->item.subtype == 1) {
            result = 1;
        }
        item++;
    }
    return result;
}

extern void  AtreeDelete(void* atree);
extern void  MBNodeSetParent(void* node, void* parent);
extern void  MBRemoveNode(void* node, s32 mode);
/* gWorldInfo: game/worldinfo.h (WorldInfo, 0x8028CA8C, size 0xA4) */
extern const char lbl_80346F10[8];     /* "CHICKEN"  */
extern const char lbl_80346F18[6];     /* "APPLE"    */
extern const char lbl_80346F20[8];   /* sdata2 string, size 0x8     */
extern const char lbl_80346F28[8];   /* sdata2 string, size 0x8     */
extern const char lbl_80346F34[5];   /* "%s_D", sdata2 size 0x5     */
extern char  lbl_802583A8[];     /* scratch name buffer          */

f32 fn_8005C1DC(Item* item, f32 power, s32 flags, s32 owner);

/* 0x8005BA1C - apply a gold/silver-wizard reward to one item (swap the item's
 * atree to a treasure/food model, retarget generators, pop doors/walls). */
void fn_8005BA1C(Item* item, u8* player)
{
    char* objects = (char*)&sObjectsFile_80112AB8;
    s32 evt = -1;                                 /* r25: fx event         */
    s32 msg = -1;                                 /* r24: message code     */
    iteminfo* info = item->info;
    s32* sub = (s32*)((u8*)info + 4);
    s32 rank = ((Player*)player)->level;          /* character level 1..99 */
    s32 mode = (u32)((Player*)player)->char_type & 3;
    void* hdr;
    s32 k;
    WorldInfo* world;
    iteminfo** records;
    iteminfo* rec;
    s32* rsub;
    u8 unused[32];

    (void)unused;

    switch (info->type) {
    case 1:
        switch (*sub) {
        case 2:
            break;
        case 3:
            if (mode != 2) {
                break;
            }
            if (*(s32*)&item->data.raw[4] <= -100) {
                if (rank < 0x32) {
                    break;
                }
                hdr = AtreeMatch(sPowerupsBuf, lbl_80346F10, 1);
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                }
                ((atree*)item->atree)->root = AtreeInit(hdr, item->atree, 0, 0x800);
                MBNodeSetParent(**(void***)&item->atree[0], item->objgrp.node);
                *(s32*)&item->data.raw[4] = 100;
                evt = 0x2F;
                msg = 0x90;
            } else if (*(s32*)&item->data.raw[4] < 0) {
                if (rank < 0x19) {
                    break;
                }
                hdr = AtreeMatch(sPowerupsBuf, lbl_80346F18, 1);
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                }
                ((atree*)item->atree)->root = AtreeInit(hdr, item->atree, 0, 0x800);
                MBNodeSetParent(**(void***)&item->atree[0], item->objgrp.node);
                *(s32*)&item->data.raw[4] = 50;
                evt = 0x2F;
                msg = 0x8F;
            }
            break;
        case 1:
            if (mode != 0) {
                break;
            }
            if (*(s32*)&item->data.raw[4] > 10) {
                break;
            }
            if (rank >= 0x32) {
                hdr = AtreeMatch(sPowerupsBuf, &objects[0x130], 1);
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                }
                ((atree*)item->atree)->root = AtreeInit(hdr, item->atree, 0, 0x800);
                MBNodeSetParent(**(void***)&item->atree[0], item->objgrp.node);
                *(s32*)&item->data.raw[4] = 200;
                evt = 0x30;
                msg = 0x8C;
            } else if (rank >= 0x19) {
                hdr = AtreeMatch(sPowerupsBuf, &objects[0x13C], 1);
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                }
                ((atree*)item->atree)->root = AtreeInit(hdr, item->atree, 0, 0x800);
                MBNodeSetParent(**(void***)&item->atree[0], item->objgrp.node);
                *(s32*)&item->data.raw[4] = 100;
                evt = 0x30;
                msg = 0x8B;
            }
            break;
        }
        break;

    case 2:
        if (*(s16*)&item->data.raw[0] < 0) {
            break;
        }
        world = &gWorldInfo;
        records = &world->iteminfo;
        rec = &(*records)[*(s16*)&item->data.raw[0]];
        if (rec->type != 1) {
            break;
        }
        switch (rec->item.subtype) {
        case 2:
            break;
        case 3:
            if (item->action > 0) {
                break;
            }
            if (mode != 2) {
                break;
            }
            if (rec->item.value <= -100) {
                if (rank < 0x32) {
                    break;
                }
                rec = *records;
                for (k = 0; k < world->niteminfos; k++, rec++) {
                    rsub = &rec->item.subtype;
                    if (strcmp(lbl_80346F10, (char*)rsub + 0x24) == 0 &&
                        rec->type == 1 && *rsub == 3) {
                        goto found_chicken;
                    }
                }
                k = -1;
found_chicken:
                *(s16*)&item->data.raw[0] = (s16)k;
                evt = 0x2F;
                msg = 0x90;
            } else if (rec->item.value < 0) {
                if (rank < 0x19) {
                    break;
                }
                rec = *records;
                for (k = 0; k < world->niteminfos; k++, rec++) {
                    rsub = &rec->item.subtype;
                    if (strcmp(lbl_80346F18, (char*)rsub + 0x24) == 0 &&
                        rec->type == 1 && *rsub == 3) {
                        goto found_apple;
                    }
                }
                k = -1;
found_apple:
                *(s16*)&item->data.raw[0] = (s16)k;
                evt = 0x2F;
                msg = 0x8F;
            }
            break;
        case 1:
            if (mode != 0) {
                break;
            }
            if (rec->item.value > 10) {
                break;
            }
            if (rank >= 0x32) {
                if (*sub != 0x2B) {
                    hdr = AtreeMatch(sGoodWizObj, lbl_80346F20, 1);
                    if (*(u32*)&item->atree[0] != 0) {
                        AtreeDelete(item->atree);
                    }
                    *(void**)&item->atree[0] =
                        AtreeInit(hdr, item->atree, 0, 0x800);
                    MBNodeSetParent(**(void***)&item->atree[0],
                                    item->objgrp.node);
                }
                *(s16*)&item->data.raw[0x10] = 200;
                if (item->action == 0) {
                    *(s16*)&item->data.raw[0x10] = 200;
                    rec = *records;
                    for (k = 0; k < gWorldInfo.niteminfos; k++, rec++) {
                        rsub = &rec->item.subtype;
                        if (strcmp(&objects[0x130],
                                   (char*)rsub + 0x24) == 0 &&
                            rec->type == 1 && *rsub == 1) {
                            goto found_gold;
                        }
                    }
                    k = -1;
found_gold:
                    *(s16*)&item->data.raw[0] = (s16)k;
                } else {
                    *(s32*)&item->data.raw[4] = 200;
                }
                evt = 0x30;
                msg = 0x8C;
            } else if (rank >= 0x19) {
                if (*sub != 0x2B) {
                    hdr = AtreeMatch(sGoodWizObj, lbl_80346F28, 1);
                    if (*(u32*)&item->atree[0] != 0) {
                        AtreeDelete(item->atree);
                    }
                    *(void**)&item->atree[0] =
                        AtreeInit(hdr, item->atree, 0, 0x800);
                    MBNodeSetParent(**(void***)&item->atree[0],
                                    item->objgrp.node);
                }
                if (item->action == 0) {
                    *(s16*)&item->data.raw[0x10] = 100;
                    rec = *records;
                    for (k = 0; k < gWorldInfo.niteminfos; k++, rec++) {
                        rsub = &rec->item.subtype;
                        if (strcmp(&objects[0x13C],
                                   (char*)rsub + 0x24) == 0 &&
                            rec->type == 1 && *rsub == 1) {
                            goto found_silver;
                        }
                    }
                    k = -1;
found_silver:
                    *(s16*)&item->data.raw[0] = (s16)k;
                } else {
                    *(s32*)&item->data.raw[4] = 100;
                }
                evt = 0x30;
                msg = 0x8B;
            }
            break;
        }
        break;

    case 10:
        switch (*sub) {
        case 0x29:
        case 0x2B:
            break;
        default:
            if (mode != 3) {
                break;
            }
            if (rank >= 0x32) {
                fn_8005C1DC(item, 9999.0f, 0, *(s32*)player);
                msg = 0x92;
            } else {
                *(s16*)&item->data.raw[4] = 4;
                msg = 0x91;
            }
            break;
        }
        break;

    case 8:
        if (mode != 1) {
            break;
        }
        if (rank >= 0x32) {
            if (item->active == 0) {
                break;
            }
            sprintf(lbl_802583A8, lbl_80346F34, (char*)info + 0x28);
            hdr = AtreeMatchAnyHeader(lbl_802583A8, 0);
            if (hdr != 0) {
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                }
                ((atree*)item->atree)->root = AtreeInit(hdr, item->atree, 0, 0x800);
                MBNodeSetParent(**(void***)&item->atree[0], item->objgrp.node);
                item->action = 0;
                item->daction = 0;
                item->active = 0;
            } else {
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                    *(u32*)&item->atree[0] = 0;
                }
                if (item->objgrp.node != 0) {
                    MBRemoveNode(item->objgrp.node, 0);
                    item->objgrp.node = 0;
                }
                item->active = -1;
                k = item - sItems;
                if (k < gNextItemIdx) {
                    gNextItemIdx = k;
                }
            }
            msg = 0x8E;
            evt = 0x31;
        } else {
            if (item->activetime < 0x21C) {
                evt = 0x31;
            }
            item->action = 0;
            item->daction = 0;
            item->activetime = 0x258;
            AnimateATree(item->atree, item->daction, 3);
            msg = 0x8D;
        }
        break;
    }

    if (evt >= 0) {
        fn_8009190C(&item->objgrp, evt);
    }
    if (msg >= 0) {
        msgPost(msg, *(s32*)player, (char*)(player + 0x44));
    }
}

/* 0x8005F0F4 - collision/visibility probe of one item against the segment
 * from -> pos.  Returns the 2D distance to the item (negative = no hit); when
 * `out` is given it receives the pushed-out target position. */
extern void NormalVector2D(f32* v);
extern s32  towerAllPlayersMetBossReq(s32 level);
extern f32  lbl_80346F6C;
extern f32  fn_8005FDA8(u8* e, f32* a, f32* b, f32* outPos, f32* outNorm,
                        f32 margin);

/* float magnitude via sign-bit clear (inline fabs). */
static f32 wfabsf_(f32 x)
{
    *(u32*)&x &= 0x7FFFFFFF;
    return x;
}

f32 fn_8005F0F4(Item* item, f32* from, f32* pos, f32* out, f32 a, f32 b)
{
    iteminfo* info;
    iteminfodata* data;
    s32* sub;
    s32 coltype;
    s32 keep;
    s32 type;
    f32 R;
    f32 Rsum;
    f32 dist;
    f32 cx, cz;
    f32 unused2[4];
    f32 nv[3];
    f32 mv[3];
    f32 norm[3];
    f32 hitpt[3];
    f32 f1, f2, f3, f4;
    f32 unused[6];

    (void)unused;
    (void)unused2;

    if (item->active == -1) {
        return -1.0f;
    }
    if ((item->active & 0x8100) != 0) {
        return -1.0f;
    }
    info = item->info;
    type = info->type;
    if (type == -1) {
        return -1.0f;
    }
    if (item->minoff != 0) {
        return -1.0f;
    }
    data = &info->item;
    sub = &data->subtype;
    coltype = data->coltype;
    R = data->radius;
    if (coltype == 0) {
        return -1.0f;
    }
    if ((item->active & 0x40) == 0 && (item->active & 0x4000) == 0) {
        return -1.0f;
    }

    keep = 1;
    switch (type) {
    case 7:
        if (item->action >= 2 ||
            (item->action == 1 && item->activetime > 0x1E)) {
            keep = 0;
        }
        break;
    case 9:
        if (*sub != 0x32) {
            R = (f32)(R + ((f64)lbl_80344768 - lbl_80346EE8));
        }
        break;
    case 2:
        switch (*sub) {
        case 0x2B:
            if (item->action == 2) {
                keep = 0;
            }
            break;
        }
        break;
    case 3:
        if (item->armor < 0 && data->height <= sNewtonThree) {
            keep = 0;
        }
        break;
    case 4:
        if (*(f32*)&item->data.raw[0xC] >= 0.0) {
            R = *(f32*)&item->data.raw[0xC];
            coltype = 1;
        } else {
            R = lbl_80346F6C;
            coltype = 1;
        }
        break;
    case 10:
        switch (*sub) {
        case 0x2B:
        case 0x2C:
        case 0x2D:
            if (item->action > 0) {
                keep = 0;
            }
            break;
        }
        break;
    case 13:
        keep = 0;
        break;
    case 5:
        if ((item->active & 0x400) != 0) {
            keep = 0;
            break;
        }
        if (*(f32*)&item->data.raw[0xC] > 0.0) {
            coltype = 1;
            R = *(f32*)&item->data.raw[0xC];
        } else if (*sub == 0x1B) {
            coltype = 1;
            R = (f32)(R * lbl_80346EF0);
        }
        if ((*(s16*)&item->data.raw[4] & 0x200) != 0) {
            keep = 0;
        } else if ((*(s16*)&item->data.raw[4] & 0x40) != 0 &&
                   (s8)item->data.raw[6] < 100 &&
                   towerAllPlayersMetBossReq((s8)item->data.raw[6]) != 0) {
            R = (f32)(R * lbl_80346EF0);
        }
        break;
    }
    if (keep == 0) {
        return -1.0f;
    }

    Rsum = (f32)(a + R);
    cx = item->objgrp.coll_pos[0];
    cz = item->objgrp.coll_pos[2];
    f1 = (f32)(cx - pos[0]);
    f2 = (f32)(cz - pos[2]);
    if (f1 * f1 + f2 * f2 > Rsum * Rsum) {
        return -1.0f;
    }

    nv[0] = (f32)(pos[0] - cx);
    nv[1] = pos[1] - item->objgrp.coll_pos[1];
    nv[2] = (f32)(pos[2] - cz);
    if (coltype != 2) {
        if (wfabsf_(nv[1]) > (f32)(data->height + b)) {
            return -1.0f;
        }
    }
    dist = fqdist(nv[0], nv[2]);
    if (dist > Rsum) {
        return -1.0f;
    }

    switch (coltype) {
    case 1:
        break;
    case 2:
        if (fqdist(dist, nv[1]) > Rsum) {
            keep = 0;
        }
        break;
    case 3:
        /* oriented box footprint */
        if (wfabsf_(nv[0] * item->objgrp.worldmat[0][0] +
                    nv[2] * item->objgrp.worldmat[0][2]) >
            (f32)(data->xdim + a)) {
            keep = 0;
        } else if (wfabsf_(nv[0] * item->objgrp.worldmat[2][0] +
                           nv[2] * item->objgrp.worldmat[2][2]) >
                   (f32)(data->zdim + a)) {
            keep = 0;
        }
        break;
    case 4:
        /* tri-list collision sweep */
        if (fn_8005FDA8((u8*)item, from, pos, hitpt, norm, a) < 0.0) {
            keep = 0;
        }
        break;
    default:
        keep = 0;
        break;
    }
    if (keep == 0) {
        return -1.0f;
    }

    dist -= R;
    if (dist < 0.0) {
        dist = 0.0f;
    }

    /* soft types accept immediately at the probe point */
    type = item->info->type;
    if (type != 10) {
        if (type < 10) {
            if (type == 5) {
                goto accept_at_pos;
            }
            if (type < 5) {
                goto los_check;
            }
            if (type >= 8) {
                goto accept_at_pos;
            }
        } else if (type < 0xC) {
accept_at_pos:
            if (out != 0) {
                out[0] = pos[0];
                out[1] = pos[1];
                out[2] = pos[2];
            }
            return dist;
        }
    }

los_check:
    /* line-of-sight check from the probe origin */
    keep = 0;
    nv[0] = (f32)(from[0] - cx);
    nv[1] = 0.0f;
    nv[2] = (f32)(from[2] - cz);
    switch (coltype) {
    case 3:
        f1 = nv[0] * item->objgrp.worldmat[0][0] +
             nv[2] * item->objgrp.worldmat[0][2];
        if (wfabsf_(f1) > (f32)(data->xdim + a)) {
            goto los_done;
        }
        f2 = nv[0] * item->objgrp.worldmat[2][0] +
             nv[2] * item->objgrp.worldmat[2][2];
        if (wfabsf_(f2) > (f32)(data->zdim + a)) {
            goto los_done;
        }
        nv[0] = (f32)(pos[0] - cx);
        nv[1] = 0.0f;
        nv[2] = (f32)(pos[2] - cz);
        f3 = nv[0] * item->objgrp.worldmat[0][0] +
             nv[2] * item->objgrp.worldmat[0][2];
        if (f1 < 0.0f) {
            if (f3 > f1) {
                goto los_done;
            }
        }
        if (f1 > 0.0f && f3 < f1) {
            goto los_done;
        }
        f4 = nv[0] * item->objgrp.worldmat[2][0] +
             nv[2] * item->objgrp.worldmat[2][2];
        if (f2 < 0.0f) {
            if (f4 > f2) {
                goto los_done;
            }
        }
        if (f2 > 0.0f && f4 < f2) {
            goto los_done;
        }
        keep = 1;
        break;
    case 1:
        keep = 1;
        break;
    case 4:
        break;
    default:
        if (fqdist(nv[0], nv[2]) > Rsum) {
            break;
        }
        keep = 1;
        break;
    }

los_done:
    if (keep != 0) {
        nv[0] = pos[0] - from[0];
        nv[1] = 0.0f;
        nv[2] = pos[2] - from[2];
        mv[0] = (f32)(cx - from[0]);
        mv[1] = 0.0f;
        mv[2] = (f32)(cz - from[2]);
        NormalVector2D(nv);
        NormalVector2D(mv);
        if (nv[0] * mv[0] + nv[2] * mv[2] < 0.0f) {
            return -1.0f;
        }
    }

    if (out != 0) {
        switch (coltype) {
        case 3: {
            nv[0] = (f32)(pos[0] - cx);
            nv[1] = 0.0f;
            nv[2] = (f32)(pos[2] - cz);
            f3 = nv[0] * item->objgrp.worldmat[0][0] +
                 nv[2] * item->objgrp.worldmat[0][2];
            f4 = nv[0] * item->objgrp.worldmat[2][0] +
                 nv[2] * item->objgrp.worldmat[2][2];
            f2 = (f32)(data->xdim + a) - wfabsf_(f3);
            f1 = (f32)(data->zdim + a) - wfabsf_(f4);
            if (f2 > 0.0f || f1 > 0.0f) {
                if (f2 < f1 && f2 > 0.0f) {
                    if (f3 > 0.0f) {
                        out[0] = item->objgrp.worldmat[0][0] * f2 + pos[0];
                        out[1] = item->objgrp.worldmat[0][1] * f2 + pos[1];
                        out[2] = item->objgrp.worldmat[0][2] * f2 + pos[2];
                    } else {
                        f2 = -f2;
                        out[0] = item->objgrp.worldmat[0][0] * f2 + pos[0];
                        out[1] = item->objgrp.worldmat[0][1] * f2 + pos[1];
                        out[2] = item->objgrp.worldmat[0][2] * f2 + pos[2];
                    }
                } else if (f1 < f2 && f1 > 0.0f) {
                    if (f4 > 0.0f) {
                        out[0] = item->objgrp.worldmat[2][0] * f1 + pos[0];
                        out[1] = item->objgrp.worldmat[2][1] * f1 + pos[1];
                        out[2] = item->objgrp.worldmat[2][2] * f1 + pos[2];
                    } else {
                        f1 = -f1;
                        out[0] = item->objgrp.worldmat[2][0] * f1 + pos[0];
                        out[1] = item->objgrp.worldmat[2][1] * f1 + pos[1];
                        out[2] = item->objgrp.worldmat[2][2] * f1 + pos[2];
                    }
                } else {
                    out[0] = from[0];
                    out[1] = from[1];
                    out[2] = from[2];
                }
            } else {
                out[0] = from[0];
                out[1] = from[1];
                out[2] = from[2];
            }
            break;
        }
        case 4: {
            /* push the target out along the tri-list hit normal */
            nv[0] = hitpt[0] - pos[0];
            nv[1] = hitpt[1] - pos[1];
            nv[2] = hitpt[2] - pos[2];
            f1 = (f32)((nv[0] * norm[0] + nv[2] * norm[2]) + a);
            out[0] = pos[0];
            out[1] = pos[1];
            out[2] = pos[2];
            if (f1 > 0.0f) {
                out[0] = norm[0] * f1 + out[0];
                out[2] = norm[2] * f1 + out[2];
            }
            break;
        }
        case 1:
        default: {
        tangent_output:
            if (keep == 0) {
                nv[0] = pos[0] - from[0];
                nv[1] = 0.0f;
                nv[2] = pos[2] - from[2];
                mv[0] = (f32)(cx - from[0]);
                mv[1] = 0.0f;
                mv[2] = (f32)(cz - from[2]);
            }
            if (nv[2] * mv[0] - nv[0] * mv[2] > 0.0f) {
                f1 = -mv[2];
                mv[2] = mv[0];
                mv[0] = f1;
            } else {
                f1 = -mv[0];
                mv[0] = mv[2];
                mv[2] = f1;
            }
            NormalVector2D(mv);
            f1 = nv[0] * mv[0] + nv[2] * mv[2];
            out[0] = mv[0] * f1 + from[0];
            out[1] = mv[1] * f1 + from[1];
            out[2] = mv[2] * f1 + from[2];
            break;
        }
        }
    }
    return dist;
}

extern s32 LineCylinderCollide(f32* center, f32 radius, f32 halfHeight,
                               f32* from, f32* to, f32* hit,
                               s32 directional);
extern f32 lbl_80347004;

s32 fn_8005FB48(f32 radius, f32* from, f32* to, f32* limitPosition,
                 s32 stopAtFirst)
{
    Item* item;
    s32 itemIndex;
    s32 bestIndex;
    register s32 stop;
    s32 off;
    f32 delta[3];
    f32 zero;
    f64 zeroDouble;
    f32 limitDistance;
    f32 distance;
    s32 hit;
    iteminfo* info;
    s32 type;
    s32 subtype;

    stop = stopAtFirst;
    bestIndex = -1;
    if (limitPosition != 0) {
        delta[0] = limitPosition[0] - from[0];
        delta[1] = sItemZero;
        delta[2] = limitPosition[2] - from[2];
        limitDistance = fqdist(delta[0], delta[2]);
    } else {
        limitDistance = lbl_80347004;
    }

    off = 0;
    zeroDouble = sZeroDouble;
    itemIndex = 0;
    zero = sItemZero;
    for (; itemIndex < sNumItems;
         itemIndex++, off += sizeof(Item)) {
        item = (Item*)((u8*)sItems + off);
        if (item->active == -1) {
            continue;
        }

        info = item->info;
        type = info->type;
        subtype = info->item.subtype;
        switch (type) {
        case 2:
            switch (subtype) {
            case 43:
                if (item->action == 2) {
                    continue;
                }
                goto eligible_item;
            default:
                continue;
            }

        case 10:
            if (subtype == 49) {
                continue;
            }
            if (subtype >= 49) {
                goto high_subtype;
            }
            if (subtype == 41) {
                goto subtype_41;
            } else if (subtype >= 41) {
                goto eligible_item;
            } else if (subtype >= 40) {
                continue;
            }
            goto eligible_item;

        default:
            continue;
        }

high_subtype:
        if (subtype >= 54) {
            goto eligible_item;
        }
        if (subtype >= 51) {
            continue;
        }
        goto eligible_item;

subtype_41:
        if (item->health <= 0 || *(s16*)&item->data.raw[2] <= 0) {
            continue;
        }

eligible_item:
        if (info->item.coltype == 4) {
            if ((f64)(s32)fn_8005FDA8((u8*)item, from, to, 0, 0,
                                       radius) >= zeroDouble) {
                hit = 1;
            } else {
                hit = 0;
            }
        } else {
            hit = LineCylinderCollide(&item->objgrp.coll_pos[0],
                                      info->item.radius + radius,
                                      info->item.height + radius,
                                      from, to, delta, 0);
        }

        if (hit != 0) {
            delta[0] = item->objgrp.coll_pos[0] - from[0];
            delta[1] = zero;
            delta[2] = item->objgrp.coll_pos[2] - from[2];
            distance = fqdist(delta[0], delta[2]);
            if (bestIndex >= 0 && distance >= zero) {
                continue;
            }
            if (limitDistance < distance) {
                continue;
            }
            bestIndex = itemIndex;
        }

        if (bestIndex >= 0 && stop != 0) {
            break;
        }
    }

    return bestIndex;
}

/* 0x8005C1DC - apply a hit of `power` to one world item (item/object damage
 * dispatcher).  Returns the remaining health as a float (-1 none, -2 heavy). */
extern void  DeleteItem(Item* item, s32 mode);
extern void  start_magic(s32 owner, f32* pos, s32 value, s32 mode, f32 radius);
extern void  StartFXMat(s32 fx, OBJGRP* grp);
extern s32   StartExplosion(OBJGRP* grp, s32 kind, f32 radius);
extern s32   MBOX_ReallyFindObject(char* name, s32 a, s32 b, s32 c);
extern void  MBTreeSetZsortAdd(void* node, s32 value, s32 mode);
extern s32   EnemyDescType(char* desc);
extern char* EnemyTypePrefix(s32 type);
extern void  AudioPlayEvt101(f32* pos);
extern void  AudioExplodeWall(f32* pos, s32 health);
void fn_8005E90C(Item* item, s32* inst);
extern void  fn_8009DA78(f32* pos);
extern void  fn_8009DA28(f32* pos);
extern void  fn_8009D9D8(f32* pos);
extern void  fn_8009EF4C(f32* pos);
extern void  AudioGeneratorDies(f32* pos, s32 gen);
extern void  AudioGeneratorDamaged(f32* pos, s32 gen);
extern void  StartGenHitFx(OBJGRP* grp, s32 gen, s32 off);
extern s32   fn_80094440(f32* pos, u32 flags, s32 destroyed);
extern void  AddItemWobj(Item* item);
extern s32   Round(f32 value);
extern s32   stricmp(const char* a, const char* b);
extern char* strcat(char* dst, const char* src);
extern f64   lbl_80346F40;
extern f32   lbl_80346F54;
extern const char lbl_80346F58[8]; /* "BADMEAT" */
extern const char lbl_80346F60[7]; /* "GAPPLE"  */
extern f32   lbl_80346F68;
extern const char lbl_80346F70[8];    /* "BOSSGEN" */
extern f64   lbl_80346F88;
extern const char lbl_80346F90[8]; /* "BARPOI0" */
extern f64   lbl_80346F98;
extern const char lbl_80346FA0[8]; /* "BAREXP0" */
extern f32   lbl_80346FA8;

f32 fn_8005C1DC(Item* item, f32 power, s32 flags, s32 owner)
{
    s32 destroyed = 0;                        /* item died from this hit  */
    s32 alive;                                /* item tracks health       */
    s32 ret;                                  /* remaining health / code  */
    iteminfo* info = item->info;
    s32* sub = (s32*)((u8*)info + 4);
    char* objects = (char*)&sObjectsFile_80112AB8;
    char buf[0x24];
    f32 v[4];
    void* hdr;
    u8* rec;
    s16* generator;
    s32 k;
    s32 thr;
    s32 state;

    /* scale generator hits by how far the player's gold exceeds the ramp */
    if (info->type == 3 && owner >= 0 &&
        gCurLevel->plevel > sItemZero) {
        f32 mult;
        f32 ramp = gCurLevel->plevel;
        f32 gold = (f32)*(s32*)((u8*)gPlayers + owner * 0x335C + 0x3324);

        if (gold < ramp) {
            mult = (f32)(lbl_80346EE8 - lbl_80346F40 * (ramp - gold));
        } else if (gold > ramp) {
            mult = (f32)(sItemFloorYOffset * (gold - ramp) + lbl_80346EE8);
        } else {
            mult = sItemFloorRadius;
        }
        power = power * mult;
        if (power < lbl_80346EE8) {
            power = sItemFloorRadius;
        }
    }

    if ((flags & 0x800) != 0) {
        if (power <= sCameraVisibilityRadius) {
            ret = -1;
        } else {
            ret = -2;
        }
        power = sItemZero;
        alive = 0;
    } else {
        if (item->armor >= 0) {
            power = power - (f32)item->armor;
            if (power <= sItemZero) {
                power = sItemFloorRadius;
            }
            item->health -= Round(power);
            if (item->health < 0) {
                item->health = 0;
            }
            if (item->health == 0) {
                destroyed = 1;
            } else {
                destroyed = 0;
            }
        }
        if ((s8)item->armor == -1) {
            ret = -1;
            alive = 0;
        } else {
            alive = 1;
            ret = (s32)(f32)item->health;
        }
    }

    v[1] = item->objgrp.coll_pos[0];
    v[2] = item->objgrp.coll_pos[1];
    v[3] = item->objgrp.coll_pos[2];
    v[2] = (f32)(v[2] + lbl_80346EF0);

    switch (info->type) {
    case 1:
        switch (*sub) {
        case 4:
            if (ret == 0) {
                start_magic(-1, item->objgrp.attn_pos,
                            *(s32*)((u8*)info + 0x3C), 0, lbl_80346F54);
                if (item->info->type == 1 && *(Item**)&item->data.raw[0xC] != 0) {
                    DeleteItem(*(Item**)&item->data.raw[0xC], 0);
                }
                if (item->info->type == 2 && *(Item**)&item->data.raw[0xC] != 0) {
                    DeleteItem(*(Item**)&item->data.raw[0xC], 0);
                }
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                    *(u32*)&item->atree[0] = 0;
                }
                if (item->objgrp.node != 0) {
                    MBRemoveNode(item->objgrp.node, 0);
                    item->objgrp.node = 0;
                }
                item->active = -1;
                k = item - sItems;
                if (k < gNextItemIdx) {
                    gNextItemIdx = k;
                }
            }
            break;

        case 2:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
            break;

        case 3:
            if (ret == -2) {
                /* good food goes bad on a heavy hit */
                if (item->health == 2) {
                    hdr = AtreeMatch(sPowerupsBuf, lbl_80346F58, 1);
                    if (*(u32*)&item->atree[0] != 0) {
                        AtreeDelete(item->atree);
                    }
                    *(void**)&item->atree[0] =
                        AtreeInit(hdr, item->atree, 0, 0x800);
                    MBNodeSetParent(**(void***)&item->atree[0],
                                    item->objgrp.node);
                    *(s32*)&item->data.raw[4] = -100;
                } else {
                    hdr = AtreeMatch(sPowerupsBuf, lbl_80346F60, 1);
                    if (*(u32*)&item->atree[0] != 0) {
                        AtreeDelete(item->atree);
                    }
                    *(void**)&item->atree[0] =
                        AtreeInit(hdr, item->atree, 0, 0x800);
                    MBNodeSetParent(**(void***)&item->atree[0],
                                    item->objgrp.node);
                    *(s32*)&item->data.raw[4] = -50;
                }
                destroyed = 1;
                msgPost(0x88, -1, 0);
            }
            /* fall through */

        default:
        /* generic destroy */
        if ((flags & 0x400) != 0 && power >= lbl_80346F68) {
            StartFXMat(0x20, &item->objgrp);
            StartFXMat(0x21, &item->objgrp);
            MBOX_NewObject(&objects[0x14C], item->objgrp.node,
                           item->objgrp.node->parent, 0x80800);
            if (item->info->type == 1 && *(Item**)&item->data.raw[0xC] != 0) {
                DeleteItem(*(Item**)&item->data.raw[0xC], 0);
            }
            if (item->info->type == 2 && *(Item**)&item->data.raw[0xC] != 0) {
                DeleteItem(*(Item**)&item->data.raw[0xC], 0);
            }
            if (*(u32*)&item->atree[0] != 0) {
                AtreeDelete(item->atree);
                *(u32*)&item->atree[0] = 0;
            }
            if (item->objgrp.node != 0) {
                MBRemoveNode(item->objgrp.node, 0);
                item->objgrp.node = 0;
            }
            item->active = -1;
            k = item - sItems;
            if (k < gNextItemIdx) {
                gNextItemIdx = k;
            }
            msgPost(0x87, -1, 0);
        }
            break;

        case 1:
        /* food -> junk treasure */
        if ((flags & 0x400) != 0 && power >= lbl_80346F68) {
            StartFXMat(0x20, &item->objgrp);
            StartFXMat(0x21, &item->objgrp);
            hdr = AtreeMatch(sPowerupsBuf, &objects[0x158], 1);
            if (*(u32*)&item->atree[0] != 0) {
                AtreeDelete(item->atree);
            }
            ((atree*)item->atree)->root = AtreeInit(hdr, item->atree, 0, 0x800);
            MBNodeSetParent(**(void***)&item->atree[0], item->objgrp.node);
            *(s32*)&item->data.raw[4] = 10;
        }
            break;
        }
        break;

    case 2:
        rec = 0;
        if (*(s16*)&item->data.raw[0] >= 0) {
            rec = (u8*)gWorldInfo.iteminfo + *(s16*)&item->data.raw[0] * 0x50;
        }
        if (rec != 0 && (flags & 0x200) != 0 &&
            EnemyDescType(((iteminfo*)rec)->item.desc) == 0x1E && *sub != 0x2B) {
            /* enemy chest converts to an apple generator */
            *sub = 1;
            rec = (u8*)gWorldInfo.iteminfo;
            for (k = 0; k < *(s32*)((u8*)&gWorldInfo + offsetof(WorldInfo, niteminfos)); k++) {
                s32* rec_sub = &((iteminfo*)rec)->item.subtype;

                if (strcmp(lbl_80346F18, ((iteminfodata*)rec_sub)->desc) == 0 &&
                    ((iteminfo*)rec)->type == 1 && *rec_sub == 3) {
                    goto found_gen;
                }
                rec += 0x50;
            }
            k = -1;
found_gen:
            *(s16*)&item->data.raw[0] = (s16)k;
            AudioPlayEvt101(&v[1]);
            alive = 1;
            k = (s32)(lbl_80346F6C * power);
            *(s16*)&item->data.raw[2] = (s16)k;
        } else if (destroyed != 0 && (item->active & 0x200) != 0) {
            if ((item->active & 1) == 0) {
                item->active |= 1;
                fn_8005E90C(item, 0);
                if (*sub == 0x2B) {
                    fn_8009DA78(&v[1]);
                    msgPost(0x1B, -1, (char*)&v[1]);
                }
            }
        } else if ((flags & 0x400) != 0 && power >= lbl_80346F68) {
            if (*sub == 0x2C) {
                item->daction = 2;
                item->action = 2;
                item->active |= 1;
                fn_8005E90C(item, 0);
            } else {
                if (rec != 0 && EnemyDescType(((iteminfo*)rec)->item.desc) == 0x1E) {
                    fn_8005E90C(item, 0);
                }
                StartFXMat(0x1F, &item->objgrp);
                StartFXMat(0x21, &item->objgrp);
                if (*sub == 0x30) {
                    MBOX_NewObject(&objects[0x164], item->objgrp.node,
                                   item->objgrp.node->parent,
                                   0x80800);
                } else {
                    MBOX_NewObject(&objects[0x170], item->objgrp.node,
                                   item->objgrp.node->parent,
                                   0x80800);
                }
                if (item->info->type == 1 && *(Item**)&item->data.raw[0xC] != 0) {
                    DeleteItem(*(Item**)&item->data.raw[0xC], 0);
                }
                if (item->info->type == 2 && *(Item**)&item->data.raw[0xC] != 0) {
                    DeleteItem(*(Item**)&item->data.raw[0xC], 0);
                }
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                    *(u32*)&item->atree[0] = 0;
                }
                if (item->objgrp.node != 0) {
                    MBRemoveNode(item->objgrp.node, 0);
                    item->objgrp.node = 0;
                }
                item->active = -1;
                k = item - sItems;
                if (k < gNextItemIdx) {
                    gNextItemIdx = k;
                }
            }
        }
        break;
    case 3:
        /* enemy generator damage-state machine */
        generator = (s16*)&item->data.raw[0];
        thr = (s32)((f32)*(s16*)((u8*)info + 0x44) *
                    gCurLevel->gen_health);
        if (ret < 0) {
            break;
        }
        if (destroyed != 0) {
            state = 0;
        } else if (item->health <= thr) {
            state = 1;
        } else if (item->health <= thr * 2) {
            state = 2;
        } else {
            state = 3;
        }
        if (state != (s8)item->data.raw[6]) {
            item->data.raw[6] = state;
            if (state == 0) {
                item->armor = -1;
            } else {
                item->data.raw[3] = (u8)(item->data.raw[3] << 1);
            }
            if ((s8)item->data.raw[7] == 0x1C || (s8)item->data.raw[7] == 0x1D) {
                item->data.raw[7] = 0;
            }
            if ((s8)item->data.raw[7] == 0x1E) {
                item->data.raw[7] = 0;
            }
            if (stricmp(buf, lbl_80346F70) != 0) {
                if (*generator < -1) {
                    sprintf(buf, &objects[0x17C], (s8)item->data.raw[6]);
                } else {
                    sprintf(buf, &objects[0x18C],
                            EnemyTypePrefix(*generator),
                            (s8)item->data.raw[6]);
                }
            }
            hdr = AtreeMatchAnyHeader(buf, 1);
            if (hdr != 0) {
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                }
                ((atree*)item->atree)->root = AtreeInit(hdr, item->atree, 0, 0x800);
                MBNodeSetParent(**(void***)&item->atree[0], item->objgrp.node);
            } else {
                k = MBOX_ReallyFindObject(buf, -1, -1, -1);
                if (k < 0) {
                    strcat(buf, sLevelOneSuffix);
                    k = MBOX_ReallyFindObject(buf, -1, -1, -1);
                }
                if (k < 0) {
                    strcat(buf, sRootSuffix);
                    k = MBOX_ReallyFindObject(buf, -1, -1, -1);
                }
                if (k < 0) {
                    if (*(u32*)&item->atree[0] != 0) {
                        AtreeDelete(item->atree);
                        *(u32*)&item->atree[0] = 0;
                    }
                    if (item->objgrp.node != 0) {
                        MBRemoveNode(item->objgrp.node, 0);
                        item->objgrp.node = 0;
                    }
                    item->active = -1;
                    k = item - sItems;
                    if (k < gNextItemIdx) {
                        gNextItemIdx = k;
                    }
                } else {
                    MBSetObject(item->objgrp.node, k);
                }
            }
            if (state == 0) {
                item->active &= ~1;
                item->armor = -1;
            }
            if (state == 0) {
                StartGenHitFx(&item->objgrp, *generator, 1);
            } else {
                StartGenHitFx(&item->objgrp, *generator, 0);
            }
        }
        if (state == 0) {
            s32 enemy_count;

            AudioGeneratorDies(&v[1], *generator);
            enemy_count = gNumEnemies;
            for (k = 0; k < enemy_count; k++) {
                if (gEnemies[k].generator == item) {
                    gEnemies[k].generator = 0;
                }
            }
        } else {
            AudioGeneratorDamaged(&v[1], *generator);
        }
        break;

    case 10:
        if (ret < 0) {
            break;
        }
        if (alive != 0 && destroyed == 0 && *sub != 0x29) {
            *(s16*)&item->data.raw[4] = 1;
        }
        switch (*(s32*)((u8*)item->info + 4)) {
        default:
            /* 0x2B, walls, everything else: shake / rumble */
            if (destroyed != 0) {
                item->active |= 1;
                fn_8009DA78(&v[1]);
                destroyed = 0;
            } else {
                fn_8009EF4C(&v[1]);
            }
            break;
        case 0x2C:
            if (destroyed != 0) {
                item->active |= 1;
                StartExplosion(&item->objgrp, 0x18,
                               (f32)(lbl_80346F88 *
                                     gCurLevel->trap_damage));
                fn_8009D9D8(&v[1]);
                MBOX_NewObject(lbl_80346F90, item->objgrp.node,
                               item->objgrp.node->parent,
                               0x80800);
                alive = 0;
                ret = -2;
                destroyed = 0;
            } else {
                fn_8009EF4C(&v[1]);
            }
            break;
        case 0x2D:
            if (destroyed != 0) {
                item->active |= 1;
                StartExplosion(&item->objgrp, 0x19,
                               (f32)(lbl_80346F98 *
                                     gCurLevel->trap_damage));
                fn_8009DA28(&v[1]);
                MBOX_NewObject(lbl_80346FA0, item->objgrp.node,
                               item->objgrp.node->parent,
                               0x80800);
                alive = 0;
                ret = -2;
                destroyed = 0;
            } else {
                fn_8009EF4C(&v[1]);
            }
            break;
        case 0x2A:
            AudioExplodeWall(&v[1], item->health);
            break;
        case 0x29:
            if (*(s16*)&item->data.raw[2] >= 0) {
                AddItemWobj(item);
                destroyed = 0;
            }
            break;
        }
        if (destroyed != 0) {
            if (*(u32*)&item->atree[0] != 0) {
                AtreeDelete(item->atree);
                *(u32*)&item->atree[0] = 0;
            }
            if (item->objgrp.node != 0) {
                MBRemoveNode(item->objgrp.node, 0);
                item->objgrp.node = 0;
            }
            item->active = -1;
            k = item - sItems;
            if (k < gNextItemIdx) {
                gNextItemIdx = k;
            }
        }
        break;

    case 4:
        *(s32*)&item->data.raw[8] |= 1;
        break;

    case 5:
        if ((flags & 0x800) == 0 && *sub == 0x1F) {
            Item* w;
            ret = 1;
            for (w = item; w != 0; w = *(Item**)&w->data.raw[8]) {
                w->playermask |= 0xF;
            }
        }
        break;
    }

    if (alive != 0) {
        k = fn_80094440(&v[1], flags, destroyed);
        if (k >= 0) {
            MBTreeSetZsortAdd(*(void**)((u8*)Effects + k * 0xF0 + offsetof(Effect, node)),
                              (s32)(lbl_80346FA8 * info->item.radius), 1);
        }
    }
    return (f32)ret;
}

/* --------------------------------------------------------------------------
 * fn_8005D730  0x8005D730  size 0x0720 - resolve a player's collision with a
 * live world item.  The outer switch selects the broad item class; individual
 * cases queue pickups, activate generators, apply hazards, or update the
 * per-player mask carried by linked trigger items.
 * ------------------------------------------------------------------------ */
extern void GetPlayerPos(s32 player, f32* position);
extern void PlayerGiveGold(s32 player, s32 amount);
extern void add_got_it(s32 player, s32 subtype, s32 count);
extern void fn_8009D038(s32 player);
extern void fn_8009D078(f32* position);
extern void fn_8009D0A8(f32* position, s32 subtype);
extern void fn_8009D8CC(f32* position);
extern void AudioDamageTile(f32* position, s32 subtype);
extern s32 damage_player(s32 player, f32 damage, s32 type, u32 flags,
                         f32* direction);
extern s32 SumnerAnimate(s32 player);
extern s32 lbl_8034476C;
extern const char lbl_80346FD8[7];

s32 fn_8005D730(Player* player, Item* item)
{
    s32 result;
    iteminfo* info;
    iteminfodata* data;
    f32 playerPos[3];
    f32 itemPos[3];
    u8 unused[28];

    result = 0;
    info = item->info;
    data = &info->item;
    GetPlayerPos(player->index, playerPos);
    itemPos[0] = item->objgrp.coll_pos[0];
    itemPos[1] = item->objgrp.coll_pos[1];
    itemPos[2] = item->objgrp.coll_pos[2];

    switch (info->type) {
    case 1:
        if (*(s16*)&item->data.raw[0x10] <= 0 && player->speech_req == NULL) {
            player->speech_req = (s32*)item;
        }
        break;

    case 2:
        result = 1;
        if (item->action == 2) {
            if (info->item.subtype == 47) {
                s32 amount = *(s32*)&item->data.raw[4];

                if (amount >= 25) {
                    msgPost(17, player->index, (char*)player->col_pos);
                }
                PlayerGiveGold(player->index, amount);
                fn_8009D038(player->index);
                add_got_it(player->index, 1, amount);
                item->activetime = 8;
                item->active |= 0x100;
                {
                    s32* row = (s32*)player;

                    row += player->character * 7;
                    row[777] += amount;
                }
            } else {
                Item* linked = *(Item**)&item->data.raw[0xC];

                if (linked != NULL) {
                    result = 0;
                    if (player->speech_req == NULL) {
                        player->speech_req = (s32*)linked;
                    }
                }
            }
        } else if ((item->active & 0x10) != 0 && item->action == 0 &&
                   (item->active & 1) == 0) {
            s32 allow;

            if (player->item_body_lo > 0) {
                player->item_body_lo--;
                allow = -1;
            } else if ((gGameOptions.unlimited & 1) != 0) {
                allow = -1;
            } else {
                allow = 0;
            }

            if (allow != 0) {
                fn_8009D078(itemPos);
                item->active |= 1;
                item->opener = (s8)player->index;
                if (*(s16*)&item->data.raw[0] >= 0) {
                    fn_8005E90C(item, (s32*)player);
                }
            } else if (msgPost(2, player->index,
                               (char*)player->col_pos) == 0 &&
                       data->subtype == 48) {
                msgPost(23, player->index, (char*)player->col_pos);
            }
        }
        break;

    case 3:
        {
            s32 inverted;
            s32 selected;

            if (*(s8*)&item->data.raw[6] != 0) {
                inverted = 0;
            } else {
                inverted = 1;
            }
            if (inverted != 0) {
                selected = 0;
            } else {
                selected = 1;
            }
            result = selected;
        }
        break;

    case 4:
        if (*(f32*)&item->data.raw[0xC] >= 0.0) {
            *(u32*)&item->data.raw[8] |= 1;
        }
        if (fqdist(itemPos[0] - playerPos[0],
                   itemPos[2] - playerPos[2]) <= data->radius) {
            result = 1;
        } else {
            result = 0;
        }
        break;

    case 7:
        if (item->action == 0 && (item->active & 1) == 0) {
            f32 dot;

            dot = (playerPos[0] - *(f32*)((u8*)player + 0x87C)) *
                      (itemPos[0] - playerPos[0]) +
                  (playerPos[2] - *(f32*)((u8*)player + 0x884)) *
                      (itemPos[2] - playerPos[2]);
            if (dot < 0.0f) {
                result = 1;
            } else {
                s32 allow;

                if (player->item_body_lo > 0) {
                    player->item_body_lo--;
                    allow = -1;
                } else if ((gGameOptions.unlimited & 1) != 0) {
                    allow = -1;
                } else {
                    allow = 0;
                }

                if (allow != 0) {
                    item->active |= 1;
                    fn_8009D0A8(itemPos, data->subtype);
                } else {
                    msgPost(1, player->index,
                            (char*)&item->objgrp.worldmat[3][0]);
                    result = 1;
                }
            }
        } else {
            result = 1;
        }
        break;

    case 8:
        if ((player->flags & 1) == 0 &&
            sMusicFadeBase >= player->fxhittime) {
            s32 rawSubtype = data->subtype;
            s32 subtype = rawSubtype;
            s32 damageType = 2;
            s32 flags = info->item.properties | 0x80;
            f32 damage = *(f32*)&item->data.raw[0];

            if (rawSubtype == 0 || (u32)(subtype - 3) <= 1) {
                damageType = 3;
            }
            if ((flags & 0x30) != 0) {
                f32 direction[3];
                u8 directionPad[24];

                direction[0] =
                    (f32)(-1.0 * item->objgrp.worldmat[2][0]);
                direction[1] =
                    (f32)(-1.0 * item->objgrp.worldmat[2][1]);
                direction[2] =
                    (f32)(-1.0 * item->objgrp.worldmat[2][2]);
                damage_player(player->index, damage, damageType, flags,
                              direction);
            } else {
                damage_player(player->index, damage, damageType, flags, NULL);
            }

            if (sMusicTrackHi == 3 && subtype == 1 &&
                strcmp(info->item.desc, lbl_80346FD8) == 0) {
                subtype = 6;
            }
            AudioDamageTile(playerPos, subtype);
            player->fxhittime =
                (f32)(0.0333333333 * (f64)(item->activetime + 1) +
                      sMusicFadeBase);
            msgPost(21, player->index, (char*)player->col_pos);
        }
        break;

    case 9:
        *(u32*)&item->data.raw[4] |= 1 << player->index;
        result = 2;
        break;

    case 10:
        switch (data->subtype) {
        case 49:
            if ((item->active & 1) == 0) {
                fn_8009D8CC(&item->objgrp.worldmat[3][0]);
                item->active |= 1;
            }
            break;
        case 40:
        case 53:
            if ((item->active & 1) == 0) {
                fn_8009D91C(&item->objgrp.worldmat[3][0]);
                item->active |= 1;
            }
            break;
        case 41:
            if (*(s16*)&item->data.raw[2] > 0) {
                result = 1;
            }
            break;
        case 51:
        case 52:
            break;
        default:
            result = 1;
            break;
        }
        break;

    case 12:
        if (item->info->item.subtype == 2 && (item->active & 1) == 0) {
            item->active |= 1;
        }
        break;

    case 11:
        result = 2;
        break;

    case 5: {
        s16 flags = *(s16*)&item->data.raw[4];

        if ((flags & 0x100) != 0) {
            u8* record = *(u8**)&item->data.raw[0];

            if (record != NULL && (u8*)player->floor_name2 != record) {
                u8* worldRecords = (u8*)gWorldInfo.wobjs;
                u8* wanted = (u8*)player->floor_name2;
                s32 index = *(s16*)(record + 0x2E);
                s32 found = 0;

                while (index >= 0) {
                    record = worldRecords + index * 0x3C;
                    if (record == wanted) {
                        found = 1;
                        break;
                    }
                    index = *(s16*)(record + 0x2C);
                }
                if (found == 0) {
                    result = 0;
                    break;
                }
            }
            if (item->playermask == 0 && lbl_8034476C > 1 &&
                (flags & 0x400) != 0) {
                msgPost(126, player->index, (char*)player->col_pos);
            }
        } else if (item->playermask == 0 && lbl_8034476C > 1 &&
                   (flags & 0x400) != 0) {
            msgPost(127, player->index, (char*)player->col_pos);
        }

        if ((*(s16*)&item->data.raw[4] & 0x40) != 0) {
            item->playermask |= 1 << player->index;
        } else {
            Item* linked = item;
            s32 one = 1;

            while (linked != NULL) {
                linked->playermask |= one << player->index;
                linked = *(Item**)&linked->data.raw[8];
            }
        }

        if (sMusicTrackHi == 13) {
            s32 marker = *(u8*)&item->data.raw[6];

            if (marker == 0xF0) {
                if ((item->playermask & 0xF) != 0) {
                    if ((item->playermask & 0x10) == 0 &&
                        SumnerAnimate(player->index) != 0) {
                        item->playermask |= 0x10;
                    }
                } else {
                    item->playermask = 0;
                }
            }
        }
        break;
    }
    }

    return result;
}

/* --------------------------------------------------------------------------
 * fn_8005DE50  0x8005DE50  size 0x0ABC - item-pickup effect state machine
 * Called once per frame from do_players (player.c) for the item a player is
 * currently touching:  fn_8005DE50(player, player->special_collision_item).
 * Dispatches on the item info subtype (b->info->item.subtype) and applies the
 * pickup effect (gold / count / potion / milestone / powerup / shard / rune /
 * boss / level), then reparents + re-shows the item's scene node.
 * ------------------------------------------------------------------------ */
extern void PlayerGiveGold(s32 player, s32 amount);
extern void fn_8009CFA8(s32 player, s32 amount);
extern void add_got_it(s32 player, s32 subtype, s32 count);
extern s32  towerAwardWorldRunes(void);
extern s32  fn_8009FB30(void);
extern s32  FindStringMessageListSub_8001FC4C(s32 a, char* name);
extern void ControllerMessageBox(s32 a, s32 b, s32 c, s32 d);
extern void fn_8009CDF8(s32 player);
extern void fn_8009F748(s32 player, s32 sourcePlayer);
extern void fn_8009D038(s32 player);
extern s32  heal_player(Player* p, f32 amount);
extern s32 damage_player(s32 i, f32 dmg, s32 mode, u32 flags,
                         f32* direction);
extern void AudioPlayerSeverePain(s32 player);
extern void AudioPlayerEatFood(s32 player, s32 kind);
extern void PlayerAddPowerup(f32 duration, f32 strength, void* p, s32 type,
                             u32 mask);
extern void fn_8009CEE0(s32 player, s32 subtype, s32 flags);
extern void PlayerGiveShard(s32 player, s32 shard);
extern void fn_8009CE38(s32 player);
extern void StartGemFX(f32* pos, s32 kind);
extern void AudioNumRunesFound(s32 count);
extern void towerSetRuneNear(s32 player, s32 value);
extern void towerAdvanceBossRecord(s32 a, s32 b);
extern void towerAdvanceLevelRecord(s32 a, s32 b);

extern char  lbl_80112C50[];      /* .rodata string (absolute)  */
extern char  lbl_80112C5C[];      /* .rodata string (absolute)  */
extern const char lbl_80346FEC[7];/* .sdata2 string (sda21)     */
extern f32   lbl_80346FE8;        /* .sdata2 float              */
extern s32   lbl_80344810;        /* .sbss                      */
extern f32   lbl_80344818;        /* .sbss float                */
extern s32   welcome_timer;       /* .sbss                      */

void fn_8005DE50(Player* a, Item* b)
{
    iteminfo* ev;
    iteminfodata* it;
    s32 ret;
    s32 flag;
    u8 unused[8];

    ret = 0;
    if (b == NULL)
        return;
    ev = b->info;
    if (ev == NULL)
        return;
    if (a != NULL)
        goto process_item;
    if (a == NULL)
        goto done;
    goto done;
process_item:
    switch ((it = &ev->item)->subtype) {
    case 1: { /* gold */
        s32 amount = *(s32*)&b->data.raw[4];
        PlayerGiveGold(a->index, amount);
        {
            s32* row = (s32*)a;
            row += a->character * 7;
            row[777] += amount;
        }
        fn_8009CFA8(a->index, *(s32*)&b->data.raw[4]);
        add_got_it(a->index, it->subtype, *(s32*)&b->data.raw[4]);
        a->speak_kind = 1;
        if (sMusicTrackHi == 12) {
            if (towerAwardWorldRunes() != 0) {
                s32 h = fn_8009FB30();
                ControllerMessageBox(-1,
                    FindStringMessageListSub_8001FC4C(0, lbl_80112C50), 0, h);
                lbl_80344810 = 1;
                lbl_80344818 = sItemFloorRadius;
            }
        } else if (*(s32*)&b->data.raw[4] >= 25) {
            s8 op;
            msgPost(17, a->index, (char*)a->col_pos);
            op = b->opener;
            if (op >= 0 && op != a->index)
                fn_8009F748(op, a->index);
        }
        ret = 1;
        break;
    }
    case 2: { /* accumulating count (clamped to lbl_803448A4) */
        if (a->item_body_lo + *(s32*)&b->data.raw[4] <= lbl_803448A4) {
            s8 op;
            if (gNumType7Items != 0)
                msgPost(8, a->index, (char*)a->col_pos);
            else
                msgPost(2, a->index, (char*)a->col_pos);
            a->item_body_lo += *(s32*)&b->data.raw[4];
            op = b->opener;
            if (op >= 0 && op != a->index)
                fn_8009F748(op, a->index);
            fn_8009CDF8(a->index);
            add_got_it(a->index, it->subtype, *(s32*)&b->data.raw[4]);
            a->speak_kind = 1;
            ret = 1;
        } else if (a->item_body_lo < lbl_803448A4) {
            s32 room = lbl_803448A4 - a->item_body_lo;
            if (gNumType7Items != 0)
                msgPost(8, a->index, (char*)a->col_pos);
            else
                msgPost(2, a->index, (char*)a->col_pos);
            a->item_body_lo += room;
            *(s32*)&b->data.raw[4] -= room;
            fn_8009CDF8(a->index);
            add_got_it(a->index, it->subtype, *(s32*)&b->data.raw[4]);
            a->speak_kind = 1;
        } else {
            msgPost(4, a->index, (char*)a->col_pos);
        }
        break;
    }
    case 4: { /* milestone list */
        s32 j;
        s32 r;
        s8  op;
        if (a->item_body_hi < lbl_803448A0) {
            s32 prop = it->properties;
            for (j = 0; j < *(s32*)&b->data.raw[4]; j++) {
                if (a->item_body_hi >= lbl_803448A0)
                    break;
                *(s32*)((u8*)a + a->item_body_hi * 4 + 13056) = prop;
                a->item_body_hi++;
            }
            r = msgPost(7, a->index, (char*)a->col_pos);
            if (r < 0)
                r = msgPost(94, a->index, (char*)a->col_pos);
            if (r < 0)
                msgPost(95, a->index, (char*)a->col_pos);
            op = b->opener;
            if (op >= 0 && op != a->index)
                fn_8009F748(op, a->index);
            fn_8009D038(a->index);
            add_got_it(a->index, it->subtype, 0);
            a->speak_kind = 1;
            ret = 1;
        } else {
            msgPost(3, a->index, (char*)a->col_pos);
        }
        break;
    }
    case 3: { /* potion (heal / damage) */
        s8 op;
        f32 amt = (f32)*(s32*)&b->data.raw[4];
        if ((a->flags & 0x400) && strcmp(it->desc, lbl_80346F10) == 0)
            amt = lbl_80346FE8;
        if (amt >= sItemZero) {
            if (heal_player(a, amt) == 0) {
                msgPost(133, a->index, (char*)a->col_pos);
                break;
            }
        } else {
            damage_player(a->index, -amt, 0, 2048, 0);
        }
        if (*(s32*)&b->data.raw[4] >= 100)
            msgPost(15, a->index, (char*)a->col_pos);
        else if (*(s32*)&b->data.raw[4] >= 50)
            msgPost(16, a->index, (char*)a->col_pos);
        else if (*(s32*)&b->data.raw[4] < 0)
            msgPost(28, a->index, (char*)a->col_pos);
        op = b->opener;
        if (op >= 0 && op != a->index)
            fn_8009F748(op, a->index);
        add_got_it(a->index, it->subtype, (s32)amt);
        if (amt < sZeroDouble) {
            a->speak_kind = 3;
            AudioPlayerSeverePain(a->index);
        } else {
            s32 kind = 0;
            a->speak_kind = 1;
            if (strcmp(it->desc, lbl_80112C5C) == 0)
                kind = 3;
            else if (strcmp(it->desc, lbl_80346F18) == 0)
                kind = 1;
            else if (strcmp(it->desc, lbl_80346FEC) == 0)
                kind = 2;
            AudioPlayerEatFood(a->index, kind);
        }
        ret = 1;
        break;
    }
    case 5:
    case 6:
    case 7:
    case 8:
    case 9: { /* powerup */
        s32* data;
        s32 snd;
        s32 flags220;
        s32 subtype;
        data = (s32*)&b->data.raw[0];
        flags220 = data[0];
        snd = -1;
        subtype = it->subtype;
        if (subtype == 9 && (flags220 & 0xF000) && (a->flags & 0xF000))
            break;
        PlayerAddPowerup((f32)data[1], *(f32*)&data[2], a,
                         subtype, flags220);
        switch (it->subtype) {
        case 5: {
            s32 lownib = flags220 & 0xF;
            if (flags220 & 0x80000)
                snd = 37;
            else if (flags220 & 0x400000)
                snd = 47;
            else if (flags220 & 0x200000)
                snd = 38;
            else if (flags220 & 0x100000)
                snd = 48;
            else if (flags220 & 0x10000000)
                snd = 86;
            else if (flags220 & 0x20000000)
                snd = 87;
            else if (lownib == 1)
                snd = 40;
            else if (lownib == 2)
                snd = 41;
            else if (lownib == 3)
                snd = 42;
            else if (lownib == 4)
                snd = 43;
            break;
        }
        case 6:
            if (flags220 & 0x100000)
                snd = 54;
            else if (flags220 & 0x10000)
                snd = 35;
            else if (flags220 & 0x80000)
                snd = 49;
            else if (flags220 & 0x20000)
                snd = 52;
            else if (flags220 & 0x200000)
                snd = 91;
            else if (flags220 & 0x400000)
                snd = 92;
            else if (flags220 & 0x2000)
                snd = 132;
            break;
        case 7:
            snd = 32;
            break;
        case 8:
            snd = 33;
            break;
        case 9:
            if (flags220 & 0x4)
                snd = 36;
            else if (flags220 & 0x2)
                snd = 39;
            else if (flags220 & 0x8)
                snd = 51;
            else if (flags220 & 0x1)
                snd = 53;
            else if (flags220 & 0x10)
                snd = 81;
            else if (flags220 & 0x20)
                snd = 82;
            else if (flags220 & 0x40)
                snd = 83;
            else if (flags220 & 0x80)
                snd = 84;
            else if (flags220 & 0x100)
                snd = 88;
            else if (flags220 & 0x200)
                snd = 89;
            else if (flags220 & 0x400)
                snd = 93;
            else if (flags220 & 0x2000)
                snd = 98;
            else if (flags220 & 0x1000)
                snd = 99;
            else if (flags220 & 0x8000)
                snd = 100;
            else if (flags220 & 0x4000)
                snd = 100;
            else if (flags220 & 0x80000)
                snd = 113;
            else if (flags220 & 0x100000)
                snd = 148;
            else if (flags220 & 0x200000)
                snd = 149;
            else if (flags220 & 0x400000)
                snd = 150;
            break;
        }
        if (snd >= 0)
            msgPost(snd, a->index, (char*)a->col_pos);
        fn_8009CEE0(a->index, it->subtype, flags220);
        add_got_it(a->index, it->subtype, 0);
        a->speak_kind = 1;
        ret = 1;
        break;
    }
    case 10: { /* shard */
        s32 i;
        s32 mask;
        s32 count;
        if (PlayerHasShard(a->index, ev->item.value) != 0) {
            msgPost(90, a->index, (char*)a->col_pos);
            ret = 0;
            break;
        }
        for (i = 0; i < 4; i++)
            PlayerGiveShard(i, b->info->item.value);
        fn_8009CE38(a->index);
        add_got_it(a->index, it->subtype, *(s32*)&b->data.raw[4]);
        welcome_timer = 300;
        StartGemFX(b->objgrp.worldmat[3], 1024);
        sSpecialItem10 = 0;
        count = 0;
        mask = 0;
        for (i = 0; i < 4; i++) {
            if (gPlayers[i].state != 0)
                mask |= gPlayers[i].shards;
        }
        for (i = 0; i < 13; i++) {
            if (mask & (1 << i))
                count++;
        }
        AudioNumRunesFound(count);
        ret = 1;
        break;
    }
    case 13: /* rune */
        msgPost(*(s32*)&b->data.raw[4] + 113, a->index, (char*)a->col_pos);
        towerSetRuneNear(a->index, *(s32*)&b->data.raw[4]);
        fn_8009D038(a->index);
        add_got_it(a->index, it->subtype, *(s32*)&b->data.raw[4]);
        ret = 1;
        break;
    case 14: /* controller message */
        ControllerMessageBox(1 << a->index, -1, *(s32*)&b->data.raw[4] - 1, -1);
        ret = 1;
        break;
    case 15: { /* boss record */
        s32 amount = *(s32*)&b->data.raw[4];
        ret = 1;
        StartGemFX(b->objgrp.worldmat[3], amount);
        towerAdvanceBossRecord(-1, amount);
        fn_8009D038(a->index);
        add_got_it(a->index, it->subtype, amount);
        break;
    }
    case 16: /* level record */
        towerAdvanceLevelRecord(-1, *(s32*)&b->data.raw[4]);
        fn_8009D038(a->index);
        add_got_it(a->index, it->subtype, *(s32*)&b->data.raw[4]);
        ret = 1;
        StartGemFX(b->objgrp.worldmat[3], 256);
        break;
    }

    flag = ((s8)b->opener == -1) ? 8 : 15;
    if (ret != 0) {
        if (flag < 0)
            b->activetime = 16;
        else
            b->activetime = flag;
        b->active |= 0x100;
        if (*(void**)&b->data.raw[0xC] != NULL) {
            if (*(void**)((u8*)b->objgrp.node + 116) != sItemsRootNode) {
                MBNodeSetParent(b->objgrp.node, sItemsRootNode);
                UpdateObjWorldMat(&b->objgrp);
            }
            {
                Item* other = *(Item**)&b->data.raw[0xC];
                if (flag < 0)
                    other->activetime = 16;
                else
                    other->activetime = flag;
                other->active |= 0x100;
            }
        }
    }
done:
    ;
}

/* ---- fn_800606FC externs ------------------------------------------------- */
extern s32   find_enemy_slot(s32 type, s32 level);
extern void  SetPlayerVars(void);
extern s32   MBWorldSphereVisible3(f32* position, f32 radius);
extern s32   ItemVisible(Item* item);
extern s32   MBTreeGetAlpha(void* node);
extern void  MBTreeSetAlpha(void* node, s32 alpha, s32 value);
extern void  MBTreeSetAltTex(void* node, s32 tex, s32 alt, s32 value);
extern void  ExtractYPR(f32* matrix, f32* angles);
extern void CreateYPRMatrix(f32* mtx, f32* pyr);
extern f32   Random(f32 scale);
extern f64   DistanceToClosestPlayer(f32* pos);
extern s32   generate_now(Item* it, f32* pos, s32 max, s32 vis);
extern void  generate_single(Item* it, s32 algorithm, s32 important);
extern void fn_80060114(Item* item, f32* pos, f32* dir);
extern void fn_80062A00(void);
extern void  place_logic12(s8* data, s32 enemy_index);
extern s32   did_generate(void* owner, s32 checkEnemies);
extern void  add_target(void* id);
extern void  del_target(void* id);
extern void  ShakeCamera(s32 type, s32 count, s32 delay, f32 rad, s32 priority);
extern s32   towerAllPlayersMetLevelReq(s32 level);
extern void  TowerNeedGargItemsMsg(s32 who, s32 slot);
extern void  TowerNeedCrystalsMsg(s32 who, s32 slot);
extern s32   AudioSecretProc(f32 scale, s32 sound, f32* position, u32 flags,
                             s32* instance, s32* mask);
extern void  AudioStopAll(void);
extern void  SaveAllRecords(s32 excludedItem, s32 player, f32* playerPos,
                            u32 record);
extern void  init_got_it(void);
extern void  fn_8009D9A4(f32* pos);
extern void  fn_8009D7E4(s32 mode, f32* pos);
extern void  YawMat3(f32* matrix, f32 angle);
extern f64   __frsqrte(f64 x);
extern f32   gClockFrameStep;
extern s32   gTriggerCameraState;
extern s32   lbl_803447D0;
extern s32   lbl_803447DC;
extern s32   lbl_803447E0;
extern s32   lbl_80344500;
extern s32   lbl_80344960;
extern u32   lbl_80344A80;
extern s32   sMusicSubIndex;
extern s32   sMusicSubState;
extern f32   lbl_80343C50;
extern f32   lbl_80344C5C;
extern f64   lbl_80346FB8;
extern f64   lbl_80347018;
extern f32   lbl_80127D00[4];
extern f32   lbl_8011C904[8];


/* world_update's original source wrote these as raw literal constants, not
 * named globals: MWCC pooled them in .sdata2 and cached them in callee-saved
 * FPRs across the two item loops (target world_update saves f14-f31), which
 * extern references can never reproduce because calls may clobber globals.
 * Shadow the names with their exact pool values for this function only. */
#define sArrowFloorYOffset      0.5
#define sNewtonThree            3.0
#define sZeroDouble             0.0
#define sPi                     3.141592654
#define sTwoPi                  6.283185308
#define sNegativePi             -3.141592654
#define lbl_80346EE8            1.0
#define lbl_80346EF0            2.0
#define lbl_80346F88            30.0
#define lbl_80346F98            10.0
#define lbl_80346FB8            1.5
#define lbl_80347018            50.0
#define lbl_80347040            1.75
#define lbl_80347048            15.0
#define lbl_80347058            0.7853981635
#define lbl_80347060            40.0
#define lbl_80347068            -0.05235987756666667
#define lbl_80347070            0.05235987756666667
#define lbl_80347078            0.06981317008888889
#define lbl_80347080            -0.06981317008888889
#define lbl_80347088            0.2
#define lbl_80347090            0.8
#define lbl_803470A0            2.83
#define lbl_803470A8            9.0
#define lbl_803470B0            20.0
#define lbl_803470C0            0.17453292522222225
#define lbl_803470C8            0.017453292522222223
#define lbl_803470D0            0.3490658504444445
#define lbl_803470D8            200.0
#define sItemZero               0.0f
#define sItemFloorRadius        1.0f
#define sItemSearchDistance     10.0f
#define sCameraVisibilityRadius 2.0f
#define sArrowFloorRadius       0.2f
#define lbl_80347000            100000.0f
#define lbl_80347050            6.0f
#define lbl_80347098            0.1f
#define lbl_803470B8            15.0f
#define lbl_803470BC            100.0f

/* delete a live item and recycle its pool slot */
#define KILL_ITEM(itm)                                                       \
    do {                                                                     \
        if (*(void**)(itm)->atree != NULL) {                                 \
            AtreeDelete((itm)->atree);                                       \
            *(void**)(itm)->atree = NULL;                                    \
        }                                                                    \
        if ((itm)->objgrp.node != NULL) {                                    \
            MBRemoveNode((itm)->objgrp.node, 0);                             \
            (itm)->objgrp.node = NULL;                                       \
        }                                                                    \
        (itm)->active = -1;                                                  \
        {                                                                    \
            s32 slot_ = ((s32)((u8*)(itm) - (u8*)sItems)) / 0xF0;            \
            if (slot_ < gNextItemIdx) {                                      \
                gNextItemIdx = slot_;                                        \
            }                                                                \
        }                                                                    \
    } while (0)

#define WRAP_ANGLE(dst)                                                          do {                                                                             f64 wa_ = (dst);                                                             if (wa_ > sPi) {                                                                 wa_ = wa_ - sTwoPi;                                                      } else if (wa_ <= sNegativePi) {                                                 wa_ = sTwoPi + wa_;                                                      }                                                                            (dst) = (f32)wa_;                                                        } while (0)

/* 0x800606FC - per-frame world item/object update dispatcher */
void fn_800606FC(void)
{
    Item* it;
    s32 i;
    s32 paused;
    s32 musicState;
    s32 musicIdx;
    f32 gpos[3];
    f32 gypr[3];
    u8* rt = (u8*)&sItemRuntime;
    u8 unused[120];

    if (gGameMode == MA_FLYBY) {
        paused = 0;
    } else if (gGameMode == MG_PLAY) {
        paused = lbl_803447B8;
    } else {
        paused = 1;
    }

    default_gen_count = 0;
    sItemRandSeed = sItemRandSeed + 1;
    find_enemy_slot(0, -1);
    musicIdx = -1;
    musicState = 0;
    if (lbl_803447B4 == 0) {
        lbl_803447D0 = 0;
    }
    if (lbl_80344768 == 0) {
        SetPlayerVars();
    }

    it = sItems;
    for (i = 0; i < sNumItems; i++, it++) {
        s32 type;
        s32 vis;
        s32 cond;
        s32 isopen;
        s16 a;

        if (it->active == -1) {
            continue;
        }
        type = it->info->type;
        if (type == -1) {
            continue;
        }
        vis = MBWorldSphereVisible3(it->objgrp.attn_pos, it->visrad);
        if (vis != 0 && lbl_80344A6C != NULL && (u32)(lbl_80344A80 - 1) <= 1) {
            f32 dy = lbl_80344A6C->attention.y - it->objgrp.attn_pos[1];
            f32 dx = lbl_80344A6C->attention.x - it->objgrp.attn_pos[0];
            f32 dz = lbl_80344A6C->attention.z - it->objgrp.attn_pos[2];
            f32 d2 = dy * dy;
            d2 = dx * dx + d2;
            d2 = dz * dz + d2;
            if (d2 > lbl_80343C50) {
                vis = 0;
            }
        }
        if (paused == 0 || it->info->type == 10) {
            if (vis != 0) {
                it->active |= 0x4000;
            } else {
                it->active &= ~0x4000;
            }
        }
        cond = 0;
        if (type == 1 && it->info->item.subtype == 2) {
            cond = 1;
        }
        isopen = cond ? 1 : 0;
        {
            s8 mo = it->minoff;
            if (mo == 1 ||
                (mo == 2 &&
                 (isopen || gGameOptions.items != 0 ||
                  gGameOptions.players != 0))) {
                if (vis == 0 || isopen || gGameOptions.items != 0 ||
                    gGameOptions.players != 0) {
                    if (ItemVisible(it) == 0) {
                        continue;
                    }
                    it->minoff = 0;
                    MBTreeClearFlags(it->objgrp.node, 2, 0);
                } else {
                    if (paused != 0) {
                        continue;
                    }
                    it->minoff = 2;
                    continue;
                }
            } else {
                if (mo != 0) {
                    continue;
                }
                if (ItemVisible(it) == 0) {
                    if (vis == 0 || gGameOptions.items != 0 ||
                        gGameOptions.players != 0) {
                        it->minoff = 1;
                        MBTreeSetFlags(it->objgrp.node, 2, 0);
                        continue;
                    }
                }
            }
        }
        a = it->active;
        if (!(a & 0x40) && vis == 0 && !((a & 4) && (s8)it->action > 0)) {
            if (it->info->type != 2 || (s8)it->action <= 0) {
                continue;
            }
        }
        if (it->activetime > 0) {
            it->activetime -= gFrameTicks;
        }
        a = it->active;
        if ((a & 0x100) && it->activetime <= 0) {
            KILL_ITEM(it);
            continue;
        }
        if (!(a & 1)) {
            s32 mode;
            s32 dact;
            s32 act;
            if (*(void**)it->atree == NULL) {
                continue;
            }
            if ((s8)it->action == (s8)it->daction) {
                mode = 0;
            } else {
                mode = 2;
            }
            act = (s8)it->action;
            dact = (s8)it->daction;
            if (it->info->type == 5) {
                if (act == 0 && dact == 2) {
                    dact = 1;
                } else if (act == 2 && dact == 0) {
                    dact = 3;
                }
                mode = 1;
            }
            if (AnimateATree(it->atree, dact, mode) & 3) {
                it->action = it->daction;
            }
            continue;
        }
        if (*(void**)it->atree != NULL) {
            animinfo* anim = &((atree*)it->atree)->animinfo;
            s32 mode = 2;
            s32 t;
            s32 w;
            s32 res;
            if (it->activetime <= 0) {
                if (a & 4) {
                    if ((s8)(it->daction += 1) >= anim->numseqs) {
                        if (it->active & 2) {
                            it->daction--;
                        } else {
                            it->daction = 0;
                        }
                    }
                }
                if (it->active & 0x80) {
                    it->active &= ~1;
                }
            }
            if ((s8)it->action == (s8)it->daction) {
                mode = 0;
            }
            if (!(AnimateATree(it->atree, (s8)it->daction, mode) & 3)) {
                continue;
            }
            if (!(it->active & 0x400)) {
                if ((s8)it->daction != 0) {
                    t = it->info->item.activeon << 1;
                } else {
                    iteminfo* inf = it->info;
                    t = inf->item.activeoff << 1;
                    if (inf->type == 8) {
                        t = (s32)((f32)t * gCurLevel->trap_rate);
                    }
                }
                w = (s32)(sArrowFloorYOffset +
                          (f32)anim->numframes * anim->seqscale)
                    << 1;
                if (it->info->type == 8 && anim->numseqs > 4 && t < 0) {
                    t = 0;
                    w = 0;
                }
                res = w;
                if (t != 0) {
                    res = t;
                    if (t < 0) {
                        res = RandInt(-t) + ((-t) >> 1);
                    }
                }
                it->activetime = res;
            }
            it->action = it->daction;
            continue;
        }
        if (it->activetime > 0) {
            continue;
        }
        if (a & 4) {
            s8 c8 = it->action;
            if (!((s8)c8 != 0 && (a & 2))) {
                s32 na;
                s32 t;
                if ((s8)c8 != 0) {
                    na = 0;
                } else {
                    na = 1;
                }
                it->action = na;
                if ((s8)it->action != 0) {
                    t = it->info->item.activeon << 1;
                } else {
                    iteminfo* inf = it->info;
                    t = inf->item.activeoff << 1;
                    if (inf->type == 8) {
                        t = (s32)((f32)t * gCurLevel->trap_rate);
                    }
                }
                if (t < 0) {
                    t = RandInt(-t) + ((-t) >> 1);
                }
                it->activetime = t;
            }
        }
        if (it->active & 0x80) {
            it->active &= ~1;
        }
    }

    lbl_80344960 = -1;
    it = sItems;
    for (i = 0; i < sNumItems; i++, it++) {
        s32 type;
        if (it->active == -1) {
            continue;
        }
        if (it->active & 0x8100) {
            continue;
        }
        fn_8005A338(&it->objgrp, it->coll_offset, it->coll_offset);
        type = it->info->type;
        if (type == -1) {
            continue;
        }
        if ((s8)it->minoff != 0) {
            continue;
        }
        if (!((type == 3 && it->data.gen.ai == 0xF) ||
              (type == 0xC && it->info->item.subtype == 2) ||
              (it->active & 0x40) || (it->active & 0x4000))) {
            continue;
        }
        switch (type) {
        case 1:
            if (it->data.powerup.nograb > 0) {
                it->data.powerup.nograb -= gFrameTicks;
            }
            switch (it->info->item.subtype) {
            case 0xF:
                if (it->active & 0x800) {
                    s32 alpha;
                    if (sZeroDouble == lbl_80344C5C) {
                        break;
                    }
                    if (lbl_80344C5C > sZeroDouble) {
                        LookoutParam* lp = (LookoutParam*)(rt + 0xCB8);
                        s32 n = sNumLookoutParams;
                        s32 i;
                        for (i = 0; i < n; i++) {
                            if (lp->param == 0) {
                                goto lookout_found;
                            }
                            lp++;
                        }
                        ErrorPrintf(sMissingLookoutParamFmt, 0);
                        lp = NULL;
                    lookout_found:
                        if (lp != NULL) {
                            f32 fade = (f32)(lbl_80347040 +
                                             (sMusicFadeBase - lbl_80344C5C));
                            f32 d = fqdist(
                                it->objgrp.worldmat[3][0] - lbl_80127D00[0],
                                it->objgrp.worldmat[3][2] - lbl_80127D00[2]);
                            if (d > lbl_80347048 * fade) {
                                break;
                            }
                        }
                    }
                    alpha = MBTreeGetAlpha(it->objgrp.node) - 8;
                    if (alpha <= 0) {
                        alpha = 0;
                        it->active &= ~0x800;
                    }
                    MBTreeSetAlpha(it->objgrp.node, alpha, 1);
                }
                break;
            case 4:
                break;
            }
            break;
        case 3: {
            gendata* gen = &it->data.gen;
            s32 max;
            s32 visflag;
            if (gGameOptions.gen_active <= 1) {
                break;
            }
            if (gGameOptions.gen_active == 2) {
                break;
            }
            if (gen->strength <= 0) {
                break;
            }
            if (gen->etype == -1) {
                break;
            }
            visflag = it->active & 0x4000;
            max = gen->maxenemies;
            if (gen->ai == 0xF) {
                generate_single(it, 0xF, 0);
                break;
            }
            if (gen->numenemies >= max) {
                break;
            }
            gpos[0] = it->objgrp.coll_pos[0];
            gpos[1] = it->objgrp.coll_pos[1];
            gpos[2] = it->objgrp.coll_pos[2];
            if (generate_now(it, gpos, max, visflag) != 0) {
                s32 slot;
                s32 imp;
                f32 rad = it->info->item.height;
                gypr[0] = it->objgrp.worldmat[2][0];
                gypr[1] = it->objgrp.worldmat[2][1];
                gypr[2] = it->objgrp.worldmat[2][2];
                if (gen->ai == 0xC && gen->num_generated >= 3) {
                    gen->num_generated -= 3;
                    gen->flags = 0;
                }
                if (visflag != 0) {
                    imp = 0;
                } else {
                    imp = -1;
                }
                slot = generate_enemy(gpos, gen->etype, gen->strength, gypr,
                                      gen->ai, it, imp, rad);
                if (slot < 0) {
                    break;
                }
                {
                    Enemy* e = &gEnemies[slot];
                    s32 wob;
                    f32 fa = sItemFloorRadius + it->data.gen.genratio;
                    f32 rate = sItemFloorRadius /
                               (f32)(sCameraVisibilityRadius * (f32)max);
                    wob = (s32)((f32)(lbl_80347050 * (f32)gen->interval) * fa);
                    it->data.gen.counter = (s16)wob;
                    *(f32*)&it->data.raw[0xC] =
                        *(f32*)&it->data.raw[0xC] + rate;
                    if (*(f32*)&it->data.raw[0xC] > sItemFloorRadius) {
                        *(f32*)&it->data.raw[0xC] = sItemZero;
                    }
                    e->birth_style = 0;
                    e->ang = it->data.gen.ang + e->genang_offset;
                    WRAP_ANGLE(e->ang);
                    e->angbak = e->ang;
                    e->birth_pos[0] = e->objgrp.worldmat[3][0];
                    e->birth_pos[1] = e->objgrp.worldmat[3][1];
                    e->birth_pos[2] = e->objgrp.worldmat[3][2];
                    e->pyr[0] = sItemZero;
                    e->pyr[1] = e->ang;
                    e->pyr[2] = sItemZero;
                    if (gen->ai == 0xC) {
                        place_logic12((s8*)gen, slot);
                        break;
                    }
                    if (gen->ai == 0xD) {
                        if (gen->tail < 0) {
                            e->prev_enemy = -1;
                        } else {
                            Enemy* prev = &gEnemies[gen->tail];
                            prev->next_enemy = slot;
                            e->prev_enemy = gen->tail;
                        }
                        e->next_enemy = -1;
                        gen->tail = (s8)slot;
                        gen->numenemies++;
                        gen->num_generated++;
                        break;
                    }
                    if (gen->ai == 0xE) {
                        if (gen->num_generated & 1) {
                            e->ang = (f32)(e->ang - lbl_80347058);
                            e->flag1 = -1;
                        } else {
                            e->ang = (f32)(e->ang + lbl_80347058);
                            e->flag1 = 1;
                        }
                        WRAP_ANGLE(e->ang);
                        e->angbak = e->ang;
                        gen->numenemies++;
                        gen->num_generated++;
                        break;
                    }
                    gen->numenemies++;
                    gen->num_generated++;
                }
            }
            break;
        }
        case 8:
            if (lbl_803447DC != 0) {
                it->action = 0;
                it->daction = 0;
                *(s16*)(it->atree + 0x12) = 0;
                it->activetime = 0x1E;
            } else if ((s8)it->paction == 0 && (s8)it->action == 1 &&
                       it->info->item.subtype == 4) {
                f32 dy = gCameras[0].attn[1] - it->objgrp.worldmat[3][1];
                f32 dx = gCameras[0].attn[0] - it->objgrp.worldmat[3][0];
                f32 dz = gCameras[0].attn[2] - it->objgrp.worldmat[3][2];
                f32 d2 = dy * dy;
                volatile f32 root;
                d2 = dx * dx + d2;
                d2 = dz * dz + d2;
                if (d2 > sItemZero) {
                    f64 estimate = __frsqrte(d2);
                    estimate = sArrowFloorYOffset * estimate *
                               (sNewtonThree - estimate * estimate * d2);
                    estimate = sArrowFloorYOffset * estimate *
                               (sNewtonThree - estimate * estimate * d2);
                    estimate = sArrowFloorYOffset * estimate *
                               (sNewtonThree - estimate * estimate * d2);
                    root = (f32)(d2 *
                                 (sArrowFloorYOffset * estimate *
                                  (sNewtonThree - estimate * estimate * d2)));
                    d2 = root;
                }
                if (d2 < lbl_80347060) {
                    fn_8009D9A4(it->objgrp.worldmat[3]);
                }
            }
            it->paction = it->action;
            break;
        case 2: {
            u8* link;
            if ((s8)it->action == 0 && it->data.container.timer > 0) {
                f32 wob[3];
                s16 t;
                wob[0] = sItemZero;
                wob[1] = it->data.container.yaw;
                wob[2] = sItemZero;
                t = it->data.container.timer - gFrameTicks;
                it->data.container.timer = t;
                if (t <= 0) {
                    it->data.container.timer = 0;
                } else {
                    f64 step;
                    if ((it->data.container.timer + 4) & 8) {
                        step = lbl_80347068;
                    } else {
                        step = lbl_80347070;
                    }
                    wob[0] = (f32)(wob[0] + step);
                    WRAP_ANGLE(wob[0]);
                    if (it->data.container.timer & 8) {
                        step = lbl_80347078;
                    } else {
                        step = lbl_80347080;
                    }
                    wob[1] = (f32)(wob[1] + step);
                    WRAP_ANGLE(wob[1]);
                }
                CreateYPRMatrix(it->objgrp.worldmat[0], wob);
                UpdateObjWorldMat(&it->objgrp);
            }
            link = (u8*)it->data.container.contents;
            if (link != NULL && *(void**)it->atree != NULL &&
                it->info->item.subtype != 0x2C) {
                mbnode* node2 = ((Item*)link)->objgrp.node;
                if ((s8)it->action < 2) {
                    animinfo* anim = &((atree*)it->atree)->animinfo;
                    f32 al;
                    if ((s8)it->action == 0) {
                        al = sArrowFloorRadius;
                    } else if (anim->numframes > 1) {
                        al = (f32)(lbl_80347090 *
                                       ((lbl_80346EE8 + anim->frame) /
                                        (f64)anim->numframes) +
                                   lbl_80347088);
                    } else {
                        al = sItemFloorRadius;
                    }
                    MBTreeSetFlags(node2, 8, 0);
                    node2->scale[0] = al;
                    node2->scale[1] = al;
                    node2->scale[2] = al;
                } else {
                    MBTreeClearFlags(node2, 8, 0);
                }
            }
            if ((s8)it->action == 2) {
                s32 sub2 = it->info->item.subtype;
                if (sub2 == 0x2C) {
                    gpos[0] = it->objgrp.coll_pos[0];
                    gpos[1] = it->objgrp.coll_pos[1];
                    gpos[2] = it->objgrp.coll_pos[2];
                    StartExplosion(&it->objgrp, 0x1D,
                                   (f32)(lbl_80347018 *
                                         gCurLevel->trap_damage));
                    fn_8009D9D8(gpos);
                    KILL_ITEM(it);
                    msgPost(0x89, -1, 0);
                } else if (it->data.container.contents == NULL && sub2 != 0x2B &&
                           sub2 != 0x2F) {
                    KILL_ITEM(it);
                }
                {
                    s32 n;
                    for (n = 0; n < 25; n++) {
                        Enemy* e = &gEnemies[n];
                        s32* genid = &e->guard_closest;
                        if (n == *genid) {
                            e->guard_mode = 0;
                            *genid = -1;
                            e->guard_dist = lbl_80347000;
                        }
                    }
                }
            }
            break;
        }
        case 4:
            fn_80060114(it, gpos, gypr);
            break;
        case 5: {
            WorldObj* tgt;
            s16 flags;
            s32 mask;
            s32 pdact;
            s32 subtype;
            mask = 0;
            pdact = it->daction;
            subtype = it->info->item.subtype;
            if (it->active & 0x400) {
                break;
            }
            if (*(s16*)&it->data.raw[0x10] > 0) {
                *(s16*)&it->data.raw[0x10] -= gFrameTicks;
            }
            flags = *(s16*)&it->data.raw[4];
            tgt = it->data.trigger.target;
            if (flags & 0x40) {
                Item* p2;
                s32 lvl = it->data.trigger.id;
                if (lvl < 100) {
                    if (it->playermask != 0 &&
                        towerAllPlayersMetBossReq(lvl) == 0) {
                        TowerNeedCrystalsMsg(it->playermask,
                                             (s8)it->data.raw[6]);
                        it->playermask = 0;
                    }
                } else {
                    s32 garg = lvl - 0x65;
                    if (it->playermask != 0 &&
                        towerAllPlayersMetLevelReq(garg) == 0) {
                        if (garg < 3) {
                            TowerNeedGargItemsMsg(it->playermask, garg);
                        }
                        it->playermask = 0;
                    }
                }
                for (p2 = it; p2 != NULL;
                     p2 = p2->data.trigger.next) {
                    p2->playermask = it->playermask;
                }
            }
            if (it->playermask != 0) {
                if (it->data.trigger.flags & 0x100) {
                    u32 m = 0xFFFFFFF0;
                    s32 b;
                    for (b = 0; b < 4; b++) {
                        Player* p = &gPlayers[b];
                        if (p->state == 1 &&
                            (p->floor_name2 == tgt ||
                             (p->floor_name2 != NULL &&
                              p->floor_name2->parent == tgt))) {
                            m |= 1 << b;
                        }
                    }
                    it->playermask &= m;
                }
                mask = it->playermask;
            }
            flags = it->data.trigger.flags;
            if (flags & 0x400) {
                if (mask != lbl_803447E0) {
                    it->playermask = 0;
                    mask = 0;
                    if (tgt != NULL) {
                        tgt->flags &= ~0x4000000;
                    }
                } else {
                    if (tgt != NULL) {
                        tgt->flags |= 0x4000000;
                    }
                }
            }
            if (tgt != NULL) {
                if (flags & 1) {
                    if (mask != 0) {
                        if ((tgt->triggerstate & ~0xF) == 0x20) {
                            tgt->triggerstate &= 0xF;
                        }
                        it->daction = 2;
                    } else {
                        s32 d;
                        if (tgt->triggerstate & 0x20) {
                            d = 0;
                        } else {
                            d = 2;
                        }
                        it->daction = d;
                    }
                } else if (flags & 2) {
                    if (mask != 0) {
                        if ((tgt->triggerstate & ~0xF) == 0) {
                            tgt->triggerstate &= 0xF;
                            tgt->triggerstate |= 0x20;
                        }
                        it->daction = 2;
                    } else {
                        s32 d;
                        if (tgt->triggerstate & 0x20) {
                            d = 2;
                        } else {
                            d = 0;
                        }
                        it->daction = d;
                    }
                    if (subtype == 0x17 && !(gGameMode & MODE_GROUP_ATTRACT)) {
                        msgPost(5, -1, (char*)it->objgrp.attn_pos);
                    }
                } else if (flags & 4) {
                    if (mask != 0) {
                        s32 doset = 1;
                        s32 low = tgt->triggerstate & 0xF;
                        if ((tgt->triggerstate & 0x10) == 0) {
                            if (low < (low | mask)) {
                                tgt->triggerstate &= ~0xF;
                                tgt->triggerstate |= mask;
                            }
                            if (it->data.trigger.idletime <= 0) {
                                tgt->triggerstate ^= 0x20;
                                tgt->triggerstate &= ~0xF;
                                tgt->triggerstate |= mask;
                            } else {
                                doset = 0;
                            }
                            if (doset != 0) {
                                it->data.trigger.idletime = 0x78;
                            }
                        } else {
                            it->data.trigger.idletime = 0x78;
                        }
                        it->daction = 2;
                    } else {
                        it->daction = 0;
                        if (flags & 0x800) {
                            it->data.trigger.idletime =
                                (lbl_80344768 - 1) * 0x3C;
                        }
                    }
                } else {
                    if (mask != 0) {
                        tgt->triggerstate = (s8)mask;
                        tgt->triggerstate |= 0x20;
                        it->daction = 2;
                    } else {
                        s32 d;
                        if (tgt->triggerstate & 0x20) {
                            d = 2;
                        } else {
                            d = 0;
                        }
                        it->daction = d;
                    }
                }
            } else {
                if (flags & 4) {
                    s32 d;
                    if (mask != 0) {
                        d = 2;
                    } else {
                        d = 0;
                    }
                    it->daction = d;
                } else if (mask != 0) {
                    it->daction = 2;
                }
            }
            if ((s8)it->daction != 0 && pdact == 0) {
                if (flags & 0x1000) {
                    ShakeCamera(0, 0, 0xB4, lbl_80347098, 0x64);
                }
                if (flags & 0x2000) {
                    f32 d;
                    Item* hit = fn_80062FF0(sItemSearchDistance,
                                            it->objgrp.worldmat[3], 4, &d, 0);
                    if (hit != NULL && d < lbl_80346F98) {
                        *(u32*)&hit->data.raw[8] |= 1;
                    }
                }
                if (it->data.trigger.camid >= 0 && mask != 0) {
                    TriggerCamera* cam = &sItemRuntime.triggerCameras[it->data.trigger.camid];
                    TriggerCameraActivate((s32)tgt, cam->eye, cam->target,
                                          cam->subtype, 0x1E, 0);
                }
            }
            *(s16*)&it->atree[0x38] = 0;
            if (gTriggerCameraState == 0 &&
                !(it->data.trigger.flags & 0xC0)) {
                u8 pm = it->playermask;
                if (pm & 0xF) {
                    it->playermask = pm & ~0xF;
                } else {
                    it->playermask = 0;
                }
            }
            break;
        }
        case 0xC: {
            s32 sub = it->info->item.subtype;
            WorldObj* tgt = it->data.rot.target;
            if (sub == 0) {
                f32 ang;
                if (tgt == NULL) {
                    break;
                }
                if (lbl_80344500 != 0 && (tgt->flags & 4)) {
                    break;
                }
                ang = it->data.rot.speed * (f32)(u32)gFrameTicks;
                it->daction = 2;
                if (tgt->nodeptr == NULL) {
                    break;
                }
                tgt->flags &= ~1;
                YawMat3((f32*)tgt->nodeptr, ang);
            } else if (sub == 2) {
                if (tgt == NULL) {
                    break;
                }
                if (!(it->active & 1)) {
                    break;
                }
                if (it->active & 0x1000) {
                    break;
                }
                if ((it->data.rot.speed >= sItemZero &&
                     it->data.rot.curang >= it->data.rot.delta) ||
                    (it->data.rot.speed < sItemZero &&
                     it->data.rot.curang <= -it->data.rot.delta)) {
                    fn_8009D7E4(2, (f32*)((u8*)tgt->nodeptr + 0x30));
                    it->active |= 0x1000;
                    it->daction = 2;
                } else {
                    f32 ang;
                    fn_8009D7E4(0, (f32*)((u8*)tgt->nodeptr + 0x30));
                    ang = it->data.rot.speed * (f32)(u32)gFrameTicks;
                    it->data.rot.curang = it->data.rot.curang + ang;
                    it->daction = 2;
                    if (tgt->nodeptr != NULL) {
                        tgt->flags &= ~1;
                        YawMat3((f32*)tgt->nodeptr, ang);
                    }
                }
            } else if (sub == 1 && tgt != NULL) {
                if (lbl_80344768 > 1 && did_generate(tgt, 0) != 0) {
                    add_target(&it->objgrp);
                    lbl_80344960 = i;
                } else {
                    del_target(&it->objgrp);
                }
            }
            break;
        }
        case 9: {
            s32 pmask = 0;
            s32 count5 = 0;
            if (it->info->item.subtype == 0x32) {
                s32 b;
                s32 pbits = it->data.exit.onexit;
                s32 rec;
                if (pbits == 0) {
                    break;
                }
                if (lbl_8034481C >= 3) {
                    break;
                }
                if (it->active & 1) {
                    break;
                }
                for (b = 0; b < 4; b++) {
                    if (pbits & (1 << b)) {
                        break;
                    }
                }
                it->active |= 1;
                rec = (u8)it->data.exit.wave;
                lbl_8034481C = rec + 3;
                SaveAllRecords(i, b, gPlayers[b].pos, rec);
                init_got_it();
                break;
            } else {
                s32 b;
                for (b = 0; b < 4; b++) {
                    s32 st = gPlayers[b].state;
                    if (st == 1 || (u32)(st - 4) <= 1) {
                        pmask |= 1 << b;
                    }
                }
                it->active |= 1;
                it->active &= ~0x400;
                for (b = 0; b < 4; b++) {
                    if (gPlayers[b].state == 5) {
                        count5++;
                    }
                }
                if (count5 != 0) {
                    lbl_803447D0 = 0xF;
                } else if (pmask != 0 && it->data.exit.onexit == pmask) {
                    if ((s8)it->daction == 0) {
                        it->activetime = 0;
                    }
                    if (it->activetime <= 0 && (s8)it->daction < 4) {
                        it->daction++;
                    }
                    if ((s8)it->action < 4) {
                        it->data.exit.onexit = 0;
                    }
                    if ((s8)it->action == 4 && it->activetime <= 0) {
                        s32 n4 = 0;
                        for (b = 0; b < 4; b++) {
                            if (gPlayers[b].state == 4) {
                                break;
                            }
                            n4++;
                        }
                        if (n4 >= 4) {
                            lbl_803447D0 = 0xF;
                        } else {
                            lbl_803447D0 = 0xE;
                        }
                    } else {
                        lbl_803447D0 = (s8)it->action + 10;
                    }
                } else if (it->data.exit.onexit != 0) {
                    if (it->activetime <= 0) {
                        if ((s8)it->daction < 3) {
                            it->daction++;
                        } else if ((s8)it->daction > 3) {
                            it->daction = 0;
                        }
                    }
                    it->data.exit.onexit = 0;
                    if ((s8)it->action > lbl_803447D0) {
                        lbl_803447D0 = (s8)it->action;
                    }
                    it->paction = 2;
                } else {
                    if ((s8)it->action > 0) {
                        if (it->activetime <= 0) {
                            if ((s8)it->daction < 4) {
                                it->daction++;
                            } else {
                                it->daction = 0;
                            }
                        } else if ((s8)it->action == 3) {
                            it->activetime = 0;
                            it->daction = 4;
                            it->action = 4;
                        }
                    }
                    it->data.exit.onexit = 0;
                    if ((s8)it->action > lbl_803447D0) {
                        lbl_803447D0 = (s8)it->action;
                    }
                    it->paction = 0;
                }
                if ((s8)it->action == 3) {
                    if ((s8)it->paction == 2) {
                        it->activetime = 0x2D;
                    }
                    it->active |= 0x400;
                }
                if ((s8)it->daction == 3 || (s8)it->daction <= 1) {
                    *(s16*)&it->atree[0x38] = 1;
                } else {
                    *(s16*)&it->atree[0x38] = 0;
                }
                it->paction = it->action;
            }
            break;
        }
        case 0xD: {
            f32 range = it->data.sound.radius;
            f64 d;
            s16 sub;
            if (it->data.sound.parent != NULL) {
                GetWorldMat(it->data.sound.parent,
                            it->objgrp.worldmat[0], 0);
            }
            d = DistanceToClosestPlayer(it->objgrp.worldmat[3]);
            sub = it->data.sound.musicarea;
            if (sub > 0) {
                if (d < range) {
                    if (sub - 1 > musicIdx) {
                        musicState = it->data.sound.fade;
                        musicIdx = sub - 1;
                    }
                }
                break;
            }
            {
                f32 vol;
                if (d < range || range <= lbl_80346EF0) {
                    vol = sItemFloorRadius;
                } else {
                    vol = (f32)((lbl_80346EF0 *
                                 (lbl_80346FB8 * range - d)) /
                                range);
                }
                if (vol > sItemZero || it->data.sound.active < 0) {
                    s32 vmask = 0;
                    s32 vinst = 0;
                    s32 sw = it->data.sound.active;
                    if (sw < 0) {
                        vinst = -sw;
                    } else if (sw != 0) {
                        vmask = sw;
                    }
                    AudioSecretProc(vol, it->data.sound.sound,
                                    it->objgrp.worldmat[3],
                                    it->data.sound.fade, &vinst, &vmask);
                    if (vmask != 0) {
                        it->data.sound.active = vmask;
                    } else if (vinst != 0) {
                        it->data.sound.active = (s16)-vinst;
                    } else {
                        it->data.sound.active = 0;
                    }
                } else if (it->data.sound.active > 0) {
                    AudioStopAll();
                    it->data.sound.active = 0;
                }
            }
            break;
        }
        case 0xA: {
            s16 c = it->data.obsticle.flash;
            if (c > 1) {
                if (it->data.obsticle.timer < 0x1E) {
                    MBTreeSetAltTex(it->objgrp.node, -2,
                                    gWorldInfo.whitetex, 0);
                    MBTreeSetFlags(it->objgrp.node, 0x4000, 1);
                } else {
                    MBTreeSetAltTex(it->objgrp.node, -1, 0, 0);
                    MBTreeClearFlags(it->objgrp.node, 0x4000, 1);
                }
                it->data.obsticle.timer += gFrameTicks;
                if (it->data.obsticle.timer > 0x3C) {
                    it->data.obsticle.timer = 0;
                    it->data.obsticle.flash -= 1;
                    if (it->data.obsticle.flash == 1) {
                        it->data.obsticle.flash = 0;
                    }
                }
            } else if (c == 1) {
                MBTreeSetAltTex(it->objgrp.node, -2,
                                gWorldInfo.whitetex, 0);
                MBTreeSetFlags(it->objgrp.node, 0x4000, 1);
                it->data.obsticle.flash = 0;
                it->data.obsticle.timer = 0;
            } else {
                MBTreeSetAltTex(it->objgrp.node, -1, 0, 0);
                MBTreeClearFlags(it->objgrp.node, 0x4000, 1);
            }
            switch (it->info->item.subtype) {
            case 0x2C:
            case 0x2D:
                if ((s8)it->action == 1) {
                    if (it->activetime < 0x68) {
                        s32 alpha =
                            0x100 -
                            (s32)(lbl_803470A0 *
                                  (f64)(it->activetime - 0x10));
                        if (alpha > 0xFF) {
                            alpha = 0xFF;
                        }
                        MBTreeSetAlpha(it->objgrp.node, alpha, 1);
                    }
                } else if ((s8)it->action == 2) {
                    f32 best = lbl_80347000;
                    Player* p = gPlayers;
                    s32 b;
                    for (b = 0; b < 4; b++, p++) {
                        if (p->state == 1) {
                            f32 dy = it->objgrp.coll_pos[1] - p->pos[1];
                            f32 dx = it->objgrp.coll_pos[0] - p->pos[0];
                            f32 dz = it->objgrp.coll_pos[2] - p->pos[2];
                            f32 d2 = dy * dy;
                            volatile f32 root;
                            d2 = dx * dx + d2;
                            d2 = dz * dz + d2;
                            if (d2 > sItemZero) {
                                f64 estimate = __frsqrte(d2);
                                estimate =
                                    sArrowFloorYOffset * estimate *
                                    (sNewtonThree - estimate * estimate * d2);
                                estimate =
                                    sArrowFloorYOffset * estimate *
                                    (sNewtonThree - estimate * estimate * d2);
                                estimate =
                                    sArrowFloorYOffset * estimate *
                                    (sNewtonThree - estimate * estimate * d2);
                                root = (f32)(d2 * (sArrowFloorYOffset *
                                                   estimate *
                                                   (sNewtonThree -
                                                    estimate * estimate * d2)));
                                d2 = root;
                            }
                            if (d2 < best) {
                                best = d2;
                            }
                        }
                    }
                    if (best <= lbl_803470A8) {
                        s32 msg;
                        if (it->info->item.subtype == 0x2C) {
                            msg = 0x2C;
                        } else {
                            msg = 0x2D;
                        }
                        msgPost(msg, -1, (char*)it->objgrp.attn_pos);
                    }
                    KILL_ITEM(it);
                }
                break;
            case 0x33:
                if (sSafeRockCount != sPreviousSafeRockCount) {
                    switch (sSafeRockCount) {
                    case 1:
                        it->data.obsticle.vel[1] =
                            (f32)(lbl_803470B0 + Random(sItemSearchDistance));
                        break;
                    case 2:
                        it->data.obsticle.vel[1] =
                            (f32)(lbl_80346F88 + Random(lbl_803470B8));
                        break;
                    case 3: {
                        f32 dir[3];
                        dir[0] = it->objgrp.worldmat[3][0] -
                                 ((f32*)rt)[7546];
                        dir[1] = it->objgrp.worldmat[3][1] -
                                 ((f32*)rt)[7547];
                        dir[2] = it->objgrp.worldmat[3][2] -
                                 ((f32*)rt)[7548];
                        NormalVector2D(dir);
                        it->data.obsticle.vel[0] = (f32)(lbl_80346F98 * dir[0]);
                        it->data.obsticle.vel[1] = (f32)(lbl_80346F98 * dir[1]);
                        it->data.obsticle.vel[2] = (f32)(lbl_80346F98 * dir[2]);
                        it->data.obsticle.vel[1] =
                            (f32)(lbl_80347018 + Random(lbl_803470BC));
                        break;
                    }
                    }
                    it->active |= 1;
                }
                /* fallthrough */
            case 0x28:
            case 0x31:
            case 0x34:
            case 0x35: {
                f32 s2 = lbl_8011C904[i & 7];
                f32 s1 = lbl_8011C904[~i & 7];
                f32 ypr[3];
                if (!(it->active & 1)) {
                    break;
                }
                ExtractYPR(it->objgrp.worldmat[0], ypr);
                if (it->info->item.subtype == 0x31) {
                    it->data.obsticle.vel[1] =
                        (f32)(it->data.obsticle.vel[1] - lbl_80346EE8);
                    ypr[0] = (f32)(lbl_803470C0 * s2 * gClockFrameStep +
                                   ypr[0]);
                    ypr[2] = (f32)(lbl_803470C0 * s1 * gClockFrameStep +
                                   ypr[2]);
                } else if (it->info->item.subtype == 0x35) {
                    it->data.obsticle.vel[1] =
                        (f32)(it->data.obsticle.vel[1] - lbl_80346EF0);
                    ypr[0] = (f32)(lbl_803470C8 * s2 * gClockFrameStep +
                                   ypr[0]);
                    ypr[2] = (f32)(lbl_803470C8 * s1 * gClockFrameStep +
                                   ypr[2]);
                } else {
                    it->data.obsticle.vel[1] =
                        (f32)(it->data.obsticle.vel[1] - lbl_80346EF0);
                    ypr[0] = (f32)(lbl_803470D0 * s2 * gClockFrameStep +
                                   ypr[0]);
                    ypr[2] = (f32)(lbl_803470D0 * s1 * gClockFrameStep +
                                   ypr[2]);
                }
                CreateYPRMatrix(it->objgrp.worldmat[0], ypr);
                it->objgrp.worldmat[3][0] +=
                    it->data.obsticle.vel[0] * gClockFrameStep;
                it->objgrp.worldmat[3][1] +=
                    it->data.obsticle.vel[1] * gClockFrameStep;
                it->objgrp.worldmat[3][2] +=
                    it->data.obsticle.vel[2] * gClockFrameStep;
                UpdateObjWorldMat(&it->objgrp);
                if (it->objgrp.worldmat[3][1] <
                    lbl_80344880 - lbl_803470D8) {
                    KILL_ITEM(it);
                }
                break;
            }
            }
            break;
        }
        }
    }

    if (musicIdx >= 0 && musicIdx != sMusicSubIndex) {
        sMusicSubIndex = musicIdx;
        sMusicSubState = musicState;
    }
    fn_80062A00();
    sPreviousSafeRockCount = sSafeRockCount;
}

#undef sArrowFloorYOffset
#undef sNewtonThree
#undef sZeroDouble
#undef sPi
#undef sTwoPi
#undef sNegativePi
#undef lbl_80346EE8
#undef lbl_80346FB8
#undef lbl_80347018
#undef sItemZero
#undef sItemFloorRadius
#undef sItemSearchDistance
#undef sCameraVisibilityRadius
#undef sArrowFloorRadius

extern char* strcpy(char* d, const char* s);

extern s32 damage_enemy(u8* e, f32 amount, s32 dtype, s32 knock, s32 srcflags,
                        s32 arg6, s32 arg7);

s32 fn_8005D3D8(s32 index, u8* wobj)
{
    u8* hdr = *(u8**)wobj;
    s32 ret;
    u8* sub = hdr + 4;
    u8* e;
    s32 t;
    s32 bval;

    if (index >= 0) {
        e = (u8*)&gEnemies[index];
    } else {
        e = 0;
    }
    ret = 1;
    switch (*(u32*)hdr) {
    case 1:
        ret = 0;
        break;
    case 10:
        switch (*(s32*)sub) {
        case 40:
        case 49:
        case 51:
        case 52:
        case 53:
            ret = 0;
            break;
        case 41:
            if (*(s16*)(wobj + 222) > 0) {
                ret = 1;
            }
            break;
        default:
            ret = 1;
            break;
        }
        break;
    case 2:
        if (e == 0) {
            break;
        }
        if (*(s32*)e == 29 || *(s32*)e == 32) {
            ret = 0;
        }
        break;
    case 3:
        if (e == 0) {
            break;
        }
        if (*(s8*)(wobj + 226) != 0) {
            t = 0;
        } else {
            t = 1;
        }
        if (t != 0) {
            bval = 0;
        } else {
            bval = 1;
        }
        ret = bval;
        if (*(s16*)(wobj + 220) == 17) {
            ret = 0;
        }
        if (*(s32*)e == 29 || *(s32*)e == 32) {
            if (*(f32*)(hdr + 16) <= sNewtonThree) {
                ret = 0;
            }
        }
        break;
    case 8:
        if (e == 0) {
            break;
        }
        t = *(s32*)e;
        if (t == 30) goto case8_blocked;
        if (t == 29) goto case8_blocked;
        if (t != 32) goto case8_damage_check;
case8_blocked:
        ret = 0;
        break;
case8_damage_check:
        if (t == 3 || t == 0) {
            s32 b = *(s8*)(wobj + 200);
            if (b == 2) goto dmg;
            if (b != 4) goto nodmg;
dmg:
            damage_enemy(e, *(f32*)(wobj + 220), -1, 0, (s32)(e + 68), 0, 2);
nodmg:
            ret = 0;
        }
        break;
    case 5:
    case 9:
    case 11:
    case 12:
        ret = 0;
        break;
    }
    return ret;
}

/* 0x8005D5C8 - classify a world object for a player (jumptable pair) */
s32 fn_8005D5C8(u8* pl, u8* wobj)
{
    u8* hdr = *(u8**)wobj;
    s32 ret = 1;
    u8* sub = hdr + 4;
    s32 cls = *(s16*)(*(u8**)(*(u8**)(pl + 4) + 288) + 32);
    s32 t;

    switch (*(u32*)hdr) {
    case 1:
        ret = 0;
        break;
    case 10:
        switch (*(s32*)sub) {
        case 40:
        case 49:
        case 51:
        case 52:
        case 53:
            ret = 0;
            break;
        case 41:
            if (*(s16*)(wobj + 222) > 0) {
                ret = 1;
            }
            break;
        case 43:
        case 44:
        case 45:
            if (cls == 3 || cls == 7) {
                ret = 3;
            } else {
                ret = 1;
            }
            break;
        default:
            ret = 1;
            break;
        }
        break;
    case 2:
        ret = 1;
        if (*(s32*)sub == 43) {
            if (cls == 3 || cls == 7) {
                ret = 3;
            }
        } else {
            if (cls == 3 || cls == 7) {
                ret = 2;
            }
        }
        break;
    case 3:
        if (*(s8*)(wobj + 226) != 0) {
            t = 0;
        } else {
            t = 1;
        }
        ret = (t != 0) ? 0 : 1;
        if (*(f32*)(hdr + 16) <= sNewtonThree) {
            if (cls == 3 || cls == 7) {
                ret = 3;
            } else {
                ret = 0;
            }
        }
        break;
    case 8:
        ret = 0;
        break;
    case 5:
    case 9:
    case 11:
    case 12:
        ret = 0;
        break;
    }
    return ret;
}

/* 0x8005D20C - track/find the world object ahead of an enemy (cached in
 * e+652 with a rescan timer at e+812) */
s32 fn_8005D20C(s32 index, f32* from, f32* to, s32 ticking)
{
    u8* e = (u8*)&gEnemies[index];
    u32 obj;
    s32 blocked;
    f32 rad;
    f32 hit;
    f64 d;

    rad = (f32)(sArrowFloorYOffset * *(f32*)(e + 568));
    obj = 0;
    blocked = 0;
    if (ticking == 0 && *(u32*)(e + 652) != 0) {
        d = fn_8005F0F4((Item*)*(u32*)(e + 652), from, to, 0, rad,
                        (f32)(lbl_80346FB8 * rad));
        obj = (d >= sZeroDouble) ? *(u32*)(e + 652) : 0;
    } else {
        if (ticking != 0) {
            *(s32*)(e + 812) = *(s32*)(e + 812) - gFrameTicks;
        }
        if (*(s32*)(e + 812) <= 0) {
            obj = (s32)fn_80062FF0(rad, to, 0, (f32*)0, &hit);
            hit = hit - rad;
            if (hit > sItemZero) {
                s32 t1 = (s32)(hit * (sArrowFloorYOffset * *(f32*)(e + 184)));
                s32 t2 = (s32)(hit * (sArrowFloorYOffset * *(f32*)(e + 184)));
                *(s32*)(e + 812) = (t2 < 30) ? t1 : 30;
            }
            if (obj != 0) {
                d = fn_8005F0F4((Item*)obj, from, to, 0, rad,
                                (f32)(lbl_80346FB8 * rad));
                obj = (d >= sZeroDouble) ? obj : 0;
            }
        }
    }
    if (obj != 0) {
        blocked = fn_8005D3D8(index, (u8*)obj);
    }
    if (blocked != 0) {
        *(s32*)(e + 652) = obj;
    } else {
        *(s32*)(e + 652) = 0;
    }
    return blocked;
}

extern char lbl_80112C68[];            /* "COL OBJECT Item: idx < 0" */
extern f32 lbl_8023F7E8[3];
extern f32 lbl_8023F7F8[3];
/* gWorldInfo: game/worldinfo.h (WorldInfo, 0x8028CA8C, size 0xA4) */
extern s32 lbl_80344188;
extern char lbl_8034418C;
extern f32 lbl_80344190;
extern f32 lbl_80344194;
f32 CTriListCollide(f32 radius, s32 base, s32 count, u8** outTri,
                    s16* idxList, f32* outPt, s32 layerLo, s32 layerHi,
                    s32 noFilter);
void MulBodyVecMat4(const f32* vector, f32* out, const f32* matrix);
void MulVecMat4(const f32* vector, f32* out, const f32* matrix);

/* 0x8005FDA8 - sweep an item's collision tri list along segment a->b */
f32 fn_8005FDA8(u8* e, f32* a, f32* b, f32* outPos, f32* outNorm, f32 margin)
{
    f32* m = (f32*)((Item*)e)->objgrp.node;
    f32 pt[5];
    u8* triOut;
    f32 lo;
    f32 hi;
    f32 hit;
    char v;
    f32 k;
    f64 d1;
    f64 d2;

    if (*(s16*)(e + 192) < 0) {
        FatalError(lbl_80112C68, 0x800000);
    }
    if (a[1] < b[1]) {
        lo = a[1] - margin;
        hi = b[1] + margin;
    } else {
        lo = b[1] - margin;
        hi = a[1] + margin;
    }
    *(s32*)((u8*)&gWorldInfo + offsetof(WorldInfo, checknum)) =
        *(s32*)((u8*)&gWorldInfo + offsetof(WorldInfo, checknum)) + 1;
    if (*(s32*)((u8*)&gWorldInfo + offsetof(WorldInfo, checknum)) > 255) {
        *(s32*)((u8*)&gWorldInfo + offsetof(WorldInfo, checknum)) = 1;
    }
    v = (char)*(s32*)((u8*)&gWorldInfo + offsetof(WorldInfo, checknum));
    lbl_80344188 = 0;
    lbl_8034418C = v;
    MulBodyVecMat4(a, lbl_8023F7F8, m);
    MulBodyVecMat4(b, lbl_8023F7E8, m);
    k = m[5];
    d1 = 64.0 * (k * (lo - m[13]));
    d2 = 64.0 * (k * (hi - m[13]));
    lbl_80344194 = -0.5f;
    lbl_80344190 = sCameraVisibilityRadius;
    hit = CTriListCollide(margin, *(s16*)(e + 192), *(s16*)(e + 194), &triOut,
                          (s16*)0, pt, (s16)(s32)d1, (s16)(s32)d2, 0);
    if (hit >= 0.0) {
        if (outNorm != 0) {
            WorldVector((f32*)(triOut + 8), outNorm, m);
        }
        if (outPos != 0) {
            MulVecMat4(pt, outPos, m);
        }
    }
    return hit;
}

extern char lbl_80112C84[];            /* "Special trigger has no target" */
s16* FindWobjWanim(void* wobj);

/* Parallel column arrays over sItemRuntime (.bss 0x802577F0, size 0xBB8 =
 * 3000 = 5 columns * 150 f32).  The object[] column lives past the symbol's
 * own 3000 bytes but is addressed off the same single base relocation, so it
 * stays part of this view rather than becoming a second symbol. */
typedef struct ItemWobjRuntime {
    f32 y[150];         /* +0     current offset from initialY            */
    f32 initialY[150];  /* +600   resting Y the offset is added to        */
    f32 openY[150];     /* +1200  target offset when st & 0x20 is clear   */
    f32 closedY[150];   /* +1800  target offset when st & 0x20 is set     */
    f32 dist[150];      /* +2400  compared against sItemSearchDistance    */
    u8 _pad[26216];     /* +3000                                          */
    u32 object[150];    /* +29216 WorldObj* per wobj                      */
} ItemWobjRuntime;

/* Fire all special triggers of the given class.
 * dont_inline: the target calls FindWobjWanim (0x80055CB8) out of line from
 * here.  That function used to live in another TU's source slice, so nothing
 * could inline it; now that the split map puts it in this file, -inline auto
 * folds it in and this function stops matching (measured 2026-09-04:
 * 100.0000% -> 77.3853% without this bracket). */
#pragma dont_inline on
void ActivateSpecialTrigger(s32 type, s32 flag)
{
    s32 i;
    Item* it;
    Item* w;
    WorldObj* obj;
    u8* entry;
    s32 n;
    s32 j;
    ItemWobjRuntime* rt;

    it = sItems;
    rt = (ItemWobjRuntime*)&sItemRuntime;
    for (i = 0; i < sNumItems; i++, it++) {
        if (it->active != -1 && (it->active & 0x8100) == 0 &&
            it->info->type == 5 &&
            (u8)it->data.trigger.id == type) {
            w = it;
            while (w != 0) {
                w->active |= 0x400;
                w->action = 2;
                w->daction = 2;
                obj = w->data.trigger.target;
                if (obj != 0) {
                    obj->triggerstate = 47;
                    if (obj->flags & 0x2000000) {
                        s16* wa = FindWobjWanim(obj);
                        obj->ptriggerstate = 47;
                        obj->triggerstate = 47;
                        obj->flags |= 0x200000;
                        if (flag != 0) {
                            obj->flags |= 0x800000;
                            if (wa != 0) {
                                *(f32*)(wa + 4) = wa[1] - 1;
                            }
                        }
                    } else {
                        if (flag != 0) {
                            n = sNumItemWobjs;
                            for (j = 0; j < n; j++) {
                                entry = (u8*)rt;
                                entry += j * 4;
                                if (*(WorldObj**)(entry + 29216) == obj) {
                                    break;
                                }
                            }
                            if (j < n) {
                                f32 v;

                                entry = (u8*)rt;
                                entry += j * 4;
                                v = *(f32*)(entry + 1800);
                                rt->y[j] = v;
                                *(f32*)((u8*)obj->nodeptr + 52) = v;
                            }
                        }
                    }
                } else {
                    ErrorPrintf(lbl_80112C84);
                }
                w = w->data.trigger.next;
            }
        }
    }
}
#pragma dont_inline reset

extern f32 lbl_80347014;
extern f32 lbl_803447D8;
extern void MBTreeSetScale(void* node, f32 x, f32 y, f32 z);
extern u8* CritterNewInst(s32 type, s32 sub, void* mat);
extern f32 atan2(f32 y, f32 x);
extern void CreateYPRMatrix(f32* mtx, f32* pyr);
extern char lbl_80112CA4[];
extern char lbl_80112CD4[];
extern char* lbl_8011C8F0[];

extern f64 lbl_80347100;
extern f64 lbl_803470F0;
extern f64 lbl_803470F8;
extern f32 lbl_803470E8;
extern s32 lbl_80344A28;
extern s32 did_generate(void* w, s32 a);
extern void AudioBridgeClose(f32* pos);
extern void AudioBridgeOpen(f32* pos);
extern void AudioWorldObjectMotion(f32* pos, s32 d);
extern s32 fn_8009D694(s32 mode, f32* pos, s32 d);
extern void WorldPsysActivate(void* w);
extern void WorldPsysDeActivate(void* w);
extern void MBTreeSetAlpha(void* node, s32 alpha, s32 mode);

/* 0x80062A00 - per-frame world-object motion pump: bridge/door audio cues,
 * fade-in/out alpha ramps, door open/close latching, continuous sliders. */
void fn_80062A00(void)
{
    u8* rt;
    u8* row;
    WorldObj* w;
    mbnode* node;
    s32 heard;
    s32 i;
    s32 off;
    s32 st;
    s32 prev;
    s32 gen;
    s32 kind;
    s32 flags8;
    s32 act;
    s32 a;
    s32 on;
    u32 fl16;
    u32 fl;
    f32 pos[3];
    f32 dcur;
    f32 delta;
    f32 step;
    f64 kHi;
    f64 kRate;
    f64 kLo;
    f32 dist;
    f32 kMoving;
    f32 zero;

    rt = (u8*)&sItemRuntime;
    kHi = lbl_80347100;
    kRate = lbl_803470F0;
    kLo = lbl_803470F8;
    dist = sItemSearchDistance;
    kMoving = lbl_803470E8;
    zero = sItemZero;
    heard = 0;
    i = 0;
    off = 0;
    for (; i < sNumItemWobjs; i++, off += 4) {
        row = rt + off;
        w = *(WorldObj**)(row + 29216);
        st = w->triggerstate;
        prev = w->ptriggerstate;
        gen = did_generate(w, 1);
        pos[0] = w->nodeptr->mat[3][0];
        pos[1] = w->nodeptr->mat[3][1];
        pos[2] = w->nodeptr->mat[3][2];
        dcur = *(f32*)(row + 2400);
        kind = w->triggertype & 0xFF;
        flags8 = (w->triggertype >> 8) & 0xFF;
        if (dcur >= zero && lbl_80344A28 == 0 && lbl_803447B8 == 0) {
            if (kind == 20 || kind == 22) {
                if ((st ^ prev) & 0x20) {
                    if (st & 0x20) {
                        AudioBridgeClose(pos);
                    } else {
                        AudioBridgeOpen(pos);
                    }
                }
            } else if (dcur >= dist) {
                if (kMoving == dcur) {
                    if ((st & 0x20) && !(prev & 0x20) &&
                        (w->flags & 0x00C00000)) {
                        AudioWorldObjectMotion(pos, (s32)(dcur - dist));
                    }
                } else if ((st ^ prev) & 0x10) {
                    AudioWorldObjectMotion(pos, (s32)(dcur - dist));
                }
            } else {
                if (st & 0x10) {
                    if (heard == 0) {
                        heard = fn_8009D694(0, pos, (s32)dcur);
                    }
                } else if (prev & 0x10) {
                    fn_8009D694(2, pos, (s32)dcur);
                }
            }
        }
        w->ptriggerstate = st;
        w->nocol = 0;
        fl16 = w->flags;
        if (fl16 & 0x800) {
            act = 0;
            if (st & 0x20) {
                WorldPsysActivate(w);
            } else {
                WorldPsysDeActivate(w);
            }
        } else if (flags8 & 0x10) {
            if (flags8 & 0x20) {
                if (st & 47) {
                    on = 1;
                }
            } else {
                if (!(st & 0xF)) {
                    on = 1;
                }
            }
            w->nocol = (s8)(on != 0 ? 255 : 0);
            act = 0;
            node = w->nodeptr;
            if (node == NULL) {
                goto tail;
            }
            if (node->flags & 0x200) {
                a = 255 - node->alpha;
            } else {
                a = 0;
            }
            if (on != 0) {
                if (a >= 255) {
                    goto tail;
                }
                if (lbl_803447B8 != 0) {
                    a = 255;
                } else {
                    a = a + (gFrameTicks << 3);
                }
                if (a >= 248) {
                    MBTreeSetFlags(node, 2, 0);
                    MBTreeSetAlpha(w->nodeptr, 255, 1);
                } else {
                    MBTreeClearFlags(node, 2, 0);
                    MBTreeSetAlpha(w->nodeptr, a, 1);
                    act = 1;
                }
            } else {
                MBTreeClearFlags(node, 2, 0);
                if (a == 0) {
                    goto tail;
                }
                if (lbl_803447B8 != 0) {
                    a = 0;
                } else {
                    a = a - (gFrameTicks << 3);
                }
                if (a < 0) {
                    a = 0;
                    act = 1;
                } else {
                    act = 1;
                }
                MBTreeSetAlpha(w->nodeptr, a, 1);
            }
        } else if (fl16 & 0x02000000) {
            if (!(flags8 & 8) && gen >= 2) {
                w->flags = fl16 | 0x00300000;
                w->triggerstate = w->triggerstate & ~0x10;
                goto next;
            }
            act = 1;
            if (flags8 & 0x20) {
                if (st & 0xF) {
                    w->flags &= ~0x00300000;
                    st |= 48;
                } else if (fl16 & 0x00800000) {
                    w->flags |= 0x00300000;
                    st &= ~0x30;
                    act = 0;
                }
            } else {
                if (st & 0x20) {
                    w->flags |= 0x00200000;
                    w->flags &= ~0x00100000;
                } else {
                    w->flags |= 0x00100000;
                    w->flags &= ~0x00200000;
                }
                fl = w->flags;
                if (((fl & 0x00100000) && (fl & 0x00400000)) ||
                    ((fl & 0x00200000) && (fl & 0x00800000))) {
                    act = 0;
                }
            }
        } else {
            if (!(flags8 & 8) && gen >= 2) {
                w->nodeptr->mat[3][1] =
                    *(f32*)(row + 600) + *(f32*)row;
                goto next;
            }
            if (st & 0x20) {
                delta = *(f32*)(row + 1800) - *(f32*)row;
            } else {
                delta = *(f32*)(row + 1200) - *(f32*)row;
            }
            step = (f32)(kRate * gClockFrameStep);
            act = 1;
            if (delta > kLo) {
                if (delta > step) {
                    delta = step;
                }
            } else if (delta < kHi) {
                if (delta < -step) {
                    delta = -step;
                }
            } else {
                if (flags8 & 0x20) {
                    st ^= 0x20;
                } else {
                    act = 0;
                }
            }
            if (act != 0) {
                *(f32*)row = *(f32*)row + delta;
                w->flags |= 0x08000000;
            } else {
                w->flags &= ~0x08000000;
            }
            w->nodeptr->mat[3][1] = *(f32*)(row + 600) + *(f32*)row;
        }
    tail:
        if (act != 0) {
            if (!(flags8 & 8)) {
                w->nocol = 1;
            }
            if (!(st & 0x10)) {
                st |= 0x10;
            }
            w->flags |= 0x20000000;
        } else {
            st &= ~0x30;
            w->flags &= ~0x20000000;
        }
        if (!(flags8 & 71)) {
            if (flags8 & 0x10) {
                if (gen == 0) {
                    st = 0;
                }
            } else {
                st = 0;
            }
        }
        w->triggerstate = (s8)st;
    next:;
    }
    if (heard == 0) {
        fn_8009D694(-1, 0, 0);
    }
}

/* 0x80060114 - convert a pending enemy-spawn item into a live critter or
 * generated enemy once it becomes visible, then retire the item slot. */
void fn_80060114(Item* item, f32* pos, f32* dir)
{
    Item* it = item;
    enemydata* sp;
    s32 kind;
    Critter* crit;
    Enemy* e;
    s32 g;
    s32 idx;
    f32 root;
    f32 d2;
    u8 unused[40];

    sp = &it->data.enemy;
    kind = sp->etype;
    if (kind < 0) {
        return;
    }
    if ((gGameMode & MODE_GROUP_ATTRACT) && (u32)gGameMode != MA_DEMO &&
        (u32)gGameMode != MA_INSTRUCT) {
        return;
    }
    if (lbl_8034488C == 0) {
        return;
    }
    if (gGameOptions.gen_active == 0) {
        return;
    }
    if (MBWorldSphereVisible3(it->objgrp.worldmat[3],
                              lbl_80347014 * it->visrad) == 0) {
        return;
    }
    if (it->data.enemy.etype == 31 && gNumPlayers <= 1) {
        return;
    }
    {
        f32 dy = gCameras[0].attn[1] - it->objgrp.worldmat[3][1];
        f32 dx = gCameras[0].attn[0] - it->objgrp.worldmat[3][0];
        f32 dz = gCameras[0].attn[2] - it->objgrp.worldmat[3][2];
        d2 = dy * dy;
        d2 = dx * dx + d2;
        d2 = dz * dz + d2;
        if (d2 > sItemZero) {
            f64 estimate = __frsqrte(d2);
            estimate = sArrowFloorYOffset * estimate *
                       (sNewtonThree - estimate * estimate * d2);
            estimate = sArrowFloorYOffset * estimate *
                       (sNewtonThree - estimate * estimate * d2);
            estimate = sArrowFloorYOffset * estimate *
                       (sNewtonThree - estimate * estimate * d2);
            root = (f32)(d2 *
                         (sArrowFloorYOffset * estimate *
                          (sNewtonThree - estimate * estimate * d2)));
            d2 = root;
        }
        if ((f64)d2 > lbl_80347018) {
            return;
        }
    }
    if (kind == 29 || kind == 30 || kind == 32) {
        if (lbl_80346EE8 != (f64)lbl_803447D8 && it->objgrp.node != NULL) {
            MBTreeSetScale(it->objgrp.node, lbl_803447D8, lbl_803447D8,
                           lbl_803447D8);
        }
        if (!(it->data.enemy.flags & 1)) {
            return;
        }
        if (!(it->active & 1)) {
            it->active |= 1;
            it->daction = 1;
            return;
        }
        if (it->activetime > 0) {
            return;
        }
    }
    crit = 0;
    pos[0] = it->objgrp.coll_pos[0];
    pos[1] = it->objgrp.coll_pos[1];
    pos[2] = it->objgrp.coll_pos[2];
    dir[0] = it->objgrp.worldmat[2][0];
    dir[1] = it->objgrp.worldmat[2][1];
    dir[2] = it->objgrp.worldmat[2][2];
    switch (kind) {
    case 29:
        crit = (Critter*)CritterNewInst(3, 0, &it->objgrp);
        break;
    case 33:
        crit = (Critter*)CritterNewInst(8, 0, &it->objgrp);
        break;
    case 32:
        crit = (Critter*)CritterNewInst(7, 0, &it->objgrp);
        break;
    }
    if (crit != NULL) {
        if (*(u32*)it->atree != 0) {
            AtreeDelete(it->atree);
            *(s32*)it->atree = 0;
        }
        if (it->objgrp.node != NULL) {
            MBRemoveNode(it->objgrp.node, 0);
            it->objgrp.node = NULL;
        }
        it->active = -1;
        idx = it - sItems;
        if (idx < gNextItemIdx) {
            gNextItemIdx = idx;
        }
        if (sp->rad > sZeroDouble) {
            crit->visrad = sp->rad * gCurLevel->ene_visrad;
        }
        if (sp->pickup >= 0) {
            crit->gotitem = &sItems[sp->pickup];
        }
        return;
    }
    g = generate_enemy(pos, kind, sp->strength, dir, sp->ai,
                       0, 1, sItemFloorRadius);
    if (g >= 0) {
        f64 yaw;
        e = &gEnemies[g];
        e->birth_style = 1;
        e->endurance = 1;
        e->ang = atan2(dir[0], *(f32*)((u8*)dir + 32));
        yaw = e->ang;
        if (yaw > sPi) {
            yaw = yaw - sTwoPi;
        } else if (yaw <= sNegativePi) {
            yaw = sTwoPi + yaw;
        }
        e->ang = yaw;
        e->angbak = e->ang;
        e->pyr[1] = e->ang;
        {
            f32 mtx[12];
            CreateYPRMatrix(mtx, e->pyr);
            CopyMat3(mtx, e->objgrp.worldmat[0]);
        }
        UpdateObjWorldMat(&e->objgrp);
        e->birth_pos[0] = e->objgrp.worldmat[3][0];
        e->birth_pos[1] = e->objgrp.worldmat[3][1];
        e->birth_pos[2] = e->objgrp.worldmat[3][2];
        if (e->type != 31 && e->type != 30 && sp->strength == 0) {
            e->state = 6;
        } else if (sp->strength < 4) {
            e->stun_timer = 30;
        }
        if (e->type == 30) {
            e->sight = sNoNearbyPlayerDistance;
        } else if (sp->rad > sZeroDouble) {
            e->sight = sp->rad * gCurLevel->ene_visrad;
        }
        if (sp->interval > 0) {
            if (sp->interval == 1) {
                e->idle_time = sItemZero;
            } else {
                e->idle_time =
                    (f32)(sItemFloorYOffset * (f64)sp->interval);
            }
        }
        if (sp->pickup >= 0) {
            e->gotitem = &sItems[sp->pickup];
        }
    } else if (g > -99) {
        if (sp->strength >= 4 || e->type > 1) {
            if (g == -5) {
                ErrorPrintf(lbl_80112CA4, lbl_8011B578[kind],
                            sp->strength);
            } else {
                ErrorPrintf(lbl_80112CD4, lbl_8011B578[kind],
                            sp->ai, lbl_8011C8F0[-(g + 1)]);
            }
        }
    }
    if (*(u32*)it->atree != 0) {
        AtreeDelete(it->atree);
        *(s32*)it->atree = 0;
    }
    if (it->objgrp.node != NULL) {
        MBRemoveNode(it->objgrp.node, 0);
        it->objgrp.node = NULL;
    }
    it->active = -1;
    idx = it - sItems;
    if (idx < gNextItemIdx) {
        gNextItemIdx = idx;
    }
}

void AddItemSub(Item* item)
{
    u8 unused_before[4];
    f32 position[3];
    u8 unused_after[4];
    void** current;

    if (item == 0) {
        return;
    }
    if (item->objgrp.node == 0) {
        return;
    }
    if ((item->info->item.colflags & 1) != 0) {
        return;
    }

    position[0] = item->objgrp.worldmat[3][0];
    position[1] = item->objgrp.worldmat[3][1];
    position[2] = item->objgrp.worldmat[3][2];
    item->objgrp.worldmat[3][1] =
        sItemFloorYOffset + FloorPos(position[1], sItemFloorRadius,
                                   position, 0);

    current = &gFloorCollisionResult.current;
    if (*current == 0 && (gControllerButtons & 0x10) != 0) {
        ErrorPrintf(sBadItemFloorPosFmt, item->info->item.desc,
                    position[0], position[1], position[2]);
    }

    if (*current != 0 &&
        *(void**)((u8*)*current + offsetof(WorldObj, nodeptr)) != 0 &&
        (*(u32*)((u8*)*current + offsetof(WorldObj, flags)) & 0x1000) != 0) {
        MBNodeSetParent(item->objgrp.node,
                        *(void**)((u8*)*current + offsetof(WorldObj, nodeptr)));
    }

    UpdateObjWorldMat(&item->objgrp);
    switch (item->info->type) {
    case 5:
        break;
    default:
        goto done;
    }
    {
        void* linked;
        void* scene;

        if ((item->data.trigger.flags & 0x400) == 0) {
            goto done;
        }
        linked = item->data.trigger.target;
        if (linked == 0) {
            goto done;
        }
        scene = *current;
        if (scene == 0) {
            goto done;
        }
        if (linked != scene &&
            linked != *(void**)((u8*)scene + offsetof(WorldObj, parent))) {
            goto done;
        }
        item->data.trigger.flags |= 0x100;
    }
done:
    return;
}

void InitItemInfoData(void)
{
    u8* runtime = (u8*)&sItemRuntime;
    iteminfo* infos = gWorldInfo.iteminfo;
    s32 info_count = gWorldInfo.niteminfos;
    s32 i;
    s32 offset;

    sDeathItemInfo = 0;
    sChestAtree = 0;
    sDeathIconAtree = 0;
    sKeyringAtree = 0;

    if (sGoodWizObj != 0 || sItemFile1Buf != 0 || sPowerupsBuf != 0) {
        s32 j;

        for (j = 0, offset = 0; j < info_count; j++, offset += 0x50) {
            iteminfo* info;
            s32 also_wads;

            info = (iteminfo*)((u8*)infos + offset);
            also_wads = (info->type == 3) ? 1 : 0;

            info->item.atreeheader = AtreeMatchAnyHeader(
                info->item.desc, also_wads);
            if (info->type == 2 && info->item.subtype == 0x2F) {
                sDeathItemInfo = info;
            }
        }

        if (sGoodWizObj != 0) {
            sChestAtree = AtreeMatch(sGoodWizObj, sGoodWizardChestName, 0);
            sDeathIconAtree = AtreeMatch(sPowerupsBuf, sDeathIconName, 0);
            sKeyringAtree = AtreeMatch(sPowerupsBuf, sKeyringName, 0);
        }
    }

    {
        s32 overlay_offset;

        i = 0;
        overlay_offset = 0;
        offset = 0;
        do {
            u8* player_runtime;
            u8* overlay_runtime;
            mbnode** node_slot;
            void* node;

            node = MBNewNode(sItemsRootNode, 0, 4);
            player_runtime = runtime + offset;
            *(void**)(player_runtime + 0x74B8) = node;
            overlay_runtime = runtime;
            overlay_runtime += overlay_offset;
            *(s32*)(player_runtime + 0x74A8) = 0;
            *(s32*)(overlay_runtime + 0x74C8) = 0;
            *(node_slot = (mbnode**)(player_runtime + 0x7478)) =
                MBOX_NewObject(sSeeThroughObjectName, 0, sItemsRootNode,
                               0x04200000);
            MBTreeSetFlags(*node_slot, 1, 0);
            (*node_slot)->zmod = -800;
            i++;
            overlay_offset += 0x48;
            *(s32*)(player_runtime + 0x7498) = 0;
            offset += 4;
            *(s32*)(player_runtime + 0x7488) = 0;
        } while (i < 4);
    }
}

/* allocate the next free item slot, scanning from gNextItemIdx. */
Item* NewItemPtr(void)
{
    s16   gridnext;
    s32   i;
    Item* it;

    for (i = gNextItemIdx; i < sNumItems; i++) {
        if (sItems[i].active == -1) {
            break;
        }
    }
    if (i >= gMaxItems) {
        FatalError(sMaxItemsError, 0x800000);
    }
    if (i == sNumItems) {
        sNumItems++;
    }
    gNextItemIdx = i + 1;
    it = &sItems[i];
    gridnext = it->gridnext;
    memset(it, 0, 240);
    it->ctriidx = -1;
    it->gridnext = gridnext;
    return it;
}

/* Expand the level's compact iteminst records into the live item pool. */
void AddItemInstList(void)
{
    iteminst* instances = gWorldInfo.iteminst;
    s32 i = 0;
    s32 instance_count = gWorldInfo.niteminsts;
    s32 visible_sum_coins = 0;
    u8 frame_pad[8];
    f32 matrix[16];
    u8 unused[4];

    sItemRandSeed = pbLoad;
    SetPlayerVars();
    gMaxItems = instance_count + 500;
    sItems = AllocMem(gMaxItems * sizeof(Item));

    for (; i < instance_count; i++) {
        Item* item = NewItemPtr();
        iteminst* instance;
        Item* vis;

        if ((instance = &instances[i])->index < 0) {
            FatalError(sNewItemBadIndex, 0x800000);
        }
        CopyMat3(gIdentityMatrix, matrix);
        {
            f32 angle = instance->pyr[0];
            WPitchMat3(matrix, angle);
            angle = instance->pyr[1];
            WYawMat3(matrix, angle);
            angle = instance->pyr[2];
            WRollMat3(matrix, angle);
        }
        matrix[12] = instance->pos[0];
        matrix[13] = instance->pos[1];
        matrix[14] = instance->pos[2];
        SetItem(item, instance, &gWorldInfo.iteminfo[instance->index],
                matrix);
        vis = item;
        if (item != NULL) {
            iteminfo* info = item->info;

            if (info != NULL && info->type == 1 &&
                info->item.subtype == 1 && ItemVisible(vis)) {
                visible_sum_coins++;
            }
        }
    }
    sVisibleSumCoinCount = visible_sum_coins;
    fn_80062A00();
    {
        s32 k;
        for (k = 0; k < sNumItems; k++) {
            AddItemSub(&sItems[k]);
        }
    }
    MatchTransporters();
    LinkItemTriggers();
}

/* 0x800674F4 - match name against the weapon/powerup/item atrees, then all
 * wad headers when alsoWads is set. */
void* AtreeMatchAnyHeader(char* name, s32 alsoWads)
{
    void* r = NULL;

    if (name == NULL || *name == 0) {
        return 0;
    } else {
        if (sGoodWizObj != NULL) {
            r = AtreeMatch(sGoodWizObj, name, 0);
        }
        if (r == 0 && sPowerupsBuf != NULL) {
            r = AtreeMatch(sPowerupsBuf, name, 0);
        }
        if (r == 0 && sItemFile1Buf != NULL) {
            r = AtreeMatch(sItemFile1Buf, name, 0);
        }
        if (r == 0 && alsoWads != 0) {
            s32 i;
            for (i = 0; i < 45; i++) {
                if (gWadAtreeHeaders[i] != NULL) {
                    r = AtreeMatch(gWadAtreeHeaders[i], name, 0);
                    if (r != 0) {
                        break;
                    }
                }
            }
        }
    }
    return r;
}

/* pair up transporter items by matching each one's dest id to another's id. */
void MatchTransporters(void)
{
    s32   i;
    Item* p;
    s32   j;
    Item* q;

    p = sItems;
    for (i = 0; i < sNumItems; i++, p = (Item*)((u8*)p + 240)) {
        if (p->active != -1 && p->info->type == 11) {
            q = sItems;
            for (j = 0; j < sNumItems; j++, q = (Item*)((u8*)q + 240)) {
                if (j != i && q->active != -1 && q->info->type == 11 &&
                    ((s32*)p)[0x38] == ((s32*)q)[0x37]) {
                    ((Item**)p)[0x39] = q;
                    break;
                }
            }
            if (j >= sNumItems) {
                ErrorPrintf(sTransporterNoDestFmt, ((s32*)p)[0x37], ((s32*)p)[0x38]);
            }
        }
    }
}

/* Validate trigger identifiers and connect each trigger to its requested
 * successor.  The link occupies item data +8; bits 0x40 and 0x200 distinguish
 * special triggers and nodes that are link targets. */
void LinkItemTriggers(void)
{
    char* strings = (char*)&sObjectsFile_80112AB8;
    s32 i;
    Item* item;
    s32 j;
    Item* other;
    s32 nitems;

    {
    s32 i1;
    Item* it1;
    s32 j1;
    s32 dup1;
    s32 dup2;
    Item* ot1;

    it1 = sItems;
    for (i1 = 0; i1 < sNumItems; i1++, it1++) {
        if (it1->active != -1 && it1->info->type == 5) {
            for (dup1 = 0, j1 = 0, ot1 = sItems;
                 j1 < sNumItems; j1++, ot1++) {
                if (j1 != i1 && ot1->active != -1 &&
                    ot1->info->type == 5 &&
                    (ot1->data.trigger.flags & 0x40) ==
                        (it1->data.trigger.flags & 0x40) &&
                    it1->data.trigger.id > 0) {
                    if (ot1->data.trigger.id == it1->data.trigger.id) {
                        ot1->data.trigger.id = 0;
                        dup1++;
                    }
                    if (*(volatile s8*)&ot1->data.raw[7] ==
                        *(volatile s8*)&it1->data.raw[6]) {
                        dup2++;
                    }
                }
            }
            if (dup1 > 0) {
                if (it1->data.trigger.flags & 0x40) {
                    ErrorPrintf(strings + 0x2EC,
                                dup1 + 1,
                                (s32)it1->data.trigger.id);
                } else {
                    ErrorPrintf(strings + 0x310,
                                dup1 + 1,
                                (s32)it1->data.trigger.id);
                }
            }
        }
    }

    }

    item = sItems;
    for (i = 0; i < (nitems = sNumItems); i++, item++) {
        if (item->active != -1 && item->info->type == 5) {
            s8 next_id = item->data.trigger.nextid;

            if (next_id != 0) {
                other = sItems;
                for (j = 0; j < nitems; j++, other++) {
                    if (j != i && other->active != -1 &&
                        other->info->type == 5 &&
                        (other->data.trigger.flags & 0x40) == 0 &&
                        next_id == other->data.trigger.id) {
                        Item* next;
                        s32 loop = 0;
                        Item* chain;

                        for (chain = other; chain != NULL;
                             chain = next) {
                            next = chain->data.trigger.next;
                            if (next == item) {
                                ErrorPrintf(strings + 0x32C,
                                            (s32)next_id,
                                            (s32)other->data.trigger.nextid);
                                loop = 1;
                                break;
                            }
                        }
                        if (!loop) {
                            item->data.trigger.next = other;
                            other->data.trigger.flags |= 0x200;
                        }
                        break;
                    }
                }
                if (j >= sNumItems) {
                    ErrorPrintf(strings + 0x350,
                                (s32)item->data.trigger.id,
                                (s32)item->data.trigger.nextid);
                }
            }
        }
    }
}

/* tear down an item (and its parented anim item for type 1/2), freeing its
 * psys/node handles and rewinding the free-scan cursor. */
void DeleteItem(Item* item, s32 flag)
{
    u8* e;
    Item* ei;
    s32 idx;

    if (flag != 0) {
        if (item->info->type == 1 &&
            (ei = item->data.powerup.container) != NULL) {
            e = (u8*)ei;
            if (*(u32*)ei->atree != 0) {
                AtreeDelete(ei->atree);
                *(u32*)ei->atree = 0;
            }
            if (*(u32*)&ei->objgrp.node != 0) {
                MBRemoveNode(ei->objgrp.node, 0);
                *(u32*)&ei->objgrp.node = 0;
            }
            ei->active = -1;
            idx = (s32)(e - (u8*)sItems) / 240;
            if (idx < gNextItemIdx) {
                gNextItemIdx = idx;
            }
        }
        if (item->info->type == 2 &&
            (ei = item->data.powerup.container) != NULL) {
            e = (u8*)ei;
            if (*(u32*)ei->atree != 0) {
                AtreeDelete(ei->atree);
                *(u32*)ei->atree = 0;
            }
            if (*(u32*)&ei->objgrp.node != 0) {
                MBRemoveNode(ei->objgrp.node, 0);
                *(u32*)&ei->objgrp.node = 0;
            }
            ei->active = -1;
            idx = (s32)(e - (u8*)sItems) / 240;
            if (idx < gNextItemIdx) {
                gNextItemIdx = idx;
            }
        }
    }
    e = (u8*)item;
    if (*(u32*)item->atree != 0) {
        AtreeDelete(item->atree);
        *(u32*)item->atree = 0;
    }
    if (*(u32*)&item->objgrp.node != 0) {
        MBRemoveNode(item->objgrp.node, 0);
        *(u32*)&item->objgrp.node = 0;
    }
    item->active = -1;
    idx = (s32)(e - (u8*)sItems) / 240;
    if (idx < gNextItemIdx) {
        gNextItemIdx = idx;
    }
}

/* look up an item definition by name (+ type / optional level) and spawn it. */
static s32 FindInfoIndex(s32 type, s32 level, char* name)
{
    iteminfo* def;
    s32 i;

    def = gWorldInfo.iteminfo;

    for (i = 0; i < gWorldInfo.niteminfos; i++, def++) {
        iteminfodata* body = &def->item;
        if (strcmp(name, body->desc) != 0) {
            continue;
        }
        if (type != def->type) {
            continue;
        }
        if (level <= 0 || level == body->subtype) {
            return i;
        }
    }
    return -1;
}

Item* PlaceItem(s32 type, s32 level, char* name, void* matrix)
{
    u8 unused[8];
    iteminfo* d;
    s32 i;
    Item* item;

    i = FindInfoIndex(type, level, name);
    if (i < 0) {
        ErrorPrintf(sUnableToAddItemFmt, name);
        return NULL;
    }

    d = &gWorldInfo.iteminfo[i];
    item = NewItemPtr();
    if (matrix != NULL) {
        SetItem(item, 0, d, matrix);
        AddItemSub(item);
    } else {
        SetItem(item, 0, d, gIdentityMatrix);
    }
    return item;
}

/* allocate an item and initialise it from a1; a2 supplies the transform. */
Item* AddItem(void* a1, void* a2)
{
    Item* item = NewItemPtr();

    if (a2 != NULL) {
        SetItem(item, 0, a1, a2);
        AddItemSub(item);
    } else {
        SetItem(item, 0, a1, gIdentityMatrix);
    }
    return item;
}

/* boss-specific fixup: re-hide a level prop for a couple of boss ids. */
void SafeRockSetup(void)
{
    WorldObj* obj;

    switch (gBossType) {
    case 42:
        sSafeRockCount++;
        break;
    case 41:
        obj = FindWORLDOBJ(sSafeRockBoss41ObjectName);
        if (obj != NULL && obj->nodeptr != NULL) {
            MBTreeSetFlags(obj->nodeptr, 1, 0);
        }
        break;
    case 44:
        obj = FindWORLDOBJ(sSafeRockBoss44ObjectName);
        if (obj != NULL && obj->nodeptr != NULL) {
            MBTreeSetFlags(obj->nodeptr, 1, 0);
        }
        break;
    }
}

/* GC-only safe-rock collector (no Xbox-PDB symbol): collect indices of
 * type-10 items in state 0x29 (up to max); flag hides them. */
s32 CollectSafeRocks(s32* out, s32 max, s32 flag)
{
    Item* it;
    s32 i;
    s32 count;

    count = 0;
    i = 0;

    while (i < sNumItems) {
        it = &sItems[i];
        if (it->info->type == 10 && it->data.obsticle.subtype == 0x29) {
            out[count] = i;
            if (flag != 0) {
                MBTreeSetFlags(it->objgrp.node, 1, 1);
                it->data.obsticle.strength = -1;
            }
            count++;
            if (count >= max) {
                break;
            }
        }
        i++;
    }
    return count;
}

/* item proximity/timer gate; returns 1 when the item should trigger. */
s32 generate_now(Item* it, f32* pos, s32 a3, s32 a4)
{
    u8* p = it->data.raw;
    s32 v;

    if ((gGameBusy | gScriptedCameraState) != 0) {
        return 0;
    }
    v = *(s16*)(p + 8);
    if (v > 0) {
        *(s16*)(p + 8) = v - gFrameTicks;
        return 0;
    }
    if (a3 <= 0) {
        return 0;
    }
    if (default_gen_count != 0 && a4 == 0) {
        return 0;
    }
    if (gGameMode == MG_ROUND_START) {
        return 0;
    }
    if (DistanceToClosestPlayer(pos) > ITEM_ACTIVE_DIST) {
        return 0;
    }
    return 1;
}

double DistanceToClosestPlayer(f32* position)
{
    f64 best;
    u8 unused[16];

    best = sNoNearbyPlayerDistance;
    if (gGameMode == MA_FLYBY) {
        f32 sphere[4];
        volatile f32 root;
        f32 distance;
        f32 dx;
        f32 dy;
        f32 dz;

        sphere[0] = position[0];
        sphere[1] = position[1];
        sphere[2] = position[2];
        sphere[3] = sItemZero;
        dy = gCameras[0].mat[3][1] - sphere[1];
        dx = gCameras[0].mat[3][0] - sphere[0];
        dz = gCameras[0].mat[3][2] - sphere[2];
        distance = dx * dx + dy * dy;
        distance = dz * dz + distance;
        if (distance > sItemZero) {
            f64 estimate = __frsqrte(distance);
            estimate = sArrowFloorYOffset * estimate *
                       (sNewtonThree - estimate * estimate * distance);
            estimate = sArrowFloorYOffset * estimate *
                       (sNewtonThree - estimate * estimate * distance);
            estimate = sArrowFloorYOffset * estimate *
                       (sNewtonThree - estimate * estimate * distance);
            root = (f32)(distance *
                         (sArrowFloorYOffset * estimate *
                          (sNewtonThree - estimate * estimate * distance)));
            distance = root;
        }
        if (distance < sCameraDistanceLimit &&
            MBWorldSphereVisible3(sphere, sCameraVisibilityRadius) != 0) {
            best = sItemSearchDistance;
        }
    } else {
        u8* player = (u8*)gPlayers;
        s32 i;

        for (i = 0; i < 4; i++, player += 0x335C) {
            if (((Player*)player)->state == 1) {
                volatile f32 root;
                f32 distance;
                f32 dx;
                f32 dy;
                f32 dz;

                dy = ((Player*)player)->pos[1] - position[1];
                dx = ((Player*)player)->pos[0] - position[0];
                dz = ((Player*)player)->pos[2] - position[2];
                distance = dx * dx + dy * dy;
                distance = dz * dz + distance;

                if (distance > sItemZero) {
                    f64 estimate = __frsqrte(distance);
                    estimate = 0.5 * estimate *
                               (3.0 - estimate * estimate * distance);
                    estimate = 0.5 * estimate *
                               (3.0 - estimate * estimate * distance);
                    estimate = 0.5 * estimate *
                               (3.0 - estimate * estimate * distance);
                    root = (f32)(distance *
                                 (0.5 * estimate *
                                  (3.0 - estimate * estimate * distance)));
                    distance = root;
                }
                if (distance < best) {
                    best = distance;
                }
            }
        }
    }
    return best;
}

/* is this object claimed by a selecting player (ret 2) or a live enemy (1)? */
s32 did_generate(void* owner, s32 checkEnemies)
{
    u8* player = (u8*)gPlayers;
    u8* enemy = (u8*)gEnemies;
    s32 i;

    for (i = 0; i < 4; i++, player += 13148) {
        s32 state = ((Player*)player)->state;
        if (state == 1 || state == 8 || PlayerSelecting(i) != 0) {
            if ((void*)((Player*)player)->floor_name2 == owner) {
                return 2;
            }
        }
    }
    if (checkEnemies != 0) {
        for (i = 0; i < gNumEnemies; i++, enemy += 916) {
            if (((Enemy*)enemy)->state == 1 &&
                ((Enemy*)enemy)->floor_wobj == owner) {
                return 1;
            }
        }
    }
    return 0;
}

/* record which trigger-camera an item-trigger of the given type links to. */
void LinkTriggerToCam(s32 idx, s32 type)
{
    u8*  base = (u8*)&sItemRuntime;
    s32  i;
    s16  sidx;
    Item* p;

    if (sMusicTrackHi == 13) {
        if (type > 200) {
            if (type == 201) {
                *(void**)(base + 5584) = (void*)(base + idx * 40 + 5652);
            }
            if (type == 202) {
                sWindowCameras[0] = (TriggerCamera*)(base + idx * 40 + 5652);
            }
            if (type == 203) {
                *(void**)(base + 5588) = (void*)(base + idx * 40 + 5652);
            }
            if (type == 204) {
                sWindowCameras[1] = (TriggerCamera*)(base + idx * 40 + 5652);
            }
            if (type == 205) {
                *(void**)(base + 5592) = (void*)(base + idx * 40 + 5652);
            }
            if (type >= 240 && type < 254) {
                *(void**)(base + type * 4 + 4456) = (void*)(base + idx * 40 + 5652);
            }
            if (type < 220) {
                return;
            }
            if (type >= 234) {
                return;
            }
            *(void**)(base + type * 4 + 4592) = (void*)(base + idx * 40 + 5652);
            return;
        }
        if (type == 198) {
            sCrystalCamera = (TriggerCamera*)(base + idx * 40 + 5652);
            return;
        }
        if (type >= 170 && type < 184) {
            *(void**)(base + type * 4 + 4848) = (void*)(base + idx * 40 + 5652);
            return;
        }
    }
    sidx = idx;
    p = sItems;
    for (i = 0; i < sNumItems; i++, p = (Item*)((u8*)p + 240)) {
        if (p->active != -1 && p->info->type == 5 &&
            p->data.trigger.id == type) {
            s16 cur = p->data.trigger.camid;
            if (cur >= 0) {
                ErrorPrintf(sTriggerCameraConflictFmt, i, cur, idx);
            }
            p->data.trigger.camid = sidx;
        }
    }
}

/* find the MB object for name, retrying with the "L1" then "ROOT" suffix
 * appended (Xbox PDB: ItemFindMBObjectL1, static; inlined on GC). */
static inline s32 ItemFindMBObjectL1(char* name)
{
    s32 object = MBOX_ReallyFindObject(name, -1, -1, -1);

    if (object < 0) {
        strcat(name, sLevelOneSuffix);
        object = MBOX_ReallyFindObject(name, -1, -1, -1);
    }
    if (object < 0) {
        strcat(name, sRootSuffix);
        object = MBOX_ReallyFindObject(name, -1, -1, -1);
    }
    return object;
}

/* 0x80063DB0 - retexture a damageable item by health tier (name + tier
 * digit, falling back to name+"L1"/"ROOT"), blanking it at tier 0. */
void AddItemWobj(Item* it)
{
    char buf[32];
    s32 hp = it->health;
    s32 base = it->info->item.hitpoints;
    s32 tier;

    if (hp == 0) {
        tier = 0;
    } else if (hp <= base) {
        tier = 1;
    } else if (hp <= base << 1) {
        tier = 2;
    } else {
        tier = 3;
    }
    if (tier != *(s16*)(it->data.raw + 2)) {
        s32 found;
        s32 tex;
        *(s16*)(it->data.raw + 2) = tier;
        sprintf(buf, sItemHealthTextureFmt, it->info->item.desc,
                *(s16*)(it->data.raw + 2));
        found = ItemFindMBObjectL1(buf);
        tex = found;
        if (found < 0) {
            MBTreeSetFlags(it->objgrp.node, 1, 1);
            *(s16*)(it->data.raw + 2) = -1;
        } else {
            MBSetObject(it->objgrp.node, tex);
            if (tier == 0) {
                it->active &= ~1;
                it->armor = -1;
            }
        }
        if (tier == 0) {
            StartFXNoLoop(30, (f32*)((u8*)it + 0x34));
        }
    }
}

/* (re)arm an item's combat stats from its info and (re)attach its wobj. */
void SafeRockActivate(s32 idx)
{
    Item* it = &sItems[idx];

    MBTreeClearFlags(it->objgrp.node, 1, 1);
    it->health = it->info->item.hitpoints * 3;
    *(s16*)&it->data.raw[2] = 0;
    it->armor = (s8)it->info->item.armor;
    AddItemWobj(it);
}

/* item is "hot": has hitpoints and a positive damage/state field. */
s32 SafeRockActive(s32 idx)
{
    Item* it = &sItems[idx];

    if (it->health > 0 && *(s16*)&it->data.raw[2] > 0) {
        return 1;
    }
    return 0;
}

/* minimum-player gating check for an item's opener requirement. */
s32 ItemVisible(Item* it)
{
    s32 val  = gNumPlayers;
    s32 raw_minp = it->minplayers;
    s32 minp = raw_minp;
    s32 useEq = 0;
    s32 visible;

    if (gGameMode & MODE_GROUP_ATTRACT) {
        val = 2;
    }
    if (raw_minp > 10) {
        useEq = 1;
        minp -= 10;
    }
    if (gGameOptions.items > 0) {
        val = gGameOptions.items;
        useEq = 1;
    }
    if (useEq) {
        visible = val == minp ? 1 : 0;
    } else {
        visible = val >= minp ? 1 : 0;
    }
    return visible;
}

/*
 * Logic 12 places successive enemies alternately beside their generator.
 * Once a side is unusable the phase advances; after both sides, subsequent
 * enemies retain their spawn point.
 */
void place_logic12(s8* data, s32 enemy_index)
{
    f32 matrix[16];
    u8 unused_middle[12];
    f32 transformed[3];
    f32 angles[3];
    f32 vector[3];
    u8 unused_before[4];
    Enemy* enemy = &gEnemies[enemy_index];
    f64 angle;
    f32 z;
    f32 y;
    f32 x;

    enemy->flag1 = 0;
    switch (data[4]) {
    case 0:
        angle = sHalfPi;
        angle += *(f32*)&data[16];
        x = enemy->objgrp.worldmat[3][0];
        y = enemy->objgrp.worldmat[3][1];
        z = enemy->objgrp.worldmat[3][2];
        if (angle > sPi) {
            angle -= sTwoPi;
        } else if (angle <= sNegativePi) {
            angle = sTwoPi + angle;
        }
        angles[0] = sItemZero;
        angles[1] = (f32)angle;
        angles[2] = sItemZero;
        CreateYPRMatrix(matrix, angles);
        vector[0] = sItemZero;
        vector[1] = sItemZero;
        vector[2] = sLogic12Distance;
        WorldVector(vector, transformed, matrix);
        enemy->dest[0] = x + transformed[0];
        enemy->dest[1] = y + transformed[1];
        enemy->dest[2] = z + transformed[2];
        if (check_vacancy(enemy_index, enemy->dest) != 0) {
            enemy->flag1 = 1;
        }
        break;
    case 1:
        angle = *(f32*)&data[16] - sHalfPi;
        z = enemy->objgrp.worldmat[3][0];
        y = enemy->objgrp.worldmat[3][1];
        x = enemy->objgrp.worldmat[3][2];
        if (angle > sPi) {
            angle -= sTwoPi;
        } else if (angle <= sNegativePi) {
            angle = sTwoPi + angle;
        }
        angles[0] = sItemZero;
        angles[1] = (f32)angle;
        angles[2] = sItemZero;
        CreateYPRMatrix(matrix, angles);
        vector[0] = sItemZero;
        vector[1] = sItemZero;
        vector[2] = sLogic12Distance;
        WorldVector(vector, transformed, matrix);
        enemy->dest[0] = z + transformed[0];
        enemy->dest[1] = y + transformed[1];
        enemy->dest[2] = x + transformed[2];
        if (check_vacancy(enemy_index, enemy->dest) != 0) {
            enemy->flag1 = 2;
        }
        break;
    default:
        enemy->dest[0] = enemy->objgrp.worldmat[3][0];
        enemy->dest[1] = enemy->objgrp.worldmat[3][1];
        enemy->dest[2] = enemy->objgrp.worldmat[3][2];
        enemy->flag1 = 4;
        break;
    }

    if (enemy->flag1 != 0) {
        data[2]++;
        data[4]++;
        enemy->birth_pos[0] = enemy->objgrp.worldmat[3][0];
        enemy->birth_pos[1] = enemy->objgrp.worldmat[3][1];
        enemy->birth_pos[2] = enemy->objgrp.worldmat[3][2];
        if (data[2] >= data[3]) {
            data[10] = 7;
            data[4] = 3;
        }
    }
}

/*
 * Spawn the one enemy described by a generator item.  The data union is the
 * PDB's generator payload: enemy type/level/spew at +0/+6/+7, generated count
 * at +2, and the generator angle at +0x10.
 */
void generate_single(Item* item, s32 algorithm, s32 important)
{
    u8* data = item->data.raw;
    f32 position[3];
    f32 direction[3];
    s32 enemy_index;
    Enemy* enemy;
    f64 angle;
    f32 zero;
    f32 radius;

    if (*(s8*)&data[2] != 0) {
        return;
    }

    radius = item->info->item.radius;
    position[0] = item->objgrp.coll_pos[0];
    position[1] = item->objgrp.coll_pos[1];
    position[2] = item->objgrp.coll_pos[2];
    direction[0] = item->objgrp.worldmat[2][0];
    direction[1] = item->objgrp.worldmat[2][1];
    direction[2] = item->objgrp.worldmat[2][2];

    enemy_index =
        generate_enemy(position, *(s16*)&data[0], *(s8*)&data[6],
                       direction, *(s8*)&data[7], item, important,
                       radius);
    if (enemy_index < 0) {
        return;
    }

    enemy = &gEnemies[enemy_index];
    enemy->birth_style = algorithm == 15 ? 2 : 0;
    enemy->algorithm = algorithm;
    if (algorithm == 11) {
        enemy->state = DECORATION;
    }

    enemy->ang = *(f32*)&data[0x10] + enemy->genang_offset;
    angle = enemy->ang;
    if (angle > sPi) {
        angle -= sTwoPi;
    } else if (angle <= sNegativePi) {
        angle = sTwoPi + angle;
    }
    enemy->ang = (f32)angle;
    enemy->angbak = enemy->ang;
    zero = sItemZero;
    enemy->pyr[0] = zero;
    enemy->pyr[1] = enemy->ang;
    enemy->pyr[2] = zero;
    enemy->birth_pos[0] = enemy->objgrp.worldmat[3][0];
    enemy->birth_pos[1] = enemy->objgrp.worldmat[3][1];
    enemy->birth_pos[2] = enemy->objgrp.worldmat[3][2];
    (*(s8*)&data[2])++;
}

/*
 * Construct one live item from a serialized placement and its static
 * descriptor.  The Item.data union is interpreted by info->type below.
 */
void SetItem(Item* item, iteminst* instance, iteminfo* info, f32* matrix)
{
    char name[36];
    char child_name[32];
    u8 stack_pad[20];
    char* strings = (char*)&sObjectsFile_80112AB8;
    iteminfo** infos = &gWorldInfo.iteminfo;
    iteminfo* info_base = *infos;
    ItemRuntime* runtime = &sItemRuntime;
    char** arrows = sArrowObjectNames;
    void* atree_header;
    s32 attach_geometry = 1;
    s32 type;
    s32 subtype;
    s32 item_index = (s32)(item - sItems);
    s32 i;
    s32 found;
    s32 vis;

    /* Random descriptors contain a count and an array of s16 info indices. */
    while (info->type == -1) {
        s32 count = *(s32*)((u8*)info + 4);
        s32 choice;

        if (count != 0) {
            u32 seed = (sItemRandSeed >> 5) + item_index;
            choice = seed - (seed / (u32)count) * count;
        } else {
            choice = 0;
        }
        sItemRandSeed += 439;
        info = &info_base[
            *(s16*)((u8*)info + 8 + choice * sizeof(s16))];
    }

    type = info->type;
    subtype = info->item.subtype;

    if (info->type == 1) {
        switch (subtype) {
        case 2:
            if (instance != NULL && *(s16*)&instance->params[0] > 1) {
                for (found = 0; found < gWorldInfo.niteminfos;
                     found++, info_base++) {
                    iteminfodata* body = &info_base->item;

                    if (strcmp(sKeyringName, body->desc) != 0) {
                        continue;
                    }
                    if (type != info_base->type) {
                        continue;
                    }
                    if (subtype > 0 && subtype != body->subtype) {
                        continue;
                    }
                    goto keyring_found;
                }
                found = -1;
keyring_found:
                info = &(*infos)[found];
                type = info->type;
                subtype = info->item.subtype;
            }
            break;
        case 12:
            ErrorPrintf(strings + 0x38C, matrix[12], matrix[13], matrix[14]);
            item->active = -1;
            return;
        case 15:
            if (sMusicTrackHi == 13 &&
                (gDemoMode != 0 ||
                 towerAllPlayersMetBossReq(1) != 0)) {
                item->active = -1;
                return;
            }
            break;
        }
    }

    item->info = info;
    item->coll_offset[0] = info->item.coloffset[0];
    item->coll_offset[1] = info->item.coloffset[1];
    item->coll_offset[2] = info->item.coloffset[2];
    item->coll_offset[1] += 1.0;
    {
        f32 radius = info->item.radius;
        f32 height = info->item.height;

        item->visrad =
            (f32)(2.0 * (f64)(radius > height ? radius : height));
    }
    item->objgrp.flags = 0;
    {
        f32 z;
        f32 x;
        u8 abs_pad[12];

        z = item->coll_offset[2];
        *(u32*)&z &= 0x7FFFFFFF;
        x = item->coll_offset[0];
        *(u32*)&x &= 0x7FFFFFFF;
        if ((f64)(x + z) < 0.01) {
            item->objgrp.flags = 2;
        }
    }
    item->ctriidx = instance != NULL ? instance->ctriidx : -1;
    item->nctris = instance != NULL ? instance->nctris : 0;
    item->active = info->item.activetype;
    item->activetime = 0;
    item->action = 0;
    item->paction = 0;
    item->daction = 0;
    item->minplayers = instance != NULL ? instance->minplayers : 0;
    if (instance != NULL && (instance->flags & 1)) {
        item->active |= 0x40;
    }
    item->playermask = 0;
    item->opener = -1;
    vis = 0;
    switch (ItemVisible((Item*)&item->info)) {
    case 0:
        vis = 1;
        break;
    }
    item->minoff = (s8)vis;

    atree_header = info->item.atreeheader;
    if (instance != NULL && instance->desc[0] != '\0') {
        strncpy(name, instance->desc, sizeof(instance->desc));
    } else {
        strcpy(name, info->item.desc);
    }
    item->armor = (s8)info->item.armor;
    item->health = info->item.hitpoints;

#define DATA_S16(off) (*(s16*)&item->data.raw[(off)])
#define DATA_U16(off) (*(u16*)&item->data.raw[(off)])
#define DATA_S32(off) (*(s32*)&item->data.raw[(off)])
#define DATA_U32(off) (*(u32*)&item->data.raw[(off)])
#define DATA_F32(off) (*(f32*)&item->data.raw[(off)])
#define DATA_S8(off)  (*(s8*)&item->data.raw[(off)])
#define DATA_U8(off)  (*(u8*)&item->data.raw[(off)])
#define params (instance->params)
#define PARAM_S16(off, fallback) \
    (instance != NULL ? *(s16*)&params[(off)] : (fallback))
#define PARAM_S32(off, fallback) \
    (instance != NULL ? *(s32*)&params[(off)] : (fallback))

    switch (type) {
    case 4:
    {
        void* loaded = NULL;
        DATA_S8(2) = (s8)PARAM_S16(0, 1);
        DATA_S8(3) = (s8)PARAM_S16(2, -1);
        DATA_S16(0) = (s16)EnemyDescType(info->item.desc);
        DATA_F32(4) = atan2(matrix[8], matrix[10]);
        DATA_S32(8) = 0;
        DATA_F32(12) =
            instance != NULL ? *(f32*)&params[4] : 0.0;
        DATA_S16(16) = PARAM_S16(8, 0);
        if (gGameOptions.testai != 0 || DATA_S8(3) < 0) {
            DATA_S8(3) = (s8)sEnemyDefaultAlgorithm[DATA_S16(0)];
        }
        attach_geometry = 0;
        DATA_S16(0) =
            (s16)GetEnemyType(DATA_S16(0), DATA_S8(2));

        if (DATA_S16(0) == 32) {
            if (CritterTypeLoaded(7, 0) != NULL) {
                void* header;
                item->active &= ~1;
                attach_geometry = 1;
                loaded = CritterTypeLoaded(7, 0);
                /* +0x120 = the critter type header's descriptor pointer and
                 * +0x28 below its model/atreelist handle (crit_type/crit_desc;
                 * the records are file-local to critter.c, so the offsets
                 * stay literal here) */
                header = *(void**)((u8*)loaded + 0x120);
                atree_header = (void*)AtreeMatch(
                    *(void**)((u8*)header + 0x28),
                    strings + 0x3A8, 1);
            }
            DATA_U8(2) = 1;
        } else if (DATA_S16(0) == 29) {
            void** hdr = &gWadAtreeHeaders[29];
            if (*hdr != NULL) {
                item->active &= ~1;
                attach_geometry = 1;
                atree_header =
                    (void*)AtreeMatch(*hdr, strings + 0x3B4, 1);
            } else if (CritterTypeLoaded(3, 0) != NULL) {
                void* header;

                item->active &= ~1;
                attach_geometry = 1;
                loaded = CritterTypeLoaded(3, 0);
                header = *(void**)((u8*)loaded + 0x120);
                atree_header = (void*)AtreeMatch(
                    *(void**)((u8*)header + 0x28),
                    strings + 0x3B4, 1);
            }
            DATA_U8(2) = 1;
        } else if (DATA_S16(0) == 30) {
            void** hdr = &gWadAtreeHeaders[30];
            if (*hdr != NULL) {
                item->active &= ~1;
                attach_geometry = 1;
                switch (DATA_S8(2)) {
                case 1:
                    DATA_U32(8) |= 1;
                    /* fallthrough */
                case 0:
                    atree_header = (void*)AtreeMatch(
                        *hdr, strings + 0x3C0, 1);
                    DATA_U8(2) = 1;
                    break;
                case 3:
                    DATA_U32(8) |= 1;
                    /* fallthrough */
                case 2:
                    atree_header = (void*)AtreeMatch(
                        *hdr, strings + 0x3D0, 1);
                    DATA_U8(2) = 2;
                    break;
                }
            }
        }
        break;
    }

    case 2:
        DATA_S16(0) = (s16)PARAM_S32(0, -1);
        DATA_S16(2) = 0;
        DATA_S32(8) = 0;
        DATA_S32(12) = 0;
        DATA_F32(4) = atan2(matrix[8], matrix[10]);
        DATA_S16(16) = PARAM_S16(4, 0);
        break;

    case 5:
    {
        u8* wobj = NULL;
        s32 trigger_flags;
        s16 wobj_index;

        if (instance != NULL) {
            switch (subtype) {
            case 20: trigger_flags = 0x10; break;
            case 21: trigger_flags = 8; break;
            case 22: trigger_flags = 0x12; break;
            case 23: trigger_flags = 10; break;
            case 25: trigger_flags = 0x804; break;
            case 26: trigger_flags = 2; break;
            case 27: trigger_flags = 0x80C; break;
            case 28: trigger_flags = 9; break;
            case 29: trigger_flags = 10; break;
            case 24:
            case 30:
            case 31:
            default:
                trigger_flags = *(s16*)&params[2] | 8;
                break;
            }
            subtype |= (trigger_flags & 0xFF) << 8;
            wobj_index = *(s16*)&params[0];
            if (wobj_index >= 0) {
                if (wobj_index >= gWorldInfo.nwobjs) {
                    ErrorPrintf(strings + 0x3E0,
                                gWorldInfo.nwobjs, wobj_index);
                } else {
                    u8* w = (u8*)gWorldInfo.wobjs + wobj_index * 0x3C;
                    wobj = w;
                    if (*(void**)(w + 0x28) == NULL) {
                        ErrorPrintf(strings + 0x3FC, wobj);
                        wobj = NULL;
                    }
                }
            }
            DATA_S32(0) = (s32)wobj;
            if (wobj != NULL) {
                if (((WorldObj*)wobj)->flags & 0x800) {
                    *(u32*)(wobj + offsetof(WorldObj, flags)) |= 0x10000000;
                }
                RegisterItemWobj(
                    wobj, (s16)subtype,
                    *(s16*)&params[8], *(s16*)&params[10],
                    *(s8*)&params[5]);
                wobj[0x17] = 0;
                wobj[0x16] = 0;
                ((WorldObj*)wobj)->flags |= 0x100000;
            }
            trigger_flags |= *(s16*)&params[2] & ~0xFF;
            DATA_S16(4) = (s16)trigger_flags;
            if (params[4] == 0xFF) {
                DATA_F32(12) = 0.01f;
            } else {
                DATA_F32(12) = (f32)(0.5 * (f64)params[4]);
            }
            DATA_U8(6) = params[6];
            DATA_U8(7) = params[7];
        } else {
            DATA_S32(0) = 0;
            DATA_U16(4) = 0;
            DATA_F32(12) = 0.0f;
            DATA_U8(6) = 0;
            DATA_U8(7) = 0;
        }
        DATA_S32(8) = 0;
        DATA_S16(16) = 0;
        DATA_S16(18) = -1;

        if (wobj != NULL && sMusicTrackHi == 13) {
            u32 flags = 0;
            u32 player_flags = 0;
            s32 activate = 0;
            s32 player;

            switch (DATA_U8(6)) {
            case 104:
            case 199:
                for (player = 0; player < 4; player++) {
                    u8* q = (u8*)gPlayers + player * 13148;
                    if (((Player*)q)->state != 0) {
                        u8* p = q;
                        s32 player_index;
                        flags |= towerGetLevelFlag(p, 8);
                        player_index = ((Player*)p)->character;
                        player_flags |=
                            *(u16*)(p + player_index * 240 + 8738);
                    }
                }
                if (flags & 1) {
                    activate = 1;
                }
                break;
            }

            if (activate != 0) {
                s16* anim = FindWobjWanim(wobj);
                wobj[0x17] = '/';
                wobj[0x16] = '/';
                ((WorldObj*)wobj)->flags |= 0xA00000;
                if (anim != NULL) {
                    *((f32*)anim + 2) = (f32)(anim[1] - 1);
                }
            }
        }
        break;
    }

    case 12:
        if (instance != NULL && *(s32*)&params[0] >= 0) {
            DATA_S32(0) =
                (s32)((u8*)gWorldInfo.wobjs +
                      *(s32*)&params[0] * 0x3C);
            DATA_F32(4) = *(f32*)&params[4];
        } else {
            DATA_S32(0) = 0;
            DATA_F32(4) = 0.0f;
        }
        DATA_F32(8) =
            instance != NULL ? *(f32*)&params[8] : 0.0;
        DATA_F32(12) = 0.0f;
        if (item->info->item.subtype != 2) {
            attach_geometry = 0;
        }
        break;

    case 3:
    {
        s32 enemy_type;
        s32 count_index;

        item->visrad *= 4.0;
        DATA_U8(2) = 0;
        DATA_S8(3) = (s8)PARAM_S16(4, 0);
        DATA_U8(11) = (u8)PARAM_S16(6, 0);
        DATA_U8(4) = 0;
        DATA_S8(7) = (s8)PARAM_S16(2, 0);
        enemy_type = EnemyDescType(info->item.desc);
        DATA_S16(0) = (s16)enemy_type;

        if (DATA_S16(0) == 3) {
            iteminfo* candidate = *infos;
            for (found = 0; found < gWorldInfo.niteminfos;
                 found++, candidate++) {
                iteminfodata* body = &candidate->item;

                if (strcmp("LOW", body->desc) != 0) {
                    continue;
                }
                if (type != candidate->type) {
                    continue;
                }
                if (subtype > 0 && subtype != body->subtype) {
                    continue;
                }
                goto low_item_found;
            }
            found = -1;
low_item_found:
            if (found >= 0) {
                item->info = &(*infos)[found];
            }
        }
        if (gGameOptions.testai != 0 || DATA_S8(7) < 0) {
            DATA_S8(7) = (s8)sEnemyDefaultAlgorithm[DATA_S16(0)];
        }
        DATA_S8(6) = (s8)PARAM_S16(0, 1);
        {
            s32 n = DATA_S8(6);
            if (n < 1) {
                ErrorPrintf(strings + 0x424, n,
                            matrix[12], matrix[13], matrix[14]);
                DATA_S8(6) = 1;
            }
        }
        DATA_S8(5) = -1;
        DATA_F32(12) = 0.0f;
        DATA_S16(8) = 0;
        DATA_U8(10) = 0;
        DATA_F32(16) = atan2(matrix[8], matrix[10]);
        item->activetime = 40;
        item->health *= DATA_S8(6);

        enemy_type = DATA_S8(6) - 1;
        if (enemy_type < 0) {
            goto count_index_done;
        }
        if (enemy_type > 2) {
            enemy_type = 2;
        }
        count_index = enemy_type;
count_index_done:
        if (DATA_S8(3) == 0) {
            s32* defaults = (s32*)((u8*)arrows + count_index * 4);
            DATA_S8(3) = (s8)defaults[12];
        }
        if (DATA_U8(11) == 0) {
            s32* defaults = (s32*)((u8*)arrows + count_index * 4);
            DATA_U8(11) = (u8)defaults[15];
        }
        DATA_S8(3) = (s8)(DATA_S8(3) *
                           gCurLevel->gen_max);
        DATA_U8(11) = (u8)(DATA_U8(11) *
                            gCurLevel->gen_rate);
        item->health = (s16)(item->health *
                              gCurLevel->gen_health);
        if (sMusicTrackHi == 5) {
            strcpy(item->info->item.desc, "CAT");
            DATA_S16(0) = -2;
            DATA_S8(6) = 2;
        } else if (sMusicTrackHi == 6) {
            strcpy(item->info->item.desc, "HEL");
            DATA_S16(0) = -3;
            DATA_S8(6) = 3;
        }
        DATA_S16(0) =
            (s16)GetEnemyType(DATA_S16(0), DATA_S8(2));
        if (stricmp(name, "BOSSGEN") != 0) {
            if (DATA_S16(0) < -1) {
                sprintf(name, strings + 0x17C, DATA_S8(6));
            } else {
                sprintf(name, strings + 0x18C,
                        EnemyTypePrefix(DATA_S16(0)), DATA_S8(6));
            }
        }
        atree_header = AtreeMatchAnyHeader(name, 1);
        break;
    }

    case 8:
    {
        s32 scaled;
        DATA_F32(0) = instance != NULL ?
                      (f32)*(s16*)&params[0] : 0.0f;
        if (DATA_F32(0) == 0.0f) {
            DATA_F32(0) = (f32)item->info->item.value;
        }
        DATA_S16(4) = PARAM_S16(2, 0);
        scaled = DATA_S16(4);
        if ((s16)scaled != 0) {
            scaled = -scaled;
            item->info->item.activeoff = (s16)(scaled * 3);
        }
        DATA_F32(0) *= gCurLevel->trap_damage;
        item->health = (s16)(item->health *
                              gCurLevel->gen_health);
        scaled = info->item.activeoff * 2;
        if (scaled == 0) {
            scaled = 0;
        } else if (scaled < 0) {
            scaled = -scaled;
            scaled = (scaled >> 1) + (s32)RandInt((u32)scaled);
        }
        item->activetime =
            (s16)(scaled * gCurLevel->trap_rate);
        break;
    }

    case 9:
        if (*(s32*)&params[0] != 0) {
            DATA_S16(0) = -1;
        } else {
            DATA_S16(0) = (s16)FindWave((const s8*)&params[4]);
        }
        DATA_S32(4) = 0;
        item->active &= ~1;
        item->active |= 0x40;
        break;

    case 11:
        DATA_S32(0) = PARAM_S32(0, 0);
        DATA_S32(4) = PARAM_S32(4, 0);
        DATA_S32(8) = 0;
        break;

    case 13:
        DATA_F32(0) =
            instance != NULL ? *(f32*)&params[0] : 0.0f;
        DATA_S16(12) = (s16)PARAM_S32(4, 0);
        if (instance != NULL && DATA_S16(12) == 0) {
            for (i = 0; i < strlen(instance->desc); i++) {
                instance->desc[i] = (char)toupper(instance->desc[i]);
            }
            DATA_S32(4) =
                AudioFindSound(instance->desc,
                               sizeof(instance->desc), 1);
        } else {
            DATA_S32(4) = -1;
        }
        DATA_S16(14) = 0;
        DATA_S16(16) = PARAM_S16(8, 0);
        DATA_S16(18) = PARAM_S16(10, 0);
        strncpy(item->info->item.desc, instance->desc, 16);
        DATA_S32(8) = FindWorldAnimNode(&matrix[12], 10.0f);
        attach_geometry = 0;
        item->active |= 0x40;
        break;

    case 10:
        DATA_S16(0) = PARAM_S16(0, 0);
        DATA_S16(2) = PARAM_S16(2, 1);
        if (DATA_S16(0) <= 0) {
            DATA_S16(0) = (s16)item->info->item.subtype;
        }
        switch (DATA_S16(0)) {
        case 40:
        case 49:
        case 51:
        case 52:
        case 53:
            item->active |= 0x40;
            break;
        }
        if (DATA_S16(0) == 41) {
            item->health *= DATA_S16(2);
            sprintf(name, sItemHealthTextureFmt,
                    item->info->item.desc, DATA_S16(2));
        }
        DATA_S16(4) = 0;
        DATA_S16(6) = 0;
        DATA_F32(8) = 0.0f;
        DATA_F32(12) = 0.0f;
        DATA_F32(16) = 0.0f;
        break;

    case 1:
        DATA_S32(0) = (s32)item->info->item.properties;
        DATA_S32(4) = item->info->item.value;
        DATA_F32(8) = (f32)item->info->item.activeon;
        DATA_S32(12) = 0;
        DATA_S16(16) = 0;
        {
        s32 st = item->info->item.subtype;
        subtype = st;
        switch (st) {
        case 10:
            ((f32*)((u8*)runtime + 0x7214))[0] = matrix[12];
            ((f32*)((u8*)runtime + 0x7214))[1] = matrix[13];
            ((f32*)((u8*)runtime + 0x7214))[2] = matrix[14];
            sSpecialItem10 = (s32)item;
            break;
        case 14:
            if (instance != NULL) {
                DATA_S32(4) = *(s16*)&params[0];
            }
            break;
        case 2:
            if (instance != NULL) {
                DATA_S32(4) = *(s16*)&params[0];
            }
            if (DATA_S32(4) < 1) {
                DATA_S32(4) = 1;
            }
            break;
        case 15:
            DATA_S32(4) =
                ((s32*)((u8*)arrows + 0x10))[DATA_S32(4)];
            break;
        case 13:
            sSpecialItem13 = (s32)item;
            break;
        }
        }
        break;

    case 7:
        gNumType7Items++;
        break;
    }

    if (*(void**)item->atree != NULL) {
        AtreeDelete(item->atree);
        *(void**)item->atree = NULL;
    }

    if (attach_geometry &&
        (instance == NULL || (instance->flags & 2) == 0)) {
        u32 flags = 0;
        if (instance != NULL && (instance->flags & 4)) {
            flags |= 0x80000;
        }
        SetItemGeo(item, atree_header, name, flags);
    } else if (item->objgrp.node == NULL) {
        item->objgrp.node =
            MBNewNode(sItemsRootNode, gIdentityMatrix, 1);
    } else {
        *((u8*)item->objgrp.node + 0x52) = 1;
    }

    CopyMat4(matrix, (f32*)item->objgrp.node);
    CopyMat4(matrix, &item->objgrp.worldmat[0][0]);
    if (item->minoff) {
        MBTreeSetFlags(item->objgrp.node, 2, 0);
    }
    UpdateObjWorldMat(&item->objgrp);
    fn_8005A404(&item->objgrp.worldmat[0][0],
                item->coll_offset, item->coll_offset);
    MBTreeSetZMod(item->objgrp.node, 20.0f, 1);

    switch (type) {
    case 2:
        if (*(void**)item->atree != NULL) {
            s32 mbidx;
            sprintf(child_name, "%sNULL1", name);
            if ((mbidx = MBOX_ReallyFindObject(child_name, sItemFile0Handle,
                                               sItemFile0Handle, -1)) >= 0) {
                void* node = AtreeFindMbidxNode(*(void**)item->atree, mbidx);
                if (node != NULL) {
                    DATA_S32(8) = *(s32*)node;
                    MBTreeSetFlags(*(void**)node, 1, 0);
                }
            }
        }
        break;
    case 8:
        *((u8*)item + 0x83) |= 4;
        break;
    case 1:
        switch (subtype) {
        case 15:
            if (sMusicTrackHi == 13 && gSumnerReady != 0) {
                item->active |= 0x800;
                MBTreeSetAlpha(item->objgrp.node, 255, 1);
            }
            break;
        }
        break;
    }

#undef DATA_S16
#undef DATA_U16
#undef DATA_S32
#undef DATA_U32
#undef DATA_F32
#undef DATA_S8
#undef DATA_U8
#undef params
#undef PARAM_S16
#undef PARAM_S32
}

/* Attach either an animation tree or a static MB object to an item. */
void SetItemGeo(Item* item, void* atree_header, char* name, u32 flags)
{
    s32 node_type = 1;
    u32 mbflags;

    if (name == NULL) {
        name = item->info->item.desc;
    }
    if (atree_header == NULL) {
        atree_header = item->info->item.atreeheader;
    }
    mbflags = flags | 0x800;
    mbflags |= item->info->item.mbflags;
    if (item->info->type != 2) {
        node_type = 3;
    }

    if (atree_header != NULL) {
        item->action = 0;
        ((atree*)item->atree)->root =
            AtreeInit(atree_header, item->atree, name, mbflags);
        ((atree*)item->atree)->animinfo.repeat = 1;
        if (item->objgrp.node == NULL) {
            item->objgrp.node =
                MBNewNode(sItemsRootNode, gIdentityMatrix, node_type);
        } else {
            *((u8*)item->objgrp.node + 0x52) = 1;
        }
        MBNodeSetParent(*(void**)*(void**)item->atree, item->objgrp.node);
        if (item->active & 1) {
            s32 action;
            if (item->active & 4) {
                action = 0;
            } else {
                action = 1;
            }
            item->daction = action;
        }
        AnimateATree(item->atree, item->daction, 2);
    } else {
        u32 rv = (u32)ItemFindMBObjectL1(name);
        s32 object = (s32)rv;

        if ((s32)rv < 0) {
            ErrorPrintf(sSetItemFailedFmt, name);
            if (item->objgrp.node == NULL) {
                item->objgrp.node =
                    MBNewNode(sItemsRootNode, gIdentityMatrix, 1);
            } else {
                *((u8*)item->objgrp.node + 0x52) = 1;
            }
        } else if (item->objgrp.node == NULL) {
            item->objgrp.node = MBNewObject(object, gIdentityMatrix, 0, 0);
            MBTreeSetFlags(item->objgrp.node, mbflags, 0);
        } else {
            MBSetObject(item->objgrp.node, object);
            *((u8*)item->objgrp.node + 0x52) = 2;
            MBTreeSetFlags(item->objgrp.node, mbflags, 0);
        }
        *(s32*)item->atree = 0;
    }
}

s32 RegisterItemWobj(void* target_ptr, s16 type, s32 x_grid, s32 z_grid,
                     s32 value)
{
    s32 trigger_type = (u8)type;
    ItemRuntime* runtime = &sItemRuntime;
    u8* target = target_ptr;
    WorldObj* wtarget = (WorldObj*)target_ptr;
    char* strings = (char*)&sObjectsFile_80112AB8;
    f32 x = (f32)(0.1 * (f32)x_grid);
    f32 z = (f32)(0.1 * (f32)z_grid);
    s32 i;

    if (*(void**)(target + offsetof(WorldObj, nodeptr)) == NULL) {
        ErrorPrintf(strings + 0x464, target);
        return -1;
    }

    for (i = 0; i < sNumItemWobjs; i++) {
        if (runtime->wobjTarget[i] == target) {
            s16 flags = wtarget->triggertype;
            s32 old_type = (u8)flags;

            if (old_type != trigger_type) {
                if (old_type >= 27 && old_type <= 29 &&
                    trigger_type >= 27 && trigger_type <= 29) {
                    flags &= ~0xFF;
                    wtarget->triggertype = flags;
                    flags = wtarget->triggertype;
                    flags |= 27;
                    wtarget->triggertype = flags;
                } else {
                    ErrorPrintf(strings + 0x480, target, old_type, trigger_type);
                }
            }
            if (0.0 == (f64)runtime->wobjX2[i]) {
                runtime->wobjX2[i] = x;
                runtime->wobjX[i] = x;
            }
            if (0.0 == (f64)runtime->wobjZ[i]) {
                runtime->wobjZ[i] = z;
            }
            if (runtime->wobjValue[i] <= 0.0f) {
                runtime->wobjValue[i] = (f32)value;
            }
            return -1;
        }
    }

    if (++sNumItemWobjs >= 150) {
        FatalError(strings + 0x4B0, 0x800000);
    }
    runtime->wobjTarget[i] = target;
    runtime->wobjNodeY[i] =
        *(f32*)((u8*)*(void**)(target + 0x28) + 0x34);
    runtime->wobjX[i] = x;
    runtime->wobjX2[i] = x;
    runtime->wobjZ[i] = z;
    runtime->wobjValue[i] = (f32)value;
    wtarget->triggertype = (s16)type;
    return i;
}

/* ------------------------------------------------------------------ */
/* init / reset                                                       */
/* ------------------------------------------------------------------ */

void InitItems(void) {
    sItemFile0Buf = NULL;
    sItemFile0Handle = -1;
    sItemFile1Buf = NULL;
    sItemFile1Handle = -1;
}

/* ------------------------------------------------------------------ */
/* resource loaders                                                   */
/* ------------------------------------------------------------------ */

void UnloadWeaponsPowerups(void) {
    sWeaponsHandle = -1;
    sPowerupsHandle = -1;
}

void LoadWeapons(void) {
    if (sWeaponsHandle < 0) {
        sWeaponsHandle = LoadModel(sWeaponsName, &sWeaponsBuf, 0, -1);
    }
}

void LoadPowerups(char* name) {
    if (name == NULL) {
        name = sPowerupsName;
    }
    if (sPowerupsHandle < 0) {
        sPowerupsHandle = LoadModel(name, &sPowerupsBuf, 0, -1);
    }
}

void LoadItems(void)
{
    ItemRuntime* runtime = &sItemRuntime;
    ItemStrings* strings = &sObjectsFile_80112AB8;

    if (sItemFile0Handle < 0 && gBossType < 0) {
        sprintf(runtime->itemPath, strings->file0Format, WorldItemDesc());
        sItemFile0Handle =
            LoadModel(runtime->itemPath, &sGoodWizObj, 0, -1);
    }

    if (sItemFile1Handle < 0) {
        sprintf(runtime->itemPath, strings->file1Format, LevelItemDesc());
        if (FileExists(runtime->itemPath, strings->objectsFile)) {
            sItemFile1Handle =
                LoadModel(runtime->itemPath, &sItemFile1Buf, 0, -1);
        }
    }
}

void ResetItems(void)
{
    f32* runtime = (f32*)&sItemRuntime;

    sItemsRootNode = MBNewNode(gSceneRoot, gIdentityMatrix, 1);
    MBTreeSetFlags(sItemsRootNode, 4, 0);

    {
        f32 initial = sItemZero;

        sItems = 0;
        sNumItems = 0;
        gMaxItems = 0;
        gNextItemIdx = 0;
        sNumItemWobjs = 0;
        sUnusedItemState = 0;
        sUnusedResetState = 0;
        runtime[0x7214 / sizeof(f32)] = initial;
        runtime[0x7218 / sizeof(f32)] = initial;
        runtime[0x721C / sizeof(f32)] = initial;
        sSpecialItem10 = 0;
        sSpecialItem13 = 0;
        sItemRandSeed = pbLoad;
        sSafeRockCount = 0;
        sPreviousSafeRockCount = 0;
    }
}

/* ------------------------------------------------------------------ */
/* texture-mod (re)initialisation                                     */
/* ------------------------------------------------------------------ */

/* re-arm the two level item-file texmods after a load. */
void SetupItemTexMods(void) {
    if (sItemFile0Buf != NULL) {
        InitTexMods(sItemFile0Buf, sItemFile0Handle);
    }
    if (sItemFile1Buf != NULL) {
        InitTexMods(sItemFile1Buf, sItemFile1Handle);
    }
}

/* re-arm the shared weapon/powerup texmods, then refresh the four
 * per-slot texmods. */
void SetupWeaponPowerupTexMods(void) {
    int i;
    if (sWeaponsBuf != NULL) {
        InitTexMods(sWeaponsBuf, sWeaponsHandle);
    }
    if (sPowerupsBuf != NULL) {
        InitTexMods(sPowerupsBuf, sPowerupsHandle);
    }
    for (i = 0; i < 4; i++) {
        DoPlayerTexMods(i);
    }
}

/* ------------------------------------------------------------------ */
/* misc small helpers                                                 */
/* ------------------------------------------------------------------ */

/* deterministic pseudo-random index in [0, mod).  advance != 0 steps the
 * shared item seed by 439 (matches the DOL: (seed>>5 + n) % mod). */
s32 RandItemIdx(s32 n, s32 mod, s32 advance) {
    s32 result;

    result = mod != 0 ?
        (((u32)sItemRandSeed >> 5) + (u32)n) % (u32)mod : 0;
    if (advance != 0) {
        sItemRandSeed += 439;
    }
    return result;
}

/* return the magic-bus scene node backing item[idx]. */
struct mbnode* ItemGetNode(s32 idx) {
    return sItems[idx].objgrp.node;
}

/* ------------------------------------------------------------------ */
/* milestone, lookout, and trigger-camera boundary                    */
/* ------------------------------------------------------------------ */

s32 ClosestStartPos(f32* position)
{
    f64 zero;
    f64 minimum_y;
    f32 best_distance;
    s32 i;
    s32 result;
    u8 unused[8];

    best_distance = sNoDistance;
    zero = sZeroDouble;
    minimum_y = sInvalidPlayerStartY;
    result = 0;
    i = 0;

    do {
        f32* candidate = sPlayerStartPositions[i];
        f32* candidate_y = candidate + 1;

        if ((f64)*candidate_y <= minimum_y) {
            goto next_start;
        }
        if (WorldOpen(crystal_order[i]) == 0) {
            goto next_start;
        }
        {
            f32 dx = candidate[0] - position[0];
            f32 dy = *candidate_y - position[1];
            f32 dz = candidate[2] - position[2];
            f32 distance = dx * dx + dy * dy + dz * dz;

            if (best_distance < zero ||
                distance < best_distance) {
                best_distance = distance;
                result = i;
            }
        }

next_start:
        i++;
    } while (i < 14);

    return result;
}

void SetPlayerStartPos(s32 idx)
{
    u8*  base = (u8*)&sItemRuntime;
    f32* posY;

    if (idx > sLastPlayerStart) {
        idx = 0;
    }
    posY = (f32*)((u32)base + 3092);
    if ((double)*(f32*)((u8*)posY + idx * 12) <= -100000.0) {
        idx = 0;
    }
    if (WorldOpen(crystal_order[idx]) == 0) {
        idx = 0;
    }
    gDefaultPlayerPosition[0] = *(f32*)(base + idx * 12 + 3088);
    gDefaultPlayerPosition[1] = *(f32*)((u8*)posY + idx * 12);
    gDefaultPlayerPosition[2] = *(f32*)(base + idx * 12 + 3096);
    gPlayerStartYaw = *(f32*)(base + idx * 4 + 3032);
    if (*(u32*)(base + idx * 4 + 5596) == 0) {
        idx = 0;
    }
    CurTransmitter = *(TriggerCamera**)(base + idx * 4 + 5596);
}

void GetMilestonePos(s32 idx, f32* out)
{
    out[0] = sItemRuntime.milestones[idx].matrix[12];
    out[1] = sItemRuntime.milestones[idx].matrix[13];
    out[2] = sItemRuntime.milestones[idx].matrix[14];
}

void update_player_milestone(struct Player* player_ptr)
{
    u8* player = (u8*)player_ptr;
    u8* runtime = (u8*)&sItemRuntime;
    struct {
        union {
            f32 value;
            u32 bits;
        } absolute_y;
        u8 unused1[12];
        f32 position[3];
    } locals;
    u8 unused[8];
    s32 i;
    s32 offset;
    s32 j;
    f64 distance_tolerance;
    f64 height_tolerance;

    GetPlayerPos(*(s32*)player, locals.position);
    height_tolerance = sMilestoneHeightTolerance;
    distance_tolerance = sMilestoneDistanceTolerance;
    for (i = 0, offset = 0; i < sNumMilestones; i++, offset += 0x68) {
        u8* milestone = runtime + offset;
        f32 dy = locals.position[1] -
                 *(f32*)(milestone + offsetof(ItemRuntime, milestones) +
                         13 * sizeof(f32));
        f32 dz = locals.position[2] -
                 *(f32*)(milestone + offsetof(ItemRuntime, milestones) +
                         14 * sizeof(f32));
        f32 dx = locals.position[0] -
                 *(f32*)(milestone + offsetof(ItemRuntime, milestones) +
                         12 * sizeof(f32));

        locals.absolute_y.value = dy;
        locals.absolute_y.bits &= 0x7FFFFFFF;
        if ((f64)locals.absolute_y.value < height_tolerance &&
            (f64)fqdist(dx, dz) < distance_tolerance &&
            (player_ptr->milestone[0] < 0 ||
             (player_ptr->milestone[0] >= 0 &&
              player_ptr->milestone[0] != i))) {
            if (ShowMilestones(-1) != 0 && *(s32*)player == 0) {
                for (j = 0; j < 5; j++) {
                    s32 old = player_ptr->milestone[j];

                    if (old >= 0) {
                        u8* m = runtime + old * 0x68;

                        if (*(void**)(m + 0x3E74) != NULL) {
                            MBTreeClearFlags(*(void**)(m + 0x3E74), 2, 0);
                        }
                    }
                }
            }
            for (j = 4; j > 0; j--) {
                player_ptr->milestone[j] = player_ptr->milestone[j - 1];
            }
            player_ptr->milestone[0] = i;
        }
    }
    if (ShowMilestones(-1) != 0 && *(s32*)player == 0) {
        for (i = 0; i < 5; i++) {
            s32 milestone_index = player_ptr->milestone[i];

            if (milestone_index >= 0) {
                u8* m = runtime + milestone_index * 0x68;

                if (*(void**)(m + 0x3E74) != NULL) {
                    if ((lbl_80344800 & (1 << i)) != 0) {
                        MBTreeClearFlags(*(void**)(m + 0x3E74), 2, 0);
                    } else {
                        MBTreeSetFlags(*(void**)(m + 0x3E74), 2, 0);
                    }
                }
            }
        }
    }
}

#define ADD_TRANSMITTER(camera, loc)                                      \
    do {                                                                  \
        if (++sNumTriggerCameras > 256) {                                 \
            FatalError(strings + 1220, 0x800000);                         \
        }                                                                 \
        (camera) =                                                        \
            &(runtime)->triggerCameras[sNumTriggerCameras - 1];           \
        (camera)->eye[0] = (loc)->pos[0];                                 \
        (camera)->eye[1] = (loc)->pos[1];                                 \
        (camera)->eye[2] = (loc)->pos[2];                                 \
        (camera)->target[0] = (loc)->pyr[0];                              \
        (camera)->target[1] = (loc)->pyr[1];                              \
        (camera)->target[2] = (loc)->pyr[2];                              \
    } while (0)

/*
 * Expand the compact locator records loaded with WORLDINFO into the runtime
 * camera, milestone, boss, player-start, and lookout tables.
 */
void AddLocatorInstList(void)
{
    char* strings = (char*)&sObjectsFile_80112AB8;
    locator* locators = gWorldInfo.locators;
    s32 locator_count = gWorldInfo.nlocators;
    ItemRuntime* runtime = &sItemRuntime;
    f32 boss_matrix[16];
    u8 unused[12];
    f64 pi;
    f32 invalid_start;
    s32 i;
    s32 selected;

    sNumMilestones = 0;
    sNumTriggerCameras = 0;
    CurTransmitter = 0;
    sSpecialTransmitter = NULL;
    sLastTransmitter = 0;
    gNumTransmitters = 0;
    sNumLookoutParams = 0;

    for (i = 0; i < 20; i++) {
        runtime->lookoutParams[i].next = -1;
        runtime->lookoutParams[i].param = -1;
    }
    invalid_start = sInvalidPlayerStartYFloat;
    sLastPlayerStart = 0;
    for (i = 0; i < 14; i++) {
        runtime->playerStartPositions[i].y = invalid_start;
        runtime->startCameras[i] = NULL;
    }
    runtime->runeCameras[0] = NULL;
    runtime->runeCameras[1] = NULL;
    runtime->runeCameras[2] = NULL;
    sWindowCameras[0] = NULL;
    sWindowCameras[1] = NULL;
    sShownCameras = 0;
    pi = sPi;
    sShownMilestones = 0;

    for (i = 0; i < locator_count; i++) {
        locator* loc = &locators[i];
        TriggerCamera* camera;
        MilestoneParam* milestone;
        LookoutParam* lookout;
        s32 linked = 0;

        switch (loc->type) {
        case 1:
            linked = 1;
            /* fallthrough */
        case 2:
            ADD_TRANSMITTER(camera, loc);
            camera->target[1] =
                (f32)((f64)camera->target[1] + pi);
            camera->target[1] = FixAngle(camera->target[1]);
            camera->active = 1;
            camera->type = 0;
            camera->subtype = loc->subtype;
            gNumTransmitters++;
            if (linked) {
                if (sMusicTrackHi != 13) {
                    loc->index = 0;
                }
                if (loc->index > sLastTransmitter) {
                    sLastTransmitter = loc->index;
                }
                runtime->startCameras[loc->index] = camera;
                camera->type = 3;
            }
            camera->handle =
                add_arrow(1, 0, 0, loc->pyr, loc->pos, NULL);
            break;
        case 3:
            linked = 1;
            /* fallthrough */
        case 4:
            ADD_TRANSMITTER(camera, loc);
            camera->target[0] = -camera->target[0];
            camera->active = 1;
            camera->type = 1;
            camera->subtype = loc->subtype;
            if (linked) {
                sSpecialTransmitter = camera;
            }
            camera->handle =
                add_arrow(1, 0, 1, loc->pyr, loc->pos, NULL);
            break;
        case 9:
            ADD_TRANSMITTER(camera, loc);
            camera->target[1] =
                (f32)((f64)camera->target[1] + pi);
            camera->target[1] = FixAngle(camera->target[1]);
            camera->active = 1;
            camera->type = 2;
            camera->subtype = loc->subtype;
            LinkTriggerToCam((s32)(camera - runtime->triggerCameras),
                             loc->index);
            camera->handle =
                add_arrow(2, 0, 0, loc->pyr, loc->pos, NULL);
            break;
        case 5:
            if (++sNumMilestones > 128) {
                FatalError(strings + 1240, 0x800000);
            }
            milestone = &runtime->milestones[sNumMilestones - 1];
            milestone->handle =
                add_arrow(0, 0, 1, loc->pyr, loc->pos, milestone->matrix);
            milestone->pos[0] = milestone->matrix[12];
            milestone->pos[1] = milestone->matrix[13];
            milestone->pos[2] = milestone->matrix[14];
            milestone->saved_pos[0] = milestone->matrix[12];
            milestone->saved_pos[1] = milestone->matrix[13];
            milestone->saved_pos[2] = milestone->matrix[14];
            milestone->active = 1;
            break;
        case 6:
            if (gGameMode != MG_PLAYER_SELECT) {
                CreateYPRMatrix(boss_matrix, loc->pyr);
                boss_matrix[12] = loc->pos[0];
                boss_matrix[13] = loc->pos[1];
                boss_matrix[14] = loc->pos[2];
                AddBoss(boss_matrix);
            }
            break;
        case 7:
            if (sMusicTrackHi != 13) {
                loc->index = 0;
            }
            if (loc->index > sLastPlayerStart) {
                sLastPlayerStart = loc->index;
            }
            runtime->playerStartPositions[loc->index].x = loc->pos[0];
            runtime->playerStartPositions[loc->index].y = loc->pos[1];
            runtime->playerStartPositions[loc->index].z = loc->pos[2];
            runtime->playerStartYaw[loc->index] = loc->pyr[1];
            gPlayerStartYaw = loc->pyr[1];
            break;
        case 8:
        case 10:
            if (sNumLookoutParams >= 20) {
                ErrorPrintf(strings + 1260, 20);
                break;
            }
            sNumLookoutParams++;
            lookout = &runtime->lookoutParams[sNumLookoutParams - 1];
            lookout->node =
                add_arrow(2, 0, 1, loc->pyr, loc->pos,
                          (f32*)lookout->worldmat);
            lookout->attn_pos[0] = lookout->worldmat[3][0];
            lookout->attn_pos[1] = lookout->worldmat[3][1];
            lookout->attn_pos[2] = lookout->worldmat[3][2];
            lookout->coll_pos[0] = lookout->worldmat[3][0];
            lookout->coll_pos[1] = lookout->worldmat[3][1];
            lookout->coll_pos[2] = lookout->worldmat[3][2];
            lookout->flags = 1;
            lookout->next = loc->index;
            lookout->param = loc->subtype;
            break;
        default:
            ErrorPrintf(strings + 1276, loc->type);
            break;
        }
    }

    ShowCameras(gGameOptions.showcam);
    selected = 0;
    if (selected > sLastPlayerStart) {
        selected = 0;
    }
    if ((f64)((f32*)((u8*)runtime + 3092))[selected * 3] <=
        sInvalidPlayerStartY) {
        selected = 0;
    }
    if (WorldOpen(crystal_order[selected]) == 0) {
        selected = 0;
    }
    gDefaultPlayerPosition[0] =
        runtime->playerStartPositions[selected].x;
    gDefaultPlayerPosition[1] =
        ((f32*)((u8*)runtime + 3092))[selected * 3];
    gDefaultPlayerPosition[2] =
        runtime->playerStartPositions[selected].z;
    gPlayerStartYaw = runtime->playerStartYaw[selected];
    if (runtime->startCameras[selected] == NULL) {
        selected = 0;
    }
    CurTransmitter = runtime->startCameras[selected];
}

LookoutParam* FindLookoutParam(s32 id)
{
    LookoutParam* param = sLookoutParams;
    s32 count = sNumLookoutParams;
    s32 i;

    for (i = 0; i < count; i++) {
        if (param->param == id) {
            return param;
        }
        param++;
    }

    ErrorPrintf(sMissingLookoutParamFmt, id, sNumLookoutParams);
    return 0;
}

/* 0x80066E6C - show/hide the level milestone arrows for player idx. */
s32 ShowMilestones(s32 idx)
{
    s32 old = sShownMilestones;
    u8* base;
    s32 i;

    if (idx < 0) {
        return old;
    }
    sShownMilestones = idx;
    if (idx != old) {
        base = sMilestones;
        for (i = 0; i < sNumMilestones; i++) {
            MilestoneParam* melem = (MilestoneParam*)(base + i * 0x68);
            if (sShownMilestones != 0) {
                if ((u32)melem->handle == 0) {
                    melem->handle = add_arrow(1, 1, 1, NULL, NULL,
                                               melem->matrix);
                }
                MBTreeClearFlags((void*)melem->handle, 2, 0);
            } else {
                if ((u32)melem->handle != 0) {
                    MBRemoveNode(melem->handle, 1);
                    melem->handle = 0;
                }
            }
        }
    }
    return sShownMilestones;
}

/* 0x80066F48 - show/hide the trigger-camera arrows for player idx. */
s32 ShowCameras(s32 idx)
{
    s32 old = sShownCameras;
    u8* base;
    s32 i;
    f32 tmp[17];

    if (idx < 0) {
        return old;
    }
    sShownCameras = idx;
    if (idx != old) {
        base = sTriggerCameras;
        for (i = 0; i < sNumTriggerCameras; i++) {
            TriggerCamera* celem = (TriggerCamera*)(base + i * 0x28);
            s32 alt = 0;
            s32 kind = 1;
            if (celem->type == 1) {
                alt = 1;
                kind = 3;
            } else if (celem->type == 2) {
                kind = 2;
            }
            if (sShownCameras != 0) {
                if ((u32)celem->handle == 0) {
                    celem->handle = add_arrow(kind, 1, alt, celem->target,
                                               celem->eye, tmp);
                }
                MBTreeClearFlags((void*)celem->handle, 2, 0);
            } else {
                if ((u32)celem->handle != 0) {
                    MBRemoveNode(celem->handle, 1);
                    celem->handle = 0;
                }
            }
        }
        CurTransmitterBlink(sShownCameras);
    }
    return sShownCameras;
}

/* 0x80067050 - create a floor/level arrow blit; kind indexes the name table,
 * angles (optional) orient it, look supplies the aim point. */
void* add_arrow(s32 kind, s32 refresh, s32 useAngles, f32* angles, f32* look, f32* pos)
{
    f32 ang2[3];
    f32 tmp[16];
    void* handle = NULL;
    f32* mtx;

    if ((mtx = pos) == NULL) {
        mtx = tmp;
    }
    if (angles != NULL) {
        if (useAngles != 0) {
            ang2[0] = angles[0];
            ang2[1] = angles[1];
            ang2[2] = angles[2];
            ang2[1] = ang2[1] + sPi;
            ang2[1] = FixAngle(ang2[1]);
            ang2[0] = -ang2[0];
            ang2[0] = FixAngle(ang2[0]);
            CreateYPRMatrix(mtx, ang2);
            angles = ang2;
        }
        CreateYPRMatrix(mtx, angles);
        mtx[12] = look[0];
        mtx[13] = look[1];
        mtx[14] = look[2];
    }
    if (kind == 0) {
        mtx[13] = sArrowFloorYOffset + FloorPos(mtx[13], sArrowFloorRadius, mtx + 12, 0);
    }
    if (refresh != 0) {
        handle = MBOX_NewObject(sArrowObjectNames[kind], mtx, 0, 0);
        MBTreeSetAlpha(handle, 100, 0);
        if (refresh == 2) {
            MBTreeSetFlags(handle, 2, 0);
        }
    }
    return handle;
}

/* 0x80067248 - closest waypoint to pos within maxDist (all != 0 scans every
 * node; otherwise only chained ones). */
LookoutParam* FindClosestWaypoint(f32 maxDist, f32* pos, s32 all)
{
    s32 i;
    LookoutParam* w = sLookoutParams;
    LookoutParam* result = NULL;
    f32 d2;
    f32 dx;
    f32 dy;
    f32 dz;
    u8 unused[12];
    u8 unused2[8];
    volatile f32 root;

    for (i = 0; i < sNumLookoutParams; i++, w++) {
        if (all != 0 || (w->next >= 0 && w->next != i)) {
            dx = w->worldmat[3][0] - pos[0];
            dy = w->worldmat[3][1] - pos[1];
            dz = w->worldmat[3][2] - pos[2];
            if ((d2 = dz * dz + (dx * dx + dy * dy)) > 0.0f) {
                f64 guess = __frsqrte(d2);
                guess = 0.5 * guess * (3.0 - guess * guess * d2);
                guess = 0.5 * guess * (3.0 - guess * guess * d2);
                guess = 0.5 * guess * (3.0 - guess * guess * d2);
                root = (f32)(d2 * (0.5 * guess *
                                   (3.0 - guess * guess * d2)));
                d2 = root;
            }
            if (d2 < maxDist) {
                maxDist = d2;
                result = w;
            }
        }
    }
    return result;
}

LookoutParam* NextWaypoint(LookoutParam* waypoint)
{
    LookoutParam* result;
    s16 next = waypoint->next;

    if (next < 0) {
        result = 0;
    } else {
        result = &sLookoutParams[next];
    }
    if (result == waypoint) {
        result = 0;
    }
    return result;
}

void CrystalCamActivate(void)
{
    TriggerCamera* camera = sCrystalCamera;
    TriggerCameraActivate(0, camera->eye, camera->target, 50, 0, 0);
}

void SumnerCamActivate(s32 idx, s32 sub)
{
    TriggerCamera* camera = sSumnerCameras[idx][sub];

    while (camera == 0 && idx > 0) {
        idx--;
        camera = sSumnerCameras[idx][sub];
    }
    if (camera != 0) {
        TriggerCameraActivate(0, camera->eye, camera->target, -1, 0, 0);
    }
}

void WindowCamActivate(s32 idx)
{
    s32 variants[2];
    TriggerCamera* camera = sWindowCameras[idx];

    variants[0] = sWindowCameraVariant0;
    variants[1] = sWindowCameraVariant1;
    if (camera == 0) {
        camera = sWindowCameras[0];
    }
    if (camera != 0) {
        TriggerCameraActivate(0, camera->eye, camera->target, 0, 0,
                              variants[idx]);
    }
}

void RuneCamActivate(s32 idx)
{
    TriggerCamera* camera = sRuneCameras[idx];
    RuneCameraVariants variants = sRuneCameraVariants;

    while (camera == 0 && idx > 0) {
        idx--;
        camera = sRuneCameras[idx];
    }
    if (camera != 0) {
        TriggerCameraActivate(0, camera->eye, camera->target, 0, 0,
                              variants.value[idx]);
    }
}
