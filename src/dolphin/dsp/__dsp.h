#ifndef __DSP_H__
#define __DSP_H__

#include "types.h"
#include "dolphin/dsp.h"
#include "dolphin/os/OSContext.h"

typedef s16 __OSInterrupt;

extern DSPTaskInfo* __DSP_curr_task;
extern DSPTaskInfo* __DSP_first_task;
extern DSPTaskInfo* __DSP_last_task;
extern DSPTaskInfo* __DSP_tmp_task;
extern DSPTaskInfo* __DSP_rude_task;
extern int __DSP_rude_task_pending;

void __DSPHandler(__OSInterrupt intr, OSContext* context);
void __DSP_debug_printf(const char* fmt, ...);
void __DSP_exec_task(DSPTaskInfo* curr, DSPTaskInfo* next);
void __DSP_boot_task(DSPTaskInfo* task);
void __DSP_insert_task(DSPTaskInfo* task);
void __DSP_remove_task(DSPTaskInfo* task);

volatile u16 __DSPRegs[32] : 0xCC005000;

#endif
