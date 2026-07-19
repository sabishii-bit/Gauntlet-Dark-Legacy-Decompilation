#ifndef METROTRK_PORTABLE_MSGHNDLR_H
#define METROTRK_PORTABLE_MSGHNDLR_H

#include "trk.h"

typedef struct DSVersions {
    u8 kernelMajor;
    u8 kernelMinor;
    u8 protocolMajor;
    u8 protocolMinor;
} DSVersions;

typedef struct DSCPUType {
    u8 cpuMajor;
    u8 cpuMinor;
    u8 bigEndian;
    u8 defaultTypeSize;
    u8 fpTypeSize;
    u8 extended1TypeSize;
    u8 extended2TypeSize;
} DSCPUType;

typedef u8 DSSupportMask[32];

void TRKMessageIntoReply(TRKBuffer* b, MessageCommandID commandId, DSReplyError replyError);
DSError TRKStandardACK(TRKBuffer* b, MessageCommandID commandId, DSReplyError replyError);

/* targimpl.c (not yet decompiled) */
DSError TRKTargetVersions(DSVersions*);
DSError TRKTargetSupportMask(DSSupportMask*);
DSError TRKTargetCPUType(DSCPUType*);
DSError TRKTargetAccessMemory(void*, u32, size_t*, MemoryAccessOptions, BOOL);
DSError TRKTargetAccessDefault(u32, u32, TRKBuffer*, size_t*, BOOL);
DSError TRKTargetAccessFP(u32, u32, TRKBuffer*, size_t*, BOOL);
DSError TRKTargetAccessExtended1(u32, u32, TRKBuffer*, size_t*, BOOL);
DSError TRKTargetAccessExtended2(u32, u32, TRKBuffer*, size_t*, BOOL);
DSError TRKTargetFlushCache(u8, u32, u32);
u32 TRKTargetGetPC(void);
DSError TRKTargetSingleStep(u32, BOOL);
DSError TRKTargetStepOutOfRange(u32, u32, BOOL);
DSError TRKTargetStop(void);
DSError TRKTargetContinue(void);
BOOL TRKTargetStopped(void);

/* dolphin_trk.c (not yet decompiled) */
void __TRK_reset(void);

void SetTRKConnected(BOOL);
BOOL GetTRKConnected(void);
DSError TRKDoSetOption(TRKBuffer*);
DSError TRKDoStop(TRKBuffer*);
DSError TRKDoStep(TRKBuffer*);
DSError TRKDoContinue(TRKBuffer*);
DSError TRKDoWriteRegisters(TRKBuffer*);
DSError TRKDoReadRegisters(TRKBuffer*);
DSError TRKDoWriteMemory(TRKBuffer*);
DSError TRKDoReadMemory(TRKBuffer*);
DSError TRKDoSupportMask(TRKBuffer*);
DSError TRKDoVersions(TRKBuffer*);
DSError TRKDoOverride(TRKBuffer*);
DSError TRKDoReset(TRKBuffer*);
DSError TRKDoDisconnect(TRKBuffer*);
DSError TRKDoConnect(TRKBuffer*);

#endif /* METROTRK_PORTABLE_MSGHNDLR_H */
