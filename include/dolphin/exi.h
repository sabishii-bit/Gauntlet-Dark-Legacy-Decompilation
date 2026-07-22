#ifndef DOLPHIN_EXI_H
#define DOLPHIN_EXI_H

#include "types.h"
#include "dolphin/os/OSContext.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXI_MAX_CHAN 3
#define EXI_MAX_DEV 3

#define EXI_READ 0
#define EXI_WRITE 1
#define EXI_READ_WRITE 2

#define EXI_FREQ_1M 0
#define EXI_FREQ_2M 1
#define EXI_FREQ_4M 2
#define EXI_FREQ_8M 3
#define EXI_FREQ_16M 4
#define EXI_FREQ_32M 5

#define EXI_STATE_IDLE 0x00
#define EXI_STATE_DMA 0x01
#define EXI_STATE_IMM 0x02
#define EXI_STATE_BUSY (EXI_STATE_DMA | EXI_STATE_IMM)
#define EXI_STATE_SELECTED 0x04
#define EXI_STATE_ATTACHED 0x08
#define EXI_STATE_LOCKED 0x10

#define EXI_USB_ADAPTER 0x01010000

typedef void (*EXICallback)(s32 chan, OSContext* context);

BOOL EXIImm(s32 chan, void* buf, s32 len, u32 type, EXICallback callback);
BOOL EXIImmEx(s32 chan, void* buf, s32 len, u32 mode);
BOOL EXIDma(s32 chan, void* buf, s32 len, u32 type, EXICallback callback);
BOOL EXISync(s32 chan);
u32 EXIClearInterrupts(s32 chan, BOOL exi, BOOL tc, BOOL ext);
EXICallback EXISetExiCallback(s32 chan, EXICallback exiCallback);
void EXIProbeReset(void);
BOOL EXIProbe(s32 chan);
s32 EXIProbeEx(s32 chan);
BOOL EXIAttach(s32 chan, EXICallback extCallback);
BOOL EXIDetach(s32 chan);
BOOL EXISelect(s32 chan, u32 dev, u32 freq);
BOOL EXIDeselect(s32 chan);
void EXIInit(void);
BOOL EXILock(s32 chan, u32 dev, EXICallback unlockedCallback);
BOOL EXIUnlock(s32 chan);
u32 EXIGetState(s32 chan);
s32 EXIGetID(s32 chan, u32 dev, u32* id);

#ifdef __cplusplus
}
#endif

#endif // DOLPHIN_EXI_H
