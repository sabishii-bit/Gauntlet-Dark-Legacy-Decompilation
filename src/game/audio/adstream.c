/*
 * adstream.c - Midway ADSTREAM streaming-audio layer, GameCube port.
 *
 * Text 0x800D6234-0x800D7CFC (Xbox module ADSTREAM.OBJ).  Sits just above the
 * DCS driver TUs (dcs.c/buffile.c/dcsdrv.c/mempool.c, 0xD1E04-0xD621C) and is
 * driven by them plus soundmgr.c/main.c.  It streams a Midway ".ss" file
 * (chunk tags "SShd" header / "SSbd" body) off disk through a triple-buffer
 * pipeline - file -> raw -> cooked -> ARAM - and plays it back on the GameCube
 * AX voices via the sndvoice.c / dcs voice layer.  Replaces the PS2/Xbox SPU
 * DirectSoundStream backend with an AX + ARQ (AR queue) backend.
 *
 * Debug strings tag the module: "DCSERROR: ", "SPU UNDERRUN, loop-glitch
 * likely", "AdsPutBuffer EOF -- %d bytes UNSENT", "AdsPutBuffer overrun! %d
 * bytes UNSENT", "DCS: ".
 *
 * The single global stream lives at gADS (0x80320B00, 0x13C bytes);
 * per-voice AX handles come from sVoice (the dcs.c voice pool).  The global
 * stream-config words (block/frame sizing, ring cursors) live in the .sbss
 * block lbl_80345268..lbl_80345298 (kept in their auto data split - referenced
 * cross-TU by dcs/dcsdrv).
 *
 * NAMES: real ADSTREAM.OBJ names from the Xbox PDB, mapped to the GCN
 * functions by behaviour (callee/string/data anchors) - the GCN link order is
 * NOT the Xbox source order, and the AX port merged/dropped a couple of the
 * SPU-only helpers (26 GCN fns vs 28 Xbox).  Low-confidence / behavioural
 * names are flagged in the per-function comments.  adsPoll is the per-frame
 * entry called by main.c and soundmgr.c.
 *
 * NonMatching: the lifecycle/allocation helpers are reconstructed; the large
 * pipeline movers and command processor remain scaffolds. Extracted bytes are
 * linked from the DOL.
 */
#include "types.h"
#include "dolphin/ax.h"
#include "game/dcs.h"
#include "game/sndvoice.h"

/*
 * The streaming state block (single global gAdsStream, 0x13C bytes).  Only the
 * fields touched by the reconstructed bodies are named; the rest is padding.
 */
typedef struct ADSTREAM {
    /* 0x00 */ u32 mode;               /* mode/flag bits */
    /* 0x04 */ void* file;              /* FileBuf handle */
    /* 0x08 */ void* buffer;            /* stream work/ring buffer */
    /* 0x0C */ void* cookedPtr;
    /* 0x10 */ s32 ringSize;
    /* 0x14 */ s32 ringUsed;
    /* 0x18 */ u32 spuReadBase;
    /* 0x1C */ u32 loopMarker;
    /* 0x20 */ s32 fileRemaining;
    /* 0x24 */ void* ringPtr;
    /* 0x28 */ s32 ringRead;
    /* 0x2C */ s32 ringWrite;
    /* 0x30 */ s32 refillState;
    /* 0x34 */ s32 voice[2];
    /* 0x3C */ s32 vol;                /* pending volume */
    /* 0x40 */ s32 volDirty;           /* "volume changed" flag */
    /* 0x44 */ s32 keyCount;           /* voice-keying counter */
    /* 0x48 */ s32 loopCount;          /* loop/refill counter */
    /* 0x4C */ s32 endCount;           /* end-of-stream counter */
    /* 0x50 */ s32 status;             /* 0 / 0x1000 (playing) / 0x2000 */
    /* 0x54 */ u8 _pad54[0x5C - 0x54];
    /* 0x5C */ u32 sampleBits;
    /* 0x60 */ u32 sampleRate;
    /* 0x64 */ u32 blocks;
    /* 0x68 */ u32 frameAlign;
    /* 0x6C */ u8 _pad6C[0x78 - 0x6C];
    /* 0x78 */ u32 fileLoopSize;
    /* 0x7C */ u8 _pad7C[0x13C - 0x7C];
} ADSTREAM;

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

/*
 * Shared ADS globals.  These live in the module's auto data split at fixed
 * addresses and are also touched cross-TU by the dcs driver, so they are
 * referenced here by their address-keyed lbl_ names (the relocs must resolve
 * to the exact addresses).
 */
extern volatile u8 lbl_80345268; /* 0x80345268  "voices ready" gate flag */
extern void* gBuf;         /* 0x8034526C  the AllocMem work buffer */
extern s32 lbl_80345274;   /* 0x80345274  pending command code */
extern s32 gAddrSpuNext;   /* 0x80345278  ring base cursor */
extern s32 gAddrSpuTop;    /* 0x8034527C  ring end cursor */
extern u32 sizeVoiceLoop;  /* 0x80345280  bytes per frame */
extern u32 halfVoiceLoop;  /* 0x80345284  half-frame bytes */
extern s32 sShortenedSizeVoiceLoop; /* 0x8034528C samples per frame x2 */
extern s32 sShortenedHalfVoiceLoop; /* 0x80345290 samples per frame */
extern s32 lbl_80345270;   /* largest stream allocation seen */
extern u32 lbl_80345288;   /* global ADS flags */
extern s32 lbl_80344694;   /* music duck request */
extern s32 dcsMemLockOwner();
extern s32 sConfig;
extern ADSTREAM gADS;
extern AXVPB* sVoice[14];
extern ARQRequest lbl_80320C3C[];
extern ARQCallback lbl_80345294;

