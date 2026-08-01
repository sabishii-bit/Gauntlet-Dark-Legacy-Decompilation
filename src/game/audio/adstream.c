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
    /* 0x0C */ u8 _pad0C[4];
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
    /* 0x60 */ u8 _pad60[0x64 - 0x60];
    /* 0x64 */ u32 blocks;
    /* 0x68 */ u8 _pad68[0x13C - 0x68];
} ADSTREAM;

/*
 * Shared ADS globals.  These live in the module's auto data split at fixed
 * addresses and are also touched cross-TU by the dcs driver, so they are
 * referenced here by their address-keyed lbl_ names (the relocs must resolve
 * to the exact addresses).
 */
extern u8 lbl_80345268;    /* 0x80345268  "voices ready" gate flag */
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
extern s32 sConfig;
extern ADSTREAM gADS;
extern AXVPB* sVoice[14];

extern void* AllocMem(u32 size);
extern s32 FileBufClose(void* file);
extern s32 FileBufOpen(void* file, void* desc);
extern void* FileBufStart(void* desc);
extern void dcsSetStreamFlag(void* stream, s32 looping);
extern void* memset(void* p, int c, u32 n);

void _AdsThread(void);
s32 adsMoveCookedToSpu(ADSTREAM* stream);
s32 adsMoveRawToCooked(ADSTREAM* stream);
s32 adsMoveFileToRaw(ADSTREAM* stream);

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
s32 adsMoveRawToCooked(ADSTREAM* stream) {
}
#pragma dont_inline off

/* 0x800D683C  file -> raw ring: FileBufSeek to the SSbd body / loop point,
 * FileBufGet into the raw ring.  Xbox: adsMoveFileToRaw. */
#pragma dont_inline on
s32 adsMoveFileToRaw(ADSTREAM* stream) {
}
#pragma dont_inline off

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
void _AdsThread(void) {
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
void adsInitFromHeader(void) {
}

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
void AdsPutBuffer(void) {
}

/* 0x800D76A4  (re)start playback / loop: FileBufReopen, read+parse the 0x28
 * header, adsInitFromHeader, prime the ring (adsFeed x2), set status=playing.
 * Xbox: AdsStart. */
void AdsStart(void) {
}

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
void AdsParseHeader(void) {
}
