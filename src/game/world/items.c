#include "types.h"
#include "game/item.h"

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
extern int   fn_8005A260(const char* name, void** out, int a, int b); /* LoadResource */
extern void  InitTexMods(void* buf, int handle);                       /* fn_8007xxxx */
extern void  fn_8007C230(int idx);                                     /* per-slot texmod */
extern int   ErrorPrintf(const char* fmt, ...);
extern int   sprintf(char* dst, const char* fmt, ...);
extern int   FileExists(const char* dev, const char* path);
extern void  TriggerCameraActivate(s32 type, f32* eye, f32* target,
                                   s32 duration, s32 flags, s32 variant);
extern char* fn_80057AB4(void);
extern char* fn_80057AC0(void);
extern void* fn_800BB29C(void* parent, f32* matrix, s32 flags);
extern void  fn_800BA368(void* node, s32 flags, s32 value);

typedef struct TriggerCamera {
    /* 0x00 */ u8  _pad00[4];
    /* 0x04 */ f32 eye[4];
    /* 0x14 */ f32 target[3];
} TriggerCamera;

typedef struct RuneCameraVariants {
    s32 value[3];
} RuneCameraVariants;

typedef struct ItemRuntime {
    u8   _pad0000[0xBB8];
    char itemPath[0x100];
} ItemRuntime;

typedef struct ItemStrings {
    char objectsFile[0x554];
    char file0Format[0xC];
    char file1Format[1];
} ItemStrings;

extern ItemRuntime    lbl_802577F0;
extern LookoutParam   lbl_802584A8[];
extern TriggerCamera* lbl_80258D18[3][14];
extern TriggerCamera* lbl_80258DC0[17];
extern RuneCameraVariants lbl_80112AF4;
extern char           lbl_80112D04[0x1C];
extern TriggerCamera* lbl_803448E4;
extern s32            lbl_80344900;
extern TriggerCamera* lbl_80344908[2];
extern s32            lbl_80346DC0;
extern s32            lbl_80346DC4;
extern char           sWeaponsName[8];
extern char           sPowerupsName[0x28];
extern ItemStrings    sObjectsFile;
extern s32            gBossType;
extern void*          lbl_80344EB8;
extern void*          lbl_8034497C;
extern f32            lbl_80127D60[16];
extern f32            lbl_80346EE4;
extern s32            lbl_8034494C;
extern s32            gMaxItems;
extern s32            gNextItemIdx;
extern s32            lbl_8034492C;
extern s32            lbl_80344928;
extern s32            lbl_803448E0;
extern s32            lbl_80344924;
extern s32            lbl_80344920;
extern s32            lbl_80344958;
extern s32            lbl_80344954;
extern u32            pbLoad;

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
        sWeaponsHandle = fn_8005A260(sWeaponsName, &sWeaponsBuf, 0, -1);
    }
}

void LoadPowerups(char* name) {
    if (name == NULL) {
        name = sPowerupsName;
    }
    if (sPowerupsHandle < 0) {
        sPowerupsHandle = fn_8005A260(name, &sPowerupsBuf, 0, -1);
    }
}

void LoadItems(void)
{
    ItemRuntime* runtime = &lbl_802577F0;
    ItemStrings* strings = &sObjectsFile;

    if (sItemFile0Handle < 0 && gBossType < 0) {
        sprintf(runtime->itemPath, strings->file0Format, fn_80057AC0());
        sItemFile0Handle =
            fn_8005A260(runtime->itemPath, &sGoodWizObj, 0, -1);
    }

    if (sItemFile1Handle < 0) {
        sprintf(runtime->itemPath, strings->file1Format, fn_80057AB4());
        if (FileExists(runtime->itemPath, strings->objectsFile)) {
            sItemFile1Handle =
                fn_8005A260(runtime->itemPath, &sItemFile1Buf, 0, -1);
        }
    }
}

void ResetItems(void)
{
    f32* runtime = (f32*)&lbl_802577F0;

    lbl_8034497C = fn_800BB29C(lbl_80344EB8, lbl_80127D60, 1);
    fn_800BA368(lbl_8034497C, 4, 0);

    {
        f32 initial = lbl_80346EE4;

        sItems = 0;
        lbl_8034494C = 0;
        gMaxItems = 0;
        gNextItemIdx = 0;
        lbl_8034492C = 0;
        lbl_80344928 = 0;
        lbl_803448E0 = 0;
        runtime[0x7214 / sizeof(f32)] = initial;
        runtime[0x7218 / sizeof(f32)] = initial;
        runtime[0x721C / sizeof(f32)] = initial;
        lbl_80344924 = 0;
        lbl_80344920 = 0;
        sItemRandSeed = pbLoad;
        lbl_80344958 = 0;
        lbl_80344954 = 0;
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
        fn_8007C230(i);
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

void GetMilestonePos(s32 idx, f32* out)
{
    u8* milestone = (u8*)&lbl_802577F0 + idx * 0x68;

    out[0] = *(f32*)(milestone + 0x3E44);
    out[1] = *(f32*)(milestone + 0x3E48);
    out[2] = *(f32*)(milestone + 0x3E4C);
}

LookoutParam* FindLookoutParam(s32 id)
{
    LookoutParam* param = lbl_802584A8;
    s32 count = lbl_80344900;
    s32 i;

    for (i = 0; i < count; i++) {
        if (param->id == id) {
            return param;
        }
        param++;
    }

    ErrorPrintf(lbl_80112D04, id, lbl_80344900);
    return 0;
}

LookoutParam* NextWaypoint(LookoutParam* waypoint)
{
    LookoutParam* result;
    s16 next = waypoint->next;

    if (next < 0) {
        result = 0;
    } else {
        result = &lbl_802584A8[next];
    }
    if (result == waypoint) {
        result = 0;
    }
    return result;
}

void CrystalCamActivate(void)
{
    TriggerCamera* camera = lbl_803448E4;
    TriggerCameraActivate(0, camera->eye, camera->target, 50, 0, 0);
}

void SumnerCamActivate(s32 idx, s32 sub)
{
    TriggerCamera* camera = lbl_80258D18[idx][sub];

    while (camera == 0 && idx > 0) {
        idx--;
        camera = lbl_80258D18[idx][sub];
    }
    if (camera != 0) {
        TriggerCameraActivate(0, camera->eye, camera->target, -1, 0, 0);
    }
}

void WindowCamActivate(s32 idx)
{
    s32 variants[2];
    TriggerCamera* camera = lbl_80344908[idx];

    variants[0] = lbl_80346DC0;
    variants[1] = lbl_80346DC4;
    if (camera == 0) {
        camera = lbl_80344908[0];
    }
    if (camera != 0) {
        TriggerCameraActivate(0, camera->eye, camera->target, 0, 0,
                              variants[idx]);
    }
}

void RuneCamActivate(s32 idx)
{
    TriggerCamera* camera = lbl_80258DC0[idx];
    RuneCameraVariants variants = lbl_80112AF4;

    while (camera == 0 && idx > 0) {
        idx--;
        camera = lbl_80258DC0[idx];
    }
    if (camera != 0) {
        TriggerCameraActivate(0, camera->eye, camera->target, 0, 0,
                              variants.value[idx]);
    }
}
