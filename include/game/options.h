#ifndef GAME_OPTIONS_H
#define GAME_OPTIONS_H

#include "types.h"

/* Debug/game option words (Xbox shell3D.pdb struct OPTIONS, type 0x3f45,
 * size 0x30; the Xbox global is `Options`). The GameCube object is
 * gGameOptions (.bss 0x80257590, 0x30 bytes). */
typedef struct OPTIONS {
    /* 0x00 */ s32 no_damage;
    /* 0x04 */ s32 unlimited;
    /* 0x08 */ s32 gen_active;
    /* 0x0C */ s32 players;
    /* 0x10 */ s32 items;
    /* 0x14 */ s32 showcam;
    /* 0x18 */ s32 fly;
    /* 0x1C */ s32 showfps;
    /* 0x20 */ s32 showpos;
    /* 0x24 */ s32 startwave;
    /* 0x28 */ s32 testai;
    /* 0x2C */ s32 skip;
} OPTIONS;

extern OPTIONS gGameOptions;

#endif
