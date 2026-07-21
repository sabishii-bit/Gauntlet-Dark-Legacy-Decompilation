#ifndef DOLPHIN_OS_OSTHREAD_H
#define DOLPHIN_OS_OSTHREAD_H

#include "dolphin/types.h"
#include "dolphin/os/OSContext.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSThread OSThread;
typedef struct OSMutex OSMutex;

typedef s32 OSPriority;

typedef struct OSThreadQueue {
    OSThread* head;
    OSThread* tail;
} OSThreadQueue;

typedef struct OSThreadLink {
    OSThread* next;
    OSThread* prev;
} OSThreadLink;

typedef struct OSMutexQueue {
    OSMutex* head;
    OSMutex* tail;
} OSMutexQueue;

typedef struct OSMutexLink {
    OSMutex* next;
    OSMutex* prev;
} OSMutexLink;

typedef struct OSCond {
    OSThreadQueue queue;
} OSCond;

struct OSMutex {
    /* 0x00 */ OSThreadQueue queue;
    /* 0x08 */ OSThread* thread;
    /* 0x0C */ s32 count;
    /* 0x10 */ OSMutexLink link;
};

struct OSThread {
    /* 0x000 */ OSContext context;
    /* 0x2C8 */ u16 state;
    /* 0x2CA */ u16 attr;
    /* 0x2CC */ s32 suspend;
    /* 0x2D0 */ OSPriority priority;
    /* 0x2D4 */ OSPriority base;
    /* 0x2D8 */ void* val;
    /* 0x2DC */ OSThreadQueue* queue;
    /* 0x2E0 */ OSThreadLink link;
    /* 0x2E8 */ OSThreadQueue queueJoin;
    /* 0x2F0 */ OSMutex* mutex;
    /* 0x2F4 */ OSMutexQueue queueMutex;
    /* 0x2FC */ OSThreadLink linkActive;
    /* 0x304 */ u8* stackBase;
    /* 0x308 */ u32* stackEnd;
};

void OSInitThreadQueue(OSThreadQueue* queue);
OSThread* OSGetCurrentThread(void);
void OSSleepThread(OSThreadQueue* queue);
void OSWakeupThread(OSThreadQueue* queue);
void OSDisableScheduler(void);
void OSEnableScheduler(void);
OSPriority __OSGetEffectivePriority(OSThread* thread);
void __OSPromoteThread(OSThread* thread, OSPriority priority);
void __OSReschedule(void);

void OSInitMutex(OSMutex* mutex);
void OSLockMutex(OSMutex* mutex);
void OSUnlockMutex(OSMutex* mutex);
void __OSUnlockAllMutex(OSThread* thread);
void OSInitCond(OSCond* cond);
void OSWaitCond(OSCond* cond, OSMutex* mutex);
void OSSignalCond(OSCond* cond);

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_OS_OSTHREAD_H */
