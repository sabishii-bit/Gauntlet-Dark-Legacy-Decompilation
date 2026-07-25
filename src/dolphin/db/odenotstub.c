/*
 * odenotstub.c - real ODEMU debugger EXI driver (Dolphin SDK).
 * Reference: melee extern/dolphin/src/dolphin/odenotstub/odenotstub.c
 */
#include "types.h"
#include "dolphin/os/OSContext.h"

#define ALIGN_PREV(X, N) ((X) & ~((N) - 1))
#define ALIGN_NEXT(X, N) ALIGN_PREV(((X) + (N) - 1), N)

typedef s16 __OSInterrupt;
typedef void (*__OSInterruptHandler)(__OSInterrupt interrupt, OSContext* context);

BOOL OSDisableInterrupts(void);
BOOL OSRestoreInterrupts(BOOL enabled);
u32 __OSMaskInterrupts(u32 mask);
u32 __OSUnmaskInterrupts(u32 mask);
__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler);

volatile u32 __EXIRegs[] : 0xCC006800;
volatile u32 __PIRegs[] : 0xCC003000;

static u8 SendCount = 0x80;

static __OSInterruptHandler MTRCallback;
static void (*DBGCallback)(u32, OSContext*);
static u32 SendMailData;
static s32 RecvDataLeng;
static u8* pEXIInputFlag;
static u8 EXIInputFlag;

static void DBGEXIInit(void)
{
    __OSMaskInterrupts(0x18000);
    __EXIRegs[10] = 0;
}

static BOOL DBGEXISelect(u32 v)
{
    u32 regs = __EXIRegs[10];
    regs &= 0x405;
    regs |= 0x80 | (v << 4);
    __EXIRegs[10] = regs;
    return TRUE;
}

static BOOL DBGEXIDeselect(void)
{
    __EXIRegs[10] &= 0x405;
    return TRUE;
}

static BOOL DBGEXISync(void)
{
    while (__EXIRegs[13] & 1) {
    }
    return TRUE;
}

static BOOL DBGEXIImm(void* data, s32 byte_size, u32 write)
{
    u32 writeVal;
    u32 readVal;
    int i;

    if (write) {
        writeVal = 0;
        for (i = 0; i < byte_size; ++i) {
            u8* nextWordPtr = (u8*) data + i;
            writeVal |= *nextWordPtr << ((3 - i) << 3);
        }
        __EXIRegs[14] = writeVal;
    }

    __EXIRegs[13] = 1 | (write << 2) | ((byte_size - 1U) << 4);

    DBGEXISync();
    if (!write) {
        u8* dataPtr = (u8*) data;
        readVal = __EXIRegs[14];
        for (i = 0; i < byte_size; ++i) {
            *dataPtr++ = readVal >> ((3 - i) << 3);
        }
    }

    return TRUE;
}

static BOOL DBGReadMailbox(void* param1)
{
    BOOL error = FALSE;
    u32 cmd;

    if (!DBGEXISelect(4)) {
        return FALSE;
    }

    cmd = 0x60000000;
    error |= !DBGEXIImm((u8*) &cmd, 2, 1);
    error |= !DBGEXISync();
    error |= !DBGEXIImm(param1, sizeof(param1), 0);
    error |= !DBGEXISync();
    error |= !DBGEXIDeselect();

    return !error;
}

static BOOL DBGWriteMailbox(u32 param1)
{
    BOOL error = FALSE;
    u32 value;

    if (!DBGEXISelect(4)) {
        return FALSE;
    }

    value = (param1 & 0x1FFFFFFF) | 0xC0000000;
    error |= !DBGEXIImm((u8*) &value, sizeof(value), 1);
    error |= !DBGEXISync();
    error |= !DBGEXIDeselect();

    return !error;
}

static BOOL DBGRead(u32 param1, u32* data, s32 byte_size)
{
    BOOL error = FALSE;
    u32* dataPtr = (u32*) data;
    u32 writeValue;
    u32 readValue;

    if (!DBGEXISelect(4)) {
        return FALSE;
    }

    writeValue = (u32) (param1 << 8) & 0x1FFFC00 | 0x20000000;
    error |= !DBGEXIImm((u8*) &writeValue, sizeof(writeValue), 1);
    error |= !DBGEXISync();

    while (byte_size != 0) {
        error |= !DBGEXIImm((u8*) &readValue, sizeof(readValue), 0);
        error |= !DBGEXISync();

        *dataPtr++ = readValue;

        byte_size -= 4;
        if (byte_size < 0) {
            byte_size = 0;
        }
    }
    error |= !DBGEXIDeselect();

    return !error;
}

