#include "types.h"
#include "dolphin/si.h"
#include "dolphin/os/OSContext.h"

typedef s64 OSTime;
typedef s16 __OSInterrupt;
typedef void (*__OSInterruptHandler)(__OSInterrupt interrupt, OSContext* context);

int OSDisableInterrupts(void);
void OSRestoreInterrupts(int level);
OSTime __OSGetSystemTime(void);
void OSReport(const char* msg, ...);
u16 OSGetWirelessID(s32 chan);
void OSSetWirelessID(s32 chan, u16 id);
__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler);
u32 __OSUnmaskInterrupts(u32 global);
u32 VIGetCurrentLine(void);

typedef struct OSAlarm OSAlarm;
typedef void (*OSAlarmHandler)(OSAlarm* alarm, OSContext* context);

struct OSAlarm {
    /* 0x00 */ OSAlarmHandler handler;
    /* 0x04 */ u32 tag;
    /* 0x08 */ OSTime fire;
    /* 0x10 */ OSAlarm* prev;
    /* 0x14 */ OSAlarm* next;
    /* 0x18 */ OSTime period;
    /* 0x20 */ OSTime start;
};

void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler);
void OSCancelAlarm(OSAlarm* alarm);

u32 __OSBusClock : 0x800000F8;
#define OS_BUS_CLOCK __OSBusClock
#define OS_TIMER_CLOCK (OS_BUS_CLOCK / 4)
#define OSMicrosecondsToTicks(usec) (((OS_TIMER_CLOCK / 125000) * (usec)) / 8)
#define OSMillisecondsToTicks(msec) ((msec) * (OS_TIMER_CLOCK / 1000))

volatile u32 __SIRegs[] : 0xCC006400;

#define SI_STATUS_IDX 14
#define SI_COMCSR_IDX 13

#define SI_COMCSR_TSTART_MASK 0x00000001
#define SI_COMCSR_TCINT_MASK 0x80000000

#define ROUND(n, a) (((u32)(n) + (a)-1) & ~((a)-1))

typedef struct SIPacket {
    /* 0x00 */ s32 chan;
    /* 0x04 */ void* output;
    /* 0x08 */ u32 outputBytes;
    /* 0x0C */ void* input;
    /* 0x10 */ u32 inputBytes;
    /* 0x14 */ SICallback callback;
    /* 0x18 */ OSTime time;
} SIPacket; // size 0x20

typedef struct SIControl {
    /* 0x00 */ s32 chan;
    /* 0x04 */ u32 poll;
    /* 0x08 */ u32 inputBytes;
    /* 0x0C */ void* input;
    /* 0x10 */ SICallback callback;
} SIControl; // size 0x14

static SIControl Si = {
    /* chan */ -1,
    /* poll */ 0,
    /* inputBytes */ 0,
    /* input */ NULL,
    /* callback */ NULL,
};

static u32 Type[SI_MAX_CHAN] = {
    SI_ERROR_NO_RESPONSE,
    SI_ERROR_NO_RESPONSE,
    SI_ERROR_NO_RESPONSE,
    SI_ERROR_NO_RESPONSE,
};

u32 __PADFixBits;

static SIPacket Packet[SI_MAX_CHAN];
static OSAlarm Alarm[SI_MAX_CHAN];
static OSTime TypeTime[SI_MAX_CHAN];
static OSTime XferTime[SI_MAX_CHAN];
static SITypeAndStatusCallback TypeCallback[SI_MAX_CHAN][4];
static __OSInterruptHandler RDSTHandler[4];
static BOOL InputBufferValid[SI_MAX_CHAN];
static u32 InputBuffer[SI_MAX_CHAN][2];
static volatile u32 InputBufferVcount[SI_MAX_CHAN];
static u32 cmdFixDevice[SI_MAX_CHAN];

static u32 CompleteTransfer(void);
static void SITransferNext(s32 chan);
static void SIInterruptHandler(__OSInterrupt interrupt, OSContext* context);
static BOOL __SITransfer(s32 chan, void* output, u32 outputBytes, void* input, u32 inputBytes,
                         SICallback callback);
