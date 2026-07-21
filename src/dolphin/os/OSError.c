#include "types.h"
#include "__va_arg.h"
#include "dolphin/os/OSContext.h"

int vprintf(const char* format, va_list arg);

typedef u16 __OSException;
typedef void (*OSErrorHandler)(int exception, ...);
typedef s64 OSTime;

int OSDisableInterrupts(void);
void OSDisableScheduler(void);
void OSEnableScheduler(void);
void __OSReschedule(void);
OSTime OSGetTime(void);
void PPCHalt(void);

extern volatile s16 __OSLastInterrupt;
extern volatile u32 __OSLastInterruptSrr0;
extern volatile OSTime __OSLastInterruptTime;

volatile u16 __DSPRegs[] : 0xCC005000;
volatile u32 __DIRegs[] : 0xCC006000;

OSErrorHandler __OSErrorTable[16];

void OSReport(const char* msg, ...) {
    va_list marker;
    va_start(marker, msg);
    vprintf(msg, marker);
    va_end(marker);
}

void OSPanic(const char* file, int line, const char* msg, ...) {
    va_list marker;
    u32 i;
    u32* p;

    OSDisableInterrupts();
    va_start(marker, msg);
    vprintf(msg, marker);
    va_end(marker);
    OSReport(" in \"%s\" on line %d.\n", file, line);

    OSReport("\nAddress:      Back Chain    LR Save\n");
    for (i = 0, p = (u32*)OSGetStackPointer(); p && (u32)p != 0xffffffff && i++ < 16;
         p = (u32*)*p) {
        OSReport("0x%08x:   0x%08x    0x%08x\n", p, p[0], p[1]);
    }

    PPCHalt();
}

OSErrorHandler OSSetErrorHandler(__OSException error, OSErrorHandler handler) {
    OSErrorHandler oldHandler;

    oldHandler = __OSErrorTable[error];
    __OSErrorTable[error] = handler;
    return oldHandler;
}

void __OSUnhandledException(u8 exception, OSContext* context, u32 dsisr, u32 dar) {
    if (!(context->srr1 & 0x2)) {
        OSReport("Non-recoverable Exception %d", exception);
    } else {
        if (__OSErrorTable[exception]) {
            OSDisableScheduler();
            __OSErrorTable[exception](exception, context, dsisr, dar);
            OSEnableScheduler();
            __OSReschedule();
            OSLoadContext(context);
        }
        if (exception == 8) {
            OSLoadContext(context);
        }
        OSReport("Unhandled Exception %d", exception);
    }
    OSReport("\n");
    OSDumpContext(context);
    OSReport("\nDSISR = 0x%08x                   DAR  = 0x%08x\n", dsisr, dar);
    OSReport("TB = 0x%016llx\n", OSGetTime());

    switch (exception) {
    case 2:
        OSReport("\nInstruction at 0x%x (read from SRR0) attempted to access "
                 "invalid address 0x%x (read from DAR)\n",
                 context->srr0, dar);
        break;
    case 3:
        OSReport("\nAttempted to fetch instruction from invalid address 0x%x "
                 "(read from SRR0)\n",
                 context->srr0);
        break;
    case 5:
        OSReport("\nInstruction at 0x%x (read from SRR0) attempted to access "
                 "unaligned address 0x%x (read from DAR)\n",
                 context->srr0, dar);
        break;
    case 6:
        OSReport("\nProgram exception : Possible illegal instruction/operation "
                 "at or around 0x%x (read from SRR0)\n",
                 context->srr0, dar);
        break;
    case 15:
        OSReport("\n");
        OSReport("AI DMA Address =   0x%04x%04x\n", __DSPRegs[24], __DSPRegs[25]);
        OSReport("ARAM DMA Address = 0x%04x%04x\n", __DSPRegs[16], __DSPRegs[17]);
        OSReport("DI DMA Address =   0x%08x\n", __DIRegs[5]);
        break;
    }

    OSReport("\nLast interrupt (%d): SRR0 = 0x%08x  TB = 0x%016llx\n", __OSLastInterrupt,
             __OSLastInterruptSrr0, __OSLastInterruptTime);
    PPCHalt();
}
