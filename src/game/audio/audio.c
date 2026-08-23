#include "types.h"

/*
 * Game audio core -- upper slice (Xbox AUDIO.OBJ port), text 0x800160F0-0x800183FC.
 *
 * This is the mid-level game audio API that sits on top of the sndCmd / sndSys
 * message driver in game/audio/soundmgr.c and feeds the high-level speech/music
 * helpers in game/sound/sounds.c.  The lower slice of the same module (the
 * sndFx / sndVoice track helpers, text 0x800150CC-0x800160F0) is a separate TU
 * (sndfx.c) that shares this module's state (the 12-entry track table
 * sAudioChanUpdate, the disable flag sAudioSuspend, the pending-command counter
 * sAudioMute, and the loaded-ROM root sAudioBankTable).
 *
 * Module identity is confirmed from the retained AUDIO.OBJ debug strings:
 *   "AUDIO: UNABLE TO FIND MODE %s"      -> AudioSetMode
 *   "AudioStreamStop: Timeout"           -> AudioStreamStop
 *   "DCS Audio Bank load failed:%s..."   -> AudioLoadComplete
 *   "AudioUnloadPart skipped bank..."    -> AudioUnloadPart
 *   "Audio Reset Error"                  -> AudioReset
 *   "AUDIO sfx volume can't be..."       -> AudioSetVolSfx
 *   "AUDIO music volume can't be..."     -> AudioSetVolMusic
 *   "UNABLE TO FIND SOUND: %s"           -> AudioFindSound
 *   "audatps2.rom"                       -> AudioLoadRom (GC endian fix-up)
 * The remaining names (AudioSetTrackPan/Vol, AudioMask../AudioKill.., AudioAng,
 * AudioBankLoadName, AudioLoadPart, AudioStreamPlay, AudioClearTracks,
 * AudioTrackRegister, ...) are behavioural: exact Midway identifiers for these
 * are unconfirmed but they map 1:1 onto AUDIO.OBJ by call-graph and data use.
 *
 * NonMatching: original DOL bytes are linked (dtk substitutes).  Most bodies are
 * now full reconstructions from the target asm; ~20 functions are byte-identical
 * (a few only modulo compiler float-pool constant names).  The remaining diffs
 * are register-allocation, addressing-mode (indexed vs displacement) or shared-
 * rodata string-pool residuals that a NonMatching TU cannot resolve.  The one
 * large body left as a documented stub is AudioLoadRom (0x508, an unrolled
 * LE->BE descriptor-tree byte-swap).
 */

/* ---- message driver (game/audio/soundmgr.c) ---- */
extern s32  sndRegisterList(void* list, s32 kind);
extern void sndCmd1(void);
extern void sndCmd3(s32 a);
extern s32  sndCmd4();   /* DCS async load request; arg shape varies by caller */
extern void sndCmd6(void);
extern s32  sndCmd7(s16 a, u16* b, u16* c);
extern s32  sndCmd8(u16* a, s32 b, s32 c);
extern s32  sndCmdA(u16 a, s32 b, s32 c, void* d);
extern void sndCmdB(void);
extern s32  sndCmdC(void);
extern void sndCmd18(s16 a);
extern s32  sndSysUpdate(f32 t);
extern void sndSysSetBit0(s32 v);
extern void sndSysSetBit1(s32 v);
extern void sndDeferSlot(void* a, s32 b);
extern void sndTestAcquire(s32 v);
extern s32  sndSysFrameCallback(void);

/* ---- support / other TUs ---- */
extern void* AllocFile(const char* name, void* param);
extern s32   FileMap();  /* variadic-ish DCS file mapper; arg shape varies by caller */
extern void  FreeHiMem(s32 which);
extern void  FatalError(const char* fmt, s32 a);
extern void  ErrorPrintf(const char* fmt, ...);
extern void  bulletproof_printf(const char* fmt, ...);
extern int   sprintf(char* buf, const char* fmt, ...);
extern int   strncmp(const char* a, const char* b, u32 n);
extern void  serve_busy(s32 a);       /* per-frame service pump */
extern f32   NormalVector(void* vec);  /* normalize vector, return approximate length */
extern void  sndFxInitVoices(void);    /* sndfx.c (0x80015C48): reset voices */
extern void  FatalErrorf(const char* fmt, ...); /* fatal/debug printer */
extern void  MBNewWorldPsys(s32 a, void* out, s32 arg, s32 b, s32 c, void* v);

/* lower-slice (sndfx.c) killers used when tearing tracks down */
extern s32 AudioKillMask(s32 mask);

/* ---- module state (names taken verbatim from GUNE5D symbols.txt so the
 *      reloc stream matches; shared with sndfx.c / soundmgr.c) ---- */
extern volatile u32 pbLoad;      /* frame timestamp (ticks); re-read each use */
extern s32  sAudioInitFlag;      /* 0x80344308: 1 while audio is paused/muted */
extern s32  sFlags;              /* 0x803445CC: packed config flags */
extern s64  gControllerButtons;

extern s32  sAudioSuspend;       /* 0x803442A0: nonzero => audio suspended/off */
extern s32  lbl_803442A4;        /* 0x803442A4: stream-active flag */
extern s32  lbl_803442A8;        /* 0x803442A8: hi-mem service lock */
extern s32  sAudioOverride;      /* 0x803442AC */
extern s32  sAudioQueBusy;       /* 0x803442FC: reentrancy guard for list ops */
extern s32  sAudioMute;          /* 0x80344300: pending/blocked command flag */
extern s32  lbl_803442B4;        /* 0x803442B4: busy-wait scratch accumulator */
extern u8*  sAudioBankTable;     /* 0x803442B0: loaded audatps2.rom root */
extern s32* gAudioBankTbl;       /* 0x80344304: current mode's bank table */
extern s32  lbl_803442C8;        /* 0x803442C8: bank load-error count */
extern s32  lbl_803442CC;        /* 0x803442CC: reset error count */
extern s32  lbl_803442C4;        /* 0x803442C4: soft-reset count */
extern s32  sAudioErrFlags;      /* 0x803442C0: error/status bitfield */
extern s32  sAudioReady;         /* 0x803442D0: last sndSysUpdate response */
extern s32  sAudioTimeoutAcc;    /* 0x803442B8 */
extern s32  sAudioTimeoutErrs;   /* 0x803442BC */
extern s32  lbl_803442EC;        /* 0x803442EC */
extern s32  lbl_803442F0;        /* 0x803442F0 */
extern s32  lbl_803442D4;        /* 0x803442D4 */
extern s32  sAudioQueCount[2];   /* 0x803442E4 */
extern f32  sAudioQueFade[2];    /* 0x803442DC */
extern s32  lbl_80343B4C;        /* music volume, 0..255 */
extern s32  lbl_80343B48;        /* sfx volume, 0..255 */
extern s32  sMusicField2F4;      /* 0x803442F4: one-shot stream end counter */
extern s32  lbl_803449A8;        /* 0x803449A8: extra suspend companion flag */
extern const char lbl_803459B0; /* "ALL" (default startup mode) */
extern const char lbl_803459A0[]; /* "streams" (stream file group) */
extern const char lbl_80345990; /* "audio" (bank file group) */
extern const char lbl_80345998[]; /* "%s.vbk" (bank filename format) */
extern const u8 lbl_80111348[];   /* 0xB4-byte AllocFile load descriptor */

