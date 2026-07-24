#include "types.h"
#include "dolphin/os/OSContext.h"

int OSDisableInterrupts(void);
void OSRestoreInterrupts(int level);
void OSClearContext(OSContext* context);
void OSSetCurrentContext(OSContext* context);

typedef s16 __OSInterrupt;
typedef void (*__OSInterruptHandler)(__OSInterrupt interrupt, OSContext* context);
__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler);
u32 __OSUnmaskInterrupts(u32 global);

void DCFlushRange(void* addr, u32 nBytes);
void DCInvalidateRange(void* addr, u32 nBytes);
void PPCSync(void);
void* memset(void* dest, int c, unsigned long n);

typedef void (*ARCallback)(void);

volatile u16 __DSPRegs[32] : 0xCC005000;

#define OSPhysicalToUncached(paddr) ((void*)((u32)(paddr) + 0xC0000000))

static ARCallback __AR_Callback;
static u32 __AR_Size;
static u32 __AR_InternalSize;
static u32 __AR_ExpansionSize;
static u32 __AR_StackPointer;
static u32 __AR_FreeBlocks;
static u32* __AR_BlockLength;
static int __AR_init_flag;

static void __ARHandler(__OSInterrupt exception, OSContext* context);
static void __ARClearInterrupt(void);
static void __ARWaitForDMA(void);
static void __ARWriteDMA(u32 mmem_addr, u32 aram_addr, u32 length);
static void __ARReadDMA(u32 mmem_addr, u32 aram_addr, u32 length);
static void __ARChecksize(void);

ARCallback ARRegisterDMACallback(ARCallback callback)
{
    ARCallback old_callback;
    int old;

    old_callback = __AR_Callback;
    old = OSDisableInterrupts();
    __AR_Callback = callback;
    OSRestoreInterrupts(old);
    return old_callback;
}

void ARStartDMA(u32 type, u32 mainmem_addr, u32 aram_addr, u32 length)
{
    int old;

    old = OSDisableInterrupts();
    __DSPRegs[16] = (__DSPRegs[16] & 0xFFFFFC00 | (mainmem_addr >> 0x10));
    __DSPRegs[17] = (__DSPRegs[17] & 0xFFFF001F | ((u16) mainmem_addr));
    __DSPRegs[18] = (__DSPRegs[18] & 0xFFFFFC00 | (aram_addr >> 0x10));
    __DSPRegs[19] = (__DSPRegs[19] & 0xFFFF001F | ((u16) aram_addr));
    __DSPRegs[20] = __DSPRegs[20] & ~0x8000 | ((type << 0xF) & ~0x7FFF);
    __DSPRegs[20] = (__DSPRegs[20] & 0xFFFFFC00) | (length >> 0x10);
    __DSPRegs[21] = (__DSPRegs[21] & 0xFFFF001F) | (length & 0x0000FFFF);
    OSRestoreInterrupts(old);
}

u32 ARInit(u32* stack_index_addr, u32 num_entries)
{
    int old;
    u16 refresh;

    if (__AR_init_flag == 1) {
        return 0x4000;
    }
    old = OSDisableInterrupts();
    __AR_Callback = NULL;
    __OSSetInterruptHandler(6, __ARHandler);
    __OSUnmaskInterrupts(0x02000000);
    __AR_FreeBlocks = num_entries;
    __AR_StackPointer = 0x4000;
    __AR_BlockLength = stack_index_addr;
    refresh = __DSPRegs[13] & 0xFF;
    __DSPRegs[13] = (u16) ((__DSPRegs[13] & ~0xFF) | (refresh & 0xFF));
    __ARChecksize();
    __AR_init_flag = 1;
    OSRestoreInterrupts(old);
    return __AR_StackPointer;
}

u32 ARGetBaseAddress(void)
{
    return 0x4000;
}

static void __ARHandler(__OSInterrupt exception, OSContext* context)
{
    OSContext exceptionContext;
    u16 tmp;

    tmp = __DSPRegs[5];
    tmp = (tmp & ~0x88) | 0x20;
    __DSPRegs[5] = (tmp);
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);
    if (__AR_Callback) {
        __AR_Callback();
    }
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);
}

static void __ARClearInterrupt(void)
{
    u16 tmp;

    tmp = __DSPRegs[5];
    tmp = (tmp & ~0x88) | 0x20;
    __DSPRegs[5] = (tmp);
}

static void __ARWaitForDMA(void)
{
    while (__DSPRegs[5] & 0x200)
        ;
}

static void __ARWriteDMA(u32 mmem_addr, u32 aram_addr, u32 length)
{
    __DSPRegs[16] =
        (u16) ((__DSPRegs[16] & ~0x03ff) | (u16) (mmem_addr >> 16));
    __DSPRegs[17] =
        (u16) ((__DSPRegs[17] & ~0xffe0) | (u16) (mmem_addr & 0xffff));

    __DSPRegs[18] =
        (u16) ((__DSPRegs[18] & ~0x03ff) | (u16) (aram_addr >> 16));
    __DSPRegs[19] =
        (u16) ((__DSPRegs[19] & ~0xffe0) | (u16) (aram_addr & 0xffff));

    __DSPRegs[20] = (u16) (__DSPRegs[20] & ~0x8000);

    __DSPRegs[20] =
        (u16) ((__DSPRegs[20] & ~0x03ff) | (u16) (length >> 16));
    __DSPRegs[21] =
        (u16) ((__DSPRegs[21] & ~0xffe0) | (u16) (length & 0xffff));

    __ARWaitForDMA();
    __ARClearInterrupt();
}

