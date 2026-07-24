#include "types.h"
#include "dolphin/os/OSContext.h"

int OSDisableInterrupts(void);
void OSRestoreInterrupts(int level);
void OSClearContext(OSContext* context);
void OSSetCurrentContext(OSContext* context);
s64 OSGetTime(void);

typedef s16 __OSInterrupt;
typedef void (*__OSInterruptHandler)(__OSInterrupt interrupt, OSContext* context);
__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler);
u32 __OSUnmaskInterrupts(u32 global);

typedef s64 OSTime;

u32 __OSBusClock : 0x800000F8;
#define OS_TIMER_CLOCK (__OSBusClock / 4)
#define OSNanosecondsToTicks(nsec) (((nsec) * (OS_TIMER_CLOCK / 125000)) / 8000)

volatile u32 __AIRegs[16] : 0xCC006C00;
volatile u16 __DSPRegs[32] : 0xCC005000;

#define SET_REG_FIELD(reg, size, shift, val)                                  \
    do {                                                                      \
        (reg) = ((u32) (reg) & ~(((1 << (size)) - 1) << (shift))) |           \
                ((u32) (val) << (shift));                                     \
    } while (0)

typedef void (*AISCallback)(u32 count);
typedef void (*AIDCallback)(void);

static AISCallback __AIS_Callback;
static AIDCallback __AID_Callback;
static u8* __CallbackStack;
static u8* __OldStack;
static int __AI_init_flag;
static OSTime bound_32KHz;
static OSTime bound_48KHz;
static OSTime min_wait;
static OSTime max_wait;
static OSTime buffer;

static void __AIDHandler(__OSInterrupt interrupt, OSContext* context);
static void __AISHandler(__OSInterrupt interrupt, OSContext* context);
static void __AICallbackStackSwitch(register void* cb);
static void __AI_SRC_INIT(void);

u32 AIGetStreamPlayState(void);
u32 AIGetDSPSampleRate(void);
void AISetStreamSampleRate(u32 rate);
u32 AIGetStreamSampleRate(void);
void AISetStreamVolLeft(u8 vol);
u8 AIGetStreamVolLeft(void);
void AISetStreamVolRight(u8 vol);
u8 AIGetStreamVolRight(void);
void AISetStreamPlayState(u32 state);

AIDCallback AIRegisterDMACallback(AIDCallback callback)
{
    AIDCallback old_callback;
    int old;

    old_callback = __AID_Callback;
    old = OSDisableInterrupts();
    __AID_Callback = callback;
    OSRestoreInterrupts(old);
    return old_callback;
}

void AIInitDMA(u32 start_addr, u32 length)
{
    int old;

    old = OSDisableInterrupts();
    __DSPRegs[24] = (__DSPRegs[24] & 0xFFFFFC00) | (start_addr >> 16);
    __DSPRegs[25] = (__DSPRegs[25] & 0xFFFF001F) | (start_addr & 0xFFFF);
    __DSPRegs[27] = (__DSPRegs[27] & 0xFFFF8000) | ((length >> 5) & 0xFFFF);
    OSRestoreInterrupts(old);
}

void AIStartDMA(void)
{
    __DSPRegs[27] = __DSPRegs[27] | 0x8000;
}

void AISetStreamPlayState(u32 state)
{
    int old;
    u8 vol_left;
    u8 vol_right;

    if (state != AIGetStreamPlayState()) {
        if (AIGetStreamSampleRate() == 0 && state == 1) {
            vol_left = AIGetStreamVolRight();
            vol_right = AIGetStreamVolLeft();
            AISetStreamVolRight(0);
            AISetStreamVolLeft(0);
            old = OSDisableInterrupts();
            __AI_SRC_INIT();
            SET_REG_FIELD(__AIRegs[0], 1, 5, 1);
            SET_REG_FIELD(__AIRegs[0], 1, 0, 1);
            OSRestoreInterrupts(old);
            AISetStreamVolLeft(vol_left);
            AISetStreamVolRight(vol_right);
            return;
        }
        SET_REG_FIELD(__AIRegs[0], 1, 0, state);
    }
}

