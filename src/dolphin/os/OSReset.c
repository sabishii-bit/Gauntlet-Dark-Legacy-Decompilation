#include "types.h"
#include "dolphin/os/OSThread.h"

typedef struct OSResetFunctionInfo OSResetFunctionInfo;

struct OSResetFunctionInfo {
    /* 0x0 */ int (*func)(int final);
    /* 0x4 */ u32 priority;
    /* 0x8 */ OSResetFunctionInfo* next;
    /* 0xC */ OSResetFunctionInfo* prev;
};

struct OSResetFunctionQueue {
    OSResetFunctionInfo* head;
    OSResetFunctionInfo* tail;
};

typedef struct OSSram {
    /* 0x00 */ u16 checkSum;
    /* 0x02 */ u16 checkSumInv;
    /* 0x04 */ u32 ead0;
    /* 0x08 */ u32 ead1;
    /* 0x0C */ u32 counterBias;
    /* 0x10 */ s8 displayOffsetH;
    /* 0x11 */ u8 ntd;
    /* 0x12 */ u8 language;
    /* 0x13 */ u8 flags;
} OSSram;

int OSDisableInterrupts(void);
void __OSStopAudioSystem(void);
int __PADDisableRecalibration(int disable);
int __OSSyncSram(void);
OSSram* __OSLockSram(void);
int __OSUnlockSram(int commit);
void LCDisable(void);
void ICFlashInvalidate(void);
void OSCancelThread(OSThread* thread);
void __OSReboot(u32 resetCode, u32 bootDol);
void* memset(void* dst, int val, u32 n);

OSThreadQueue __OSActiveThreadQueue : 0x800000DC;
volatile u16 __VIRegs[] : 0xCC002000;
volatile u32 __PIRegs[] : 0xCC003000;
u8 OS_REBOOT_BOOL : 0x800030E2;

#define ENQUEUE_INFO(info, queue)                          \
    do {                                                   \
        struct OSResetFunctionInfo* __prev = (queue)->tail; \
        if (__prev == 0) {                                 \
            (queue)->head = (info);                        \
        } else {                                           \
            __prev->next = (info);                         \
        }                                                  \
        (info)->prev = __prev;                             \
        (info)->next = 0;                                  \
        (queue)->tail = (info);                            \
    } while (0);

#define ENQUEUE_INFO_PRIO(info, queue)               \
    do {                                             \
        struct OSResetFunctionInfo* __prev;          \
        struct OSResetFunctionInfo* __next;          \
        for (__next = (queue)->head;                 \
             __next && (__next->priority <= (info)->priority); \
             __next = __next->next)                  \
            ;                                        \
                                                     \
        if (__next == 0) {                           \
            ENQUEUE_INFO(info, queue);               \
        } else {                                     \
            (info)->next = __next;                   \
            __prev = __next->prev;                   \
            __next->prev = (info);                   \
            (info)->prev = __prev;                   \
            if (__prev == 0) {                       \
                (queue)->head = (info);              \
            } else {                                 \
                __prev->next = (info);               \
            }                                        \
        }                                            \
    } while (0);

static struct OSResetFunctionQueue ResetFunctionQueue;

static int CallResetFunctions(int final);
static void Reset(unsigned long resetCode);

void OSRegisterResetFunction(struct OSResetFunctionInfo* info) {
    ENQUEUE_INFO_PRIO(info, &ResetFunctionQueue);
}

static int CallResetFunctions(int final) {
    struct OSResetFunctionInfo* info;
    int err = 0;

    for (info = ResetFunctionQueue.head; info; info = info->next) {
        err |= !info->func(final);
    }
    err |= !__OSSyncSram();
    if (err) {
        return 0;
    }
    return 1;
}

/* clang-format off */
ASM static void Reset(unsigned long resetCode) {
#ifdef __MWERKS__
    nofralloc

    b L_000001BC
L_000001A0:
    mfspr r8, HID0
    ori r8, r8, 0x8
    mtspr HID0, r8
    isync
    sync
    nop
    b L_000001C0
L_000001BC:
    b L_000001DC
L_000001C0:
    mftb r5, 268
L_000001C4:
    mftb r6, 268
    subf r7, r5, r6
    cmplwi r7, 0x1124
    blt L_000001C4
    nop
    b L_000001E0
L_000001DC:
    b L_000001FC
L_000001E0:
    lis r8, 0xcc00
    ori r8, r8, 0x3000
    li r4, 0x3
    stw r4, 0x24(r8)
    stw r3, 0x24(r8)
    nop
    b L_00000200
L_000001FC:
    b L_00000208
L_00000200:
    nop
    b L_00000200
L_00000208:
    b L_000001A0
#endif
}
/* clang-format on */

static void CancelThreads(void) {
    OSThread* thread = __OSActiveThreadQueue.head;
    while (thread != NULL) {
        OSThread* next = thread->linkActive.next;
        switch (thread->state) {
        case 1:
        case 4:
            OSCancelThread(thread);
        default:
            break;
        }
        thread = next;
    }
}

void __OSDoHotReset(int resetCode) {
    OSDisableInterrupts();
    __VIRegs[1] = 0;
    ICFlashInvalidate();
    Reset(resetCode << 3);
}

void OSResetSystem(int reset, unsigned long resetCode, int forceMenu) {
    int rc;
    int enabled;
    int padcal;
    struct OSSram* sram;
    u8 _[8];

    OSDisableScheduler();
    __OSStopAudioSystem();

    if (reset == 2) {
        padcal = __PADDisableRecalibration(1);
    }

    do {
    } while (CallResetFunctions(0) == 0);

    if ((reset == 1 && (forceMenu != 0))) {
        sram = __OSLockSram();
        sram->flags |= 0x40;
        __OSUnlockSram(1);
        do {
        } while (__OSSyncSram() == 0);
    }
    enabled = OSDisableInterrupts();
    rc = CallResetFunctions(1);
    LCDisable();
    if (reset == 1) {
        enabled = OSDisableInterrupts();
        __VIRegs[1] = 0;
        ICFlashInvalidate();
        Reset(resetCode * 8);
    } else if (reset == 0) {
        CancelThreads();
        OSEnableScheduler();
        __OSReboot(resetCode, forceMenu);
    }

    CancelThreads();

    memset((void*)0x80000040, 0, 0x8C);
    memset((void*)0x800000D4, 0, 0x14);
    memset((void*)0x800000F4, 0, 4);
    memset((void*)0x80003000, 0, 0xC0);
    memset((void*)0x800030C8, 0, 0xC);

    __PADDisableRecalibration(padcal);
}

unsigned long OSGetResetCode(void) {
    if (OS_REBOOT_BOOL != 0) {
        return 0x80000000;
    }
    return (__PIRegs[9] & ~7) >> 3;
}