extern void* AllocMem(u32 size);
extern s32 FileBufClose(void* file);
extern s32 FileBufOpen(void* file, void* desc);
extern void* FileBufStart(void* desc);
extern s32 FileBufSeek(void* file, s32 offset, s32 whence);
extern s32 FileBufGet(void* file, void* destination, s32 length);
extern s32 FileBufReopen(void* file);
extern void dcsSetStreamFlag(void* stream, s32 looping);
extern void* memset(void* p, int c, u32 n);
extern void* memcpy(void* destination, const void* source, u32 length);
extern s32 strncmp(const char* lhs, const char* rhs, u32 length);
extern f32 lbl_80349308;   /* SRC ratio divisor */
extern f64 lbl_80349310;   /* SRC fraction scale (65536.0) */
extern f64 lbl_80349318;   /* u32->f64 conversion bias */
extern f64 lbl_80349320;   /* s32->f64 conversion bias */
extern char lbl_801174A8[]; /* "AdsPutBuffer..." message pool */
extern char lbl_80349328[];  /* short EOF tag */
extern char lbl_80349330[5];
extern char lbl_80349338[5];
extern char lbl_80349340[5];
extern char lbl_80349348[5];
extern s32 printf(const char* format, ...);
extern void DCFlushRange(void* address, u32 length);
extern void ARQPostRequest(ARQRequest* request, u32 owner, u32 type,
                           u32 priority, u32 source, u32 destination,
                           u32 length, ARQCallback callback);
extern s32 dcsMemTryLock(u32 address, s32 voice,
                         s32 (*callback)(s32), s32 command);

s32 _AdsThread(void);
s32 AdsStart(ADSTREAM* stream);
s32 adsFeed(ADSTREAM* stream);
void adsArqDone(void);
s32 adsLockCallback(s32 command);
void AdsSetVolumeDirect(ADSTREAM* stream, s32 volume);
s32 adsMoveCookedToSpu(ADSTREAM* stream);
s32 adsMoveRawToCooked(ADSTREAM* stream);
s32 adsMoveFileToRaw(ADSTREAM* stream);
s32 AdsParseHeader(ADSTREAM* stream, u32* header, u32* body);

/* Mark a stream's volume dirty (inlined helper). */
static void adsMarkVol(ADSTREAM* s, s32 vol) {
    if (s != NULL) {
        s->volDirty = 1;
        s->vol = vol;
    }
}

/* 0x800D6234  per-frame poll (main.c/soundmgr.c): reads each AX voice's
 * current playback address and, when the SPU has drained past the refill
 * threshold, queues a refill command (13) to _AdsThread.  Xbox: adsPoll. */
void adsPoll(void) {
    ADSTREAM* stream;
    AXVPB* voice;
    AXPBADDR* addr;
    u32 current;
    s32 refillState;

    if ((lbl_80345288 & 0x2000) != 0) {
        voice = sVoice[13];
        stream = &gADS;
        addr = &voice->pb.addr;
        current = ((u32)addr->currentAddressHi << 16) +
                  addr->currentAddressLo;
        if (stream->sampleBits == 32) {
            current >>= 1;
            refillState = current >= stream->spuReadBase + halfVoiceLoop;
        } else {
            current <<= 1;
            refillState = current >=
                          stream->spuReadBase + sShortenedHalfVoiceLoop;
        }
        if (stream->refillState != refillState || stream->loopCount != 0) {
            lbl_80345274 = 13;
            _AdsThread();
        }
    }
}

/* 0x800D62F0  cooked ring -> ARAM/SPU: ARQPostRequest per block, DCFlushRange,
 * AXSetVoiceAdpcmLoop; prints "DCSERROR: SPU UNDERRUN..." on underrun.
 * Xbox: adsMoveCookedToSpu. */
#pragma dont_inline on
s32 adsMoveCookedToSpu(ADSTREAM* stream) {
    u8 unused[8];
    AXPBADPCMLOOP loop;
    register u32 cookedSize;
    s32 result;
    u32 i;
    u32 transferSize;
    u8* source;
    u32 destination;
    ADSTREAM* self;

    self = stream;
    result = 0;
    if ((cookedSize = self->ringWrite) < halfVoiceLoop &&
        (self->fileRemaining > 0 || self->ringRead > 0)) {
        if (self->status != 0) {
            s32 volume;

            printf("DCSERROR: ");
            printf("SPU UNDERRUN, loop-glitch likely");
            dcsMemTryLock(self->spuReadBase +
                              self->refillState * halfVoiceLoop,
                          self->voice[0], adsLockCallback, 0);
            lbl_80345288 |= 1 << self->voice[0];
            volume = self->vol;
            AdsSetVolumeDirect(self, 0);
            self->vol = volume;
            self->mode |= 0x80;
        }
    } else {
        s32 requestOffset;
        s32 voiceOffset;

        source = self->cookedPtr;
        if (self->sampleBits >= 32) {
            destination = self->spuReadBase +
                          self->refillState * halfVoiceLoop;
        } else {
            destination = self->spuReadBase +
                          self->refillState * sShortenedHalfVoiceLoop;
        }
        transferSize = cookedSize;
        i = 0;
        requestOffset = 0;
        voiceOffset = 0;
        while (i < self->blocks) {
            if (self->refillState == 0) {
                loop.loop_pred_scale = *source;
                loop.loop_yn1 = 0;
                loop.loop_yn2 = 0;
                AXSetVoiceAdpcmLoop(
                    sVoice[*(s32*)((u8*)self + voiceOffset + 0x34)],
                    &loop);
            }
            DCFlushRange(source, transferSize);
            lbl_80345294 = NULL;
            if (i == self->blocks - 1) {
                lbl_80345294 = (ARQCallback)adsArqDone;
            }
            ARQPostRequest((ARQRequest*)((u8*)lbl_80320C3C + requestOffset),
                           0, 0, 1, (u32)source, destination, transferSize,
                           lbl_80345294);
            i++;
            requestOffset += sizeof(ARQRequest);
            source += halfVoiceLoop;
            destination += sizeVoiceLoop;
            voiceOffset += 4;
        }
        if (self->status != 0) {
            if ((self->mode & 0x80) != 0) {
                s32 volume = self->vol;

                self->mode &= ~0x80;
                AdsSetVolumeDirect(self, volume);
            }
            {
                u32 lockAddress = destination - sizeVoiceLoop;

                if (self->ringWrite < halfVoiceLoop) {
                    lockAddress = self->ringWrite + lockAddress;
                    lockAddress -= 16;
                }
                dcsMemTryLock(lockAddress, self->voice[i - 1],
                              adsLockCallback, 0);
            }
            lbl_80345288 |= 0x2000;
        }
        self->ringWrite = 0;
        result = 1;
        self->refillState = (u32)__cntlzw(self->refillState) >> 5;
    }
    return result;
}
#pragma dont_inline off

