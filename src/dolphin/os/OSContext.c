#include "types.h"
#include "dolphin/os/OSContext.h"

#define HID2 920

void __OSSetExceptionHandler(u8 exception, void (*handler)(u8, OSContext*));
void DBPrintf(char*, ...);
int OSDisableInterrupts(void);
void OSRestoreInterrupts(int enabled);
void OSReport(const char* msg, ...);

OSContext* volatile __OSCurrentContext : 0x800000D4;
OSContext* volatile __OSFPUContext : 0x800000D8;

static void OSSwitchFPUContext(u8 exception, OSContext* context);

/* clang-format off */
ASM static void __OSLoadFPUContext(register u32 unused, register OSContext* fpuContext) {
#ifdef __MWERKS__
    nofralloc

    lhz r5, 0x1a2(fpuContext)
    clrlwi. r5, r5, 31
    beq _return

    lfd f0, 0x190(fpuContext)
    mtfsf 0xff, f0
    mfspr r5, HID2
    rlwinm. r5, r5, 3, 31, 31
    beq _regular_FPRs

    psq_l f0, 0x1c8(fpuContext), 0, 0
    psq_l f1, 0x1d0(fpuContext), 0, 0
    psq_l f2, 0x1d8(fpuContext), 0, 0
    psq_l f3, 0x1e0(fpuContext), 0, 0
    psq_l f4, 0x1e8(fpuContext), 0, 0
    psq_l f5, 0x1f0(fpuContext), 0, 0
    psq_l f6, 0x1f8(fpuContext), 0, 0
    psq_l f7, 0x200(fpuContext), 0, 0
    psq_l f8, 0x208(fpuContext), 0, 0
    psq_l f9, 0x210(fpuContext), 0, 0
    psq_l f10, 0x218(fpuContext), 0, 0
    psq_l f11, 0x220(fpuContext), 0, 0
    psq_l f12, 0x228(fpuContext), 0, 0
    psq_l f13, 0x230(fpuContext), 0, 0
    psq_l f14, 0x238(fpuContext), 0, 0
    psq_l f15, 0x240(fpuContext), 0, 0
    psq_l f16, 0x248(fpuContext), 0, 0
    psq_l f17, 0x250(fpuContext), 0, 0
    psq_l f18, 0x258(fpuContext), 0, 0
    psq_l f19, 0x260(fpuContext), 0, 0
    psq_l f20, 0x268(fpuContext), 0, 0
    psq_l f21, 0x270(fpuContext), 0, 0
    psq_l f22, 0x278(fpuContext), 0, 0
    psq_l f23, 0x280(fpuContext), 0, 0
    psq_l f24, 0x288(fpuContext), 0, 0
    psq_l f25, 0x290(fpuContext), 0, 0
    psq_l f26, 0x298(fpuContext), 0, 0
    psq_l f27, 0x2a0(fpuContext), 0, 0
    psq_l f28, 0x2a8(fpuContext), 0, 0
    psq_l f29, 0x2b0(fpuContext), 0, 0
    psq_l f30, 0x2b8(fpuContext), 0, 0
    psq_l f31, 0x2c0(fpuContext), 0, 0

_regular_FPRs:
    lfd f0, 0x90(fpuContext)
    lfd f1, 0x98(fpuContext)
    lfd f2, 0xa0(fpuContext)
    lfd f3, 0xa8(fpuContext)
    lfd f4, 0xb0(fpuContext)
    lfd f5, 0xb8(fpuContext)
    lfd f6, 0xc0(fpuContext)
    lfd f7, 0xc8(fpuContext)
    lfd f8, 0xd0(fpuContext)
    lfd f9, 0xd8(fpuContext)
    lfd f10, 0xe0(fpuContext)
    lfd f11, 0xe8(fpuContext)
    lfd f12, 0xf0(fpuContext)
    lfd f13, 0xf8(fpuContext)
    lfd f14, 0x100(fpuContext)
    lfd f15, 0x108(fpuContext)
    lfd f16, 0x110(fpuContext)
    lfd f17, 0x118(fpuContext)
    lfd f18, 0x120(fpuContext)
    lfd f19, 0x128(fpuContext)
    lfd f20, 0x130(fpuContext)
    lfd f21, 0x138(fpuContext)
    lfd f22, 0x140(fpuContext)
    lfd f23, 0x148(fpuContext)
    lfd f24, 0x150(fpuContext)
    lfd f25, 0x158(fpuContext)
    lfd f26, 0x160(fpuContext)
    lfd f27, 0x168(fpuContext)
    lfd f28, 0x170(fpuContext)
    lfd f29, 0x178(fpuContext)
    lfd f30, 0x180(fpuContext)
    lfd f31, 0x188(fpuContext)
_return:
    blr
#endif
}

