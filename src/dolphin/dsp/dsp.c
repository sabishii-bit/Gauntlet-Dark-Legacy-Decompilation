#include "types.h"
#include "dolphin/dsp.h"

#include "__dsp.h"

int OSDisableInterrupts(void);
void OSRestoreInterrupts(int level);

typedef void (*__OSInterruptHandler)(__OSInterrupt interrupt, OSContext* context);
__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler);
u32 __OSUnmaskInterrupts(u32 global);

static int __DSP_init_flag;

u32 DSPCheckMailToDSP(void)
{
    return (__DSPRegs[0] & (1 << 15)) >> 15;
}

u32 DSPCheckMailFromDSP(void)
{
    return (__DSPRegs[2] & (1 << 15)) >> 15;
}

u32 DSPReadMailFromDSP(void)
{
    return (__DSPRegs[2] << 16) | __DSPRegs[3];
}

void DSPSendMailToDSP(u32 mail)
{
    __DSPRegs[0] = mail >> 16;
    __DSPRegs[1] = mail & 0xFFFF;
}

static void DSPAssertInt(void)
{
    int old;
    u16 tmp;

    old = OSDisableInterrupts();
    tmp = __DSPRegs[5];
    tmp = (tmp & ~0xA8) | 2;
    __DSPRegs[5] = tmp;
    OSRestoreInterrupts(old);
}

void DSPInit(void)
{
    int old;
    u16 tmp;

    __DSP_debug_printf("DSPInit(): Build Date: %s %s\n", "Dec 17 2001",
                       "18:25:00");

    if (__DSP_init_flag == 1) {
        return;
    }

    old = OSDisableInterrupts();
    __OSSetInterruptHandler(7, __DSPHandler);
    __OSUnmaskInterrupts(0x01000000);

    tmp = __DSPRegs[5];
    tmp = (tmp & ~0xA8) | 0x800;
    __DSPRegs[5] = tmp;

    tmp = __DSPRegs[5];
    __DSPRegs[5] = tmp = tmp & ~0xAC;

    __DSP_first_task = __DSP_last_task = __DSP_curr_task = __DSP_tmp_task =
        NULL;
    __DSP_init_flag = 1;

    OSRestoreInterrupts(old);
}

int DSPCheckInit(void)
{
    return __DSP_init_flag;
}

DSPTaskInfo* DSPAddTask(DSPTaskInfo* task)
{
    int old;

    old = OSDisableInterrupts();

    __DSP_insert_task(task);
    task->state = 0;
    task->flags = 1;

    OSRestoreInterrupts(old);
    if (task == __DSP_first_task) {
        __DSP_boot_task(task);
    }
    return task;
}

DSPTaskInfo* DSPAssertTask(DSPTaskInfo* task)
{
    s32 old;

    old = OSDisableInterrupts();

    if (__DSP_curr_task == task) {
        __DSP_rude_task = task;
        __DSP_rude_task_pending = 1;
        OSRestoreInterrupts(old);
        return task;
    }
    if (task->priority < __DSP_curr_task->priority) {
        __DSP_rude_task = task;
        __DSP_rude_task_pending = 1;
        if (__DSP_curr_task->state == 1) {
            DSPAssertInt();
        }
        OSRestoreInterrupts(old);
        return task;
    }
    OSRestoreInterrupts(old);
    return NULL;
}
