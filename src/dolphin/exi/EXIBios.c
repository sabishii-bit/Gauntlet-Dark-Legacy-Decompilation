#include "types.h"
#include "dolphin/exi.h"
#include "dolphin/os/OSContext.h"

typedef s64 OSTime;

int OSDisableInterrupts(void);
void OSRestoreInterrupts(int level);
OSTime OSGetTime(void);
void OSClearContext(OSContext* context);
void OSSetCurrentContext(OSContext* context);
u32 OSGetConsoleType(void);
u32 __OSGetDIConfig(void);
void* memmove(void* dst, const void* src, u32 n);

typedef s16 __OSInterrupt;
typedef void (*__OSInterruptHandler)(__OSInterrupt interrupt, OSContext* context);
__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler);
__OSInterruptHandler __OSGetInterruptHandler(__OSInterrupt interrupt);
u32 __OSMaskInterrupts(u32 global);
u32 __OSUnmaskInterrupts(u32 global);

u32 __OSBusClock : 0x800000F8;
#define OS_BUS_CLOCK __OSBusClock
#define OS_TIMER_CLOCK (OS_BUS_CLOCK / 4)
#define OSTicksToMilliseconds(ticks) ((ticks) / (OS_TIMER_CLOCK / 1000))

volatile u32 __EXIRegs[] : 0xCC006800;
vs32 __EXIProbeStartTime[2] : 0x800030C0;

typedef struct EXIControl {
    /* 0x00 */ EXICallback exiCallback;
    /* 0x04 */ EXICallback tcCallback;
    /* 0x08 */ EXICallback extCallback;
    /* 0x0C */ vu32 state;
    /* 0x10 */ int immLen;
    /* 0x14 */ u8* immBuf;
    /* 0x18 */ u32 dev;
    /* 0x1C */ u32 id;
    /* 0x20 */ s32 idTime;
    /* 0x24 */ int items;
    /* 0x28 */ struct {
        u32 dev;
        EXICallback callback;
    } queue[3];
} EXIControl; // size 0x40

static EXIControl Ecb[EXI_MAX_CHAN];

static void SetExiInterruptMask(s32 chan, EXIControl* exi);
static void CompleteTransfer(s32 chan);
static int __EXIProbe(s32 chan);
static void EXIIntrruptHandler(__OSInterrupt interrupt, OSContext* context);
static void TCIntrruptHandler(__OSInterrupt interrupt, OSContext* context);
static void EXTIntrruptHandler(__OSInterrupt interrupt, OSContext* context);
static void UnlockedHandler(s32 chan, OSContext* context);

static void SetExiInterruptMask(s32 chan, EXIControl* exi) {
    EXIControl* exi2 = &Ecb[2];

    switch (chan) {
    case 0:
        if ((exi->exiCallback == 0 && exi2->exiCallback == 0) || (exi->state & EXI_STATE_LOCKED)) {
            __OSMaskInterrupts(0x410000);
        } else {
            __OSUnmaskInterrupts(0x410000);
        }
        break;
    case 1:
        if (exi->exiCallback == 0 || (exi->state & EXI_STATE_LOCKED)) {
            __OSMaskInterrupts(0x80000);
        } else {
            __OSUnmaskInterrupts(0x80000);
        }
        break;
    case 2:
        if (__OSGetInterruptHandler(0x19) == 0 || (exi->state & EXI_STATE_LOCKED)) {
            __OSMaskInterrupts(0x40);
        } else {
            __OSUnmaskInterrupts(0x40);
        }
        break;
    }
}

static void CompleteTransfer(s32 chan) {
    EXIControl* exi = &Ecb[chan];
    u8* buf;
    u32 data;
    int i;
    int len;

    if (exi->state & EXI_STATE_BUSY) {
        if (exi->state & EXI_STATE_IMM) {
            if ((len = exi->immLen) != 0) {
                buf = exi->immBuf;
                data = __EXIRegs[chan * 5 + 4];
                for (i = 0; i < len; i++) {
                    *buf++ = (u8)(data >> ((3 - i) * 8));
                }
            }
        }
        exi->state &= ~EXI_STATE_BUSY;
    }
}

