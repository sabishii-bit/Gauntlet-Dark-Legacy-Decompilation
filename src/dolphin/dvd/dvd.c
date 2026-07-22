#include "types.h"
#include "dolphin/dvd.h"
#include "dolphin/os/OSContext.h"

typedef s64 OSTime;

int OSDisableInterrupts(void);
void OSRestoreInterrupts(int level);
void OSReport(const char* msg, ...);
void DCInvalidateRange(void* addr, u32 nBytes);
int memcmp(const void* s1, const void* s2, u32 n);
void* memcpy(void* dst, const void* src, u32 n);

typedef s16 __OSInterrupt;
typedef void (*__OSInterruptHandler)(__OSInterrupt interrupt, OSContext* context);
__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler);
u32 __OSUnmaskInterrupts(u32 global);

typedef struct OSThreadQueue {
    struct OSThread* head;
    struct OSThread* tail;
} OSThreadQueue;

void OSInitThreadQueue(OSThreadQueue* queue);
void OSSleepThread(OSThreadQueue* queue);
void OSWakeupThread(OSThreadQueue* queue);

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

u32 __OSBusClock : 0x800000F8;
#define OS_BUS_CLOCK __OSBusClock
#define OS_TIMER_CLOCK (OS_BUS_CLOCK / 4)
#define OSMillisecondsToTicks(msec) ((msec) * (OS_TIMER_CLOCK / 1000))

volatile u32 __DIRegs[] : 0xCC006000;

struct OSBootInfo_s {
    /* 0x00 */ DVDDiskID DVDDiskID;
    /* 0x20 */ u32 magic;
    /* 0x24 */ u32 version;
    /* 0x28 */ u32 memorySize;
    /* 0x2C */ u32 consoleType;
    /* 0x30 */ void* arenaLo;
    /* 0x34 */ void* arenaHi;
    /* 0x38 */ void* FSTLocation;
    /* 0x3C */ u32 FSTMaxLength;
};

#define OSPhysicalToCached(paddr) ((void*)((u32)(paddr) + 0x80000000))

extern OSThreadQueue __DVDThreadQueue;

void __DVDInterruptHandler(s16 interrupt, void* context);
void __DVDStoreErrorCode(u32 error);
void __DVDClearWaitingQueue(void);
BOOL __DVDPushWaitingQueue(s32 prio, DVDCommandBlock* block);
DVDCommandBlock* __DVDPopWaitingQueue(void);
BOOL __DVDCheckWaitingQueue(void);
BOOL __DVDDequeueWaitingQueue(DVDCommandBlock* block);
void __fstLoad(void);

BOOL DVDReadAbsAsyncPrio(DVDCommandBlock* block, void* addr, s32 length, s32 offset,
                         DVDCBCallback callback, s32 prio);
BOOL DVDReadAbsAsyncForBS(DVDCommandBlock* block, void* addr, s32 length, s32 offset,
                          DVDCBCallback callback);
BOOL DVDReadDiskID(DVDCommandBlock* block, DVDDiskID* diskID, DVDCBCallback callback);
void DVDReset(void);
s32 DVDGetCommandBlockStatus(DVDCommandBlock* block);
s32 DVDGetDriveStatus(void);
BOOL DVDSetAutoInvalidation(BOOL autoInval);
void DVDPause(void);
void DVDResume(void);
BOOL DVDCancelAsync(DVDCommandBlock* block, DVDCBCallback callback);
s32 DVDCancel(volatile DVDCommandBlock* block);
BOOL DVDCancelAllAsync(DVDCBCallback callback);
BOOL DVDCheckDisk(void);
void __DVDPrepareResetAsync(DVDCBCallback callback);

#define DVD_STATE_FATAL_ERROR -1
#define DVD_STATE_END 0
#define DVD_STATE_BUSY 1
#define DVD_STATE_WAITING 2
#define DVD_STATE_COVER_CLOSED 3
#define DVD_STATE_NO_DISK 4
#define DVD_STATE_COVER_OPEN 5
#define DVD_STATE_WRONG_DISK 6
#define DVD_STATE_MOTOR_STOPPED 7
#define DVD_STATE_PAUSING 8
#define DVD_STATE_IGNORED 9
#define DVD_STATE_CANCELED 10
#define DVD_STATE_RETRY 11

#define DVD_COMMAND_READ 1
#define DVD_COMMAND_SEEK 2
#define DVD_COMMAND_CHANGE_DISK 3
#define DVD_COMMAND_BSREAD 4
#define DVD_COMMAND_READID 5
#define DVD_COMMAND_INITSTREAM 6
#define DVD_COMMAND_CANCELSTREAM 7
#define DVD_COMMAND_STOP_STREAM_AT_END 8
#define DVD_COMMAND_REQUEST_AUDIO_ERROR 9
#define DVD_COMMAND_REQUEST_PLAY_ADDR 10
#define DVD_COMMAND_REQUEST_START_ADDR 11
#define DVD_COMMAND_REQUEST_LENGTH 12
#define DVD_COMMAND_AUDIO_BUFFER_CONFIG 13
#define DVD_COMMAND_INQUIRY 14
#define DVD_COMMAND_BS_CHANGE_DISK 15