ASM static void __OSSaveFPUContext(register u32 unused0, register u32 unused1, register OSContext* fpuContext) {
#ifdef __MWERKS__
    nofralloc

    lhz r3, 0x1a2(fpuContext)
    ori r3, r3, 1
    sth r3, 0x1a2(fpuContext)

    stfd f0, 0x90(fpuContext)
    stfd f1, 0x98(fpuContext)
    stfd f2, 0xa0(fpuContext)
    stfd f3, 0xa8(fpuContext)
    stfd f4, 0xb0(fpuContext)
    stfd f5, 0xb8(fpuContext)
    stfd f6, 0xc0(fpuContext)
    stfd f7, 0xc8(fpuContext)
    stfd f8, 0xd0(fpuContext)
    stfd f9, 0xd8(fpuContext)
    stfd f10, 0xe0(fpuContext)
    stfd f11, 0xe8(fpuContext)
    stfd f12, 0xf0(fpuContext)
    stfd f13, 0xf8(fpuContext)
    stfd f14, 0x100(fpuContext)
    stfd f15, 0x108(fpuContext)
    stfd f16, 0x110(fpuContext)
    stfd f17, 0x118(fpuContext)
    stfd f18, 0x120(fpuContext)
    stfd f19, 0x128(fpuContext)
    stfd f20, 0x130(fpuContext)
    stfd f21, 0x138(fpuContext)
    stfd f22, 0x140(fpuContext)
    stfd f23, 0x148(fpuContext)
    stfd f24, 0x150(fpuContext)
    stfd f25, 0x158(fpuContext)
    stfd f26, 0x160(fpuContext)
    stfd f27, 0x168(fpuContext)
    stfd f28, 0x170(fpuContext)
    stfd f29, 0x178(fpuContext)
    stfd f30, 0x180(fpuContext)
    stfd f31, 0x188(fpuContext)

    mffs f0
    stfd f0, 0x190(fpuContext)

    lfd f0, 0x90(fpuContext)

    mfspr r3, HID2
    rlwinm. r3, r3, 3, 31, 31
    bc 12, 2, _return

    psq_st f0, 0x1c8(fpuContext), 0, 0
    psq_st f1, 0x1d0(fpuContext), 0, 0
    psq_st f2, 0x1d8(fpuContext), 0, 0
    psq_st f3, 0x1e0(fpuContext), 0, 0
    psq_st f4, 0x1e8(fpuContext), 0, 0
    psq_st f5, 0x1f0(fpuContext), 0, 0
    psq_st f6, 0x1f8(fpuContext), 0, 0
    psq_st f7, 0x200(fpuContext), 0, 0
    psq_st f8, 0x208(fpuContext), 0, 0
    psq_st f9, 0x210(fpuContext), 0, 0
    psq_st f10, 0x218(fpuContext), 0, 0
    psq_st f11, 0x220(fpuContext), 0, 0
    psq_st f12, 0x228(fpuContext), 0, 0
    psq_st f13, 0x230(fpuContext), 0, 0
    psq_st f14, 0x238(fpuContext), 0, 0
    psq_st f15, 0x240(fpuContext), 0, 0
    psq_st f16, 0x248(fpuContext), 0, 0
    psq_st f17, 0x250(fpuContext), 0, 0
    psq_st f18, 0x258(fpuContext), 0, 0
    psq_st f19, 0x260(fpuContext), 0, 0
    psq_st f20, 0x268(fpuContext), 0, 0
    psq_st f21, 0x270(fpuContext), 0, 0
    psq_st f22, 0x278(fpuContext), 0, 0
    psq_st f23, 0x280(fpuContext), 0, 0
    psq_st f24, 0x288(fpuContext), 0, 0
    psq_st f25, 0x290(fpuContext), 0, 0
    psq_st f26, 0x298(fpuContext), 0, 0
    psq_st f27, 0x2a0(fpuContext), 0, 0
    psq_st f28, 0x2a8(fpuContext), 0, 0
    psq_st f29, 0x2b0(fpuContext), 0, 0
    psq_st f30, 0x2b8(fpuContext), 0, 0
    psq_st f31, 0x2c0(fpuContext), 0, 0

_return:
    blr
#endif
}

ASM void OSSetCurrentContext(register OSContext* context) {
#ifdef __MWERKS__
    nofralloc

    addis r4, r0, 0x8000

    stw context, 0x00D4(r4)

    clrlwi r5, context, 2
    stw r5, 0x00C0(r4)

    lwz r5, 0x00D8(r4)
    cmpw r5, context
    bne _disableFPU

    lwz r6, 0x19c(context)
    ori r6, r6, 0x2000
    stw r6, 0x19c(context)
    mfmsr r6
    ori r6, r6, 2
    mtmsr r6
    blr

_disableFPU:
    lwz r6, 0x19c(context)
    rlwinm r6, r6, 0, 19, 17
    stw r6, 0x19c(context)
    mfmsr r6
    rlwinm r6, r6, 0, 19, 17
    ori r6, r6, 2
    mtmsr r6
    isync
    blr
#endif
}
/* clang-format on */