BOOL EXIImm(s32 chan, void* buf, s32 len, u32 type, EXICallback callback) {
    EXIControl* exi = &Ecb[chan];
    BOOL enabled;
    u32 data;
    int i;

    enabled = OSDisableInterrupts();

    if ((exi->state & EXI_STATE_BUSY) || !(exi->state & EXI_STATE_SELECTED)) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }

    exi->tcCallback = callback;
    if (exi->tcCallback) {
        EXIClearInterrupts(chan, FALSE, TRUE, FALSE);
        __OSUnmaskInterrupts(0x200000u >> (chan * 3));
    }

    exi->state |= EXI_STATE_IMM;

    if (type != EXI_READ) {
        data = 0;
        for (i = 0; i < len; i++) {
            data |= ((u8*)buf)[i] << ((3 - i) * 8);
        }
        __EXIRegs[chan * 5 + 4] = data;
    }

    exi->immBuf = buf;
    exi->immLen = (type != EXI_WRITE) ? len : 0;

    __EXIRegs[chan * 5 + 3] = (type << 2) | 1 | ((len - 1) << 4);
    OSRestoreInterrupts(enabled);
    return TRUE;
}

BOOL EXIImmEx(s32 chan, void* buf, s32 len, u32 mode) {
    s32 xLen;

    while (len) {
        xLen = (len < 4) ? len : 4;
        if (!EXIImm(chan, buf, xLen, mode, 0)) {
            return FALSE;
        }
        if (!EXISync(chan)) {
            return FALSE;
        }
        ((u8*)buf) += xLen;
        len -= xLen;
    }
    return TRUE;
}

BOOL EXIDma(s32 chan, void* buf, s32 len, u32 type, EXICallback callback) {
    EXIControl* exi = &Ecb[chan];
    BOOL enabled;

    enabled = OSDisableInterrupts();

    if ((exi->state & EXI_STATE_BUSY) || !(exi->state & EXI_STATE_SELECTED)) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }

    exi->tcCallback = callback;
    if (exi->tcCallback) {
        EXIClearInterrupts(chan, FALSE, TRUE, FALSE);
        __OSUnmaskInterrupts(0x200000u >> (chan * 3));
    }

    exi->state |= EXI_STATE_DMA;

    __EXIRegs[chan * 5 + 1] = (u32)buf & 0x03FFFFE0;
    __EXIRegs[chan * 5 + 2] = (u32)len;
    __EXIRegs[chan * 5 + 3] = (type << 2) | 3;
    OSRestoreInterrupts(enabled);
    return TRUE;
}

BOOL EXISync(s32 chan) {
    EXIControl* exi = &Ecb[chan];
    BOOL rc = FALSE;
    BOOL enabled;

    while (exi->state & EXI_STATE_SELECTED) {
        if (!(__EXIRegs[chan * 5 + 3] & 1)) {
            enabled = OSDisableInterrupts();
            if (exi->state & EXI_STATE_SELECTED) {
                CompleteTransfer(chan);
                if (__OSGetDIConfig() != 0xFF || exi->immLen != 4 ||
                    (__EXIRegs[chan * 5] & 0x00000070) != (EXI_FREQ_1M << 4) ||
                    __EXIRegs[chan * 5 + 4] != EXI_USB_ADAPTER) {
                    rc = TRUE;
                }
            }
            OSRestoreInterrupts(enabled);
            break;
        }
    }
    return rc;
}

u32 EXIClearInterrupts(s32 chan, BOOL exi, BOOL tc, BOOL ext) {
    u32 cpr;
    u32 prev;

    cpr = prev = __EXIRegs[chan * 5];
    prev &= 0x7F5;
    if (exi) {
        prev |= 2;
    }
    if (tc) {
        prev |= 8;
    }
    if (ext) {
        prev |= 0x800;
    }
    __EXIRegs[chan * 5] = prev;
    return cpr;
}