/* sdata2 constants */
extern const f32 lbl_80345930;   /* 0.0f */
extern const f32 lbl_80345940;   /* 60.0f (seconds -> frames) */
extern const f32 lbl_80345950;   /* 1.0f */
extern const f32 lbl_80345960;   /* 20.0f (pan projection scale) */
extern const f64 lbl_803459A8;   /* 400000000.0 (reset spin budget) */

/* shared .rodata format/message strings (owned by another TU; referenced by
 * label so the reloc stream matches the original) */
extern const char sAudioTimeoutMsg[];       /* "Audio Play Timeout" */
extern const char sAudioBankNotLoadedMsg[]; /* "AUDIO: BANK %s NOT LOADED. SOUND:%s\n" */
extern const char lbl_80111304[];           /* "AUDIO: UNABLE TO FIND MODE %s" */
extern const char lbl_80111324[];           /* "RESETTING AUDIO AND TRYING AGAIN" */
extern const char lbl_801113FC[];           /* "aud_stream_stop failed: %d" */
extern const char lbl_80111418[];           /* "AudioStreamStop: Timeout" */
extern const char lbl_80111434[];           /* "DCS Audio Bank load failed:%s (%d): %d" */
extern const char lbl_8011145C[];           /* "AudioUnloadPart skipped bank, still in use\n" */
extern const char lbl_80111488[];           /* "Audio Reset Error\n" */
extern const char lbl_8011149C[];           /* "Audio Busy = -2" */
extern const char lbl_801114AC[];           /* "AUDIO sfx volume can't be less than 0  (%d)\n" */
extern const char lbl_801114DC[];           /* "AUDIO sfx volume can't be greater than %d  (%d)\n" */
extern const char lbl_80111510[];           /* "AUDIO music volume can't be less than 0  (%d)\n" */
extern const char lbl_80111540[];           /* "AUDIO music volume can't be greater than %d  (%d)\n" */
extern const char lbl_80111574[];           /* "UNABLE TO FIND SOUND: %s\n" */

/* per-track table: 12 entries, stride 20 (sAudioChanUpdate, 0x8023DD28) */
typedef struct AudTrack {
    /* 0x00 */ s32 soundId;   /* bank<<16 | sound */
    /* 0x04 */ s32 event;     /* event/tid; 0 = free slot */
    /* 0x08 */ f32 dur;       /* lifetime seconds */
    /* 0x0C */ s32 startTick;
    /* 0x10 */ s32 instId;
} AudTrack;
extern AudTrack sAudioChanUpdate[12];   /* 0x8023DD28 */

typedef struct AudioRomBankEntry {
    u8 _pad00[38];
    s16 firstSound;
    u8 _pad28[4];
} AudioRomBankEntry;

typedef struct AudioRomSoundEntry {
    u8 _pad00[20];
    f32 volume;
    u8 _pad18[4];
} AudioRomSoundEntry;

/* 32-entry kill/voice table, stride 48 (gAudioKillTbl, 0x8023D728) */
extern u8 gAudioKillTbl[32 * 48];   /* 0x8023D728 */

/* 12-slot spatial voice descriptor table, stride 32 (gAudioVoiceDesc, 0x8023D218) */
extern s32 gAudioVoiceDesc[12][8];  /* 0x8023D218 */

/* driver-state block; sub-tables above are addressed relative to it in the
 * original but carry their own labels (0x8023D200) */
extern u8 sAudioState[];            /* 0x8023D200 */

/* listener/camera transform array; listener basis at [0], position at +300 */
extern f32 gCameras[];              /* 0x8023F8D0 */

/* forward decls */
s32  AudioLoadPart(s32 bankIdx, s32 partIdx, s32 waitLevel, s32 flag);
void AudioStreamStop(void);
void AudioClearTracks(void);
s32  AudioUnloadPart(char* bankName);
s32  AudioSetMode(char* modeName);
void AudioReset(s32 force);
void AudioLoadComplete(volatile s32* slot);

/* command opcodes issued through sndRegisterList */
#define SND_LIST_VOL  0x55AB
#define SND_LIST_PAN  0x55AC
#define SND_LIST_STOP 0x55AE

/* ---------------------------------------------------------------- */
/* per-track parameter setters (sndRegisterList list ops)           */
/* ---------------------------------------------------------------- */

s32 AudioSetTrackPan(s32 handle, s32 pan)
{
    volatile u8 unused[8];
    s32 param[2];
    s32 wasBusy = sAudioQueBusy;

    (void)unused;
    if (sAudioInitFlag != 0) {
        pan = 127;
    }
    if (sAudioMute == 0) {
        if ((handle & 0x1FFF) == 0) {
            return 0;
        }
        param[0] = SND_LIST_PAN;
        param[1] = (handle << 16) | (pan & 0xFFFF);
        sAudioQueBusy = 1;
        if (sAudioSuspend == 0) {
            sndRegisterList(param, 2);
        }
        if (wasBusy == 0) {
            sAudioQueBusy = 0;
        }
    }
    return 0;
}

s32 AudioSetTrackVolMusic(s32 handle, s32 vol)
{
    s32 param[2];
    volatile u8 unused[8];
    s32 wasBusy = sAudioQueBusy;
    s32 t = (vol * lbl_80343B4C) >> 8;
    s32 v;

    (void)unused;
    if (t < 0) {
        v = 0;
    } else if (t > 255) {
        v = 255;
    } else {
        v = t;
    }
    if (sAudioMute == 0) {
        if ((handle & 0x1FFF) == 0) {
            return 0;
        }
        param[0] = SND_LIST_VOL;
        param[1] = (handle << 16) | (v & 0xFF);
        sAudioQueBusy = 1;
        if (sAudioSuspend == 0) {
            sndRegisterList(param, 2);
        }
        if (wasBusy == 0) {
            sAudioQueBusy = 0;
        }
    }
    return 0;
}