static u8* tmpBuffer[32] ATTRIBUTE_ALIGN(32);
static DVDCommandBlock DummyCommandBlock;
static OSAlarm alarm;

static BOOL autoInvalidation = TRUE;

static DVDCommandBlock* executing;
static DVDDiskID* currID;
static struct OSBootInfo_s* bootInfo;
static volatile BOOL PauseFlag;
static volatile BOOL PausingFlag;
static BOOL AutoFinishing;
static BOOL FatalErrorFlag;
static volatile u32 CurrCommand;
static volatile u32 Canceling;
static DVDCBCallback CancelCallback;
static volatile u32 ResumeFromHere;
static volatile u32 CancelLastError;
static u32 LastError;
static volatile s32 NumInternalRetry;
static BOOL ResetRequired;
static BOOL FirstTimeInBootrom;
static BOOL DVDInitialized;
void (*LastState)(DVDCommandBlock*);

static void stateReadingFST();
static void cbForStateReadingFST(u32 intType);
static void cbForStateError(u32 intType);
static void stateTimeout();
static void stateGettingError();
static u32 CategorizeError(u32 error);
static void cbForStateGettingError(u32 intType);
static void cbForUnrecoveredError(u32 intType);
static void cbForUnrecoveredErrorRetry(u32 intType);
static void stateGoToRetry();
static void cbForStateGoToRetry(u32 intType);
static void stateCheckID();
static void stateCheckID3();
static void stateCheckID2();
static void cbForStateCheckID1(u32 intType);
static void cbForStateCheckID2(u32 intType);
static void cbForStateCheckID3(u32 intType);
static void stateCoverClosed();
static void stateCoverClosed_CMD();
static void cbForStateCoverClosed(u32 intType);
static void stateMotorStopped();
static void cbForStateMotorStopped(u32 intType);
static void stateReady();
static void stateBusy(DVDCommandBlock* block);
static void cbForStateBusy(u32 intType);
static int issueCommand(s32 prio, DVDCommandBlock* block);
static void cbForCancelSync();

void DVDInit(void) {
    if (DVDInitialized == 0) {
        OSInitAlarm();
        DVDInitialized = 1;
        __DVDFSInit();
        __DVDClearWaitingQueue();
        __DVDInitWA();
        bootInfo = (void*)OSPhysicalToCached(0);
        currID = &bootInfo->DVDDiskID;
        __OSSetInterruptHandler(0x15, (__OSInterruptHandler)__DVDInterruptHandler);
        __OSUnmaskInterrupts(0x400);
        OSInitThreadQueue(&__DVDThreadQueue);
        __DIRegs[0] = 0x2A;
        __DIRegs[1] = 0;
        if (bootInfo->magic == 0xE5207C22) {
            OSReport("app booted via JTAG\n");
            OSReport("load fst\n");
            __fstLoad();
        } else if (bootInfo->magic == 0x0D15EA5E) {
            OSReport("app booted from bootrom\n");
        } else {
            FirstTimeInBootrom = 1;
            OSReport("bootrom\n");
        }
    }
}

static void stateReadingFST() {
    LastState = stateReadingFST;
    DVDLowRead(bootInfo->FSTLocation, (u32)(tmpBuffer[2] + 0x1F) & 0xFFFFFFE0, (u32)tmpBuffer[1],
               cbForStateReadingFST);
}

static void cbForStateReadingFST(u32 intType) {
    DVDCommandBlock* finished;

    if (intType == 16) {
        executing->state = DVD_STATE_FATAL_ERROR;
        stateTimeout();
        return;
    }

    if (intType & 1) {
        NumInternalRetry = 0;
        finished = executing;
        executing = &DummyCommandBlock;
        finished->state = DVD_STATE_END;
        if (finished->callback) {
            finished->callback(0, finished);
        }
        stateReady();
    } else {
        stateGettingError();
    }
}

static void stateError(u32 code) {
    __DVDStoreErrorCode(code);
    DVDLowStopMotor(cbForStateError);
}

static void cbForStateError(u32 intType) {
    DVDCommandBlock* finished;

    if (intType == 16) {
        executing->state = DVD_STATE_FATAL_ERROR;
        stateTimeout();
        return;
    }

    finished = executing;
    FatalErrorFlag = TRUE;
    executing = &DummyCommandBlock;

    if (finished->callback != NULL) {
        finished->callback(-1, finished);
    }

    if (Canceling) {
        Canceling = FALSE;
        if (CancelCallback != NULL) {
            CancelCallback(0, finished);
        }
    }

    stateReady();
}

