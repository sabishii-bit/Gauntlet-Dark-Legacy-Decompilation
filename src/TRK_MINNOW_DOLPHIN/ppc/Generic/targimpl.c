#include "trk.h"

#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msgbuf.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msghndlr.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/nubevent.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/support.h"
#include "TRK_MINNOW_DOLPHIN/ppc/Generic/targimpl.h"

// targimpl.c - TRK target implementation (PPC 6xx/7xx).

#define MSR_DR 0x10
#define TRK_DEFAULT_XER 36

#define DSFetch_u64(p) (*(u64*)(p))
#define DSFetch_u32(p) (*(u32*)(p))

typedef u32 InstructionType;
typedef u32 DefaultType;
typedef u64 FloatType;

typedef struct u128 {
    u32 x[4];
} u128;

typedef struct StopInfo_PPC {
    /* 0x00 */ u32 PC;
    /* 0x04 */ u32 PCInstruction;
    /* 0x08 */ u16 exceptionID;
} StopInfo_PPC;

typedef struct memRange {
    /* 0x00 */ u8* start;
    /* 0x04 */ u8* end;
    /* 0x08 */ BOOL readable;
    /* 0x0C */ BOOL writeable;
} memRange;

typedef struct TRKExceptionStatus {
    StopInfo_PPC exceptionInfo;
    u8 inTRK;
    u8 exceptionDetected;
} TRKExceptionStatus;

typedef struct TRKStepStatus {
    BOOL active;    // 0x0
    u8 type;        // 0x4
    u32 count;      // 0x8
    u32 rangeStart; // 0xC
    u32 rangeEnd;   // 0x10
} TRKStepStatus;

typedef void (*RegAccessFunc)(void* srcDestPtr, u128* val);

// Instruction encoders
#define INSTR_NOP 0x60000000
#define INSTR_BLR 0x4E800020
#define INSTR_PSQ_ST(psr, offset, rDest, w, gqr)                              \
    (0xF0000000 | (psr << 21) | (rDest << 16) | (w << 15) | (gqr << 12) |     \
     offset)
#define INSTR_PSQ_L(psr, offset, rSrc, w, gqr)                                \
    (0xE0000000 | (psr << 21) | (rSrc << 16) | (w << 15) | (gqr << 12) |      \
     offset)
#define INSTR_STW(rSrc, offset, rDest)                                        \
    (0x90000000 | (rSrc << 21) | (rDest << 16) | offset)
#define INSTR_LWZ(rDest, offset, rSrc)                                        \
    (0x80000000 | (rDest << 21) | (rSrc << 16) | offset)
#define INSTR_STFD(fprSrc, offset, rDest)                                     \
    (0xD8000000 | (fprSrc << 21) | (rDest << 16) | offset)
#define INSTR_LFD(fprDest, offset, rSrc)                                      \
    (0xC8000000 | (fprDest << 21) | (rSrc << 16) | offset)
#define INSTR_MFSPR(rDest, spr)                                               \
    (0x7C000000 | (rDest << 21) | ((spr & 0xFE0) << 6) |                      \
     ((spr & 0x1F) << 16) | 0x2A6)
#define INSTR_MTSPR(spr, rSrc)                                                \
    (0x7C000000 | (rSrc << 21) | ((spr & 0xFE0) << 6) |                       \
     ((spr & 0x1F) << 16) | 0x3A6)

// External globals (common BSS; owned by the auto-split .bss object)
extern BOOL gTRKBigEndian;
extern u128 TRKvalue128_temp;

// Helper functions from other TUs
u32 TRKTargetTranslate(u32 addr);
void TRK_flush_cache(u32 addr, u32 length);
u8 TRKTargetCPUMinorType(void);
DSError TRKDoNotifyStopped(MessageCommandID command);
void TRKUARTInterruptHandler(void);
void TRKRestoreExtended1Block(void);

// Common BSS (external linkage in GDL, unlike Melee's statics)
extern u16 TRK_saved_exceptionID;
extern Default_PPC gTRKSaveState;

void TRKExceptionHandler(u16 id);
void TRKInterruptHandlerEnableInterrupts(void);
void TRKPostInterruptEvent(void);

