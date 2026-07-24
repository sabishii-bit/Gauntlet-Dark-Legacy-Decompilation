#include "types.h"

int OSDisableInterrupts(void);
void OSRestoreInterrupts(int level);

typedef void (*ARCallback)(void);
ARCallback ARRegisterDMACallback(ARCallback callback);
void ARStartDMA(u32 type, u32 mainmem_addr, u32 aram_addr, u32 length);

typedef void (*ARQCallback)(u32 pointerToRequest);

typedef struct ARQRequest ARQRequest;

struct ARQRequest {
    /* 0x00 */ ARQRequest* next;
    /* 0x04 */ u32 owner;
    /* 0x08 */ u32 type;
    /* 0x0C */ u32 priority;
    /* 0x10 */ u32 source;
    /* 0x14 */ u32 destination;
    /* 0x18 */ u32 length;
    /* 0x1C */ ARQCallback callback;
};

static ARQRequest* __ARQRequestQueueHi;
static ARQRequest* __ARQRequestTailHi;
static ARQRequest* __ARQRequestQueueLo;
static ARQRequest* __ARQRequestTailLo;
static ARQRequest* __ARQRequestPendingHi;
static ARQRequest* __ARQRequestPendingLo;
static ARQCallback __ARQCallbackHi;
static ARQCallback __ARQCallbackLo;
static u32 __ARQChunkSize;
static int __ARQ_init_flag;

static void __ARQPopTaskQueueHi(void)
{
    if (__ARQRequestQueueHi) {
        if (__ARQRequestQueueHi->type == 0) {
            ARStartDMA(__ARQRequestQueueHi->type, __ARQRequestQueueHi->source,
                       __ARQRequestQueueHi->destination,
                       __ARQRequestQueueHi->length);
        } else {
            ARStartDMA(__ARQRequestQueueHi->type,
                       __ARQRequestQueueHi->destination,
                       __ARQRequestQueueHi->source, __ARQRequestQueueHi->length);
        }
        __ARQCallbackHi = __ARQRequestQueueHi->callback;
        __ARQRequestPendingHi = __ARQRequestQueueHi;
        __ARQRequestQueueHi = __ARQRequestQueueHi->next;
    }
}

static void __ARQServiceQueueLo(void)
{
    if (__ARQRequestPendingLo == 0 && __ARQRequestQueueLo) {
        __ARQRequestPendingLo = __ARQRequestQueueLo;
        __ARQRequestQueueLo = __ARQRequestQueueLo->next;
    }

    if (__ARQRequestPendingLo) {
        if (__ARQRequestPendingLo->length <= __ARQChunkSize) {
            if (__ARQRequestPendingLo->type == 0) {
                ARStartDMA(__ARQRequestPendingLo->type,
                           __ARQRequestPendingLo->source,
                           __ARQRequestPendingLo->destination,
                           __ARQRequestPendingLo->length);
            } else {
                ARStartDMA(__ARQRequestPendingLo->type,
                           __ARQRequestPendingLo->destination,
                           __ARQRequestPendingLo->source,
                           __ARQRequestPendingLo->length);
            }
            __ARQCallbackLo = __ARQRequestPendingLo->callback;
        } else if (__ARQRequestPendingLo->type == 0) {
            ARStartDMA(__ARQRequestPendingLo->type,
                       __ARQRequestPendingLo->source,
                       __ARQRequestPendingLo->destination, __ARQChunkSize);
        } else {
            ARStartDMA(__ARQRequestPendingLo->type,
                       __ARQRequestPendingLo->destination,
                       __ARQRequestPendingLo->source, __ARQChunkSize);
        }

        __ARQRequestPendingLo->length -= __ARQChunkSize;
        __ARQRequestPendingLo->source += __ARQChunkSize;
        __ARQRequestPendingLo->destination += __ARQChunkSize;
    }
}

static void __ARQCallbackHack(u32 unused) {}

static void __ARQInterruptServiceRoutine(void)
{
    if (__ARQCallbackHi) {
        __ARQCallbackHi((u32) __ARQRequestPendingHi);
        __ARQRequestPendingHi = NULL;
        __ARQCallbackHi = NULL;
    } else if (__ARQCallbackLo) {
        __ARQCallbackLo((u32) __ARQRequestPendingLo);
        __ARQRequestPendingLo = NULL;
        __ARQCallbackLo = NULL;
    }

    __ARQPopTaskQueueHi();

    if (__ARQRequestPendingHi == 0) {
        __ARQServiceQueueLo();
    }
}

void ARQInit(void)
{
    if (__ARQ_init_flag != 1) {
        __ARQRequestQueueHi = __ARQRequestQueueLo = NULL;
        __ARQChunkSize = 0x1000;
        ARRegisterDMACallback(__ARQInterruptServiceRoutine);
        __ARQRequestPendingHi = NULL;
        __ARQRequestPendingLo = NULL;
        __ARQCallbackHi = NULL;
        __ARQCallbackLo = NULL;
        __ARQ_init_flag = 1;
    }
}

void ARQPostRequest(ARQRequest* request, u32 owner, u32 type, u32 priority,
                    u32 source, u32 dest, u32 length, ARQCallback callback)
{
    int level;

    request->next = NULL;
    request->owner = owner;
    request->type = type;
    request->source = source;
    request->destination = dest;
    request->length = length;
    if (callback) {
        request->callback = callback;
    } else {
        request->callback = __ARQCallbackHack;
    }

    level = OSDisableInterrupts();
    switch (priority) {
    case 0:
        if (__ARQRequestQueueLo) {
            __ARQRequestTailLo->next = request;
        } else {
            __ARQRequestQueueLo = request;
        }
        __ARQRequestTailLo = request;
        break;
    case 1:
        if (__ARQRequestQueueHi) {
            __ARQRequestTailHi->next = request;
        } else {
            __ARQRequestQueueHi = request;
        }
        __ARQRequestTailHi = request;
        break;
    }

    if ((__ARQRequestPendingHi == 0) && (__ARQRequestPendingLo == 0)) {
        __ARQPopTaskQueueHi();
        if (__ARQRequestPendingHi == 0) {
            __ARQServiceQueueLo();
        }
    }

    OSRestoreInterrupts(level);
}