static void stateTimeout() {
    __DVDStoreErrorCode(0x1234568);
    DVDReset();
    cbForStateError(0);
}

static void stateGettingError() {
    DVDLowRequestError(cbForStateGettingError);
}

static u32 CategorizeError(u32 error) {
    if (error == 0x20400) {
        LastError = error;
        return 1;
    }
    error &= 0x00FFFFFF;
    if ((error == 0x62800) || (error == 0x23A00) || (error == 0xB5A01)) {
        return 0;
    }
    NumInternalRetry += 1;
    if (NumInternalRetry == 2) {
        if (error == LastError) {
            LastError = error;
            return 1;
        }
        LastError = error;
        return 2;
    }

    LastError = error;

    if (error == 0x31100 || executing->command == DVD_COMMAND_READID) {
        return 2;
    }
    return 3;
}

static BOOL cancel_inline(u32 resume) {
    if (Canceling) {
        DVDCommandBlock* finished = executing;
        ResumeFromHere = resume;
        Canceling = 0;
        executing = &DummyCommandBlock;
        finished->state = DVD_STATE_CANCELED;
        if (finished->callback) {
            finished->callback(-3, finished);
        }
        if (CancelCallback) {
            CancelCallback(0, finished);
        }
        stateReady();
        return TRUE;
    }
    return FALSE;
}

static void cbForStateGettingError(u32 intType) {
    u32 error;
    u32 error_hi;
    u32 resume_from;
    u32 errorCategory;

    if (intType == 0x10) {
        executing->state = DVD_STATE_FATAL_ERROR;
        stateTimeout();
        return;
    }

    if (intType & 2) {
        executing->state = DVD_STATE_FATAL_ERROR;
        stateError(0x1234567);
        return;
    }

    error = __DIRegs[8];
    error_hi = error & 0xFF000000;

    errorCategory = CategorizeError(error);

    if (errorCategory == 1) {
        executing->state = DVD_STATE_FATAL_ERROR;
        stateError(error);
        return;
    }

    if (errorCategory - 2 <= 1) {
        resume_from = 0;
    } else if (error_hi == 0x1000000) {
        resume_from = 4;
    } else if (error_hi == 0x2000000) {
        resume_from = 6;
    } else if (error_hi == 0x3000000) {
        resume_from = 3;
    } else {
        resume_from = 5;
    }

    if (cancel_inline(resume_from)) {
        return;
    }

    if (errorCategory == 2) {
        __DVDStoreErrorCode(error);
        stateGoToRetry();
        return;
    } else if (errorCategory == 3) {
        if ((error & 0xFFFFFF) == 0x31100) {
            DVDLowSeek(executing->offset, cbForUnrecoveredError);
        } else {
            LastState(executing);
        }
        return;
    } else {
        if (error_hi == 0x1000000) {
            executing->state = DVD_STATE_COVER_OPEN;
            stateMotorStopped();
            return;
        } else if (error_hi == 0x2000000) {
            executing->state = DVD_STATE_COVER_CLOSED;
            stateCoverClosed();
            return;
        } else if (error_hi == 0x3000000) {
            executing->state = DVD_STATE_NO_DISK;
            stateMotorStopped();
            return;
        }
        executing->state = DVD_STATE_FATAL_ERROR;
        stateError(0x1234567);
        return;
    }
}

static void cbForUnrecoveredError(u32 intType) {
    if (intType == 0x10) {
        executing->state = DVD_STATE_FATAL_ERROR;
        stateTimeout();
        return;
    }
    if (intType & 1) {
        stateGoToRetry();
        return;
    }
    DVDLowRequestError(cbForUnrecoveredErrorRetry);
}

static void cbForUnrecoveredErrorRetry(u32 intType) {
    if (intType == 0x10) {
        executing->state = DVD_STATE_FATAL_ERROR;
        stateTimeout();
        return;
    }
    executing->state = DVD_STATE_FATAL_ERROR;
    if (intType & 2) {
        stateError(0x1234567);
        return;
    }
    stateError(__DIRegs[8]);
}

static void stateGoToRetry() {
    DVDLowStopMotor(cbForStateGoToRetry);
}

static void cbForStateGoToRetry(u32 intType) {
    if (intType == 0x10) {
        executing->state = DVD_STATE_FATAL_ERROR;
        stateTimeout();
        return;
    }
    if (intType & 2) {
        executing->state = DVD_STATE_FATAL_ERROR;
        stateError(0x1234567);
        return;
    }
    NumInternalRetry = 0;
    if ((CurrCommand == DVD_COMMAND_BSREAD) || (CurrCommand == DVD_COMMAND_READID) ||
        (CurrCommand == DVD_COMMAND_AUDIO_BUFFER_CONFIG) ||
        (CurrCommand == DVD_COMMAND_BS_CHANGE_DISK)) {
        ResetRequired = 1;
    }

    if (!cancel_inline(2)) {
        executing->state = DVD_STATE_RETRY;
        stateMotorStopped();
    }
}

