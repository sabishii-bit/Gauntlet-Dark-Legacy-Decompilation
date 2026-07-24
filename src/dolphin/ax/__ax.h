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

/* AXCL.c */
void __AXClInit(void);

/* AXOut.c */
void __AXOutInit(void);

/* AXSPB.c */
void __AXSPBInit(void);

/* AXVPB.c */
void __AXVPBInit(void);
void __AXSetPBDefault(AXVPB* p);

#endif
