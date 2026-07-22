#include "types.h"
#include "dolphin/os/OSContext.h"

#pragma peephole off

typedef s64 OSTime;
typedef u8 __OSException;
typedef void (*__OSExceptionHandler)(__OSException exception, OSContext* context);

struct OSBootInfo_s {
    /* 0x00 */ u8 DiskID[0x20];
    /* 0x20 */ u32 magic;
    /* 0x24 */ u32 version;
    /* 0x28 */ u32 memorySize;
    /* 0x2C */ u32 consoleType;
    /* 0x30 */ void* arenaLo;
    /* 0x34 */ void* arenaHi;
    /* 0x38 */ void* FSTLocation;
    /* 0x3C */ u32 FSTMaxLength;
};

int OSDisableInterrupts(void);
int OSEnableInterrupts(void);
OSTime __OSGetSystemTime(void);
void OSSetArenaLo(void* newLo);
void OSSetArenaHi(void* newHi);
void* OSGetArenaLo(void);
void* OSGetArenaHi(void);
unsigned long OSGetResetCode(void);
void* memset(void* dst, int val, u32 n);
void* memcpy(void* dst, const void* src, u32 n);
void DCFlushRangeNoSync(void* addr, u32 nBytes);
void ICInvalidateRange(void* addr, u32 nBytes);
void ICFlashInvalidate(void);
void OSReport(const char* msg, ...);
void DBPrintf(char*, ...);
int __DBIsExceptionMarked(__OSException exception);
void EnableMetroTRKInterrupts(void);
u32 PPCMfhid2(void);
void PPCMthid2(u32 hid2);
void EXIInit(void);
void SIInit(void);
void OSInitAlarm(void);
void __OSInitSystemCall(void);
void __OSModuleInit(void);
void __OSInterruptInit(void);
void __OSContextInit(void);
void __OSCacheInit(void);
void __OSInitSram(void);
void __OSThreadInit(void);
void __OSInitAudioSystem(void);
void __OSInitMemoryProtection(void);
typedef s16 __OSInterrupt;
typedef void (*__OSInterruptHandler)(__OSInterrupt interrupt, OSContext* context);
__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler);
void __OSResetSWInterruptHandler(s16 exception, OSContext* context);
void __OSUnhandledException(u8 exception, OSContext* context, u32 dsisr, u32 dar);

extern unsigned long __DVDLongFileNameFlag;
extern unsigned long __PADSpec;
extern unsigned char __ArenaLo[];
extern char _stack_addr[];
extern unsigned char __ArenaHi[];

volatile u32 __DIRegs[] : 0xCC006000;
volatile u32 __PIRegs[] : 0xCC003000;

// dummy entry points to the OS Exception vector
void __OSEVStart(void);
void __OSEVEnd(void);
void __OSEVSetNumber(void);

void __DBVECTOR(void);
void __OSDBINTSTART(void);
void __OSDBINTEND(void);
void __OSDBJUMPSTART(void);
void __OSDBJUMPEND(void);

#define NOP 0x60000000

#define OSPhysicalToCached(paddr) ((void*)((u32)(paddr) + 0x80000000))

#define __sync() asm { sync }

u64 __OSStartTime;
int __OSInIPL;

static struct OSBootInfo_s* BootInfo;
static unsigned long* BI2DebugFlag;
static u32 BI2DebugFlagHolder;
static int AreWeInitialized;
static __OSExceptionHandler* OSExceptionTable;

static void OSExceptionInit(void);
void OSDefaultExceptionHandler(register __OSException exception, register OSContext* context);
__OSExceptionHandler __OSSetExceptionHandler(__OSException exception, __OSExceptionHandler handler);
__OSExceptionHandler __OSGetExceptionHandler(__OSException exception);

unsigned long OSGetConsoleType() {
    if ((!BootInfo) || (BootInfo->consoleType == 0)) {
        return 0x10000002;
    }
    return BootInfo->consoleType;
}

