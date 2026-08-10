#include "types.h"
#include "game/item.h"
#include "game/enemy.h"
#include "game/worldinfo.h"

/*
 * dtk makes TU-local functions globally addressable in the extracted object
 * by appending their retail address.  Keep the recovered source names readable
 * while emitting symbols that objdiff can pair with those target functions.
 */
#define place_logic12       place_logic12_800631AC
#define generate_single     generate_single_80063444
#define AddItemWobj         AddItemWobj_80063DB0
#define NewItemPtr          NewItemPtr_800642C8
#define AtreeMatchAnyHeader AtreeMatchAnyHeader_800674F4

/* Gauntlet item / world-object system (Xbox ITEMS.OBJ), region
 * 0x800631AC-0x80067AE0 -- the whole gap between gauntworld.c and main.c.
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
    /* 0x24 */ s32 handle;
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
    s32 handle;
    s32 active;
} MilestoneParam; /* 0x68 */

typedef struct ItemRuntime {
    /* 0x0000 */ f32 wobjX[150];
    /* 0x0258 */ f32 wobjNodeY[150];
    /* 0x04B0 */ f32 wobjX2[150];
    /* 0x0708 */ f32 wobjZ[150];
    /* 0x0960 */ f32 wobjValue[150];
    char itemPath[0x20];
    f32 playerStartYaw[14];
    f32 playerStartPositions[14][3];
    LookoutParam lookoutParams[20];
    TriggerCamera* sumnerCameras[3][14];
    TriggerCamera* runeCameras[17];
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
extern ItemStrings    sObjectsFile;
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
extern s32             sKeyringAtree;
extern s32             sDeathIconAtree;
extern s32             sChestAtree;
extern u32            pbLoad;
extern s32            gNumPlayers;
extern u32            gGameMode;
extern s32            gGameOptions[12];
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
extern s32   AtreeInit(void* header, void* tree, char* name, u32 flags);
extern void  AnimateATree(void* tree, s32 action, s32 mode);
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
extern u32*  FindWORLDOBJ(const char* name);
extern double DistanceToClosestPlayer(f32* pos);
extern void  AddItemSub(Item* item);
extern void  AtreeDelete(void* p);
extern void  MBRemoveNode(s32 handle, s32 flag);
extern s32   MBTreeClearFlags(void* node, s32 a, s32 b);
extern void  MBNodeSetParent(void* node, void* parent);
extern void  UpdateObjWorldMat(OBJGRP* group);
static void AddItemWobj(Item* it);
extern s32   RegisterItemWobj(void* target_ptr, s16 type, s32 x_grid,
                              s32 z_grid, s32 value);
extern s32   PlayerSelecting(s32 idx);
extern u8    gPlayers[];
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
extern u8    gCameras[];
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

static u32 AtreeMatchAnyHeader(char* name, s32 alsoWads);

extern s32     sLastPlayerStart;
extern double  sInvalidPlayerStartY;
extern u8*     gCurLevel;
extern f32     sLevelAmbient;
extern f32     sLevelAmbientScale;
extern f32     sLightingScratchY;
extern f32     sLightingScratchZ;
extern f32     sLightingScratchX;
extern f32     AmbientSpecialTime;
extern f32     AmbientSpecialValue;
extern f32     AmbientSpecialCurValue;
extern f32     sOne;
extern f32     sNegativeHalf;
extern f32     sLightingZero;

extern void  MBInitLights(void);
extern void  MBAddLight(double val, void* a, f32* b);
extern void  MBSetAmbient(f32 val, f32* p);
extern void  DoLighting(s32 flag);
extern void  pbResetWindowPool(void);
extern void  pbSetWindowUV1(double a, double b);
extern void  pbSetWindowUV0(double a, double b);
extern volatile f32 sMusicFadeBase;
extern f32   sNegativeOne;
extern f64   sAmbientMinimum;
extern f64   sAmbientDecay;
extern f64   sAmbientBrightenStep;
extern f64   sAmbientDarkenStep;
extern f64   sAmbientMaximum;
extern s32     crystal_order[14];
extern f64   __frsqrte(f64 value);
extern s32   add_arrow(s32 kind, s32 one, s32 alt, f32* a, f32* b, f32* pos);
extern void  CurTransmitterBlink(s32 idx);                        /* newcam hook */
extern s32   AtreeMatch(void* tree, char* name, s32 flag);
extern f32   FixAngle(f64 ang);                        /* angle wrap */
extern void  CreateYPRMatrix(f32* mtx, f32* angles);          /* mtx from angles */
extern f64   FloorPos(f64 y, f32 r, f32* pos, s32 mode); /* ground probe */
extern s32   MBOX_NewObject(char* name, f32* mtx, s32 a, s32 b);
extern void  MBTreeSetAlpha(s32 handle, s32 pri, s32 b);
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
extern f32   sItemZero;   /* waypoint dist epsilon */
extern f32     gDefaultPlayerPosition[3];
extern f32     gPlayerStartYaw;
extern s32     CurTransmitter;
extern char    sNewItemBadIndex[];
extern char    sSetItemFailedFmt[];

s32 ItemVisible(Item* item);
void SetItemGeo(Item* item, void* atree_header, char* name, u32 flags);

/* ------------------------------------------------------------------ */
/* item pool                                                          */
/* ------------------------------------------------------------------ */

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

    if (*current != 0 && *(void**)((u8*)*current + 0x28) != 0 &&
        (*(u32*)((u8*)*current + 0x10) & 0x1000) != 0) {
        MBNodeSetParent(item->objgrp.node, *(void**)((u8*)*current + 0x28));
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

        if ((*(s16*)&item->data[4] & 0x400) == 0) {
            goto done;
        }
        linked = *(void**)&item->data[0];
        if (linked == 0) {
            goto done;
        }
        scene = *current;
        if (scene == 0) {
            goto done;
        }
        if (linked != scene && linked != *(void**)((u8*)scene + 0x18)) {
            goto done;
        }
        *(s16*)&item->data[4] |= 0x100;
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

            info->item.atreeheader = (void*)AtreeMatchAnyHeader(
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
            s32* node_slot;
            void* node;

            node = MBNewNode(sItemsRootNode, 0, 4);
            player_runtime = runtime + offset;
            *(void**)(player_runtime + 0x74B8) = node;
            overlay_runtime = runtime;
            overlay_runtime += overlay_offset;
            *(s32*)(player_runtime + 0x74A8) = 0;
            *(s32*)(overlay_runtime + 0x74C8) = 0;
            *(s32*)(player_runtime + 0x7478) =
                MBOX_NewObject(sSeeThroughObjectName, 0, (s32)sItemsRootNode,
                               0x04200000);
            node_slot = (s32*)(player_runtime + 0x7478);
            MBTreeSetFlags((void*)*node_slot, 1, 0);
            *(s16*)(*node_slot + 0x68) = -800;
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
    s32 instance_offset = 0;
    u8 frame_pad[8];
    f32 matrix[16];
    u8 unused[4];

    sItemRandSeed = pbLoad;
    SetPlayerVars();
    gMaxItems = instance_count + 500;
    sItems = AllocMem(gMaxItems * sizeof(Item));

    for (; i < instance_count; i++, instance_offset += sizeof(iteminst)) {
        Item* item = NewItemPtr();
        iteminst* instance;
        Item* vis;

        if ((instance = (iteminst*)((u8*)instances + instance_offset))
                ->index < 0) {
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

/* 0x8006799C - per-frame ambient light fade toward the level target. */
void DoLighting(s32 flag)
{
    u8 unused[16];
    f32 a;
    u8 unused2[8];
    f64 step;
    f64 lit;

    pbResetWindowPool();
    if (gCurLevel != NULL && (*(u32*)gCurLevel & 8)) {
        AmbientSpecialCurValue = sNegativeOne;
        AmbientSpecialValue = sNegativeOne;
    } else {
        if (sAmbientMinimum != AmbientSpecialValue && sMusicFadeBase > AmbientSpecialTime) {
            AmbientSpecialValue = (f32)(AmbientSpecialValue * sAmbientDecay);
            a = AmbientSpecialValue;
            *(u32*)&a &= 0x7FFFFFFF;
            if (a < sAmbientBrightenStep) {
                AmbientSpecialValue = sLightingZero;
            }
        }
    }
    if (AmbientSpecialValue != AmbientSpecialCurValue) {
        if (AmbientSpecialValue - AmbientSpecialCurValue < sAmbientDarkenStep) {
            step = sAmbientDarkenStep;
        } else if (AmbientSpecialValue - AmbientSpecialCurValue > sAmbientBrightenStep) {
            step = sAmbientBrightenStep;
        } else {
            step = AmbientSpecialValue - AmbientSpecialCurValue;
        }
        AmbientSpecialCurValue = AmbientSpecialCurValue + (f32)step;
    }
    lit = sLevelAmbient * sLevelAmbientScale + AmbientSpecialCurValue <
                  sAmbientMinimum ?
          sAmbientMinimum :
          (sLevelAmbient * sLevelAmbientScale + AmbientSpecialCurValue >
                   sAmbientMaximum ?
           sAmbientMaximum :
           sLevelAmbient * sLevelAmbientScale + AmbientSpecialCurValue);
    MBSetAmbient((f32)lit, NULL);
    pbSetWindowUV1(sOne, AmbientSpecialCurValue);
    pbSetWindowUV0(sOne, AmbientSpecialCurValue);
}

/* 0x800674F4 - match name against the weapon/powerup/item atrees, then all
 * wad headers when alsoWads is set. */
static u32 AtreeMatchAnyHeader(char* name, s32 alsoWads)
{
    u32 r = 0;

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

/* (re)build the level lights and ambient from the current level record. */
void InitLighting(s32 flag)
{
    MBInitLights();
    if (flag != 0) {
        sLevelAmbient = *(f32*)(gCurLevel + 236);
        MBAddLight(*(f32*)(gCurLevel + 264), gCurLevel + 240,
                   (f32*)(gCurLevel + 252));
    } else {
        sLevelAmbient = sOne;
    }
    sLevelAmbientScale = sOne;
    MBSetAmbient(sLevelAmbient, NULL);
    DoLighting(1);
    sLightingScratchY = sLightingZero;
    sLightingScratchZ = sNegativeHalf;
    sLightingScratchX = sNegativeHalf;
    AmbientSpecialTime = sLightingZero;
    AmbientSpecialValue = sLightingZero;
    AmbientSpecialCurValue = sLightingZero;
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
    char* strings = (char*)&sObjectsFile;
    Item* item;
    s32 i;
    s32 j;
    Item* other;
    s32 nitems;

    {
    Item* it1;
    s32 i1;
    s32 j1;
    s32 dup1;
    Item* ot1;

    it1 = sItems;
    for (i1 = 0; i1 < sNumItems; i1++, it1++) {
        if (it1->active != -1 && it1->info->type == 5) {
            for (dup1 = 0, j1 = 0, ot1 = sItems;
                 j1 < sNumItems; j1++, ot1++) {
                if (j1 != i1 && ot1->active != -1 &&
                    ot1->info->type == 5 &&
                    (*(s16*)&ot1->data[4] & 0x40) ==
                        (*(s16*)&it1->data[4] & 0x40) &&
                    *(s8*)&it1->data[6] > 0) {
                    if (*(s8*)&ot1->data[6] == *(s8*)&it1->data[6]) {
                        *(s8*)&ot1->data[6] = 0;
                        dup1++;
                    }
                    if (*(s8*)&ot1->data[7] != *(s8*)&it1->data[6]) {
                        asm { b link_cont_j }
                    }
link_cont_j:;
                }
            }
            if (dup1 > 0) {
                if (*(s16*)&it1->data[4] & 0x40) {
                    ErrorPrintf(strings + 0x2EC,
                                dup1 + 1,
                                (s32)*(s8*)&it1->data[6]);
                } else {
                    ErrorPrintf(strings + 0x310,
                                dup1 + 1,
                                (s32)*(s8*)&it1->data[6]);
                }
            }
        }
    }

    }

    item = sItems;
    for (i = 0; i < (nitems = sNumItems); i++, item++) {
        if (item->active != -1 && item->info->type == 5) {
            s8 next_id = *(s8*)&item->data[7];

            if (next_id != 0) {
                other = sItems;
                for (j = 0; j < nitems; j++, other++) {
                    if (j != i && other->active != -1 &&
                        other->info->type == 5 &&
                        (*(s16*)&other->data[4] & 0x40) == 0 &&
                        next_id == *(s8*)&other->data[6]) {
                        Item* chain;
                        s32 loop = 0;
                        Item* next;

                        for (chain = other; chain != NULL;
                             chain = next) {
                            next = *(Item**)&chain->data[8];
                            if (next == item) {
                                ErrorPrintf(strings + 0x32C,
                                            (s32)next_id,
                                            (s32)*(s8*)&other->data[7]);
                                loop = 1;
                                break;
                            }
                        }
                        if (!loop) {
                            *(Item**)&item->data[8] = other;
                            *(s16*)&other->data[4] |= 0x200;
                        }
                        break;
                    }
                }
                if (j >= sNumItems) {
                    ErrorPrintf(strings + 0x350,
                                (s32)*(s8*)&item->data[6],
                                (s32)*(s8*)&item->data[7]);
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
    s32 idx;

    if (flag != 0) {
        if (item->info->type == 1 && (e = *(u8**)((u8*)item + 0xE8)) != NULL) {
            if (*(u32*)(e + 0x6C) != 0) {
                AtreeDelete(e + 0x6C);
                *(u32*)(e + 0x6C) = 0;
            }
            if (*(u32*)(e + 0x64) != 0) {
                MBRemoveNode(*(u32*)(e + 0x64), 0);
                *(u32*)(e + 0x64) = 0;
            }
            *(s16*)(e + 0xC4) = -1;
            idx = (s32)(e - (u8*)sItems) / 240;
            if (idx < gNextItemIdx) {
                gNextItemIdx = idx;
            }
        }
        if (item->info->type == 2 && (e = *(u8**)((u8*)item + 0xE8)) != NULL) {
            if (*(u32*)(e + 0x6C) != 0) {
                AtreeDelete(e + 0x6C);
                *(u32*)(e + 0x6C) = 0;
            }
            if (*(u32*)(e + 0x64) != 0) {
                MBRemoveNode(*(u32*)(e + 0x64), 0);
                *(u32*)(e + 0x64) = 0;
            }
            *(s16*)(e + 0xC4) = -1;
            idx = (s32)(e - (u8*)sItems) / 240;
            if (idx < gNextItemIdx) {
                gNextItemIdx = idx;
            }
        }
    }
    e = (u8*)item;
    if (*(u32*)(e + 0x6C) != 0) {
        AtreeDelete(e + 0x6C);
        *(u32*)(e + 0x6C) = 0;
    }
    if (*(u32*)(e + 0x64) != 0) {
        MBRemoveNode(*(u32*)(e + 0x64), 0);
        *(u32*)(e + 0x64) = 0;
    }
    *(s16*)(e + 0xC4) = -1;
    idx = (s32)(e - (u8*)sItems) / 240;
    if (idx < gNextItemIdx) {
        gNextItemIdx = idx;
    }
}

/* look up an item definition by name (+ type / optional level) and spawn it. */
Item* PlaceItem(s32 type, s32 level, char* name, void* matrix)
{
    u8 unused[8];
    s32 i;
    Item* item;
    iteminfo* d;
    iteminfo* def;

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
            goto found;
        }
    }
    i = -1;
found:
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
    u32* obj;

    switch (gBossType) {
    case 42:
        sSafeRockCount++;
        break;
    case 41:
        obj = FindWORLDOBJ(sSafeRockBoss41ObjectName);
        if (obj != NULL && obj[10] != 0) {
            MBTreeSetFlags((void*)obj[10], 1, 0);
        }
        break;
    case 44:
        obj = FindWORLDOBJ(sSafeRockBoss44ObjectName);
        if (obj != NULL && obj[10] != 0) {
            MBTreeSetFlags((void*)obj[10], 1, 0);
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
        if (it->info->type == 10 && *(s16*)((u8*)it + 0xDC) == 0x29) {
            out[count] = i;
            if (flag != 0) {
                MBTreeSetFlags(it->objgrp.node, 1, 1);
                *(s16*)((u8*)it + 0xDE) = -1;
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
    u8* p = (u8*)it + 0xDC;
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
    if ((s32)gGameMode == 0x400C) {
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
    if ((s32)gGameMode == 0x8008) {
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
        dy = *(f32*)(gCameras + 0x38) - sphere[1];
        dx = *(f32*)(gCameras + 0x34) - sphere[0];
        dz = *(f32*)(gCameras + 0x3C) - sphere[2];
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
        u8* player = gPlayers;
        s32 i;

        for (i = 0; i < 4; i++, player += 0x335C) {
            if (*(s32*)(player + 0xE8) == 1) {
                volatile f32 root;
                f32 distance;
                f32 dx;
                f32 dy;
                f32 dz;

                dy = *(f32*)(player + 0x48) - position[1];
                dx = *(f32*)(player + 0x44) - position[0];
                dz = *(f32*)(player + 0x4C) - position[2];
                distance = dx * dx + dy * dy;
                distance = dz * dz + distance;

                if (distance > sItemZero) {
                    f64 estimate = __frsqrte(distance);
                    estimate = sArrowFloorYOffset * estimate *
                               (sNewtonThree -
                                estimate * estimate * distance);
                    estimate = sArrowFloorYOffset * estimate *
                               (sNewtonThree -
                                estimate * estimate * distance);
                    estimate = sArrowFloorYOffset * estimate *
                               (sNewtonThree -
                                estimate * estimate * distance);
                    root = (f32)(distance *
                                 (sArrowFloorYOffset * estimate *
                                  (sNewtonThree -
                                   estimate * estimate * distance)));
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
    u8* player = gPlayers;
    u8* enemy = (u8*)gEnemies;
    s32 i;

    for (i = 0; i < 4; i++, player += 13148) {
        s32 state = *(s32*)(player + 0xE8);
        if (state == 1 || state == 8 || PlayerSelecting(i) != 0) {
            if (*(void**)(player + 0x8C4) == owner) {
                return 2;
            }
        }
    }
    if (checkEnemies != 0) {
        for (i = 0; i < gNumEnemies; i++, enemy += 916) {
            if (*(s32*)(enemy + 0xB4) == 1 &&
                *(void**)(enemy + 0x298) == owner) {
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
    Item* p;
    s32  i;
    s16  sidx;

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
            *(s8*)((u8*)p + 0xE2) == type) {
            s16 cur = *(s16*)((u8*)p + 0xEE);
            if (cur >= 0) {
                ErrorPrintf(sTriggerCameraConflictFmt, i, cur, idx);
            }
            *(s16*)((u8*)p + 0xEE) = sidx;
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
static void AddItemWobj(Item* it)
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
    if (tier != *(s16*)(it->data + 2)) {
        s32 found;
        s32 tex;
        *(s16*)(it->data + 2) = tier;
        sprintf(buf, sItemHealthTextureFmt, it->info->item.desc,
                *(s16*)(it->data + 2));
        found = ItemFindMBObjectL1(buf);
        tex = found;
        if (found < 0) {
            MBTreeSetFlags(it->objgrp.node, 1, 1);
            *(s16*)(it->data + 2) = -1;
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
    *(s16*)((u8*)it + 0xDE) = 0;
    it->armor = (s8)it->info->item.armor;
    AddItemWobj(it);
}

/* item is "hot": has hitpoints and a positive damage/state field. */
s32 SafeRockActive(s32 idx)
{
    Item* it = &sItems[idx];

    if (it->health > 0 && *(s16*)((u8*)it + 0xDE) > 0) {
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

    if (gGameMode & 0x8000) {
        val = 2;
    }
    if (raw_minp > 10) {
        useEq = 1;
        minp -= 10;
    }
    if (gGameOptions[4] > 0) {
        val = gGameOptions[4];
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
static void place_logic12(s8* data, s32 enemy_index)
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
static void generate_single(Item* item, s32 algorithm, s32 important)
{
    u8* data = item->data;
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
    char* strings = (char*)&sObjectsFile;
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
                    asm { b keyring_found }
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

        if (radius > info->item.height) {
            asm { b keep_r }
        }
        radius = info->item.height;
    keep_r:
        item->visrad = (f32)(2.0 * (f64)radius);
    }
    item->objgrp.flags = 0;
    {
        f32 z;
        f32 x;
        u8 abs_pad[24];

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

#define DATA_S16(off) (*(s16*)&item->data[(off)])
#define DATA_U16(off) (*(u16*)&item->data[(off)])
#define DATA_S32(off) (*(s32*)&item->data[(off)])
#define DATA_U32(off) (*(u32*)&item->data[(off)])
#define DATA_F32(off) (*(f32*)&item->data[(off)])
#define DATA_S8(off)  (*(s8*)&item->data[(off)])
#define DATA_U8(off)  (*(u8*)&item->data[(off)])
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
            instance != NULL ? *(f32*)&params[4] : sZeroDouble;
        DATA_S16(16) = PARAM_S16(8, 0);
        if (gGameOptions[10] != 0 || DATA_S8(3) < 0) {
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
                if (*(u32*)(wobj + 0x10) & 0x800) {
                    *(u32*)(wobj + 0x10) |= 0x10000000;
                }
                RegisterItemWobj(
                    wobj, (s16)subtype,
                    *(s16*)&params[8], *(s16*)&params[10],
                    *(s8*)&params[5]);
                wobj[0x17] = 0;
                wobj[0x16] = 0;
                *(u32*)(wobj + 0x10) |= 0x100000;
            }
            trigger_flags |= *(s16*)&params[2] & ~0xFF;
            DATA_S16(4) = (s16)trigger_flags;
            if (params[4] == 0xFF) {
                DATA_F32(8) = 0.01f;
            } else {
                DATA_F32(8) = (f32)(0.5 * (f64)params[4]);
            }
            DATA_U8(6) = params[6];
            DATA_U8(7) = params[7];
        } else {
            DATA_S32(0) = 0;
            DATA_U16(4) = 0;
            DATA_F32(8) = 0.0f;
            DATA_U8(6) = 0;
            DATA_U8(7) = 0;
        }
        DATA_S32(12) = 0;
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
                    u8* q = gPlayers + player * 13148;
                    u8* p = q;
                    if (*(s32*)(q + 0xE8) != 0) {
                        s32 player_index;
                        flags |= towerGetLevelFlag(p, 8);
                        player_index = *(s32*)(p + 0x0C);
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
                *(u32*)(wobj + 0x10) |= 0xA00000;
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
            instance != NULL ? *(f32*)&params[8] : sZeroDouble;
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
                asm { b low_item_found }
            }
            found = -1;
low_item_found:
            if (found >= 0) {
                item->info = &(*infos)[found];
            }
        }
        if (gGameOptions[10] != 0 || DATA_S8(7) < 0) {
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
            asm { b count_index_done }
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
                           *(f32*)(gCurLevel + 0xD4));
        DATA_U8(11) = (u8)(DATA_U8(11) *
                            *(f32*)(gCurLevel + 0xD0));
        item->health = (s16)(item->health *
                              *(f32*)(gCurLevel + 0xCC));
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
        atree_header = (void*)AtreeMatchAnyHeader(name, 1);
        break;
    }

    case 8:
    {
        s32 scaled;
        DATA_F32(0) = instance != NULL ?
                      (f32)*(s16*)&params[0] : 0.0f;
        if (DATA_F32(0) == 0.0f) {
            DATA_F32(0) =
                (f32)*(s16*)((u8*)item->info + 0x40);
        }
        DATA_S16(4) = PARAM_S16(2, 0);
        scaled = DATA_S16(4);
        if ((s16)scaled != 0) {
            scaled = -scaled;
            *(s16*)((u8*)item->info + 0x48) = (s16)(scaled * 3);
        }
        DATA_F32(0) *= *(f32*)(gCurLevel + 0xDC);
        item->health = (s16)(item->health *
                              *(f32*)(gCurLevel + 0xCC));
        scaled = *(s16*)((u8*)info + 0x48) * 2;
        if (scaled == 0) {
            scaled = 0;
        } else if (scaled < 0) {
            scaled = -scaled;
            scaled = (scaled >> 1) + (s32)RandInt((u32)scaled);
        }
        item->activetime =
            (s16)(scaled * *(f32*)(gCurLevel + 0xD8));
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
                s32 off = i + 8;
                char* c = (char*)instance + off;
                *c = (char)toupper(*c);
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
        DATA_S32(8) = FindWorldAnimNode(&matrix[12], sItemSearchDistance);
        attach_geometry = 0;
        item->active |= 0x40;
        break;

    case 10:
        DATA_S16(0) = PARAM_S16(0, 0);
        DATA_S16(2) = PARAM_S16(2, 1);
        if (DATA_S16(0) <= 0) {
            DATA_S16(0) = (s16)item->info->item.subtype;
        }
        if (DATA_S16(0) == 49) {
            goto set_type10_active;
        }
        if (DATA_S16(0) < 49) {
            if (DATA_S16(0) == 40) {
                goto set_type10_active;
            }
            goto after_type10_active;
        }
        if (DATA_S16(0) >= 54) {
            goto after_type10_active;
        }
        if (DATA_S16(0) < 51) {
            asm { b after_type10_active }
        }
set_type10_active:
        item->active |= 0x40;
after_type10_active:
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
        DATA_S32(0) = *(s32*)((u8*)item->info + 0x3C);
        DATA_S32(4) = *(s16*)((u8*)item->info + 0x40);
        DATA_F32(8) = (f32)*(s16*)((u8*)item->info + 0x4A);
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
                MBTreeSetAlpha((s32)item->objgrp.node, 255, 1);
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
        *(s32*)item->atree =
            AtreeInit(atree_header, item->atree, name, mbflags);
        *(s16*)((u8*)item + 0xA4) = 1;
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
        s32 object = ItemFindMBObjectL1(name);

        if (object < 0) {
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
    ItemRuntime* runtime = &sItemRuntime;
    u8* target = target_ptr;
    char* strings = (char*)&sObjectsFile;
    s32 trigger_type = (u8)type;
    f32 x = (f32)(sItemFloorYOffset * (f32)x_grid);
    f32 z = (f32)(sItemFloorYOffset * (f32)z_grid);
    s32 i;
    s32 offset;

    if (*(void**)(target + 0x28) == NULL) {
        ErrorPrintf(strings + 0x464, target);
        return -1;
    }

    for (i = 0, offset = 0; i < sNumItemWobjs; i++, offset += 4) {
        if (*(void**)((u8*)runtime->wobjTarget + offset) == target) {
            s16 flags = *(s16*)(target + 0x14);
            s32 old_type = (u8)flags;

            if (old_type != trigger_type) {
                if (old_type >= 27 && old_type <= 29 &&
                    trigger_type >= 27 && trigger_type <= 29) {
                    *(s16*)(target + 0x14) &= 0xFF00;
                    *(s16*)(target + 0x14) |= 27;
                } else {
                    ErrorPrintf(strings + 0x480, target, old_type, trigger_type);
                }
            }
            if (sZeroDouble ==
                (f64)*(f32*)((u8*)runtime->wobjX2 + offset)) {
                *(f32*)((u8*)runtime->wobjX2 + offset) = x;
                *(f32*)((u8*)runtime->wobjX + offset) = x;
            }
            if (sZeroDouble ==
                (f64)*(f32*)((u8*)runtime->wobjZ + offset)) {
                *(f32*)((u8*)runtime->wobjZ + offset) = z;
            }
            if (*(f32*)((u8*)runtime->wobjValue + offset) <= sItemZero) {
                *(f32*)((u8*)runtime->wobjValue + offset) = (f32)value;
            }
            return -1;
        }
    }

    if (++sNumItemWobjs >= 150) {
        FatalError(strings + 0x4B0, 0x800000);
    }
    {
        void* node = *(void**)(target + 0x28);

        runtime->wobjTarget[i] = target;
        runtime->wobjNodeY[i] = *(f32*)((u8*)node + 0x34);
        runtime->wobjX[i] = x;
        runtime->wobjX2[i] = x;
        runtime->wobjZ[i] = z;
        runtime->wobjValue[i] = (f32)value;
        *(s16*)(target + 0x14) = (s16)type;
    }
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
    ItemStrings* strings = &sObjectsFile;

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
    if ((double)*(f32*)((u8*)posY + idx * 12) <= sInvalidPlayerStartY) {
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
    CurTransmitter = *(s32*)(base + idx * 4 + 5596);
}

void GetMilestonePos(s32 idx, f32* out)
{
    u8* milestone = (u8*)&sItemRuntime + idx * 0x68;

    out[0] = *(f32*)(milestone + 0x3E44);
    out[1] = *(f32*)(milestone + 0x3E48);
    out[2] = *(f32*)(milestone + 0x3E4C);
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
        f32 dy = locals.position[1] - *(f32*)(milestone + 0x3E48);
        f32 dx = locals.position[0] - *(f32*)(milestone + 0x3E44);
        f32 dz = locals.position[2] - *(f32*)(milestone + 0x3E4C);

        locals.absolute_y.value = dy;
        locals.absolute_y.bits &= 0x7FFFFFFF;
        if ((f64)locals.absolute_y.value < height_tolerance &&
            (f64)fqdist(dx, dz) < distance_tolerance &&
            (*(s32*)(player + 0xA34) < 0 ||
             (*(s32*)(player + 0xA34) >= 0 &&
              *(s32*)(player + 0xA34) != i))) {
            if (ShowMilestones(-1) != 0 && *(s32*)player == 0) {
                for (j = 0; j < 5; j++) {
                    s32 old = *(s32*)(player + 0xA34 + j * 4);

                    if (old >= 0) {
                        u8* m = runtime + old * 0x68;

                        if (*(void**)(m + 0x3E74) != NULL) {
                            MBTreeClearFlags(*(void**)(m + 0x3E74), 2, 0);
                        }
                    }
                }
            }
            for (j = 4; j > 0; j--) {
                *(s32*)(player + 0xA34 + j * 4) =
                    *(s32*)(player + 0xA30 + j * 4);
            }
            *(s32*)(player + 0xA34) = i;
        }
    }
    if (ShowMilestones(-1) != 0 && *(s32*)player == 0) {
        for (i = 0; i < 5; i++) {
            s32 milestone_index = *(s32*)(player + 0xA34 + i * 4);

            if (milestone_index >= 0) {
                u8* m = runtime + milestone_index * 0x68;

                if (*(void**)(m + 0x3E74) != NULL) {
                    if ((sShownMilestones & (1 << i)) != 0) {
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
    locator* locators = gWorldInfo.locators;
    s32 locator_count = gWorldInfo.nlocators;
    ItemRuntime* runtime = &sItemRuntime;
    char* strings = (char*)&sObjectsFile;
    f32 boss_matrix[16];
    u8 unused[12];
    f64 pi;
    f32 invalid_start;
    s32 i;
    s32 selected;
    s32 yaw_offset;
    s32 position_offset;

    sNumMilestones = 0;
    sNumTriggerCameras = 0;
    CurTransmitter = 0;
    sSpecialTransmitter = NULL;
    sLastTransmitter = 0;
    gNumTransmitters = 0;
    sNumLookoutParams = 0;

    for (i = 0; i < 20; i++) {
        runtime->lookoutParams[i].next = -1;
        runtime->lookoutParams[i].id = -1;
    }
    invalid_start = sInvalidPlayerStartYFloat;
    sLastPlayerStart = 0;
    yaw_offset = 0;
    position_offset = 0;
    for (i = 0; i < 14; i++) {
        *(f32*)((u8*)runtime + position_offset + 3092) = invalid_start;
        *(TriggerCamera**)((u8*)runtime + yaw_offset + 5596) = NULL;
        position_offset += 12;
        yaw_offset += 4;
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
                runtime->runeCameras[loc->index + 3] = camera;
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
            if ((s32)gGameMode != 0x400B) {
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
            runtime->playerStartPositions[loc->index][0] = loc->pos[0];
            runtime->playerStartPositions[loc->index][1] = loc->pos[1];
            runtime->playerStartPositions[loc->index][2] = loc->pos[2];
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
            lookout->handle =
                add_arrow(2, 0, 1, loc->pyr, loc->pos,
                          (f32*)lookout->data);
            lookout->saved_pos[0] = lookout->pos[0];
            lookout->saved_pos[1] = lookout->pos[1];
            lookout->saved_pos[2] = lookout->pos[2];
            lookout->saved_pos2[0] = lookout->pos[0];
            lookout->saved_pos2[1] = lookout->pos[1];
            lookout->saved_pos2[2] = lookout->pos[2];
            lookout->active = 1;
            lookout->next = loc->index;
            lookout->id = loc->subtype;
            break;
        default:
            ErrorPrintf(strings + 1276, loc->type);
            break;
        }
    }

    ShowCameras(gGameOptions[5]);
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
        runtime->playerStartPositions[selected][0];
    gDefaultPlayerPosition[1] =
        ((f32*)((u8*)runtime + 3092))[selected * 3];
    gDefaultPlayerPosition[2] =
        runtime->playerStartPositions[selected][2];
    gPlayerStartYaw = runtime->playerStartYaw[selected];
    if (runtime->runeCameras[selected + 3] == NULL) {
        selected = 0;
    }
    CurTransmitter = (s32)runtime->runeCameras[selected + 3];
}

#undef ADD_TRANSMITTER

LookoutParam* FindLookoutParam(s32 id)
{
    LookoutParam* param = sLookoutParams;
    s32 count = sNumLookoutParams;
    s32 i;

    for (i = 0; i < count; i++) {
        if (param->id == id) {
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
            u8* elem = base + i * 0x68;
            if (sShownMilestones != 0) {
                if (*(u32*)(elem + 0x60) == 0) {
                    *(s32*)(elem + 0x60) = add_arrow(1, 1, 1, NULL, NULL,
                                                     (f32*)elem);
                }
                MBTreeClearFlags((void*)*(s32*)(elem + 0x60), 2, 0);
            } else {
                if (*(u32*)(elem + 0x60) != 0) {
                    MBRemoveNode(*(s32*)(elem + 0x60), 1);
                    *(s32*)(elem + 0x60) = 0;
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
            u8* elem = base + i * 0x28;
            s32 alt = 0;
            s32 kind = 1;
            if (*(u8*)elem == 1) {
                alt = 1;
                kind = 3;
            } else if (*(u8*)elem == 2) {
                kind = 2;
            }
            if (sShownCameras != 0) {
                if (*(u32*)(elem + 0x24) == 0) {
                    *(s32*)(elem + 0x24) = add_arrow(kind, 1, alt,
                                                     (f32*)(elem + 0x14),
                                                     (f32*)(elem + 0x04),
                                                     tmp);
                }
                MBTreeClearFlags((void*)*(s32*)(elem + 0x24), 2, 0);
            } else {
                if (*(u32*)(elem + 0x24) != 0) {
                    MBRemoveNode(*(s32*)(elem + 0x24), 1);
                    *(s32*)(elem + 0x24) = 0;
                }
            }
        }
        CurTransmitterBlink(sShownCameras);
    }
    return sShownCameras;
}

/* 0x80067050 - create a floor/level arrow blit; kind indexes the name table,
 * angles (optional) orient it, look supplies the aim point. */
s32 add_arrow(s32 kind, s32 refresh, s32 useAngles, f32* angles, f32* look, f32* pos)
{
    f32 ang2[3];
    f32 tmp[16];
    s32 handle = 0;
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
            MBTreeSetFlags((void*)handle, 2, 0);
        }
    }
    return handle;
}

/* 0x80067248 - closest waypoint to pos within maxDist (all != 0 scans every
 * node; otherwise only chained ones). */
LookoutParam* FindClosestWaypoint(f64 maxDist, f32* pos, s32 all)
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
            dy = w->pos[1] - pos[1];
            dx = w->pos[0] - pos[0];
            dz = w->pos[2] - pos[2];
            d2 = dx * dx + dy * dy;
            d2 = dz * dz + d2;
            if (d2 > sItemZero) {
                f64 guess = __frsqrte(d2);
                guess = sArrowFloorYOffset * guess * (3.0 - guess * guess * d2);
                guess = sArrowFloorYOffset * guess * (3.0 - guess * guess * d2);
                guess = sArrowFloorYOffset * guess * (3.0 - guess * guess * d2);
                root = (f32)(d2 * (sArrowFloorYOffset * guess *
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
