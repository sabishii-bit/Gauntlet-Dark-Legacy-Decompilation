#include "types.h"
#include "dolphin/pad.h"

typedef struct PADClampRegion {
    /* 0x0 */ u8 minTrigger;
    /* 0x1 */ u8 maxTrigger;
    /* 0x2 */ s8 minStick;
    /* 0x3 */ s8 maxStick;
    /* 0x4 */ s8 xyStick;
    /* 0x5 */ s8 minSubstick;
    /* 0x6 */ s8 maxSubstick;
    /* 0x7 */ s8 xySubstick;
} PADClampRegion;

static PADClampRegion ClampRegion = {
    // Triggers
    30,
    180,

    // Left stick
    15,
    72,
    40,

    // Right stick
    15,
    59,
    31,
};

static void ClampStick(s8* px, s8* py, s8 max, s8 xy, s8 min) {
    int x = *px;
    int y = *py;
    int signX;
    int signY;
    int d;

    if (0 <= x) {
        signX = 1;
    } else {
        signX = -1;
        x = -x;
    }

    if (0 <= y) {
        signY = 1;
    } else {
        signY = -1;
        y = -y;
    }

    if (x <= min) {
        x = 0;
    } else {
        x -= min;
    }

    if (y <= min) {
        y = 0;
    } else {
        y -= min;
    }

    if (x == 0 && y == 0) {
        *px = *py = 0;
        return;
    }

    if (xy * y <= xy * x) {
        d = xy * x + y * (max - xy);
        if (xy * max < d) {
            x = (s8)(x * (xy * max) / d);
            y = (s8)(y * (xy * max) / d);
        }
    } else {
        d = xy * y + x * (max - xy);
        if (xy * max < d) {
            x = (s8)(x * (xy * max) / d);
            y = (s8)(y * (xy * max) / d);
        }
    }

    *px = (s8)(signX * x);
    *py = (s8)(signY * y);
}

static void ClampTrigger(u8* trigger) {
    if (*trigger <= ClampRegion.minTrigger) {
        *trigger = 0;
    } else {
        if (ClampRegion.maxTrigger < *trigger) {
            *trigger = ClampRegion.maxTrigger;
        }
        *trigger -= ClampRegion.minTrigger;
    }
}

void PADClamp(PADStatus* status) {
    s32 i;

    for (i = 0; i < PAD_CHANMAX; i++, status++) {
        if (status->err == PAD_ERR_NONE) {
            ClampStick(&status->stickX, &status->stickY, ClampRegion.maxStick, ClampRegion.xyStick,
                       ClampRegion.minStick);
            ClampStick(&status->substickX, &status->substickY, ClampRegion.maxSubstick,
                       ClampRegion.xySubstick, ClampRegion.minSubstick);
            ClampTrigger(&status->triggerLeft);
            ClampTrigger(&status->triggerRight);
        }
    }
}
