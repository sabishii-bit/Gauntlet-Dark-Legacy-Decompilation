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

typedef struct DcsSampleData {
    u32 aramAddress;
    u32 swappedLength;
    u8 _pad08[0x18];
    u32 sampleRate;
    u32 length;
    u16 coefficients[16];
    u16 predScale;
    u16 _pad4A;
} DcsSampleData;

typedef struct DcsVagExtraHeader {
    u32 words[24];
} DcsVagExtraHeader;

typedef struct DcsChannelInfo {
    s16 volume;
    s16 pan;
    u16 duck;
    u16 _pad06;
    s32 priority;
    s32 sample;
    DcsSampleData* sampleData;
} DcsChannelInfo;

#define DCS_STRING_POOL                                                       \
    "DCSERROR: \0\0"                                                        \
    "Duck nonzero (%d) -- resetting\n\0"                                    \
    "DCSFATAL: \0\0"                                                        \
    "OVERWRITE existing SAMPLE (%d)\n\0"                                   \
    "%d samples read OK -- %d PUNTED\n\0\0\0\0"                           \
    "BANK End of file in CALL list\n\0\0"                                  \
    "BANK max call index exceeded\n\0\0\0"                                \
    "BANK max call instructions exceeded\n\0\0\0\0"                       \
    "BANK nCall exceeds size\n\0\0\0\0"                                  \
    "BANK call list size mismatches header\n\0\0"                           \
    "BANK max sample index exceeded\n\0"                                   \
    "No BANK header found\n\0\0\0"                                        \
    "BankReadHeader NOT A BANK!\n\0"                                       \
    "BankReadHeader TOO MANY CALLS: 0x%04x > 0x4FF\0\0\0"                 \
    "Incomplete BANK header found\n\0\0\0"                                \
    "BankReadHeader BAD SIZE for call list\0\0\0"                         \
    "Unknown BANK version\0\0\0\0"                                       \
    "Bank type %d.%03d,  made with VAGBANK v%d.%03d\n\0"                   \
    "VagParseHeader NOT A VAG!\n\0"

#define DCS_SWAP32(value)                                                     \
    (((value) << 24) | (((value) << 8) & 0x00FF0000) |                       \
     ((value) >> 24) | (((value) >> 8) & 0x0000FF00))

extern u8 lbl_802F5F60[];
extern DcsSampleData lbl_802C9F60[];
extern s32 lbl_802C9ED8[];
extern u16 lbl_802EFF5E[];
extern u16 lbl_802F0F60[];
extern DcsBankData dcsBankData[];
extern DcsChannelInfo ch_info[12];
extern AXVPB* sVoice[14];
extern s32 lbl_80343FF8;
extern s32 lbl_803451F8;
extern s32 lbl_80345200;
extern s32 lbl_8034520C;
extern u32 lbl_80345214;
extern s32 lbl_80345220;
extern u32 lbl_80345234;
extern s32 lbl_80345204;
extern s32 lbl_80345224;
extern u32 lbl_80345228;
extern u32 lbl_8034522C;
extern u32 lbl_80345230;
extern u32 lbl_80129588[];
extern char lbl_80117080[];
extern int printf(const char* format, ...);
extern int strncmp(const char* lhs, const char* rhs, u32 length);
extern void* memcpy(void* dst, const void* src, u32 length);
extern u32 FileBufGet(void* file, void* destination, s32 length);
extern s32 dcsResetPending;
extern volatile u8 dcsSampleBusy;
extern volatile u8 dcsAramBusy;
extern ARQRequest dcsAramReq;

extern u32 pool_new(void* list);
extern void* pool_alloc(void* list, void* node);
extern void pool_free(void* pool, void* node);
extern void pool_dispose(void* pool, s32 handle, void* base, s32 size);
extern void pool_dispose_and_alloc(void* pool, void* node, u32 size, s32 arg, void* pool2);
extern void* pool_alloc_at(void* pool, void* node, u32 size, u32 arg, void* pool2);
extern ARQRequest dcsSampleReq;
extern u32 BytesFree(void);
extern void* AllocHiMem(u32 size, u32 tag);
extern s32 lbl_80345208;
extern void dcsMemLockTag(s32 slot, u32 tag);
extern s32 dcsMemLock(void);
extern s32 dcsMemUnlock(s32 channel);
extern void ResetAllocTot(void);
extern void DCFlushRange(void* address, u32 length);
extern void DCInvalidateRange(void* address, u32 length);
extern void ARQPostRequest(ARQRequest* request, u32 owner, u32 type,
                           u32 priority, u32 source, u32 destination,
                           u32 length, ARQCallback callback);

