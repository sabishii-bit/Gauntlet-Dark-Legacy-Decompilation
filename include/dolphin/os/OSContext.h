#ifndef DOLPHIN_OS_OSCONTEXT_H
#define DOLPHIN_OS_OSCONTEXT_H

#include "dolphin/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSContext {
    /* 0x000 */ u32 gpr[32];
    /* 0x080 */ u32 cr;
    /* 0x084 */ u32 lr;
    /* 0x088 */ u32 ctr;
    /* 0x08C */ u32 xer;
    /* 0x090 */ f64 fpr[32];
    /* 0x190 */ u32 fpscr_pad;
    /* 0x194 */ u32 fpscr;
    /* 0x198 */ u32 srr0;
    /* 0x19C */ u32 srr1;
    /* 0x1A0 */ u16 mode;
    /* 0x1A2 */ u16 state;
    /* 0x1A4 */ u32 gqr[8];
    /* 0x1C4 */ u32 psf_pad;
    /* 0x1C8 */ f64 psf[32];
} OSContext;

#define OS_CONTEXT_STATE_FPSAVED 0x1
#define OS_CONTEXT_STATE_EXC 0x2

OSContext* OSGetCurrentContext(void);
u32 OSSaveContext(OSContext* context);
void OSLoadContext(OSContext* context);
void OSSetCurrentContext(OSContext* context);
void OSClearContext(OSContext* context);
void OSInitContext(OSContext* context, u32 pc, u32 newsp);
void OSDumpContext(OSContext* context);
u32 OSGetStackPointer(void);
void __OSContextInit(void);

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_OS_OSCONTEXT_H */
