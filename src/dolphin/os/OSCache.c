#include "types.h"
#include "dolphin/os/OSContext.h"

typedef void (*OSErrorHandler)(int exception, ...);

u32 PPCMfmsr(void);
void PPCMtmsr(u32 msr);
u32 PPCMfhid0(void);
u32 PPCMfhid2(void);
void PPCMthid2(u32 hid2);
u32 PPCMfl2cr(void);
void PPCMtl2cr(u32 l2cr);
void PPCHalt(void);
void OSReport(const char* msg, ...);
void DBPrintf(char*, ...);
OSErrorHandler OSSetErrorHandler(u16 error, OSErrorHandler handler);

#define HID2 920

#define __sync() asm { sync }

/* clang-format off */
ASM void DCEnable(void) {
#ifdef __MWERKS__
    nofralloc

    sync
    mfspr r3, HID0
    ori r3, r3, 0x4000
    mtspr HID0, r3
    blr
#endif
}

ASM void DCInvalidateRange(register void* addr, register u32 nBytes) {
#ifdef __MWERKS__
    nofralloc

    cmplwi nBytes, 0
    blelr
    clrlwi. r5, addr, 27
    beq _aligned
    addi nBytes, nBytes, 0x20
_aligned:
    addi nBytes, nBytes, 0x1f
    srwi nBytes, nBytes, 5
    mtctr nBytes
_loop:
    dcbi r0, addr
    addi addr, addr, 0x20
    bdnz _loop
    blr
#endif
}

ASM void DCFlushRange(register void* addr, register u32 nBytes) {
#ifdef __MWERKS__
    nofralloc

    cmplwi nBytes, 0
    blelr
    clrlwi. r5, addr, 27
    beq _aligned
    addi nBytes, nBytes, 0x20
_aligned:
    addi nBytes, nBytes, 0x1f
    srwi nBytes, nBytes, 5
    mtctr nBytes
_loop:
    dcbf r0, addr
    addi addr, addr, 0x20
    bdnz _loop
    sc
    blr
#endif
}

ASM void DCStoreRange(register void* addr, register u32 nBytes) {
#ifdef __MWERKS__
    nofralloc

    cmplwi nBytes, 0
    blelr
    clrlwi. r5, addr, 27
    beq _aligned
    addi nBytes, nBytes, 0x20
_aligned:
    addi nBytes, nBytes, 0x1f
    srwi nBytes, nBytes, 5
    mtctr nBytes
_loop:
    dcbst r0, addr
    addi addr, addr, 0x20
    bdnz _loop
    sc
    blr
#endif
}

ASM void DCFlushRangeNoSync(register void* addr, register u32 nBytes) {
#ifdef __MWERKS__
    nofralloc

    cmplwi nBytes, 0
    blelr
    clrlwi. r5, addr, 27
    beq _aligned
    addi nBytes, nBytes, 0x20
_aligned:
    addi nBytes, nBytes, 0x1f
    srwi nBytes, nBytes, 5
    mtctr nBytes
_loop:
    dcbf r0, addr
    addi addr, addr, 0x20
    bdnz _loop
    blr
#endif
}

ASM void ICInvalidateRange(register void* addr, register u32 nBytes) {
#ifdef __MWERKS__
    nofralloc

    cmplwi nBytes, 0
    blelr
    clrlwi. r5, addr, 27
    beq _aligned
    addi nBytes, nBytes, 0x20
_aligned:
    addi nBytes, nBytes, 0x1f
    srwi nBytes, nBytes, 5
    mtctr nBytes
_loop:
    icbi r0, addr
    addi addr, addr, 0x20
    bdnz _loop
    sync
    isync
    blr
#endif
}

ASM void ICFlashInvalidate(void) {
#ifdef __MWERKS__
    nofralloc

    mfspr r3, HID0
    ori r3, r3, 0x800
    mtspr HID0, r3
    blr
#endif
}

ASM void ICEnable(void) {
#ifdef __MWERKS__
    nofralloc

    isync
    mfspr r3, HID0
    ori r3, r3, 0x8000
    mtspr HID0, r3
    blr
#endif
}

ASM void LCDisable(void) {
#ifdef __MWERKS__
    nofralloc

    lis r3, 0xe000
    li r4, 0x200
    mtctr r4
_loop:
    dcbi r0, r3
    addi r3, r3, 32
    bdnz _loop
    mfspr r4, HID2
    rlwinm r4, r4, 0, 4, 2
    mtspr HID2, r4
    blr
#endif
}
/* clang-format on */

static void L2Disable(void) {
    __sync();
    PPCMtl2cr(PPCMfl2cr() & ~0x80000000);
    __sync();
}

void L2GlobalInvalidate(void) {
    L2Disable();
    PPCMtl2cr(PPCMfl2cr() | 0x00200000);
    while (PPCMfl2cr() & 0x00000001u)
        ;
    PPCMtl2cr(PPCMfl2cr() & ~0x00200000);
    while (PPCMfl2cr() & 0x00000001u) {
        DBPrintf(">>> L2 INVALIDATE : SHOULD NEVER HAPPEN\n");
    }
}

void DMAErrorHandler(int error, OSContext* context, ...) {
    u32 hid2 = PPCMfhid2();

    OSReport("Machine check received\n");
    OSReport("HID2 = 0x%x   SRR1 = 0x%x\n", hid2, context->srr1);
    if (!(hid2 & 0x00F00000) || !(context->srr1 & 0x00200000)) {
        OSReport("Machine check was not DMA/locked cache related\n");
        OSDumpContext(context);
        PPCHalt();
    }

    OSReport("DMAErrorHandler(): An error occurred while processing DMA.\n");
    OSReport("The following errors have been detected and cleared :\n");

    if (hid2 & 0x00800000) {
        OSReport("\t- Requested a locked cache tag that was already in the cache\n");
    }

    if (hid2 & 0x00400000) {
        OSReport("\t- DMA attempted to access normal cache\n");
    }

    if (hid2 & 0x00200000) {
        OSReport("\t- DMA missed in data cache\n");
    }

    if (hid2 & 0x00100000) {
        OSReport("\t- DMA queue overflowed\n");
    }

    PPCMthid2(hid2);
}

static void L2Init(void) {
    u32 oldMSR;
    oldMSR = PPCMfmsr();
    __sync();
    PPCMtmsr(0x30);
    __sync();
    L2Disable();
    L2GlobalInvalidate();
    PPCMtmsr(oldMSR);
}

static void L2Enable(void) {
    PPCMtl2cr((PPCMfl2cr() | 0x80000000) & ~0x00200000);
}

void __OSCacheInit(void) {
    if (!(PPCMfhid0() & 0x8000)) {
        ICEnable();
        DBPrintf("L1 i-caches initialized\n");
    }
    if (!(PPCMfhid0() & 0x4000)) {
        DCEnable();
        DBPrintf("L1 d-caches initialized\n");
    }

    if (!(PPCMfl2cr() & 0x80000000)) {
        L2Init();
        L2Enable();
        DBPrintf("L2 cache initialized\n");
    }

    OSSetErrorHandler(1, DMAErrorHandler);
    DBPrintf("Locked cache machine check handler installed\n");
}