s32 AudioSetTrackVolSfx(s32 handle, s32 vol)
{
    s32 param[2];
    volatile u8 unused[8];
    s32 wasBusy = sAudioQueBusy;
    s32 t = (vol * lbl_80343B48) >> 8;
    s32 v;

    (void)unused;
    if (t < 0) {
        v = 0;
    } else if (t > 255) {
        v = 255;
    } else {
        v = t;
    }
    if (sAudioMute == 0) {
        if ((handle & 0x1FFF) == 0) {
            return 0;
        }
        param[0] = SND_LIST_VOL;
        param[1] = (handle << 16) | (v & 0xFF);
        sAudioQueBusy = 1;
        if (sAudioSuspend == 0) {
            sndRegisterList(param, 2);
        }
        if (wasBusy == 0) {
            sAudioQueBusy = 0;
        }
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/* track queries -> bitmask / handle                                */
/* ---------------------------------------------------------------- */

s32 AudioMaskByInstance(s32 instId)
{
    s32 i;
    s32 mask = 0;

    for (i = 0; i < 12; i++) {
        if (sAudioChanUpdate[i].instId == instId && sAudioChanUpdate[i].event != 0) {
            mask |= (1 << i);
        }
    }
    return mask;
}

s32 AudioMaskByEvent(s32 event)
{
    s32 i;
    s32 mask = 0;

    for (i = 0; i < 12; i++) {
        if (sAudioChanUpdate[i].event == event) {
            mask |= (1 << i);
        }
    }
    return mask;
}

s32 AudioSoundPlaying(s32 soundId)
{
    s32 i;

    for (i = 0; i < 12; i++) {
        if (sAudioChanUpdate[i].soundId == soundId) {
            /* field +8 holds the absolute expiry tick */
            if ((f32)pbLoad < sAudioChanUpdate[i].dur) {
                return 1;
            }
        }
    }
    return 0;
}

#pragma opt_common_subs off
#pragma opt_propagation off
s32 AudioSoundExists(s32 soundId)
{
    /* two-table scan, both addressed off sAudioState: the 12 x 20 channel table
     * (sAudioChanUpdate, +2856) then the 32 x 48 voice table (gAudioKillTbl,
     * +1320).  Returns the stored handle of the first live match. */
    s32 i;
    u8* state = sAudioState;

    for (i = 0; i < 12; i++) {
        u8* entry = state + i * 20;

        if (soundId == *(s32*)(entry += 2856) && *(s32*)(entry + 4) != 0) {
            u8* resultEntry = state + i * 20;
            return *(s32*)(resultEntry + 2872);
        }
    }
    for (i = 0; i < 32; i++) {
        u8* entry = state + i * 48;

        if (soundId == *(s32*)(entry += 1320) && *(s32*)(entry + 4) != 0) {
            u8* resultEntry = state + i * 48;
            return *(s32*)(resultEntry + 1340);
        }
    }
    return 0;
}
#pragma opt_propagation reset
#pragma opt_common_subs reset

s32 AudioMaskBySound(s32 soundId)
{
    s32 i;
    s32 mask = 0;

    for (i = 0; i < 12; i++) {
        if (sAudioChanUpdate[i].soundId == soundId && sAudioChanUpdate[i].event != 0) {
            mask |= (1 << i);
        }
    }
    return mask;
}

/* ---------------------------------------------------------------- */
/* track killers                                                    */
/* ---------------------------------------------------------------- */

s32 AudioKillByEvent(s32 event)
{
    s32 mask = 0;
    s32 i;
    s32* k;

    for (i = 0; i < 12; i++) {
        if (sAudioChanUpdate[i].event == event) {
            mask |= (1 << i);
        }
    }
    if (mask != 0) {
        AudioKillMask(mask);
    }
    for (i = 0; i < 32; i++) {
        if (event == *(s32*)(gAudioKillTbl + i * 48 + 4)) {
            *(s32*)(gAudioKillTbl + i * 48 + 20) = 0;
        }
    }
    return mask;
}

s32 AudioKillBySound(s32 soundId)
{
    s32 mask = 0;
    s32 i;
    s32* k;

    for (i = 0; i < 12; i++) {
        if (sAudioChanUpdate[i].soundId == soundId && sAudioChanUpdate[i].event != 0) {
            mask |= (1 << i);
        }
    }
    if (mask != 0) {
        AudioKillMask(mask);
    }
    for (i = 0; i < 32; i++) {
        if (soundId == *(s32*)(gAudioKillTbl + i * 48 + 0)
            && *(s32*)(gAudioKillTbl + i * 48 + 4) != 0) {
            *(s32*)(gAudioKillTbl + i * 48 + 20) = 0;
        }
    }
    return mask;
}

/* AudioKillByBank: unlike the by-event/by-sound killers this one does not
 * return the mask (the original discards it after the stop request). */
void AudioKillByBank(s32 bankId)
{
    s32 mask = 0;
    s32 i;

    for (i = 0; i < 12; i++) {
        if ((sAudioChanUpdate[i].soundId >> 16) == bankId) {
            mask |= (1 << i);
        }
    }
    if (mask != 0) {
        AudioKillMask(mask);
    }
    for (i = 0; i < 32; i++) {
        if ((*(s32*)(gAudioKillTbl + i * 48 + 0) >> 16) == bankId) {
            *(s32*)(gAudioKillTbl + i * 48 + 20) = 0;
        }
    }
}

/* AudioKillMask (AudioKillMask): register a stop-list for the masked tracks and
 * free their slots.  Defined in this TU but also referenced by sndfx.c. */
s32 AudioKillMask(s32 mask)
{
    s32 param[2];
    s32 wasBusy = sAudioQueBusy;
    s32 i;

    if (sAudioMute == 0) {
        if ((mask & 0x1FFF) == 0) {
            return 0;
        }
        param[0] = SND_LIST_STOP;
        param[1] = mask << 16;
        sAudioQueBusy = 1;
        if (sAudioSuspend == 0) {
            sndRegisterList(param, 2);
        }
        for (i = 0; i < 12; i++) {
            if (mask & (1 << i)) {
                sAudioChanUpdate[i].event = 0;
            }
        }
        if (wasBusy == 0) {
            sAudioQueBusy = 0;
        }
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/* 3D pan from listener-relative position (AudioAng)                */
/* ---------------------------------------------------------------- */

/* AudioAng: project a world position onto the listener's right axis and map it
 * to a pan value in [-256,255] (127 = centre).  The source is flattened onto
 * the ground plane, normalised against a 20-unit reference distance, and the
 * X/Z cross term picks the left/right sign. */
s32 AudioAng(f32* pos)
{
    f32 rel[3];
    f32 dist;
    f32 dot;
    f64 scale;
    s32 pan;

    if (sAudioInitFlag != 0 || pos == 0) {
        return 127;
    }
    rel[0] = pos[0] - gCameras[75];
    rel[1] = pos[1] - gCameras[76];
    rel[2] = pos[2] - gCameras[77];
    rel[1] = lbl_80345930;                    /* 0.0f: flatten to ground plane */
    dist = NormalVector(rel) / lbl_80345960;  /* normalise by 20.0f */
    dot = rel[0] * gCameras[1] + rel[1] * gCameras[2] + rel[2] * gCameras[3];
    scale = (1.0 >= dist) ? (f64)dist : 1.0;  /* clamp normalised distance */
    pan = (s32)(127.5 * dot * scale + 127.5);
    if (gCameras[3] * rel[0] > gCameras[1] * rel[2]) {
        pan = -pan;
    }
    if (pan < -256) {
        pan = -256;
    } else if (pan > 255) {
        pan = 255;
    }
    return pan;
}

/* ---------------------------------------------------------------- */
/* mode / ROM loading                                               */
/* ---------------------------------------------------------------- */

/* AudioSetMode: select the named audio mode from the ROM's mode table (stride
 * 9364), load its startup parts and, if any part fails, reset the driver and
 * retry once ("RESETTING AUDIO AND TRYING AGAIN").  Returns nonzero on success. */
s32 AudioSetMode(char* modeName)
{
    s32 result;
    s32 attempt;
    s32 i;

    gAudioBankTbl = 0;
    for (attempt = 0; attempt < 2; attempt++) {
        for (i = 0; i < *(s32*)(sAudioBankTable + 0); i++) {
            if (strncmp((char*)(*(u8**)(sAudioBankTable + 12) + i * 9364), modeName, 16) == 0) {
                gAudioBankTbl = (s32*)(*(u8**)(sAudioBankTable + 12) + i * 9364);
                break;
            }
        }
        if (gAudioBankTbl == 0) {
            FatalErrorf(lbl_80111304, modeName);
        }
        result = 0;
        i = 0;
        while (i < gAudioBankTbl[4]) {
            result = AudioLoadPart(i, 0, 0, 0);
            if (result != 0) {
                i++;
            }
            if (result == 0) {
                break;
            }
        }
        if (result != 0) {
            break;
        }
        ErrorPrintf(lbl_80111324);
        sndTestAcquire(0);
    }
    return result;
}

typedef union AudioWordBytes {
    u32 value;
    u8 bytes[4];
} AudioWordBytes;

typedef union AudioHalfBytes {
    u16 value;
    u8 bytes[2];
} AudioHalfBytes;

typedef union AudioRomFloatBytes {
    f32 value;
    u8 bytes[4];
} AudioRomFloatBytes;

typedef struct AudioLoadFrame {
    u8 unused[164];
    AudioRomFloatBytes float2Src;
    AudioRomFloatBytes float2Dst;
    AudioRomFloatBytes float1Src;
    AudioRomFloatBytes float1Dst;
    AudioRomFloatBytes float2Input;
    AudioRomFloatBytes float2Output;
    AudioRomFloatBytes float1Input;
    AudioRomFloatBytes float1Output;
    AudioWordBytes soundSrc;
    AudioWordBytes soundDst;
    AudioHalfBytes half42;
    AudioHalfBytes half40;
    AudioHalfBytes half38;
    AudioHalfBytes half36;
    AudioWordBytes bankSrc;
    AudioWordBytes bankDst;
    AudioWordBytes arraySrc;
    AudioWordBytes arrayDst;
    AudioWordBytes part5Src;
    AudioWordBytes part5Dst;
    AudioWordBytes part4Src;
    AudioWordBytes part4Dst;
    AudioWordBytes part3Src;
    AudioWordBytes part3Dst;
    AudioWordBytes part2Src;
    AudioWordBytes part2Dst;
    AudioWordBytes part1Src;
    AudioWordBytes part1Dst;
    AudioWordBytes modeSrc;
    AudioWordBytes modeDst;
    AudioWordBytes root6Src;
    AudioWordBytes root6Dst;
    AudioWordBytes root5Src;
    AudioWordBytes root5Dst;
    AudioWordBytes root4Src;
    AudioWordBytes root4Dst;
    AudioWordBytes root3Src;
    AudioWordBytes root3Dst;
    AudioWordBytes root2Src;
    AudioWordBytes root2Dst;
    AudioWordBytes root1Src;
    AudioWordBytes root1Dst;
} AudioLoadFrame;

#define AUDIO_SWAP_WORD_AT(src, dst, ptr) do { \
    (src).value = *(ptr); \
    *(volatile u8*)&(dst).bytes[0] = *(volatile u8*)&(src).bytes[3]; \
    *(volatile u8*)&(dst).bytes[1] = *(volatile u8*)&(src).bytes[2]; \
    *(volatile u8*)&(dst).bytes[2] = *(volatile u8*)&(src).bytes[1]; \
    *(volatile u8*)&(dst).bytes[3] = *(volatile u8*)&(src).bytes[0]; \
    *(ptr) = (dst).value; \
} while (0)

#define AUDIO_SWAP_ROOT_AT(src, dst, offset) do { \
    (src).value = *(u32*)(sAudioBankTable + (offset)); \
    *(volatile u8*)&(dst).bytes[0] = *(volatile u8*)&(src).bytes[3]; \
    *(volatile u8*)&(dst).bytes[1] = *(volatile u8*)&(src).bytes[2]; \
    *(volatile u8*)&(dst).bytes[2] = *(volatile u8*)&(src).bytes[1]; \
    *(volatile u8*)&(dst).bytes[3] = *(volatile u8*)&(src).bytes[0]; \
    *(u32*)((*(u8* volatile*)&sAudioBankTable) + (offset)) = (dst).value; \
} while (0)

#define AUDIO_SWAP_HALF_AT(src, ptr) do { \
    (src).value = *(ptr); \
    *(ptr) = (u16)(((src).bytes[0]) | ((src).bytes[1] << 8)); \
} while (0)

#define AUDIO_SWAP_FLOAT_AT(input, src, dst, output, ptr) do { \
    (input).value = *(ptr); \
    (src) = (input); \
    *(volatile u8*)&(dst).bytes[0] = *(volatile u8*)&(src).bytes[3]; \
    *(volatile u8*)&(dst).bytes[1] = *(volatile u8*)&(src).bytes[2]; \
    *(volatile u8*)&(dst).bytes[2] = *(volatile u8*)&(src).bytes[1]; \
    *(volatile u8*)&(dst).bytes[3] = *(volatile u8*)&(src).bytes[0]; \
    (output) = (dst); \
    *(ptr) = (output).value; \
} while (0)