static BOOL DBGWrite(u32 param1, u32* data, s32 byte_size)
{
    BOOL error = FALSE;
    u32* dataPtr = (u32*) data;
    u32 value;
    u32 nextWord;

    if (!DBGEXISelect(4)) {
        return FALSE;
    }

    value = (param1 & 0x1FFFC) << 8 | 0xA0000000;
    error = !DBGEXIImm((u8*) &value, sizeof(value), 1);
    error |= !DBGEXISync();

    while (byte_size != 0) {
        nextWord = *dataPtr++;

        error |= !DBGEXIImm((u8*) &nextWord, sizeof(nextWord), 1);
        error |= !DBGEXISync();

        byte_size -= 4;
        if (byte_size < 0) {
            byte_size = 0;
        }
    }

    error |= !DBGEXIDeselect();

    return !error;
}

static BOOL DBGReadStatus(void* param1)
{
    BOOL error = FALSE;
    u32 cmd;

    if (!DBGEXISelect(4)) {
        return FALSE;
    }

    cmd = 0x40000000;
    error |= !DBGEXIImm((u8*) &cmd, 2, 1);
    error |= !DBGEXISync();
    error |= !DBGEXIImm(param1, sizeof(param1), 0);
    error |= !DBGEXISync();
    error |= !DBGEXIDeselect();

    return !error;
}

static void MWCallback(u32 interrupt, OSContext* context)
{
    EXIInputFlag = 1;
    if (MTRCallback) {
        (*MTRCallback)(0, context);
    }
}

static void DBGHandler(__OSInterrupt interrupt, OSContext* context)
{
    __PIRegs[0] = 0x1000;
    if (DBGCallback) {
        (*DBGCallback)(interrupt, context);
    }
}

static void CheckMailBox(void)
{
    u32 mail[2];

    DBGReadStatus(mail);
    if (mail[0] & 1) {
        DBGReadMailbox(mail);
        mail[0] &= ~0xE0000000;
        if ((mail[0] & 0x1F000000) == 0x1F000000) {
            SendMailData = mail[0];
            RecvDataLeng = mail[0] & 0x7FFF;
            EXIInputFlag = 1;
        }
    }
}

void DBInitComm(int* inputFlagPtr, int* mtrCallback)
{
    BOOL enabled;

    enabled = OSDisableInterrupts();
    pEXIInputFlag = &EXIInputFlag;
    *inputFlagPtr = (int) pEXIInputFlag;
    MTRCallback = (__OSInterruptHandler) mtrCallback;
    DBGEXIInit();
    OSRestoreInterrupts(enabled);
}

int DBInitInterrupts(void)
{
    __OSMaskInterrupts(0x18000);
    __OSMaskInterrupts(0x40);
    DBGCallback = MWCallback;
    __OSSetInterruptHandler(0x19, DBGHandler);
    __OSUnmaskInterrupts(0x40);
}

int DBQueryData(void)
{
    BOOL enable;

    EXIInputFlag = 0;
    if (RecvDataLeng == 0) {
        enable = OSDisableInterrupts();
        CheckMailBox();
    }
    OSRestoreInterrupts(enable);
    return RecvDataLeng;
}

int DBRead(void* data, u32 size)
{
    BOOL enabled;
    u32 offset;

    enabled = OSDisableInterrupts();
    if (SendMailData & 0x10000) {
        offset = 0x1000;
    } else {
        offset = 0;
    }
    DBGRead(offset + 0x1E000, data, size + 3U & 0xFFFFFFFC);
    RecvDataLeng = 0;
    EXIInputFlag = 0;
    OSRestoreInterrupts(enabled);
    return 0;
}

int DBWrite(const void* data, u32 size)
{
    u32 value;
    u32 busyFlag;
    BOOL enabled;

    enabled = OSDisableInterrupts();
    do {
        DBGReadStatus(&busyFlag);
    } while (busyFlag & 2);

    ++SendCount;

    value = ((SendCount & 1) ? 0x1000 : 0);
    while (!DBGWrite(value | 0x1C000, (u32*) data, ALIGN_NEXT(size, 4))) {
    }

    do {
        DBGReadStatus(&busyFlag);
    } while (busyFlag & 2);

    value = (SendCount << 0x10) | 0x1F000000 | size;
    while (!DBGWriteMailbox(value)) {
    }

    do {
        while (!DBGReadStatus(&busyFlag)) {
        }
    } while (busyFlag & 2);

    OSRestoreInterrupts(enabled);

    return 0;
}

void DBOpen(void)
{
}

void DBClose(void)
{
}