static BOOL SIGetResponseRaw(s32 chan);
static void AlarmHandler(OSAlarm* alarm, OSContext* context);
static void GetTypeCallback(s32 chan, u32 error, OSContext* context);

BOOL SIBusy(void) {
    return (Si.chan != -1) ? TRUE : FALSE;
}

BOOL SIIsChanBusy(s32 chan) {
    return (Packet[chan].chan != -1 || Si.chan == chan);
}

static void SIClearTCInterrupt(void) {
    u32 reg;

    reg = __SIRegs[SI_COMCSR_IDX];
    reg |= 0x80000000;
    reg &= ~0x00000001;
    __SIRegs[SI_COMCSR_IDX] = reg;
}

static u32 CompleteTransfer(void) {
    u32 sr;
    u32 i;
    u32 rLen;
    u8* input;
    u32 temp;

    sr = __SIRegs[SI_STATUS_IDX];
    SIClearTCInterrupt();

    if (Si.chan != -1) {
        XferTime[Si.chan] = __OSGetSystemTime();

        input = Si.input;
        rLen = Si.inputBytes / 4;
        for (i = 0; i < rLen; i++) {
            *((u32*)input)++ = __SIRegs[i + 0x20];
        }

        rLen = Si.inputBytes & 3;
        if (rLen != 0) {
            temp = __SIRegs[i + 0x20];
            for (i = 0; i < rLen; i++) {
                *(input++) = (u8)(temp >> ((3 - i) * 8));
            }
        }

        if (__SIRegs[SI_COMCSR_IDX] & 0x20000000) {
            sr >>= 8 * (3 - Si.chan);
            sr &= 0xF;

            if ((sr & SI_ERROR_NO_RESPONSE) && !(Type[Si.chan] & SI_ERROR_BUSY)) {
                Type[Si.chan] = SI_ERROR_NO_RESPONSE;
            }
            if (sr == 0) {
                sr = SI_ERROR_COLLISION;
            }
        } else {
            TypeTime[Si.chan] = __OSGetSystemTime();
            sr = 0;
        }

        Si.chan = -1;
    }

    return sr;
}

static void SITransferNext(s32 chan) {
    int i;
    SIPacket* packet;

    for (i = 0; i < 4; i++) {
        chan++;
        chan %= 4;
        packet = &Packet[chan];

        if (packet->chan != -1) {
            if (packet->time <= __OSGetSystemTime()) {
                if (__SITransfer(packet->chan, packet->output, packet->outputBytes, packet->input,
                                 packet->inputBytes, packet->callback)) {
                    OSCancelAlarm(&Alarm[chan]);
                    packet->chan = -1;
                }
                return;
            }
        }
    }
}

static void SIInterruptHandler(__OSInterrupt interrupt, OSContext* context) {
    u32 reg;

    reg = __SIRegs[SI_COMCSR_IDX];

    if ((reg & 0xC0000000) == 0xC0000000) {
        s32 chan;
        u32 sr;
        SICallback callback;

        chan = Si.chan;
        sr = CompleteTransfer();
        callback = Si.callback;
        Si.callback = 0;

        SITransferNext(chan);

        if (callback) {
            callback(chan, sr, context);
        }

        sr = __SIRegs[SI_STATUS_IDX];
        sr &= 0x0F000000 >> (8 * chan);
        __SIRegs[SI_STATUS_IDX] = sr;

        if (Type[chan] == SI_ERROR_BUSY && !SIIsChanBusy(chan)) {
            static u32 cmdTypeAndStatus = 0 << 24;
            SITransfer(chan, &cmdTypeAndStatus, 1, &Type[chan], 3, GetTypeCallback,
                       OSMicrosecondsToTicks(65));
        }
    }

    if ((reg & 0x18000000) == 0x18000000) {
        int i;
        u32 vcount;
        u32 x;

        vcount = VIGetCurrentLine() + 1;
        x = (Si.poll & 0x03FF0000) >> 16;

        for (i = 0; i < SI_MAX_CHAN; ++i) {
            if (SIGetResponseRaw(i)) {
                InputBufferVcount[i] = vcount;
            }
        }

        for (i = 0; i < SI_MAX_CHAN; ++i) {
            if (!(Si.poll & (SI_CHAN0_BIT >> (31 - 7 + i)))) {
                continue;
            }
            if (InputBufferVcount[i] == 0 || InputBufferVcount[i] + (x / 2) < vcount) {
                return;
            }
        }

        for (i = 0; i < SI_MAX_CHAN; ++i) {
            InputBufferVcount[i] = 0;
        }

        for (i = 0; i < 4; ++i) {
            if (RDSTHandler[i]) {
                RDSTHandler[i](interrupt, context);
            }
        }
    }
}

