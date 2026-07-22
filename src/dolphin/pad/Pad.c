#include "types.h"
#include "dolphin/pad.h"
#include "dolphin/si.h"
#include "dolphin/os/OSContext.h"

typedef s64 OSTime;

#define PAD_ALL                                                                                  \
    (PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT | PAD_BUTTON_DOWN | PAD_BUTTON_UP | PAD_TRIGGER_Z |      \
     PAD_TRIGGER_R | PAD_TRIGGER_L | PAD_BUTTON_A | PAD_BUTTON_B | PAD_BUTTON_X | PAD_BUTTON_Y | \
     PAD_BUTTON_MENU | 0x2000 | 0x0080)

int OSDisableInterrupts(void);
void OSRestoreInterrupts(int level);
OSTime OSGetTime(void);
void OSSetWirelessID(s32 chan, u16 id);
void OSClearContext(OSContext* context);
void OSSetCurrentContext(OSContext* context);
void* memset(void* dst, int val, u32 n);

typedef struct OSResetFunctionInfo OSResetFunctionInfo;
struct OSResetFunctionInfo {
    /* 0x0 */ BOOL (*func)(BOOL final);
    /* 0x4 */ u32 priority;
    /* 0x8 */ OSResetFunctionInfo* next;
    /* 0xC */ OSResetFunctionInfo* prev;
};
void OSRegisterResetFunction(OSResetFunctionInfo* info);

u16 __OSWirelessPadFixMode : 0x800030E0;
u8 GameChoice : 0x800030E3;

extern u32 __PADFixBits;
u32 __PADSpec;

static s32 ResettingChan = 32;
static u32 XPatchBits = PAD_CHAN0_BIT | PAD_CHAN1_BIT | PAD_CHAN2_BIT | PAD_CHAN3_BIT;
static u32 AnalogMode = 0x00000300;
static u32 Spec = PAD_SPEC_5;

static BOOL Initialized;
static u32 EnabledBits;
static u32 ResettingBits;
static u32 RecalibrateBits;
static u32 WaitingBits;
static u32 CheckingBits;
static u32 PendingBits;

static u32 Type[SI_MAX_CHAN];
static PADStatus Origin[SI_MAX_CHAN];
static u32 CmdProbeDevice[SI_MAX_CHAN];

static void UpdateOrigin(s32 chan);
static void PADOriginCallback(s32 chan, u32 error, OSContext* context);
static void PADOriginUpdateCallback(s32 chan, u32 error, OSContext* context);
static void PADProbeCallback(s32 chan, u32 error, OSContext* context);
static void PADTypeAndStatusCallback(s32 chan, u32 type);
static void PADReceiveCheckCallback(s32 chan, u32 type);
static void SPEC0_MakeStatus(s32 chan, PADStatus* status, u32 data[2]);
static void SPEC1_MakeStatus(s32 chan, PADStatus* status, u32 data[2]);
static void SPEC2_MakeStatus(s32 chan, PADStatus* status, u32 data[2]);
static BOOL OnReset(BOOL final);
static void SamplingHandler(s16 interrupt, OSContext* context);

static void (*MakeStatus)(s32 chan, PADStatus* status, u32* data) = SPEC2_MakeStatus;

static PADSamplingCallback SamplingCallback;

static OSResetFunctionInfo ResetFunctionInfo = {
    OnReset,
    127,
    NULL,
    NULL,
};

static void DoReset(void) {
    ResettingChan = __cntlzw(ResettingBits);
    if (ResettingChan != 32) {
        u32 chanBit = PAD_CHAN0_BIT >> ResettingChan;
        ResettingBits &= ~chanBit;
        memset(&Origin[ResettingChan], 0, sizeof(Origin[0]));
        SIGetTypeAsync(ResettingChan, PADTypeAndStatusCallback);
    }
}

static void PADEnable(s32 chan) {
    u32 cmd;
    u32 chanBit;
    u32 data[2];

    chanBit = PAD_CHAN0_BIT >> chan;
    EnabledBits |= chanBit;
    SIGetResponse(chan, &data);
    cmd = AnalogMode | 0x00400000;
    SISetCommand(chan, cmd);
    SIEnablePolling(EnabledBits);
}

static void PADDisable(s32 chan) {
    BOOL enabled;
    u32 chanBit;

    enabled = OSDisableInterrupts();
    chanBit = PAD_CHAN0_BIT >> chan;
    SIDisablePolling(chanBit);
    EnabledBits &= ~chanBit;
    WaitingBits &= ~chanBit;
    CheckingBits &= ~chanBit;
    PendingBits &= ~chanBit;
    OSSetWirelessID(chan, 0);
    OSRestoreInterrupts(enabled);
}