/* AudioLoadRom: load the "audio" ROM group (via a 0xB4-byte load descriptor at
 * lbl_80111348) into hi-mem, then byte-reverse every u32 field of its little-
 * endian descriptor tree into GameCube big-endian in place.
 *
 * sequence of the 4-byte reversal idiom seen in AudioSetListenerPos. */
#ifdef __MWERKS__
#pragma opt_lifetimes off
#endif
void AudioLoadRom(void)
{
    AudioLoadFrame frame;
    s32 modeIndex;
    s32 partIndex;
    u8* mode;
    u8* part;
    s32 modeOffset;
    s32 partOffset;
    s32 wordIndex;
    s32 soundIndex;
    s32 bankIndex;
    u8* bank;
    u8* sound;
    u8* table;

    sAudioBankTable = (u8*)AllocFile((char*)&lbl_80345990, (void*)lbl_80111348);

    AUDIO_SWAP_ROOT_AT(frame.root1Src, frame.root1Dst, 0);
    AUDIO_SWAP_ROOT_AT(frame.root2Src, frame.root2Dst, 4);
    AUDIO_SWAP_ROOT_AT(frame.root3Src, frame.root3Dst, 8);
    AUDIO_SWAP_ROOT_AT(frame.root4Src, frame.root4Dst, 12);
    AUDIO_SWAP_ROOT_AT(frame.root5Src, frame.root5Dst, 16);
    AUDIO_SWAP_ROOT_AT(frame.root6Src, frame.root6Dst, 20);

    {
        u8* root;
        root = sAudioBankTable;
        *(u32*)(root + 12) = (u32)root + *(u32*)(root + 12);
        root = sAudioBankTable;
        *(u32*)(root + 16) = (u32)root + *(u32*)(root + 16);
        root = sAudioBankTable;
        *(u32*)(root + 20) = (u32)root + *(u32*)(root + 20);
    }

    modeOffset = 0;
    for (modeIndex = 0; modeIndex < *(s32*)(sAudioBankTable + 0);
         modeIndex++, modeOffset += 9364) {
        mode = *(u8**)(sAudioBankTable + 12) + modeOffset;
        AUDIO_SWAP_WORD_AT(frame.modeSrc, frame.modeDst, (u32*)(mode + 16));
        partIndex = 0;
        partOffset = 0;
        while (partIndex < *(s32*)(mode + 16)) {
            part = mode + 20 + partOffset;
            AUDIO_SWAP_WORD_AT(frame.part1Src, frame.part1Dst, (u32*)(part + 16));
            AUDIO_SWAP_WORD_AT(frame.part2Src, frame.part2Dst, (u32*)(part + 20));
            AUDIO_SWAP_WORD_AT(frame.part3Src, frame.part3Dst, (u32*)(part + 24));
            AUDIO_SWAP_WORD_AT(frame.part4Src, frame.part4Dst, (u32*)(part + 284));
            AUDIO_SWAP_WORD_AT(frame.part5Src, frame.part5Dst, (u32*)(part + 288));
            for (wordIndex = 0; wordIndex < 64; wordIndex++) {
                AUDIO_SWAP_WORD_AT(frame.arraySrc, frame.arrayDst,
                                   (u32*)(part + 28 + wordIndex * 4));
            }
            partIndex++;
            partOffset += 292;
        }
    }

    bankIndex = 0;
    modeIndex = 0;
    while ((table = sAudioBankTable, bankIndex < *(s32*)(table + 4))) {
        bank = *(u8**)(table + 16) + modeIndex;
        AUDIO_SWAP_WORD_AT(frame.bankSrc, frame.bankDst, (u32*)(bank + 32));
        AUDIO_SWAP_HALF_AT(frame.half36, (u16*)(bank + 36));
        AUDIO_SWAP_HALF_AT(frame.half38, (u16*)(bank + 38));
        AUDIO_SWAP_HALF_AT(frame.half40, (u16*)(bank + 40));
        AUDIO_SWAP_HALF_AT(frame.half42, (u16*)(bank + 42));
        bankIndex++;
        modeIndex += 44;
    }

    soundIndex = 0;
    modeIndex = 0;
    while ((table = sAudioBankTable, soundIndex < *(s32*)(table + 8))) {
        sound = *(u8**)(table + 20) + modeIndex;
        AUDIO_SWAP_WORD_AT(frame.soundSrc, frame.soundDst, (u32*)(sound + 16));
        AUDIO_SWAP_FLOAT_AT(frame.float1Input, frame.float1Src, frame.float1Dst,
                            frame.float1Output, (f32*)(sound + 20));
        AUDIO_SWAP_FLOAT_AT(frame.float2Input, frame.float2Src, frame.float2Dst,
                            frame.float2Output, (f32*)(sound + 24));
        soundIndex++;
        modeIndex += 28;
    }
}
#ifdef __MWERKS__
#pragma opt_lifetimes reset
#endif

