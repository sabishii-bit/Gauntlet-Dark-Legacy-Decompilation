#ifndef METROTRK_PORTABLE_MSGBUF_H
#define METROTRK_PORTABLE_MSGBUF_H

#include "trk.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kMessageBufferSize TRKMSGBUF_SIZE
#define NUM_BUFFERS 3

typedef struct TRKMsgBufs {
    TRKBuffer buffers[NUM_BUFFERS];
} TRKMsgBufs;

extern TRKMsgBufs gTRKMsgBufs;

DSError TRKInitializeMessageBuffers(void);
DSError TRKGetFreeBuffer(int* bufferIndexPtr, TRKBuffer** destBufPtr);
void TRKReleaseBuffer(int index);
void TRKResetBuffer(TRKBuffer* buf, u8 keepData);
DSError TRKAppendBuffer(TRKBuffer* buf, const void* data, size_t length);
DSError TRKReadBuffer(TRKBuffer* buf, void* data, size_t length);
DSError TRKMessageSend(TRKBuffer* msg);
DSError TRKSetBufferPosition(TRKBuffer* msg, u32 pos);
TRKBuffer* TRKGetBuffer(int index);

static inline DSError TRKAppendBuffer1_ui8(TRKBuffer* buffer, const u8 data) {
    if (buffer->position >= kMessageBufferSize) {
        return DS_MessageBufferOverflow;
    }
    buffer->data[buffer->position++] = data;
    buffer->length++;
    return DS_NoError;
}

DSError TRKAppendBuffer1_ui16(TRKBuffer* buffer, const u16 data);
DSError TRKAppendBuffer1_ui32(TRKBuffer* buffer, const u32 data);
DSError TRKAppendBuffer1_ui64(TRKBuffer* buffer, const u64 data);

DSError TRKAppendBuffer_ui8(TRKBuffer* buffer, const u8* data, int count);
DSError TRKAppendBuffer_ui16(TRKBuffer* buffer, const u16* data, int count);
DSError TRKAppendBuffer_ui32(TRKBuffer* buffer, const u32* data, int count);
DSError TRKAppendBuffer_ui64(TRKBuffer* buffer, const u64* data, int count);

DSError TRKReadBuffer1_ui8(TRKBuffer* buffer, u8* data);
DSError TRKReadBuffer1_ui16(TRKBuffer* buffer, u16* data);
DSError TRKReadBuffer1_ui32(TRKBuffer* buffer, u32* data);
DSError TRKReadBuffer1_ui64(TRKBuffer* buffer, u64* data);

DSError TRKReadBuffer_ui8(TRKBuffer* buffer, u8* data, int count);
DSError TRKReadBuffer_ui16(TRKBuffer* buffer, u16* data, int count);
DSError TRKReadBuffer_ui32(TRKBuffer* buffer, u32* data, int count);
DSError TRKReadBuffer_ui64(TRKBuffer* buffer, u64* data, int count);

#ifdef __cplusplus
}
#endif

#endif /* METROTRK_PORTABLE_MSGBUF_H */
