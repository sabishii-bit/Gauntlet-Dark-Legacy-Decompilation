#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msgbuf.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/nubinit.h"

// Defined as common BSS in the original; definition currently owned by the
// auto-split .bss object (see common_bss notes). Reference as extern here.
extern TRKMsgBufs gTRKMsgBufs;
extern BOOL gTRKBigEndian;

DSError TRK_WriteUARTN(const void* data, u32 length);
void* TRK_memcpy(void* dst, const void* src, size_t n);

static void TRKSetBufferUsed(TRKBuffer* b, BOOL state) {
    b->isInUse = state;
}

DSError TRKInitializeMessageBuffers(void) {
    int i;

    for (i = 0; i < NUM_BUFFERS; i++) {
        TRKInitializeMutex(&gTRKMsgBufs.buffers[i]);
        TRKAcquireMutex(&gTRKMsgBufs.buffers[i]);
        TRKSetBufferUsed(&gTRKMsgBufs.buffers[i], FALSE);
        TRKReleaseMutex(&gTRKMsgBufs.buffers[i]);
    }

    return DS_NoError;
}

DSError TRKGetFreeBuffer(int* bufferIndexPtr, TRKBuffer** destBufPtr) {
    DSError error = DS_NoMessageBufferAvailable;
    int i;
    *destBufPtr = NULL;

    for (i = 0; i < NUM_BUFFERS; i++) {
        TRKBuffer* buf = TRKGetBuffer(i);

        TRKAcquireMutex(buf);

        if (!buf->isInUse) {
            TRKResetBuffer(buf, 1);
            TRKSetBufferUsed(buf, TRUE);
            *destBufPtr = buf;
            *bufferIndexPtr = i;
            error = DS_NoError;
            i = NUM_BUFFERS;
        }

        TRKReleaseMutex(buf);
    }

    return error;
}

TRKBuffer* TRKGetBuffer(int index) {
    TRKBuffer* buf = NULL;

    if (index >= 0 && index < NUM_BUFFERS) {
        buf = &gTRKMsgBufs.buffers[index];
    }

    return buf;
}

void TRKReleaseBuffer(int index) {
    TRKBuffer* b;

    if (index == -1) {
        return;
    }

    if (index >= 0 && index < NUM_BUFFERS) {
        b = &gTRKMsgBufs.buffers[index];
        TRKAcquireMutex(b);
        TRKSetBufferUsed(b, 0);
        TRKReleaseMutex(b);
    }
}

void TRKResetBuffer(TRKBuffer* buf, u8 keepData) {
    buf->length = 0;
    buf->position = 0;

    if (!keepData) {
        TRK_memcpy(buf->data, 0, kMessageBufferSize);
    }
}

DSError TRKSetBufferPosition(TRKBuffer* buf, u32 pos) {
    DSError error = DS_NoError;

    if (pos > kMessageBufferSize) {
        error = DS_MessageBufferOverflow;
    } else {
        buf->position = pos;
        if (pos > buf->length) {
            buf->length = pos;
        }
    }

    return error;
}

DSError TRKAppendBuffer(TRKBuffer* buf, const void* data, size_t length) {
    DSError error = DS_NoError;
    u32 bytesLeft;

    if (length == 0) {
        return DS_NoError;
    }

    bytesLeft = kMessageBufferSize - buf->position;

    if (bytesLeft < length) {
        error = DS_MessageBufferOverflow;
        length = bytesLeft;
    }

    if (length == 1) {
        buf->data[buf->position] = ((u8*)data)[0];
    } else {
        TRK_memcpy(buf->data + buf->position, data, length);
    }

    buf->position += length;
    buf->length = buf->position;

    return error;
}

DSError TRKReadBuffer(TRKBuffer* buf, void* data, size_t length) {
    DSError error = DS_NoError;
    unsigned int bytesLeft;

    if (length == 0) {
        return DS_NoError;
    }

    bytesLeft = buf->length - buf->position;

    if (length > bytesLeft) {
        error = DS_MessageBufferReadError;
        length = bytesLeft;
    }

    TRK_memcpy(data, buf->data + buf->position, length);
    buf->position += length;
    return error;
}

DSError TRKAppendBuffer1_ui16(TRKBuffer* buffer, const u16 data) {
    u8* bigEndianData;
    u8* byteData;
    u8 swapBuffer[sizeof(data)];

    if (gTRKBigEndian) {
        bigEndianData = (u8*)&data;
    } else {
        byteData = (u8*)&data;
        bigEndianData = swapBuffer;

        bigEndianData[0] = byteData[1];
        bigEndianData[1] = byteData[0];
    }

    return TRKAppendBuffer(buffer, (const void*)bigEndianData, sizeof(data));
}

