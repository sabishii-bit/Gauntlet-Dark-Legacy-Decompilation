/*
 * AmcExi2Stubs.c - AMC EXI2 stub library (Dolphin SDK).
 * Reference: melee extern/dolphin/src/dolphin/amcstubs/AmcExi2Stubs.c
 */
#include "types.h"

typedef int AmcExiError;
typedef void (*EXICallback)(int chan, void* context);

void EXI2_Init(volatile unsigned char** inputPendingPtrRef, EXICallback monitorCallback)
{
}

void EXI2_EnableInterrupts(void)
{
}

int EXI2_Poll(void)
{
    return 0;
}

AmcExiError EXI2_ReadN(void* bytes, unsigned long length)
{
    return 0;
}

AmcExiError EXI2_WriteN(const void* bytes, unsigned long length)
{
    return 0;
}

void EXI2_Reserve(void)
{
}

void EXI2_Unreserve(void)
{
}

int AMC_IsStub(void)
{
    return 1;
}
