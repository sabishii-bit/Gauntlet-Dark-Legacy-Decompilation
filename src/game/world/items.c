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
    u8   _pad0000[0xBB8];
    char itemPath[0x100];
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

extern ItemRuntime    lbl_802577F0;
extern f32            lbl_80258400[14][3];
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
extern f32            lbl_80346F3C;
extern f64            lbl_80346F48;
extern f32            lbl_80346EE0;
extern f64            lbl_80346FC0;
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
extern iteminfo*       lbl_80344930;
extern s32             lbl_80344934;
extern s32             lbl_80344938;
extern s32             lbl_8034493C;
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
extern void  MBRemoveNode(s32 handle, s32 flag);
extern s32   MBTreeClearFlags(void* node, s32 a, s32 b);
extern void  MBNodeSetParent(void* node, void* parent);
extern void  fn_8005A3B8(OBJGRP* group);
extern void  AddItemWobj(Item* item);
extern s32   PlayerSelecting(s32 idx);
extern s32   gNumEnemies;
extern u8    gEnemies[];
extern u8    lbl_80275AE0[];
extern ItemSceneContext lbl_8023CAE0;
extern s64   lbl_803445C8;
extern char  lbl_80112D68[];
extern char  lbl_80112FF4[0xB];
extern char  lbl_80346FF4[8];
extern char  lbl_80347168[8];
extern char  lbl_80347170[8];
extern s32   WorldOpen(s32 handle);
extern char  lbl_80112E24[];
extern char  lbl_80112D20[];
extern char  lbl_80112FC8[];
extern s32   sMusicTrackHi;
extern int   strcmp(const char* a, const char* b);

static u32 AtreeMatchAnyHeader(char* name, s32 alsoWads);

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
extern void  fn_800C0CF4(void);
extern void  fn_800C0DF4(double a, double b);
extern void  fn_800C0DDC(double a, double b);
extern volatile f32 sMusicFadeBase;
extern f32   lbl_8034718C;
extern f64   lbl_80347190;
extern f64   lbl_80347198;
extern f64   lbl_803471A0;
extern f64   lbl_803471A8;
extern f64   lbl_803471B0;
extern s32     lbl_80124D14[14];
extern f64   __frsqrte(f64 value);
extern s32   add_arrow(s32 kind, s32 one, s32 alt, f32* a, f32* b, f32* pos);
extern void  fn_8006EC18(s32 idx);                        /* newcam hook */
extern s32   AtreeMatch(void* tree, char* name, s32 flag);
extern f32   fn_800BD3E8(f64 ang);                        /* angle wrap */
extern void  fn_800BD050(f32* mtx, f32* angles);          /* mtx from angles */
extern f64   FloorPos(f64 y, f32 r, f32* pos, s32 mode); /* ground probe */
extern s32   MBOX_NewObject(char* name, f32* mtx, s32 a, s32 b);
extern void  MBTreeSetAlpha(s32 handle, s32 pri, s32 b);
extern s32   fn_800B8E94(char* name, s32 a, s32 b, s32 c);/* find texture */
extern void* fn_800B92B0(void* node, s32 tex);
extern char* strcat(char* dst, const char* src);
extern void  fn_80093B04(s32 type, f32* pos);
extern char* lbl_8011C8A8[];   /* arrow blit names by kind */
extern char  lbl_80346F78[];   /* "L1" */
extern char  lbl_80346F7C[];   /* "ROOT" */
extern char  lbl_80347128[];   /* "%s%d" health-tier fmt */
extern f64   lbl_80347020;     /* pi (rounded) */
extern f64   lbl_80346FB0;     /* 0.5 */
extern f32   lbl_80346FFC;
extern s32   lbl_803448F4;   /* milestone shown idx */
extern s32   lbl_803448F8;   /* cameras shown idx */
extern s32   lbl_8034491C;   /* milestone count */
extern s32   lbl_80344918;   /* camera count */
extern u8    lbl_8025B604[]; /* milestone table (stride 0x68: pos@0, handle@0x60) */
extern u8    lbl_80258E04[]; /* camera table (stride 0x28: type@0, a@4, b@0x14, handle@0x24) */
extern void* lbl_80251364[45]; /* wad atree headers */
extern f32   lbl_80346EE4;   /* waypoint dist epsilon */
extern f32     lbl_802757D4[3];
extern f32     lbl_80344B18;
extern s32     lbl_80344914;

/* ------------------------------------------------------------------ */
/* item pool                                                          */
/* ------------------------------------------------------------------ */