static void __ARReadDMA(u32 mmem_addr, u32 aram_addr, u32 length)
{
    __DSPRegs[16] =
        (u16) ((__DSPRegs[16] & ~0x03ff) | (u16) (mmem_addr >> 16));
    __DSPRegs[17] =
        (u16) ((__DSPRegs[17] & ~0xffe0) | (u16) (mmem_addr & 0xffff));

    __DSPRegs[18] =
        (u16) ((__DSPRegs[18] & ~0x03ff) | (u16) (aram_addr >> 16));
    __DSPRegs[19] =
        (u16) ((__DSPRegs[19] & ~0xffe0) | (u16) (aram_addr & 0xffff));

    __DSPRegs[20] = (u16) (__DSPRegs[20] | 0x8000);

    __DSPRegs[20] =
        (u16) ((__DSPRegs[20] & ~0x03ff) | (u16) (length >> 16));
    __DSPRegs[21] =
        (u16) ((__DSPRegs[21] & ~0xffe0) | (u16) (length & 0xffff));

    __ARWaitForDMA();
    __ARClearInterrupt();
}

static void __ARChecksize(void)
{
    u8 test_data_pad[63];
    u8 dummy_data_pad[63];
    u8 buffer_pad[63];
    u32* test_data;
    u32* dummy_data;
    u32* buffer;
    u16 ARAM_mode = 0;
    u32 ARAM_size = 0;
    u32 i;

    do {
    } while (!(__DSPRegs[11] & 1));

    ARAM_mode = 3;
    ARAM_size = __AR_InternalSize = 0x1000000;
    __DSPRegs[9] = ((__DSPRegs[9] & 0xFFFFFFC0) | 3) | 0x20;

    test_data = (void*) (((u32) &test_data_pad + 0x1F) & 0xFFFFFFE0);
    dummy_data = (void*) (((u32) &dummy_data_pad + 0x1F) & 0xFFFFFFE0);
    buffer = (void*) (((u32) &buffer_pad + 0x1F) & 0xFFFFFFE0);
    for (i = 0; i < 8; i++) {
        test_data[i] = 0xDEADBEEF;
        dummy_data[i] = 0xBAD0BAD0;
    }
    DCFlushRange(test_data, 0x20);
    DCFlushRange(dummy_data, 0x20);
    __AR_ExpansionSize = 0;
    __ARWriteDMA((u32) dummy_data, ARAM_size + 0x0000000, 0x20U);
    __ARWriteDMA((u32) dummy_data, ARAM_size + 0x0200000, 0x20U);
    __ARWriteDMA((u32) dummy_data, ARAM_size + 0x1000000, 0x20U);
    __ARWriteDMA((u32) dummy_data, ARAM_size + 0x0000200, 0x20U);
    __ARWriteDMA((u32) dummy_data, ARAM_size + 0x0400000, 0x20U);
    memset(buffer, 0, 0x20);
    DCFlushRange(buffer, 0x20);
    __ARWriteDMA((u32) test_data, ARAM_size + 0x0000000, 0x20U);
    DCInvalidateRange(buffer, 0x20);
    __ARReadDMA((u32) buffer, ARAM_size + 0x0000000, 0x20U);
    PPCSync();
    if (buffer[0] == test_data[0]) {
        memset(buffer, 0, 0x20);
        DCFlushRange(buffer, 0x20);
        __ARReadDMA((u32) buffer, ARAM_size + 0x0200000, 0x20U);
        PPCSync();
        if (buffer[0] == test_data[0]) {
            ARAM_mode |= 0 << 1;
            ARAM_size += 0x0200000;
            __AR_ExpansionSize = 0x0200000;
        } else {
            memset(buffer, 0, 0x20);
            DCFlushRange(buffer, 0x20);
            __ARReadDMA((u32) buffer, ARAM_size + 0x1000000, 0x20U);
            PPCSync();
            if (buffer[0] == test_data[0]) {
                ARAM_mode |= 4 << 1;
                ARAM_size += 0x0400000;
                __AR_ExpansionSize = 0x0400000;
            } else {
                memset(buffer, 0, 0x20);
                DCFlushRange(buffer, 0x20);
                __ARReadDMA((u32) buffer, ARAM_size + 0x0000200, 0x20U);
                PPCSync();
                if (buffer[0] == test_data[0]) {
                    ARAM_mode |= 8 << 1;
                    ARAM_size += 0x0800000;
                    __AR_ExpansionSize = 0x0800000;
                } else {
                    memset(buffer, 0, 0x20);
                    DCFlushRange(buffer, 0x20);
                    __ARReadDMA((u32) buffer, ARAM_size + 0x0400000, 0x20U);
                    PPCSync();
                    if (buffer[0] == test_data[0]) {
                        ARAM_mode |= 12 << 1;
                        ARAM_size += 0x1000000;
                        __AR_ExpansionSize = 0x1000000;
                    } else {
                        ARAM_mode |= 16 << 1;
                        ARAM_size += 0x2000000;
                        __AR_ExpansionSize = 0x2000000;
                    }
                }
            }
        }
        __DSPRegs[9] = (u16) ((__DSPRegs[9] & ~(0x07 | 0x38)) | ARAM_mode);
    }
    *(u32*) OSPhysicalToUncached(0x00D0) = ARAM_size;
    __AR_Size = ARAM_size;
}
