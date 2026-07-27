/*
 * dcs.c - Midway DCS (Digital Compression System) audio driver, GameCube port.
 *
 * Text 0x800D1E04-0x800D4308.  The low-level sound driver: loads DCS sound
 * "banks" (VAG/BANK format), decodes/streams ADPCM samples into ARAM via the
 * AR queue (ARQ), and drives the GameCube AX voices through the sndvoice.c
 * layer.  Replaces the PS2/IOP SPU backend (DCS.OBJ/DCS_PS2.OBJ/BANK.OBJ on
 * the arcade/Xbox builds) with an AX/ARQ backend.  Debug strings tag the
 * module: "DCSERROR:", "BankReadHeader", "VagParseHeader NOT A VAG!",
 * "Duck nonzero -- resetting".
 *
 * The ARAM helpers dcsAram* are also called cross-TU by game/sys/memcard.c
 * (save data is parked in the top of ARAM).
 *
 * NonMatching: partial reconstruction. The public ABI, bank query/reset,
 * load polling, ARAM transfer path, callbacks, and stream flag are translated;
 * the voice/bank decoding core is still linked from the extracted DOL.
 */
#include "types.h"
#include "dolphin/ax.h"
#include "game/dcs.h"
#include "game/sndvoice.h"

typedef void (*ARQCallback)(u32 request);

typedef struct ARQRequest {
    struct ARQRequest* next;
    u32 owner;
    u32 type;
    u32 priority;
    u32 source;
    u32 destination;
    u32 length;
    ARQCallback callback;
} ARQRequest;

typedef struct DcsBankData {
    u32 handle;
    u32 size;
} DcsBankData;

typedef struct DcsStream {
    u8 _pad[28];
    u32 oneShot;
} DcsStream;

typedef struct DcsChannelInfo {
    s16 volume;
    s16 pan;
    u16 duck;
    u16 _pad06;
    u32 _unk08;
    s32 sample;
    u32 _unk10;
} DcsChannelInfo;

extern u8 lbl_802F5F60[];
extern DcsBankData dcsBankData[];
extern DcsChannelInfo ch_info[12];
extern AXVPB* sVoice[14];
extern s32 lbl_803451F8;
extern s32 dcsResetPending;
extern volatile u8 dcsSampleBusy;
extern volatile u8 dcsAramBusy;
extern ARQRequest dcsAramReq;

extern u32 pool_new(void* list);
extern void* pool_alloc(void* list, void* node);
extern void ResetAllocTot(void);
extern void DCFlushRange(void* address, u32 length);
extern void DCInvalidateRange(void* address, u32 length);
extern void ARQPostRequest(ARQRequest* request, u32 owner, u32 type,
                           u32 priority, u32 source, u32 destination,
                           u32 length, ARQCallback callback);

/* 0x800D1E04  trigger/refresh a channel; -> dcsVoiceStart */
void dcsChannelPlay(s32 value) {
}

/* 0x800D1ED0  recompute per-channel voice state each tick */
s32 update_chinfo(u32 channels) {
}

/* 0x800D1FFC  push volume/pan to a channel voice */
s32 dcsChannelSetVolPan(u32 channels, s16 pan) {
}

/* 0x800D21B4  volume/pan variant */
s32 dcsChannelSetVolPan2(u32 channels, s32 volume) {
}

/* 0x800D2314  reset the sample allocator (-> pool_new) */
void dcsAllocReset(s32* high, s32* current, s32* low) {
    *high = pool_new(lbl_802F5F60);
}

/* 0x800D2350  look up bank handle/size in dcsBankData */
s32 dcsBankQuery(s32 bank, s32* handle, s32* size) {
    s32 result = 1;
    s32 index = bank - 1;

    if (bank != 0) {
        *handle = (index + result) * 0x1000 + result;
        *size = dcsBankData[index].size;
        return result;
    }
    *handle = 0;
    *size = 0;
    return 0;
}