void fn_80064154(Item* item)
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
        lbl_80346F48 + FloorPos(position[1], lbl_80346F3C,
                                   position, 0);

    current = &lbl_8023CAE0.current;
    if (*current == 0 && (lbl_803445C8 & 0x10) != 0) {
        ErrorPrintf(lbl_80112D68, item->info->item.desc,
                    position[0], position[1], position[2]);
    }

    if (*current != 0 && *(void**)((u8*)*current + 0x28) != 0 &&
        (*(u32*)((u8*)*current + 0x10) & 0x1000) != 0) {
        MBNodeSetParent(item->objgrp.node, *(void**)((u8*)*current + 0x28));
    }

    fn_8005A3B8(&item->objgrp);
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
    u8* runtime = (u8*)&lbl_802577F0;
    iteminfo* infos = gWorldInfo.iteminfo;
    s32 info_count = gWorldInfo.niteminfos;
    s32 i;
    s32 offset;

    lbl_80344930 = 0;
    lbl_8034493C = 0;
    lbl_80344938 = 0;
    lbl_80344934 = 0;

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
                lbl_80344930 = info;
            }
        }

        if (sGoodWizObj != 0) {
            lbl_8034493C = AtreeMatch(sGoodWizObj, lbl_80347168, 0);
            lbl_80344938 = AtreeMatch(sPowerupsBuf, lbl_80112FF4, 0);
            lbl_80344934 = AtreeMatch(sPowerupsBuf, lbl_80346FF4, 0);
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
                MBNewNode(lbl_8034497C, 0, 4);
            zero = 0;
            overlay_runtime = runtime + overlay_offset;
            *(s32*)(player_runtime + 0x74A8) = zero;
            *(s32*)(overlay_runtime + 0x74C8) = zero;
            *(s32*)(player_runtime + 0x7478) =
                MBOX_NewObject(lbl_80347170, 0, (s32)lbl_8034497C,
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

/* 0x8006799C - per-frame ambient light fade toward the level target. */
void fn_8006799C(s32 flag)
{
    u8 unused[16];
    f32 a;
    u8 unused2[8];
    f64 step;
    f64 lit;

    fn_800C0CF4();
    if (gCurLevel != NULL && (*(u32*)gCurLevel & 8)) {
        lbl_80344988 = lbl_8034718C;
        lbl_80344984 = lbl_8034718C;
    } else {
        if (lbl_80347190 != lbl_80344984 && sMusicFadeBase > lbl_80344980) {
            lbl_80344984 = (f32)(lbl_80344984 * lbl_80347198);
            a = lbl_80344984;
            *(u32*)&a &= 0x7FFFFFFF;
            if (a < lbl_803471A0) {
                lbl_80344984 = lbl_80347188;
            }
        }
    }
    if (lbl_80344984 != lbl_80344988) {
        if (lbl_80344984 - lbl_80344988 < lbl_803471A8) {
            step = lbl_803471A8;
        } else if (lbl_80344984 - lbl_80344988 > lbl_803471A0) {
            step = lbl_803471A0;
        } else {
            step = lbl_80344984 - lbl_80344988;
        }
        lbl_80344988 = lbl_80344988 + (f32)step;
    }
    if (lbl_80344998 * lbl_8034499C + lbl_80344988 < lbl_80347190) {
        lit = lbl_80347190;
    } else if (lbl_80344998 * lbl_8034499C + lbl_80344988 > lbl_803471B0) {
        lit = lbl_803471B0;
    } else {
        lit = lbl_80344998 * lbl_8034499C + lbl_80344988;
    }
    MBSetAmbient((f32)lit, NULL);
    fn_800C0DF4(lbl_80347180, lbl_80344988);
    fn_800C0DDC(lbl_80347180, lbl_80344988);
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
                if (lbl_80251364[i] != NULL) {
                    r = AtreeMatch(lbl_80251364[i], name, 0);
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
                fn_800115D0(e + 0x6C);
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
        fn_800115D0(e + 0x6C);
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
            MBTreeSetFlags((void*)obj[10], 1, 0);
        }
        break;
    case 44:
        obj = FindWORLDOBJ(lbl_80112D44);
        if (obj != NULL && obj[10] != 0) {
            MBTreeSetFlags((void*)obj[10], 1, 0);
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
        sprintf(buf, lbl_80347128, it->info->item.desc,
                *(s16*)(it->data + 2));
        tex = fn_800B8E94(buf, -1, -1, -1);
        if (tex < 0) {
            strcat(buf, lbl_80346F78);
            tex = fn_800B8E94(buf, -1, -1, -1);
        }
        if (tex < 0) {
            strcat(buf, lbl_80346F7C);
            tex = fn_800B8E94(buf, -1, -1, -1);
        }
        if (tex < 0) {
            MBTreeSetFlags(it->objgrp.node, 1, 1);
            *(s16*)(it->data + 2) = -1;
        } else {
            fn_800B92B0(it->objgrp.node, tex);
            if (tier == 0) {
                *(u16*)((u8*)it + 0x62) &= ~1u;
                it->armor = -1;
            }
        }
        if (tier == 0) {
            fn_80093B04(30, (f32*)((u8*)it + 0x34));
        }
    }
}

/* (re)arm an item's combat stats from its info and (re)attach its wobj. */
void fn_80063D40(s32 idx)
{
    Item* it = &sItems[idx];

    MBTreeClearFlags(it->objgrp.node, 1, 1);
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

    lbl_8034497C = MBNewNode(lbl_80344EB8, lbl_80127D60, 1);
    MBTreeSetFlags(lbl_8034497C, 4, 0);

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
    if (advance == 0) {
        return (s32)result;
    }
    sItemRandSeed += 439;
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

    best_distance = lbl_80346EE0;
    zero = lbl_80346FC0;
    minimum_y = lbl_80347160;
    result = 0;
    i = 0;

    do {
        f32* candidate = lbl_80258400[i];
        f32* candidate_y = candidate + 1;

        if ((f64)*candidate_y <= minimum_y) {
            goto next_start;
        }
        if (WorldOpen(lbl_80124D14[i]) == 0) {
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

/* 0x80066E6C - show/hide the level milestone arrows for player idx. */
s32 ShowMilestones(s32 idx)
{
    s32 old = lbl_803448F4;
    u8* base;
    s32 off;
    s32 i;

    if (idx < 0) {
        return old;
    }
    lbl_803448F4 = idx;
    if (idx != old) {
        base = lbl_8025B604;
        for (i = 0, off = 0; i < lbl_8034491C; i++, off += 0x68) {
            u8* elem = base + off;
            if (lbl_803448F4 != 0) {
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
    return lbl_803448F4;
}

/* 0x80066F48 - show/hide the trigger-camera arrows for player idx. */
s32 ShowCameras(s32 idx)
{
    s32 old = lbl_803448F8;
    u8* base;
    s32 off;
    s32 i;
    f32 tmp[17];

    if (idx < 0) {
        return old;
    }
    lbl_803448F8 = idx;
    if (idx != old) {
        base = lbl_80258E04;
        for (i = 0, off = 0; i < lbl_80344918; i++, off += 0x28) {
            u8* elem = base + off;
            s32 alt = 0;
            s32 kind = 1;
            if (*(u8*)elem == 1) {
                alt = 1;
                kind = 3;
            } else if (*(u8*)elem == 2) {
                kind = 2;
            }
            if (lbl_803448F8 != 0) {
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
        fn_8006EC18(lbl_803448F8);
    }
    return lbl_803448F8;
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
            ang2[1] = ang2[1] + lbl_80347020;
            ang2[1] = fn_800BD3E8(ang2[1]);
            ang2[0] = -ang2[0];
            ang2[0] = fn_800BD3E8(ang2[0]);
            fn_800BD050(mtx, ang2);
            angles = ang2;
        }
        fn_800BD050(mtx, angles);
        mtx[12] = look[0];
        mtx[13] = look[1];
        mtx[14] = look[2];
    }
    if (kind == 0) {
        mtx[13] = lbl_80346FB0 + FloorPos(mtx[13], lbl_80346FFC, mtx + 12, 0);
    }
    if (refresh != 0) {
        handle = MBOX_NewObject(lbl_8011C8A8[kind], mtx, 0, 0);
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
    LookoutParam* w = lbl_802584A8;
    LookoutParam* result = NULL;
    volatile f32 root;
    s32 i;
    u8 unused[16];

    for (i = 0; i < lbl_80344900; i++, w++) {
        if (all != 0 || (w->next >= 0 && w->next != i)) {
            f32 dy = w->pos[1] - pos[1];
            f32 dx = w->pos[0] - pos[0];
            f32 dz = w->pos[2] - pos[2];
            f64 d2 = dz * dz + dx * dx + dy * dy;
            if (d2 > lbl_80346EE4) {
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