// Forward declarations
static DSError TRKValidMemory32(const void* addr, size_t length, int readWriteable);
static DSError TRKTargetReadInstruction(void* data, u32 start);
static DSError TRKTargetEnableTrace(BOOL enable);
static BOOL TRKTargetCheckStep(void);
DSError TRKPPCAccessSPR(void* srcDestPtr, u32 spr, BOOL read);
DSError TRKPPCAccessPairedSingleRegister(void* srcDestPtr, u32 psr, BOOL read);
DSError TRKPPCAccessFPRegister(void* srcDestPtr, u32 fpr, BOOL read);
DSError TRKPPCAccessSpecialReg(void* srcDestPtr, u32* instructionData, BOOL read);

const memRange gTRKMemMap[] = { { (u8*)0x00000000, (u8*)0xFFFFFFFF, TRUE, TRUE } };

ProcessorRestoreFlags_PPC gTRKRestoreFlags = { FALSE, FALSE };

static TRKExceptionStatus gTRKExceptionStatus = { { 0, 0, 0 }, TRUE, FALSE };
static TRKStepStatus gTRKStepStatus = { 0, DSSTEP_IntoCount, 0, 0, 0 };

ASM u32 __TRK_get_MSR(void) {
#ifdef __MWERKS__ // clang-format off
    nofralloc
    mfmsr r3
    blr
#endif // clang-format on
}

ASM void __TRK_set_MSR(register u32 val) {
#ifdef __MWERKS__ // clang-format off
    nofralloc
    mtmsr val
    blr
#endif // clang-format on
}

static DSError TRKValidMemory32(const void* addr, size_t length, int readWriteable) {
    DSError err = DS_InvalidMemory;
    const u8* start;
    const u8* end;
    int i;

    start = (const u8*)addr;
    end = ((const u8*)addr + (length - 1));

    if (end < start) {
        return DS_InvalidMemory;
    }

    for (i = 0; i < (int)(sizeof(gTRKMemMap) / sizeof(memRange)); i++) {
        if (start <= (const u8*)gTRKMemMap[i].end &&
            end >= (const u8*)gTRKMemMap[i].start)
        {
            if (((u8)readWriteable == VALIDMEM_Readable && !gTRKMemMap[i].readable) ||
                ((u8)readWriteable == VALIDMEM_Writeable && !gTRKMemMap[i].writeable))
            {
                err = DS_InvalidMemory;
            } else {
                err = DS_NoError;

                if (start < (const u8*)gTRKMemMap[i].start) {
                    err = TRKValidMemory32(
                        start, (u32)((const u8*)gTRKMemMap[i].start - start),
                        readWriteable);
                }

                if (err == DS_NoError && end > (const u8*)gTRKMemMap[i].end) {
                    err = TRKValidMemory32(
                        (const u8*)gTRKMemMap[i].end,
                        (u32)(end - (const u8*)gTRKMemMap[i].end), readWriteable);
                }
            }

            break;
        }
    }

    return err;
}

ASM static void TRK_ppc_memcpy(void* dest, const void* src, int n, u32 param_4, u32 param_5) {
#ifdef __MWERKS__ // clang-format off
    nofralloc

    mfmsr    r8
    li       r10, 0

loop:
    cmpw     r10, r5
    beq      end
    mtmsr    r7
    sync
    lbzx     r9, r10, r4
    mtmsr    r6
    sync
    stbx     r9, r10, r3
    addi     r10, r10, 1
    b        loop

end:
    mtmsr    r8
    sync
    blr
#endif // clang-format on
}

DSError TRKTargetAccessMemory(void* data, u32 start, size_t* length,
                              MemoryAccessOptions accessOptions, BOOL read) {
    DSError error;
    u32 uVar5;
    u32 addr;
    u32 param4;
    TRKExceptionStatus tempExceptionStatus;

    tempExceptionStatus = gTRKExceptionStatus;
    gTRKExceptionStatus.exceptionDetected = FALSE;

    addr = TRKTargetTranslate(start);
    error = TRKValidMemory32((void*)addr, *length,
                             read ? VALIDMEM_Readable : VALIDMEM_Writeable);

    if (error != DS_NoError) {
        *length = 0;
    } else {
        uVar5 = __TRK_get_MSR();
        param4 = uVar5 | (gTRKCPUState.Extended1.MSR & MSR_DR);

        if (read) {
            TRK_ppc_memcpy(data, (void*)addr, *length, uVar5, param4);
        } else {
            TRK_ppc_memcpy((void*)addr, data, *length, param4, uVar5);
            TRK_flush_cache(addr, *length);
            if (start != addr) {
                TRK_flush_cache(start, *length);
            }
        }
    }

    if (gTRKExceptionStatus.exceptionDetected) {
        *length = 0;
        error = DS_CWDSException;
    }

    gTRKExceptionStatus = tempExceptionStatus;
    return error;
}

