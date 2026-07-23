#include "types.h"
#include "dolphin/gx.h"
#include "dolphin/os/OSContext.h"

#include "__gx.h"

int OSDisableInterrupts(void);
void OSRestoreInterrupts(int level);
void OSClearContext(OSContext* context);
void OSSetCurrentContext(OSContext* context);
void OSInitThreadQueue(OSThreadQueue* queue);
void OSSleepThread(OSThreadQueue* queue);
void OSWakeupThread(OSThreadQueue* queue);
void PPCSync(void);

typedef s16 __OSInterrupt;
typedef void (*__OSInterruptHandler)(__OSInterrupt interrupt, OSContext* context);
__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler);
u32 __OSUnmaskInterrupts(u32 global);

static GXDrawSyncCallback TokenCB;
static GXDrawDoneCallback DrawDoneCB;
static u8 DrawDone;
static OSThreadQueue FinishQueue;

void GXFlush(void)
{
    u32 i;

    if (gx->dirtyState) {
        __GXSetDirtyState();
    }
    for (i = 8; i > 0; i--) {
        GX_WRITE_U32(0);
    }
    PPCSync();
}

static void GXSetDrawDone(void)
{
    u32 reg;
    BOOL enabled;

    enabled = OSDisableInterrupts();
    reg = 0x45000002;
    GX_WRITE_RAS_REG(reg);
    GXFlush();
    DrawDone = 0;
    OSRestoreInterrupts(enabled);
}

static void GXWaitDrawDone(void)
{
    BOOL enabled;

    enabled = OSDisableInterrupts();
    while (!DrawDone) {
        OSSleepThread(&FinishQueue);
    }
    OSRestoreInterrupts(enabled);
}

void GXDrawDone(void)
{
    GXSetDrawDone();
    GXWaitDrawDone();
}

void GXPokeAlphaMode(GXCompare func, u8 threshold)
{
    u32 reg;

    reg = (func << 8) | threshold;
    __peReg[3] = reg;
}

void GXPokeAlphaRead(GXAlphaReadMode mode)
{
    u32 reg;

    reg = 0;
    SET_REG_FIELD(0x26D, reg, 2, 0, mode);
    SET_REG_FIELD(0x26E, reg, 1, 2, 1);
    __peReg[4] = reg;
}

void GXPokeAlphaUpdate(GXBool update_enable)
{
    u32 reg;

    reg = __peReg[1];
    SET_REG_FIELD(0x27A, reg, 1, 4, update_enable);
    __peReg[1] = reg;
}

void GXPokeBlendMode(GXBlendMode type, GXBlendFactor src_factor, GXBlendFactor dst_factor, GXLogicOp op)
{
    u32 reg;

    reg = __peReg[1];
    SET_REG_FIELD(0x28C, reg, 1, 0, (type == GX_BM_BLEND) || (type == GX_BM_SUBTRACT));
    SET_REG_FIELD(0x28D, reg, 1, 11, (type == GX_BM_SUBTRACT));
    SET_REG_FIELD(0x28F, reg, 1, 1, (type == GX_BM_LOGIC));
    SET_REG_FIELD(0x290, reg, 4, 12, op);
    SET_REG_FIELD(0x291, reg, 3, 8, src_factor);
    SET_REG_FIELD(0x292, reg, 3, 5, dst_factor);
    SET_REG_FIELD(0x293, reg, 8, 24, 0x41);
    __peReg[1] = reg;
}

void GXPokeColorUpdate(GXBool update_enable)
{
    u32 reg;

    reg = __peReg[1];
    SET_REG_FIELD(0x2A0, reg, 1, 3, update_enable);
    __peReg[1] = reg;
}

void GXPokeDstAlpha(GXBool enable, u8 alpha)
{
    u32 reg = 0;

    SET_REG_FIELD(0x2AB, reg, 8, 0, alpha);
    SET_REG_FIELD(0x2AC, reg, 1, 8, enable);
    __peReg[2] = reg;
}

void GXPokeDither(GXBool dither)
{
    u32 reg;

    reg = __peReg[1];
    SET_REG_FIELD(0x2B8, reg, 1, 2, dither);
    __peReg[1] = reg;
}

void GXPokeZMode(GXBool compare_enable, GXCompare func, GXBool update_enable)
{
    u32 reg = 0;

    SET_REG_FIELD(0x2C3, reg, 1, 0, compare_enable);
    SET_REG_FIELD(0x2C4, reg, 3, 1, func);
    SET_REG_FIELD(0x2C5, reg, 1, 4, update_enable);
    __peReg[0] = reg;
}

static void GXTokenInterruptHandler(__OSInterrupt interrupt, OSContext* context)
{
    u16 token;
    OSContext exceptionContext;
    u32 reg;

    token = __peReg[7];
    if (TokenCB != NULL) {
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);
        TokenCB(token);
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);
    }
    reg = __peReg[5];
    SET_REG_FIELD(0, reg, 1, 2, 1);
    __peReg[5] = reg;
}

static void GXFinishInterruptHandler(__OSInterrupt interrupt, OSContext* context)
{
    OSContext exceptionContext;
    u32 reg;

    reg = __peReg[5];
    SET_REG_FIELD(0, reg, 1, 3, 1);
    __peReg[5] = reg;
    DrawDone = 1;
    if (DrawDoneCB != NULL) {
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);
        DrawDoneCB();
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);
    }
    OSWakeupThread(&FinishQueue);
}

void __GXPEInit(void)
{
    u32 reg;

    __OSSetInterruptHandler(0x12, GXTokenInterruptHandler);
    __OSSetInterruptHandler(0x13, GXFinishInterruptHandler);
    OSInitThreadQueue(&FinishQueue);
    __OSUnmaskInterrupts(0x2000);
    __OSUnmaskInterrupts(0x1000);
    reg = __peReg[5];
    SET_REG_FIELD(0, reg, 1, 2, 1);
    SET_REG_FIELD(0, reg, 1, 3, 1);
    SET_REG_FIELD(0, reg, 1, 0, 1);
    SET_REG_FIELD(0, reg, 1, 1, 1);
    __peReg[5] = reg;
}
