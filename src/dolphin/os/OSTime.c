#include "dolphin/types.h"

int OSDisableInterrupts(void);
void OSRestoreInterrupts(int enabled);

/* clang-format off */
ASM s64 OSGetTime(void) {
#ifdef __MWERKS__
jump:
    nofralloc

    mftbu r3
    mftb r4

    // Check for possible carry from TBL to TBU
    mftbu r5
    cmpw r3, r5
    bne jump

    blr
#endif
}

ASM u32 OSGetTick(void) {
#ifdef __MWERKS__
    nofralloc

    mftb r3
    blr
#endif
}
/* clang-format on */

s64 __OSGetSystemTime(void) {
    int enabled;
    s64* timeAdjustAddr;
    s64 result;

    timeAdjustAddr = (s64*)0x800030D8;
    enabled = OSDisableInterrupts();

    result = OSGetTime() + *timeAdjustAddr;
    OSRestoreInterrupts(enabled);
    return result;
}