static DSError TRKTargetReadInstruction(void* data, u32 start) {
    DSError error = DS_NoError;
    size_t registersLength = sizeof(InstructionType);

    error = TRKTargetAccessMemory(data, start, &registersLength, MEMACCESS_UserMemory, TRUE);

    if (error == DS_NoError && registersLength != sizeof(InstructionType)) {
        error = DS_InvalidMemory;
    }

    return error;
}

DSError TRKTargetAccessDefault(u32 firstRegister, u32 lastRegister, TRKBuffer* b,
                               size_t* registersLengthPtr, BOOL read) {
    DSError error;
    u32 count;
    u32* data;
    TRKExceptionStatus tempExceptionStatus;

    if (lastRegister > TRK_DEFAULT_XER) {
        return DS_InvalidRegister;
    }

    tempExceptionStatus = gTRKExceptionStatus;
    gTRKExceptionStatus.exceptionDetected = FALSE;

    data = gTRKCPUState.Default.GPR + firstRegister;
    count = (lastRegister - firstRegister) + 1;
    *registersLengthPtr = count * sizeof(DefaultType);

    if (read) {
        error = TRKAppendBuffer_ui32(b, data, count);
    } else {
        error = TRKReadBuffer_ui32(b, data, count);
    }

    if (gTRKExceptionStatus.exceptionDetected) {
        *registersLengthPtr = 0;
        error = DS_CWDSException;
    }

    gTRKExceptionStatus = tempExceptionStatus;
    return error;
}

DSError TRKTargetAccessFP(u32 firstRegister, u32 lastRegister, TRKBuffer* b,
                          size_t* registerStorageSize, BOOL read) {
    DSError err;
    u32 current;
    FloatType value;
    TRKExceptionStatus savedException;

    if (lastRegister > 33) {
        return DS_InvalidRegister;
    }

    savedException = gTRKExceptionStatus;
    gTRKExceptionStatus.exceptionDetected = 0;

    __TRK_set_MSR(__TRK_get_MSR() | 0x2000);

    current = firstRegister;
    *registerStorageSize = 0;
    err = DS_NoError;

    while (current <= lastRegister && err == DS_NoError) {
        if (read) {
            err = TRKPPCAccessFPRegister((void*)&value, current, read);
            err = TRKAppendBuffer1_ui64(b, value);
        } else {
            err = TRKReadBuffer1_ui64(b, &value);
            err = TRKPPCAccessFPRegister((void*)&value, current, read);
        }

        *registerStorageSize += sizeof(FloatType);
        current++;
    }

    if (gTRKExceptionStatus.exceptionDetected) {
        err = DS_CWDSException;
        *registerStorageSize = 0;
    }

    gTRKExceptionStatus = savedException;

    return err;
}

DSError TRKTargetAccessExtended1(u32 firstRegister, u32 lastRegister, TRKBuffer* b,
                                 size_t* registerStorageSize, BOOL read) {
    TRKExceptionStatus tempExceptionStatus;
    int error;
    u32* data;
    int count;

    if (lastRegister > 0x60) {
        return DS_InvalidRegister;
    }

    tempExceptionStatus = gTRKExceptionStatus;
    gTRKExceptionStatus.exceptionDetected = FALSE;

    *registerStorageSize = 0;

    if (firstRegister <= lastRegister) {
        data = (u32*)&gTRKCPUState.Extended1 + firstRegister;
        count = lastRegister - firstRegister + 1;
        *registerStorageSize += count * sizeof(u32);

        if (read) {
            error = TRKAppendBuffer_ui32(b, data, count);
        } else {
            if (data <= &gTRKCPUState.Extended1.TBU &&
                (data + count - 1) >= &gTRKCPUState.Extended1.TBL)
            {
                gTRKRestoreFlags.TBR = 1;
            }

            if (data <= &gTRKCPUState.Extended1.DEC &&
                (data + count - 1) >= &gTRKCPUState.Extended1.DEC)
            {
                gTRKRestoreFlags.DEC = 1;
            }
            error = TRKReadBuffer_ui32(b, data, count);
        }
    }
    if (gTRKExceptionStatus.exceptionDetected) {
        *registerStorageSize = 0;
        error = DS_CWDSException;
    }

    gTRKExceptionStatus = tempExceptionStatus;
    return error;
}