/* 0x800D23A0  start playback on a channel */
s32 dcsVoiceStart(u32 sample, s32 volumePan, s32 priority) {
}

/* 0x800D2534  poll: any bank/stream still loading? */
s32 AudioStillLoading(void) {
    s32 loading;

    while ((loading = lbl_803451F8) != 0) {
        AudioQueUpdate(loading);
    }
    return 1;
}

/* 0x800D2568  wrapper -> AudioQueUpdate */
void dcsQuePoll(void) {
    AudioQueUpdate(lbl_803451F8);
}

/* 0x800D258C  service the queued sample/duck requests */
s32 AudioQueUpdate(s32 bank) {
}

/* 0x800D285C  open file, read bank header/calls/vags */
s32 dcsBankLoad(void* bank, s32 mode) {
}

/* 0x800D29D4  set dcsResetPending */
void dcsRequestReset(void) {
    dcsResetPending = 1;
}

/* 0x800D29E0  free a loaded bank (-> pool_dispose) */
s32 dcsBankUnload(void* bank) {
}

/* 0x800D2A68  read VAG sample table, upload to ARAM (readVags) */
void dcsReadVags(void) {
}

/* 0x800D2CEC  read the bank call list (readCalls) */
void dcsReadCalls(void) {
}

/* 0x800D30B4  validate sample indices (callFixup) */
void dcsBankCheckSamples(void) {
}

/* 0x800D3184  service the channel queue */
void dcsServiceQueue(void) {
}

/* 0x800D321C  assign+start a voice for a sample */
void dcsVoicePlay(void) {
}

/* 0x800D33B8  configure AX ADPCM voice for a sample */
void dcsVoiceSetupAdpcm(void) {
}

/* 0x800D3608  begin streaming a sample into ARAM */
s32 dcsSampleAllocUpload(void* sample, s32 arg);
s32 dcsSampleUpload(void* state, u32 uploadArg);

s32 dcsSampleStream(void* sample, u32 uploadArg) {
    s32 result;
    u32* state = (u32*)((u8*)sample + 16);
    u8 pad[8];

    dcsSampleAllocUpload(sample, 0);
    pool_alloc(lbl_802F5F60, sample);
    result = dcsSampleUpload(state, uploadArg);
    state[0] = 0;
    state[1] = 0;
    ResetAllocTot();
    return result;
}

/* 0x800D3674  ARQ post MRAM->ARAM for sample data */
s32 dcsSampleUpload(void* state, u32 uploadArg) {
}

/* 0x800D374C  alloc ARAM + ARQ upload */
s32 dcsSampleAllocUpload(void* sample, s32 arg) {
}

/* 0x800D3874  ARQ read from top of ARAM (memcard uses) */
void dcsAramCallback(u32 request);

void dcsAramReadTop(void* destination, u32 length) {
    u32 aramSource;

    dcsAramBusy = 1;
    aramSource = 0x1000000 - length;
    DCFlushRange(destination, length);
    ARQPostRequest(&dcsAramReq, 0, 1, 1, aramSource, (u32)destination,
                   length, dcsAramCallback);
    while (dcsAramBusy != 0) {
    }
    DCInvalidateRange(destination, length);
}

/* 0x800D38F8  ARQ write to top of ARAM (memcard uses) */
void dcsAramWriteTop(void* source, u32 length) {
    u32 aramDestination;

    dcsAramBusy = 1;
    aramDestination = 0x1000000 - length;
    DCFlushRange(source, length);
    ARQPostRequest(&dcsAramReq, 0, 0, 1, (u32)source, aramDestination,
                   length, dcsAramCallback);
    while (dcsAramBusy != 0) {
    }
}

/* 0x800D3970  ARQ MRAM->ARAM copy (memcard uses) */
void dcsAramWrite(void* source, u32 aramDestination, u32 length) {
    dcsAramBusy = 1;
    DCFlushRange(source, length);
    ARQPostRequest(&dcsAramReq, 0, 0, 1, (u32)source, aramDestination,
                   length, dcsAramCallback);
    while (dcsAramBusy != 0) {
    }
}