static void stateCheckID() {
    switch (CurrCommand) {
    case DVD_COMMAND_CHANGE_DISK:
        if (memcmp(&tmpBuffer, executing->id, 0x1C)) {
            DVDLowStopMotor(cbForStateCheckID1);
            return;
        }
        memcpy(currID, &tmpBuffer, 0x20);
        executing->state = DVD_STATE_BUSY;
        DCInvalidateRange(&tmpBuffer, 0x20);
        LastState = stateCheckID2;
        stateCheckID2(executing);
        break;
    default:
        if (memcmp(&tmpBuffer, currID, 0x20)) {
            DVDLowStopMotor(cbForStateCheckID1);
            return;
        }
        LastState = stateCheckID3;
        stateCheckID3(executing);
        return;
    }
}

static void stateCheckID3() {
    DVDLowAudioBufferConfig(currID->streaming, 10, cbForStateCheckID3);
}

static void stateCheckID2() {
    DVDLowRead(&tmpBuffer, 0x20, 0x420, cbForStateCheckID2);
}

static void cbForStateCheckID1(u32 intType) {
    if (intType == 0x10) {
        executing->state = DVD_STATE_FATAL_ERROR;
        stateTimeout();
        return;
    }
    if (intType & 2) {
        executing->state = DVD_STATE_FATAL_ERROR;
        stateError(0x1234567);
        return;
    }

    NumInternalRetry = 0;

    if (!cancel_inline(1)) {
        executing->state = DVD_STATE_WRONG_DISK;
        stateMotorStopped();
    }
}

static void cbForStateCheckID2(u32 intType) {
    if (intType == 0x10) {
        executing->state = DVD_STATE_FATAL_ERROR;
        stateTimeout();
        return;
    }

    if (intType & 1) {
        NumInternalRetry = 0;
        stateReadingFST();
    } else {
        stateGettingError();
    }
}

static void cbForStateCheckID3(u32 intType) {
    if (intType == 0x10) {
        executing->state = DVD_STATE_FATAL_ERROR;
        stateTimeout();
        return;
    }

    if (intType & 1) {
        NumInternalRetry = 0;
        if (!cancel_inline(0)) {
            executing->state = DVD_STATE_BUSY;
            stateBusy(executing);
        }
    } else {
        stateGettingError();
    }
}

static void AlarmHandler(OSAlarm* alm, OSContext* context) {
    DVDReset();
    DCInvalidateRange(tmpBuffer, 0x20);
    LastState = stateCoverClosed_CMD;
    stateCoverClosed_CMD(executing);
}

static void stateCoverClosed() {
    DVDCommandBlock* finished;

    switch (CurrCommand) {
    case DVD_COMMAND_BSREAD:
    case DVD_COMMAND_READID:
    case DVD_COMMAND_AUDIO_BUFFER_CONFIG:
    case DVD_COMMAND_BS_CHANGE_DISK:
        __DVDClearWaitingQueue();
        finished = executing;
        executing = &DummyCommandBlock;
        if (finished->callback) {
            finished->callback(-4, finished);
        }
        stateReady();
        return;
    }
    DVDReset();
    OSCreateAlarm(&alarm);
    OSSetAlarm(&alarm, OSMillisecondsToTicks(1150), AlarmHandler);
}

static void stateCoverClosed_CMD() {
    DVDLowReadDiskID((void*)&tmpBuffer, cbForStateCoverClosed);
}

static void cbForStateCoverClosed(u32 intType) {
    if (intType == 0x10) {
        executing->state = DVD_STATE_FATAL_ERROR;
        stateTimeout();
        return;
    }
    if (intType & 1) {
        NumInternalRetry = 0;
        stateCheckID();
    } else {
        stateGettingError();
    }
}

static void stateMotorStopped() {
    DVDLowWaitCoverClose(cbForStateMotorStopped);
}

static void cbForStateMotorStopped(u32 intType) {
    __DIRegs[1] = 0;
    executing->state = DVD_STATE_COVER_CLOSED;
    stateCoverClosed();
}

