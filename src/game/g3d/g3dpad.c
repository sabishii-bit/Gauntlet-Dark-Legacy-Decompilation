/*
 * g3dpad.c - G3D GameCube control-pad hardware layer.
 *
 * Text 0x800D7CFC-0x800D860C.  Sits between adstream.c (0xD6234-0xD7CFC) and
 * MoviePlayer.cpp (0xD860C-0xDC180).  This is the low-level GameCube pad
 * driver that backs the higher-level G3D control-pad C API in
 * game/g3d/gcontrolpads.c (0x800CB060): gcontrolpads.c reads the shared
 * gPadManager block and calls G3DAnalogToStickXY (here) as a helper, while the
 * engine-init blob (fn_800B27C4) drives G3DUpdatePadStatus / G3DInitPadStatus
 * here to pump the raw dolphin PAD library.  PADStatus buffer is handed out by
 * G3DGetPadStatusBuffer @0x800DDC78 (sysservice region).
 *
 * On Xbox these lived in gcontrolpads.obj (the gcontrolpadmanager class); the
 * GameCube port split the raw hardware access (PADRead/PADClamp/PADReset +
 * analog->stick response curve) into this separate TU.  It has no Xbox PDB
 * counterpart (Xbox used XInput), so the two internal names are behavioural.
 *
 * sdata2 pool: 0x80349350-0x80349388 (the analog curve constants).
 * Analog response table: lbl_80320C80 (three parallel f32[128] curves at
 * +0x000/+0x204/+0x408), built once by G3DInitStickCurve, guarded by
 * lbl_803452A4.
 *
 * NonMatching: all four functions are translated. The response-curve math and
 * status pump retain compiler-layout differences, so extracted bytes still
 * link from the DOL for the whole TU.
 */
#include "types.h"
#include "dolphin/pad.h"
#include "dolphin/vi/vifuncs.h"
#include "game/g3dpad.h"

extern f64 __frsqrte(f64 value);

static inline f32 g3dSqrt(f32 value)
{
    volatile f32 result;

    if (value > 0.0f) {
        f64 guess = __frsqrte((f64)value);
        guess = 0.5 * guess * (3.0 - guess * guess * value);
        guess = 0.5 * guess * (3.0 - guess * guess * value);
        guess = 0.5 * guess * (3.0 - guess * guess * value);
        result = (f32)(value * guess);
        return result;
    }
    return value;
}

extern u32 lbl_80345298;
extern u32 lbl_8034529C;
extern u32 lbl_803452A0;
extern s32 lbl_803452A4;

typedef struct G3DStickCurve {
    f32 magnitude[129];
    f32 x[129];
    f32 y[129];
} G3DStickCurve;

extern G3DStickCurve lbl_80320C80;

typedef struct G3DPadState {
    PADStatus status;
    u16 previous;
    u16 pressed;
    u16 released;
    u16 repeat;
    u32 repeatCounter;
} G3DPadState;

typedef struct G3DPadHardwareState {
    G3DPadState pad[4];
    u16 buttons;
    s8 stickX;
    s8 stickY;
    s8 substickX;
    s8 substickY;
    u8 triggerLeft;
    u8 triggerRight;
    u8 analogA;
    u8 analogB;
    s8 error;
    u8 _pad6B;
    u16 previous;
    u16 pressed;
    u16 released;
    u16 repeat;
    u8 _tail[8];
} G3DPadHardwareState;

extern G3DPadHardwareState lbl_8032128C;

/*
 * 0x800D7CFC  G3DAnalogToStickXY(f32* outX, f32* outY, int rawX, int rawY)
 * Convert a raw GameCube analog reading (rawX,rawY) to a normalised stick
 * vector.  Zero-input shortcut writes 0.0f to both outputs; otherwise it takes
 * the octant (sign + |x|<=>|y| swap flags in r9), interpolates the per-axis
 * response curve from lbl_80320C80, and renormalises the magnitude with an
 * frsqrte + three Newton-Raphson refinement steps. Called by gcontrolpads.c
 * (G3DGetControlPadAnalogStick).
 */
