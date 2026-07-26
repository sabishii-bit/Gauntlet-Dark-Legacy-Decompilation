#include "types.h"
#include "game/item.h"

/* Gauntlet item / world-object system (Xbox ITEMS.OBJ), region
 * 0x800631AC-0x80067AE0 -- the whole gap between gauntworld.c and main.c.
 *
 * This is the GameCube retail slice of ITEMS.OBJ.  The Xbox shell3D.pdb debug
 * build lists ~100 functions in this module; the retail GC build keeps ~51
 * (editor/debug display helpers -- ShowCameras, ShowMilestones, print_*,
 * get_info_type -- and most of the collision/damage bulk are inlined or
 * stripped).  Wired NonMatching so the tree stays green while symbols are
 * mapped; matching is out of scope for this pass.
 *
 * IDENTIFIED (real Xbox-PDB names; string / call-graph / behaviour anchored):
 *   InitItems              fn_800678E8 - init the four item-file handle/flag
 *                                        slots to (loaded=0, handle=-1)
 *   ResetItems             fn_8006784C - alloc the item pool + zero the item
 *                                        state globals (per-level reset)
 *   LoadItems              fn_80067780 - sprintf level object-file names and
 *                                        load the two item/object blobs
 *   UnloadWeaponsPowerups  fn_8006772C - mark weapon+powerup handles unloaded
 *   LoadWeapons            fn_800676EC - load the shared weapons resource file
 *   LoadPowerups           fn_800676A0 - load the shared powerups file
 *                                        (defaults to "powerups")
 *   RandItemIdx            fn_800675F8 - deterministic (seed>>5+n) % mod picker
 *   FindLookoutParam       fn_800671A8 - lookup a camera "lookout" param by id
 *                                        ("CAN'T FIND LOOKPUT PARAM:%d")
 *   AddLocatorInstList     fn_8006626C - parse placed-object records, dispatch
 *                                        by type (AddBoss, link cameras, ...)
 *   LinkTriggerToCam       fn_80066C90 - bind a trigger volume to a camera
 *                                        ("Two cameras link to trigger %d")
 *   SetItem                fn_800646F8 - configure an item from its type/name
 *                                        (big type switch; AtreeMatch, etc.)
 *   MatchTransporters      fn_80064614 - pair transporter items to their dests
 *                                        ("Transporter id %d: no dest %d")
 *   NewItemPtr             fn_800642C8 - allocate the next item slot ("> MAX
 *                                        ITEMS" FatalError, memset the slot)
 *   PlaceItem              fn_80063928 - place a named item, calls SetItem
 *                                        ("Unable to AddItem '%s'")
 *
 * The remaining ~37 functions in the slice are item gameplay / placement /
 * camera / exit / music helpers left as fn_XXXXXXXX (see session report for
 * the candidate list).  Only the small, unambiguous loader/init functions are
 * reconstructed below.
 */

/* ------------------------------------------------------------------ */
/* externs                                                            */
/* ------------------------------------------------------------------ */
extern int  LoadResource(const char* name, void** out, int a, int b); /* fn_8005A260 */
extern int  FileExists(const char* name);                             /* fn_800BF70C */
extern void FreeResource(int tag, int handle);                        /* fn_80010B4C */
extern char* BuildLevelName(void);                                    /* fn_80057AC0 */
extern char* BuildAltLevelName(void);                                 /* fn_80057AB4 */
extern int  sprintf(char* buf, const char* fmt, ...);

/* item resource-file state (r13 / SDA relative, addresses in comments) */
extern int   sWeaponsHandle;     /* r13-0x7EB8 */
extern void* sWeaponsBuf;        /* r13-0x7190 */
extern int   sPowerupsHandle;    /* r13-0x7EB4 */
extern void* sPowerupsBuf;       /* r13-0x7194 */
extern int   sItemFile0Loaded;   /* r13-0x7188 */
extern int   sItemFile0Handle;   /* r13-0x7198 */
extern int   sItemFile1Loaded;   /* r13-0x718C */
extern int   sItemFile1Handle;   /* r13-0x719C */
extern int   sItemRandSeed;      /* r13-0x71C0 */

extern const char sPowerupsName[]; /* "powerups" @0x80113000 */
extern const char sWeaponsName[];  /* sdata2 name @r2-0x65A8 */

/* ------------------------------------------------------------------ */

void InitItems(void) {
    sItemFile0Loaded = 0;
    sItemFile0Handle = -1;
    sItemFile1Loaded = 0;
    sItemFile1Handle = -1;
}

void UnloadWeaponsPowerups(void) {
    sWeaponsHandle = -1;
    sPowerupsHandle = -1;
}

void LoadWeapons(void) {
    if (sWeaponsHandle < 0) {
        sWeaponsHandle = LoadResource(sWeaponsName, &sWeaponsBuf, 0, -1);
    }
}

void LoadPowerups(char* name) {
    if (name == NULL) {
        name = (char*)sPowerupsName;
    }
    if (sPowerupsHandle < 0) {
        sPowerupsHandle = LoadResource(name, &sPowerupsBuf, 0, -1);
    }
}

/* deterministic pseudo-random index in [0, mod).  advance != 0 steps the
 * shared item seed by 439 (matches the DOL: (seed>>5 + n) % mod). */
int RandItemIdx(int n, int mod, int advance) {
    int r = 0;
    if (mod != 0) {
        r = ((sItemRandSeed >> 5) + n) % mod;
    }
    if (advance != 0) {
        sItemRandSeed += 439;
    }
    return r;
}