static void stateReady() {
    DVDCommandBlock* finished;

    if (!__DVDCheckWaitingQueue()) {
        executing = NULL;
        return;
    }

    if (PauseFlag) {
        PausingFlag = 1;
        executing = NULL;
        return;
    }

    executing = __DVDPopWaitingQueue();

    if (FatalErrorFlag) {
        executing->state = DVD_STATE_FATAL_ERROR;
        finished = executing;
        executing = &DummyCommandBlock;
        if (finished->callback != NULL) {
            finished->callback(-1, finished);
        }
        stateReady();
        return;
    }

    CurrCommand = executing->command;

    if (ResumeFromHere != 0) {
        switch (ResumeFromHere) {
        case 1:
            executing->state = DVD_STATE_WRONG_DISK;
            stateMotorStopped();
            break;
        case 2:
            executing->state = DVD_STATE_RETRY;
            stateMotorStopped();
            break;
        case 3:
            executing->state = DVD_STATE_NO_DISK;
            stateMotorStopped();
            break;
        case 7:
            executing->state = DVD_STATE_MOTOR_STOPPED;
            stateMotorStopped();
            break;
        case 4:
            executing->state = DVD_STATE_COVER_OPEN;
            stateMotorStopped();
            break;
        case 6:
            executing->state = DVD_STATE_COVER_CLOSED;
            stateCoverClosed();
            break;
        case 5:
            executing->state = DVD_STATE_FATAL_ERROR;
            stateError(CancelLastError);
            break;
        }
        ResumeFromHere = 0;
        return;
    }
    executing->state = DVD_STATE_BUSY;
    stateBusy(executing);
}

static void stateBusy(DVDCommandBlock* block) {
    LastState = stateBusy;
    switch (block->command) {
    case DVD_COMMAND_READID:
        __DIRegs[1] = __DIRegs[1];
        block->currTransferSize = 0x20;
        DVDLowReadDiskID(block->addr, cbForStateBusy);
        return;
    case DVD_COMMAND_READ:
    case DVD_COMMAND_BSREAD:
        __DIRegs[1] = __DIRegs[1];
        block->currTransferSize = (block->length - block->transferredSize > 0x80000)
                                      ? 0x80000
                                      : (block->length - block->transferredSize);
        DVDLowRead((char*)block->addr + block->transferredSize, block->currTransferSize,
                   block->offset + block->transferredSize, cbForStateBusy);
        return;
    case DVD_COMMAND_SEEK:
        __DIRegs[1] = __DIRegs[1];
        DVDLowSeek(block->offset, cbForStateBusy);
        return;
    case DVD_COMMAND_CHANGE_DISK:
        DVDLowStopMotor(cbForStateBusy);
        return;
    case DVD_COMMAND_BS_CHANGE_DISK:
        DVDLowStopMotor(cbForStateBusy);
        return;
    case DVD_COMMAND_INITSTREAM:
        __DIRegs[1] = __DIRegs[1];
        if (AutoFinishing) {
            executing->currTransferSize = 0;
            DVDLowRequestAudioStatus(0, cbForStateBusy);
            return;
        }
        executing->currTransferSize = 1;
        DVDLowAudioStream(0, block->length, block->offset, cbForStateBusy);
        return;
    case DVD_COMMAND_CANCELSTREAM:
        __DIRegs[1] = __DIRegs[1];
        DVDLowAudioStream(0x10000, 0, 0, cbForStateBusy);
        return;
    case DVD_COMMAND_STOP_STREAM_AT_END:
        __DIRegs[1] = __DIRegs[1];
        AutoFinishing = 1;
        DVDLowAudioStream(0, 0, 0, cbForStateBusy);
        return;
    case DVD_COMMAND_REQUEST_AUDIO_ERROR:
        __DIRegs[1] = __DIRegs[1];
        DVDLowRequestAudioStatus(0, cbForStateBusy);
        return;
    case DVD_COMMAND_REQUEST_PLAY_ADDR:
        __DIRegs[1] = __DIRegs[1];
        DVDLowRequestAudioStatus(0x10000, cbForStateBusy);
        return;
    case DVD_COMMAND_REQUEST_START_ADDR:
        __DIRegs[1] = __DIRegs[1];
        DVDLowRequestAudioStatus(0x20000, cbForStateBusy);
        return;
    case DVD_COMMAND_REQUEST_LENGTH:
        __DIRegs[1] = __DIRegs[1];
        DVDLowRequestAudioStatus(0x30000, cbForStateBusy);
        return;
    case DVD_COMMAND_AUDIO_BUFFER_CONFIG:
        __DIRegs[1] = __DIRegs[1];
        DVDLowAudioBufferConfig(block->offset, block->length, cbForStateBusy);
        return;
    case DVD_COMMAND_INQUIRY:
        __DIRegs[1] = __DIRegs[1];
        block->currTransferSize = 0x20;
        DVDLowInquiry(block->addr, cbForStateBusy);
        return;
    }
}