/* ---------------------------------------------------------------- */
/* bank part registration / async load                              */
/* ---------------------------------------------------------------- */

/* AudioBankLoadName: (re)load bank "bankName"'s part "partName" at priority
 * mode.  Resolves both names to indices; if the requested part is already the
 * loaded one it returns 2, otherwise it drains any in-flight load, unloads the
 * previous occupant and kicks the async loader. */
s32 AudioBankLoadName(char* bankName, char* partName, s32 mode)
{
    s32 partIdx;
    s32 bankIdx;
    s32 i;
    s32 busy;
    u8* bankEntry;

    if (sAudioSuspend != 0) {
        return 1;
    }
    bankIdx = 0;
    i = bankIdx;
    while (bankIdx < gAudioBankTbl[4]) {
        if (strncmp((char*)((u8*)gAudioBankTbl + i + 20), bankName, 16) == 0) {
            break;
        }
        bankIdx++;
        i += 292;
    }
    if (bankIdx == gAudioBankTbl[4]) {
        sAudioSuspend = 1;
        bankIdx = -1;
    }
    bankEntry = (u8*)gAudioBankTbl + bankIdx * 292 + 20;
    for (partIdx = 0, i = 0; partIdx < *(s32*)(bankEntry + 24); partIdx++, i += 4) {
        u8* romBank = *(u8**)(sAudioBankTable + 16)
                      + *(s32*)(bankEntry + i + 28) * 44;
        if (strncmp((char*)(romBank + 16), partName, 16) == 0) {
            break;
        }
    }
    if (partIdx == *(s32*)(bankEntry + 24)) {
        sAudioSuspend = 1;
        partIdx = -1;
    }
    partName = (char*)partIdx;
    if (*(s32*)(bankEntry + 284) == partIdx) {
        return 2;
    }
    goto poll_load;
drain_load:
    lbl_803442A8 = 0;
    FreeHiMem(1);
poll_load:
    if (sAudioSuspend != 0) {
        busy = 0;
    } else {
        i = 0;
        lbl_803442A8 = 0;
        sndSysUpdate(lbl_80345950);
        if (sAudioMute != 0) {
            lbl_803442B4++;
            for (i = 10000; i != 0; i--) {
            }
        } else {
            lbl_803442B4 = 0;
        }
        busy = sAudioMute;
    }
    if (busy != 0) {
        goto drain_load;
    }
    if (*(s32*)(bankEntry + 284) == partIdx) {
        return 2;
    }
    AudioUnloadPart(bankName);
    return AudioLoadPart(bankIdx, (s32)partName, mode, 0);
}

/* AudioBankQueueName: resolve bankName -> bank index and partName -> part index
 * within the current mode's bank table, then queue the load via AudioLoadPart. */
s32 AudioBankQueueName(char* bankName, char* partName, s32 arg)
{
    s32 bankOffset;
    s32 partIdx;
    s32 bankIndex;
    s32 foundBank;
    s32 partArg;
    u8* bankEntry;

    if (sAudioSuspend != 0) {
        return 1;
    }
    bankIndex = 0;
    bankOffset = bankIndex;
    while (bankIndex < gAudioBankTbl[4]) {
        char* name = (char*)((u8*)gAudioBankTbl + bankOffset + 20);

        if (strncmp(name, bankName, 16) == 0) {
            break;
        }
        bankIndex++;
        bankOffset += 292;
    }
    if (bankIndex == gAudioBankTbl[4]) {
        sAudioSuspend = 1;
        bankIndex = -1;
    }
    bankEntry = (u8*)gAudioBankTbl + bankIndex * 292 + 20;
    foundBank = bankIndex;
    for (partIdx = 0, bankOffset = 0; partIdx < *(s32*)(bankEntry + 24);
         partIdx++, bankOffset += 4) {
        u8* romBank = *(u8**)(sAudioBankTable + 16)
                      + *(s32*)(bankEntry + bankOffset + 28) * 44;
        if (strncmp((char*)(romBank + 16), partName, 16) == 0) {
            break;
        }
    }
    if (partIdx == *(s32*)(bankEntry + 24)) {
        sAudioSuspend = 1;
        partIdx = -1;
    }
    return AudioLoadPart(foundBank, (partArg = partIdx), arg, 0);
}

/* AudioLoadPart: async DCS part loader.  Grabs a free queue slot, resolves the
 * part's ROM bank (returning 2 early if it is already resident), maps its .vbk
 * file, issues sndCmd4 with an AudioLoadComplete callback (retrying up to 10000
 * times, then FatalError), and -- for waitLevel < 2 -- drains until the load
 * finishes.  Returns 1 (or 2 already-loaded), 0 on failure. */
