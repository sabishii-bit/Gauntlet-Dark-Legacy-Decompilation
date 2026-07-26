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
        sWeaponsHandle = fn_8005A260("weapons", &sWeaponsBuf, 0, -1);
    }
}

void LoadPowerups(char* name) {
    if (name == NULL) {
        name = "powerups";
    }
    if (sPowerupsHandle < 0) {
        sPowerupsHandle = fn_8005A260(name, &sPowerupsBuf, 0, -1);
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
    s32 r;
    if (mod != 0) {
        r = (s32)((((u32)sItemRandSeed >> 5) + (u32)n) % (u32)mod);
    } else {
        r = 0;
    }
    if (advance != 0) {
        sItemRandSeed += 439;
    }
    return r;
}

/* return the magic-bus scene node backing item[idx]. */
struct mbnode* ItemGetNode(s32 idx) {
    return sItems[idx].objgrp.node;
}