static void cbForStateBusy(u32 intType) {
    DVDCommandBlock* finished;
    s32 result;

    if (intType == 0x10) {
        executing->state = DVD_STATE_FATAL_ERROR;
        stateTimeout();
        return;
    }

    if ((CurrCommand == DVD_COMMAND_CHANGE_DISK) || (CurrCommand == DVD_COMMAND_BS_CHANGE_DISK)) {
        if (intType & 2) {
            executing->state = DVD_STATE_FATAL_ERROR;
            stateError(0x1234567);
            return;
        }
        NumInternalRetry = 0;
        if (CurrCommand == DVD_COMMAND_BS_CHANGE_DISK) {
            ResetRequired = 1;
        }
        if (cancel_inline(7)) {
            return;
        }
        executing->state = DVD_STATE_MOTOR_STOPPED;
        stateMotorStopped();
        return;
    }

    if ((CurrCommand == DVD_COMMAND_READ) || (CurrCommand == DVD_COMMAND_BSREAD) ||
        (CurrCommand == DVD_COMMAND_READID) || (CurrCommand == DVD_COMMAND_INQUIRY)) {
        executing->transferredSize += executing->currTransferSize - __DIRegs[6];
    }

    if (intType & 8) {
        Canceling = 0;
        finished = executing;
        executing = &DummyCommandBlock;
        finished->state = DVD_STATE_CANCELED;
        if (finished->callback) {
            finished->callback(-3, finished);
        }
        if (CancelCallback) {
            CancelCallback(0, finished);
        }
        stateReady();
        return;
    }

    if (intType & 1) {
        NumInternalRetry = 0;
        if (cancel_inline(0)) {
            return;
        }
        if (CurrCommand == DVD_COMMAND_READ || CurrCommand == DVD_COMMAND_BSREAD ||
            CurrCommand == DVD_COMMAND_READID || CurrCommand == DVD_COMMAND_INQUIRY) {
            if (executing->transferredSize != executing->length) {
                stateBusy(executing);
                return;
            }
            finished = executing;
            executing = &DummyCommandBlock;
            finished->state = DVD_STATE_END;
            if (finished->callback) {
                finished->callback(finished->transferredSize, finished);
            }
            stateReady();
            return;
        } else if (CurrCommand == DVD_COMMAND_REQUEST_AUDIO_ERROR ||
                   CurrCommand == DVD_COMMAND_REQUEST_PLAY_ADDR ||
                   CurrCommand == DVD_COMMAND_REQUEST_START_ADDR ||
                   CurrCommand == DVD_COMMAND_REQUEST_LENGTH) {
            if (CurrCommand == DVD_COMMAND_REQUEST_START_ADDR ||
                CurrCommand == DVD_COMMAND_REQUEST_PLAY_ADDR) {
                result = __DIRegs[8] * 4;
            } else {
                result = __DIRegs[8];
            }

            finished = executing;
            executing = &DummyCommandBlock;
            finished->state = DVD_STATE_END;
            if (finished->callback) {
                finished->callback(result, finished);
            }
            stateReady();
            return;
        } else if (CurrCommand == DVD_COMMAND_INITSTREAM) {
            if (executing->currTransferSize == 0) {
                if (__DIRegs[8] & 1) {
                    finished = executing;
                    executing = &DummyCommandBlock;
                    finished->state = DVD_STATE_IGNORED;
                    if (finished->callback) {
                        finished->callback(-2, finished);
                    }
                    stateReady();
                    return;
                }
                AutoFinishing = 0;
                executing->currTransferSize = 1;
                DVDLowAudioStream(0, executing->length, executing->offset, cbForStateBusy);
                return;
            }
            finished = executing;
            executing = &DummyCommandBlock;
            finished->state = DVD_STATE_END;
            if (finished->callback) {
                finished->callback(0, finished);
            }
            stateReady();
            return;
        } else {
            finished = executing;
            executing = &DummyCommandBlock;
            finished->state = DVD_STATE_END;
            if (finished->callback) {
                finished->callback(0, finished);
            }
            stateReady();
            return;
        }
    } else if (CurrCommand == DVD_COMMAND_INQUIRY) {
        executing->state = DVD_STATE_FATAL_ERROR;
        stateError(0x1234567);
        return;
    } else {
        if ((CurrCommand == DVD_COMMAND_READ || CurrCommand == DVD_COMMAND_BSREAD ||
             CurrCommand == DVD_COMMAND_READID || CurrCommand == DVD_COMMAND_INQUIRY) &&
            executing->transferredSize == executing->length) {
            if (!cancel_inline(0)) {
                finished = executing;
                executing = &DummyCommandBlock;
                finished->state = DVD_STATE_END;
                if (finished->callback != NULL) {
                    finished->callback(finished->transferredSize, finished);
                }
                stateReady();
            }
            return;
        }
        stateGettingError();
    }
}

static int issueCommand(s32 prio, DVDCommandBlock* block) {
    int level;
    int result;

    if (autoInvalidation &&
        (block->command == DVD_COMMAND_READ || block->command == DVD_COMMAND_BSREAD ||
         block->command == DVD_COMMAND_READID || block->command == DVD_COMMAND_INQUIRY)) {
        DCInvalidateRange(block->addr, block->length);
    }

    level = OSDisableInterrupts();

    block->state = DVD_STATE_WAITING;
    result = __DVDPushWaitingQueue(prio, block);

    if (executing == NULL && !PauseFlag) {
        stateReady();
    }

    OSRestoreInterrupts(level);
    return result;
}