/* 0x800D657C  ARQ last-block completion callback: sets the "voices ready"
 * gate flag (lbl_80345268 = 1).  Xbox: (SPU/ARQ done callback - behavioural). */
void adsArqDone(void) {
    lbl_80345268 = 1;
}

/* 0x800D6588  raw ring -> cooked ring: wraps/segments the copy across the ring
 * boundary via memcpy, advances the cooked cursors.  Xbox: adsMoveRawToCooked. */
#pragma dont_inline on
#pragma opt_lifetimes off
#pragma opt_propagation off
#pragma opt_common_subs off
s32 adsMoveRawToCooked(ADSTREAM* stream) {
    register u32 divisor;
    u8* rawEnd;
    u32 chunk;
    u8* destination;
    u8* blockDestination;
    u8* source;
    u8* destinationEnd;
    u32 blockIndex;
    u32 copySize;
    s32 padding;
    s32 remaining;
    ADSTREAM* self;
    u32 firstPart;
    u32 available;
    u32 ringWrite;
    s32 ringRead;
    u32 space;
    u32 half;
    u8* initialSource;
    s32 ringSize;
    u32 dead;

    self = stream;
    divisor = self->blocks;
    if (divisor <= 1) {
        divisor = 1;
    } else {
        dead = divisor;
    }

    ringRead = self->ringRead;
    padding = 0;
    initialSource = self->ringPtr;
    source = initialSource;
    available = ringRead / (s32)divisor;
    ringWrite = self->ringWrite;
    half = halfVoiceLoop;
    space = half - ringWrite;
    ringSize = self->ringSize;
    chunk = self->frameAlign;
    rawEnd = (u8*)self->buffer + ringSize;
    destination = (u8*)self->cookedPtr + ringWrite;

    if (available >= space) {
        available = space;
    } else {
        dead = available;
    }
    copySize = available;
    if (copySize < half && ringWrite != half) {
        source = initialSource;
    }
    if (chunk != 0) {
        copySize -= copySize % chunk;
    } else {
        if (self->loopMarker != 0) {
            if (source > (u8*)self->loopMarker) {
                chunk = ((u8*)self->loopMarker + ringSize) - source;
            } else {
                chunk = (u8*)self->loopMarker - source;
            }
            if (chunk > copySize) {
                chunk = copySize;
            }
        } else {
            chunk = copySize;
        }
    }

    remaining = ringRead / (s32)divisor - copySize;
    destinationEnd = destination + copySize;
    while (destination < destinationEnd) {
        if (chunk == 0) {
            copySize = destinationEnd - destination;
            break;
        }

        blockDestination = destination;
        blockIndex = 0;
        while (blockIndex < self->blocks) {
            if (source + chunk > rawEnd) {
                firstPart = rawEnd - source;
                memcpy(blockDestination, source, firstPart);
                memcpy(blockDestination + firstPart, self->buffer,
                       chunk - firstPart);
            } else {
                memcpy(blockDestination, source, chunk);
            }
            source += chunk;
            blockDestination += halfVoiceLoop;
            if (source >= rawEnd) {
                source -= self->ringSize;
            }
            blockIndex++;
        }

        if ((u8*)self->loopMarker != source) {
            destination += chunk;
        } else {
            u8* aligned;
            u32 amount;

            self->loopMarker = amount = 0;
            if (self->frameAlign != 0) {
                if ((s32)(destination - (u8*)self->cookedPtr) >= 16) {
                    for (; (s32)blockIndex > 0; blockIndex--) {
                        blockDestination -= halfVoiceLoop;
                    }
                }
                padding += chunk;
                if (remaining < padding) {
                    destinationEnd -= chunk;
                    copySize -= chunk;
                }
            } else {
                aligned = destination + chunk - 15;
                while (aligned > destination) {
                    amount += 16;
                    aligned -= 16;
                }
                padding += amount;
                if (remaining < padding) {
                    destinationEnd -= amount;
                    copySize -= amount;
                }
                destination = aligned - 1;
                chunk = destinationEnd - destination;
            }
        }
    }

    if ((s32)copySize >= 16) {
        u32 count = self->blocks;
        u8* end = destination;

        for (; count > 0; count--) {
            end += halfVoiceLoop;
        }
    }

    available = divisor * (copySize + padding);
    self->ringRead -= available;
    self->ringPtr = (u8*)self->ringPtr + available;
    if ((u8*)self->ringPtr >= rawEnd) {
        self->ringPtr = (u8*)self->ringPtr - self->ringSize;
    }
    self->ringWrite += copySize;
    return copySize;
}
#pragma opt_lifetimes reset
#pragma opt_propagation reset
#pragma opt_common_subs reset
#pragma dont_inline off