s32 dcsVoiceStart(u32 sample, s32 volumePan, s32 priority);

/* 0x800D1E04  trigger/refresh a channel; -> dcsVoiceStart */
void dcsChannelPlay(s32 value) {
    if (value != lbl_80345228) {
        lbl_80345230 = 0;
        lbl_80345228 = value;
        if ((value & 0xfdc0000) != 0 || lbl_8034522C == 15) {
            lbl_8034522C = 0;
        } else if ((value & 0xf0f00000) == lbl_80129588[lbl_8034522C]) {
            lbl_8034522C++;
        }
    } else {
        if (++lbl_80345230 > 15) {
            if (lbl_8034522C == 15) {
                lbl_80345214 = (u32)__cntlzw(lbl_80345214) >> 5;
                dcsVoiceStart(0x1001, 0x7f7f, 0xff);
            }
            lbl_8034522C = 0;
            lbl_80345230 = 0;
        }
    }
}

/* 0x800D1ED0  recompute per-channel voice state each tick */
s32 update_chinfo(u32 channels) {
    s32 channel = 0;
    s32 adjustment;
    s32 i;

    channels &= 0xFFF;
    dcsMemLockTag(0, channels);
    dcsMemLock();
    while (channels != 0) {
        if ((channels & 1) != 0) {
            if (ch_info[channel].duck != 0) {
                adjustment =
                    -(s32)ch_info[channel].duck * lbl_80343FF8;
                adjustment >>= 8;
                lbl_8034520C += adjustment;
                for (i = 0; i < 12; i++) {
                    if (dcsVoiceInUse(i)) {
                        ch_info[i].volume -= adjustment;
                        dcsVoiceUpdate(i);
                    }
                }
                ch_info[channel].duck = 0;
            }
            ch_info[channel].sample = -1;
            lbl_80345234 &= ~(1 << channel);
            AXSetVoiceState(sVoice[channel], 0);
        }
        channel++;
        channels >>= 1;
    }
    return 0;
}

/* 0x800D1FFC  push volume/pan to a channel voice */
s32 dcsChannelSetVolPan(u32 channels, s16 pan) {
    s32 channel = 0;

    channels &= 0xFFF;
    while (channels != 0) {
        if ((channels & 1) != 0) {
            s16 delta = pan - ch_info[channel].pan;

            if (delta != 0) {
                if (delta > 0x100) {
                    delta -= 0x200;
                }
                if (delta < -0x100) {
                    delta += 0x200;
                }
                if (delta != 0) {
                    if (delta < -8) {
                        delta = -8;
                    }
                    if (delta > 8) {
                        delta = 8;
                    }
                    ch_info[channel].pan += delta;
                    if (ch_info[channel].pan > 0x1FF) {
                        ch_info[channel].pan -= 0x400;
                    }
                    if (ch_info[channel].pan < -0x200) {
                        ch_info[channel].pan += 0x400;
                    }
                    dcsVoiceUpdate(channel);
                }
            }
        }
        channels >>= 1;
        channel++;
    }
    return 0;
}

/* 0x800D21B4  volume/pan variant */
s32 dcsChannelSetVolPan2(u32 channels, s32 volume) {
    s32 channel = 0;
    s32 desired = volume - lbl_8034520C;

    channels &= 0xFFF;
    while (channels != 0) {
        s32 delta = desired - ch_info[channel].volume;

        if ((channels & 1) != 0 && delta != 0) {
            if (delta < -8) {
                delta = -8;
            }
            if (delta > 8) {
                delta = 8;
            }
            ch_info[channel].volume += delta;
            dcsVoiceUpdate(channel);
        }
        channels >>= 1;
        channel++;
    }
    return 0;
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
    } else {
        *handle = 0;
        *size = 0;
        result = 0;
    }
    return result;
}