EXICallback EXISetExiCallback(s32 chan, EXICallback exiCallback) {
    EXIControl* exi = &Ecb[chan];
    EXICallback prev;
    BOOL enabled;

    enabled = OSDisableInterrupts();
    prev = exi->exiCallback;
    exi->exiCallback = exiCallback;
    if (chan != 2) {
        SetExiInterruptMask(chan, exi);
    } else {
        SetExiInterruptMask(0, &Ecb[0]);
    }
    OSRestoreInterrupts(enabled);
    return prev;
}

void EXIProbeReset(void) {
    __EXIProbeStartTime[0] = __EXIProbeStartTime[1] = 0;
    Ecb[0].idTime = Ecb[1].idTime = 0;
    __EXIProbe(0);
    __EXIProbe(1);
}

static int __EXIProbe(s32 chan) {
    EXIControl* exi = &Ecb[chan];
    BOOL enabled;
    int rc;
    u32 cpr;
    s32 t;

    if (chan == 2) {
        return TRUE;
    }

    rc = TRUE;
    enabled = OSDisableInterrupts();
    cpr = __EXIRegs[chan * 5];
    if (!(exi->state & EXI_STATE_ATTACHED)) {
        if (cpr & 0x800) {
            EXIClearInterrupts(chan, FALSE, FALSE, TRUE);
            exi->idTime = 0;
            __EXIProbeStartTime[chan] = 0;
        }
        if (cpr & 0x1000) {
            t = (s32)(OSTicksToMilliseconds(OSGetTime()) / 100) + 1;
            if (__EXIProbeStartTime[chan] == 0) {
                __EXIProbeStartTime[chan] = t;
            }
            if (t - (s32)__EXIProbeStartTime[chan] < 3) {
                rc = FALSE;
            }
        } else {
            exi->idTime = 0;
            __EXIProbeStartTime[chan] = 0;
            rc = FALSE;
        }
    } else if (!(cpr & 0x1000) || (cpr & 0x800)) {
        exi->idTime = 0;
        __EXIProbeStartTime[chan] = 0;
        rc = FALSE;
    }
    OSRestoreInterrupts(enabled);
    return rc;
}

BOOL EXIProbe(s32 chan) {
    EXIControl* exi = &Ecb[chan];
    int rc;
    u32 id;

    if ((rc = __EXIProbe(chan))) {
        if (exi->idTime == 0) {
            if (EXIGetID(chan, 0, &id) != 0) {
                rc = TRUE;
            } else {
                rc = FALSE;
            }
        }
    }
    return rc;
}

s32 EXIProbeEx(s32 chan) {
    if (EXIProbe(chan)) {
        return 1;
    }
    if (__EXIProbeStartTime[chan]) {
        return 0;
    }
    return -1;
}

static BOOL __EXIAttach(s32 chan, EXICallback extCallback) {
    EXIControl* exi = &Ecb[chan];
    BOOL enabled;
    BOOL rc;

    enabled = OSDisableInterrupts();
    if ((exi->state & EXI_STATE_ATTACHED) || __EXIProbe(chan) == 0) {
        OSRestoreInterrupts(enabled);
        rc = FALSE;
    } else {
        EXIClearInterrupts(chan, TRUE, FALSE, FALSE);
        exi->extCallback = extCallback;
        __OSUnmaskInterrupts(0x100000u >> (chan * 3));
        exi->state |= EXI_STATE_ATTACHED;
        OSRestoreInterrupts(enabled);
        rc = TRUE;
    }
    return rc;
}

BOOL EXIAttach(s32 chan, EXICallback extCallback) {
    EXIControl* exi = &Ecb[chan];
    BOOL enabled;
    BOOL rc;

    EXIProbe(chan);

    enabled = OSDisableInterrupts();
    if (exi->idTime == 0) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }

    rc = __EXIAttach(chan, extCallback);
    OSRestoreInterrupts(enabled);
    return rc;
}

BOOL EXIDetach(s32 chan) {
    EXIControl* exi = &Ecb[chan];
    BOOL enabled;

    enabled = OSDisableInterrupts();
    if (!(exi->state & EXI_STATE_ATTACHED)) {
        OSRestoreInterrupts(enabled);
        return TRUE;
    }
    if ((exi->state & EXI_STATE_LOCKED) && exi->dev == 0) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }
    exi->state &= ~EXI_STATE_ATTACHED;
    __OSMaskInterrupts(0x700000u >> (chan * 3));
    OSRestoreInterrupts(enabled);
    return TRUE;
}

