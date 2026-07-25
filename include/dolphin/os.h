#ifndef _DOLPHIN_OS_H_
#define _DOLPHIN_OS_H_

/* Minimal OS surface for SDK library TUs (card, ...). The project style is
 * lean local prototypes; this shim only collects the types shared across
 * dolphin headers so melee-vendored SDK sources compile unmodified. */

#include "types.h"
#include "dolphin/os/OSContext.h"
#include "dolphin/os/OSThread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* release build: SDK asserts compile out */
#define ASSERTLINE(line, cond) ((void) 0)
#define ASSERTMSGLINE(line, cond, msg) ((void) 0)
#define ASSERTMSG1LINE(line, cond, msg, arg1) ((void) 0)
#define ASSERTMSG2LINE(line, cond, msg, arg1, arg2) ((void) 0)
#define OSSetErrorHandler_UNUSED_

typedef s64 OSTime;
typedef u32 OSTick;

#define OS_BASE_CACHED 0x80000000
#define OSRoundUp32B(x) (((u32) (x) + 31) & ~31)
#define OSRoundDown32B(x) ((u32) (x) & ~31)
#define OSPhysicalToCached(paddr) ((void*) ((u32) (paddr) + OS_BASE_CACHED))
#define OSCachedToPhysical(caddr) ((u32) (caddr) - OS_BASE_CACHED)
#define __OSBusClock (*(u32*) (OS_BASE_CACHED | 0x00F8))
#define OS_BUS_CLOCK __OSBusClock
#define OS_TIMER_CLOCK (OS_BUS_CLOCK / 4)
#define OSSecondsToTicks(sec) ((sec) * (OS_TIMER_CLOCK))
#define OSMillisecondsToTicks(msec) ((msec) * (OS_TIMER_CLOCK / 1000))
#define OSMicrosecondsToTicks(usec) ((usec) * (OS_TIMER_CLOCK / 8) / 125000)

typedef struct OSAlarm OSAlarm;
typedef void (*OSAlarmHandler)(OSAlarm* alarm, OSContext* context);

struct OSAlarm {
    /* 0x00 */ OSAlarmHandler handler;
    /* 0x04 */ u32 tag;
    /* 0x08 */ OSTime fire;
    /* 0x10 */ OSAlarm* prev;
    /* 0x14 */ OSAlarm* next;
    /* 0x18 */ OSTime period;
    /* 0x20 */ OSTime start;
};

typedef struct OSResetFunctionInfo OSResetFunctionInfo;
struct OSResetFunctionInfo {
    /* 0x0 */ int (*func)(int final);
    /* 0x4 */ u32 priority;
    /* 0x8 */ OSResetFunctionInfo* next;
    /* 0xC */ OSResetFunctionInfo* prev;
};

int OSDisableInterrupts(void);
void OSRestoreInterrupts(int level);

OSTime OSGetTime(void);
OSTick OSGetTick(void);

void OSInitAlarm(void);
void OSCreateAlarm(OSAlarm* alarm);
void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler);
void OSCancelAlarm(OSAlarm* alarm);

void OSInitThreadQueue(OSThreadQueue* queue);
void OSSleepThread(OSThreadQueue* queue);
void OSWakeupThread(OSThreadQueue* queue);

void OSRegisterResetFunction(OSResetFunctionInfo* info);

#ifdef __cplusplus
}
#endif

#endif