/* 0x800D683C  file -> raw ring: FileBufSeek to the SSbd body / loop point,
 * FileBufGet into the raw ring.  Xbox: adsMoveFileToRaw. */
#pragma opt_propagation off
#pragma dont_inline on
s32 adsMoveFileToRaw(ADSTREAM* stream) {
    s32 result;
    u32 isEmpty;

    isEmpty = (u32)__cntlzw(stream->fileRemaining) >> 5;
    result = isEmpty;
    if (isEmpty != 0) {
        if (stream->endCount == 0 && (stream->mode & 2) != 0) {
            if (stream->sampleBits == 32) {
                FileBufSeek(stream->file,
                            ((stream->blocks * 192) >> 1) + 40, 0);
            } else {
                FileBufSeek(stream->file, 40, 0);
            }
            stream->fileRemaining += stream->fileLoopSize;
        }
    } else {
        s32 offset;
        s32 remaining;
        void* destination;
        s32 amount;

        if (stream->ringRead > 0) {
            memcpy(stream->buffer, stream->ringPtr, stream->ringRead);
            if (stream->loopMarker != 0) {
                stream->loopMarker -=
                    (u32)stream->ringPtr - (u32)stream->buffer;
            }
        }
        if (stream->ringRead != stream->ringSize - sizeVoiceLoop) {
            result = 0;
        }
        stream->ringPtr = stream->buffer;
        offset = stream->ringRead;
        remaining = stream->fileRemaining;
        amount = stream->ringSize - offset;
        amount = amount < remaining ? amount : remaining;
        destination = (u8*)stream->ringPtr + offset;
        amount = FileBufGet(stream->file, destination, amount);
        stream->fileRemaining -= amount;
        stream->ringRead += amount;
        if (stream->ringRead != stream->ringSize) {
            result = 0;
        }
        if (stream->fileRemaining == 0) {
            stream->loopMarker =
                (u32)stream->ringPtr + stream->ringRead;
        }
    }
    return result;
}
#pragma dont_inline off
#pragma opt_propagation reset

/* 0x800D69B8  pump one pipeline cycle: cooked->spu, file->raw, raw->cooked,
 * then handle loop wrap.  Xbox: adsFeed. */
s32 adsFeed(ADSTREAM* stream) {
    s32 result = 0;
    u32 previousMarker;
    s32 remaining;

    adsMoveCookedToSpu(stream);
    if (stream->file != NULL) {
        adsMoveFileToRaw(stream);
    }
    previousMarker = stream->loopMarker;
    adsMoveRawToCooked(stream);
    remaining = stream->fileRemaining;
    if (remaining <= 0 && stream->ringRead <= 0) {
        result = 1;
    } else if (previousMarker != 0 && stream->loopMarker == 0) {
        result = 1;
    }
    if (stream->file != NULL) {
        s32* file = stream->file;

        if ((remaining & ~0xF) != ((file[3] + file[5]) & ~0xF) &&
            (stream->mode & 0x80) != 0) {
            stream->loopCount++;
            stream->ringWrite = 0;
        }
    }
    return result;
}

/* 0x800D6A8C  command/state processor: consumes the pending command
 * (lbl_80345274), walks the voices (dcsMemLockOwner / AXSetVoiceState) and
 * dispatches start/stop/loop by stream state (+0x50: 0/0x1000/0x2000).
 * Xbox: _AdsThread (no real thread on GCN - runs synchronously). */
#pragma dont_inline on
s32 _AdsThread(void) {
    ADSTREAM* s;
    s32 v;
    s32 count;
    s32 j;
    u32 i;
    ADSTREAM* base = &gADS;
    s32 sv;
    u8 unused[8];

    s = 0;
    v = lbl_80345274;
    lbl_80345274 = -1;
    j = 0;
    for (count = 0; count < 2; count++) {
        if (v == base->voice[j]) {
            s = base;
            break;
        }
        j++;
    }
    if (s == 0) {
        return 0;
    }
    switch (s->status) {
    case 0x2000:
        dcsMemLockOwner(0, 0);
        if (s->keyCount != 0) {
            i = 0;
            s->keyCount = i;
            s->status = 0x1000;
            if (s->endCount != 0) {
                s->endCount = i;
            } else {
                s->mode &= ~0x80;
                AdsSetVolumeDirect(s, s->vol);
                for (; i < s->blocks; i++) {
                    while (lbl_80345268 == 0) {
                    }
                    AXSetVoiceState(sVoice[s->voice[i]], 1);
                }
            }
            lbl_80345288 |= 0x2000;
            dcsMemLockOwner(0, 1);
            adsFeed(s);
        } else {
            dcsMemLockOwner(0);
        }
        break;
    case 0x1000:
        dcsMemLockOwner(0, 0);
        lbl_80345288 &= ~0x2000;
        if (s->volDirty != 0) {
            s->volDirty = 0;
            AdsSetVolumeDirect(s, s->vol);
        }
        if (s->loopCount != 0) {
            s->loopCount = 0;
            if (s->endCount != 0) {
                gAddrSpuNext -= sizeVoiceLoop * s->blocks;
                s->status = 0;
                AdsStart(s);
            } else {
                s->status = 0x2000;
                for (i = 0; i < s->blocks; i++) {
                    AXSetVoiceState(sVoice[s->voice[i]], 0);
                }
                sv = s->vol;
                AdsSetVolumeDirect(s, 0);
                s->vol = sv;
                s->mode |= 0x80;
                sConfig = 0;
            }
        } else {
            if (s->endCount != 0) {
                s->fileRemaining = 0;
                s->ringRead = 0;
            }
            if (adsFeed(s) != 0) {
                if (s->endCount == 0 && (s->mode & 2) != 0) {
                    sConfig = 3;
                } else {
                    lbl_80345288 &= ~0x2000;
                    if (s->endCount == 0) {
                        sConfig = 0;
                    }
                    s->loopCount++;
                }
            }
            lbl_80345288 |= 0x2000;
            dcsMemLockOwner(0, 1);
        }
        break;
    }
    return 0;
}
#pragma dont_inline off

