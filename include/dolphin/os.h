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

typedef s64 OSTime;
typedef u32 OSTick;

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
