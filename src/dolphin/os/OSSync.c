#include "types.h"

void* memcpy(void* dst, const void* src, u32 n);
void DCFlushRangeNoSync(void* addr, u32 nBytes);
void ICInvalidateRange(void* addr, u32 nBytes);

#define __sync() asm { sync }

void __OSSystemCallVectorStart(void);
void __OSSystemCallVectorEnd(void);

/* clang-format off */
ASM static void SystemCallVector(void) {
#ifdef __MWERKS__
    nofralloc

entry __OSSystemCallVectorStart
    mfspr r9, HID0
    ori r10, r9, 0x8
    mtspr HID0, r10
    isync
    sync
    mtspr HID0, r9
    rfi
entry __OSSystemCallVectorEnd
    nop
#endif
}
/* clang-format on */

void __OSInitSystemCall(void) {
    void* addr = (void*)0x80000C00;
    memcpy(addr, (void*)__OSSystemCallVectorStart,
           (u32)__OSSystemCallVectorEnd - (u32)__OSSystemCallVectorStart);
    DCFlushRangeNoSync(addr, 0x100);
    __sync();
    ICInvalidateRange(addr, 0x100);
}
