#include "types.h"
#include "dolphin/ax.h"

#include "__ax.h"

void DCFlushRange(void* addr, u32 nBytes);

static u16 __AXCommandList[2][384];

static u32 __AXCommandListPosition;
static u16* __AXClWrite;
static u32 __AXCommandListCycles;
u32 __AXClMode;

u32 __AXGetCommandListCycles(void)
{
    return __AXCommandListCycles;
}

u32 __AXGetCommandListAddress(void)
{
    u32 address;

    address = (u32) &__AXCommandList[__AXCommandListPosition][0];
    __AXCommandListPosition += 1;
    __AXCommandListPosition &= 1;
    __AXClWrite = (void*) &__AXCommandList[__AXCommandListPosition][0];
    return address;
}

static void __AXWriteToCommandList(u16 data)
{
    *__AXClWrite = data;
    __AXClWrite++;
}

void __AXNextFrame(void* sbuffer, void* buffer)
{
    u32 data;
    u16* pCommandList;

    __AXCommandListCycles = 0x1A9;
    pCommandList = __AXClWrite;
    data = __AXGetStudio();
    __AXWriteToCommandList(0);
    __AXWriteToCommandList((u16) (data >> 0x10U));
    __AXWriteToCommandList((u16) (data));
    __AXCommandListCycles += 0x2E44;
    switch (__AXClMode) {
    case 0:
        __AXWriteToCommandList(7);
        __AXWriteToCommandList((u16) ((u32) sbuffer >> 0x10));
        __AXWriteToCommandList((u32) sbuffer);
        __AXCommandListCycles += 0x546;
        break;
    case 1:
    case 4:
        __AXWriteToCommandList(0x11);
        __AXWriteToCommandList((u16) ((u32) sbuffer >> 0x10));
        __AXWriteToCommandList((u32) sbuffer);
        __AXCommandListCycles += 0x5E6;
        break;
    case 2:
    case 3:
        break;
    }
    data = (u32) __AXGetPBs();
    __AXWriteToCommandList(2U);
    __AXWriteToCommandList((u16) (data >> 0x10U));
    __AXWriteToCommandList((u16) data);
    __AXWriteToCommandList(3U);
    __AXGetAuxAInput(&data);
    if (data != 0) {
        __AXWriteToCommandList(4U);
        __AXWriteToCommandList((u16) (data >> 0x10U));
        __AXWriteToCommandList((u16) data);
        __AXGetAuxAOutput(&data);
        __AXWriteToCommandList((u16) (data >> 0x10U));
        __AXWriteToCommandList((u16) data);
        __AXCommandListCycles += 0xDED;
    }
    __AXGetAuxBInput(&data);
    if (data != 0) {
        if ((u32) __AXClMode == 4U) {
            __AXWriteToCommandList(0x10U);
            __AXCommandListCycles += 0xDED;
        } else {
            __AXWriteToCommandList(5U);
            __AXCommandListCycles += 0xDED;
        }
        __AXWriteToCommandList((u16) (data >> 0x10U));
        __AXWriteToCommandList((u16) data);
        __AXGetAuxBOutput(&data);
        __AXWriteToCommandList((u16) (data >> 0x10U));
        __AXWriteToCommandList((u16) data);
    }
    __AXWriteToCommandList(0xEU);
    __AXWriteToCommandList((u16) ((u32) sbuffer >> 0x10U));
    __AXWriteToCommandList((u32) sbuffer);
    __AXWriteToCommandList((u16) ((u32) buffer >> 0x10U));
    __AXWriteToCommandList((u32) buffer);
    __AXCommandListCycles += 0x2710;
    __AXWriteToCommandList(0xFU);
    __AXCommandListCycles += 2;
    DCFlushRange(pCommandList, 0x300);
}

void __AXClInit(void)
{
    __AXClMode = 0;
    __AXCommandListPosition = 0;
    __AXClWrite = (void*) &__AXCommandList;
}
