/*
 * sysservice.c - per-frame system service: reset/eject handling, the assert
 * message pump, the global system-flag word, and the controller-pad manager.
 *
 * Address range 0x800DD180..0x800DDE34. Called every frame from the main loop
 * (sysResetService drives the GameCube RESET button + disc-cover state machine
 * and pumps the pad manager); memcard.c/main.c poke the sys flag word through
 * sysSetFlags/sysClearFlags/sysTestFlags.
 *
 * NOTE: NonMatching - kept for symbol mapping / documentation. Function order
 * follows the DOL (same-TU inlining depends on it). Data is defined locally so
 * the TU compiles stand-alone; the linked image supplies the real globals.
 */
#include "types.h"

/* --- dolphin/os --- */
s32  OSGetResetButtonState(void);
void OSResetSystem(int reset, u32 resetCode, BOOL forceMenu);

/* --- dolphin/dvd (game wrapper) --- */
s32  DVDCheckDisk(void);

/* --- dolphin/vi + Midway DEMO layer --- */
void VISetBlack(BOOL black);
void VIFlush(void);
void VIWaitForRetrace(void);
void DEMOSwapBuffers(void);
void DEMODoneRender(void);
void AISetStreamPlayState(u32 state);

/* --- dolphin/pad --- */
typedef struct PADStatus {
    u16 button;        /* +0x00 */
    s8  stickX;        /* +0x02 */
    s8  stickY;        /* +0x03 */
    s8  substickX;     /* +0x04 */
    s8  substickY;     /* +0x05 */
    u8  triggerLeft;   /* +0x06 */
    u8  triggerRight;  /* +0x07 */
    u8  analogA;       /* +0x08 */
    u8  analogB;       /* +0x09 */
    s8  err;           /* +0x0A */
    u8  pad;           /* +0x0B */
} PADStatus;           /* 0x0C */

u32  PADRead(PADStatus* status);
void PADClamp(PADStatus* status);
u32  PADReset(u32 mask);
BOOL PADRecalibrate(u32 mask);

/* --- game --- */
void G3DReadControlPadStates(void);
void sndTestStopAll(void);
void MBEndFrame(void);
void vibrators_off(void);
void* memset(void* p, u32 n, int c);
int  sprintf();

/* PAD_ERR codes */
#define PAD_ERR_NONE          0
#define PAD_ERR_NO_CONTROLLER (-1)
#define PAD_ERR_NOT_READY     (-2)
#define PAD_ERR_TRANSFER      (-3)

/* button combos that arm a soft reset */
#define PAD_RESET_COMBO_A 0x1C00 /* START | Y | X    */
#define PAD_RESET_COMBO_B 0x1600 /* START | X | B    */

/* gSysFlags bits */
#define SF_RESET_REQ     0x00000001 /* soft reset queued              */
#define SF_RESET_HELD    0x00000002 /* reset combo held long enough   */
#define SF_RESET_DOWN    0x00000004 /* RESET button currently down    */
#define SF_RESET_TRIGGER 0x00000008 /* RESET button released -> fire  */
#define SF_ASSERT        0x00000020 /* assert message pending         */
#define SF_NO_RESET      0x00000040 /* suppress reset handling        */
#define SF_RESET_RDY     0x00000800 /* subsystems idle, safe to reset */
#define SF_RECALIB       0x00001000 /* pads need recalibration        */

/* Inline flag query. The shipped build inlines sysTestFlags() (which returns
 * a byte-wide boolean), so each test materialises the flag into 0/1 and then
 * compares. Reproduced here as the byte-cast ternary the inliner emitted. */
#define SYS_FLAG(f) ((u8)((gSysFlags & (f)) ? TRUE : FALSE))

/* --- forward declarations (DOL call order) --- */
BOOL       sysPollResetButton(void);
void       sysFadeToBlack(void);
BOOL       sysResetReady(void);
BOOL       padUpdate(void);
void       sysClearFlags(u32 mask);
void       sysSetFlags(u32 mask);
BOOL       sysTestFlags(u32 mask);

/* --- state (real addresses in .sdata/.sbss/.bss) --- */
static u32   gSysFlags;             /* 0x803452E0 */
static s32   gResetState;           /* 0x803452DC */
static s32   gLastResetBtnState;    /* 0x803452D4 */
static u32   gPadErrMask;           /* 0x803452D0 mask of active pads */
static u32   gLastFrameTicks;       /* 0x803452E4 */
static u32   gFrameDeltaTicks;      /* 0x803452D8 */
extern long  sSeconds;              /* 0x80345150 monotonically-updated seconds (owned by pb/pbutils.c) */