/* 0x800D39E8  ARQ ARAM->MRAM copy (memcard uses) */
void dcsAramRead(u32 aramSource, void* destination, u32 length) {
    dcsAramBusy = 1;
    DCFlushRange(destination, length);
    ARQPostRequest(&dcsAramReq, 0, 1, 1, aramSource, (u32)destination,
                   length, dcsAramCallback);
    while (dcsAramBusy != 0) {
    }
    DCInvalidateRange(destination, length);
}

/* 0x800D3A70  ARQ completion cb; clears dcsAramBusy */
void dcsAramCallback(u32 request) {
    dcsAramBusy = 0;
}

/* 0x800D3A7C  ARQ completion cb; clears dcsSampleBusy */
void dcsSampleCallback(u32 request) {
    dcsSampleBusy = 0;
}

/* 0x800D3A88  start an AX voice (src/state/vol/pan) */
void dcsVoiceStartAx(void) {
}

/* 0x800D3C40  set voice master volume */
#pragma dont_inline on
void dcsVoiceSetMaster(s32 channel, s32 left, s32 right) {
    s32 master;
    u32 bit;

    if ((u32)left == 0x3FFF) {
        master = 0;
    } else if (left == 0) {
        master = -1000;
    } else {
        bit = 0x2000;
        master = -30;
        while ((left & bit) == 0) {
            bit >>= 1;
            master -= 30;
        }
        master += ((bit >> 1) + (left - bit) * 30) / bit;
    }
    sndVoiceSetMaster(sVoice[channel], master);
}
#pragma dont_inline off

/* 0x800D3CCC  refresh voice vol/pan/master */
void dcsVoiceUpdate(s32 channel) {
    DcsChannelInfo* info = &ch_info[channel];
    s32 volume = info->volume;
    u16 master;
    u16 excess;
    s32 scaled;
    s32 pan;
    s32 sign;

    if (volume < 0) {
        volume = 0;
    }
    scaled = volume * 0x3FFF / 0xFF;
    excess = (u16)scaled;
    master = excess;
    if ((u16)scaled > 0x3FFF) {
        excess -= 0x3FFF - master;
        master = 0x3FFF;
    }
    if (excess > 0x3FFF) {
        master -= 0x3FFF - excess;
    }

    pan = 0x100 - ((info->pan + 0x100) & 0x1FF);
    sign = pan >> 31;
    pan = (sign ^ pan) - sign;
    sndVoiceSetVolume(sVoice[channel], pan >> 1);
    pan = 0x100 - ((info->pan + 0x180) & 0x1FF);
    sign = pan >> 31;
    pan = (sign ^ pan) - sign;
    sndVoiceSetPan(sVoice[channel], pan >> 1);
    dcsVoiceSetMaster(channel, master, master);
}

/* 0x800D3DC4  is this channel/voice active? (voiceInUse) */
s32 dcsVoiceInUse(s32 channel) {
    DcsChannelInfo* info = &ch_info[channel];
    s32 result = 0;

    if (info->sample >= 0 || info->duck != 0) {
        result = 1;
    } else if (sVoice[channel]->pb.state != 0) {
        result = 1;
    }
    return result;
}

/* 0x800D3E24  read+validate a BANK file header */
void BankReadHeader(void) {
}

/* 0x800D4048  parse in-memory BANK header */
void BankParseHeader(void) {
}

/* 0x800D415C  parse a VAG sample header */
void VagParseHeader(void) {
}

/* 0x800D42E4  set stream loop/one-shot flag */
void dcsSetStreamFlag(DcsStream* stream, s32 looping) {
    u32 oneShot;

    if (stream != 0) {
        if (looping != 0) {
            oneShot = 0;
        } else {
            oneShot = 1;
        }
        stream->oneShot = oneShot;
    }
}
