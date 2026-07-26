#include "types.h"

/* Gauntlet world-object/scene module (Xbox WORLD.OBJ), region
 * 0x800A87C8 - 0x800AB8E0.  Wired NonMatching so the tree stays green while
 * the real Xbox-PDB symbols are mapped onto the DOL bytes.  This TU manages
 * the loaded "worlds/<name>" scene: a tree of WORLDOBJ nodes (child at +44,
 * sibling at +46, stride 0x3C), their g3d display nodes, particle systems
 * ("PSYS" tagged objects), keyframe animation, and the save/restore of each
 * object's initial transform.
 *
 * The three module-level data globals match the Xbox WORLD.OBJ data symbols:
 *   world_load_state  0x80344D68 - load state machine (0..5) driven by StartWorldLoad
 *   atree_finfo       0x80344D6C - animation-tree file read handle
 *   WorldDisplay      0x80344D70 - display mode; ToggleWorldDisplay maps it to
 *                                  which of the two scene roots are visible
 *
 * IDENTIFIED functions (real WORLD.OBJ names; HIGH confidence unless noted).
 * Evidence = rodata strings ("ALLOC World Save State/Data", "World psys: bad
 * name", "World obj with dynamic parent", "worlds"/"anim"/"PSYS"), call graph,
 * and behaviour:
 *   DoWorldAnimSub        fn_800A87C8 - advance one object's keyframe animation
 *   NewWorld              fn_800A8E6C - create the two scene-root display nodes   [med]
 *   WorldSaveInitState    fn_800A8ECC - alloc + snapshot each object's init xform
 *   WorldRestoreInitState fn_800A9088 - restore the snapshot taken above
 *   ToggleWorldDisplay    fn_800A91B0 - show/hide scene roots per WorldDisplay    [med]
 *   FinishLoadWorldAnim   fn_800A92A0 - poll/close the atree_finfo read           [med]
 *   StartLoadWorldAnim    fn_800A92FC - begin reading "anim" file into atree_finfo
 *   StartWorldLoad        fn_800A93C4 - multi-file "worlds" load state machine    [med]
 *   BGLoadWorldFile       fn_800A95AC - background file-read progress callback
 *   LoadWorldDone         fn_800A95CC - finish load: alloc, copy name, report mem
 *   FindWORLDOBJ          fn_800A9C50 - find object by name from current world
 *   FindWorldObject       fn_800A9CFC - recursive by-name subtree search          [med]
 *   InitWorldInfo         fn_800A9E1C - alloc + init a world-data block ("World Data")
 *   WorldObjGetAllFlags   fn_800AB47C - OR the flag word down an object list
 *   CreateWorldNode       fn_800AB4A0 - recursively build g3d nodes for the tree
 *   WorldPsysDeActivate   fn_800AB5A4 - free an object's particle system
 *   WorldPsysActivate     fn_800AB5FC - look up "PSYS" name, spawn the particle sys
 *   NewWorldObject        fn_800AB780 - create one object's g3d node
 *
 * Left fn_XXXXXXXX (GC-specific / genuinely ambiguous; see session report):
 *   fn_800A8CD8 nearest-object-to-point search (distance, no clean PDB name)
 *   fn_800A8E30 world-globals reset (InitWorlds vs ResetWorlds ambiguous)
 *   fn_800A9088 restore helper, fn_800A9378/fn_800A9398 tiny model-load wrappers
 *   fn_800A971C big-endian byte-swap of a loaded world header (GC-only fixup)
 *
 * Only a compilable subset of bodies is transcribed; matching is out of scope
 * for this pass (NonMatching, DOL bytes substituted).  See symbols.txt for the
 * authoritative address->name mapping. */

/* --- WORLDINFO scene struct at 0x8028C9A8 (>= 0x180 bytes; +232 obj array,
 *     +324 obj count, +368/+372 psys array/count, +64/+196/+360 roots) --- */
typedef struct WorldObj WorldObj;
struct WorldObj {
    char name[6];      /* +0  strcmp key */
    s16 pyr;           /* +6  packed flags/orientation */
    /* ... */
    float x;           /* +28 */
    float y;           /* +32 */
    float z;           /* +36 */
    u32 flags;         /* +16 */
    void* node;        /* +40 g3d display node */
    void* next;        /* +24 sibling list link used by WorldObjGetAllFlags */
    s16 child;         /* +44 first-child index (-1 = none) */
    s16 sibling;       /* +46 next-sibling index (-1 = none) */
};

/* --- module data (names authoritative in symbols.txt) --- */
extern s32 world_load_state; /* 0x80344D68 */
extern s32 atree_finfo;      /* 0x80344D6C (file read handle) */
extern s32 WorldDisplay;     /* 0x80344D70 */

/* g3d node API (dolphin/g3d region 0x800BAxxx-0x800BExxx) */
extern void* fn_800BB29C();  /* create child node */
extern void  fn_800BA368();  /* set node flag / show */
extern void  fn_800BA2C4();  /* clear node flag / hide */

/* WorldObjGetAllFlags: OR the +16 flag word across the +24 sibling list. */
u32 WorldObjGetAllFlags(WorldObj* o) {
    u32 acc = 0;
    do {
        acc |= o->flags;
        o = (WorldObj*)o->next;
    } while (o != 0);
    return acc;
}

/* BGLoadWorldFile: streamed-read progress callback.  status(+16)==2 -> done;
 * otherwise advance the running offset (+4) by the last chunk size (+8). */
void BGLoadWorldFile(s32* h) {
    if (h[4] == 2) {
        return;
    }
    h[1] = h[1] + h[2];
}
