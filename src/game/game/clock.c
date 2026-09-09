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

extern s32 gClockFrameNumber;
extern f32 sMusicFadeBase;
extern f32 gClockTime;
extern s32 InfFrame;
extern s32 sLastVBlankCounter;
extern f32 gClockFrameReciprocal;
extern f32 gClockFrameStep;
extern f32 gClockPreviousTime;
extern s32 sLastTimerCount;
extern u32 sLastFrameTime;
extern s32 sClockAccumulator;
extern u32 gClockElapsedTime;
extern u32 gClockCurrentTime;
extern s32 gClockStepTicks;
extern u32 gFrameTicks;
extern u32 pbLoad;
extern s32 gGameBusy;
extern s32 options_state;
extern s32 gGameplayPauseTimer;
extern s32 gModalRenderDepth;
extern s32 gGameMode;
extern s32 sFlags;
extern s32 lbl_803445D4;
extern ClockInputPair gControllerButtons;
extern ClockInputPair sPreviousFlags;
extern const f32 lbl_803462E8;
extern const f32 lbl_803462EC;
extern const f32 lbl_803462F0;
extern const f32 lbl_803462F4;
extern const f32 lbl_803462F8;

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
        gClockFrameStep = lbl_803462E8;
        gClockFrameReciprocal = lbl_803462EC;
    } else if (resumed || gFrameTicks > 60 || gFrameTicks == 0) {
        gFrameTicks = 2;
        gClockElapsedTime = 10000000;
        gClockFrameStep = lbl_803462F0;
        gClockFrameReciprocal = lbl_803462EC;
    } else {
        gClockFrameStep = (f32)gFrameTicks / lbl_803462F4;
        gClockFrameReciprocal = lbl_803462F8 / gClockFrameStep;
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
