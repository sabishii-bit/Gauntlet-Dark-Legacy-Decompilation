#include "types.h"
#include "dolphin/dvd.h"
#include "dolphin/os/OSContext.h"

typedef s64 OSTime;

int OSDisableInterrupts(void);
void OSRestoreInterrupts(int level);
OSTime __OSGetSystemTime(void);
void OSClearContext(OSContext* context);
void OSSetCurrentContext(OSContext* context);
u32 __OSMaskInterrupts(u32 global);

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

void OSInitAlarm(void);
void OSCreateAlarm(OSAlarm* alarm);
void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler);
void OSCancelAlarm(OSAlarm* alarm);

u32 __OSBusClock : 0x800000F8;
#define OS_BUS_CLOCK __OSBusClock
#define OS_TIMER_CLOCK (OS_BUS_CLOCK / 4)
#define OSMicrosecondsToTicks(usec) (((OS_TIMER_CLOCK / 125000) * (usec)) / 8)
#define OSMillisecondsToTicks(msec) ((msec) * (OS_TIMER_CLOCK / 1000))
#define OSSecondsToTicks(sec) ((sec)*OS_TIMER_CLOCK)

volatile u32 __DIRegs[] : 0xCC006000;
volatile u32 __PIRegs[] : 0xCC003000;

typedef struct DVDCommand {
    /* 0x00 */ s32 cmd;
    /* 0x04 */ void* addr;
    /* 0x08 */ u32 length;
    /* 0x0C */ u32 offset;
    /* 0x10 */ DVDLowCallback callback;
} DVDCommand;

static DVDCommand CommandList[3];
static OSAlarm AlarmForWA;
static OSAlarm AlarmForTimeout;
static OSAlarm AlarmForBreak;

// Smaller version of DVDBB2
static struct {
    /* 0x00 */ void* bootFilePosition;
    /* 0x04 */ u32 FSTPosition;
    /* 0x08 */ u32 FSTLength;
} Prev, Curr;

static volatile BOOL StopAtNextInt;
static int LastLength;
static DVDLowCallback Callback;
static DVDLowCallback ResetCoverCallback;
static volatile OSTime LastResetEnd;
static volatile u32 ResetOccurred;
static volatile BOOL WaitingCoverClose;
static volatile BOOL Breaking;
static volatile u32 WorkAroundType;
static u32 WorkAroundSeekLocation;
static volatile OSTime LastReadFinished;
static volatile OSTime LastReadIssued;
static volatile BOOL LastCommandWasRead;
static volatile s32 NextCommandNumber;

static BOOL FirstRead = TRUE;

static void Read(void* addr, u32 length, u32 offset, DVDLowCallback callback);

void __DVDInitWA(void) {
    NextCommandNumber = 0;
    CommandList[0].cmd = -1;
    __DVDLowSetWAType(0, 0);
    OSInitAlarm();
}

static BOOL ProcessNextCommand(void) {
    s32 n = NextCommandNumber;

    if (CommandList[n].cmd == 1) {
        ++NextCommandNumber;
        Read(CommandList[n].addr, CommandList[n].length, CommandList[n].offset,
             CommandList[n].callback);
        return TRUE;
    } else if (CommandList[n].cmd == 2) {
        ++NextCommandNumber;
        DVDLowSeek(CommandList[n].offset, CommandList[n].callback);
        return TRUE;
    }

    return FALSE;
}

