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
extern int   fn_8005A260(const char* name, void** out, int a, int b); /* LoadResource */
extern void  InitTexMods(void* buf, int handle);                       /* fn_8007xxxx */
extern void  DoPlayerTexMods(int idx);                                     /* per-slot texmod */
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
extern s32            lbl_80344764;
extern u32            lbl_8034477C;
extern s32            lbl_80257590[12];
extern char           lbl_80112D98[];

extern s32            lbl_80344568;
extern s32            lbl_803447BC;
extern s32            lbl_8034457C;
extern s32            lbl_8034495C;
extern double         lbl_80347118;
extern char           lbl_80112D38[];
extern char           lbl_80112D44[];

extern void  FatalError(const char* msg, s32 code);
extern void* memset(void* dst, s32 val, u32 n);
extern u32*  FindWORLDOBJ(const char* name);
extern double fn_8006366C(f32* pos);
extern void  fn_80064154(Item* item);
extern void  fn_800115D0(void* p);
extern void  fn_800BAEAC(s32 handle, s32 flag);
extern s32   fn_800BA2C4(void* node, s32 a, s32 b);
extern void  AddItemWobj(Item* item);
extern s32   PlayerSelecting(s32 idx);
extern s32   gNumEnemies;
extern u8    gEnemies[];
extern u8    lbl_80275AE0[];
extern s32   WorldOpen(s32 handle);
extern char  lbl_80112E24[];
extern char  lbl_80112D20[];
extern char  lbl_80112FC8[];
extern s32   sMusicTrackHi;
extern int   strcmp(const char* a, const char* b);

extern s32     lbl_803448FC;
extern double  lbl_80347160;
extern u8*     gCurLevel;
extern f32     lbl_80344998;
extern f32     lbl_8034499C;
extern f32     lbl_80344990;
extern f32     lbl_80344994;
extern f32     lbl_8034498C;
extern f32     lbl_80344980;
extern f32     lbl_80344984;
extern f32     lbl_80344988;
extern f32     lbl_80347180;
extern f32     lbl_80347184;
extern f32     lbl_80347188;

extern void  MBInitLights(void);
extern void  MBAddLight(double val, void* a, f32* b);
extern void  MBSetAmbient(double val, f32* p);
extern void  fn_8006799C(s32 flag);
extern s32     lbl_80124D14[14];
extern f32     lbl_802757D4[3];
extern f32     lbl_80344B18;
extern s32     lbl_80344914;

/* ------------------------------------------------------------------ */
/* item pool                                                          */
/* ------------------------------------------------------------------ */

/* allocate the next free item slot, scanning from gNextItemIdx. */
Item* NewItemPtr(void)
{
    s16   gridnext;
    s32   i;
    Item* it;

    for (i = gNextItemIdx; i < lbl_8034494C; i++) {
        if (sItems[i].active == -1) {
            break;
        }
    }
    if (i >= gMaxItems) {
        FatalError(lbl_80112D98, 0x800000);
    }
    if (i == lbl_8034494C) {
        lbl_8034494C++;
    }
    gNextItemIdx = i + 1;
    it = &sItems[i];
    gridnext = it->gridnext;
    memset(it, 0, 240);
    it->ctriidx = -1;
    it->gridnext = gridnext;
    return it;
}

/* (re)build the level lights and ambient from the current level record. */
void fn_80067904(s32 flag)
{
    MBInitLights();
    if (flag != 0) {
        lbl_80344998 = *(f32*)(gCurLevel + 236);
        MBAddLight(*(f32*)(gCurLevel + 264), gCurLevel + 240,
                   (f32*)(gCurLevel + 252));
    } else {
        lbl_80344998 = lbl_80347180;
    }
    lbl_8034499C = lbl_80347180;
    MBSetAmbient(lbl_80344998, NULL);
    fn_8006799C(1);
    lbl_80344990 = lbl_80347188;
    lbl_80344994 = lbl_80347184;
    lbl_8034498C = lbl_80347184;
    lbl_80344980 = lbl_80347188;
    lbl_80344984 = lbl_80347188;
    lbl_80344988 = lbl_80347188;
}