static void UpdateOrigin(s32 chan) {
    PADStatus* origin;
    u32 mode = AnalogMode & 0x00000700;
    u32 chanBit = PAD_CHAN0_BIT >> chan;

    origin = &Origin[chan];
    switch (mode) {
    case 0x00000000:
    case 0x00000500:
    case 0x00000600:
    case 0x00000700:
        origin->triggerLeft &= ~15;
        origin->triggerRight &= ~15;
        origin->analogA &= ~15;
        origin->analogB &= ~15;
        break;
    case 0x00000100:
        origin->substickX &= ~15;
        origin->substickY &= ~15;
        origin->analogA &= ~15;
        origin->analogB &= ~15;
        break;
    case 0x00000200:
        origin->substickX &= ~15;
        origin->substickY &= ~15;
        origin->triggerLeft &= ~15;
        origin->triggerRight &= ~15;
        break;
    case 0x00000300:
        break;
    case 0x00000400:
        break;
    }

    origin->stickX -= 128;
    origin->stickY -= 128;
    origin->substickX -= 128;
    origin->substickY -= 128;

    if ((XPatchBits & chanBit) && 0x40 < origin->stickX &&
        (SIGetType(chan) & 0xFFFF0000) == SI_GC_CONTROLLER) {
        origin->stickX = 0;
    }
}

static void PADOriginCallback(s32 chan, u32 error, OSContext* context) {
    if (!(error &
          (SI_ERROR_UNDER_RUN | SI_ERROR_OVER_RUN | SI_ERROR_NO_RESPONSE | SI_ERROR_COLLISION))) {
        UpdateOrigin(ResettingChan);
        PADEnable(ResettingChan);
    }
    DoReset();
}

static void PADOriginUpdateCallback(s32 chan, u32 error, OSContext* context) {
    if (!(EnabledBits & (PAD_CHAN0_BIT >> chan))) {
        return;
    }

    if (!(error &
          (SI_ERROR_UNDER_RUN | SI_ERROR_OVER_RUN | SI_ERROR_NO_RESPONSE | SI_ERROR_COLLISION))) {
        UpdateOrigin(chan);
    }

    if (error & SI_ERROR_NO_RESPONSE) {
        PADDisable(chan);
    }
}

static void PADProbeCallback(s32 chan, u32 error, OSContext* context) {
    if (!(error &
          (SI_ERROR_UNDER_RUN | SI_ERROR_OVER_RUN | SI_ERROR_NO_RESPONSE | SI_ERROR_COLLISION))) {
        PADEnable(ResettingChan);
        WaitingBits |= PAD_CHAN0_BIT >> ResettingChan;
    }
    DoReset();
}

static u32 CmdReadOrigin = 0x41 << 24;
static u32 CmdCalibrate = 0x42 << 24;

static void PADTypeAndStatusCallback(s32 chan, u32 type) {
    u32 chanBit;
    u32 recalibrate;
    BOOL rc = TRUE;

    chanBit = PAD_CHAN0_BIT >> ResettingChan;
    recalibrate = RecalibrateBits & chanBit;
    RecalibrateBits &= ~chanBit;

    if (type & 0xF) {
        DoReset();
        return;
    }

    type &= ~0xFF;
    Type[ResettingChan] = type;

    if ((type & SI_TYPE_MASK) != SI_TYPE_GC || !(type & SI_GC_STANDARD)) {
        DoReset();
        return;
    }

    if (Spec < PAD_SPEC_2) {
        PADEnable(ResettingChan);
        DoReset();
        return;
    }

    if (!(type & SI_GC_WIRELESS) || (type & SI_WIRELESS_IR)) {
        if (recalibrate) {
            rc = SITransfer(ResettingChan, &CmdCalibrate, 3, &Origin[ResettingChan], 10,
                            PADOriginCallback, 0);
        } else {
            rc = SITransfer(ResettingChan, &CmdReadOrigin, 1, &Origin[ResettingChan], 10,
                            PADOriginCallback, 0);
        }
    } else if ((type & SI_WIRELESS_FIX_ID) && !(type & SI_WIRELESS_CONT_MASK) &&
               !(type & SI_WIRELESS_LITE)) {
        if (type & SI_WIRELESS_RECEIVED) {
            rc = SITransfer(ResettingChan, &CmdReadOrigin, 1, &Origin[ResettingChan], 10,
                            PADOriginCallback, 0);
        } else {
            rc = SITransfer(ResettingChan, &CmdProbeDevice[ResettingChan], 3,
                            &Origin[ResettingChan], 8, PADProbeCallback, 0);
        }
    }

    if (!rc) {
        PendingBits |= chanBit;
        DoReset();
    }
}