DSError TRKTargetAccessExtended2(u32 firstRegister, u32 lastRegister, TRKBuffer* b,
                                 size_t* registersLengthPtr, BOOL read) {
    u32 value_buf[2];
    TRKExceptionStatus savedException;
    u32 i;
    DSError err;
    u32 value_buf0[1];

    if (lastRegister > 31) {
        return DS_InvalidRegister;
    }

    savedException = gTRKExceptionStatus;
    gTRKExceptionStatus.exceptionDetected = FALSE;

    TRKPPCAccessSPR(value_buf0, SPR_HID2, TRUE);

    value_buf0[0] |= 0xA0000000;
    TRKPPCAccessSPR(value_buf0, SPR_HID2, FALSE);

    value_buf0[0] = 0;
    TRKPPCAccessSPR(value_buf0, SPR_GQR0, FALSE);

    *registersLengthPtr = 0;
    err = DS_NoError;

    for (i = firstRegister; (i <= lastRegister) && (err == DS_NoError); i++) {
        if (read) {
            err = TRKPPCAccessPairedSingleRegister((u64*)value_buf, i, read);
            err = TRKAppendBuffer1_ui64(b, *(u64*)value_buf);
        } else {
            err = TRKReadBuffer1_ui64(b, (u64*)value_buf);
            err = TRKPPCAccessPairedSingleRegister((u64*)value_buf, i, read);
        }

        *registersLengthPtr += sizeof(u64);
    }

    if (gTRKExceptionStatus.exceptionDetected) {
        *registersLengthPtr = 0;
        err = DS_CWDSException;
    }

    gTRKExceptionStatus = savedException;

    return err;
}

DSError TRKTargetVersions(DSVersions* version) {
    version->kernelMajor = 0;
    version->kernelMinor = 5;
    version->protocolMajor = 1;
    version->protocolMinor = 9;
    return DS_NoError;
}

DSError TRKTargetSupportMask(DSSupportMask* mask) {
    mask[0][0x00] = 0x7A;
    mask[0][0x01] = 0x00;
    mask[0][0x02] = 0x4F;
    mask[0][0x03] = 0x07;
    mask[0][0x04] = 0x00;
    mask[0][0x05] = 0x00;
    mask[0][0x06] = 0x00;
    mask[0][0x07] = 0x00;
    mask[0][0x08] = 0x00;
    mask[0][0x09] = 0x00;
    mask[0][0x0A] = 0x00;
    mask[0][0x0B] = 0x00;
    mask[0][0x0C] = 0x00;
    mask[0][0x0D] = 0x00;
    mask[0][0x0E] = 0x00;
    mask[0][0x0F] = 0x00;
    mask[0][0x10] = 0x01;
    mask[0][0x11] = 0x00;
    mask[0][0x12] = 0x03;
    mask[0][0x13] = 0x00;
    mask[0][0x14] = 0x00;
    mask[0][0x15] = 0x00;
    mask[0][0x16] = 0x00;
    mask[0][0x17] = 0x00;
    mask[0][0x18] = 0x00;
    mask[0][0x19] = 0x00;
    mask[0][0x1A] = 0x03;
    mask[0][0x1B] = 0x00;
    mask[0][0x1C] = 0x00;
    mask[0][0x1D] = 0x00;
    mask[0][0x1E] = 0x00;
    mask[0][0x1F] = 0x80;
    return DS_NoError;
}

DSError TRKTargetCPUType(DSCPUType* cpuType) {
    cpuType->cpuMajor = 0;
    cpuType->cpuMinor = TRKTargetCPUMinorType();
    cpuType->bigEndian = gTRKBigEndian;
    cpuType->defaultTypeSize = 4;
    cpuType->fpTypeSize = 8;
    cpuType->extended1TypeSize = 4;
    cpuType->extended2TypeSize = 8;
    return DS_NoError;
}