static BOOL SIEnablePollingInterrupt(BOOL enable) {
    BOOL enabled;
    BOOL rc;
    u32 reg;
    int i;

    enabled = OSDisableInterrupts();
    reg = __SIRegs[SI_COMCSR_IDX];
    rc = (reg & 0x08000000) ? TRUE : FALSE;
    if (enable) {
        reg |= 0x08000000;
        for (i = 0; i < SI_MAX_CHAN; ++i) {
            InputBufferVcount[i] = 0;
        }
    } else {
        reg &= ~0x08000000;
    }
    reg &= ~0x80000001;
    __SIRegs[SI_COMCSR_IDX] = reg;
    OSRestoreInterrupts(enabled);
    return rc;
}

BOOL SIRegisterPollingHandler(__OSInterruptHandler handler) {
    BOOL enabled;
    int i;

    enabled = OSDisableInterrupts();
    for (i = 0; i < 4; ++i) {
        if (RDSTHandler[i] == handler) {
            OSRestoreInterrupts(enabled);
            return TRUE;
        }
    }
    for (i = 0; i < 4; ++i) {
        if (RDSTHandler[i] == 0) {
            RDSTHandler[i] = handler;
            SIEnablePollingInterrupt(TRUE);
            OSRestoreInterrupts(enabled);
            return TRUE;
        }
    }
    OSRestoreInterrupts(enabled);
    return FALSE;
}

BOOL SIUnregisterPollingHandler(__OSInterruptHandler handler) {
    BOOL enabled;
    int i;

    enabled = OSDisableInterrupts();
    for (i = 0; i < 4; ++i) {
        if (RDSTHandler[i] == handler) {
            RDSTHandler[i] = 0;
            for (i = 0; i < 4; ++i) {
                if (RDSTHandler[i]) {
                    break;
                }
            }
            if (i == 4) {
                SIEnablePollingInterrupt(FALSE);
            }
            OSRestoreInterrupts(enabled);
            return TRUE;
        }
    }
    OSRestoreInterrupts(enabled);
    return FALSE;
}

void SIInit(void) {
    Packet[0].chan = Packet[1].chan = Packet[2].chan = Packet[3].chan = -1;

    Si.poll = 0;
    SISetSamplingRate(0);

    while (__SIRegs[SI_COMCSR_IDX] & SI_COMCSR_TSTART_MASK) {
        ;
    }

    __SIRegs[SI_COMCSR_IDX] = SI_COMCSR_TCINT_MASK;

    __OSSetInterruptHandler(0x14, SIInterruptHandler);
    __OSUnmaskInterrupts(0x00000800);

    SIGetType(0);
    SIGetType(1);
    SIGetType(2);
    SIGetType(3);
}