#define BOOT_REGION_START (*(u32*)0x812FDFF0)
#define BOOT_REGION_END (*(u32*)0x812FDFEC)

static void ClearArena(void) {
    if (OSGetResetCode() != 0x80000000) {
        memset(OSGetArenaLo(), 0, (u32)OSGetArenaHi() - (u32)OSGetArenaLo());
    } else {
        u32 boot_region_start = BOOT_REGION_START;
        u32 boot_region_end = BOOT_REGION_END;

        if (boot_region_start == 0) {
            memset(OSGetArenaLo(), 0, (u32)OSGetArenaHi() - (u32)OSGetArenaLo());
        } else if ((u32)OSGetArenaLo() < boot_region_start) {
            if ((u32)OSGetArenaHi() <= boot_region_start) {
                memset(OSGetArenaLo(), 0, (u32)OSGetArenaHi() - (u32)OSGetArenaLo());
            } else {
                memset(OSGetArenaLo(), 0, boot_region_start - (u32)OSGetArenaLo());
                if ((u32)OSGetArenaHi() > boot_region_end) {
                    memset((void*)boot_region_end, 0, (u32)OSGetArenaHi() - boot_region_end);
                }
            }
        }
    }
}

void OSInit() {
    unsigned long consoleType;
    u32 bi2StartAddr;

    if (AreWeInitialized == 0) {
        AreWeInitialized = 1;
        __OSStartTime = __OSGetSystemTime();
        OSDisableInterrupts();
        BootInfo = (struct OSBootInfo_s*)OSPhysicalToCached(0);
        BI2DebugFlag = NULL;
        __DVDLongFileNameFlag = 0;

        bi2StartAddr = *(u32*)OSPhysicalToCached(0xF4);
        if (bi2StartAddr) {
            BI2DebugFlag = (void*)((char*)bi2StartAddr + 0xC);
            __PADSpec = ((u32*)bi2StartAddr)[9];
            *(u8*)OSPhysicalToCached(0x30E8) = *BI2DebugFlag;
            *(u8*)OSPhysicalToCached(0x30E9) = __PADSpec;
        } else if ((void*)(*(u32*)OSPhysicalToCached(0x34)) != NULL) {
            bi2StartAddr = *(u8*)OSPhysicalToCached(0x30E8);
            BI2DebugFlagHolder = bi2StartAddr;
            BI2DebugFlag = &BI2DebugFlagHolder;
            __PADSpec = *(u8*)OSPhysicalToCached(0x30E9);
        }

        __DVDLongFileNameFlag = 1;

        OSSetArenaLo((!BootInfo->arenaLo) ? &__ArenaLo : BootInfo->arenaLo);
        if ((!BootInfo->arenaLo) && (BI2DebugFlag) && (*(u32*)BI2DebugFlag < 2)) {
            OSSetArenaLo((void*)(((u32)(char*)&_stack_addr + 0x1F) & 0xFFFFFFE0));
        }
        OSSetArenaHi((!BootInfo->arenaHi) ? &__ArenaHi : BootInfo->arenaHi);
        OSExceptionInit();
        __OSInitSystemCall();
        OSInitAlarm();
        __OSModuleInit();
        __OSInterruptInit();
        __OSSetInterruptHandler(0x16, &__OSResetSWInterruptHandler);
        __OSContextInit();
        __OSCacheInit();
        EXIInit();
        SIInit();
        __OSInitSram();
        __OSThreadInit();
        __OSInitAudioSystem();
        PPCMthid2(PPCMfhid2() & 0xBFFFFFFF);
        if ((BootInfo->consoleType & 0x10000000) != 0) {
            BootInfo->consoleType = 0x10000004;
        } else {
            BootInfo->consoleType = 1;
        }
        BootInfo->consoleType += (__PIRegs[11] & 0xF0000000) >> 28;
        if (__OSInIPL == 0) {
            __OSInitMemoryProtection();
        }
        OSReport("\nDolphin OS $Revision: 49 $.\n");
        OSReport("Kernel built : %s %s\n", "Dec 17 2001", "18:46:45");
        OSReport("Console Type : ");

        consoleType = OSGetConsoleType();
        if ((consoleType & 0x10000000) == 0) {
            OSReport("Retail %d\n", consoleType);
        } else {
            switch (consoleType) {
            case 0x10000000:
                OSReport("Mac Emulator\n");
                break;
            case 0x10000001:
                OSReport("PC Emulator\n");
                break;
            case 0x10000002:
                OSReport("EPPC Arthur\n");
                break;
            case 0x10000003:
                OSReport("EPPC Minnow\n");
                break;
            default:
                OSReport("Development HW%d\n", ((u32)consoleType - 0x10000000) - 3);
                break;
            }
        }
        OSReport("Memory %d MB\n", (u32)BootInfo->memorySize >> 0x14U);
        OSReport("Arena : 0x%x - 0x%x\n", OSGetArenaLo(), OSGetArenaHi());

        if (BI2DebugFlag != NULL && *BI2DebugFlag >= 2) {
            EnableMetroTRKInterrupts();
        }

        ClearArena();
        OSEnableInterrupts();
    }
}