void G3DAnalogToStickXY(f32* outX, f32* outY, int rawX, int rawY) {
    s32 flags = 0;
    s32 tmp;
    s32 index;
    s32 minor;
    f32 magnitude;
    f32 scale;

    if (rawX == 0 && rawY == 0) {
        *outX = 0.0f;
        *outY = 0.0f;
        return;
    }

    if (rawY < 0) {
        flags |= 1;
        rawY = -rawY;
    }
    if (rawX < 0) {
        flags |= 2;
        rawX = -rawX;
    }
    if (rawY > rawX) {
        tmp = rawX;
        rawX = rawY;
        rawY = tmp;
        flags |= 4;
    }

    index = (s32)(128.0f * (1.0f / 256.0f +
                            (f32)rawY / (f32)rawX));
    minor = (s32)((f32)index * (1.0f / 128.0f) * (f32)rawX);
    magnitude = g3dSqrt((f32)(rawX * rawX + minor * minor));
    scale = magnitude / lbl_80320C80.magnitude[index];
    *outX = scale * lbl_80320C80.x[index];
    *outY = scale * lbl_80320C80.y[index];

    if ((flags & 4) != 0) {
        f32 swap = *outY;
        *outY = *outX;
        *outX = swap;
    }
    if ((flags & 2) != 0) {
        *outX = -*outX;
    }
    if ((flags & 1) != 0) {
        *outY = -*outY;
    }
}

/*
 * 0x800D7F44  G3DInitStickCurve(void)
 * Build the analog->stick response table lbl_80320C80 once (guarded by the
 * lbl_803452A4 init-once flag): a 128-entry curve for each of three parallel
 * f32 arrays.
 */
void G3DInitStickCurve(void) {
    s32 i;
    G3DStickCurve* curve = &lbl_80320C80;

    if (lbl_803452A4 == 0) {
        lbl_803452A4 = 1;
        curve->magnitude[0] = 72.0f;
        curve->x[0] = 1.0f;
        curve->y[0] = 0.0f;

        for (i = 1; i <= 128; i++) {
            f32 ratio = (f32)i * (1.0f / 128.0f);
            f32 divisor = 1.25f + ratio;
            s32 x = (s32)(0.5f + 90.0f / divisor);
            s32 y = (s32)(0.5f + (90.0f * ratio) / divisor);
            f32 magnitude = g3dSqrt((f32)(x * x + y * y));
            f32 storedMagnitude;

            curve->magnitude[i] = magnitude;
            storedMagnitude = curve->magnitude[i];
            curve->x[i] = (f32)x / storedMagnitude;
            curve->y[i] = (f32)y / storedMagnitude;
        }
    }
}

/*
 * 0x800D811C  G3DUpdatePadStatus(void)
 * Per-frame raw read of all four GameCube pads: PADRead into the status
 * buffer, PADClamp, and PADReset on the disconnected mask.  Called by the
 * engine-init/service blob fn_800B27C4.
 */
