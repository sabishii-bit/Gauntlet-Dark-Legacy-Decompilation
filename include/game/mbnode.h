#ifndef GAME_MBNODE_H
#define GAME_MBNODE_H

#include "types.h"

/* mbnode - one node of the MB scene graph (the handle every placed object,
 * player, enemy, critter and effect hangs its geometry from).
 *
 * The layout is the Xbox shell3D.pdb record `struct mbnode // Size=0x80
 * (Id=3249)` in research/xbox_symbols/misc.h, adopted here field for field.
 * Before this header the same record existed only as a partial file-local
 * view in src/game/sfx/sfx.c, so every other consumer addressed it with raw
 * byte offsets; those offsets are what pinned the layout on GameCube, and
 * each one below is annotated with the access that proves it:
 *
 *   mat[3][0..2]  0x30/0x34/0x38  the node's world translation row -
 *                 f32 stores in sfx.c (SfxSetNodePos), gauntworld.c
 *                 fn_80062A00 (WorldObj.nodeptr + 48/52/56) and player.c
 *                 do_players (PF(p->mbnode, 0x30..0x38, f32)).
 *   scale[0..2]   0x40/0x44/0x48  three consecutive f32 stores in sfx.c
 *                 (SfxScaleNode, which also divides by each parent's scale)
 *                 and gauntworld.c fn_800606FC's see-thru overlay.
 *   alpha         0x53            gauntworld.c fn_80062A00 reads it as a u8
 *                 and feeds 255 - alpha to MBTreeSetAlpha.
 *   flags         0x60            u32 bit tests in sfx.c (`& 8`, `|= 0x10`),
 *                 gauntworld.c (`& 0x200`, `|= 2`).
 *   ambient_add   0x6A            s16; sfx.c's local view called this
 *                 `frame` from its own use as an animation counter - the
 *                 offset, width and signedness agree, only the name is the
 *                 PDB's rather than that reading.
 *   parent        0x74            gauntworld.c and player.c both load it as
 *                 the saved reparent target; sfx.c walks it.
 *   child         0x78            gauntworld.c fn_8005D0C4 region.
 *   next          0x7C            sfx.c's recursive sibling walk called this
 *                 `sibling`; same offset and width.
 *
 * Fields with no GameCube access in this tree keep their PDB name and type
 * because the record is fully described by the PDB and its 0x80 stride is
 * what every consumer above assumes; they are not independently GC-verified.
 * `data` is the PDB's `union __unnamed` (Id=3248, a union of pointers to
 * ROMOBJECT, polyheader, blitinst and PSYS) and is left as a plain pointer
 * here: this tree has no declaration for any of those pointees. */
typedef struct mbnode {
    /* 0x00 */ f32 mat[4][4];      /* [3][0..2] is the world translation    */
    /* 0x40 */ f32 scale[4];
    /* 0x50 */ u16 id;
    /* 0x52 */ s8  type;
    /* 0x53 */ u8  alpha;
    /* 0x54 */ f32 zsort_add;
    /* 0x58 */ u32 texaltidx;
    /* 0x5C */ s16 texchangeidx;
    /* 0x5E */ u8  tex_shift_idx;
    /* 0x5F */ u8  extra_byte;
    /* 0x60 */ u32 flags;
    /* 0x64 */ u32 color;
    /* 0x68 */ s16 zmod;
    /* 0x6A */ s16 ambient_add;    /* sfx.c reads this as an anim frame     */
    /* 0x6C */ u32 index;
    /* 0x70 */ void* data;         /* PDB union: obj/poly/blit/psys         */
    /* 0x74 */ struct mbnode* parent;
    /* 0x78 */ struct mbnode* child;
    /* 0x7C */ struct mbnode* next;
} mbnode;                          /* sizeof == 0x80 */

#ifdef __MWERKS__
/* offset-exact size guard */
typedef char _mbnode_size_check[sizeof(struct mbnode) == 0x80 ? 1 : -1];
#endif

#endif /* GAME_MBNODE_H */