static void PADReceiveCheckCallback(s32 chan, u32 error) {
    u32 type;
    u32 chanBit = PAD_CHAN0_BIT >> chan;

    if (EnabledBits & chanBit) {
        WaitingBits &= ~chanBit;
        CheckingBits &= ~chanBit;
        type = error & 0xFFFFFF00;

        if (!(error & 0xF) && (type & SI_GC_WIRELESS) && (type & SI_WIRELESS_FIX_ID) &&
            (type & SI_WIRELESS_RECEIVED) && !(type & SI_WIRELESS_IR) &&
            !(type & SI_WIRELESS_CONT_MASK) && !(type & SI_WIRELESS_LITE)) {
            SITransfer(chan, &CmdReadOrigin, 1, &Origin[chan], 10, PADOriginUpdateCallback, 0);
        } else {
            PADDisable(chan);
        }
    }
}

BOOL PADReset(u32 mask) {
    BOOL enabled;
    u32 disableBits;

    enabled = OSDisableInterrupts();

    mask |= PendingBits;
    PendingBits = 0;
    mask &= ~(WaitingBits | CheckingBits);
    ResettingBits |= mask;
    disableBits = ResettingBits & EnabledBits;
    EnabledBits &= ~mask;

    if (Spec == PAD_SPEC_4) {
        RecalibrateBits |= mask;
    }

    SIDisablePolling(disableBits);
    if (ResettingChan == 32) {
        DoReset();
    }
    OSRestoreInterrupts(enabled);
    return TRUE;
}

BOOL PADRecalibrate(u32 mask) {
    BOOL enabled;
    u32 disableBits;

    enabled = OSDisableInterrupts();

    mask |= PendingBits;
    PendingBits = 0;
    mask &= ~(WaitingBits | CheckingBits);
    ResettingBits |= mask;
    disableBits = ResettingBits & EnabledBits;
    EnabledBits &= ~mask;

    if (!(GameChoice & 0x40)) {
        RecalibrateBits |= mask;
    }

    SIDisablePolling(disableBits);
    if (ResettingChan == 32) {
        DoReset();
    }
    OSRestoreInterrupts(enabled);
    return TRUE;
}

BOOL PADInit(void) {
    u8 _[8];

    if (Initialized) {
        return TRUE;
    }

    if (__PADSpec) {
        PADSetSpec(__PADSpec);
    }

    Initialized = TRUE;

    if (__PADFixBits != 0) {
        OSTime time = OSGetTime();
        __OSWirelessPadFixMode =
            (u16)(((time & 0xffff) + ((time >> 16) & 0xffff) + ((time >> 32) & 0xffff) +
                   ((time >> 48) & 0xffff)) &
                  0x3fff);
        RecalibrateBits = PAD_CHAN0_BIT | PAD_CHAN1_BIT | PAD_CHAN2_BIT | PAD_CHAN3_BIT;
    }

    CmdProbeDevice[0] = ((__OSWirelessPadFixMode & 0x3fffu) << 8) | 0x4D000000;
    CmdProbeDevice[1] = ((__OSWirelessPadFixMode & 0x3fffu) << 8) | 0x4D400000;
    CmdProbeDevice[2] = ((__OSWirelessPadFixMode & 0x3fffu) << 8) | 0x4D800000;
    CmdProbeDevice[3] = ((__OSWirelessPadFixMode & 0x3fffu) << 8) | 0x4DC00000;

    SIRefreshSamplingRate();
    OSRegisterResetFunction(&ResetFunctionInfo);

    return PADReset(PAD_CHAN0_BIT | PAD_CHAN1_BIT | PAD_CHAN2_BIT | PAD_CHAN3_BIT);
}