void __DVDInterruptHandler(s16 unused, void* context) {
    OSContext exceptionContext;
    u32 cause;
    u32 reg;
    u32 intr;
    u32 mask;

    cause = 0;

    OSCancelAlarm(&AlarmForTimeout);

    if (LastCommandWasRead) {
        LastReadFinished = __OSGetSystemTime();
        FirstRead = FALSE;
        Prev.bootFilePosition = Curr.bootFilePosition;
        Prev.FSTPosition = Curr.FSTPosition;
        Prev.FSTLength = Curr.FSTLength;
        if (StopAtNextInt == TRUE) {
            cause |= 8;
        }
    }
    LastCommandWasRead = FALSE;
    StopAtNextInt = FALSE;

    reg = __DIRegs[0];
    mask = reg & 0x2A;
    intr = (reg & 0x54) & (mask << 1);
    if (intr & 0x40) {
        cause |= 8;
    }
    if (intr & 0x10) {
        cause |= 1;
    }
    if (intr & 4) {
        cause |= 2;
    }
    if (cause) {
        ResetOccurred = FALSE;
    }
    __DIRegs[0] = intr | mask;

    if (ResetOccurred && (__OSGetSystemTime() - LastResetEnd) < OSMillisecondsToTicks(200)) {
        reg = __DIRegs[1];
        mask = reg & 2;
        intr = (reg & 4) & (mask << 1);
        if (intr & 4) {
            if (ResetCoverCallback) {
                ResetCoverCallback(4);
            }
            ResetCoverCallback = NULL;
        }
        __DIRegs[1] = __DIRegs[1];
    } else if (WaitingCoverClose) {
        reg = __DIRegs[1];
        mask = reg & 2;
        intr = (reg & 4) & (mask << 1);
        if (intr & 4) {
            cause |= 4;
        }
        __DIRegs[1] = intr | mask;
        WaitingCoverClose = FALSE;
    } else {
        __DIRegs[1] = 0;
    }

    if ((cause & 8) && !Breaking) {
        cause &= ~8;
    }

    if (cause & 1) {
        if (ProcessNextCommand()) {
            return;
        }
    } else {
        CommandList[0].cmd = -1;
        NextCommandNumber = 0;
    }

    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);
    if (cause != 0) {
        DVDLowCallback callback = Callback;
        Callback = 0;
        if (callback) {
            callback(cause);
        }
        Breaking = FALSE;
    }
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);
}

static void AlarmHandler(OSAlarm* alarm, OSContext* context) {
    ProcessNextCommand();
}

static void AlarmHandlerForTimeout(OSAlarm* alarm, OSContext* context) {
    OSContext exceptionContext;
    DVDLowCallback callback;

    __OSMaskInterrupts(0x400);
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);
    callback = Callback;
    Callback = NULL;
    if (callback != NULL) {
        callback(0x10);
    }
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);
}

static void SetTimeoutAlarm(OSTime timeout) {
    OSCreateAlarm(&AlarmForTimeout);
    OSSetAlarm(&AlarmForTimeout, timeout, AlarmHandlerForTimeout);
}

static void Read(void* addr, u32 length, u32 offset, DVDLowCallback callback) {
    StopAtNextInt = FALSE;
    LastCommandWasRead = TRUE;
    Callback = callback;
    LastReadIssued = __OSGetSystemTime();

    __DIRegs[2] = 0xA8000000;
    __DIRegs[3] = offset / 4;
    __DIRegs[4] = length;
    __DIRegs[5] = (u32)addr;
    __DIRegs[6] = length;
    LastLength = length;
    __DIRegs[7] = 3;

    if (length > 0xA00000) {
        SetTimeoutAlarm(OSSecondsToTicks(20));
    } else {
        SetTimeoutAlarm(OSSecondsToTicks(10));
    }
}

static BOOL HitCache(void) {
    u32 prev = (Prev.FSTLength + Prev.FSTPosition - 1) >> 15;
    u32 curr = Curr.FSTLength >> 15;
    s32 n = (DVDGetCurrentDiskID()->streaming ? TRUE : FALSE) ? 5 : 15;

    if (curr > prev - 2 || curr < prev + n + 3) {
        return TRUE;
    }
    return FALSE;
}

static void DoJustRead(void* addr, u32 length, u32 offset, DVDLowCallback callback) {
    CommandList[0].cmd = -1;
    NextCommandNumber = 0;
    Read(addr, length, offset, callback);
}