/* 0x800D23A0  start playback on a channel */
s32 dcsVoiceStart(u32 sample, s32 volumePan, s32 priority) {
    s32 channel = -1;
    s32 bank = (s32)sample >> 12;
    s32 callIndex =
        (sample & 0xFFF) + lbl_802C9ED8[bank * 2];

    if (callIndex > 0 && callIndex <= lbl_80345200) {
        u16 sequence = lbl_802EFF5E[callIndex];
        u16* call = &lbl_802F0F60[sequence];
        u16* end = (u16*)lbl_802F5F60;
        s32 adjustment;
        s32 i;

        while (call < end && (*call & 0x8000) == 0) {
            call++;
        }
        channel = dcsVoicePlay(priority);
        if (channel >= 0) {
            DcsChannelInfo* info = &ch_info[channel];

            info->volume =
                (s16)(((volumePan >> 16) * (u32)call[1]) / 0x7F) -
                (s16)lbl_8034520C;
            info->pan = (s16)volumePan;
            info->duck = 0;
            info->priority = ((u32)priority << 16) | call[3];
            info->sample = -1;
            if (call[2] != 0) {
                adjustment = (u32)call[2] * lbl_80343FF8;
                adjustment >>= 8;
                lbl_8034520C += adjustment;
                for (i = 0; i < 12; i++) {
                    if (dcsVoiceInUse(i)) {
                        ch_info[i].volume -= adjustment;
                        dcsVoiceUpdate(i);
                    }
                }
            }
            info->duck = call[2];
            info->sample = sequence;
            dcsVoiceSetupAdpcm(channel);
        }
    }
    return channel;
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
    u32* p = (u32*)bank;
    s32 i;

    if (p[0] != 0) {
        pool_dispose(lbl_802F5F60, p[0], (void*)p[1], 0x40);
    }
    lbl_80345208 = 0;
    lbl_80343FF8 = p[7];
    lbl_8034520C = 0;
    dcsResetPending = 0;
    for (i = 0; i < 12; i++) {
        ch_info[i].sample = -1;
    }
    return 0;
}

/* 0x800D2A68  read VAG sample table, upload to ARAM (readVags) */
void dcsReadVags(void) {
}

/* 0x800D2CEC  read the bank call list (readCalls) */
void dcsReadCalls(void) {
}

/* 0x800D30B4  validate sample indices (callFixup) */
s32 dcsBankCheckSamples(int param_9, int param_10) {
    u16* puVar4;
    int iVar5;

    for (iVar5 = lbl_80345224; iVar5 < lbl_80345204; iVar5++) {
        puVar4 = &lbl_802F0F60[iVar5];
        if ((int)(*puVar4 & 0xfff) >= param_9) {
            if ((int)((*puVar4 & 0xfff) + param_10) > 0xfff) {
                printf(DCS_STRING_POOL);
                printf(lbl_80117080);
                param_10 = 0;
            }
            if (param_10 == 0) {
                *puVar4 |= 0xfff;
            } else {
                *puVar4 += param_10;
            }
        }
        if ((*puVar4 & 0x8000) != 0) {
            iVar5 += 3;
        }
    }
    return 0;
}

/* 0x800D3184  service the channel queue */
void dcsServiceQueue(void) {
    s32 channel;

    for (channel = 0; channel < 12; channel++) {
        if ((lbl_80345234 & (1 << channel)) != 0) {
            AXPBADDR* addr = &sVoice[channel]->pb.addr;
            u32 end = ((u32)addr->endAddressHi << 16) + addr->endAddressLo;
            u32 current =
                ((u32)addr->currentAddressHi << 16) + addr->currentAddressLo;

            if (end - current < 0xC0) {
                dcsVoiceSetupAdpcm(channel);
            }
        }
    }
}