BOOL DVDReadAbsAsyncPrio(DVDCommandBlock* block, void* addr, s32 length, s32 offset,
                         DVDCBCallback callback, s32 prio) {
    int idle;

    block->command = DVD_COMMAND_READ;
    block->addr = addr;
    block->length = length;
    block->offset = offset;
    block->transferredSize = 0;
    block->callback = callback;
    idle = issueCommand(prio, block);
    return idle;
}

BOOL DVDSeekAbsAsyncPrio(DVDCommandBlock* block, s32 offset, DVDCBCallback callback, s32 prio) {
    int idle;

    block->command = DVD_COMMAND_SEEK;
    block->offset = offset;
    block->callback = callback;
    idle = issueCommand(prio, block);
    return idle;
}

BOOL DVDReadAbsAsyncForBS(DVDCommandBlock* block, void* addr, s32 length, s32 offset,
                          DVDCBCallback callback) {
    int idle;

    block->command = DVD_COMMAND_BSREAD;
    block->addr = addr;
    block->length = length;
    block->offset = offset;
    block->transferredSize = 0;
    block->callback = callback;
    idle = issueCommand(2, block);
    return idle;
}

BOOL DVDReadDiskID(DVDCommandBlock* block, DVDDiskID* diskID, DVDCBCallback callback) {
    int idle;

    block->command = DVD_COMMAND_READID;
    block->addr = diskID;
    block->length = 0x20;
    block->offset = 0;
    block->transferredSize = 0;
    block->callback = callback;
    idle = issueCommand(2, block);
    return idle;
}

void DVDReset(void) {
    DVDLowReset();
    __DIRegs[0] = 0x2A;
    __DIRegs[1] = __DIRegs[1];
    ResetRequired = 0;
    ResumeFromHere = 0;
}

BOOL DVDResetRequired(void) {
    return ResetRequired;
}

s32 DVDGetCommandBlockStatus(DVDCommandBlock* block) {
    BOOL enabled;
    s32 result;

    enabled = OSDisableInterrupts();
    if (block->state == DVD_STATE_COVER_CLOSED) {
        result = DVD_STATE_BUSY;
    } else {
        result = block->state;
    }
    OSRestoreInterrupts(enabled);
    return result;
}

s32 DVDGetDriveStatus(void) {
    DVDCommandBlock* block;
    s32 result;

    BOOL enabled = OSDisableInterrupts();

    if (FatalErrorFlag) {
        result = DVD_STATE_FATAL_ERROR;
    } else if (PausingFlag) {
        result = DVD_STATE_PAUSING;
    } else {
        block = executing;
        if (block == NULL) {
            result = DVD_STATE_END;
        } else if (block == &DummyCommandBlock) {
            result = DVD_STATE_END;
        } else {
            result = DVDGetCommandBlockStatus(block);
        }
    }

    OSRestoreInterrupts(enabled);
    return result;
}

BOOL DVDSetAutoInvalidation(BOOL autoInval) {
    BOOL prev;

    prev = autoInvalidation;
    autoInvalidation = autoInval;
    return prev;
}

void DVDPause(void) {
    int level;

    level = OSDisableInterrupts();
    PauseFlag = 1;
    if (executing == NULL) {
        PausingFlag = 1;
    }
    OSRestoreInterrupts(level);
}

void DVDResume(void) {
    int level;

    level = OSDisableInterrupts();
    PauseFlag = 0;
    if (PausingFlag) {
        PausingFlag = 0;
        stateReady();
    }
    OSRestoreInterrupts(level);
}

