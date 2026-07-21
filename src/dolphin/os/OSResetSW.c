#include "types.h"
#include "dolphin/os/OSContext.h"

typedef s64 OSTime;
typedef void (*OSResetCallback)(void);

int OSDisableInterrupts(void);
int OSRestoreInterrupts(int level);
u32 __OSMaskInterrupts(u32 global);
OSTime __OSGetSystemTime(void);

extern OSTime __OSStartTime;

volatile u32 __PIRegs[] : 0xCC003000;
u8 __gUnknown800030E3 : 0x800030E3;

u32 __OSBusClock : 0x800000F8;

#define OS_BUS_CLOCK __OSBusClock
#define OS_TIMER_CLOCK (OS_BUS_CLOCK / 4)
#define OSMicrosecondsToTicks(usec) (((OS_TIMER_CLOCK / 125000) * (usec)) / 8)
#define OSMillisecondsToTicks(msec) ((msec) * (OS_TIMER_CLOCK / 1000))
#define OSSecondsToTicks(sec) ((sec)*OS_TIMER_CLOCK)
#define OSTicksToSeconds(ticks) ((ticks) / OS_TIMER_CLOCK)

static OSResetCallback ResetCallback;
static int Down;
static int LastState;
static OSTime HoldUp;
static OSTime HoldDown;

void __OSResetSWInterruptHandler(s16 exception, OSContext* context) {
    OSResetCallback callback;
    u32 debounce;

    HoldDown = __OSGetSystemTime();
    debounce = OSMicrosecondsToTicks(100);
    while (__OSGetSystemTime() - HoldDown < debounce && !(__PIRegs[0] & 0x00010000)) {
        ;
    }
    if (!(__PIRegs[0] & 0x00010000)) {
        LastState = Down = TRUE;
        __OSMaskInterrupts(0x200);
        if (ResetCallback) {
            callback = ResetCallback;
            ResetCallback = NULL;
            callback();
        }
    }
    __PIRegs[0] = 2;
}

int OSGetResetButtonState(void) {
    int enabled = OSDisableInterrupts();
    int state;
    u32 reg;
    OSTime now;

    now = __OSGetSystemTime();

    reg = __PIRegs[0];
    if (!(reg & 0x00010000)) {
        if (!Down) {
            Down = TRUE;
            state = HoldUp ? TRUE : FALSE;
            HoldDown = now;
        } else {
            state = HoldUp || (OSMicrosecondsToTicks(100) < now - HoldDown) ? TRUE : FALSE;
        }
    } else if (Down) {
        Down = FALSE;
        state = LastState;
        if (state) {
            HoldUp = now;
        } else {
            HoldUp = 0;
        }
    } else if (HoldUp && (now - HoldUp < OSMillisecondsToTicks(40))) {
        state = TRUE;
    } else {
        state = FALSE;
        HoldUp = 0;
    }

    LastState = state;

    if (__gUnknown800030E3 & 0x3F) {
        OSTime fire = (__gUnknown800030E3 & 0x3F) * 60;
        fire = __OSStartTime + OSSecondsToTicks(fire);
        if (fire < now) {
            now -= fire;
            now = OSTicksToSeconds(now) / 2;
            if ((now & 1) == 0) {
                state = TRUE;
            } else {
                state = FALSE;
            }
        }
    }

    OSRestoreInterrupts(enabled);
    return state;
}
