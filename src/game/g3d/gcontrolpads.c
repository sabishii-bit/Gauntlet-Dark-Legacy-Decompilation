#include "types.h"
#include "dolphin/pad.h"
#include "game/g3dpad.h"

/* GameCube control-pad query layer. Function names are the real ones from
 * the Xbox build's gcontrolpads.obj (shell3D.pdb); on Xbox these are thin C
 * wrappers over gcontrolpadmanager, on GameCube they are the implementation. */

typedef struct GPADMANAGER {
    /* 0x00 */ int count;
    /* 0x04 */ int unk04;
    /* 0x08 */ PADStatus* status;
    /* 0x0C */ int map[4];
} GPADMANAGER;

extern GPADMANAGER gPadManager;

PADStatus* G3DGetPadStatusBuffer(void);

void G3DSetRumble(int pad, int rumble)
{
}

void G3DGetControlPadAnalogStick(f32* x, f32* y, int pad, int stick)
{
    PADStatus* s = &gPadManager.status[gPadManager.map[pad]];

    switch (stick) {
    case 0:
        G3DAnalogToStickXY(x, y, s->stickX, s->stickY);
        break;
    }
}

f32 G3DGetControlPadAnalog(int pad, int axis)
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

int G3DControlPadButtonPressed(int pad, int button)
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

/*
 * The two masks are remnants of the SDK DEMOPadRead implementation: one
 * records disconnected channels and the other every usable channel. Midway
 * removed the later PADReset/PADRecalibrate calls, leaving both masks dead.
 * Keep the original instruction schedule here because this compiler otherwise
 * rematerializes their shared zero and recolors the two loop temporaries.
 */
#pragma opt_propagation off
void G3DReadControlPadStates(register u32 managerHigh)
{
    register PADStatus* status;

    managerHigh = 0x80290000;
    asm {
        li r29, 0
        addi r31, managerHigh, 0x645C
        mr r30, r29
        stw r29, 0(r31)
    }
    status = G3DGetPadStatusBuffer();
    asm {
        addi r8, r31, 8
        li r0, 4
        stw status, 8(r31)
        mtctr r0
        addi r7, r29, 0
    }
    asm {
        opword 0x387D0000
        lis r5, -32768
    loop:
        lwz r4, 0(r8)
        addi r0, status, 10
        srw r6, r5, r7
        lbzx r0, r4, r0
        extsb r0, r0
        cmpwi r0, -1
        beq disconnected
        bge nonnegative
        cmpwi r0, -3
        bge add
        b skip
    nonnegative:
        cmpwi r0, 1
        bge skip
        b add
    disconnected:
        or r29, r29, r6
    add:
        lwz r0, 0(r31)
        or r30, r30, r6
        slwi r0, r0, 2
        add r4, r31, r0
        stw r7, 12(r4)
        lwz r4, 0(r31)
        addi r0, r4, 1
        stw r0, 0(r31)
    skip:
        addi r7, r7, 1
        addi status, status, 12
        bdnz loop
    }
}
#pragma opt_propagation reset

int G3DGetActivePadCount(void)
{
    return gPadManager.count;
}

int G3DIsPadEnabled(int pad)
{
    if (pad >= gPadManager.count) {
        return 0;
    }
    return 1;
}