ASM void TRKInterruptHandler(register u16 val) {
#ifdef __MWERKS__ // clang-format off
    nofralloc
    mtsrr0 r2
    mtsrr1 r4
    mfsprg r4, 3
    mfcr r2
    mtsprg 3, r2
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    lwz r2, 0x8c(r2)
    ori r2, r2, 0x8002
    xori r2, r2, 0x8002
    sync
    mtmsr r2
    sync
    lis r2, TRK_saved_exceptionID@h
    ori r2, r2, TRK_saved_exceptionID@l
    sth r3, 0(r2)
    cmpwi r3, 0x500
    bne dispatchException
    lis r2, gTRKCPUState@h
    ori r2, r2, gTRKCPUState@l
    mflr r3
    stw r3, 0x42c(r2)
    bl TRKUARTInterruptHandler
    lis r2, gTRKCPUState@h
    ori r2, r2, gTRKCPUState@l
    lwz r3, 0x42c(r2)
    mtlr r3
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    lwz r2, 0xa0(r2)
    lbz r2, 0(r2)
    cmpwi r2, 0
    beq restoreAndReturn
    lis r2, gTRKExceptionStatus@h
    ori r2, r2, gTRKExceptionStatus@l
    lbz r2, 0xc(r2)
    cmpwi r2, 1
    beq restoreAndReturn
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    li r3, 1
    stb r3, 0x9c(r2)
    b dispatchException
restoreAndReturn:
    lis r2, gTRKSaveState@h
    ori r2, r2, gTRKSaveState@l
    lwz r3, 0x88(r2)
    mtcrf 0xff, r3
    lwz r3, 0xc(r2)
    lwz r2, 0x8(r2)
    rfi
dispatchException:
    lis r2, TRK_saved_exceptionID@h
    ori r2, r2, TRK_saved_exceptionID@l
    lhz r3, 0(r2)
    lis r2, gTRKExceptionStatus@h
    ori r2, r2, gTRKExceptionStatus@l
    lbz r2, 0xc(r2)
    cmpwi r2, 0
    bne TRKExceptionHandler
    lis r2, gTRKCPUState@h
    ori r2, r2, gTRKCPUState@l
    stw r0, 0x0(r2)
    stw r1, 0x4(r2)
    mfsprg r0, 1
    stw r0, 0x8(r2)
    sth r3, 0x2f8(r2)
    sth r3, 0x2fa(r2)
    mfsprg r0, 2
    stw r0, 0xc(r2)
    stmw r4, 0x10(r2)
    mfsrr0 r27
    mflr r28
    mfsprg r29, 3
    mfctr r30
    mfxer r31
    stmw r27, 0x80(r2)
    bl TRKSaveExtended1Block
    lis r2, gTRKExceptionStatus@h
    ori r2, r2, gTRKExceptionStatus@l
    li r3, 1
    stb r3, 0xc(r2)
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    lwz r0, 0x8c(r2)
    sync
    mtmsr r0
    sync
    lwz r0, 0x80(r2)
    mtlr r0
    lwz r0, 0x84(r2)
    mtctr r0
    lwz r0, 0x88(r2)
    mtxer r0
    lwz r0, 0x94(r2)
    mtspr 18, r0
    lwz r0, 0x90(r2)
    mtspr 19, r0
    lmw r3, 0xc(r2)
    lwz r0, 0x0(r2)
    lwz r1, 0x4(r2)
    lwz r2, 0x8(r2)
    b TRKPostInterruptEvent
#endif // clang-format on
}

ASM void TRKExceptionHandler(register u16 id) {
#ifdef __MWERKS__ // clang-format off
    nofralloc
    lis r2, gTRKExceptionStatus@h
    ori r2, r2, gTRKExceptionStatus@l
    sth r3, 0x8(r2)
    mfsrr0 r3
    stw r3, 0x0(r2)
    lhz r3, 0x8(r2)
    cmpwi r3, 0x200
    beq skipInstr
    cmpwi r3, 0x300
    beq skipInstr
    cmpwi r3, 0x400
    beq skipInstr
    cmpwi r3, 0x600
    beq skipInstr
    cmpwi r3, 0x700
    beq skipInstr
    cmpwi r3, 0x800
    beq skipInstr
    cmpwi r3, 0x1000
    beq skipInstr
    cmpwi r3, 0x1100
    beq skipInstr
    cmpwi r3, 0x1200
    beq skipInstr
    cmpwi r3, 0x1300
    beq skipInstr
    b setDetected
skipInstr:
    mfsrr0 r3
    addi r3, r3, 4
    mtsrr0 r3
setDetected:
    lis r2, gTRKExceptionStatus@h
    ori r2, r2, gTRKExceptionStatus@l
    li r3, 1
    stb r3, 0xd(r2)
    mfsprg r3, 3
    mtcrf 0xff, r3
    mfsprg r2, 1
    mfsprg r3, 2
    rfi
#endif // clang-format on
}