/* 0x800D6D80  return stream->status (+0x50).  Xbox: AdsGetStatus. */
s32 AdsGetStatus(ADSTREAM* s) {
    return s->status;
}

/* 0x800D6D88  return stream ? stream->mode (+0x00) : 0.  Xbox: AdsGetMode. */
s32 AdsGetMode(ADSTREAM* s) {
    if (s != NULL) {
        return s->mode;
    }
    return 0;
}

/* 0x800D6DA0  rewrite the stream mode/flag bits and mark the volume dirty.
 * Xbox: AdsSetMode. */
void AdsSetMode(ADSTREAM* s, u32 mode) {
    if (s == NULL) {
        return;
    }
    s->mode &= 0xFFF0;
    s->mode &= 0xFF0F;
    s->mode |= mode;
    adsMarkVol(s, s->vol);
}

/* 0x800D6DE8  apply volume/pan immediately: mono (1 voice) / stereo (2 voice)
 * panning via dcsVoiceSetMaster + sndVoiceSetVolume.  Xbox: AdsSetVolumeDirect. */
void AdsSetVolumeDirect(ADSTREAM* stream, s32 volume) {
    s32 mono;

    if (stream != NULL) {
        stream->vol = volume;
        if ((stream->mode & 0x80) == 0) {
            if (stream->blocks == 2) {
                if ((stream->mode & 0x10) != 0) {
                    mono = ((((u32)volume >> 16) * 0x2D40) >> 14);
                    dcsVoiceSetMaster(stream->voice[0], mono, mono);
                    mono = (s32)(((u32)(volume & 0xFFFF) * 0x2D40) >> 14);
                    dcsVoiceSetMaster(stream->voice[1], mono, mono);
                } else {
                    dcsVoiceSetMaster(stream->voice[0],
                                      (u32)volume >> 16, 0);
                    dcsVoiceSetMaster(stream->voice[1],
                                      0, volume & 0xFFFF);
                }
                sndVoiceSetVolume(sVoice[stream->voice[0]], 0);
                sndVoiceSetVolume(sVoice[stream->voice[1]], 0x7F);
            } else if (stream->blocks == 1) {
                mono = (s32)(((((u32)volume >> 16) +
                                (volume & 0xFFFF)) >> 1) * 0x2D40) >> 14;
                dcsVoiceSetMaster(stream->voice[0], mono, mono);
                sndVoiceSetVolume(sVoice[stream->voice[0]], 0x40);
            }
        }
    }
}

/* 0x800D6F18  queue a volume change (stream->vol = v; dirty = 1) for the next
 * update.  Xbox: AdsSetVolume. */
void AdsSetVolume(ADSTREAM* s, s32 vol) {
    if (s == NULL) {
        return;
    }
    s->volDirty = 1;
    s->vol = vol;
}

/* 0x800D6F30  set up each AX voice from the SShd header: sample-rate ratio
 * (AXSetVoiceSrc), ADPCM coefficients/gain/loop (AXSetVoiceAdpcm),
 * AXSetVoiceAddr/SrcType/Type.  Xbox: adsInitFromHeader. */
#pragma dont_inline on
void adsInitFromHeader(ADSTREAM* stream) {
    f64 kscale;
    f64 kmag2;
    f32 kdiv;
    f64 kmagU;
    u32 k48;
    u32 aram;
    s32 vnum;
    u32 i;
    u32 t;
    u32 bits;
    f32 ratio;
    u32 cur;
    u32 end;
    s32 j;
    u8* ch;
    u32 cvr[2];
    u32 cvs[2];
    u16 addr[8];
    u16 srcb[8];
    u16 adp[20];

    k48 = 48000;
    kscale = lbl_80349310;
    kdiv = lbl_80349308;
    aram = stream->spuReadBase;
    vnum = 13;
    for (i = 0; i < stream->blocks; i++) {
        bits = stream->sampleBits;
        if (bits >= 32) {
            t = ((stream->sampleRate << 12) / k48) * k48;
            ratio = (f32)(t >> 12);
        } else {
            t = ((stream->sampleRate << 12) / k48) * 12000;
            ratio = (f32)(t >> 12);
        }
        if (bits == 32) {
            ch = (u8*)stream + i * 96;
            for (j = 0; j < 8; j++) {
                adp[j * 2] = *(u16*)(ch + j * 4 + 152);
                adp[j * 2 + 1] = *(u16*)(ch + j * 4 + 154);
            }
            adp[16] = *(u16*)(ch + 184);
            adp[17] = *(u16*)(ch + 186);
            adp[18] = *(u16*)(ch + 188);
            adp[19] = *(u16*)(ch + 190);
            AXSetVoiceAdpcm(sVoice[vnum], (AXPBADPCM*)adp);
            addr[0] = 1;
            addr[1] = 0;
            end = (aram + sizeVoiceLoop) * 2 - 1;
            cur = aram * 2 + 2;
        } else {
            addr[0] = 1;
            addr[1] = 10;
            end = (aram + sShortenedSizeVoiceLoop) >> 1;
            cur = aram >> 1;
        }
        ratio = ratio / kdiv;
        srcb[0] = (u16)(s32)ratio;
        {
            f32 whole = (f32)(s32)ratio;
            srcb[1] = (u16)(s32)(kscale * (ratio - whole));
        }
        srcb[2] = 0;
        srcb[3] = 0;
        srcb[4] = 0;
        srcb[5] = 0;
        srcb[6] = 0;
        AXSetVoiceSrc(sVoice[vnum], (AXPBSRC*)srcb);
        addr[2] = cur >> 16;
        addr[3] = cur;
        addr[4] = end >> 16;
        addr[5] = end;
        addr[6] = cur >> 16;
        addr[7] = cur;
        AXSetVoiceAddr(sVoice[vnum], (AXPBADDR*)addr);
        AXSetVoiceSrcType(sVoice[vnum], 1);
        AXSetVoiceType(sVoice[vnum], 1);
        stream->voice[i] = vnum;
        vnum = vnum - 1;
        aram += sizeVoiceLoop;
    }
}
#pragma dont_inline off