static void (*gMsgCallback)(const char*);  /* 0x803452CC on-screen message sink */
static void (*gSysResetCallback)(void);    /* reset notification hook */

static char* gMsgLines[2];          /* 0x80344030 two on-screen line buffers */
static s32   gMsgMaxLen;            /* 0x80344038 max chars per line */
static s32   lbl_80344800;          /* frames spent in assert pump */

/* pad-manager block @0x80321AD8 (fields kept separate for readability) */
static u32       gPadResetHoldTimer[4]; /* 0x80321B1C */
static u32       gPadStartHoldTimer[4]; /* 0x80321B2C */
static PADStatus gPadStatusBuf[2][4];   /* double-buffered PADStatus arrays */
static s32       gPadTimerA[2];         /* 0x803452E8 */
static s32       gPadTimerB[2];         /* 0x803452F0 */
static u8        gPadFlagsB[8];         /* 0x803452F8 */

static PADStatus* gPadCur  = gPadStatusBuf[0]; /* 0x80344040 */
static PADStatus* gPadPrev = gPadStatusBuf[1]; /* 0x8034403C */

/* 0x800DD180 - per-frame reset/eject state machine + pad pump */
void sysResetService(void) {
    s32 i;

    if (gMsgCallback) {
        gMsgCallback(NULL);
    }

    if (gLastFrameTicks == 0) {
        gLastFrameTicks = sSeconds;
    }
    gFrameDeltaTicks = sSeconds - gLastFrameTicks;
    gLastFrameTicks = sSeconds;

    switch (gResetState) {
    case 0:
        gResetState = 1;
        break;

    case 1:
        if (gSysFlags & SF_ASSERT) {
            if (gSysFlags & SF_RESET_TRIGGER) {
                if (gMsgCallback) {
                    gMsgCallback("RESET RELEASED..");
                }
                gSysFlags &= ~SF_ASSERT;
                gSysFlags &= ~SF_RESET_TRIGGER;
            }
            if (gSysFlags & SF_ASSERT) {
                if (gMsgCallback) {
                    gMsgCallback(gMsgLines[0]);
                }
                if (gMsgCallback) {
                    gMsgCallback(gMsgLines[1]);
                }
            }
        }
        if (!(gSysFlags & SF_NO_RESET)) {
            sysPollResetButton();
            if (gSysFlags & (SF_RESET_TRIGGER | SF_RESET_HELD)) {
                if (gMsgCallback) {
                    gMsgCallback("RESET INVOKED..");
                }
                sysFadeToBlack();
                if (DVDCheckDisk() == 0) {
                    OSResetSystem(1, 0x80000000, FALSE);
                } else {
                    OSResetSystem(0, 0x80000000, FALSE);
                }
            }
        }
        if (padUpdate() == 0) {
            gSysFlags |= SF_RESET_RDY;
        } else {
            gSysFlags &= ~SF_RESET_RDY;
            if (sysResetReady()) {
                gResetState = 2;
                gSysFlags |= SF_RESET_REQ;
            }
        }
        break;

    case 2:
        if (gSysFlags & SF_ASSERT) {
            if (gSysFlags & SF_RESET_TRIGGER) {
                if (gMsgCallback) {
                    gMsgCallback("RESET RELEASED..");
                }
                gSysFlags &= ~SF_ASSERT;
                gSysFlags &= ~SF_RESET_TRIGGER;
            }
            if (gSysFlags & SF_ASSERT) {
                if (gMsgCallback) {
                    gMsgCallback(gMsgLines[0]);
                }
                if (gMsgCallback) {
                    gMsgCallback(gMsgLines[1]);
                }
            }
        }
        if (!(gSysFlags & SF_NO_RESET)) {
            sysPollResetButton();
            if (gSysFlags & (SF_RESET_TRIGGER | SF_RESET_HELD)) {
                if (gMsgCallback) {
                    gMsgCallback("RESET INVOKED..");
                }
                sysFadeToBlack();
                if (DVDCheckDisk() == 0) {
                    OSResetSystem(1, 0x80000000, FALSE);
                } else {
                    OSResetSystem(0, 0x80000000, FALSE);
                }
            }
        }
        if (padUpdate() == 0) {
            gSysFlags &= ~SF_RESET_REQ;
            gSysFlags |= SF_RESET_RDY;
        } else {
            if (!sysResetReady()) {
                gSysFlags &= ~SF_RESET_REQ;
            }
        }
        if (gSysFlags & SF_RESET_REQ) {
            gSysFlags &= ~SF_RECALIB;
            for (i = 0; i < 4; i++) {
                if (gPadCur[0].err == PAD_ERR_NONE && gPadStartHoldTimer[i] > 3000) {
                    gSysFlags |= SF_RECALIB;
                }
            }
        } else {
            gResetState = 1;
        }
        break;

    default:
        break;
    }
}

