#ifndef GAME_ATREE_H
#define GAME_ATREE_H

#include "types.h"

struct anodeinfo;     /* the node-description record, completed in atree.c */
struct atreeseq;      /* one animation sequence (PDB misc.h Id=3262, 0x30);
                       * defined in full by atree.c, which owns it, and
                       * completed per TU by anim.c with the one field it
                       * reads (fixpos @0x26, target 8000ECD8)            */

/*
 * atree.h -- the animation-tree instance record and the two sub-records every
 * animated game object embeds.
 *
 * These three types already existed, GC-verified, inside the OWNING TU
 * (src/game/anim/atree.c); they are moved here unchanged so the consumers that
 * currently reach into them through byte cursors can name the fields.  The
 * Xbox debug PDB corroborates all three field-for-field:
 *
 *   struct atree     misc.h     Id=2219  Size=0x48
 *   struct anode     misc.h     Id=3255  Size=0x28
 *   struct animinfo  graphics.h Id=3256  Size=0x38
 *
 * (An earlier lane note claimed `anode`/`animinfo` were "declared but never
 * defined in the PDB dump".  That was a search error: `anode` is in misc.h and
 * `animinfo` is in graphics.h, not misc.h.  Both are fully defined.)
 *
 * GameCube accesses that pin the layout, independent of the PDB:
 *   anode.obj     +0x00  atree.c AtreeNodeSetParent / critter.c and enemy.c
 *                        read `*(void**)anode` as the scene node handle
 *   anode.type    +0x20  critter.c CritterDamage clears `*(s32*)(anode+0x20)`
 *   anode stride  0x28   every consumer walks `anodes + i * 0x28`
 *   animinfo.animseq   +0x0E  enemy.c reads it s16 (`*(s16*)&c->sound[0x0E]`,
 *                             i.e. Critter+0x78+0x0E)
 *   animinfo.numframes +0x10  enemy.c reads Critter+0x88 as s16
 *   animinfo.frame     +0x18  enemy.c reads Critter+0x90 as f32 and truncates
 *                             it to a frame counter
 *   atree.nanodes      +0x3C  critter.c CritterNewInst / CritterInitGeo read
 *                             the node count beside `anodes`
 *   atree.firstanode   +0x40  the base those `+ i * 0x28` walks index from
 * Every one of those four Critter offsets falls exactly where this layout puts
 * the field, at the width and signedness the target load uses.
 */

/* -- anode (0x28): one node of a live animation tree. -- */
typedef struct anode {
    /* 0x00 */ void* obj;          /* scene node handle (PDB: struct mbnode*) */
    /* 0x04 */ struct anode* parent;
    /* 0x08 */ struct anode* child;
    /* 0x0C */ struct anode* next;
    /* 0x10 */ f32 x;              /* PDB: initpos[3]                        */
    /* 0x14 */ f32 y;
    /* 0x18 */ f32 z;
    /* 0x1C */ void* anim;         /* PDB: union data                        */
    /* 0x20 */ s32 type;
    /* 0x24 */ f32 frame;          /* PDB: int offset                        */
} anode;                           /* size 0x28 */

/* -- animinfo (0x38): atree playback state.  Embedded at atree+0x04, so an
 *    object that embeds an atree has its animinfo 4 bytes further in. -- */
typedef struct animinfo {
    /* 0x00 */ struct atreeseq* seqheader;
    /* 0x04 */ void* animheader;
    /* 0x08 */ void* oanimheader;
    /* 0x0C */ s16 numseqs;
    /* 0x0E */ s16 animseq;
    /* 0x10 */ s16 numframes;
    /* 0x12 */ s8  setpanim;
    /* 0x13 */ u8  flags;
    /* 0x14 */ f32 transfrac;
    /* 0x18 */ f32 frame;
    /* 0x1C */ s16 animseq0;
    /* 0x1E */ s16 active;
    /* 0x20 */ f32 starttime;
    /* 0x24 */ f32 transtime;
    /* 0x28 */ f32 animscale;
    /* 0x2C */ f32 seqscale;
    /* 0x30 */ f32 atime;
    /* 0x34 */ s16 repeat;
    /* 0x36 */ u16 stage;
} animinfo;                        /* size 0x38 */

/* -- atree (0x48): the instance an animated object embeds.  `anodeinfo` is
 *    left as an opaque pointer here: atree.c carries two different types for
 *    that record (a placeholder `char name[0x3C]` and the richer
 *    `AtreeNodeDef`, 0x3C), and picking one is a separate recovery. -- */
typedef struct atree {
    /* 0x00 */ anode* root;
    /* 0x04 */ animinfo animinfo;
    /* 0x3C */ s32 nanodes;
    /* 0x40 */ anode* firstanode;
    /* 0x44 */ struct anodeinfo* anodeinfo;
} atree;                           /* size 0x48 */

#endif /* GAME_ATREE_H */