s32 AudioLoadPart(s32 bankIdx, s32 partIdx, s32 waitLevel, s32 flag)
{
    char name[256];
    s32 mapPtr;
    s32 mapSz1;
    s32 mapSz2;
    s32 slot;
    s32 off;
    s32 result;
    s32 savedBusy;
    s32 expected;
    s32 retry;
    s32 resp;
    u8* bankEntry;
    u8* romBank;
    u8* queueSlot;
    u16 handle;

    if (sAudioSuspend != 0) {
        return 0;
    }
    if (lbl_803442A4 == 0) {
        AudioStreamStop();
    }
    slot = 0;
    for (off = 0; off < 4 * 36; off += 36, slot++) {
        if (*(s32*)(sAudioState + off + 1176) < 0) {
            break;
        }
    }
    if (slot >= 4) {
        slot = -1;
    }
    if (slot < 0) {
        return 0;
    }
    if (partIdx < 0) {
        partIdx = 0;
    }
    result = 1;
    bankEntry = (u8*)gAudioBankTbl + bankIdx * 292 + 20;
    romBank = *(u8**)(sAudioBankTable + 16)
              + *(s32*)(bankEntry + partIdx * 4 + 28) * 44;
    handle = *(u16*)(romBank + 42);
    if (handle != 0 && handle != 0xFFFF) {
        /* already resident */
        *(s32*)(bankEntry + 284) = partIdx;
        *(s32*)(bankEntry + 288) = *(u16*)(romBank + 42);
        return 1;
    }
    *(s16*)(romBank + 40) = -1;
    *(s16*)(romBank + 42) = 0;
    savedBusy = sAudioQueBusy;
    sAudioQueBusy = 1;
    sprintf(name, lbl_80345998, (char*)romBank);   /* "%s.vbk" */
    if (FileMap((char*)&lbl_80345990, name, &mapPtr, 256, &mapSz1, &mapSz2) == 0) {
        ErrorPrintf("Audio Bank bad file: %s", name);
    } else {
        queueSlot = sAudioState + slot * 36;
        *(s32*)(queueSlot + 1176) = bankIdx;
        *(s32*)(queueSlot + 1180) = partIdx;
        *(s32*)(queueSlot + 1184) = flag;
        *(void**)(queueSlot + 1200) = (void*)AudioLoadComplete;
        *(void**)(queueSlot + 1204) = queueSlot + 1176;
        retry = 0;
        for (;;) {
            expected = sAudioMute + 1;
            sAudioMute = expected;
            resp = sndCmd4(&mapPtr, mapSz1, mapSz2, queueSlot + 1188, waitLevel);
            if (resp >= 0) {
                break;
            }
            ErrorPrintf("aud_load_bank failed: %d", resp);
            retry++;
            if (retry > 10000) {
                FatalError("aud_load_bank failed", 0x8000);
            }
        }
        if (waitLevel < 2) {
            while (expected == sAudioMute) {
                if (sAudioSuspend != 0) {
                    break;
                }
                lbl_803442A8 = 0;
                sndSysUpdate(lbl_80345950);
                if (sAudioMute != 0) {
                    s32 j;
                    lbl_803442B4++;
                    for (j = 10000; j != 0; j--) {
                    }
                } else {
                    lbl_803442B4 = 0;
                }
                if (expected != sAudioMute) {
                    break;
                }
                lbl_803442A8 = 0;
                FreeHiMem(1);
            }
            if (*(s16*)(romBank + 40) < 0) {
                result = 0;
            }
        }
    }
    sAudioQueBusy = savedBusy;
    return result;
}

/* AudioLoadComplete (AudioLoadComplete): DCS load-done / error callback stored in the
 * queue slot by AudioLoadPart.  slot+16 -> a {bankIdx, partIdx, retryCount}
 * descriptor; on success it records the returned voice handle in both the mode
 * bank table (+284/+288) and the ROM bank (+40/+42); on failure it retries up
 * to 5 times, then suspends audio. */
typedef struct AudioModeBankEntry {
    u8 _pad00[28];
    s32 partRomBank[64];
} AudioModeBankEntry;

void AudioLoadComplete(volatile s32* slot)
{
    s32* desc = *(s32**)((u8*)slot + 16);
    u8* bankEntry = (u8*)gAudioBankTbl + desc[0] * 292 + 20;
    AudioModeBankEntry* modeBank = (AudioModeBankEntry*)bankEntry;
    u8* romBank = *(u8**)(sAudioBankTable + 16)
                  + modeBank->partRomBank[desc[1]] * 44;

    if (slot[1] != 0) {
        ErrorPrintf(lbl_80111434, romBank + 16, desc[2], slot[1]);
        lbl_803442C8++;
        *(s16*)(romBank + 40) = -2;
        lbl_803442B4 = 0;
        sAudioMute--;
        if (desc[2] < 5) {
            AudioLoadPart(desc[0], desc[1], 2, desc[2] + 1);
        } else {
            sAudioSuspend = 1;
            lbl_803442A4 = 1;
            return;
        }
    } else {
        u16 h0;
        u16 h1;
        u32 unused;
        *(s32*)(bankEntry + 284) = desc[1];
        *(s16*)(romBank + 40) = (s16)slot[2];
        sndCmd7(*(s16*)(romBank + 40), &h0, &h1);
        *(u16*)(romBank + 42) = h0;
        *(s32*)(bankEntry + 288) = *(u16*)(romBank + 42);
        lbl_803442B4 = 0;
        sAudioMute--;
    }
    desc[0] = -1;
}

/* AudioDeferSlot (AudioDeferSlot): register a deferred completion slot, unless
 * audio is suspended or a stream is already active. */
void AudioDeferSlot(void* cb, s32 arg)
{
    if (sAudioSuspend != 0) {
        return;
    }
    switch (lbl_803442A4) {
    case 0:
        sndDeferSlot(cb, arg);
        break;
    }
}

/* ---------------------------------------------------------------- */
/* streaming                                                        */
/* ---------------------------------------------------------------- */

extern void AudioStreamEndCbLoop(void);
extern void AudioStreamEndCbOnce(void);

/* AudioStreamPlay: locate the stream in the "streams" file group, hand its
 * mapped block to the driver (sndCmd8), install the loop/one-shot end callback
 * and start playback.  Returns 1 on success, -1 on any failure, 0 if audio is
 * suspended or a stream is already running. */
s32 AudioStreamPlay(u16 id, s32 loopMode, s32 vol)
{
    s32 dataPtr;
    s32 sz1;
    s32 sz2;
    s32 result = -1;
    s32 resp;

    if (sAudioSuspend != 0) {
        return 0;
    }
    if (lbl_803442A4 != 0) {
        return 0;
    }
    if (FileMap((char*)lbl_803459A0, (char*)(sAudioState + 1048), &dataPtr, 256,
                &sz1, &sz2) == 0) {
        ErrorPrintf("Audio Stream bad file: %s", (char*)(sAudioState + 1048));
    } else {
        resp = sndCmd8((u16*)&dataPtr, sz1, sz2);
        if (resp == -4) {
            ErrorPrintf("Audio Stream no buffer memory: %s", (char*)(sAudioState + 1048));
        } else if (resp < 0) {
            ErrorPrintf("Audio Stream bad file: %s", (char*)(sAudioState + 1048));
        } else {
            sndCmdB();
            *(void**)(sAudioState + 12) =
                (loopMode != 0) ? (void*)AudioStreamEndCbLoop : (void*)AudioStreamEndCbOnce;
            if (loopMode >= 2) {
                loopMode = 0;
            }
            resp = sndCmdA(id, (loopMode != 0) ? 1 : 0, vol, sAudioState);
            if (resp >= -1) {
                sndSysSetBit0(1);
                result = 1;
            } else {
                ErrorPrintf("Audio Stream Err: %s", (char*)(sAudioState + 1048));
            }
        }
    }
    sndSysSetBit1(0);
    return result;
}

void AudioStreamEndCbLoop(void)
{
    sndSysSetBit0(0);
    sndSysSetBit1(1);
}

void AudioStreamEndCbOnce(void)
{
    sMusicField2F4++;
    sndSysSetBit0(0);
    sndSysSetBit1(0);
}

/* AudioStreamStop: stop the current stream, waiting out the driver with a 900ms
 * timeout guard. */
void AudioStreamStop(void)
{
    s32 resp;
    s32 start = pbLoad;

    if (sAudioSuspend != 0) {
        return;
    }
    if (lbl_803442A4 != 0) {
        sndSysSetBit0(0);
        return;
    }
    resp = sndCmdC();
    if (resp == -2) {
        return;
    }
    if (resp < 0) {
        sndSysSetBit0(0);
        if (resp != -2) {
            ErrorPrintf(lbl_801113FC, resp);
        }
    }
    while (sndSysFrameCallback() != 0) {
        s32 j;
        serve_busy(-1);
        if ((u32)((pbLoad - start) << 1) > 900) {
            ErrorPrintf(lbl_80111418);
            sndSysSetBit0(0);
        } else {
            for (j = 10000; j != 0; j--) {
                /* let the driver drain a little before re-polling */
            }
        }
        lbl_803442A8 = 0;
    }
}