static BOOL __SITransfer(s32 chan, void* output, u32 outputBytes, void* input, u32 inputBytes,
                         SICallback callback) {
    BOOL enabled;
    u32 rLen;
    u32 i;
    u32 sr;
    union {
        u32 val;
        struct {
            u32 tcint : 1;
            u32 tcintmsk : 1;
            u32 comerr : 1;
            u32 rdstint : 1;
            u32 rdstintmsk : 1;
            u32 pad2 : 4;
            u32 outlngth : 7;
            u32 pad1 : 1;
            u32 inlngth : 7;
            u32 pad0 : 5;
            u32 channel : 2;
            u32 tstart : 1;
        } f;
    } comcsr;

    enabled = OSDisableInterrupts();
    if (Si.chan != -1) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }

    sr = __SIRegs[SI_STATUS_IDX];
    sr &= 0x0F000000 >> (chan * 8);
    __SIRegs[SI_STATUS_IDX] = sr;

    Si.chan = chan;
    Si.callback = callback;
    Si.inputBytes = inputBytes;
    Si.input = input;

    rLen = ROUND(outputBytes, 4) / 4;
    for (i = 0; i < rLen; i++) {
        __SIRegs[i + 0x20] = ((u32*)output)[i];
    }

    comcsr.val = __SIRegs[SI_COMCSR_IDX];
    comcsr.f.tcint = 1;
    comcsr.f.tcintmsk = callback ? 1 : 0;
    comcsr.f.outlngth = outputBytes == 128 ? 0 : outputBytes;
    comcsr.f.inlngth = inputBytes == 128 ? 0 : inputBytes;
    comcsr.f.channel = chan;
    comcsr.f.tstart = 1;
    __SIRegs[SI_COMCSR_IDX] = comcsr.val;

    OSRestoreInterrupts(enabled);
    return TRUE;
}

u32 SISync(void) {
    BOOL enabled;
    u32 sr;

    while (__SIRegs[SI_COMCSR_IDX] & SI_COMCSR_TSTART_MASK) {
        ;
    }

    enabled = OSDisableInterrupts();
    sr = CompleteTransfer();
    SITransferNext(4);
    OSRestoreInterrupts(enabled);
    return sr;
}

u32 SIGetStatus(s32 chan) {
    BOOL enabled;
    u32 sr;

    enabled = OSDisableInterrupts();
    sr = __SIRegs[SI_STATUS_IDX];
    sr >>= (3 - chan) * 8;
    if (sr & SI_ERROR_NO_RESPONSE) {
        if (!(Type[chan] & SI_ERROR_BUSY)) {
            Type[chan] = SI_ERROR_NO_RESPONSE;
        }
    }
    OSRestoreInterrupts(enabled);
    return sr;
}

void SISetCommand(s32 chan, u32 command) {
    __SIRegs[chan * 3] = command;
}

u32 SIGetCommand(s32 chan) {
    return __SIRegs[chan * 3];
}

void SITransferCommands(void) {
    __SIRegs[SI_STATUS_IDX] = SI_COMCSR_TCINT_MASK;
}

u32 SISetXY(u32 x, u32 y) {
    u32 poll;
    BOOL enabled;

    poll = x << 16;
    poll |= y << 8;

    enabled = OSDisableInterrupts();
    Si.poll &= 0xFC0000FF;
    Si.poll |= poll;
    poll = Si.poll;
    __SIRegs[0x30 / 4] = poll;
    OSRestoreInterrupts(enabled);
    return poll;
}

u32 SIEnablePolling(u32 poll) {
    BOOL enabled;
    u32 en;

    if (poll == 0) {
        return Si.poll;
    }

    enabled = OSDisableInterrupts();
    poll >>= 24;
    en = poll & 0xF0;
    poll &= (en >> 4) | 0x03FFFFF0;
    poll &= 0xFC0000FF;

    Si.poll &= ~(en >> 4);
    Si.poll |= poll;
    poll = Si.poll;

    __SIRegs[SI_STATUS_IDX] = 0x80000000;
    __SIRegs[0x30 / 4] = poll;
    OSRestoreInterrupts(enabled);
    return poll;
}

u32 SIDisablePolling(u32 poll) {
    BOOL enabled;

    if (poll == 0) {
        return Si.poll;
    }

    enabled = OSDisableInterrupts();
    poll >>= 24;
    poll &= 0xF0;
    poll = Si.poll & ~poll;
    __SIRegs[0x30 / 4] = poll;
    Si.poll = poll;
    OSRestoreInterrupts(enabled);
    return poll;
}

