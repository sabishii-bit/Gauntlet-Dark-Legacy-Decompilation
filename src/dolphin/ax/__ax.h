#ifndef __AX_H__
#define __AX_H__

#include "types.h"
#include "dolphin/ax.h"

/* AXAlloc.c */
AXVPB* __AXGetStackHead(u32 priority);
void __AXServiceCallbackStack(void);
void __AXAllocInit(void);
void __AXPushFreeStack(AXVPB* p);
void __AXPushCallbackStack(AXVPB* p);
AXVPB* __AXPopCallbackStack(void);
void __AXRemoveFromStack(AXVPB* p);

/* AXAux.c */
void __AXAuxInit(void);
void __AXGetAuxAInput(u32* p);
void __AXGetAuxAOutput(u32* p);
void __AXGetAuxBInput(u32* p);
void __AXGetAuxBOutput(u32* p);
void __AXProcessAux(void);

/* AXCL.c */
void __AXClInit(void);
u32 __AXGetCommandListCycles(void);
u32 __AXGetCommandListAddress(void);
void __AXNextFrame(void* sbuffer, void* buffer);
extern u32 __AXClMode;

/* AXOut.c */
void __AXOutNewFrame(u32 lessDspCycles);
void __AXOutAiCallback(void);
void __AXOutInitDSP(void);

/* AXSPB.c */
u32 __AXGetStudio(void);
void __AXPrintStudio(void);
void __AXDepopVoice(AXPB* pb);

/* AXVPB.c */
AXPB* __AXGetPBs(void);
void __AXSyncPBs(u32 lessDspCycles);
void __AXServiceVPB(AXVPB* pvpb);
u32 __AXGetNumVoices(void);

/* AXOut.c */
void __AXOutInit(void);

/* AXSPB.c */
void __AXSPBInit(void);

/* AXVPB.c */
void __AXVPBInit(void);
void __AXSetPBDefault(AXVPB* p);

#endif
