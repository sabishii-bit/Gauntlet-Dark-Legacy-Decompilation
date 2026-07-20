#include "trk.h"

void __TRK_copy_vectors(void);
void TRK_copy_vector(u32 offset);
u32 TRKTargetTranslate(u32 addr);
void EnableEXI2Interrupts(void);
u32 __TRK_get_MSR(void);
void TRKSaveExtended1Block(void);
int InitMetroTRKCommTable(int hwId);
int TRK_main(void);
void* TRK_memcpy(void* dst, const void* src, size_t n);
void TRK_flush_cache(u32 addr, u32 length);

extern u8 gTRKInterruptVectorTable[];
extern TRKState_PPC gTRKState;
extern char gTRKCPUState[];
extern char _db_stack_addr[];

#define EXCEPTIONMASK_ADDR 0x44

static u32 TRK_ISR_OFFSETS[15] = {
    0x100,  0x200,  0x300,  0x400,  0x500,  0x600,  0x700, 0x800,
    0x900,  0xC00,  0xD00,  0xF00,  0x1300, 0x1400, 0x1700,
};

DECL_SECT(".init") void __TRK_reset(void) {
    __TRK_copy_vectors();
}

ASM void InitMetroTRK(void) {
#ifdef __MWERKS__ // clang-format off
    nofralloc

    addi r1, r1, -4
    stw r3, 0(r1)
    lis r3, gTRKCPUState@h
    ori r3, r3, gTRKCPUState@l
    stmw r0, 0(r3)
    lwz r4, 0(r1)
    addi r1, r1, 4
    stw r1, 4(r3)
    stw r4, 0xc(r3)
    mflr r4
    stw r4, 0x84(r3)
    stw r4, 0x80(r3)
    mfcr r4
    stw r4, 0x88(r3)
    mfmsr r4
    ori r3, r4, 0x8000
    xori r3, r3, 0x8000
    mtmsr r3
    mtsrr1 r4
    bl TRKSaveExtended1Block
    lis r3, gTRKCPUState@h
    ori r3, r3, gTRKCPUState@l
    lmw r0, 0(r3)
    li r0, 0
    mtspr 0x3f2, r0
    mtspr 0x3f5, r0
    lis r1, _db_stack_addr@h
    ori r1, r1, _db_stack_addr@l
    mr r3, r5
    bl InitMetroTRKCommTable
    cmpwi r3, 1
    bne initCommTableSuccess
    lwz r4, 0x84(r3)
    mtlr r4
    lmw r0, 0(r3)
    blr
initCommTableSuccess:
    b TRK_main
#endif // clang-format on
}

void EnableMetroTRKInterrupts(void) {
    EnableEXI2Interrupts();
}

u32 TRKTargetTranslate(u32 addr) {
    return addr & 0x3FFFFFFF | 0x80000000;
}

void TRK_copy_vector(u32 offset) {
    void* destPtr = (void*)TRKTargetTranslate(offset);
    TRK_memcpy(destPtr, (void*)(gTRKInterruptVectorTable + offset), 0x100);
    TRK_flush_cache((u32)destPtr, 0x100);
}

void __TRK_copy_vectors(void) {
    int i;
    u32 mask;

    mask = *(u32*)TRKTargetTranslate(EXCEPTIONMASK_ADDR);

    for (i = 0; i <= 14; i++) {
        if (mask & (1 << i)) {
            TRK_copy_vector(TRK_ISR_OFFSETS[i]);
        }
    }
}

DSError TRKInitializeTarget(void) {
    gTRKState.stopped = TRUE;
    gTRKState.MSR = __TRK_get_MSR();
    return DS_NoError;
}