/* 0x800D719C  if playing (status==0x1000) reset the voice-keying counters and
 * bump the loop counter.  Xbox: AdsKeyVoices (behavioural mapping). */
s32 AdsKeyVoices(ADSTREAM* s) {
    s32 ret = -1;
    if (s->status == 0x1000) {
        s->keyCount = 0;
        s->endCount = 0;
        s->loopCount++;
        ret = 0;
    }
    return ret;
}

/* 0x800D71D0  per-stream service tick: advance frame counters by state, kick a
 * refill command on end-of-stream.  Xbox: (per-stream update - behavioural). */
s32 adsUpdateStream(ADSTREAM* stream) {
    s32 result = 0;

    if (stream == NULL) {
        return 0;
    }
    switch (stream->status) {
    case 0x1000:
        if (stream->endCount != 0) {
            stream->keyCount++;
        }
        result = -1;
        break;
    case 0:
        stream->keyCount++;
        if (stream->endCount == 0) {
            stream->loopCount = 0;
        }
        break;
    case 0x2000:
        stream->keyCount++;
        if (stream->endCount == 0) {
            stream->loopCount = 0;
        }
        lbl_80345274 = 13;
        _AdsThread();
        break;
    }
    sConfig = 1;
    return result;
}

/* 0x800D72AC  submit a decoded buffer into the pipeline: parse header on the
 * first buffer (AdsParseHeader), init voices (adsInitFromHeader), feed the
 * ring; prints "AdsPutBuffer EOF/overrun -- %d bytes UNSENT".
 * Xbox: AdsPutBuffer. */
s32 AdsPutBuffer(ADSTREAM* s, u8* src, s32 len) {
    char* strs = lbl_801174A8;
    s32 hres = 0;
    u8* wrEnd;
    u8* wp;
    s32 tail;
    s32 over;
    s32 amt;
    s32 amt16;
    s32 part;
    s32 cofsz;
    u8* dst;
    s32 saved;
    u8 unused[8];

    wrEnd = (u8*)s->buffer + s->ringSize;
    wp = (u8*)s->ringPtr + s->ringRead;
    if (wp > wrEnd) {
        wp -= s->ringSize;
    }
    tail = wrEnd - wp;
    over = (s->ringRead + len) - s->ringSize;
    if ((u32)len >= (u32)s->fileRemaining && s->fileRemaining > 0) {
        printf(lbl_80349328);
        printf(strs + 48, len - s->fileRemaining);
        amt = s->fileRemaining;
        amt16 = (s->fileRemaining + 15) & ~15;
    } else {
        if (over > 0) {
            len -= over;
        }
        amt16 = len & ~15;
        if (over > 0) {
            printf(strs);
            printf(strs + 88, len - amt16);
        }
        amt = amt16;
        len = amt16;
    }
    if (amt16 > 0) {
        if (amt16 > tail) {
            memcpy(wp, src, tail);
            memcpy(s->buffer, src + tail, amt16 - tail);
        } else {
            memcpy(wp, src, amt16);
        }
    }
    s->ringRead += amt;
    s->fileRemaining -= amt;
    s->loopMarker = (u32)s->ringPtr + s->ringRead;
    if (s->loopMarker > (u32)s->buffer + s->ringSize) {
        s->loopMarker -= s->ringSize;
    }
    if (s->fileRemaining + 40 > 0) {
        goto done;
    }
    s->fileRemaining += 40;
    dst = (u8*)s + 0x54;
    if ((u8*)s->ringPtr + 40 > wrEnd) {
        part = wrEnd - (u8*)s->ringPtr;
        memcpy(dst, s->ringPtr, part);
        memcpy(dst + part, s->buffer, 40 - part);
        s->ringPtr = (u8*)s->ringPtr - (s->ringSize - 40);
        s->ringRead -= 40;
    } else {
        memcpy(dst, s->ringPtr, 40);
        s->ringPtr = (u8*)s->ringPtr + 40;
        s->ringRead -= 40;
    }
    cofsz = (s->blocks * 192) >> 1;
    dst = (u8*)s + 0x7C;
    s->fileRemaining += cofsz;
    if ((u8*)s->ringPtr + cofsz > wrEnd) {
        part = wrEnd - (u8*)s->ringPtr;
        memcpy(dst, s->ringPtr, part);
        memcpy(dst + part, s->buffer, cofsz - part);
        s->ringPtr = (u8*)s->ringPtr - (s->ringSize - cofsz);
        s->ringRead -= cofsz;
    } else {
        memcpy(dst, s->ringPtr, cofsz);
        s->ringPtr = (u8*)s->ringPtr + cofsz;
        s->ringRead -= cofsz;
    }
    saved = s->sampleBits;
    s->sampleBits = 16;
    hres = AdsParseHeader(s, (u32*)((u8*)s + 0x54), (u32*)((u8*)s + 0x74));
    s->sampleBits = saved;
    if (hres < 0) {
        len = hres;
        goto done;
    }
    hres = -1;
    if ((u32)(gAddrSpuNext + sizeVoiceLoop * s->blocks) <= (u32)gAddrSpuTop &&
        sizeVoiceLoop - (sizeVoiceLoop / s->frameAlign) * s->frameAlign == 0) {
        s->spuReadBase = gAddrSpuNext;
        gAddrSpuNext = gAddrSpuNext + sizeVoiceLoop * s->blocks;
        s->ringSize += s->ringUsed;
        s->ringUsed = halfVoiceLoop * s->blocks;
        s->ringSize -= s->ringUsed;
        s->cookedPtr = (u8*)s->buffer + s->ringSize;
        adsInitFromHeader(s);
        s->fileRemaining += s->fileLoopSize;
        hres = s->fileLoopSize;
    }
    if (hres < 0) {
        len = hres;
    }
done:
    if (amt > 0 && hres >= 0) {
        switch (s->status) {
        case 0:
            if (adsMoveRawToCooked(s) == 0) {
                adsMoveCookedToSpu(s);
                adsMoveRawToCooked(s);
                s->status = 0x2000;
            }
            break;
        case 0x2000:
            adsMoveRawToCooked(s);
            break;
        }
    }
    return len;
}