/* 0x800D321C  assign+start a voice for a sample */
s32 dcsVoicePlay(s32 priority) {
    s32 first = lbl_80345220;
    s32 bestPriority = -1;
    s32 replace = 1;
    s32 selected = -1;
    s32 channel;
    s32 adjustment;
    s32 inUse;

    do {
        channel = lbl_80345220;
        inUse = 0;
        if (ch_info[channel].sample >= 0 || ch_info[channel].duck != 0) {
            inUse = 1;
        } else if (sVoice[channel]->pb.state != 0) {
            inUse = 1;
        }
        if (!inUse) {
            selected = channel;
            replace = 0;
            break;
        }

        if (bestPriority < ch_info[channel].priority) {
            bestPriority = ch_info[channel].priority;
            if (bestPriority <= priority) {
                selected = channel;
            }
        }

        lbl_80345220++;
        if (lbl_80345220 >= 12) {
            lbl_80345220 = 0;
        }
    } while (lbl_80345220 != first);

    if (selected >= 0 && replace) {
        dcsMemUnlock(selected);
        lbl_80345234 &= ~(1 << lbl_80345220);
        if (ch_info[selected].duck != 0) {
            adjustment = -(s32)ch_info[selected].duck * lbl_80343FF8;
            adjustment >>= 8;
            lbl_8034520C += adjustment;
            for (channel = 0; channel < 12; channel++) {
                if (dcsVoiceInUse(channel)) {
                    ch_info[channel].volume -= adjustment;
                    dcsVoiceUpdate(channel);
                }
            }
        }
    }
    return selected;
}

