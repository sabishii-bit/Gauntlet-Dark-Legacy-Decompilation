#include "types.h"
#include "dolphin/pad.h"

/* GameCube control-pad query layer (Xbox counterpart: gcontrolpads.obj /
 * gcontrolpadmanager: GetActivePadCount, GetAnalogInput, ButtonIsPressed,
 * SetRumble). Names are provisional. */

typedef struct GPADMANAGER {
    /* 0x00 */ int count;
    /* 0x04 */ int unk04;
    /* 0x08 */ PADStatus* status;
    /* 0x0C */ int map[4];
} GPADMANAGER;

extern GPADMANAGER gPadManager;

PADStatus* G3DGetPadStatusBuffer(void);
void G3DAnalogToStickXY(f32* x, f32* y, int rawX, int rawY);

void G3DSetPadRumble(int pad, int rumble)
{
}

void G3DGetPadStickXY(f32* x, f32* y, int pad, int stick)
{
    PADStatus* s = &gPadManager.status[gPadManager.map[pad]];

    switch (stick) {
    case 0:
        G3DAnalogToStickXY(x, y, s->stickX, s->stickY);
        break;
    }
}

f32 G3DGetPadAnalogInput(int pad, int axis)
{
    PADStatus* s = &gPadManager.status[gPadManager.map[pad]];
    f32 v;
    u8 unused[0x20];

    switch (axis) {
    case 0:
        v = s->stickX / 70.0F;
        if (v < -1.0F) {
            v = -1.0F;
        }
        if (v > 1.0F) {
            v = 1.0F;
        }
        break;
    case 1:
        v = s->stickY / 70.0F;
        if (v < -1.0F) {
            v = -1.0F;
        }
        if (v > 1.0F) {
            v = 1.0F;
        }
        break;
    case 2:
        v = s->substickX / 70.0F;
        if (v < -1.0F) {
            v = -1.0F;
        }
        if (v > 1.0F) {
            v = 1.0F;
        }
        v = -1.0F * v;
        break;
    case 3:
        v = s->substickY / 70.0F;
        if (v < -1.0F) {
            v = -1.0F;
        }
        if (v > 1.0F) {
            v = 1.0F;
        }
        break;
    case 4:
        v = s->triggerLeft / 255.0F;
        break;
    case 5:
        v = s->triggerRight / 255.0F;
        break;
    case 6:
        v = s->analogA / 255.0F;
        break;
    case 7:
        v = s->analogB / 255.0F;
        break;
    default:
        v = 0.0F;
        break;
    }
    return v;
}

int G3DPadButtonIsPressed(int pad, int button)
{
    PADStatus* s = &gPadManager.status[gPadManager.map[pad]];

    switch (button) {
    case 0:
        if (s->button & 0x0001) {
            return 1;
        }
        return 0;
    case 1:
        if (s->button & 0x0002) {
            return 1;
        }
        return 0;
    case 2:
        if (s->button & 0x0008) {
            return 1;
        }
        return 0;
    case 3:
        if (s->button & 0x0004) {
            return 1;
        }
        return 0;
    case 4:
        if (s->button & 0x0100) {
            return 1;
        }
        return 0;
    case 5:
        if (s->button & 0x0800) {
            return 1;
        }
        return 0;
    case 6:
        if (s->button & 0x0400) {
            return 1;
        }
        return 0;
    case 7:
        if (s->button & 0x0200) {
            return 1;
        }
        return 0;
    case 8:
        if (s->button & 0x0010) {
            return 1;
        }
        return 0;
    case 9:
        if (s->button & 0x1000) {
            return 1;
        }
        return 0;
    case 10:
        if (s->substickX < -64) {
            return 1;
        }
        return 0;
    case 11:
        if (s->substickX > 64) {
            return 1;
        }
        return 0;
    case 12:
        if (s->substickY < -64) {
            return 1;
        }
        return 0;
    case 13:
        if (s->substickY > 64) {
            return 1;
        }
        return 0;
    case 14:
        if (s->button & 0x0020) {
            return 1;
        }
        return 0;
    case 15:
        if (s->button & 0x0040) {
            return 1;
        }
        return 0;
    default:
        return 0;
    }
}

/* NOTE: near-match (6/7 fns in this TU are exact). The original keeps the
 * two dead reset-request masks (melee DEMOPadRead heritage, PADReset call
 * removed) flowing as register copies of a single zero; MWCC 1.2.5n
 * rematerializes the constant instead. See gcontrolpads notes in the
 * project memory before re-attempting. */
void G3DReadControlPadStates(void)
{
    u32 maskA;
    u32 maskB;
    int i;
    u32 bit;
    s8 err;

    maskA = 0;
    maskB = maskA;
    gPadManager.count = maskA;
    gPadManager.status = G3DGetPadStatusBuffer();
    for (i = 0; i < 4; i++) {
        bit = 0x80000000 >> i;
        err = gPadManager.status[i].err;
        if (err == PAD_ERR_NO_CONTROLLER) {
            goto add;
        } else if (err < PAD_ERR_NO_CONTROLLER) {
            if (err >= PAD_ERR_TRANSFER) {
                goto add;
            }
            goto skip;
        } else {
            if (err >= 1) {
                goto skip;
            }
            goto add;
        }
    add:
        maskA |= bit;
        maskB |= bit;
        gPadManager.map[gPadManager.count] = i;
        gPadManager.count++;
    skip:;
    }
}

int G3DGetActivePadCount(void)
{
    return gPadManager.count;
}

int G3DPadIsActive(int pad)
{
    if (pad < gPadManager.count) {
        goto active;
    }
    return 0;
active:
    return 1;
}