/* 0x800DD604 - immediate reset (button/combo detected outside the pump) */
void sysHandleReset(void) {
    s32 i;
    u8 unused[8];

    if (SYS_FLAG(SF_NO_RESET)) {
        return;
    }
    sysPollResetButton();
    if (SYS_FLAG(SF_RESET_TRIGGER) || SYS_FLAG(SF_RESET_HELD)) {
        if (gMsgCallback) {
            gMsgCallback("RESET INVOKED..");
        }
        sndTestStopAll();
        vibrators_off();
        i = 3;
        VISetBlack(TRUE);
        do {
            DEMOSwapBuffers();
            DEMODoneRender();
            VIFlush();
            VIWaitForRetrace();
        } while (--i != 0);
        AISetStreamPlayState(0);
        if (DVDCheckDisk() != 0) {
            OSResetSystem(0, 0x80000000, FALSE);
        } else {
            OSResetSystem(1, 0x80000000, FALSE);
        }
    }
}

/* 0x800DD708 - blank the screen over three frames and mute streaming audio */
void sysFadeToBlack(void) {
    s32 i;
    u8 unused[48];

    sndTestStopAll();
    vibrators_off();
    i = 3;
    VISetBlack(TRUE);
    do {
        DEMOSwapBuffers();
        DEMODoneRender();
        VIFlush();
        VIWaitForRetrace();
    } while (--i != 0);
    AISetStreamPlayState(0);
}

/* 0x800DD760 - clear pad manager state, mark all pads not-yet-read */
void padInit(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        gPadFlagsB[i] = 0;
        gPadTimerB[i] = 0;
        gPadTimerA[i] = 0;
    }
    memset(gPadCur, sizeof(PADStatus) * 4, 0);
    memset(gPadPrev, sizeof(PADStatus) * 4, 0);
    for (i = 0; i < 4; i++) {
        gPadResetHoldTimer[i] = 0;
        gPadCur[i].err = PAD_ERR_NOT_READY;
        gPadPrev[i].err = PAD_ERR_NOT_READY;
    }
}

/* 0x800DD818 - sample the RESET button, accumulate the combo hold timers. */
BOOL sysPollResetButton(void) {
    BOOL fire = FALSE;
    s32 state;
    s32 i;
    u8 unused[8];

    state = OSGetResetButtonState();
    if (state != gLastResetBtnState) {
        if (state == 0) {
            /* button released: arm the trigger, drop the "held" flag */
            gSysFlags |= SF_RESET_TRIGGER;
            gSysFlags &= ~SF_RESET_DOWN;
        } else {
            gSysFlags |= SF_RESET_DOWN;
        }
        gLastResetBtnState = state;
    }
    if (SYS_FLAG(SF_RESET_TRIGGER) == TRUE) {
        fire = TRUE;
    }
    gSysFlags &= ~SF_RESET_HELD;
    for (i = 0; i < 4; i++) {
        if (gPadCur[0].err == PAD_ERR_NONE && gPadResetHoldTimer[i] > 500) {
            fire = TRUE;
            gSysFlags |= SF_RESET_HELD;
        }
    }
    return fire;
}

#ifdef __MWERKS__
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif

/* 0x800DD908 - hook for subsystems to veto a reset (always ready here) */
BOOL sysResetReady(void) {
    return TRUE;
}

/* 0x800DD910 - swap pad buffers, read all pads, update reset hold timers */
BOOL padUpdate(void) {
    u32 resetMask = 0;
    s32 i;

    if (gPadCur == gPadStatusBuf[1]) {
        gPadPrev = gPadStatusBuf[1];
        gPadCur = gPadStatusBuf[0];
    } else {
        gPadCur = gPadStatusBuf[1];
        gPadPrev = gPadStatusBuf[0];
    }
    gPadErrMask = 0;

    PADRead(gPadCur);
    PADClamp(gPadCur);
    G3DReadControlPadStates();

    for (i = 0; i < 4; i++) {
        u32 bit = 0x80000000u >> i;
        s8 err = gPadCur[i].err;
        if (err == PAD_ERR_NO_CONTROLLER) {
            resetMask |= bit;
        } else if (err == PAD_ERR_TRANSFER) {
            gPadErrMask |= bit;
        } else if (err == PAD_ERR_NONE) {
            gPadErrMask |= bit;
            if (gPadCur[i].button == PAD_RESET_COMBO_A) {
                gPadStartHoldTimer[i] += gFrameDeltaTicks;
            } else {
                gPadStartHoldTimer[i] = 0;
            }
            if (gPadCur[i].button == PAD_RESET_COMBO_B) {
                gPadResetHoldTimer[i] += gFrameDeltaTicks;
            } else {
                gPadResetHoldTimer[i] = 0;
            }
        }
    }

    if (gSysFlags & SF_RECALIB) {
        PADRecalibrate(0xF0000000);
    } else if (resetMask != 0) {
        PADReset(resetMask);
    }
    return gPadErrMask != 0;
}

