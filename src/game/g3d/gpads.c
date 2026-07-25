#include "types.h"
#include "dolphin/pad.h"

/* GameCube control-pad init (g3d pad layer; Xbox module gcontrolpads.obj has
 * no direct counterpart for this init path - name is provisional). */

typedef struct GPADSTATE {
    /* 0x00 */ PADStatus status;
    /* 0x0C */ u16 unk0C[10];
    /* 0x20 */ u8 unk20[9];
    /* 0x2A */ u16 unk2A[9];
} GPADSTATE;

static GPADSTATE gPadStates[2];

void G3DInitControlPads(void)
{
    int i;
    GPADSTATE* p;

    PADInit();
    for (i = 0; i < 2; i++) {
        p = &gPadStates[i];
        p->status.button = 0;
        p->status.stickX = 0;
        p->status.stickY = 0;
        p->status.substickX = 0;
        p->status.substickY = 0;
        p->status.triggerLeft = 0;
        p->status.triggerRight = 0;
        p->status.analogA = 0;
        p->status.analogB = 0;
        p->status.err = 0;
        p->unk0C[0] = 0;
        p->unk0C[1] = 0;
        p->unk0C[2] = 0;
        p->unk0C[3] = 0;
        p->unk0C[4] = 0;
        p->unk0C[5] = 0;
        p->unk0C[6] = 0;
        p->unk0C[7] = 0;
        p->unk0C[8] = 0;
        p->unk0C[9] = 0;
        p->unk20[0] = 0;
        p->unk20[1] = 0;
        p->unk20[2] = 0;
        p->unk20[3] = 0;
        p->unk20[4] = 0;
        p->unk20[5] = 0;
        p->unk20[6] = 0;
        p->unk20[7] = 0;
        p->unk20[8] = 0;
        p->unk2A[0] = 0;
        p->unk2A[1] = 0;
        p->unk2A[2] = 0;
        p->unk2A[3] = 0;
        p->unk2A[4] = 0;
        p->unk2A[5] = 0;
        p->unk2A[6] = 0;
        p->unk2A[7] = 0;
        p->unk2A[8] = 0;
    }
}