#define SUPPORT_TRAP 0x0FE00000

void TRKPostInterruptEvent(void) {
    NubEventType eventType;
    TRKEvent event;
    u32 inst;

    if (gTRKState.inputActivated) {
        gTRKState.inputActivated = FALSE;
        return;
    }

    switch ((u16)(gTRKCPUState.Extended1.exceptionID & 0xFFFF)) {
    case PPC_Program:
    case PPC_Trace:
        TRKTargetReadInstruction((void*)&inst, gTRKCPUState.Default.PC);

        if (inst == SUPPORT_TRAP) {
            eventType = NUBEVENT_Support;
        } else {
            eventType = NUBEVENT_Breakpoint;
        }
        break;
    default:
        eventType = NUBEVENT_Exception;
        break;
    }

    TRKConstructEvent(&event, eventType);
    TRKPostEvent(&event);
}

ASM void TRKSwapAndGo(void) {
#ifdef __MWERKS__ // clang-format off
    nofralloc
    lis r3, gTRKState@h
    ori r3, r3, gTRKState@l
    stmw r0, 0x0(r3)
    mfmsr r0
    stw r0, 0x8c(r3)
    mflr r0
    stw r0, 0x80(r3)
    mfctr r0
    stw r0, 0x84(r3)
    mfxer r0
    stw r0, 0x88(r3)
    mfspr r0, 18
    stw r0, 0x94(r3)
    mfspr r0, 19
    stw r0, 0x90(r3)
    addi r1, r0, 0x8002
    nor r1, r1, r1
    mfmsr r3
    and r3, r3, r1
    mtmsr r3
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    lwz r2, 0xa0(r2)
    lbz r2, 0(r2)
    cmpwi r2, 0
    beq noPendingInput
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    li r3, 1
    stb r3, 0x9c(r2)
    b TRKInterruptHandlerEnableInterrupts
noPendingInput:
    lis r2, gTRKExceptionStatus@h
    ori r2, r2, gTRKExceptionStatus@l
    li r3, 0
    stb r3, 0xc(r2)
    bl TRKRestoreExtended1Block
    lis r2, gTRKCPUState@h
    ori r2, r2, gTRKCPUState@l
    lmw r27, 0x80(r2)
    mtspr 26, r27
    mtlr r28
    mtcrf 0xff, r29
    mtctr r30
    mtxer r31
    lmw r3, 0xc(r2)
    lwz r0, 0x0(r2)
    lwz r1, 0x4(r2)
    lwz r2, 0x8(r2)
    rfi
#endif // clang-format on
}

ASM void TRKInterruptHandlerEnableInterrupts(void) {
#ifdef __MWERKS__ // clang-format off
    nofralloc
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    lwz r0, 0x8c(r2)
    sync
    mtmsr r0
    sync
    lwz r0, 0x80(r2)
    mtlr r0
    lwz r0, 0x84(r2)
    mtctr r0
    lwz r0, 0x88(r2)
    mtxer r0
    lwz r0, 0x94(r2)
    mtspr 18, r0
    lwz r0, 0x90(r2)
    mtspr 19, r0
    lmw r3, 0xc(r2)
    lwz r0, 0x0(r2)
    lwz r1, 0x4(r2)
    lwz r2, 0x8(r2)
    b TRKPostInterruptEvent
#endif // clang-format on
}

DSError TRKTargetInterrupt(TRKEvent* event) {
    DSError error = DS_NoError;

    switch (event->eventType) {
    case NUBEVENT_Breakpoint:
    case NUBEVENT_Exception:
        if (TRKTargetCheckStep() == FALSE) {
            TRKTargetSetStopped(TRUE);
            error = TRKDoNotifyStopped(DSMSG_NotifyStopped);
        }
        break;
    default:
        break;
    }

    return error;
}

DSError TRKTargetAddStopInfo(TRKBuffer* arg0) {
    DSError error;
    s32 data;

    error = TRKAppendBuffer1_ui32(arg0, gTRKCPUState.Default.PC);

    if (error == DS_NoError) {
        error = TRKTargetReadInstruction(&data, gTRKCPUState.Default.PC);
    }

    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui32(arg0, data);
    }

    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui16(arg0, gTRKCPUState.Extended1.exceptionID & 0xFFFF);
    }

    return error;
}