/* ---------------------------------------------------------------- */
/* track table reset + part unload                                  */
/* ---------------------------------------------------------------- */

/* AudioClearTracks: mark every mode bank's part slots not-loaded (+304/+308 = -1)
 * and every ROM bank's active-sound counters cleared (+40/+42 = 0), then flush
 * the driver (sndCmd6). */
void AudioClearTracks(void)
{
    s32 i;

    if (sAudioSuspend != 0) {
        return;
    }
    for (i = 0; i < gAudioBankTbl[4]; i++) {
        *(s32*)((u8*)gAudioBankTbl + i * 292 + 304) = -1;
        *(s32*)((u8*)gAudioBankTbl + i * 292 + 308) = -1;
    }
    for (i = 0; i < *(s32*)(sAudioBankTable + 4); i++) {
        *(u16*)(*(u8**)(sAudioBankTable + 16) + i * 44 + 40) = 0;
        *(u16*)(*(u8**)(sAudioBankTable + 16) + i * 44 + 42) = 0;
    }
    sndCmd6();
}

/* AudioUnloadPart: free the ROM bank currently held by the named mode bank's
 * loaded part -- but only if no other loaded mode bank still points at the same
 * ROM bank (otherwise it is left in place and a "still in use" note is logged).
 * Either way the mode bank's part slot (+284/+288) is reset to -1. */
s32 AudioUnloadPart(char* bankName)
{
    s32 i;
    s32 j;
    s32 partId;
    s32 romBankId;
    u8* bankEntry;
    u8* romBank;
    u16 handle;

    if (sAudioSuspend != 0) {
        return 0;
    }
    for (i = 0; i < gAudioBankTbl[4]; i++) {
        if (strncmp((char*)((u8*)gAudioBankTbl + i * 292 + 20), bankName, 16) == 0) {
            break;
        }
    }
    if (i == gAudioBankTbl[4]) {
        sAudioSuspend = 1;
        i = -1;
    }
    bankEntry = (u8*)gAudioBankTbl + i * 292 + 20;
    partId = *(s32*)(bankEntry + 284);
    if (partId < 0) {
        return 0;
    }
    romBankId = *(s32*)(bankEntry + partId * 4 + 28);
    romBank = *(u8**)(sAudioBankTable + 16) + romBankId * 44;
    handle = *(u16*)(romBank + 42);
    if (handle != 0 && handle != 0xFFFF) {
        for (j = gAudioBankTbl[4] - 1; j >= 0; j--) {
            if (j != i) {
                u8* other = (u8*)gAudioBankTbl + j * 292 + 20;
                if (romBankId == *(s32*)(other + *(s32*)(other + 284) * 4 + 28)) {
                    bulletproof_printf(lbl_8011145C);
                    break;
                }
            }
        }
        if (j < 0) {
            AudioKillByBank(romBankId);
            sndCmd18(*(s16*)(romBank + 40));
            *(s16*)(romBank + 40) = 0;
            *(s16*)(romBank + 42) = 0;
        }
    }
    *(s32*)(bankEntry + 284) = -1;
    *(s32*)(bankEntry + 288) = -1;
    return 0;
}

/* ---------------------------------------------------------------- */
/* driver pump / sync                                               */
/* ---------------------------------------------------------------- */

/* AudioSysUpdate: advance the sound system by one step and return the pending
 * command count (used as a "still busy?" poll). */
s32 AudioSysUpdate(s32 dt)
{
    s32 j;

    if (sAudioSuspend != 0) {
        return 0;
    }
    lbl_803442A8 = 0;
    sndSysUpdate((f32)dt);
    if (sAudioMute != 0) {
        lbl_803442B4 += dt;
        for (j = 10000; j != 0; j--) {
            /* short busy-wait while a command is still pending */
        }
    } else {
        lbl_803442B4 = 0;
    }
    return sAudioMute;
}

/* AudioReset: recover the driver after an error/timeout.  Clears the error
 * accumulators, re-acquires the driver and reloads the current mode's parts;
 * if the driver stays busy past the spin budget it logs "Audio Reset Error". */
void AudioReset(s32 force)
{
    s32 i = 0;
    s32 j;

    if (sAudioSuspend != 0) {
        return;
    }
    if (force == 0 && sAudioErrFlags == 0 && lbl_803442CC == 0 && lbl_803442C8 == 0) {
        return;
    }
    if (force == 0) {
        lbl_803442C4++;
    }
    sAudioErrFlags = 0;
    lbl_803442CC = 0;
    sAudioTimeoutAcc = 0;
    lbl_803442C8 = 0;
    sAudioTimeoutErrs = 0;
    sAudioQueBusy = 1;
    sndCmd1();
    AudioClearTracks();

    for (;;) {
        if (sndSysUpdate(lbl_80345930) == 1) {
            break;
        }
        i++;
        if ((f64)i > 400000000.0) {
            lbl_803442CC++;
            ErrorPrintf(lbl_80111488);
            return;
        }
        for (j = 10000; j != 0; j--) {
            /* spin while the driver drains */
        }
    }

    sAudioMute = 0;
    sndFxInitVoices();
    sAudioQueCount[0] = 0;
    sAudioQueBusy = 1;
    sAudioQueCount[1] = 0;
    sAudioQueFade[0] = lbl_80345930;
    sAudioQueFade[1] = lbl_80345930;
    lbl_803442EC = 0;
    lbl_803442D4 = 0;
    sAudioQueBusy = 0;
    sndSysSetBit0(0);
    sndSysSetBit1(0);
    *(s32*)(sAudioState + 4) = 0;
    lbl_803442F0 = -1;
    for (i = 0; i < gAudioBankTbl[4]; i++) {
        s32 part = *(s32*)((u8*)gAudioBankTbl + i * 292 + 304);
        if (AudioLoadPart(i, part, 0, 0) == 0) {
            lbl_803442C8++;
        }
    }
}

/* AudioSysSync: drive one update and latch a -2 ("busy") response. */
void AudioSysSync(s32 dt)
{
    if (sAudioSuspend != 0) {
        return;
    }
    sAudioReady = sndSysUpdate((f32)dt);
    if (sAudioReady == -2) {
        sAudioErrFlags |= 1;
        FatalErrorf(lbl_8011149C);
    }
}

/* ---------------------------------------------------------------- */
/* init / enable                                                    */
/* ---------------------------------------------------------------- */

/* audio_init: full driver bring-up.  Acquires the driver, resets track state,
 * selects the boot mode and enables SFX. */
void audio_init(void)
{
    s32 wasBusy;
    s32 enable;

    sAudioQueBusy = 1;
    if ((gControllerButtons & 0x20) != 0) {
        sAudioSuspend = 1;
        lbl_803442A4 = 1;
        lbl_803449A8 = 1;
    }
    if (sAudioSuspend == 0) {
        sndTestAcquire(0);
    }
    enable = 0;
    sAudioMute = enable;
    sndFxInitVoices();
    wasBusy = sAudioQueBusy;
    sAudioQueBusy = 1;
    sAudioQueCount[0] = enable;
    sAudioQueCount[1] = enable;
    sAudioQueFade[0] = lbl_80345930;
    sAudioQueFade[1] = lbl_80345930;
    lbl_803442EC = enable;
    if (wasBusy == 0) {
        sAudioQueBusy = enable;
    }
    AudioSetMode((char*)&lbl_803459B0);
    sAudioErrFlags = enable;
    lbl_803442C8 = enable;
    lbl_803442CC = enable;
    sAudioReady = enable;
    lbl_803442D4 = enable;
    if (sAudioSuspend == 0) {
        sndSysSetBit0(0);
        sndSysSetBit1(0);
        *(s32*)(sAudioState + 4) = enable;
        enable = (sAudioInitFlag != 0) ? enable : 1;
        sndCmd3(enable);
    }
    sAudioQueBusy = 0;
}