OSContext* OSGetCurrentContext(void) {
    return (OSContext*)__OSCurrentContext;
}

/* clang-format off */
ASM u32 OSSaveContext(register OSContext* context) {
#ifdef __MWERKS__
    nofralloc

    stmw r13, 0x34(context)
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
    mfcr r0
    stw r0, 0x80(context)
    mflr r0
    stw r0, 0x84(context)
    stw r0, 0x198(context)
    mfmsr r0
    stw r0, 0x19c(context)
    mfctr r0
    stw r0, 0x88(context)
    mfxer r0
    stw r0, 0x8c(context)
    stw r1, 0x4(context)
    stw r2, 0x8(context)
    li r0, 0x1
    stw r0, 0xc(context)
    li r3, 0
    blr
#endif
}

ASM void OSLoadContext(register OSContext* context) {
#ifdef __MWERKS__
    nofralloc

    lis r4, OSDisableInterrupts@ha
    lwz r6, 0x198(context)
    addi r5, r4, OSDisableInterrupts@l
    cmplw r6, r5
    blt _notInRAS
    lis r4, OSDisableInterrupts+0x10@ha
    addi r0, r4, OSDisableInterrupts+0x10@l
    cmplw r6, r0
    bgt _notInRAS
    stw r5, 0x198(context)

_notInRAS:
    lwz r0, 0x0(context)
    lwz r1, 0x4(context)
    lwz r2, 0x8(context)

    lhz r4, 0x1a2(context)
    rlwinm. r5, r4, 0, 30, 30
    beq notexc
    rlwinm r4, r4, 0, 31, 29
    sth r4, 0x1a2(context)
    lmw r5, 0x14(context)
    b misc
notexc:
    lmw r13, 0x34(context)
misc:
    lwz r4, 0x1a8(context)
    mtspr GQR1, r4
    lwz r4, 0x1ac(context)
    mtspr GQR2, r4
    lwz r4, 0x1b0(context)
    mtspr GQR3, r4
    lwz r4, 0x1b4(context)
    mtspr GQR4, r4
    lwz r4, 0x1b8(context)
    mtspr GQR5, r4
    lwz r4, 0x1bc(context)
    mtspr GQR6, r4
    lwz r4, 0x1c0(context)
    mtspr GQR7, r4

    lwz r4, 0x80(context)
    mtcr r4
    lwz r4, 0x84(context)
    mtlr r4
    lwz r4, 0x88(context)
    mtctr r4
    lwz r4, 0x8c(context)
    mtxer r4

    mfmsr r4
    rlwinm r4, r4, 0, 17, 15
    rlwinm r4, r4, 0, 31, 29
    mtmsr r4

    lwz r4, 0x198(context)
    mtsrr0 r4
    lwz r4, 0x19c(context)
    mtsrr1 r4

    lwz r4, 0x10(context)
    lwz r3, 0xc(context)

    rfi
#endif
}

ASM u32 OSGetStackPointer(void) {
#ifdef __MWERKS__
    nofralloc

    mr r3, r1
    blr
#endif
}
/* clang-format on */

void OSClearContext(register OSContext* context) {
    context->mode = 0;
    context->state = 0;
    if (context == __OSFPUContext)
        __OSFPUContext = NULL;
}

/* clang-format off */
ASM void OSInitContext(register OSContext* context, register u32 pc, register u32 newsp) {
#ifdef __MWERKS__
    nofralloc

    stw pc, 0x198(context)
    stw newsp, 0x4(context)
    li r11, 0
    ori r11, r11, 0x00008000 | 0x00000020 | 0x00000010 | 0x00000002 | 0x00001000
    stw r11, 0x19c(context)
    li r0, 0x0
    stw r0, 0x80(context)
    stw r0, 0x8c(context)

    stw r2, 0x8(context)
    stw r13, 0x34(context)

    stw r0, 0xc(context)
    stw r0, 0x10(context)
    stw r0, 0x14(context)
    stw r0, 0x18(context)
    stw r0, 0x1c(context)
    stw r0, 0x20(context)
    stw r0, 0x24(context)
    stw r0, 0x28(context)
    stw r0, 0x2c(context)
    stw r0, 0x30(context)

    stw r0, 0x38(context)
    stw r0, 0x3c(context)
    stw r0, 0x40(context)
    stw r0, 0x44(context)
    stw r0, 0x48(context)
    stw r0, 0x4c(context)
    stw r0, 0x50(context)
    stw r0, 0x54(context)
    stw r0, 0x58(context)
    stw r0, 0x5c(context)
    stw r0, 0x60(context)
    stw r0, 0x64(context)
    stw r0, 0x68(context)
    stw r0, 0x6c(context)
    stw r0, 0x70(context)
    stw r0, 0x74(context)
    stw r0, 0x78(context)
    stw r0, 0x7c(context)

    stw r0, 0x1a4(context)
    stw r0, 0x1a8(context)
    stw r0, 0x1ac(context)
    stw r0, 0x1b0(context)
    stw r0, 0x1b4(context)
    stw r0, 0x1b8(context)
    stw r0, 0x1bc(context)
    stw r0, 0x1c0(context)

    b OSClearContext
#endif
}
/* clang-format on */