u32 AIGetStreamPlayState(void)
{
    return __AIRegs[0] & 1;
}

void AISetDSPSampleRate(u32 rate)
{
    int old;
    u32 play_state;
    u32 afr_state;
    u8 vol_left;
    u8 vol_right;

    if (rate != AIGetDSPSampleRate()) {
        __AIRegs[0] = (__AIRegs[0] & 0xFFFFFFBF);
        if (rate == 0) {
            vol_left = AIGetStreamVolLeft();
            vol_right = AIGetStreamVolRight();
            play_state = AIGetStreamPlayState();
            afr_state = AIGetStreamSampleRate();
            AISetStreamVolLeft(0U);
            AISetStreamVolRight(0U);
            old = OSDisableInterrupts();
            __AI_SRC_INIT();
            SET_REG_FIELD(__AIRegs[0], 1, 5, 1);
            SET_REG_FIELD(__AIRegs[0], 1, 1, afr_state);
            SET_REG_FIELD(__AIRegs[0], 1, 0, play_state);
            __AIRegs[0] |= 0x40;
            OSRestoreInterrupts(old);
            AISetStreamVolLeft(vol_left);
            AISetStreamVolRight(vol_right);
        }
    }
}

u32 AIGetDSPSampleRate(void)
{
    return ((__AIRegs[0] >> 6) & 1) ^ 1;
}

void AISetStreamSampleRate(u32 rate)
{
    int old;
    u32 play_state;
    u8 vol_left;
    u8 vol_right;
    u32 dsp_src_state;

    if (rate != AIGetStreamSampleRate()) {
        play_state = AIGetStreamPlayState();
        vol_left = AIGetStreamVolLeft();
        vol_right = AIGetStreamVolRight();
        AISetStreamVolRight(0);
        AISetStreamVolLeft(0);
        dsp_src_state = __AIRegs[0] & 0x40;
        SET_REG_FIELD(__AIRegs[0], 1, 6, 0);
        old = OSDisableInterrupts();
        __AI_SRC_INIT();
        __AIRegs[0] |= dsp_src_state;
        SET_REG_FIELD(__AIRegs[0], 1, 5, 1);
        SET_REG_FIELD(__AIRegs[0], 1, 1, rate);
        OSRestoreInterrupts(old);
        AISetStreamPlayState(play_state);
        AISetStreamVolLeft(vol_left);
        AISetStreamVolRight(vol_right);
    }
}

u32 AIGetStreamSampleRate(void)
{
    return (__AIRegs[0] >> 1) & 1;
}

void AISetStreamVolLeft(u8 vol)
{
    SET_REG_FIELD(__AIRegs[1], 8, 0, vol);
}

u8 AIGetStreamVolLeft(void)
{
    return __AIRegs[1];
}

void AISetStreamVolRight(u8 vol)
{
    SET_REG_FIELD(__AIRegs[1], 8, 8, vol);
}

u8 AIGetStreamVolRight(void)
{
    return (__AIRegs[1] & (0xFF << 8)) >> 8;
}

static void AIResetStreamSampleCount(void)
{
    __AIRegs[0] = (__AIRegs[0] & ~0x20) | 0x20;
}

static void AISetStreamTrigger(u32 trigger)
{
    __AIRegs[3] = trigger;
}