BOOL EXISelect(s32 chan, u32 dev, u32 freq) {
    EXIControl* exi = &Ecb[chan];
    u32 cpr;
    BOOL enabled;

    enabled = OSDisableInterrupts();

    if ((exi->state & EXI_STATE_SELECTED) ||
        (chan != 2 &&
         ((dev == 0 && !(exi->state & EXI_STATE_ATTACHED) && __EXIProbe(chan) == 0) ||
          !(exi->state & EXI_STATE_LOCKED) || exi->dev != dev))) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }

    exi->state |= EXI_STATE_SELECTED;

    cpr = __EXIRegs[chan * 5];
    cpr &= 0x405;
    cpr |= ((1 << dev) << 7) | (freq << 4);
    __EXIRegs[chan * 5] = cpr;

    if (exi->state & EXI_STATE_ATTACHED) {
        switch (chan) {
        case 0:
            __OSMaskInterrupts(0x100000);
            break;
        case 1:
            __OSMaskInterrupts(0x20000);
            break;
        }
    }

    OSRestoreInterrupts(enabled);
    return TRUE;
}

BOOL EXIDeselect(s32 chan) {
    EXIControl* exi = &Ecb[chan];
    u32 cpr;
    BOOL enabled;

    enabled = OSDisableInterrupts();

    if (!(exi->state & EXI_STATE_SELECTED)) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }

    exi->state &= ~EXI_STATE_SELECTED;

    cpr = __EXIRegs[chan * 5];
    __EXIRegs[chan * 5] = cpr & 0x405;

    if (exi->state & EXI_STATE_ATTACHED) {
        switch (chan) {
        case 0:
            __OSUnmaskInterrupts(0x100000);
            break;
        case 1:
            __OSUnmaskInterrupts(0x20000);
            break;
        }
    }

    OSRestoreInterrupts(enabled);

    if (chan != 2 && (cpr & 0x80)) {
        if (__EXIProbe(chan) != 0) {
            return TRUE;
        }
        return FALSE;
    }
    return TRUE;
}

static void EXIIntrruptHandler(__OSInterrupt interrupt, OSContext* context) {
    s32 chan;
    EXIControl* exi;
    EXICallback callback;
    OSContext exceptionContext;

    chan = (interrupt - 9) / 3;
    exi = &Ecb[chan];
    EXIClearInterrupts(chan, TRUE, FALSE, FALSE);
    callback = exi->exiCallback;
    if (callback) {
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);
        callback(chan, context);
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);
    }
}

static void TCIntrruptHandler(__OSInterrupt interrupt, OSContext* context) {
    s32 chan;
    EXIControl* exi;
    EXICallback callback;
    OSContext exceptionContext;

    chan = (interrupt - 10) / 3;
    exi = &Ecb[chan];
    __OSMaskInterrupts(0x80000000u >> interrupt);
    EXIClearInterrupts(chan, FALSE, TRUE, FALSE);
    callback = exi->tcCallback;
    if (callback) {
        exi->tcCallback = NULL;
        CompleteTransfer(chan);
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);
        callback(chan, context);
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);
    }
}

static void EXTIntrruptHandler(__OSInterrupt interrupt, OSContext* context) {
    s32 chan;
    EXIControl* exi;
    EXICallback callback;
    OSContext exceptionContext;

    chan = (interrupt - 11) / 3;
    __OSMaskInterrupts(0x700000u >> (chan * 3));
    __EXIRegs[chan * 5] = 0;
    exi = &Ecb[chan];
    callback = exi->extCallback;
    exi->state &= ~EXI_STATE_ATTACHED;
    if (callback) {
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);
        exi->extCallback = 0;
        callback(chan, context);
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);
    }
}