void OSDumpContext(OSContext* context) {
    u32 i;
    u32* p;

    OSReport("------------------------- Context 0x%08x -------------------------\n", context);

    for (i = 0; i < 16; ++i) {
        OSReport("r%-2d  = 0x%08x (%14d)  r%-2d  = 0x%08x (%14d)\n", i, context->gpr[i],
                 context->gpr[i], i + 16, context->gpr[i + 16], context->gpr[i + 16]);
    }

    OSReport("LR   = 0x%08x                   CR   = 0x%08x\n", context->lr, context->cr);
    OSReport("SRR0 = 0x%08x                   SRR1 = 0x%08x\n", context->srr0, context->srr1);

    OSReport("\nGQRs----------\n");
    for (i = 0; i < 4; ++i) {
        OSReport("gqr%d = 0x%08x \t gqr%d = 0x%08x\n", i, context->gqr[i], i + 4,
                 context->gqr[i + 4]);
    }

    if (context->state & (u32)OS_CONTEXT_STATE_FPSAVED) {
        OSContext* currentContext;
        OSContext fpuContext;
        BOOL enabled;

        enabled = OSDisableInterrupts();
        currentContext = OSGetCurrentContext();
        OSClearContext(&fpuContext);
        OSSetCurrentContext(&fpuContext);

        OSReport("\n\nFPRs----------\n");
        for (i = 0; i < 32; i += 2) {
            OSReport("fr%d \t= %d \t fr%d \t= %d\n", i, (u32)context->fpr[i], i + 1,
                     (u32)context->fpr[i + 1]);
        }
        OSReport("\n\nPSFs----------\n");
        for (i = 0; i < 32; i += 2) {
            OSReport("ps%d \t= 0x%x \t ps%d \t= 0x%x\n", i, (u32)context->psf[i], i + 1,
                     (u32)context->psf[i + 1]);
        }

        OSClearContext(&fpuContext);
        OSSetCurrentContext(currentContext);
        OSRestoreInterrupts(enabled);
    }

    OSReport("\nAddress:      Back Chain    LR Save\n");
    for (i = 0, p = (u32*)context->gpr[1]; p && (u32)p != 0xffffffff && i++ < 16; p = (u32*)*p) {
        OSReport("0x%08x:   0x%08x    0x%08x\n", p, p[0], p[1]);
    }
}

/* clang-format off */
ASM static void OSSwitchFPUContext(register u8 exception, register OSContext* context) {
#ifdef __MWERKS__
    nofralloc

    mfmsr r5
    ori r5, r5, 0x2000
    mtmsr r5
    isync
    lwz r5, 0x19c(context)
    ori r5, r5, 0x2000
    mtsrr1 r5
    addis r3, r0, 0x8000
    lwz r5, 0x00D8(r3)
    stw context, 0x00D8(r3)
    cmpw r5, r4
    beq _restoreAndExit
    cmpwi r5, 0x0
    beq _loadNewFPUContext
    bl __OSSaveFPUContext
_loadNewFPUContext:
    bl __OSLoadFPUContext
_restoreAndExit:
    lwz r3, 0x80(context)
    mtcr r3
    lwz r3, 0x84(context)
    mtlr r3
    lwz r3, 0x198(context)
    mtsrr0 r3
    lwz r3, 0x88(context)
    mtctr r3
    lwz r3, 0x8c(context)
    mtxer r3
    lhz r3, 0x1a2(context)
    rlwinm r3, r3, 0, 31, 29
    sth r3, 0x1a2(context)
    lwz r5, 0x14(context)
    lwz r3, 0xc(context)
    lwz r4, 0x10(context)
    rfi
#endif
}
/* clang-format on */

void __OSContextInit(void) {
    __OSSetExceptionHandler(7, OSSwitchFPUContext);
    __OSFPUContext = NULL;
    DBPrintf("FPU-unavailable handler installed\n");
}