static BOOL SIGetResponseRaw(s32 chan) {
    u32 sr;

    sr = SIGetStatus(chan);
    if (sr & SI_ERROR_RDST) {
        InputBuffer[chan][0] = __SIRegs[3 * chan + 1];
        InputBuffer[chan][1] = __SIRegs[3 * chan + 2];
        InputBufferValid[chan] = TRUE;
        return TRUE;
    }
    return FALSE;
}

BOOL SIGetResponse(s32 chan, void* data) {
    BOOL rc;
    BOOL enabled;

    enabled = OSDisableInterrupts();
    SIGetResponseRaw(chan);
    rc = InputBufferValid[chan];
    InputBufferValid[chan] = FALSE;
    if (rc) {
        ((u32*)data)[0] = InputBuffer[chan][0];
        ((u32*)data)[1] = InputBuffer[chan][1];
    }
    OSRestoreInterrupts(enabled);
    return rc;
}

static void AlarmHandler(OSAlarm* alarm, OSContext* context) {
    s32 chan;
    SIPacket* packet;

    chan = alarm - Alarm;
    packet = &Packet[chan];
    if (packet->chan != -1) {
        if (__SITransfer(packet->chan, packet->output, packet->outputBytes, packet->input,
                         packet->inputBytes, packet->callback)) {
            packet->chan = -1;
        }
    }
}

BOOL SITransfer(s32 chan, void* output, u32 outputBytes, void* input, u32 inputBytes,
                SICallback callback, OSTime delay) {
    BOOL enabled;
    SIPacket* packet;
    OSTime now;
    OSTime fire;

    packet = &Packet[chan];
    enabled = OSDisableInterrupts();
    if (packet->chan != -1 || Si.chan == chan) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }

    now = __OSGetSystemTime();
    if (delay == 0) {
        fire = now;
    } else {
        fire = XferTime[chan] + delay;
    }
    if (now < fire) {
        delay = fire - now;
        OSSetAlarm(&Alarm[chan], delay, AlarmHandler);
    } else if (__SITransfer(chan, output, outputBytes, input, inputBytes, callback)) {
        OSRestoreInterrupts(enabled);
        return TRUE;
    }

    packet->chan = chan;
    packet->output = output;
    packet->outputBytes = outputBytes;
    packet->input = input;
    packet->inputBytes = inputBytes;
    packet->callback = callback;
    packet->time = fire;
    OSRestoreInterrupts(enabled);
    return TRUE;
}

static void CallTypeAndStatusCallback(s32 chan, u32 type) {
    SITypeAndStatusCallback callback;
    int i;

    for (i = 0; i < 4; ++i) {
        callback = TypeCallback[chan][i];
        if (callback) {
            TypeCallback[chan][i] = 0;
            callback(chan, type);
        }
    }
}