/* 0x800D76A4  (re)start playback / loop: FileBufReopen, read+parse the 0x28
 * header, adsInitFromHeader, prime the ring (adsFeed x2), set status=playing.
 * Xbox: AdsStart. */
#pragma opt_propagation off
s32 AdsStart(ADSTREAM* stream) {
    s32 parseResult;
    u32 setupResult;
    u32 frameSize;
    u32 aramNext;
    s32 result = -1;

    if (stream->file != NULL) {
        FileBufReopen(stream->file);
        if (stream->status == 0x1000) {
            result = 0;
        } else if (stream->status == 0) {
            lbl_80345268 = 0;
            if (FileBufGet(stream->file, (u8*)stream + 0x54, 40) == 40) {
                stream->fileRemaining = 0;
                parseResult =
                    AdsParseHeader(stream, (u32*)(&stream->status + 1),
                                   (u32*)((u8*)stream + 0x74));
                if (parseResult >= 0) {
                    setupResult = -1;
                    frameSize = sizeVoiceLoop;
                    aramNext = gAddrSpuNext;
                    if (aramNext + frameSize * stream->blocks <=
                            (u32)gAddrSpuTop &&
                        frameSize % stream->frameAlign == 0) {
                        stream->spuReadBase = aramNext;
                        gAddrSpuNext += sizeVoiceLoop * stream->blocks;
                        stream->ringSize += stream->ringUsed;
                        stream->ringUsed = halfVoiceLoop * stream->blocks;
                        stream->ringSize -= stream->ringUsed;
                        stream->cookedPtr =
                            (u8*)stream->buffer + stream->ringSize;
                        adsInitFromHeader(stream);
                        stream->fileRemaining += stream->fileLoopSize;
                        setupResult = stream->fileLoopSize;
                    }
                    if ((s32)setupResult >= 0) {
                        if (stream->endCount == 0) {
                            stream->refillState = 0;
                        }
                        if (stream->ringWrite == 0) {
                            adsFeed(stream);
                        }
                        adsFeed(stream);
                        stream->status = 0x2000;
                        if (stream->keyCount != 0) {
                            lbl_80345274 = 13;
                            _AdsThread();
                        }
                        result = 0;
                    }
                }
            }
        }
    }
    return result;
}
#pragma opt_propagation reset

/* 0x800D7848  close the stream file (FileBufClose) and release its ring space.
 * Xbox: AdsClose. */
void AdsClose(ADSTREAM* s) {
    if (s->file != 0) {
        FileBufClose(s->file);
        s->file = 0;
    }
    s->ringSize += s->ringUsed;
    s->ringUsed = 0;
    gAddrSpuNext -= sizeVoiceLoop * s->blocks;
}

/* 0x800D78B8  open the stream file (FileBufOpen/FileBufStart) and register it
 * (dcsSetStreamFlag).  Xbox: AdsOpen. */
s32 AdsOpen(ADSTREAM* s, void* desc) {
    s32 result = -1;

    if (s->file != 0) {
        if (FileBufOpen(s->file, desc) >= 0) {
            if (s->status == 0x2000) {
                lbl_80345288 &= ~0x2000;
                s->status = 0;
                gAddrSpuNext -= sizeVoiceLoop * s->blocks;
            } else {
                s->endCount++;
            }
            result = 1;
        }
    } else {
        s->file = FileBufStart(desc);
        if (s->file != 0) {
            dcsSetStreamFlag(s->file, 1);
            result = 1;
        }
    }
    return result;
}

/* 0x800D7974  clear a stream slot (memset 0x13C) and drop its global flag bit.
 * Xbox: AdsDelete. */