static void SeekTwiceBeforeRead(void* addr, u32 length, u32 offset, DVDLowCallback callback) {
    u32 newOffset = offset & ~0x7FFF;

    if (!newOffset) {
        newOffset = 0;
    } else {
        newOffset += WorkAroundSeekLocation;
    }

    CommandList[0].cmd = 2;
    CommandList[0].offset = newOffset;
    CommandList[0].callback = callback;
    CommandList[1].cmd = 1;
    CommandList[1].addr = addr;
    CommandList[1].length = length;
    CommandList[1].offset = offset;
    CommandList[1].callback = callback;
    CommandList[2].cmd = -1;
    NextCommandNumber = 0;
    DVDLowSeek(newOffset, callback);
}

static void WaitBeforeRead(void* addr, u32 length, u32 offset, DVDLowCallback callback,
                           OSTime timeout) {
    CommandList[0].cmd = 1;
    CommandList[0].addr = addr;
    CommandList[0].length = length;
    CommandList[0].offset = offset;
    CommandList[0].callback = callback;
    CommandList[1].cmd = -1;
    NextCommandNumber = 0;
    OSCreateAlarm(&AlarmForWA);
    OSSetAlarm(&AlarmForWA, timeout, AlarmHandler);
}

BOOL DVDLowRead(void* addr, u32 length, u32 offset, DVDLowCallback callback) {
    OSTime diff;
    u32 prev;

    __DIRegs[6] = length;
    Curr.bootFilePosition = addr;
    Curr.FSTPosition = length;
    Curr.FSTLength = offset;

    if (WorkAroundType == 0) {
        DoJustRead(addr, length, offset, callback);
    } else if (WorkAroundType == 1) {
        if (FirstRead) {
            SeekTwiceBeforeRead(addr, length, offset, callback);
        } else if (!HitCache()) {
            DoJustRead(addr, length, offset, callback);
        } else {
            prev = (Prev.FSTLength + Prev.FSTPosition - 1) >> 15;
            if (prev == Curr.FSTLength >> 15 || prev + 1 == Curr.FSTLength >> 15) {
                diff = __OSGetSystemTime() - LastReadFinished;
                if (OSMillisecondsToTicks(5) < diff) {
                    DoJustRead(addr, length, offset, callback);
                } else {
                    WaitBeforeRead(addr, length, offset, callback,
                                   OSMillisecondsToTicks(5) - diff + OSMicrosecondsToTicks(500));
                }
            } else {
                SeekTwiceBeforeRead(addr, length, offset, callback);
            }
        }
    }
    return TRUE;
}

BOOL DVDLowSeek(u32 offset, DVDLowCallback callback) {
    StopAtNextInt = FALSE;
    Callback = callback;
    __DIRegs[2] = 0xAB000000;
    __DIRegs[3] = offset >> 2;
    __DIRegs[7] = 1;
    SetTimeoutAlarm(OSSecondsToTicks(10));
    return TRUE;
}

BOOL DVDLowWaitCoverClose(DVDLowCallback callback) {
    Callback = callback;
    WaitingCoverClose = TRUE;
    StopAtNextInt = FALSE;
    __DIRegs[1] = 2;
    return TRUE;
}

BOOL DVDLowReadDiskID(DVDDiskID* diskID, DVDLowCallback callback) {
    StopAtNextInt = FALSE;
    Callback = callback;
    __DIRegs[2] = 0xA8000040;
    __DIRegs[3] = 0;
    __DIRegs[4] = sizeof(DVDDiskID);
    __DIRegs[5] = (u32)diskID;
    __DIRegs[6] = sizeof(DVDDiskID);
    __DIRegs[7] = 3;
    SetTimeoutAlarm(OSSecondsToTicks(10));
    return TRUE;
}

BOOL DVDLowStopMotor(DVDLowCallback callback) {
    StopAtNextInt = FALSE;
    Callback = callback;
    __DIRegs[2] = 0xE3000000;
    __DIRegs[7] = 1;
    SetTimeoutAlarm(OSSecondsToTicks(10));
    return TRUE;
}