u32 PADRead(PADStatus* status) {
    BOOL enabled = OSDisableInterrupts();
    u32 data[2];
    u32 chanBit;
    s32 chan;
    u8 _[4];
    u32 motor = 0;

    for (chan = 0; chan < SI_MAX_CHAN; chan++, status++) {
        chanBit = PAD_CHAN0_BIT >> chan;

        if (PendingBits & chanBit) {
            PADReset(0);
            status->err = PAD_ERR_NOT_READY;
            memset(status, 0, 10);
        } else if ((ResettingBits & chanBit) || (ResettingChan == chan)) {
            status->err = PAD_ERR_NOT_READY;
            memset(status, 0, 10);
        } else if (!(EnabledBits & chanBit)) {
            status->err = PAD_ERR_NO_CONTROLLER;
            memset(status, 0, 10);
        } else if (SIIsChanBusy(chan)) {
            status->err = PAD_ERR_TRANSFER;
            memset(status, 0, 10);
        } else {
            if (SIGetStatus(chan) & 8) {
                SIGetResponse(chan, &data);
                if (WaitingBits & chanBit) {
                    status->err = PAD_ERR_NONE;
                    memset(status, 0, 10);
                    if (!(CheckingBits & chanBit)) {
                        CheckingBits |= chanBit;
                        SIGetTypeAsync(chan, PADReceiveCheckCallback);
                    }
                } else {
                    PADDisable(chan);
                    status->err = PAD_ERR_NO_CONTROLLER;
                    memset(status, 0, 10);
                }
            } else {
                if (!(SIGetType(chan) & SI_GC_NOMOTOR)) {
                    motor |= chanBit;
                }

                if (!SIGetResponse(chan, &data)) {
                    status->err = PAD_ERR_TRANSFER;
                    memset(status, 0, 10);
                } else if (data[0] & 0x80000000) {
                    status->err = PAD_ERR_TRANSFER;
                    memset(status, 0, 10);
                } else {
                    MakeStatus(chan, status, data);
                    if (status->button & 0x2000) {
                        status->err = PAD_ERR_TRANSFER;
                        memset(status, 0, 10);
                        SITransfer(chan, &CmdReadOrigin, 1, &Origin[chan], 10,
                                   PADOriginUpdateCallback, 0);
                    } else {
                        status->err = PAD_ERR_NONE;
                        status->button &= 0xFFFFFF7F;
                    }
                }
            }
        }
    }

    OSRestoreInterrupts(enabled);
    return motor;
}

void PADControlAllMotors(const u32* commandArray) {
    BOOL enabled;
    s32 chan;
    u32 command;
    BOOL commit;
    u32 chanBit;

    enabled = OSDisableInterrupts();
    commit = FALSE;

    for (chan = 0; chan < SI_MAX_CHAN; chan++, commandArray++) {
        chanBit = PAD_CHAN0_BIT >> chan;
        if ((EnabledBits & chanBit) && !(SIGetType(chan) & SI_GC_NOMOTOR)) {
            command = *commandArray;
            if (Spec < PAD_SPEC_2 && command == PAD_MOTOR_STOP_HARD) {
                command = PAD_MOTOR_STOP;
            }
            SISetCommand(chan, (0x40 << 16) | AnalogMode |
                                   (command & (PAD_MOTOR_RUMBLE | PAD_MOTOR_STOP_HARD)));
            commit = TRUE;
        }
    }
    if (commit) {
        SITransferCommands();
    }
    OSRestoreInterrupts(enabled);
}

void PADControlMotor(s32 chan, u32 command) {
    BOOL enabled;
    u32 chanBit;

    enabled = OSDisableInterrupts();
    chanBit = PAD_CHAN0_BIT >> chan;
    if ((EnabledBits & chanBit) && !(SIGetType(chan) & SI_GC_NOMOTOR)) {
        if (Spec < PAD_SPEC_2 && command == PAD_MOTOR_STOP_HARD) {
            command = PAD_MOTOR_STOP;
        }
        SISetCommand(chan, (0x40 << 16) | AnalogMode |
                               (command & (PAD_MOTOR_RUMBLE | PAD_MOTOR_STOP_HARD)));
        SITransferCommands();
    }
    OSRestoreInterrupts(enabled);
}

void PADSetSpec(u32 spec) {
    __PADSpec = 0;
    switch (spec) {
    case PAD_SPEC_0:
        MakeStatus = SPEC0_MakeStatus;
        break;
    case PAD_SPEC_1:
        MakeStatus = SPEC1_MakeStatus;
        break;
    case PAD_SPEC_2:
    case PAD_SPEC_3:
    case PAD_SPEC_4:
    case PAD_SPEC_5:
        MakeStatus = SPEC2_MakeStatus;
        break;
    }
    Spec = spec;
}

