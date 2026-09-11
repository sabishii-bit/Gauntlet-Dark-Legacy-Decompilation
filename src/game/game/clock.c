/*
 * clock.c -- CLOCK.OBJ: the frame clock (ResetClock / InitializeClockIRQ /
 * ClockOncePerFrame), 0x8002EFE8..0x8002F2D4.
 */

#include "types.h"
#include "game/gamemode.h"

typedef struct ClockInputWords {
    s32 buttons;
    s32 flags;
} ClockInputWords;

typedef union ClockInputPair {
    u64 both;
    ClockInputWords word;
} ClockInputPair;

/* CLOCK state, .sbss 0x80344558..0x80344598. MWCC emits these tentative
 * definitions in reverse declaration order. The preceding camera state at
 * 0x80344550 is not part of this run. gGameBusy is the shared pause gate;
 * its consumers are outside this TU, so it must retain external linkage. */
f32 sMusicFadeBase;
f32 gClockFrameStep;
f32 gClockFrameReciprocal;
s32 gClockFrameNumber;
f32 gClockTime;
s32 InfFrame;
u32 gFrameTicks;
s32 gClockStepTicks;
u32 gClockCurrentTime;
u32 gClockElapsedTime;
s32 sClockAccumulator;
s32 gGameBusy;
s32 sLastVBlankCounter;
u32 sLastFrameTime;
s32 sLastTimerCount;
f32 gClockPreviousTime;

extern u32 pbLoad;
extern s32 options_state;
extern s32 gGameplayPauseTimer;
extern s32 gModalRenderDepth;
extern s32 gGameMode;
extern s32 sFlags;
extern s32 lbl_803445D4;
extern ClockInputPair gControllerButtons;
extern ClockInputPair sPreviousFlags;

void ResetClock(void)
{
    gClockFrameNumber = 0;
    sMusicFadeBase = 0.0f;
    gClockTime = 0.0f;
    InfFrame = 0;
    sLastVBlankCounter = 0;
    gClockFrameReciprocal = 0.0f;
    gClockFrameStep = 0.0f;
    gClockPreviousTime = 0.0f;
}

void InitializeClockIRQ(void)
{
    gClockFrameNumber = 0;
    sMusicFadeBase = 0.0f;
    gClockTime = 0.0f;
    InfFrame = 0;
    sLastVBlankCounter = 0;
    gClockFrameReciprocal = 0.0f;
    gClockFrameStep = 0.0f;
    gClockPreviousTime = 0.0f;
}

void ClockOncePerFrame(void)
{
    s32 freeze = 0;
    s32 resumed = 0;

    if ((gControllerButtons.both & 8) != 0) {
        freeze = 1;
        if ((sPreviousFlags.both & ((u64)8 << 32)) != 0) {
            sClockAccumulator = 0;
            freeze = 0;
            resumed = 1;
        } else if ((gControllerButtons.both & ((u64)8 << 32)) != 0) {
            if (sClockAccumulator >= 60) {
                sClockAccumulator -= 4;
                freeze = 0;
            } else {
                sClockAccumulator += gClockStepTicks;
            }
        }
    }
    if ((gControllerButtons.both & 4) != 0 &&
        gGameMode == MG_PLAY) {
        freeze = 1;
    }

    gFrameTicks = *(volatile u32*)&pbLoad - sLastTimerCount;
    gClockStepTicks = gFrameTicks;
    sLastTimerCount = *(volatile u32*)&pbLoad;
    gClockCurrentTime = pbGetTime();
    gClockElapsedTime = gClockCurrentTime - sLastFrameTime;
    sLastFrameTime = gClockCurrentTime;

    if (options_state != 0 || gModalRenderDepth != 0 || (freeze && !resumed)) {
        gFrameTicks = 0;
        if (options_state != 100) {
            gClockElapsedTime = 0;
        }
        gClockFrameStep = 0.0f;
        gClockFrameReciprocal = 30.0f;
    } else if (resumed || gFrameTicks > 60 || gFrameTicks == 0) {
        gFrameTicks = 2;
        gClockElapsedTime = 10000000;
        gClockFrameStep = 1.0f / 30.0f;
        gClockFrameReciprocal = 30.0f;
    } else {
        gClockFrameStep = (f32)gFrameTicks / 60.0f;
        gClockFrameReciprocal = 1.0f / gClockFrameStep;
    }
    if (gClockElapsedTime > 300000000) {
        gClockElapsedTime = 10000000;
    }
    if (!freeze && options_state == 0) {
        sMusicFadeBase += gClockFrameStep;
        if (sMusicFadeBase > 18000.0f) {
            sMusicFadeBase =
                *(volatile f32*)&sMusicFadeBase - 18000.0f;
        }
        gClockFrameNumber =
            (s32)(1000.0f * *(volatile f32 *)&sMusicFadeBase + 0.5f);
        gClockTime = *(volatile f32*)&sMusicFadeBase;
        InfFrame++;
    }
    gClockPreviousTime = sMusicFadeBase;
}