/* pair up transporter items by matching each one's dest id to another's id. */
void MatchTransporters(void)
{
    s32   i;
    Item* p;
    s32   j;
    Item* q;

    p = sItems;
    for (i = 0; i < lbl_8034494C; i++, p = (Item*)((u8*)p + 240)) {
        if (p->active != -1 && p->info->type == 11) {
            q = sItems;
            for (j = 0; j < lbl_8034494C; j++, q = (Item*)((u8*)q + 240)) {
                if (j != i && q->active != -1 && q->info->type == 11 &&
                    ((s32*)p)[0x38] == ((s32*)q)[0x37]) {
                    ((Item**)p)[0x39] = q;
                    break;
                }
            }
            if (j >= lbl_8034494C) {
                ErrorPrintf(lbl_80112E24, ((s32*)p)[0x37], ((s32*)p)[0x38]);
            }
        }
    }
}

/* tear down an item (and its parented anim item for type 1/2), freeing its
 * psys/node handles and rewinding the free-scan cursor. */
void fn_80063ABC(Item* item, s32 flag)
{
    u8* e;
    s32 idx;

    if (flag != 0) {
        if (item->info->type == 1 && (e = *(u8**)((u8*)item + 0xE8)) != NULL) {
            if (*(u32*)(e + 0x6C) != 0) {
                fn_800115D0(e + 0x6C);
                *(u32*)(e + 0x6C) = 0;
            }
            if (*(u32*)(e + 0x64) != 0) {
                fn_800BAEAC(*(u32*)(e + 0x64), 0);
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
                fn_800115D0(e + 0x6C);
                *(u32*)(e + 0x6C) = 0;
            }
            if (*(u32*)(e + 0x64) != 0) {
                fn_800BAEAC(*(u32*)(e + 0x64), 0);
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
        fn_800115D0(e + 0x6C);
        *(u32*)(e + 0x6C) = 0;
    }
    if (*(u32*)(e + 0x64) != 0) {
        fn_800BAEAC(*(u32*)(e + 0x64), 0);
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
        ErrorPrintf(lbl_80112D20, name);
        item = NULL;
    } else {
        d = &gWorldInfo.iteminfo[i];
        item = NewItemPtr();
        if (matrix != NULL) {
            SetItem(item, 0, d, matrix);
            fn_80064154(item);
        } else {
            SetItem(item, 0, d, lbl_80127D60);
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
        fn_80064154(item);
    } else {
        SetItem(item, 0, a1, lbl_80127D60);
    }
    return item;
}

/* boss-specific fixup: re-hide a level prop for a couple of boss ids. */
void fn_80063C58(void)
{
    u32* obj;

    switch (gBossType) {
    case 42:
        lbl_80344958++;
        break;
    case 41:
        obj = FindWORLDOBJ(lbl_80112D38);
        if (obj != NULL && obj[10] != 0) {
            fn_800BA368((void*)obj[10], 1, 0);
        }
        break;
    case 44:
        obj = FindWORLDOBJ(lbl_80112D44);
        if (obj != NULL && obj[10] != 0) {
            fn_800BA368((void*)obj[10], 1, 0);
        }
        break;
    }
}

/* collect indices of type-10 items in state 0x29 (up to max); flag hides. */
s32 fn_80063F10(s32* out, s32 max, s32 flag)
{
    s32 count = 0;
    s32 i = 0;
    Item* it;
    s32 off = 0;

    while (i < lbl_8034494C) {
        it = (Item*)((u8*)sItems + off);
        if (it->info->type == 10 && *(s16*)((u8*)it + 0xDC) == 0x29) {
            out[count] = i;
            if (flag != 0) {
                fn_800BA368(it->objgrp.node, 1, 1);
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
s32 fn_800635B4(Item* it, f32* pos, s32 a3, s32 a4)
{
    u8* p = (u8*)it + 0xDC;
    s32 v;

    if ((lbl_80344568 | lbl_803447BC) != 0) {
        return 0;
    }
    v = *(s16*)(p + 8);
    if (v > 0) {
        *(s16*)(p + 8) = v - lbl_8034457C;
        return 0;
    }
    if (a3 <= 0) {
        return 0;
    }
    if (lbl_8034495C != 0 && a4 == 0) {
        return 0;
    }
    if ((s32)lbl_8034477C == 0x400C) {
        return 0;
    }
    if (fn_8006366C(pos) > lbl_80347118) {
        return 0;
    }
    return 1;
}

/* is this object claimed by a selecting player (ret 2) or a live enemy (1)? */
s32 fn_80063854(void* owner, s32 checkEnemies)
{
    u8* player = lbl_80275AE0;
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
    u8*  base = (u8*)&lbl_802577F0;
    Item* p;
    s32  i;
    s16  sidx;

    if (sMusicTrackHi == 13) {
        if (type > 200) {
            if (type == 201) {
                *(void**)(base + 5584) = (void*)(base + idx * 40 + 5652);
            }
            if (type == 202) {
                lbl_80344908[0] = (TriggerCamera*)(base + idx * 40 + 5652);
            }
            if (type == 203) {
                *(void**)(base + 5588) = (void*)(base + idx * 40 + 5652);
            }
            if (type == 204) {
                lbl_80344908[1] = (TriggerCamera*)(base + idx * 40 + 5652);
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
            lbl_803448E4 = (TriggerCamera*)(base + idx * 40 + 5652);
            return;
        }
        if (type >= 170 && type < 184) {
            *(void**)(base + type * 4 + 4848) = (void*)(base + idx * 40 + 5652);
            return;
        }
    }
    sidx = idx;
    p = sItems;
    for (i = 0; i < lbl_8034494C; i++, p = (Item*)((u8*)p + 240)) {
        if (p->active != -1 && p->info->type == 5 &&
            *(s8*)((u8*)p + 0xE2) == type) {
            s16 cur = *(s16*)((u8*)p + 0xEE);
            if (cur >= 0) {
                ErrorPrintf(lbl_80112FC8, i, cur, idx);
            }
            *(s16*)((u8*)p + 0xEE) = sidx;
        }
    }
}

/* (re)arm an item's combat stats from its info and (re)attach its wobj. */
void fn_80063D40(s32 idx)
{
    Item* it = &sItems[idx];

    fn_800BA2C4(it->objgrp.node, 1, 1);
    it->health = it->info->item.hitpoints * 3;
    *(s16*)((u8*)it + 0xDE) = 0;
    it->armor = (s8)it->info->item.armor;
    AddItemWobj(it);
}

/* item is "hot": has hitpoints and a positive damage/state field. */
s32 fn_80063D0C(s32 idx)
{
    Item* it = &sItems[idx];

    if (it->health > 0 && *(s16*)((u8*)it + 0xDE) > 0) {
        return 1;
    }
    return 0;
}

/* minimum-player gating check for an item's opener requirement. */
s32 fn_80065D98(Item* it)
{
    s32 val  = lbl_80344764;
    s32 minp = it->minplayers;
    s32 useEq = 0;

    if (lbl_8034477C & 0x8000) {
        val = 2;
    }
    if (minp > 10) {
        useEq = 1;
        minp -= 10;
    }
    if (lbl_80257590[4] > 0) {
        val = lbl_80257590[4];
        useEq = 1;
    }
    if (useEq) {
        if (val == minp) {
            return 1;
        }
        return 0;
    }
    if (minp <= val) {
        return 1;
    }
    return 0;
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

void SetPlayerStartPos(s32 idx)
{
    u8*  base = (u8*)&lbl_802577F0;
    f32* posY = (f32*)(base + 3092);

    if (idx > lbl_803448FC) {
        idx = 0;
    }
    if ((double)posY[idx * 3] <= lbl_80347160) {
        idx = 0;
    }
    if (WorldOpen(lbl_80124D14[idx]) == 0) {
        idx = 0;
    }
    lbl_802757D4[0] = *(f32*)(base + idx * 12 + 3088);
    lbl_802757D4[1] = posY[idx * 3];
    lbl_802757D4[2] = *(f32*)(base + idx * 12 + 3096);
    lbl_80344B18 = *(f32*)(base + idx * 4 + 3032);
    if (*(u32*)(base + idx * 4 + 5596) == 0) {
        idx = 0;
    }
    lbl_80344914 = *(s32*)(base + idx * 4 + 5596);
}

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