void G3DUpdatePadStatus(void) {
    PADStatus status[4];
    u32 resetMask;
    u32 channelBit;
    s32 i;

    PADRead(status);
    PADClamp(status);

    lbl_8032128C.error = PAD_ERR_NO_CONTROLLER;
    resetMask = 0;
    for (i = 0; i < 4; i++) {
        channelBit = PAD_CHAN0_BIT >> i;
        if (status[i].err == PAD_ERR_NO_CONTROLLER) {
            resetMask |= channelBit;
        } else if (status[i].err < PAD_ERR_NO_CONTROLLER) {
            if (status[i].err == PAD_ERR_TRANSFER) {
                lbl_803452A0 |= channelBit;
                if (lbl_8032128C.error == PAD_ERR_NO_CONTROLLER) {
                    lbl_8032128C.error = PAD_ERR_TRANSFER;
                }
            } else if (status[i].err > -4 &&
                       lbl_8032128C.error == PAD_ERR_NO_CONTROLLER) {
                lbl_8032128C.error = PAD_ERR_NOT_READY;
            }
        } else if (status[i].err < 1) {
            lbl_803452A0 |= channelBit;
            lbl_8032128C.error = PAD_ERR_NONE;
        }
    }
    if (lbl_803452A0 != 0) {
        resetMask &= lbl_803452A0;
    }
    if (resetMask != 0) {
        PADReset(resetMask);
    }

    lbl_8032128C.previous = lbl_8032128C.buttons;
    lbl_8032128C.buttons = 0;
    lbl_8032128C.stickX = 0;
    lbl_8032128C.stickY = 0;
    lbl_8032128C.substickX = 0;
    lbl_8032128C.substickY = 0;
    lbl_8032128C.triggerLeft = 0;
    lbl_8032128C.triggerRight = 0;
    lbl_8032128C.analogA = 0;
    lbl_8032128C.analogB = 0;
    lbl_8032128C.pressed = 0;
    lbl_8032128C.released = 0;
    lbl_8032128C.repeat = 0;

    for (i = 0; i < 4; i++) {
        G3DPadState* pad = &lbl_8032128C.pad[i];
        u16 changed;

        pad->status.err = status[i].err;
        if (pad->status.err != PAD_ERR_TRANSFER) {
            pad->previous = pad->status.button;
            pad->status.button = status[i].button;
            pad->status.stickX = status[i].stickX;
            pad->status.stickY = status[i].stickY;
            pad->status.substickX = status[i].substickX;
            pad->status.substickY = status[i].substickY;
            pad->status.triggerLeft = status[i].triggerLeft;
            pad->status.triggerRight = status[i].triggerRight;
            pad->status.analogA = status[i].analogA;
            pad->status.analogB = status[i].analogB;
        }

        if (pad->status.err == PAD_ERR_NONE) {
            if (pad->status.stickX < 0) {
                pad->status.button |= PAD_BUTTON_LEFT;
            } else if (pad->status.stickX > 0) {
                pad->status.button |= PAD_BUTTON_RIGHT;
            }
            if (pad->status.stickY < 0) {
                pad->status.button |= PAD_BUTTON_DOWN;
            } else if (pad->status.stickY > 0) {
                pad->status.button |= PAD_BUTTON_UP;
            }

            changed = pad->previous ^ pad->status.button;
            pad->pressed = pad->status.button & changed;
            pad->released = pad->previous & changed;
            pad->repeat = pad->status.button & pad->previous & 0x1F7F;
            if (pad->repeat == 0) {
                pad->repeatCounter = 0;
            } else {
                pad->repeatCounter++;
                if (pad->repeatCounter < lbl_80345298 ||
                    pad->repeatCounter % lbl_8034529C != 0) {
                    pad->repeat = 0;
                }
            }
            pad->repeat |= pad->pressed;

            lbl_8032128C.buttons |= pad->status.button;
            lbl_8032128C.pressed |= pad->pressed;
            lbl_8032128C.released |= pad->released;
            lbl_8032128C.repeat |= pad->repeat;

            if ((lbl_8032128C.stickX < 0 ? -lbl_8032128C.stickX
                                         : lbl_8032128C.stickX) <
                (pad->status.stickX < 0 ? -pad->status.stickX
                                        : pad->status.stickX)) {
                lbl_8032128C.stickX = pad->status.stickX;
            }
            if ((lbl_8032128C.stickY < 0 ? -lbl_8032128C.stickY
                                         : lbl_8032128C.stickY) <
                (pad->status.stickY < 0 ? -pad->status.stickY
                                        : pad->status.stickY)) {
                lbl_8032128C.stickY = pad->status.stickY;
            }
            if ((lbl_8032128C.substickX < 0 ? -lbl_8032128C.substickX
                                            : lbl_8032128C.substickX) <
                (pad->status.substickX < 0 ? -pad->status.substickX
                                           : pad->status.substickX)) {
                lbl_8032128C.substickX = pad->status.substickX;
            }
            if ((lbl_8032128C.substickY < 0 ? -lbl_8032128C.substickY
                                            : lbl_8032128C.substickY) <
                (pad->status.substickY < 0 ? -pad->status.substickY
                                           : pad->status.substickY)) {
                lbl_8032128C.substickY = pad->status.substickY;
            }
            if (lbl_8032128C.triggerLeft < pad->status.triggerLeft) {
                lbl_8032128C.triggerLeft = pad->status.triggerLeft;
            }
            if (lbl_8032128C.triggerRight < pad->status.triggerRight) {
                lbl_8032128C.triggerRight = pad->status.triggerRight;
            }
            if (lbl_8032128C.analogA < pad->status.analogA) {
                lbl_8032128C.analogA = pad->status.analogA;
            }
            if (lbl_8032128C.analogB < pad->status.analogB) {
                lbl_8032128C.analogB = pad->status.analogB;
            }
        }
    }
}

/*
 * 0x800D8584  G3DInitPadStatus(void)
 * One-time pad init: VIGetTvFormat, PADRecalibrate + PADReset on all pads, and
 * G3DInitStickCurve() to build the analog response table.
 */
#pragma dont_inline on
void G3DInitPadStatus(u32 mask, s32 recalibrate) {
    u32 resetMask;

    if (VIGetTvFormat() == 1) {
        lbl_80345298 = 25;
        lbl_8034529C = 5;
    } else {
        lbl_80345298 = 30;
        lbl_8034529C = 6;
    }

    resetMask = 0xF0000000;
    lbl_803452A0 = mask;
    if (mask != 0) {
        resetMask &= mask;
    }
    if (recalibrate != 0) {
        PADRecalibrate(resetMask);
    } else {
        PADReset(resetMask);
    }
    G3DInitStickCurve();
}
#pragma dont_inline off