static void GetTypeCallback(s32 chan, u32 error, OSContext* context) {
    u32 type;
    u32 chanBit;
    BOOL fix;
    u32 id;

    Type[chan] &= ~SI_ERROR_BUSY;
    Type[chan] |= error;
    TypeTime[chan] = __OSGetSystemTime();

    type = Type[chan];

    chanBit = SI_CHAN0_BIT >> chan;
    fix = (BOOL)(__PADFixBits & chanBit);
    __PADFixBits &= ~chanBit;

    if ((error & (SI_ERROR_UNDER_RUN | SI_ERROR_OVER_RUN | SI_ERROR_NO_RESPONSE |
                  SI_ERROR_COLLISION)) ||
        (type & SI_TYPE_MASK) != SI_TYPE_GC || !(type & SI_GC_WIRELESS) ||
        (type & SI_WIRELESS_IR)) {
        OSSetWirelessID(chan, 0);
        CallTypeAndStatusCallback(chan, Type[chan]);
        return;
    }

    id = (u32)(OSGetWirelessID(chan) << 8);

    if (fix && (id & SI_WIRELESS_FIX_ID)) {
        cmdFixDevice[chan] = (0x4E << 24) | (id & SI_WIRELESS_TYPE_ID) | SI_WIRELESS_FIX_ID;
        Type[chan] = SI_ERROR_BUSY;
        SITransfer(chan, &cmdFixDevice[chan], 3, &Type[chan], 3, GetTypeCallback, 0);
        return;
    }

    if (type & SI_WIRELESS_FIX_ID) {
        if ((id & SI_WIRELESS_TYPE_ID) != (type & SI_WIRELESS_TYPE_ID)) {
            if (!(id & SI_WIRELESS_FIX_ID)) {
                id = type & SI_WIRELESS_TYPE_ID;
                id |= SI_WIRELESS_FIX_ID;
                OSSetWirelessID(chan, (u16)((id >> 8) & 0xFFFF));
            }

            cmdFixDevice[chan] = (0x4E << 24) | id;
            Type[chan] = SI_ERROR_BUSY;
            SITransfer(chan, &cmdFixDevice[chan], 3, &Type[chan], 3, GetTypeCallback, 0);
            return;
        }
    } else if (type & SI_WIRELESS_RECEIVED) {
        id = type & SI_WIRELESS_TYPE_ID;
        id |= SI_WIRELESS_FIX_ID;

        OSSetWirelessID(chan, (u16)((id >> 8) & 0xFFFF));

        cmdFixDevice[chan] = (0x4E << 24) | id;
        Type[chan] = SI_ERROR_BUSY;
        SITransfer(chan, &cmdFixDevice[chan], 3, &Type[chan], 3, GetTypeCallback, 0);
        return;
    } else {
        OSSetWirelessID(chan, 0);
    }

    CallTypeAndStatusCallback(chan, Type[chan]);
}

u32 SIGetType(s32 chan) {
    static u32 cmdTypeAndStatus;
    BOOL enabled;
    u32 type;
    OSTime diff;

    enabled = OSDisableInterrupts();

    type = Type[chan];
    diff = __OSGetSystemTime() - TypeTime[chan];
    if (Si.poll & (0x80 >> chan)) {
        if (type != SI_ERROR_NO_RESPONSE) {
            TypeTime[chan] = __OSGetSystemTime();
            OSRestoreInterrupts(enabled);
            return type;
        }
        type = Type[chan] = SI_ERROR_BUSY;
    } else if (diff <= OSMillisecondsToTicks(50) && type != SI_ERROR_NO_RESPONSE) {
        OSRestoreInterrupts(enabled);
        return type;
    } else if (diff <= OSMillisecondsToTicks(75)) {
        Type[chan] = SI_ERROR_BUSY;
    } else {
        type = Type[chan] = SI_ERROR_BUSY;
    }
    TypeTime[chan] = __OSGetSystemTime();

    SITransfer(chan, &cmdTypeAndStatus, 1, &Type[chan], 3, GetTypeCallback,
               OSMicrosecondsToTicks(65));

    OSRestoreInterrupts(enabled);
    return type;
}

u32 SIGetTypeAsync(s32 chan, SITypeAndStatusCallback callback) {
    BOOL enabled;
    u32 type;

    enabled = OSDisableInterrupts();
    type = SIGetType(chan);
    if (Type[chan] & SI_ERROR_BUSY) {
        int i;

        for (i = 0; i < 4; ++i) {
            if (TypeCallback[chan][i] == callback) {
                break;
            }
            if (TypeCallback[chan][i] == 0) {
                TypeCallback[chan][i] = callback;
                break;
            }
        }
    } else {
        callback(chan, type);
    }
    OSRestoreInterrupts(enabled);
    return type;
}

static void dummy(void) {
    OSReport("No response");
    OSReport("N64 controller");
    OSReport("N64 microphone");
    OSReport("N64 keyboard");
    OSReport("N64 mouse");
    OSReport("GameBoy Advance");
    OSReport("Standard controller");
    OSReport("Wireless receiver");
    OSReport("WaveBird controller");
    OSReport("Keyboard");
    OSReport("Steering");
}