u32 PADGetSpec(void) {
    return Spec;
}

static void SPEC0_MakeStatus(s32 chan, PADStatus* status, u32 data[2]) {
    status->button = 0;
    status->button |= ((data[0] >> 16) & 0x0008) ? PAD_BUTTON_A : 0;
    status->button |= ((data[0] >> 16) & 0x0020) ? PAD_BUTTON_B : 0;
    status->button |= ((data[0] >> 16) & 0x0100) ? PAD_BUTTON_X : 0;
    status->button |= ((data[0] >> 16) & 0x0001) ? PAD_BUTTON_Y : 0;
    status->button |= ((data[0] >> 16) & 0x0010) ? PAD_BUTTON_START : 0;
    status->stickX = (s8)(data[1] >> 16);
    status->stickY = (s8)(data[1] >> 24);
    status->substickX = (s8)(data[1]);
    status->substickY = (s8)(data[1] >> 8);
    status->triggerLeft = (u8)(data[0] >> 8);
    status->triggerRight = (u8)data[0];
    status->analogA = 0;
    status->analogB = 0;
    if (170 <= status->triggerLeft) {
        status->button |= PAD_TRIGGER_L;
    }
    if (170 <= status->triggerRight) {
        status->button |= PAD_TRIGGER_R;
    }
    status->stickX -= 128;
    status->stickY -= 128;
    status->substickX -= 128;
    status->substickY -= 128;
}

static void SPEC1_MakeStatus(s32 chan, PADStatus* status, u32 data[2]) {
    status->button = 0;
    status->button |= ((data[0] >> 16) & 0x0080) ? PAD_BUTTON_A : 0;
    status->button |= ((data[0] >> 16) & 0x0100) ? PAD_BUTTON_B : 0;
    status->button |= ((data[0] >> 16) & 0x0020) ? PAD_BUTTON_X : 0;
    status->button |= ((data[0] >> 16) & 0x0010) ? PAD_BUTTON_Y : 0;
    status->button |= ((data[0] >> 16) & 0x0200) ? PAD_BUTTON_START : 0;
    status->stickX = (s8)(data[1] >> 16);
    status->stickY = (s8)(data[1] >> 24);
    status->substickX = (s8)(data[1]);
    status->substickY = (s8)(data[1] >> 8);
    status->triggerLeft = (u8)(data[0] >> 8);
    status->triggerRight = (u8)data[0];
    status->analogA = 0;
    status->analogB = 0;
    if (170 <= status->triggerLeft) {
        status->button |= PAD_TRIGGER_L;
    }
    if (170 <= status->triggerRight) {
        status->button |= PAD_TRIGGER_R;
    }
    status->stickX -= 128;
    status->stickY -= 128;
    status->substickX -= 128;
    status->substickY -= 128;
}

static s8 ClampS8(s8 var, s8 org) {
    if (0 < org) {
        s8 min = (s8)(-128 + org);
        if (var < min) {
            var = min;
        }
    } else if (org < 0) {
        s8 max = (s8)(127 + org);
        if (max < var) {
            var = max;
        }
    }
    return var -= org;
}

static u8 ClampU8(u8 var, u8 org) {
    if (var < org) {
        var = org;
    }
    return var -= org;
}