BOOL DVDLowRequestError(DVDLowCallback callback) {
    StopAtNextInt = FALSE;
    Callback = callback;
    __DIRegs[2] = 0xE0000000;
    __DIRegs[7] = 1;
    SetTimeoutAlarm(OSSecondsToTicks(10));
    return TRUE;
}

BOOL DVDLowInquiry(DVDDriveInfo* info, DVDLowCallback callback) {
    StopAtNextInt = FALSE;
    Callback = callback;
    __DIRegs[2] = 0x12000000;
    __DIRegs[4] = sizeof(DVDDriveInfo);
    __DIRegs[5] = (u32)info;
    __DIRegs[6] = sizeof(DVDDriveInfo);
    __DIRegs[7] = 3;
    SetTimeoutAlarm(OSSecondsToTicks(10));
    return TRUE;
}

BOOL DVDLowAudioStream(u32 subcmd, u32 length, u32 offset, DVDLowCallback callback) {
    StopAtNextInt = FALSE;
    Callback = callback;
    __DIRegs[2] = subcmd | 0xE1000000;
    __DIRegs[3] = offset >> 2;
    __DIRegs[4] = length;
    __DIRegs[7] = 1;
    SetTimeoutAlarm(OSSecondsToTicks(10));
    return TRUE;
}

BOOL DVDLowRequestAudioStatus(u32 subcmd, DVDLowCallback callback) {
    StopAtNextInt = FALSE;
    Callback = callback;
    __DIRegs[2] = subcmd | 0xE2000000;
    __DIRegs[7] = 1;
    SetTimeoutAlarm(OSSecondsToTicks(10));
    return TRUE;
}

BOOL DVDLowAudioBufferConfig(BOOL enable, u32 size, DVDLowCallback callback) {
    StopAtNextInt = FALSE;
    Callback = callback;
    __DIRegs[2] = (enable ? 0x10000 : 0) | 0xE4000000 | size;
    __DIRegs[7] = 1;
    SetTimeoutAlarm(OSSecondsToTicks(10));
    return TRUE;
}

void DVDLowReset(void) {
    u32 reg;
    OSTime resetStart;

    __DIRegs[1] = 2;
    reg = __PIRegs[9];
    __PIRegs[9] = (reg & ~4) | 1;

    resetStart = __OSGetSystemTime();
    while (__OSGetSystemTime() - resetStart < OSMicrosecondsToTicks(12)) {
        ;
    }

    __PIRegs[9] = reg | 4 | 1;
    ResetOccurred = TRUE;
    LastResetEnd = __OSGetSystemTime();
}

DVDLowCallback DVDLowSetResetCoverCallback(DVDLowCallback callback) {
    DVDLowCallback old;
    BOOL enabled;

    enabled = OSDisableInterrupts();
    old = ResetCoverCallback;
    ResetCoverCallback = callback;
    OSRestoreInterrupts(enabled);
    return old;
}

BOOL DVDLowBreak(void) {
    StopAtNextInt = TRUE;
    Breaking = TRUE;
    return TRUE;
}

DVDLowCallback DVDLowClearCallback(void) {
    DVDLowCallback old;

    __DIRegs[1] = 0;
    old = Callback;
    Callback = NULL;
    return old;
}

u32 DVDLowGetCoverStatus(void) {
    if (__OSGetSystemTime() - LastResetEnd < OSMillisecondsToTicks(100)) {
        return 0;
    }
    if (__DIRegs[1] & 1) {
        return 1;
    }
    return 2;
}

void __DVDLowSetWAType(u32 type, u32 seekLoc) {
    BOOL enabled = OSDisableInterrupts();

    WorkAroundType = type;
    WorkAroundSeekLocation = seekLoc;
    OSRestoreInterrupts(enabled);
}