static u32 __OSExceptionLocations[] = {
    0x00000100, 0x00000200, 0x00000300, 0x00000400, 0x00000500,
    0x00000600, 0x00000700, 0x00000800, 0x00000900, 0x00000C00,
    0x00000D00, 0x00000F00, 0x00001300, 0x00001400, 0x00001700,
};

static void OSExceptionInit(void) {
    u32* opCodeAddr;
    u32* location;
    u32 dbJumpSize;
    __OSException exception;
    u32 oldOpCode;
    u8* handlerStart;
    u32 handlerSize;
    __OSException exc2;
    u32 dbIntSize;
    u8* destAddr;

    opCodeAddr = (u32*)__OSEVSetNumber;
    oldOpCode = *opCodeAddr;
    handlerStart = (u8*)__OSEVStart;
    handlerSize = (u32)((u8*)__OSEVEnd - (u8*)__OSEVStart);

    destAddr = (void*)OSPhysicalToCached(0x60);
    if (*(u32*)OSPhysicalToCached(0x60) == 0) {
        DBPrintf("Installing OSDBIntegrator\n");
        dbIntSize = (u32)__OSDBINTEND - (u32)__OSDBINTSTART;
        memcpy(destAddr, (void*)__OSDBINTSTART, dbIntSize);
        DCFlushRangeNoSync(destAddr, dbIntSize);
        __sync();
        ICInvalidateRange(destAddr, dbIntSize);
    }

    location = __OSExceptionLocations;
    dbJumpSize = (u32)__OSDBJUMPEND - (u32)__OSDBINTEND;

    for (exception = 0; exception < 15; location++, exception++) {
        if (BI2DebugFlag && (*BI2DebugFlag >= 2) && __DBIsExceptionMarked(exception)) {
            DBPrintf(">>> OSINIT: exception %d commandeered by TRK\n", exception);
            continue;
        }

        *opCodeAddr = oldOpCode | exception;

        if (__DBIsExceptionMarked(exception)) {
            DBPrintf(">>> OSINIT: exception %d vectored to debugger\n", exception);
            memcpy((void*)__DBVECTOR, (void*)__OSDBINTEND, dbJumpSize);
        } else {
            u32* ops = (u32*)__DBVECTOR;
            int cb;

            for (cb = 0; cb < dbJumpSize; cb += sizeof(u32)) {
                *ops++ = NOP;
            }
        }

        destAddr = (void*)OSPhysicalToCached(*location);
        memcpy(destAddr, handlerStart, handlerSize);
        DCFlushRangeNoSync(destAddr, handlerSize);
        __sync();
        ICInvalidateRange(destAddr, handlerSize);
    }
    OSExceptionTable = (void*)OSPhysicalToCached(0x3000);

    for (exc2 = 0; exc2 < 15; exc2++) {
        __OSSetExceptionHandler(exc2, OSDefaultExceptionHandler);
    }

    *opCodeAddr = oldOpCode;

    DBPrintf("Exceptions initialized...\n");
}