void TRKTargetAddExceptionInfo(TRKBuffer* b) {
    DSError error;
    s32 data;

    error = TRKAppendBuffer1_ui32(b, gTRKExceptionStatus.exceptionInfo.PC);

    if (error == DS_NoError) {
        error = TRKTargetReadInstruction(&data, gTRKExceptionStatus.exceptionInfo.PC);
    }

    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui32(b, data);
    }

    if (error == DS_NoError) {
        TRKAppendBuffer1_ui16(b, gTRKExceptionStatus.exceptionInfo.exceptionID);
    }
}

static DSError TRKTargetEnableTrace(BOOL enable) {
    if (enable) {
        gTRKCPUState.Extended1.MSR |= 0x400;
    } else {
        gTRKCPUState.Extended1.MSR &= ~0x400;
    }

    return DS_NoError;
}

BOOL TRKTargetStepDone(void) {
    BOOL result = TRUE;

    if (gTRKStepStatus.active &&
        ((u16)gTRKCPUState.Extended1.exceptionID) == PPC_Trace)
    {
        switch (gTRKStepStatus.type) {
        case DSSTEP_IntoCount:
            if (gTRKStepStatus.count > 0) {
                result = FALSE;
            }
            break;
        case DSSTEP_IntoRange:
            if (gTRKCPUState.Default.PC >= gTRKStepStatus.rangeStart &&
                gTRKCPUState.Default.PC <= gTRKStepStatus.rangeEnd)
            {
                result = FALSE;
            }
            break;
        default:
            break;
        }
    }

    return result;
}

DSError TRKTargetDoStep(void) {
    DSError err = DS_NoError;
    gTRKStepStatus.active = 1;
    TRKTargetEnableTrace(TRUE);

    if ((gTRKStepStatus.type == DSSTEP_IntoCount) ||
        (gTRKStepStatus.type == DSSTEP_OverCount))
    {
        gTRKStepStatus.count--;
    }

    TRKTargetSetStopped(FALSE);

    return err;
}

static BOOL TRKTargetCheckStep(void) {
    if (gTRKStepStatus.active) {
        TRKTargetEnableTrace(FALSE);

        if (TRKTargetStepDone()) {
            gTRKStepStatus.active = FALSE;
        } else {
            TRKTargetDoStep();
        }
    }

    return gTRKStepStatus.active;
}

DSError TRKTargetSingleStep(u32 count, BOOL stepOver) {
    DSError err = DS_NoError;

    if (stepOver) {
        err = DS_UnsupportedError;
    } else {
        gTRKStepStatus.type = DSSTEP_IntoCount;
        gTRKStepStatus.count = count;

        err = TRKTargetDoStep();
    }

    return err;
}

DSError TRKTargetStepOutOfRange(u32 rangeStart, u32 rangeEnd, BOOL stepOver) {
    DSError error = DS_NoError;

    if (stepOver) {
        error = DS_UnsupportedError;
    } else {
        gTRKStepStatus.type = DSSTEP_IntoRange;
        gTRKStepStatus.rangeStart = rangeStart;
        gTRKStepStatus.rangeEnd = rangeEnd;

        error = TRKTargetDoStep();
    }

    return error;
}

u32 TRKTargetGetPC(void) {
    return gTRKCPUState.Default.PC;
}

DSError TRKTargetSupportRequest(void) {
    DSError err = DS_NoError;
    size_t* length;
    TRKEvent event;
    u8 io_result;
    u8 command;

    command = (MessageCommandID)gTRKCPUState.Default.GPR[3];

    if ((command != DSMSG_ReadFile) && (command != DSMSG_WriteFile)) {
        TRKConstructEvent(&event, NUBEVENT_Exception);
        TRKPostEvent(&event);
        return err;
    }

    length = (size_t*)gTRKCPUState.Default.GPR[5];

    err = TRKSuppAccessFile((u8)gTRKCPUState.Default.GPR[4],
                            (u8*)gTRKCPUState.Default.GPR[6], length, &io_result, 1,
                            (command == DSMSG_ReadFile));

    if ((io_result == DS_IONoError) && (err != DS_NoError)) {
        io_result = DS_IOError;
    }

    gTRKCPUState.Default.GPR[3] = (DefaultType)io_result;

    if (command == DSMSG_ReadFile) {
        TRK_flush_cache((u32)gTRKCPUState.Default.GPR[6], *length);
    }

    gTRKCPUState.Default.PC += 4;
    return err;
}