s32 AdsDelete(ADSTREAM* s) {
    s32 result = 0;

    if (s->buffer == 0) {
        result = -1;
    }
    lbl_80345288 &= ~0x2000;
    memset(s, 0, sizeof(ADSTREAM));
    return result;
}

/* 0x800D79C8  allocate/init the global stream from the work buffer, memset the
 * 0x13C struct and set the ring pointers/size.  Xbox: AdsNew. */
ADSTREAM* AdsNew(s32 size) {
    ADSTREAM* result = 0;
    ADSTREAM* s = &gADS;
    s32 active;
    s32 minimum;

    lbl_80345288 &= ~0x2000;
    minimum = sizeVoiceLoop << 2;
    active = 0;
    if (s->buffer != 0) {
        active = 1;
    }
    if (active >= 1) {
        goto done;
    }
    if (size < minimum) {
        goto done;
    }
    memset(s, 0, sizeof(ADSTREAM));
    if (size > lbl_80345270) {
        if (gBuf != 0) {
            lbl_80345270 = size;
            s->buffer = gBuf;
        }
    } else {
        s->buffer = gBuf;
    }
    if (s->buffer != 0) {
        s->ringSize = size;
        result = s;
        s->ringPtr = s->buffer;
        s->ringWrite = 0;
        s->ringRead = 0;
        s->file = 0;
    }

done:
    return result;
}

/* 0x800D7AA4  allocate the 0x40000-byte ADS work buffer via AllocMem.
 * Xbox: AdsAllocBuffer. */
void AdsAllocBuffer(void) {
    gBuf = AllocMem(0x40000);
}

/* 0x800D7ACC  empty.  Xbox: AdsQuit. */
void AdsQuit(void) {
}

/* 0x800D7AD0  compute the global block/frame sizing config from
 * (base, frameBytes, blocks): bytes-per-frame, half-frame, ring cursors,
 * samples.  Xbox: AdsInit. */
s32 AdsInit(s32 base, s32 frameBytes, s32 blocks) {
    sizeVoiceLoop = frameBytes;
    frameBytes *= blocks;
    gAddrSpuNext = base;
    halfVoiceLoop = sizeVoiceLoop >> 1;
    gAddrSpuTop = base + frameBytes;
    sShortenedHalfVoiceLoop = (halfVoiceLoop >> 4) * 14;
    sShortenedSizeVoiceLoop = sShortenedHalfVoiceLoop << 1;
    return 1;
}

/* 0x800D7B14  DCS mem-lock completion callback: latches a command
 * (lbl_80345274) and runs _AdsThread.  Xbox: (lock callback - behavioural). */
s32 adsLockCallback(s32 command) {
    lbl_80345274 = command;
    _AdsThread();
    return 1;
}

/* 0x800D7B3C  parse the ".ss" header: match the "SShd"/"SSbd" chunk tags
 * (strncmp), byte-swap the header fields, and FileBufGet the ADPCM coef table.
 * Xbox: AdsParseHeader. */
s32 AdsParseHeader(ADSTREAM* stream, u32* header, u32* body) {
    s32 result = 0;
    u32 headerTag = header[0];
    u32 bodyTag = body[0];
    u8 headerName[4];
    u8 bodyName[4];
    u32 value;

    headerName[0] = (s8)(headerTag >> 24);
    bodyName[0] = (s8)(bodyTag >> 24);
    headerName[1] = (headerTag >> 16) & 0xFF;
    bodyName[1] = (bodyTag >> 16) & 0xFF;
    headerName[2] = (headerTag >> 8) & 0xFF;
    bodyName[2] = (bodyTag >> 8) & 0xFF;
    headerName[3] = headerTag;
    bodyName[3] = bodyTag;

    if (strncmp((char*)headerName, lbl_80349330, 4) == 0) {
        if (strncmp((char*)bodyName, lbl_80349338, 4) != 0) {
            result = -1;
        }
    } else if (strncmp((char*)headerName, lbl_80349340, 4) == 0) {
        if (strncmp((char*)bodyName, lbl_80349348, 4) != 0) {
            result = -1;
        } else {
            value = header[1];
            header[1] = (value << 24) | ((value << 8) & 0x00FF0000) |
                        (value >> 24) | ((value >> 8) & 0x0000FF00);
            value = header[2];
            header[2] = (value << 24) | ((value << 8) & 0x00FF0000) |
                        (value >> 24) | ((value >> 8) & 0x0000FF00);
            value = header[3];
            header[3] = (value << 24) | ((value << 8) & 0x00FF0000) |
                        (value >> 24) | ((value >> 8) & 0x0000FF00);
            value = header[4];
            header[4] = (value << 24) | ((value << 8) & 0x00FF0000) |
                        (value >> 24) | ((value >> 8) & 0x0000FF00);
            value = header[5];
            header[5] = (value << 24) | ((value << 8) & 0x00FF0000) |
                        (value >> 24) | ((value >> 8) & 0x0000FF00);
            value = body[1];
            body[1] = (value << 24) | ((value << 8) & 0x00FF0000) |
                        (value >> 24) | ((value >> 8) & 0x0000FF00);
        }
    } else {
        result = -1;
    }

    if (header[4] != 1 && header[4] != 2) {
        result = -1;
    }
    if (result < 0) {
        header[1] = 0;
        header[2] = 0;
        header[3] = 0;
        header[4] = 0;
        header[5] = 0;
    }
    if (header[2] == 32) {
        FileBufGet(stream->file, (u8*)stream + 0x7C,
                   (header[4] * 192) >> 1);
    }
    return result;
}