/* clang-format off */
ASM static void __OSDBIntegrator(void) {
#ifdef __MWERKS__
    nofralloc

entry __OSDBINTSTART
    li r5, 0x40
    mflr r3
    stw r3, 0xc(r5)
    lwz r3, 0x8(r5)
    oris r3, r3, 0x8000
    mtlr r3
    li r3, 0x30
    mtmsr r3
    blr
entry __OSDBINTEND
#endif
}

ASM static void __OSDBJump(void) {
#ifdef __MWERKS__
    nofralloc

entry __OSDBJUMPSTART
    bla 0x60
entry __OSDBJUMPEND
#endif
}
/* clang-format on */

__OSExceptionHandler __OSSetExceptionHandler(__OSException exception,
                                             __OSExceptionHandler handler) {
    __OSExceptionHandler oldHandler;

    oldHandler = OSExceptionTable[exception];
    OSExceptionTable[exception] = handler;
    return oldHandler;
}

__OSExceptionHandler __OSGetExceptionHandler(__OSException exception) {
    return OSExceptionTable[exception];
}

/* clang-format off */
ASM static void OSExceptionVector(void) {
#ifdef __MWERKS__
    nofralloc

entry __OSEVStart
    mtsprg 0, r4

    lwz r4, 0xc0(r0)

    stw r3, 0xc(r4)
    mfsprg r3, 0
    stw r3, 0x10(r4)
    stw r5, 0x14(r4)

    lhz r3, 0x1a2(r4)
    ori r3, r3, 0x2
    sth r3, 0x1a2(r4)

    mfcr r3
    stw r3, 0x80(r4)
    mflr r3
    stw r3, 0x84(r4)
    mfctr r3
    stw r3, 0x88(r4)
    mfxer r3
    stw r3, 0x8c(r4)
    mfsrr0 r3
    stw r3, 0x198(r4)
    mfsrr1 r3
    stw r3, 0x19c(r4)
    mr r5, r3

entry __DBVECTOR
    nop

    mfmsr r3
    ori r3, r3, 0x30
    mtsrr1 r3

entry __OSEVSetNumber
    addi r3, 0, 0x0000

    lwz r4, 0xd4(r0)

    rlwinm. r5, r5, 0, 30, 30
    bne recoverable
    addis r5, 0, OSDefaultExceptionHandler@ha
    addi r5, r5, OSDefaultExceptionHandler@l
    mtsrr0 r5
    rfi

recoverable:
    rlwinm r5, r3, 2, 22, 29
    lwz r5, 0x3000(r5)
    mtsrr0 r5

    rfi

entry __OSEVEnd
    nop
#endif
}

ASM void OSDefaultExceptionHandler(register __OSException exception, register OSContext* context) {
#ifdef __MWERKS__
    nofralloc

    stw r0, 0x0(context)
    stw r1, 0x4(context)
    stw r2, 0x8(context)
    stmw r6, 0x18(context)
    mfspr r0, GQR1
    stw r0, 0x1a8(context)
    mfspr r0, GQR2
    stw r0, 0x1ac(context)
    mfspr r0, GQR3
    stw r0, 0x1b0(context)
    mfspr r0, GQR4
    stw r0, 0x1b4(context)
    mfspr r0, GQR5
    stw r0, 0x1b8(context)
    mfspr r0, GQR6
    stw r0, 0x1bc(context)
    mfspr r0, GQR7
    stw r0, 0x1c0(context)
    mfdsisr r5
    mfdar r6

    b __OSUnhandledException
#endif
}
/* clang-format on */

void __OSPSInit(void) {
    PPCMthid2(PPCMfhid2() | 0x80000000 | 0x20000000);
    ICFlashInvalidate();
    __sync();
    /* clang-format off */
    asm {
        li r3, 0
        mtspr GQR0, r3
    }
    /* clang-format on */
}

u32 __OSGetDIConfig(void) {
    return __DIRegs[9] & 0xFF;
}