void AIInit(u8* stack)
{
    if (__AI_init_flag != 1) {
        bound_32KHz = OSNanosecondsToTicks(31524);
        bound_48KHz = OSNanosecondsToTicks(42024);
        min_wait = OSNanosecondsToTicks(42000);
        max_wait = OSNanosecondsToTicks(63000);
        buffer = OSNanosecondsToTicks(3000);
        AISetStreamVolRight(0);
        AISetStreamVolLeft(0);
        AISetStreamTrigger(0);
        AIResetStreamSampleCount();
        AISetStreamSampleRate(1);
        AISetDSPSampleRate(0);
        __AIS_Callback = NULL;
        __AID_Callback = NULL;
        __CallbackStack = stack;
        __OSSetInterruptHandler(5, __AIDHandler);
        __OSUnmaskInterrupts(0x04000000);
        __OSSetInterruptHandler(8, __AISHandler);
        __OSUnmaskInterrupts(0x800000);
        __AI_init_flag = 1;
    }
}

static void __AISHandler(__OSInterrupt interrupt, OSContext* context)
{
    OSContext exceptionContext;

    __AIRegs[0] |= 8;
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);
    if (__AIS_Callback) {
        __AIS_Callback(__AIRegs[2]);
    }
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);
}

static void __AIDHandler(__OSInterrupt interrupt, OSContext* context)
{
    OSContext exceptionContext;
    u16 tmp;

    tmp = __DSPRegs[5];
    tmp = (tmp & ~0xA0) | 8;
    __DSPRegs[5] = tmp;
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);
    if (__AID_Callback) {
        if (__CallbackStack) {
            __AICallbackStackSwitch(__AID_Callback);
        } else {
            __AID_Callback();
        }
    }
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);
}

static asm void __AICallbackStackSwitch(register void* cb)
{
    // clang-format off
    nofralloc
    mflr r0
    stw r0, 0x4(r1)
    stwu r1, -0x18(r1)
    stw r31, 0x14(r1)
    mr r31, r3
    lis r5, __OldStack@ha
    addi r5, r5, __OldStack@l
    stw r1, 0x0(r5)
    lis r5, __CallbackStack@ha
    addi r5, r5, __CallbackStack@l
    lwz r1, 0x0(r5)
    subi r1, r1, 0x8
    mtlr r31
    blrl
    lis r5, __OldStack@ha
    addi r5, r5, __OldStack@l
    lwz r1, 0x0(r5)
    lwz r0, 0x1c(r1)
    lwz r31, 0x14(r1)
    addi r1, r1, 0x18
    mtlr r0
    blr
    // clang-format on
}

static void __AI_SRC_INIT(void)
{
    OSTime rise32 = 0;
    OSTime rise48 = 0;
    OSTime diff = 0;
    OSTime unused1 = 0;
    OSTime temp = 0;
    u32 temp0 = 0;
    u32 temp1 = 0;
    u32 done = 0;
    u32 walking = 0;
    u32 unused2 = 0;
    u32 initCnt = 0;

    walking = 0;
    initCnt = 0;
    temp = 0;

    while (!done) {
        __AIRegs[0] = (__AIRegs[0] & ~0x20) | 0x20;
        __AIRegs[0] &= ~2;
        __AIRegs[0] = (__AIRegs[0] & ~1) | 1;

        temp0 = __AIRegs[2];

        while (temp0 == __AIRegs[2])
            ;
        rise32 = OSGetTime();

        __AIRegs[0] = (__AIRegs[0] & ~2) | 2;
        __AIRegs[0] = (__AIRegs[0] & ~1) | 1;

        temp1 = __AIRegs[2];
        while (temp1 == __AIRegs[2])
            ;

        rise48 = OSGetTime();

        diff = rise48 - rise32;
        __AIRegs[0] &= ~2;
        __AIRegs[0] &= ~1;

        if (diff < (bound_32KHz - buffer)) {
            temp = min_wait;
            done = 1;
            ++initCnt;
        } else if (diff >= (bound_32KHz + buffer) &&
                   diff < (bound_48KHz - buffer))
        {
            temp = max_wait;
            done = 1;
            ++initCnt;
        } else {
            done = 0;
            walking = 1;
            ++initCnt;
        }
    }

    while ((rise48 + temp) > OSGetTime())
        ;
}