DSError TRKAppendBuffer1_ui32(TRKBuffer* buffer, const u32 data) {
    u8* bigEndianData;
    u8* byteData;
    u8 swapBuffer[sizeof(data)];

    if (gTRKBigEndian) {
        bigEndianData = (u8*)&data;
    } else {
        byteData = (u8*)&data;
        bigEndianData = swapBuffer;

        bigEndianData[0] = byteData[3];
        bigEndianData[1] = byteData[2];
        bigEndianData[2] = byteData[1];
        bigEndianData[3] = byteData[0];
    }

    return TRKAppendBuffer(buffer, (const void*)bigEndianData, sizeof(data));
}

DSError TRKAppendBuffer1_ui64(TRKBuffer* buffer, const u64 data) {
    u8* bigEndianData;
    u8* byteData;
    u8 swapBuffer[sizeof(data)];

    if (gTRKBigEndian) {
        bigEndianData = (u8*)&data;
    } else {
        byteData = (u8*)&data;
        bigEndianData = swapBuffer;

        bigEndianData[0] = byteData[7];
        bigEndianData[1] = byteData[6];
        bigEndianData[2] = byteData[5];
        bigEndianData[3] = byteData[4];
        bigEndianData[4] = byteData[3];
        bigEndianData[5] = byteData[2];
        bigEndianData[6] = byteData[1];
        bigEndianData[7] = byteData[0];
    }

    return TRKAppendBuffer(buffer, (const void*)bigEndianData, sizeof(data));
}

DSError TRKAppendBuffer_ui8(TRKBuffer* buffer, const u8* data, int count) {
    DSError err;
    int i;

    for (i = 0, err = DS_NoError; err == DS_NoError && i < count; i++) {
        err = TRKAppendBuffer1_ui8(buffer, data[i]);
    }

    return err;
}

DSError TRKAppendBuffer_ui32(TRKBuffer* buffer, const u32* data, int count) {
    DSError err;
    int i;

    for (i = 0, err = DS_NoError; err == DS_NoError && i < count; i++) {
        err = TRKAppendBuffer1_ui32(buffer, data[i]);
    }

    return err;
}

DSError TRKReadBuffer1_ui8(TRKBuffer* buffer, u8* data) {
    return TRKReadBuffer(buffer, (void*)data, 1);
}

DSError TRKReadBuffer1_ui16(TRKBuffer* buffer, u16* data) {
    DSError err;

    u8* bigEndianData;
    u8* byteData;
    u8 swapBuffer[sizeof(data)];

    if (gTRKBigEndian) {
        bigEndianData = (u8*)data;
    } else {
        bigEndianData = swapBuffer;
    }

    err = TRKReadBuffer(buffer, (void*)bigEndianData, sizeof(*data));

    if (!gTRKBigEndian && err == DS_NoError) {
        byteData = (u8*)data;

        byteData[0] = bigEndianData[1];
        byteData[1] = bigEndianData[0];
    }

    return err;
}

DSError TRKReadBuffer1_ui32(TRKBuffer* buffer, u32* data) {
    DSError err;

    u8* bigEndianData;
    u8* byteData;
    u8 swapBuffer[sizeof(data)];

    if (gTRKBigEndian) {
        bigEndianData = (u8*)data;
    } else {
        bigEndianData = swapBuffer;
    }

    err = TRKReadBuffer(buffer, (void*)bigEndianData, sizeof(*data));

    if (!gTRKBigEndian && err == DS_NoError) {
        byteData = (u8*)data;

        byteData[0] = bigEndianData[3];
        byteData[1] = bigEndianData[2];
        byteData[2] = bigEndianData[1];
        byteData[3] = bigEndianData[0];
    }

    return err;
}

DSError TRKReadBuffer1_ui64(TRKBuffer* buffer, u64* data) {
    DSError err;

    u8* bigEndianData;
    u8* byteData;
    u8 swapBuffer[sizeof(data)];

    if (gTRKBigEndian) {
        bigEndianData = (u8*)data;
    } else {
        bigEndianData = swapBuffer;
    }

    err = TRKReadBuffer(buffer, (void*)bigEndianData, sizeof(*data));

    if (!gTRKBigEndian && err == 0) {
        byteData = (u8*)data;

        byteData[0] = bigEndianData[7];
        byteData[1] = bigEndianData[6];
        byteData[2] = bigEndianData[5];
        byteData[3] = bigEndianData[4];
        byteData[4] = bigEndianData[3];
        byteData[5] = bigEndianData[2];
        byteData[6] = bigEndianData[1];
        byteData[7] = bigEndianData[0];
    }

    return err;
}

DSError TRKReadBuffer_ui8(TRKBuffer* buffer, u8* data, int count) {
    DSError err;
    int i;

    for (i = 0, err = DS_NoError; err == DS_NoError && i < count; i++) {
        err = TRKReadBuffer1_ui8(buffer, &(data[i]));
    }

    return err;
}

DSError TRKReadBuffer_ui32(TRKBuffer* buffer, u32* data, int count) {
    DSError err;
    int i;

    for (i = 0, err = DS_NoError; err == DS_NoError && i < count; i++) {
        err = TRKReadBuffer1_ui32(buffer, &(data[i]));
    }

    return err;
}
