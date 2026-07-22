#include "types.h"
#include "dolphin/os/OSContext.h"

typedef u8 __OSException;

void OSReport(const char* msg, ...);
void OSDumpContext(OSContext* context);
void PPCHalt(void);

typedef struct DBInterface {
    /* 0x0 */ u32 bPresent;
    /* 0x4 */ u32 exceptionMask;
    /* 0x8 */ u32 ExceptionDestination;
    /* 0xC */ u32 exceptionReturn;
} DBInterface;

// Tentative globals link in reverse declaration order (-common):
// __DBInterface ends up at 0x80345488, DBVerbose at 0x8034548C.
BOOL DBVerbose;
DBInterface* __DBInterface;

#define OSPhysicalToCached(paddr) ((void*)((u32)(paddr) + 0x80000000))
#define OSCachedToPhysical(caddr) ((u32)(caddr)-0x80000000)

void __DBExceptionDestinationAux(void);
void __DBExceptionDestination(void);

void DBInit(void) {
    __DBInterface = OSPhysicalToCached(0x40);
    __DBInterface->ExceptionDestination = OSCachedToPhysical(__DBExceptionDestination);
    DBVerbose = TRUE;
}

BOOL DBIsDebuggerPresent(void) {
    if (__DBInterface == NULL) {
        return FALSE;
    }
    return __DBInterface->bPresent;
}

void __DBExceptionDestinationAux(void) {
    u32* contextAddr;
    OSContext* context;

    contextAddr = (void*)0xC0;
    context = OSPhysicalToCached(*contextAddr);
    OSReport("DBExceptionDestination\n");
    OSDumpContext(context);
    PPCHalt();
}

asm void __DBExceptionDestination(void) {
    // clang-format off
    nofralloc
    mfmsr r3
    ori r3, r3, 0x30
    mtmsr r3
    b __DBExceptionDestinationAux
    // clang-format on
}

BOOL __DBIsExceptionMarked(__OSException exception) {
    u32 mask = (1 << exception);
    return __DBInterface->exceptionMask & mask;
}

void __DBMarkException(__OSException exception, BOOL value) {
    u32 mask = (1 << exception);

    if (value) {
        __DBInterface->exceptionMask |= mask;
    } else {
        __DBInterface->exceptionMask &= ~mask;
    }
}

void __DBSetPresent(u32 value) {
    __DBInterface->bPresent = value;
}

void DBPrintf(char* format, ...) {
}
