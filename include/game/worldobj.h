#ifndef GAME_WORLDOBJ_H
#define GAME_WORLDOBJ_H

#include "types.h"

/* WorldObj - one node in the Gauntlet world scene-graph (Xbox "WORLD.OBJ").
 *
 * Authoritative field names + offsets come from the Xbox shell3D.pdb dump:
 *   research/xbox_symbols/misc.h  ->  "struct worldobj // Size=0x3c (Id=3253)".
 *
 * Every offset below was re-verified against the GameCube (GUNE5D) DOL asm of
 * src/game/world/world.c (WorldObjGetAllFlags, NewWorldObject, CreateWorldNode,
 * FindWORLDOBJ) via tools/gdl/fnasm.py.  The GC layout is identical to the Xbox
 * layout - same fields, same offsets, same 0x3C stride (CreateWorldNode/
 * FindWORLDOBJ index the array with `mulli ...,60`).
 *
 * GC vs Xbox notes:
 *   - flags lives at 0x10 on BOTH targets (an earlier work-order guess of
 *     "flags@0" was wrong; asm reads `lwz r0,16(r3)`).
 *   - src/game/world/world.c's local struct named 0x2C "child" and 0x2E
 *     "sibling"; that is inverted.  The asm shows 0x2E is recursed into with the
 *     current node passed as parent (= childidx) while 0x2C is iterated at the
 *     same tree level (= nextidx), matching the authoritative Xbox names used
 *     here: nextidx@0x2C, childidx@0x2E.
 *   - The struct is fully packed on both targets: every field is naturally
 *     aligned and contiguous, so no explicit padding members are required.
 */

struct mbnode; /* g3d/mb display node (opaque here) */

typedef struct WorldObj {
    char           desc[16];       /* 0x00 name / strcmp key ("PSYS" etc.)    */
    s32            flags;          /* 0x10 object flag word                    */
    s16            triggertype;    /* 0x14                                     */
    s8             triggerstate;   /* 0x16                                     */
    s8             ptriggerstate;  /* 0x17 previous trigger state             */
    struct WorldObj* parent;       /* 0x18 parent link (OR'd by GetAllFlags)  */
    f32            pos[3];         /* 0x1C x, y, z                            */
    struct mbnode* nodeptr;        /* 0x28 g3d display node                    */
    s16            nextidx;        /* 0x2C next-sibling index (-1 = none)      */
    s16            childidx;       /* 0x2E first-child index (-1 = none)       */
    f32            rad;            /* 0x30 bounding radius                     */
    s8             checked;        /* 0x34 traversal visited flag             */
    s8             nocol;          /* 0x35 no-collision flag                   */
    s16            nctris;         /* 0x36 collision triangle count           */
    s32            ctriidx;        /* 0x38 collision triangle index           */
} WorldObj;                        /* sizeof == 0x3C                          */

#ifdef __MWERKS__
/* offset-exact size guard (inert; header not yet included by any TU) */
typedef char _worldobj_size_check[sizeof(WorldObj) == 0x3C ? 1 : -1];
#endif

#endif /* GAME_WORLDOBJ_H */
