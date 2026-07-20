#include "trk.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/nubinit.h"

// Common BSS; definition owned by the auto-split .bss object.
extern int TRK_mainError;

DSError TRKInitializeNub(void);
void TRKNubWelcome(void);
void TRKNubMainLoop(void);
DSError TRKTerminateNub(void);

u8 TRKTargetCPUMinorType(void) {
    return 0x54;
}

int TRK_main(void) {
    TRK_mainError = TRKInitializeNub();

    if (!TRK_mainError) {
        TRKNubWelcome();
        TRKNubMainLoop();
    }

    return TRK_mainError = TRKTerminateNub();
}