/* 0x800D33B8  configure AX ADPCM voice for a sample */
s32 dcsVoiceSetupAdpcm(s32 channel) {
    DcsChannelInfo* info = &ch_info[channel];
    s32 sample = info->sample;
    s32 adjustment;
    s32 i;

    info->sample = -1;
    if (sample < 0) {
        if (info->duck != 0) {
            adjustment = -(s32)info->duck * lbl_80343FF8;
            adjustment >>= 8;
            lbl_8034520C += adjustment;
            for (i = 0; i < 12; i++) {
                if (dcsVoiceInUse(i)) {
                    ch_info[i].volume -= adjustment;
                    dcsVoiceUpdate(i);
                }
            }
        }
        info->duck = 0;
    } else {
        u16 call = lbl_802F0F60[sample];
        DcsSampleData* data = &lbl_802C9F60[call & 0xFFF];
        AXPBADPCM adpcm;
        AXPBADDR addr;
        u32 start;
        u32 end;

        info->sampleData = data;
        for (i = 0; i < 8; i++) {
            adpcm.a[i][0] = data->coefficients[i * 2];
            adpcm.a[i][1] = data->coefficients[i * 2 + 1];
        }
        adpcm.gain = 0;
        adpcm.pred_scale = data->predScale;
        adpcm.yn1 = 0;
        adpcm.yn2 = 0;
        AXSetVoiceAdpcm(sVoice[channel], &adpcm);

        start = data->aramAddress * 2 + 2;
        end = (data->aramAddress + data->length) * 2 - 1;
        addr.loopFlag = 0;
        addr.format = 0;
        addr.loopAddressHi = start >> 16;
        addr.loopAddressLo = start;
        addr.endAddressHi = end >> 16;
        addr.endAddressLo = end;
        addr.currentAddressHi = start >> 16;
        addr.currentAddressLo = start;
        AXSetVoiceAddr(sVoice[channel], &addr);
        AXSetVoiceSrcType(sVoice[channel], AX_SRC_TYPE_LINEAR);

        if (dcsVoiceStartAx(channel) != 0) {
            pool_free(lbl_802F5F60, data);
            if ((call & 0x2000) != 0) {
                while ((lbl_802F0F60[sample] & 0x4000) == 0) {
                    sample--;
                }
            } else if ((call & 0x8000) != 0) {
                sample = -1;
            } else {
                sample++;
            }
            info->sample = sample;
            if (sample >= 0 || info->duck != 0) {
                lbl_80345234 |= 1 << channel;
            }
        }
    }
    return 0;
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
void dcsSampleCallback(u32 request);

s32 dcsSampleUpload(void* state, u32 uploadArg) {
    u32* p = (u32*)state;
    u32* node = p - 4;

    if (uploadArg == 0) {
        pool_dispose_and_alloc(lbl_802F5F60, node, p[1], 0, lbl_802F5F60);
    } else {
        pool_alloc_at(lbl_802F5F60, node, p[1], uploadArg, lbl_802F5F60);
    }
    if (*node == 0) {
        return 0xfffffffe;
    }
    DCFlushRange((void*)p[0], p[1]);
    dcsSampleBusy = 1;
    ARQPostRequest(&dcsSampleReq, 0, 0, 1, p[0], *node, p[1], dcsSampleCallback);
    while (dcsSampleBusy != 0) {
    }
    return 0;
}

/* 0x800D374C  alloc ARAM + ARQ upload */
s32 dcsSampleAllocUpload(void* sample, s32 arg) {
    s32 found = 0;
    s32 channel;
    u32* p = (u32*)sample;
    u32* puVar8 = p + 4;

    for (channel = 0; channel < 12; channel++) {
        if (dcsVoiceInUse(channel) != 0 &&
            ch_info[channel].sampleData == (DcsSampleData*)sample) {
            found = 1;
            break;
        }
    }
    if (found) {
        return 0xffffffff;
    }
    if (BytesFree() < ((p[1] + 0x3f) & 0xffffffc0)) {
        return 0xfffffffe;
    }
    *puVar8 = (u32)AllocHiMem((p[1] + 0x3f) & 0xffffffc0, p[1]);
    puVar8[1] = (p[1] + 0x3f) & 0xffffffc0;
    puVar8[2] = (u32)puVar8;
    puVar8[3] = (u32)puVar8;
    DCFlushRange((void*)*puVar8, p[1]);
    dcsSampleBusy = 1;
    ARQPostRequest(&dcsSampleReq, 0, 1, 1, p[0], *puVar8, p[1], dcsSampleCallback);
    while (dcsSampleBusy != 0) {
    }
    DCInvalidateRange((void*)*puVar8, p[1]);
    return 0;
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
s32 dcsVoiceStartAx(s32 channel) {
    DcsChannelInfo* info = &ch_info[channel];
    DcsSampleData* data = info->sampleData;
    s32 volume = info->volume;
    u16 master;
    u16 excess;
    s32 scaled;
    s32 pan;
    s32 sign;
    f32 ratio;
    s32 whole;
    AXPBSRC src;

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

    ratio = (f32)(((data->sampleRate << lbl_80345214) * 48000) >> 12) /
            32000.0f;
    whole = (s32)ratio;
    src.ratioHi = whole;
    src.ratioLo = (u16)(65536.0 * (ratio - (f32)whole));
    src.currentAddressFrac = 0;
    src.last_samples[0] = 0;
    src.last_samples[1] = 0;
    src.last_samples[2] = 0;
    src.last_samples[3] = 0;
    AXSetVoiceSrc(sVoice[channel], &src);
    AXSetVoiceState(sVoice[channel], 1);
    return 1;
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
s32 BankReadHeader(void* file, u32* header) {
    char signature[4];
    s32 result;
    s32 byteSwapped;
    u32 word;

    result = 0;
    byteSwapped = 0;
    if (FileBufGet(file, header, 16) != 16) {
        printf(DCS_STRING_POOL);
        printf(DCS_STRING_POOL + 0x148);
        result = -1;
    } else {
        word = header[0];
        signature[0] = (s8)(word >> 24);
        signature[1] = (word >> 16) & 0xFF;
        signature[2] = (word >> 8) & 0xFF;
        signature[3] = word & 0xFF;
        if (strncmp(signature, "VBNK", 4) == 0) {
            byteSwapped = 1;
        } else if (strncmp(signature, "KNBV", 4) == 0) {
            byteSwapped = 0;
        } else {
            printf(DCS_STRING_POOL);
            printf(DCS_STRING_POOL + 0x160);
            result = -2;
        }
    }

    if (result >= 0) {
        if (byteSwapped != 0) {
            header[1] = DCS_SWAP32(header[1]);
            header[2] = DCS_SWAP32(header[2]);
            header[3] = DCS_SWAP32(header[3]);
        }
        if ((s32)header[3] > 0x4FF) {
            printf(DCS_STRING_POOL);
            printf(DCS_STRING_POOL + 0x17C, header[3]);
            result = -5;
        } else {
            switch (header[2] & 0xFFFF) {
            case 0x106:
                if ((s32)FileBufGet(file, header + 4, 4) != 4) {
                    printf(DCS_STRING_POOL);
                    printf(DCS_STRING_POOL + 0x1AC);
                    result = -3;
                } else if (byteSwapped != 0) {
                    header[4] = DCS_SWAP32(header[4]);
                }
                break;
            case 0x100:
                if ((s32)header[1] != (s32)(header[3] * 16)) {
                    printf(DCS_STRING_POOL);
                    printf(DCS_STRING_POOL + 0x1CC);
                    result = -4;
                }
                header[4] = -1;
                break;
            default:
                printf(DCS_STRING_POOL);
                printf(DCS_STRING_POOL + 0x1F4);
                result = -5;
                break;
            }
        }
    }
    return result;
}

/* 0x800D4048  parse in-memory BANK header */
#pragma opt_lifetimes off
s32 BankParseHeader(u32* header, s32* byteSwapped, u32* version) {
    char signature[4];
    s32 result;
    u32 word;

    result = 0;
    word = header[0];
    signature[0] = (s8)(word >> 24);
    signature[1] = (word >> 16) & 0xFF;
    signature[2] = (word >> 8) & 0xFF;
    signature[3] = word & 0xFF;

    if (strncmp(signature, "VBNK", 4) == 0) {
        *byteSwapped = 1;
    } else if (strncmp(signature, "KNBV", 4) == 0) {
        *byteSwapped = 0;
    } else {
        printf(DCS_STRING_POOL);
        printf(DCS_STRING_POOL + 0x160);
        *byteSwapped = 0;
        result = -1;
        *version = 0;
    }

    if (result >= 0) {
        *version = header[2] & 0xFFFF;
        printf("DCS: ");
        printf(DCS_STRING_POOL + 0x20C,
               (header[2] >> 8) & 0xFF,
               header[2] & 0xFF, header[2] >> 24,
               (header[2] >> 16) & 0xFF);
    }
    return result;
}
#pragma opt_lifetimes reset

/* 0x800D415C  parse a VAG sample header */
#pragma opt_lifetimes off
s32 VagParseHeader(void* file, u32* header, DcsSampleData* sample) {
    char signature[4];
    DcsVagExtraHeader extraHeader;
    s32 zero;
    s32 result;
    u32 word;

    zero = 0;
    result = 0;
    word = header[0];
    signature[0] = (s8)(word >> 24);
    signature[1] = (word >> 16) & 0xFF;
    signature[2] = (word >> 8) & 0xFF;
    signature[3] = word & 0xFF;
    sample->predScale = zero;

    if (strncmp(signature, "pGAV", 4) == 0) {
        u32* swappedLength = &sample->swappedLength;
        sample->length = (u32)swappedLength;
        *swappedLength = DCS_SWAP32(header[3]);
        sample->sampleRate = (DCS_SWAP32(header[4]) << 12) / 48000;
        header[1] = DCS_SWAP32(header[1]);
    } else if (strncmp(signature, "VAGp", 4) == 0) {
        sample->swappedLength = sample->length = header[3];
        sample->sampleRate = (header[4] << 12) / 48000;
    } else {
        printf("DCSERROR: ");
        printf("VagParseHeader NOT A VAG!\n");
        sample->sampleRate = zero;
        result = -1;
        sample->length = zero;
        sample->swappedLength = zero;
    }

    if (header[1] == 0x28) {
        if (FileBufGet(file, &extraHeader, 96) != 96) {
            result = -1;
        }
        memcpy(sample->coefficients, (u8*)&extraHeader + 28, 32);
        sample->predScale = *(u16*)((u8*)&extraHeader + 62);
    } else if (header[1] == 0x29) {
        sample->predScale = 1;
    }
    return result;
}
#pragma opt_lifetimes reset

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
