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
#include "game/dcs.h"

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

extern u8 lbl_802F5F60[];
extern DcsBankData dcsBankData[];
extern s32 lbl_803451F8;
extern s32 dcsResetPending;
extern volatile u8 dcsSampleBusy;
extern volatile u8 dcsAramBusy;
extern ARQRequest dcsAramReq;

extern u32 pool_new(void* list);
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
        *handle = (index + 1) * 0x1000 + 1;
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
void dcsSampleStream(void) {
}

/* 0x800D3674  ARQ post MRAM->ARAM for sample data */
void dcsSampleUpload(void) {
}

/* 0x800D374C  alloc ARAM + ARQ upload */
void dcsSampleAllocUpload(void) {
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
void dcsVoiceSetMaster(void) {
}

/* 0x800D3CCC  refresh voice vol/pan/master */
void dcsVoiceUpdate(void) {
}

/* 0x800D3DC4  is this channel/voice active? (voiceInUse) */
void dcsVoiceInUse(void) {
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