BOOL DVDCancelAsync(DVDCommandBlock* block, DVDCBCallback callback) {
    BOOL enabled;
    DVDLowCallback old;

    enabled = OSDisableInterrupts();

    switch (block->state) {
    case DVD_STATE_FATAL_ERROR:
    case DVD_STATE_END:
    case DVD_STATE_CANCELED:
        if (callback) {
            callback(0, block);
        }
        break;
    case DVD_STATE_BUSY:
        if (Canceling) {
            OSRestoreInterrupts(enabled);
            return FALSE;
        }
        Canceling = 1;
        CancelCallback = callback;
        if (block->command == DVD_COMMAND_BSREAD || block->command == DVD_COMMAND_READ) {
            DVDLowBreak();
        }
        break;
    case DVD_STATE_WAITING:
        __DVDDequeueWaitingQueue(block);
        block->state = DVD_STATE_CANCELED;
        if (block->callback) {
            block->callback(-3, block);
        }
        if (callback) {
            callback(0, block);
        }
        break;
    case DVD_STATE_COVER_CLOSED:
        switch (block->command) {
        case DVD_COMMAND_BSREAD:
        case DVD_COMMAND_READID:
        case DVD_COMMAND_AUDIO_BUFFER_CONFIG:
        case DVD_COMMAND_BS_CHANGE_DISK:
            if (callback) {
                callback(0, block);
            }
            break;
        default:
            if (Canceling) {
                OSRestoreInterrupts(enabled);
                return FALSE;
            }
            Canceling = 1;
            CancelCallback = callback;
            break;
        }
        break;
    case DVD_STATE_NO_DISK:
    case DVD_STATE_COVER_OPEN:
    case DVD_STATE_WRONG_DISK:
    case DVD_STATE_MOTOR_STOPPED:
    case DVD_STATE_RETRY:
        old = DVDLowClearCallback();
        if (old != cbForStateMotorStopped) {
            OSRestoreInterrupts(enabled);
            return FALSE;
        }

        if (block->state == DVD_STATE_NO_DISK) {
            ResumeFromHere = 3;
        }
        if (block->state == DVD_STATE_COVER_OPEN) {
            ResumeFromHere = 4;
        }
        if (block->state == DVD_STATE_WRONG_DISK) {
            ResumeFromHere = 1;
        }
        if (block->state == DVD_STATE_RETRY) {
            ResumeFromHere = 2;
        }
        if (block->state == DVD_STATE_MOTOR_STOPPED) {
            ResumeFromHere = 7;
        }

        block->state = DVD_STATE_CANCELED;
        if (block->callback) {
            block->callback(-3, block);
        }
        if (callback) {
            callback(0, block);
        }
        stateReady();
        break;
    }

    OSRestoreInterrupts(enabled);
    return TRUE;
}

s32 DVDCancel(volatile DVDCommandBlock* block) {
    BOOL result;
    s32 state;
    u32 command;
    BOOL enabled;

    result = DVDCancelAsync((DVDCommandBlock*)block, cbForCancelSync);
    if (result == FALSE) {
        return -1;
    }

    enabled = OSDisableInterrupts();

    while (1) {
        state = block->state;
        if (state == DVD_STATE_END || state == DVD_STATE_FATAL_ERROR ||
            state == DVD_STATE_CANCELED) {
            break;
        }
        if (state == DVD_STATE_COVER_CLOSED) {
            command = block->command;
            if ((command == DVD_COMMAND_BSREAD) || (command == DVD_COMMAND_READID) ||
                (command == DVD_COMMAND_AUDIO_BUFFER_CONFIG) ||
                (command == DVD_COMMAND_BS_CHANGE_DISK)) {
                break;
            }
        }
        OSSleepThread(&__DVDThreadQueue);
    }

    OSRestoreInterrupts(enabled);
    return 0;
}

static void cbForCancelSync() {
    OSWakeupThread(&__DVDThreadQueue);
}

BOOL DVDCancelAllAsync(DVDCBCallback callback) {
    BOOL enabled;
    DVDCommandBlock* p;
    BOOL retVal;

    enabled = OSDisableInterrupts();
    DVDPause();
    while ((p = __DVDPopWaitingQueue())) {
        DVDCancelAsync(p, NULL);
    }
    if (executing) {
        retVal = DVDCancelAsync(executing, callback);
    } else {
        retVal = TRUE;
        if (callback) {
            callback(0, NULL);
        }
    }
    DVDResume();
    OSRestoreInterrupts(enabled);
    return retVal;
}

DVDDiskID* DVDGetCurrentDiskID(void) {
    return (DVDDiskID*)OSPhysicalToCached(0);
}

BOOL DVDCheckDisk(void) {
    BOOL enabled;
    BOOL result;
    s32 state;
    u32 coverReg;

    enabled = OSDisableInterrupts();

    if (FatalErrorFlag) {
        state = DVD_STATE_FATAL_ERROR;
    } else if (PausingFlag) {
        state = DVD_STATE_PAUSING;
    } else if (executing == NULL) {
        state = DVD_STATE_END;
    } else if (executing == &DummyCommandBlock) {
        state = DVD_STATE_END;
    } else {
        state = executing->state;
    }

    switch (state) {
    case 1:
    case 2:
    case 9:
    case 10:
        result = TRUE;
        break;
    case -1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 11:
        result = FALSE;
        break;
    case 0:
    case 8:
        coverReg = __DIRegs[1];
        if (((coverReg >> 2) & 1) || (coverReg & 1)) {
            result = FALSE;
        } else {
            result = TRUE;
        }
        break;
    }

    OSRestoreInterrupts(enabled);
    return result;
}

void __DVDPrepareResetAsync(DVDCBCallback callback) {
    BOOL enabled = OSDisableInterrupts();

    __DVDClearWaitingQueue();

    if (Canceling) {
        CancelCallback = callback;
    } else {
        if (executing != NULL) {
            executing->callback = NULL;
        }
        DVDCancelAllAsync(callback);
    }

    OSRestoreInterrupts(enabled);
}
