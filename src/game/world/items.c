#include "types.h"
#include "game/item.h"
#include "game/worldinfo.h"

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
 * Wired NonMatching: the DOL links from the dtk-extracted asm object, so this
 * translation unit exists to map symbols and document behaviour.  The small,
 * unambiguous loader/init/accessor functions are reconstructed below; the
 * SetItem giant (0x1468) and the file-parse bodies are left to the asm.
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
    /* 0x00 */ u8  _pad00[4];
    /* 0x04 */ f32 eye[4];
    /* 0x14 */ f32 target[3];
} TriggerCamera;

typedef struct RuneCameraVariants {
    s32 value[3];
} RuneCameraVariants;

typedef struct ItemRuntime {
    /* 0x0000 */ f32 wobjX[150];
    /* 0x0258 */ f32 wobjNodeY[150];
    /* 0x04B0 */ f32 wobjX2[150];
    /* 0x0708 */ f32 wobjZ[150];
    /* 0x0960 */ f32 wobjValue[150];
    char itemPath[0x100];
    u8   _pad0CB8[0x6568];
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

extern s32            gGameBusy;
extern s32            gScriptedCameraState;
extern s32            gFrameTicks;
extern s32            default_gen_count;
extern double         ITEM_ACTIVE_DIST;
extern char           sSafeRockBoss41ObjectName[];
extern char           sSafeRockBoss44ObjectName[];

extern void  FatalError(const char* msg, s32 code);
extern void  fn_8005412C(void);
extern void  fn_80062A00(void);
extern void  LinkItemTriggers(void);
extern void* memset(void* dst, s32 val, u32 n);
extern u32*  FindWORLDOBJ(const char* name);
extern double DistanceToClosestPlayer(f32* pos);
extern void  AddItemSub(Item* item);
extern void  AtreeDelete(void* p);
extern void  MBRemoveNode(s32 handle, s32 flag);
extern s32   MBTreeClearFlags(void* node, s32 a, s32 b);
extern void  MBNodeSetParent(void* node, void* parent);
extern void  UpdateObjWorldMat(OBJGRP* group);
extern void  AddItemWobj(Item* item);
extern s32   PlayerSelecting(s32 idx);
extern s32   gNumEnemies;
extern u8    gEnemies[];
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
extern f32   lbl_80346F50;
extern f32   lbl_80347038;
extern f32   lbl_8034709C;
extern f64   lbl_80346FC8;
extern f64   lbl_80347120;
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
extern void  MBSetAmbient(double val, f32* p);
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
extern void  MBSetObject(void* node, s32 object);
extern char* strcat(char* dst, const char* src);
extern s32   StartFXNoLoop(s32 type, f32* pos);
extern void  CopyMat3(const f32* src, f32* dst);
extern void  WPitchMat3(f32* matrix, f32 angle);
extern void  WYawMat3(f32* matrix, f32 angle);
extern void  WRollMat3(f32* matrix, f32 angle);
extern char* sArrowObjectNames[];   /* arrow blit names by kind */
extern char  sLevelOneSuffix[];   /* "L1" */
extern char  sRootSuffix[];   /* "ROOT" */
extern char  sItemHealthTextureFmt[];   /* "%s%d" health-tier fmt */
extern f64   sPi;     /* pi (rounded) */
extern f64   sArrowFloorYOffset;     /* 0.5 */
extern f32   sArrowFloorRadius;
extern s32   sShownMilestones;   /* milestone shown idx */
extern s32   sShownCameras;   /* cameras shown idx */
extern s32   sNumMilestones;   /* milestone count */
extern s32   sNumTriggerCameras;   /* camera count */
extern u8    sMilestones[]; /* milestone table (stride 0x68: pos@0, handle@0x60) */
extern u8    sTriggerCameras[]; /* camera table (stride 0x28: type@0, a@4, b@0x14, handle@0x24) */
extern void* gWadAtreeHeaders[45]; /* wad atree headers */
extern f32   sItemZero;   /* waypoint dist epsilon */
extern f32     gDefaultPlayerPosition[3];
extern f32     gPlayerStartYaw;
extern s32     CurTransmitter;
extern char    sNewItemBadIndex[];

s32 ItemVisible(Item* item);

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
        for (i = 0, offset = 0; i < info_count; i++, offset += 0x50) {
            iteminfo* info = (iteminfo*)((u8*)infos + offset);
            s32 also_wads;

            if (info->type == 3) {
                also_wads = 1;
            } else {
                also_wads = 0;
            }
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
        s32 overlay_offset = 0;

        i = 0;
        offset = 0;
        do {
            u8* player_runtime;
            u8* overlay_runtime;
            s32* node_slot;
            s32 zero;

            player_runtime = runtime + offset;
            *(void**)(player_runtime + 0x74B8) =
                MBNewNode(sItemsRootNode, 0, 4);
            zero = 0;
            overlay_runtime = runtime + overlay_offset;
            *(s32*)(player_runtime + 0x74A8) = zero;
            *(s32*)(overlay_runtime + 0x74C8) = zero;
            *(s32*)(player_runtime + 0x7478) =
                MBOX_NewObject(sSeeThroughObjectName, 0, (s32)sItemsRootNode,
                               0x04200000);
            node_slot = (s32*)(player_runtime + 0x7478);
            MBTreeSetFlags((void*)*node_slot, 1, 0);
            *(s16*)(*node_slot + 0x68) = -800;
            i++;
            overlay_offset += 0x48;
            *(s32*)(player_runtime + 0x7498) = zero;
            offset += 4;
            *(s32*)(player_runtime + 0x7488) = zero;
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
    s32 instance_count = gWorldInfo.niteminsts;
    s32 visible_sum_coins = 0;
    s32 i;
    s32 instance_offset;
    f32 matrix[16];

    sItemRandSeed = pbLoad;
    fn_8005412C();
    gMaxItems = instance_count + 500;
    sItems = AllocMem(gMaxItems * sizeof(Item));

    for (i = 0, instance_offset = 0; i < instance_count;
         i++, instance_offset += sizeof(iteminst)) {
        iteminst* instance = (iteminst*)((u8*)instances + instance_offset);
        Item* item = NewItemPtr();

        if (instance->index < 0) {
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
        if (item != NULL && item->info != NULL && item->info->type == 1 &&
            item->info->item.subtype == 1 && ItemVisible(item)) {
            visible_sum_coins++;
        }
    }
    sVisibleSumCoinCount = visible_sum_coins;
    fn_80062A00();
    {
        s32 item_offset;
        for (i = 0, item_offset = 0; i < sNumItems;
             i++, item_offset += sizeof(Item)) {
            AddItemSub((Item*)((u8*)sItems + item_offset));
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
    if (sLevelAmbient * sLevelAmbientScale + AmbientSpecialCurValue < sAmbientMinimum) {
        lit = sAmbientMinimum;
    } else if (sLevelAmbient * sLevelAmbientScale + AmbientSpecialCurValue > sAmbientMaximum) {
        lit = sAmbientMaximum;
    } else {
        lit = sLevelAmbient * sLevelAmbientScale + AmbientSpecialCurValue;
    }
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
    Item* other;
    s32 i;
    s32 j;
    s32 duplicate_count;

    item = sItems;
    for (i = 0; i < sNumItems; i++, item++) {
        if (item->active != -1 && item->info->type == 5) {
            duplicate_count = 0;
            other = sItems;
            for (j = 0; j < sNumItems; j++, other++) {
                if (j != i && other->active != -1 &&
                    other->info->type == 5 &&
                    (*(s16*)&other->data[4] & 0x40) ==
                        (*(s16*)&item->data[4] & 0x40) &&
                    *(s8*)&item->data[6] > 0) {
                    if (*(s8*)&other->data[6] == *(s8*)&item->data[6]) {
                        *(s8*)&other->data[6] = 0;
                        duplicate_count++;
                    }
                }
            }
            if (duplicate_count > 0) {
                if (*(s16*)&item->data[4] & 0x40) {
                    ErrorPrintf(strings + 0x2EC,
                                duplicate_count + 1,
                                (s32)*(s8*)&item->data[6]);
                } else {
                    ErrorPrintf(strings + 0x310,
                                duplicate_count + 1,
                                (s32)*(s8*)&item->data[6]);
                }
            }
        }
    }

    item = sItems;
    for (i = 0; i < sNumItems; i++, item++) {
        if (item->active != -1 && item->info->type == 5) {
            s8 next_id = *(s8*)&item->data[7];

            if (next_id != 0) {
                other = sItems;
                for (j = 0; j < sNumItems; j++, other++) {
                    if (j != i && other->active != -1 &&
                        other->info->type == 5 &&
                        (*(s16*)&other->data[4] & 0x40) == 0 &&
                        next_id == *(s8*)&other->data[6]) {
                        Item* chain = other;
                        s32 loop = 0;

                        while (chain != NULL) {
                            Item* next = *(Item**)&chain->data[8];
                            if (next == item) {
                                ErrorPrintf(strings + 0x32C,
                                            (s32)next_id,
                                            (s32)*(s8*)&other->data[7]);
                                loop = 1;
                                break;
                            }
                            chain = next;
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
    iteminfo* def = gWorldInfo.iteminfo;
    iteminfo* d;
    Item*     item;
    s32       i;

    for (i = 0; i < gWorldInfo.niteminfos; i++) {
        iteminfodata* body = &def->item;
        if (strcmp(name, body->desc) == 0 && type == def->type &&
            (level <= 0 || level == body->subtype)) {
            goto found;
        }
        def++;
    }
    i = -1;
found:
    if (i < 0) {
        ErrorPrintf(sUnableToAddItemFmt, name);
        item = NULL;
    } else {
        d = &gWorldInfo.iteminfo[i];
        item = NewItemPtr();
        if (matrix != NULL) {
            SetItem(item, 0, d, matrix);
            AddItemSub(item);
        } else {
            SetItem(item, 0, d, gIdentityMatrix);
        }
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
    s32 count = 0;
    s32 i = 0;
    Item* it;
    s32 off = 0;

    while (i < sNumItems) {
        it = (Item*)((u8*)sItems + off);
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
        off += 240;
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

    best = lbl_80347038;
    if ((s32)gGameMode == 0x8008) {
        f32 sphere[4];
        volatile f32 root;
        f32 dy;
        f32 dx;
        f32 dz;
        f64 distance;

        sphere[0] = position[0];
        sphere[1] = position[1];
        sphere[2] = position[2];
        sphere[3] = sItemZero;
        dy = *(f32*)(gCameras + 0x38) - sphere[1];
        dx = *(f32*)(gCameras + 0x34) - sphere[0];
        dz = *(f32*)(gCameras + 0x3C) - sphere[2];
        distance = dx * dx + dy * dy + dz * dz;
        if (distance > sItemZero) {
            f64 estimate = __frsqrte(distance);
            estimate = sArrowFloorYOffset * estimate *
                       (lbl_80346FC8 - estimate * estimate * distance);
            estimate = sArrowFloorYOffset * estimate *
                       (lbl_80346FC8 - estimate * estimate * distance);
            estimate = sArrowFloorYOffset * estimate *
                       (lbl_80346FC8 - estimate * estimate * distance);
            root = (f32)(distance *
                         (sArrowFloorYOffset * estimate *
                          (lbl_80346FC8 - estimate * estimate * distance)));
            distance = root;
        }
        if (distance < lbl_80347120 &&
            MBWorldSphereVisible3(sphere, lbl_80346F50) != 0) {
            best = lbl_8034709C;
        }
    } else {
        u8* player = gPlayers;
        s32 i;

        for (i = 0; i < 4; i++, player += 0x335C) {
            if (*(s32*)(player + 0xE8) == 1) {
                volatile f32 root;
                f32 dy = *(f32*)(player + 0x48) - position[1];
                f32 dx = *(f32*)(player + 0x44) - position[0];
                f32 dz = *(f32*)(player + 0x4C) - position[2];
                f64 distance = dx * dx + dy * dy + dz * dz;

                if (distance > sItemZero) {
                    f64 estimate = __frsqrte(distance);
                    estimate = sArrowFloorYOffset * estimate *
                               (lbl_80346FC8 -
                                estimate * estimate * distance);
                    estimate = sArrowFloorYOffset * estimate *
                               (lbl_80346FC8 -
                                estimate * estimate * distance);
                    estimate = sArrowFloorYOffset * estimate *
                               (lbl_80346FC8 -
                                estimate * estimate * distance);
                    root = (f32)(distance *
                                 (sArrowFloorYOffset * estimate *
                                  (lbl_80346FC8 -
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
    u8* enemy = gEnemies;
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
            if (type > 233) {
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

/* 0x80063DB0 - retexture a damageable item by health tier (name + tier
 * digit, falling back to name+"L1"/"ROOT"), blanking it at tier 0. */
static void AddItemWobj(Item* it)
{
    char buf[32];
    s16 tier;
    s32 hp = it->health;
    s32 base = it->info->item.hitpoints;

    if (hp == 0) {
        tier = 0;
    } else if (hp > base) {
        if (hp > base << 1) {
            tier = 3;
        } else {
            tier = 2;
        }
    } else {
        tier = 1;
    }
    if (tier != *(s16*)(it->data + 2)) {
        s32 tex;
        *(s16*)(it->data + 2) = tier;
        sprintf(buf, sItemHealthTextureFmt, it->info->item.desc,
                *(s16*)(it->data + 2));
        tex = MBOX_ReallyFindObject(buf, -1, -1, -1);
        if (tex < 0) {
            strcat(buf, sLevelOneSuffix);
            tex = MBOX_ReallyFindObject(buf, -1, -1, -1);
        }
        if (tex < 0) {
            strcat(buf, sRootSuffix);
            tex = MBOX_ReallyFindObject(buf, -1, -1, -1);
        }
        if (tex < 0) {
            MBTreeSetFlags(it->objgrp.node, 1, 1);
            *(s16*)(it->data + 2) = -1;
        } else {
            MBSetObject(it->objgrp.node, tex);
            if (tier == 0) {
                *(u16*)((u8*)it + 0x62) &= ~1u;
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
        if (val == minp) {
            visible = 1;
        } else {
            visible = 0;
        }
    } else {
        if (val >= minp) {
            visible = 1;
        } else {
            visible = 0;
        }
    }
    return visible;
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
    u32 result;

    if (mod != 0) {
        result = (((u32)sItemRandSeed >> 5) + (u32)n) % (u32)mod;
    } else {
        result = 0;
    }
    if (advance != 0) {
        sItemRandSeed += 439;
    }
    return (s32)result;
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
    f32* posY = (f32*)(base + 3092);

    if (idx > sLastPlayerStart) {
        idx = 0;
    }
    if ((double)posY[idx * 3] <= sInvalidPlayerStartY) {
        idx = 0;
    }
    if (WorldOpen(crystal_order[idx]) == 0) {
        idx = 0;
    }
    gDefaultPlayerPosition[0] = *(f32*)(base + idx * 12 + 3088);
    gDefaultPlayerPosition[1] = posY[idx * 3];
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
                        if (*(void**)(runtime + old * 0x68 + 0x3E74) !=
                            NULL) {
                            MBTreeClearFlags(
                                *(void**)(runtime + old * 0x68 + 0x3E74),
                                2, 0);
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
                if (*(void**)(runtime + milestone_index * 0x68 + 0x3E74) !=
                    NULL) {
                    if ((sShownMilestones & (1 << i)) != 0) {
                        MBTreeClearFlags(
                            *(void**)(runtime + milestone_index * 0x68 +
                                     0x3E74),
                            2, 0);
                    } else {
                        MBTreeSetFlags(
                            *(void**)(runtime + milestone_index * 0x68 +
                                     0x3E74),
                            2, 0);
                    }
                }
            }
        }
    }
}

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
    s32 off;
    s32 i;

    if (idx < 0) {
        return old;
    }
    sShownMilestones = idx;
    if (idx != old) {
        base = sMilestones;
        for (i = 0, off = 0; i < sNumMilestones; i++, off += 0x68) {
            u8* elem = base + off;
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
    s32 off;
    s32 i;
    f32 tmp[17];

    if (idx < 0) {
        return old;
    }
    sShownCameras = idx;
    if (idx != old) {
        base = sTriggerCameras;
        for (i = 0, off = 0; i < sNumTriggerCameras; i++, off += 0x28) {
            u8* elem = base + off;
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
    LookoutParam* w = sLookoutParams;
    LookoutParam* result = NULL;
    u8 unused[12];
    volatile f32 root;
    s32 i;

    for (i = 0; i < sNumLookoutParams; i++, w++) {
        if (all != 0 || (w->next >= 0 && w->next != i)) {
            f32 dy = w->pos[1] - pos[1];
            f32 dx = w->pos[0] - pos[0];
            f32 dz = w->pos[2] - pos[2];
            f64 d2 = dx * dx + dy * dy + dz * dz;
            if (d2 > sItemZero) {
                f64 guess = __frsqrte(d2);
                guess = 0.5 * guess * (3.0 - guess * guess * d2);
                guess = 0.5 * guess * (3.0 - guess * guess * d2);
                guess = 0.5 * guess * (3.0 - guess * guess * d2);
                root = (f32)(d2 * (0.5 * guess * (3.0 - guess * guess * d2)));
                d2 = root;
            }
            if (d2 < maxDist) {
                result = w;
                maxDist = d2;
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