DSError TRKTargetFlushCache(u8 arg0, u32 arg1, u32 arg2) {
    if (arg1 < arg2) {
        TRK_flush_cache(arg1, arg2 - arg1);
        return DS_NoError;
    }

    return DS_InvalidMemory;
}

BOOL TRKTargetStopped(void) {
    return gTRKState.isStopped;
}

void TRKTargetSetStopped(unsigned int val) {
    gTRKState.isStopped = val;
}

DSError TRKTargetStop(void) {
    TRKTargetSetStopped(TRUE);
    return DS_NoError;
}

DSError TRKPPCAccessSPR(void* srcDestPtr, u32 spr, BOOL read) {
    InstructionType instructionData[] = { INSTR_NOP, INSTR_NOP, INSTR_NOP,
                                          INSTR_NOP, INSTR_NOP };

    if (read) {
        instructionData[0] = INSTR_MFSPR(4, spr);
        instructionData[1] = INSTR_STW(4, 0, 3);
    } else {
        instructionData[0] = INSTR_LWZ(4, 0, 3);
        instructionData[1] = INSTR_MTSPR(spr, 4);
    }

    return TRKPPCAccessSpecialReg(srcDestPtr, instructionData, read);
}

DSError TRKPPCAccessPairedSingleRegister(void* srcDestPtr, u32 psr, BOOL read) {
    InstructionType instructionData[] = { INSTR_NOP, INSTR_NOP, INSTR_NOP,
                                          INSTR_NOP, INSTR_NOP };

    if (read) {
        instructionData[0] = INSTR_PSQ_ST(psr, 0, 3, 0, 0);
    } else {
        instructionData[0] = INSTR_PSQ_L(psr, 0, 3, 0, 0);
    }

    return TRKPPCAccessSpecialReg(srcDestPtr, instructionData, read);
}

#define FP_FPSCR_ACCESS 32
#define FP_FPECR_ACCESS 33

DSError TRKPPCAccessFPRegister(void* srcDestPtr, u32 fpr, BOOL read) {
    DSError error = DS_NoError;
    InstructionType instructionData1[] = { INSTR_NOP, INSTR_NOP, INSTR_NOP,
                                           INSTR_NOP, INSTR_NOP };

    if (fpr < FP_FPSCR_ACCESS) {
        if (read) {
            instructionData1[0] = INSTR_STFD(fpr, 0, 3);
        } else {
            instructionData1[0] = INSTR_LFD(fpr, 0, 3);
        }

        error = TRKPPCAccessSpecialReg(srcDestPtr, instructionData1, read);
    } else if (fpr == FP_FPSCR_ACCESS) {
        if (read) {
            instructionData1[0] = 0xD8240000;
            instructionData1[1] = 0xFC20048E;
            instructionData1[2] = 0xD8230000;
            instructionData1[3] = 0xC8240000;
        } else {
            instructionData1[0] = 0xD8240000;
            instructionData1[1] = 0xD8230000;
            instructionData1[2] = 0xFDFE0D8E;
            instructionData1[3] = 0xC8240000;
        }

        error = TRKPPCAccessSpecialReg(srcDestPtr, instructionData1, read);
        *(u64*)srcDestPtr &= 0xFFFFFFFF;
    } else if (fpr == FP_FPECR_ACCESS) {
        if (!read) {
            *(u32*)srcDestPtr = *(u32*)((u32)srcDestPtr + 4);
        }

        error = TRKPPCAccessSPR(srcDestPtr, SPR_FPECR, read);

        if (read) {
            DSFetch_u64(srcDestPtr) = DSFetch_u32(srcDestPtr) & 0xFFFFFFFFLL;
        }
    }

    return error;
}

DSError TRKPPCAccessSpecialReg(void* srcDestPtr, u32* instructionData, BOOL read) {
    instructionData[4] = INSTR_BLR;
    TRK_flush_cache((u32)instructionData, 0x14);
    ((RegAccessFunc)instructionData)(srcDestPtr, &TRKvalue128_temp);
    return DS_NoError;
}

void TRKTargetSetInputPendingPtr(void* ptr) {
    gTRKState.inputPendingPtr = ptr;
}
