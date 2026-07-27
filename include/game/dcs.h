#ifndef GAME_DCS_H
#define GAME_DCS_H

#include "types.h"

void dcsChannelPlay(s32 value);
s32 update_chinfo(u32 channels);
s32 dcsChannelSetVolPan(u32 channels, s16 pan);
s32 dcsChannelSetVolPan2(u32 channels, s32 volume);
void dcsAllocReset(s32* high, s32* current, s32* low);
s32 dcsBankQuery(s32 bank, s32* handle, s32* size);
s32 dcsVoiceStart(u32 sample, s32 volumePan, s32 priority);
s32 AudioStillLoading(void);
void dcsQuePoll(void);
s32 AudioQueUpdate(s32 bank);
s32 dcsBankLoad(void* bank, s32 mode);
void dcsRequestReset(void);
s32 dcsBankUnload(void* bank);
void dcsVoiceSetMaster(s32 channel, s32 left, s32 right);
void dcsVoiceUpdate(s32 channel);
s32 dcsVoiceInUse(s32 channel);
s32 dcsSampleStream(void* sample, u32 uploadArg);

void dcsAramReadTop(void* destination, u32 length);
void dcsAramWriteTop(void* source, u32 length);
void dcsAramWrite(void* source, u32 aramDestination, u32 length);
void dcsAramRead(u32 aramSource, void* destination, u32 length);

#endif
