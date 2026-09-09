#ifndef GAME_CONTROLS_H
#define GAME_CONTROLS_H

#include "types.h"

/* Per-player assembled control state (Xbox shell3D.pdb struct PLAYERCONTROL,
 * type 0x6187, size 0x3C; every offset is verified by controls.c's own
 * accesses on the GameCube). The four records are `PlayerControl`. */
typedef struct PLAYERCONTROL {
    /* 0x00 */ u32 inactive;     /* clear/hold-off countdown */
    /* 0x04 */ s32 levels;       /* held buttons */
    /* 0x08 */ s32 edges;        /* new presses (new_* family reads) */
    /* 0x0C */ s32 repedges;     /* auto-repeat edges */
    /* 0x10 */ s32 specialdelay; /* special-move cooldown */
    /* 0x14 */ s32 special;      /* CheckSpecials result this frame */
    /* 0x18 */ s32 lastspecial;  /* last special id */
    /* 0x1C */ f32 joyang;       /* analog stick state */
    /* 0x20 */ f32 joymag;
    /* 0x24 */ f32 fireang;
    /* 0x28 */ f32 firemag;
    /* 0x2C */ s32 scheme;       /* enum CTRL_SCHEME: control scheme (SpecialData row) */
    /* 0x30 */ s32 rumble;       /* abHasActuator */
    /* 0x34 */ s32 autoaim;
    /* 0x38 */ s32 autoattack;
} PLAYERCONTROL;

extern PLAYERCONTROL PlayerControl[4];

#endif