static void SPEC2_MakeStatus(s32 chan, PADStatus* status, u32 data[2]) {
    PADStatus* origin;

    status->button = (u16)((data[0] >> 16) & PAD_ALL);
    status->stickX = (s8)(data[0] >> 8);
    status->stickY = (s8)(data[0]);

    switch (AnalogMode & 0x00000700) {
    case 0x00000000:
    case 0x00000500:
    case 0x00000600:
    case 0x00000700:
        status->substickX = (s8)(data[1] >> 24);
        status->substickY = (s8)(data[1] >> 16);
        status->triggerLeft = (u8)(((data[1] >> 12) & 0x0f) << 4);
        status->triggerRight = (u8)(((data[1] >> 8) & 0x0f) << 4);
        status->analogA = (u8)(((data[1] >> 4) & 0x0f) << 4);
        status->analogB = (u8)(((data[1] >> 0) & 0x0f) << 4);
        break;
    case 0x00000100:
        status->substickX = (s8)(((data[1] >> 28) & 0x0f) << 4);
        status->substickY = (s8)(((data[1] >> 24) & 0x0f) << 4);
        status->triggerLeft = (u8)(data[1] >> 16);
        status->triggerRight = (u8)(data[1] >> 8);
        status->analogA = (u8)(((data[1] >> 4) & 0x0f) << 4);
        status->analogB = (u8)(((data[1] >> 0) & 0x0f) << 4);
        break;
    case 0x00000200:
        status->substickX = (s8)(((data[1] >> 28) & 0x0f) << 4);
        status->substickY = (s8)(((data[1] >> 24) & 0x0f) << 4);
        status->triggerLeft = (u8)(((data[1] >> 20) & 0x0f) << 4);
        status->triggerRight = (u8)(((data[1] >> 16) & 0x0f) << 4);
        status->analogA = (u8)(data[1] >> 8);
        status->analogB = (u8)(data[1] >> 0);
        break;
    case 0x00000300:
        status->substickX = (s8)(data[1] >> 24);
        status->substickY = (s8)(data[1] >> 16);
        status->triggerLeft = (u8)(data[1] >> 8);
        status->triggerRight = (u8)(data[1] >> 0);
        status->analogA = 0;
        status->analogB = 0;
        break;
    case 0x00000400:
        status->substickX = (s8)(data[1] >> 24);
        status->substickY = (s8)(data[1] >> 16);
        status->triggerLeft = 0;
        status->triggerRight = 0;
        status->analogA = (u8)(data[1] >> 8);
        status->analogB = (u8)(data[1] >> 0);
        break;
    }

    status->stickX -= 128;
    status->stickY -= 128;
    status->substickX -= 128;
    status->substickY -= 128;

    origin = &Origin[chan];
    status->stickX = ClampS8(status->stickX, origin->stickX);
    status->stickY = ClampS8(status->stickY, origin->stickY);
    status->substickX = ClampS8(status->substickX, origin->substickX);
    status->substickY = ClampS8(status->substickY, origin->substickY);
    status->triggerLeft = ClampU8(status->triggerLeft, origin->triggerLeft);
    status->triggerRight = ClampU8(status->triggerRight, origin->triggerRight);
}

BOOL PADGetType(s32 chan, u32* type) {
    u32 chanBit;

    *type = Type[chan];
    chanBit = PAD_CHAN0_BIT >> chan;
    if ((ResettingBits & chanBit) || ResettingChan == chan || !(EnabledBits & chanBit)) {
        return FALSE;
    }
    return TRUE;
}

BOOL PADSync(void) {
    return ResettingBits == 0 && ResettingChan == 32 && !SIBusy();
}

void PADSetAnalogMode(u32 mode) {
    BOOL enabled;
    u32 mask;

    enabled = OSDisableInterrupts();
    AnalogMode = mode << 8;
    mask = EnabledBits & ~PendingBits;

    EnabledBits &= ~mask;
    WaitingBits &= ~mask;
    CheckingBits &= ~mask;

    SIDisablePolling(mask);
    OSRestoreInterrupts(enabled);
}

static BOOL OnReset(BOOL final) {
    BOOL sync;
    static BOOL recalibrated = FALSE;
    u8 _[4];

    if (SamplingCallback) {
        PADSetSamplingCallback(NULL);
    }

    if (!final) {
        sync = PADSync();
        if (!recalibrated && sync) {
            PADRecalibrate(PAD_CHAN0_BIT | PAD_CHAN1_BIT | PAD_CHAN2_BIT | PAD_CHAN3_BIT);
            recalibrated = TRUE;
            return FALSE;
        }
        return sync;
    } else {
        recalibrated = FALSE;
    }

    return TRUE;
}

static void SamplingHandler(s16 interrupt, OSContext* context) {
    OSContext exceptionContext;

    if (SamplingCallback) {
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);
        SamplingCallback();
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);
    }
}

PADSamplingCallback PADSetSamplingCallback(PADSamplingCallback callback) {
    PADSamplingCallback prev;

    prev = SamplingCallback;
    SamplingCallback = callback;
    if (callback) {
        SIRegisterPollingHandler(SamplingHandler);
    } else {
        SIUnregisterPollingHandler(SamplingHandler);
    }
    return prev;
}

BOOL __PADDisableRecalibration(BOOL disable) {
    BOOL enabled;
    BOOL prev;

    enabled = OSDisableInterrupts();
    if (GameChoice & 0x40) {
        prev = TRUE;
    } else {
        prev = FALSE;
    }
    GameChoice &= 0xBF;
    if (disable) {
        GameChoice |= 0x40;
    }
    OSRestoreInterrupts(enabled);
    return prev;
}
