#include "types.h"
#include "dolphin/os/OSContext.h"

typedef s16 __OSInterrupt;
typedef void (*__OSInterruptHandler)(__OSInterrupt interrupt, OSContext* context);
typedef void (*OSErrorHandler)(int exception, ...);

typedef struct OSResetFunctionInfo {
    int (*func)(int final);
    u32 priority;
    struct OSResetFunctionInfo* next;
    struct OSResetFunctionInfo* prev;
} OSResetFunctionInfo;

int OSDisableInterrupts(void);
int OSRestoreInterrupts(int level);
u32 __OSMaskInterrupts(u32 global);
u32 __OSUnmaskInterrupts(u32 global);
__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler);
void OSRegisterResetFunction(OSResetFunctionInfo* info);
void __OSUnhandledException(u8 exception, OSContext* context, u32 dsisr, u32 dar);

extern OSErrorHandler __OSErrorTable[16];

volatile u16 __MEMRegs[] : 0xCC004000;

static int OnReset(int final);

static OSResetFunctionInfo ResetFunctionInfo = {
    OnReset,
    127,
};

u32 OSGetPhysicalMemSize(void) {
    return *(u32*)0x80000028;
}

u32 OSGetConsoleSimulatedMemSize(void) {
    return *(u32*)0x800000F0;
}

static int OnReset(int final) {
    if (final != 0) {
        __MEMRegs[8] = 0xFF;
        __OSMaskInterrupts(0xF0000000);
    }
    return 1;
}

static void MEMIntrruptHandler(__OSInterrupt interrupt, OSContext* context) {
    u32 cause = __MEMRegs[0xF];
    u32 addr = ((__MEMRegs[0x12] & 0x3FF) << 16) | __MEMRegs[0x11];
    __MEMRegs[0x10] = 0;

    if (__OSErrorTable[0xF]) {
        __OSErrorTable[0xF](0xF, context, cause, addr);
        return;
    }

    __OSUnhandledException(0xF, context, cause, addr);
}

/* clang-format off */
ASM static void Config24MB(void) {
#ifdef __MWERKS__
    nofralloc

    li r7, 0
    lis r4, 0x0000
    addi r4, r4, 0x2
    lis r3, 0x8000
    addi r3, r3, 0x1ff
    lis r6, 0x0100
    addi r6, r6, 0x2
    lis r5, 0x8100
    addi r5, r5, 0xff
    isync
    mtdbatu 0, r7
    mtdbatl 0, r4
    mtdbatu 0, r3
    isync
    mtibatu 0, r7
    mtibatl 0, r4
    mtibatu 0, r3
    isync
    mtdbatu 2, r7
    mtdbatl 2, r6
    mtdbatu 2, r5
    isync
    mtibatu 2, r7
    mtibatl 2, r6
    mtibatu 2, r5
    isync
    mfmsr r3
    ori r3, r3, 0x30
    mtsrr1 r3
    mflr r3
    mtsrr0 r3
    rfi
#endif
}

ASM static void Config48MB(void) {
#ifdef __MWERKS__
    nofralloc

    li r7, 0
    lis r4, 0x0000
    addi r4, r4, 0x2
    lis r3, 0x8000
    addi r3, r3, 0x3ff
    lis r6, 0x0200
    addi r6, r6, 0x2
    lis r5, 0x8200
    addi r5, r5, 0x1ff
    isync
    mtdbatu 0, r7
    mtdbatl 0, r4
    mtdbatu 0, r3
    isync
    mtibatu 0, r7
    mtibatl 0, r4
    mtibatu 0, r3
    isync
    mtdbatu 2, r7
    mtdbatl 2, r6
    mtdbatu 2, r5
    isync
    mtibatu 2, r7
    mtibatl 2, r6
    mtibatu 2, r5
    isync
    mfmsr r3
    ori r3, r3, 0x30
    mtsrr1 r3
    mflr r3
    mtsrr0 r3
    rfi
#endif
}

ASM static void RealMode(register u32 addr) {
#ifdef __MWERKS__
    nofralloc

    clrlwi addr, addr, 2
    mtsrr0 addr
    mfmsr addr
    rlwinm addr, addr, 0, 28, 25
    mtsrr1 addr
    rfi
#endif
}
/* clang-format on */

void __OSInitMemoryProtection(void) {
    u8 _[40];

    u32 simulated_mem = OSGetConsoleSimulatedMemSize();
    int enabled = OSDisableInterrupts();
    if (simulated_mem <= 24 * 1024 * 1024) {
        RealMode((u32)Config24MB);
    } else if (simulated_mem <= 48 * 1024 * 1024) {
        RealMode((u32)Config48MB);
    }
    __MEMRegs[16] = 0;
    __MEMRegs[8] = 0xFF;
    __OSMaskInterrupts(0xF0000000);
    __OSSetInterruptHandler(0, MEMIntrruptHandler);
    __OSSetInterruptHandler(1, MEMIntrruptHandler);
    __OSSetInterruptHandler(2, MEMIntrruptHandler);
    __OSSetInterruptHandler(3, MEMIntrruptHandler);
    __OSSetInterruptHandler(4, MEMIntrruptHandler);
    OSRegisterResetFunction(&ResetFunctionInfo);
    simulated_mem = OSGetConsoleSimulatedMemSize();
    if (simulated_mem < OSGetPhysicalMemSize() && simulated_mem == 24 * 1024 * 1024) {
        __MEMRegs[20] = 2;
    }
    __OSUnmaskInterrupts(0x08000000);
    OSRestoreInterrupts(enabled);
}