/* 0x800DDABC - edge-detected menu stick: -1 up, +1 down, 0 none */
s32 padMenuStickY(u32 padMask) {
    s32 i;

    for (i = 0; padMask != 0 && i < 4; i++, padMask >>= 1) {
        if (padMask & 1) {
            if (gPadCur[i].err == PAD_ERR_NONE && gPadPrev[i].err == PAD_ERR_NONE) {
                if (gPadCur[i].stickY > 50) {
                    if (gPadPrev[i].stickY < 50) {
                        return -1;
                    }
                }
                if (gPadCur[i].stickY < -50) {
                    if (gPadPrev[i].stickY > -50) {
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

/* 0x800DDB68 - TRUE if any selected pad newly pressed a button in `button` */
BOOL padButtonPressed(u32 padMask, int button) {
    BOOL found = FALSE;
    s32 i;

    for (i = 0; padMask != 0 && i < 4; i++, padMask >>= 1) {
        if (padMask & 1) {
            if (gPadCur[i].err == PAD_ERR_NONE && gPadPrev[i].err == PAD_ERR_NONE) {
                u16 pressed = gPadCur[i].button & (gPadPrev[i].button ^ gPadCur[i].button);
                if (button & pressed) {
                    found = TRUE;
                    break;
                }
            }
        }
    }
    return found;
}

/* 0x800DDBF0 - TRUE if any selected pad newly released a button in `button` */
BOOL padButtonReleased(u32 padMask, int button) {
    BOOL found = FALSE;
    s32 i;

    i = 0;
    while (padMask != 0 && i < 4) {
        /* Shipped control flow skips the loop update on an unselected bit. */
        if (!(padMask & 1)) {
            continue;
        }
        if (gPadCur[i].err == PAD_ERR_NONE && gPadPrev[i].err == PAD_ERR_NONE) {
            u16 released = gPadPrev[i].button & (gPadPrev[i].button ^ gPadCur[i].button);
            if (button & released) {
                found = TRUE;
                break;
            }
        }
        padMask >>= 1;
        i++;
    }
    return found;
}

/* 0x800DDC78 - current-frame PADStatus buffer */
PADStatus* G3DGetPadStatusBuffer(void) {
    return gPadCur;
}

/* 0x800DDC80 - format an assert into the two message lines and block on reset */
void sysAssertFailed(const char* msg, const char* file, int line) {
    char buf[0x100];
    char* dst;
    s32 i;
    u8 unused[8];

    if (SYS_FLAG(SF_ASSERT)) {
        return;
    }

    sprintf(buf, "Assert Failed: File:%s Line:%d", file, line);
    dst = gMsgLines[0];
    for (i = 0; i < gMsgMaxLen - 1; i++) {
        dst[i] = buf[i];
        if (buf[i] == 0) {
            i = gMsgMaxLen;
        }
    }
    dst[i] = 0;

    sprintf(buf, "Message: %s Press RESET to continue.", msg);
    dst = gMsgLines[1];
    for (i = 0; i < gMsgMaxLen - 1; i++) {
        dst[i] = buf[i];
        if (buf[i] == 0) {
            i = gMsgMaxLen;
        }
    }
    dst[i] = 0;

    if (SYS_FLAG(SF_ASSERT)) {
        return;
    }
    gSysFlags |= SF_ASSERT;
    while (SYS_FLAG(SF_ASSERT) == TRUE) {
        lbl_80344800++;
        sysResetService();
        MBEndFrame();
    }
}

/* 0x800DDDE8 */
void sysClearFlags(u32 mask) {
    asm {
        lwz r4,gSysFlags(r0)
        andc r0,r4,r3
        stw r0,gSysFlags(r0)
    }
}

/* 0x800DDDF8 */
void sysSetFlags(u32 mask) {
    gSysFlags |= mask;
}

/* 0x800DDE08 */
BOOL sysTestFlags(u32 mask) {
    if (gSysFlags & mask) {
        return TRUE;
    }
    return FALSE;
}

/* 0x800DDE24 */
void sysSetMsgCallback(void (*cb)(const char*)) {
    gMsgCallback = cb;
}

/* 0x800DDE2C */
void sysSetResetCallback(void (*cb)(void)) {
    gSysResetCallback = cb;
}
