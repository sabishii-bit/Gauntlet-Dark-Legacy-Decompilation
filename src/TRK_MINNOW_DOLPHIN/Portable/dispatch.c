#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msgbuf.h"

typedef DSError (*DispatchCallback)(TRKBuffer* buf);

DSError TRKDoUnsupported(TRKBuffer* buf);
DSError TRKDoConnect(TRKBuffer* buf);
DSError TRKDoDisconnect(TRKBuffer* buf);
DSError TRKDoReset(TRKBuffer* buf);
DSError TRKDoVersions(TRKBuffer* buf);
DSError TRKDoSupportMask(TRKBuffer* buf);
DSError TRKDoCPUType(TRKBuffer* buf);
DSError TRKDoReadMemory(TRKBuffer* buf);
DSError TRKDoWriteMemory(TRKBuffer* buf);
DSError TRKDoReadRegisters(TRKBuffer* buf);
DSError TRKDoWriteRegisters(TRKBuffer* buf);
DSError TRKDoFlushCache(TRKBuffer* buf);
DSError TRKDoContinue(TRKBuffer* buf);
DSError TRKDoStep(TRKBuffer* buf);
DSError TRKDoStop(TRKBuffer* buf);

// Common BSS; definition owned by the auto-split .bss object.
extern u32 gTRKDispatchTableSize;

DispatchCallback gTRKDispatchTable[] = {
    TRKDoUnsupported,
    TRKDoConnect,
    TRKDoDisconnect,
    TRKDoReset,
    TRKDoVersions,
    TRKDoSupportMask,
    TRKDoCPUType,
    TRKDoUnsupported,
    TRKDoUnsupported,
    TRKDoUnsupported,
    TRKDoUnsupported,
    TRKDoUnsupported,
    TRKDoUnsupported,
    TRKDoUnsupported,
    TRKDoUnsupported,
    TRKDoUnsupported,
    TRKDoReadMemory,
    TRKDoWriteMemory,
    TRKDoReadRegisters,
    TRKDoWriteRegisters,
    TRKDoUnsupported,
    TRKDoUnsupported,
    TRKDoFlushCache,
    TRKDoUnsupported,
    TRKDoContinue,
    TRKDoStep,
    TRKDoStop,
    TRKDoUnsupported,
    TRKDoUnsupported,
    TRKDoUnsupported,
    TRKDoUnsupported,
    TRKDoUnsupported,
    NULL,
    NULL,
};

DSError TRKInitializeDispatcher(void) {
    gTRKDispatchTableSize = 32;
    return DS_NoError;
}

DSError TRKDispatchMessage(TRKBuffer* buffer) {
    DSError result = DS_DispatchError;
    u8 command;

    TRKSetBufferPosition(buffer, 0);
    TRKReadBuffer1_ui8(buffer, &command);

    if (command < gTRKDispatchTableSize) {
        result = gTRKDispatchTable[command](buffer);
    }

    return result;
}
