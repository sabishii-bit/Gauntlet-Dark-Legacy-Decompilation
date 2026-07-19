#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/serpoll.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msgbuf.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/nubevent.h"

typedef struct FramingState {
    /* 0x00 */ MessageBufferID fBufferID;
    /* 0x04 */ TRKBuffer* fBuffer;
    /* 0x08 */ u8 fReceiveState;
    /* 0x0C */ int fEscape;
    /* 0x10 */ u8 fFCS;
} FramingState;

// Common BSS; definitions owned by the auto-split .bss object.
extern void* gTRKInputPendingPtr;
extern FramingState gTRKFramingState;

int TRKPollUART(void);
DSError TRK_ReadUARTN(void* data, u32 length);
DSError TRKStandardACK(TRKBuffer* buf, int commandReplyError, int fcsType);
DSError TRKReadBuffer1_ui8(TRKBuffer* buffer, u8* data);
void TRKProcessInput(MessageBufferID bufID);

MessageBufferID TRKTestForPacket(void) {
    int bytes;
    int batch;
    DSError err;
    TRKBuffer* b;
    int id;

    bytes = TRKPollUART();

    if (bytes > 0) {
        TRKGetFreeBuffer(&id, &b);
        if (bytes > 0x880) {
            for (; bytes > 0; bytes -= batch) {
                batch = bytes > 0x880 ? 0x880 : bytes;
                TRK_ReadUARTN(b->data, batch);
            }
            TRKStandardACK(b, 0xFF, 6);
        } else {
            err = TRK_ReadUARTN(b->data, bytes);
            if (err == DS_NoError) {
                b->length = bytes;
                return id;
            }
        }
    }

    if (id != -1) {
        TRKReleaseBuffer(id);
    }
    return -1;
}

void TRKGetInput(void) {
    TRKBuffer* msgbuffer;
    MessageBufferID bufID;
    u8 command;

    bufID = TRKTestForPacket();

    if (bufID != -1) {
        msgbuffer = TRKGetBuffer(bufID);
        TRKSetBufferPosition(msgbuffer, 0);
        TRKReadBuffer1_ui8(msgbuffer, &command);
        if (command < 0x80) {
            TRKProcessInput(bufID);
        } else {
            TRKReleaseBuffer(bufID);
        }
    }
}

void TRKProcessInput(MessageBufferID bufID) {
    TRKEvent event;

    TRKConstructEvent(&event, 2);
    event.msgBufID = bufID;
    gTRKFramingState.fBufferID = -1;
    TRKPostEvent(&event);
}

DSError TRKInitializeSerialHandler(void) {
    gTRKFramingState.fBufferID = -1;
    gTRKFramingState.fReceiveState = 0;
    gTRKFramingState.fEscape = 0;
    return DS_NoError;
}

DSError TRKTerminateSerialHandler(void) {
    return DS_NoError;
}