/* AudioSetEnabled: flip the master enable and tell the driver. */
s32 AudioSetEnabled(s32 enable)
{
    if (sAudioSuspend != 0) {
        return 1;
    }
    sAudioInitFlag = !enable;
    sndCmd3(enable);
    return 1;
}

/* ---------------------------------------------------------------- */
/* volume clamps                                                    */
/* ---------------------------------------------------------------- */

s32 AudioSetVolSfx(s32 vol)
{
    lbl_80343B48 = vol;
    if (vol < 0) {
        ErrorPrintf(lbl_801114AC, vol);
        lbl_80343B48 = 0;
    } else if (vol > 255) {
        ErrorPrintf(lbl_801114DC, 255, vol);
        lbl_80343B48 = 255;
    }
    return 1;
}

s32 AudioSetVolMusic(s32 vol)
{
    lbl_80343B4C = vol;
    if (vol < 0) {
        ErrorPrintf(lbl_80111510, vol);
        lbl_80343B4C = 0;
    } else if (vol > 255) {
        ErrorPrintf(lbl_80111540, 255, vol);
        lbl_80343B4C = 255;
    }
    return 1;
}

/* ---------------------------------------------------------------- */
/* misc getters / stubs                                             */
/* ---------------------------------------------------------------- */

void AudioEmptyCb1(void) {}
void AudioEmptyCb2(void) {}

/* AudioGetSoundVol: fetch a sound instance's stored volume by packed id. */
f32 AudioGetSoundVol(s32 packedId)
{
    AudioRomBankEntry* banks;
    u8* root;
    AudioRomSoundEntry* sounds;
    s32 firstSound;

    root = sAudioBankTable;
    banks = *(AudioRomBankEntry**)(root + 16);
    sounds = *(AudioRomSoundEntry**)(root + 20);
    firstSound = banks[packedId >> 16].firstSound;
    packedId &= 0xFFF;
    firstSound = packedId + firstSound;

    return sounds[firstSound].volume;
}

/* AudioFindSound: resolve a sound handle by name across all loaded banks. */
s32 AudioFindSound(char* name, s32 maxLen, s32 warn)
{
    s32 b, s;

    if (*name == 0) {
        return -1;
    }
    if (maxLen <= 0) {
        maxLen = 15;
    }
    for (b = 0; b < *(s32*)(sAudioBankTable + 4); b++) {
        u8* bank = *(u8**)(sAudioBankTable + 16) + b * 44;
        for (s = 0; s < *(s16*)(bank + 36); s++) {
            u8* snd = *(u8**)(sAudioBankTable + 20)
                      + (*(s16*)(bank + 38) + s) * 28;
            if (strncmp((char*)snd, name, maxLen) == 0) {
                return *(s32*)(snd + 16);
            }
        }
    }
    if (warn != 0 && (gControllerButtons & 0x10) == 0) {
        ErrorPrintf(lbl_80111574, name);
    }
    return -1;
}

/* AudioTrackRegister: fill (or find-and-fill) a spatial voice descriptor slot
 * with the sound's resolved index, listener params and an expiry tick computed
 * from the sound's duration in seconds (dur * 60 frames + now). */
typedef struct AudioTrackBank {
    u8 pad[38];
    s16 firstSound;
    u8 tail[4];
} AudioTrackBank;

typedef struct AudioTrackSound {
    u8 pad[20];
    f32 duration;
    u8 tail[4];
} AudioTrackSound;

#pragma opt_propagation off
void AudioTrackRegister(s32 slot, s32 packedId, s32 a, s32 b, s32 c)
{
    s32* d;
    f32 dur;
    AudioTrackBank* bank;
    AudioTrackSound* sound;
    u8 unused[32];

    if (slot >= 0) {
        d = gAudioVoiceDesc[slot];
        bank = (AudioTrackBank*)*(u8**)(sAudioBankTable + 16) +
               (packedId >> 16);
        d[0] = (packedId & 0xFFF) + bank->firstSound;
        d[1] = d[0];
        d[2] = packedId;
        d[3] = pbLoad;
        d[4] = a;
        d[5] = b;
        d[6] = c;
        sound = (AudioTrackSound*)*(u8**)(sAudioBankTable + 20) + d[0];
        dur = sound->duration;
        if (dur > lbl_80345930) {
            d[7] = (s32)(lbl_80345940 * dur + (f32)d[3]);
        } else {
            d[7] = -1;
        }
    } else {
        s32 i;
        s32* entry;
        /* free-slot rescan (the shipped table start index leaves this path
         * effectively inert, but the body is preserved verbatim) */
        for (i = 12; i < 12; i++) {
            entry = gAudioVoiceDesc[i];
            if (entry[0] < 0) {
                bank = (AudioTrackBank*)*(u8**)(sAudioBankTable + 16) +
                       (packedId >> 16);
                entry[0] = (packedId & 0xFFF) + bank->firstSound;
                entry[1] = entry[0];
                entry[2] = packedId;
                entry[3] = pbLoad;
                entry[4] = a;
                entry[5] = b;
                entry[6] = c;
                sound = (AudioTrackSound*)*(u8**)(sAudioBankTable + 20) + entry[0];
                dur = sound->duration;
                if (dur > lbl_80345930) {
                    entry[7] = (s32)(lbl_80345940 * dur + (f32)entry[3]);
                } else {
                    entry[7] = -1;
                }
                break;
            }
        }
    }
}
#pragma opt_propagation reset

/* AudioIsActive: true when audio is not muted. */
s32 AudioIsActive(void)
{
    return (sAudioSuspend != 0) ? 0 : 1;
}

/* AudioSetListenerPos: byte-reverse each float of the incoming vec3 (PS2
 * little-endian -> GameCube big-endian) and hand it to the 3D transform
 * builder, then flag the listener slot dirty (+96 |= 1). */
#ifdef __MWERKS__
#pragma dont_inline off
#pragma opt_common_subs off
#pragma opt_propagation off
#endif
typedef union AudioFloatBytes {
    f32 f;
    u8 b[4];
} AudioFloatBytes;

static inline AudioFloatBytes AudioSwapFloat(const f32* values, s32 index)
{
    AudioFloatBytes pad;
    AudioFloatBytes src;
    AudioFloatBytes dst;
    AudioFloatBytes value;

#ifdef __MWERKS__
    pad = pad;
#endif
    value.f = values[index];
    src = value;
    *(volatile u8*)&dst.b[0] = *(volatile u8*)&src.b[3];
    *(volatile u8*)&dst.b[1] = *(volatile u8*)&src.b[2];
    *(volatile u8*)&dst.b[2] = *(volatile u8*)&src.b[1];
    *(volatile u8*)&dst.b[3] = *(volatile u8*)&src.b[0];
    return dst;
}
#ifdef __MWERKS__
#pragma opt_propagation reset
#endif

void AudioSetListenerPos(s32* out, s32 arg, f32* pos)
{
    f32 v[3];
    s32 i;

    for (i = 0; i < 3; i++) {
        v[i] = AudioSwapFloat(pos, i).f;
    }
    MBNewWorldPsys(0, out, arg, 1, 0, v);
    out[24] |= 1;
}
#ifdef __MWERKS__
#pragma opt_common_subs reset
#pragma dont_inline on
#endif
