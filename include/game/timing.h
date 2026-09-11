#ifndef GAME_TIMING_H
#define GAME_TIMING_H

#include "types.h"

/* Xbox TIMERDESCFYB/TIMERFYB corroborate the GameCube profiling records.
 * The timer renderer uses 28-byte descriptors and 16-byte timer samples. */
typedef struct TimerDesc {
    char name[16];
    s32 level;
    u32 color;
    u32 precision;
} TimerDesc;

typedef struct TimerSample {
    u32 frame;
    u32 count;
    u32 current;
    u32 last_frame;
} TimerSample;

/* TimersFYB: 24 platform profiling samples at GC 0x802C45CC, distinct
 * from the registered game timers. Existing GC symbol retained for now. */
extern TimerSample lbl_802C45CC[24];

struct MBBlit;

/* TimersAddList: existing GC label retained pending a coordinated rename. */
void fn_800C031C(TimerSample* timers, TimerDesc* descriptors,
                struct MBBlit** blits, s32 count);

#endif