void EXIInit(void) {
    __OSMaskInterrupts(0x7F8000);

    __EXIRegs[0] = 0;
    __EXIRegs[5] = 0;
    __EXIRegs[10] = 0;

    __EXIRegs[0] = 0x2000;

    __OSSetInterruptHandler(9, EXIIntrruptHandler);
    __OSSetInterruptHandler(10, TCIntrruptHandler);
    __OSSetInterruptHandler(11, EXTIntrruptHandler);
    __OSSetInterruptHandler(12, EXIIntrruptHandler);
    __OSSetInterruptHandler(13, TCIntrruptHandler);
    __OSSetInterruptHandler(14, EXTIntrruptHandler);
    __OSSetInterruptHandler(15, EXIIntrruptHandler);
    __OSSetInterruptHandler(16, TCIntrruptHandler);

    if (OSGetConsoleType() & 0x10000000) {
        EXIProbeReset();
    }
}

BOOL EXILock(s32 chan, u32 dev, EXICallback unlockedCallback) {
    EXIControl* exi = &Ecb[chan];
    BOOL enabled;
    int i;

    enabled = OSDisableInterrupts();

    if (exi->state & EXI_STATE_LOCKED) {
        if (unlockedCallback) {
            for (i = 0; i < exi->items; i++) {
                if (exi->queue[i].dev == dev) {
                    OSRestoreInterrupts(enabled);
                    return FALSE;
                }
            }
            exi->queue[exi->items].callback = unlockedCallback;
            exi->queue[exi->items].dev = dev;
            exi->items++;
        }
        OSRestoreInterrupts(enabled);
        return FALSE;
    }

    exi->state |= EXI_STATE_LOCKED;
    exi->dev = dev;
    SetExiInterruptMask(chan, exi);

    OSRestoreInterrupts(enabled);
    return TRUE;
}

BOOL EXIUnlock(s32 chan) {
    EXIControl* exi = &Ecb[chan];
    BOOL enabled;
    EXICallback unlockedCallback;

    enabled = OSDisableInterrupts();

    if (!(exi->state & EXI_STATE_LOCKED)) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }

    exi->state &= ~EXI_STATE_LOCKED;
    SetExiInterruptMask(chan, exi);

    if (0 < exi->items) {
        unlockedCallback = exi->queue[0].callback;
        if (0 < --exi->items) {
            memmove(&exi->queue[0], &exi->queue[1], sizeof(exi->queue[0]) * exi->items);
        }
        unlockedCallback(chan, 0);
    }

    OSRestoreInterrupts(enabled);
    return TRUE;
}

u32 EXIGetState(s32 chan) {
    EXIControl* exi = &Ecb[chan];

    return exi->state;
}

static void UnlockedHandler(s32 chan, OSContext* context) {
    u32 id;

    EXIGetID(chan, 0, &id);
}

s32 EXIGetID(s32 chan, u32 dev, u32* id) {
    EXIControl* exi = &Ecb[chan];
    BOOL enabled;
    BOOL err;
    u32 cmd;
    s32 startTime;

    if (chan < 2 && dev == 0) {
        if (!__EXIProbe(chan)) {
            return 0;
        }

        if (exi->idTime == __EXIProbeStartTime[chan]) {
            *id = exi->id;
            return exi->idTime;
        }

        if (!__EXIAttach(chan, NULL)) {
            return 0;
        }

        startTime = __EXIProbeStartTime[chan];
    }

    err = !EXILock(chan, dev, (chan < 2 && dev == 0) ? UnlockedHandler : NULL);
    if (!err) {
        err = !EXISelect(chan, dev, EXI_FREQ_1M);
        if (!err) {
            cmd = 0;
            err |= !EXIImm(chan, &cmd, 2, EXI_WRITE, NULL);
            err |= !EXISync(chan);
            err |= !EXIImm(chan, id, 4, EXI_READ, NULL);
            err |= !EXISync(chan);
            err |= !EXIDeselect(chan);
        }
        EXIUnlock(chan);
    }

    if (chan < 2 && dev == 0) {
        EXIDetach(chan);

        enabled = OSDisableInterrupts();
        err |= (startTime != __EXIProbeStartTime[chan]);
        if (!err) {
            exi->id = *id;
            exi->idTime = startTime;
        }
        OSRestoreInterrupts(enabled);

        return err ? 0 : exi->idTime;
    }

    if (err) {
        return 0;
    }
    return 1;
}
