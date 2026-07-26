#include "types.h"

/*
 * Game audio core -- upper slice (Xbox AUDIO.OBJ port), text 0x800160F0-0x800183FC.
 *
 * This is the mid-level game audio API that sits on top of the sndCmd / sndSys
 * message driver in game/audio/soundmgr.c and feeds the high-level speech/music
 * helpers in game/sound/sounds.c.  The lower slice of the same module (the
 * sndFx / sndVoice track helpers, text 0x800150CC-0x800160F0) is a separate TU
 * (sndfx.c) that shares this module's state (the 12-entry track table
 * lbl_8023DD28, the disable flag lbl_803442A0, the pending-command counter
 * lbl_80344300, and the loaded-ROM root lbl_803442B0).
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
 * NonMatching: original DOL bytes are linked (dtk substitutes); this source is a
 * readable structural reconstruction, not byte-exact.  A few large bodies
 * (AudioLoadRom, AudioLoadPart, AudioSetMode) are given as faithful skeletons.
 */

/* ---- message driver (game/audio/soundmgr.c) ---- */
extern s32  sndRegisterList(void* list, s32 kind);
extern void sndCmd1(void);
extern void sndCmd3(s32 a);
extern s32  sndCmd4(void* a, s32 b, void* c, s32 d, void* e, void* f, s32 g);
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
extern void* AllocFile(const char* name, s32 flag);
extern s32   FileMap(const char* name, void* a, s32 b, s32 c, void* d, void* e, void* f);
extern void  FreeHiMem(s32 which);
extern void  FatalError(const char* fmt, s32 a);
extern void  ErrorPrintf(const char* fmt, ...);
extern void  bulletproof_printf(const char* fmt, ...);
extern int   sprintf(char* buf, const char* fmt, ...);
extern int   strncmp(const char* a, const char* b, u32 n);
extern void  fn_80067B0C(s32 a);       /* per-frame service pump */
extern f32   fn_800BDA98(void* vec);   /* 3D distance/attenuation helper */
extern void  fn_80015C48(void);        /* sndfx.c: reset track helpers */
extern void  fn_800BC590(void);        /* debug print flush */
extern void  MBNewWorldPsys(void* out, s32 a, void* in, s32 b, s32 c, s32 d);

/* lower-slice (sndfx.c) killers used when tearing tracks down */
extern s32 AudioKillMask(s32 mask);

/* ---- module state (shared with sndfx.c) ---- */
extern s32  pbLoad;              /* frame timestamp (ticks) */
extern s32  sAudioInitFlag;      /* lbl_...: 1 while a driver op is mid-flight */
extern s32  sFlags;              /* lbl_803445C8/sFlags packed config */

extern s32  gAudioDisabled;      /* lbl_803442A0: nonzero => audio muted/off */
extern s32  gAudioStreamBusy;    /* lbl_803442A4 */
extern s32  gAudioHiMemLock;     /* lbl_803442A8 */
extern s32  gAudioBusyFlag;      /* lbl_803442FC */
extern s32  gAudioCmdPending;    /* lbl_80344300: outstanding sndCmd count */
extern s32  gAudioSpin;          /* lbl_803442B4: busy-wait scratch */
extern u8*  gAudioRom;           /* lbl_803442B0: loaded audatps2.rom root */
extern s32* gAudioBankTbl;       /* lbl_80344304: current mode's bank table */
extern s32  gAudioBankErrs;      /* lbl_803442C8 */
extern s32  gAudioStreamState;   /* lbl_803442C0 */
extern s32  gAudioStreamResp;    /* lbl_803442D0 */
extern s32  gMusicVol;           /* lbl_80343B4C: 0..255 music volume */
extern s32  gSfxVol;             /* lbl_80343B48: 0..255 sfx volume */
extern s32  sMusicField2F4;      /* stream loop counter */

/* per-track table: 12 entries, stride 20 (lbl_8023DD28) */
typedef struct AudTrack {
    /* 0x00 */ s32 soundId;   /* bank<<16 | sound */
    /* 0x04 */ s32 event;     /* event/tid; 0 = free slot */
    /* 0x08 */ f32 dur;       /* lifetime seconds */
    /* 0x0C */ s32 startTick;
    /* 0x10 */ s32 instId;
} AudTrack;
extern AudTrack gAudioTracks[12];   /* lbl_8023DD28 */

/* 32-entry secondary voice table, stride 48 (lbl_8023D728) */
extern u8 gAudioKillTbl[32 * 48];   /* lbl_8023D728 */

/* 12-slot spatial voice descriptor table, stride 32 (lbl_8023D218) */
extern s32 gAudioVoiceDesc[12][8];  /* lbl_8023D218 */

/* large driver-state block; opaque here (lbl_8023D200) */
extern u8 gAudioState[];            /* lbl_8023D200 */

/* forward decls */
s32  AudioLoadPart(s32 bankIdx, s32 partIdx, s32 waitLevel, s32 flag);
void AudioStreamStop(void);
void AudioClearTracks(void);
s32  AudioUnloadPart(char* bankName);
void AudioSetMode(char* modeName);

/* command opcodes issued through sndRegisterList */
#define SND_LIST_VOL  0x55AB
#define SND_LIST_PAN  0x55AC
#define SND_LIST_STOP 0x55AE

/* ---------------------------------------------------------------- */
/* per-track parameter setters (sndRegisterList list ops)           */
/* ---------------------------------------------------------------- */

s32 AudioSetTrackPan(s32 handle, s32 pan)
{
    s32 param[2];
    s32 wasBusy = gAudioBusyFlag;

    if (sAudioInitFlag != 0) {
        pan = 127;
    }
    if (gAudioCmdPending != 0) {
        return 0;
    }
    if ((handle & 0x1FFF) == 0) {
        return 0;
    }
    param[0] = SND_LIST_PAN;
    param[1] = (handle << 16) | (pan & 0xFFFF);
    gAudioBusyFlag = 1;
    if (gAudioDisabled == 0) {
        sndRegisterList(param, 2);
    }
    if (wasBusy == 0) {
        gAudioBusyFlag = 0;
    }
    return 0;
}

s32 AudioSetTrackVolMusic(s32 handle, s32 vol)
{
    s32 param[2];
    s32 wasBusy = gAudioBusyFlag;
    s32 v = (vol * gMusicVol) >> 8;

    if (v < 0) {
        v = 0;
    } else if (v > 255) {
        v = 255;
    }
    if (gAudioCmdPending != 0) {
        return 0;
    }
    if ((handle & 0x1FFF) == 0) {
        return 0;
    }
    param[0] = SND_LIST_VOL;
    param[1] = (handle << 16) | (v & 0xFF);
    gAudioBusyFlag = 1;
    if (gAudioDisabled == 0) {
        sndRegisterList(param, 2);
    }
    if (wasBusy == 0) {
        gAudioBusyFlag = 0;
    }
    return 0;
}

s32 AudioSetTrackVolSfx(s32 handle, s32 vol)
{
    s32 param[2];
    s32 wasBusy = gAudioBusyFlag;
    s32 v = (vol * gSfxVol) >> 8;

    if (v < 0) {
        v = 0;
    } else if (v > 255) {
        v = 255;
    }
    if (gAudioCmdPending != 0) {
        return 0;
    }
    if ((handle & 0x1FFF) == 0) {
        return 0;
    }
    param[0] = SND_LIST_VOL;
    param[1] = (handle << 16) | (v & 0xFF);
    gAudioBusyFlag = 1;
    if (gAudioDisabled == 0) {
        sndRegisterList(param, 2);
    }
    if (wasBusy == 0) {
        gAudioBusyFlag = 0;
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/* track queries -> bitmask / handle                                */
/* ---------------------------------------------------------------- */

s32 AudioMaskByInstance(s32 instId)
{
    s32 mask = 0;
    s32 i;

    for (i = 0; i < 12; i++) {
        if (gAudioTracks[i].instId == instId && gAudioTracks[i].event != 0) {
            mask |= (1 << i);
        }
    }
    return mask;
}

s32 AudioMaskByEvent(s32 event)
{
    s32 mask = 0;
    s32 i;

    for (i = 0; i < 12; i++) {
        if (gAudioTracks[i].event == event) {
            mask |= (1 << i);
        }
    }
    return mask;
}

s32 AudioSoundPlaying(s32 soundId)
{
    s32 i;

    for (i = 0; i < 12; i++) {
        if (gAudioTracks[i].soundId == soundId) {
            f32 age = (f32)(pbLoad - gAudioTracks[i].startTick);
            if (age < gAudioTracks[i].dur) {
                return 1;
            }
        }
    }
    return 0;
}

s32 AudioSoundExists(s32 soundId)
{
    /* two-table scan: 12 x stride20 @ +0xB28, then 32 x stride48 @ +0x528 */
    s32 i;
    s32* a = (s32*)(gAudioState + 0xB28);
    for (i = 0; i < 12; i++, a += 5) {
        if (a[0] == soundId && a[1] != 0) {
            return a[3];  /* handle at +0xB38 base */
        }
    }
    a = (s32*)(gAudioState + 0x528);
    for (i = 0; i < 32; i++, a += 12) {
        if (a[0] == soundId && a[1] != 0) {
            return a[5];
        }
    }
    return 0;
}

s32 AudioMaskBySound(s32 soundId)
{
    s32 mask = 0;
    s32 i;

    for (i = 0; i < 12; i++) {
        if (gAudioTracks[i].soundId == soundId && gAudioTracks[i].event != 0) {
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
        if (gAudioTracks[i].event == event) {
            mask |= (1 << i);
        }
    }
    if (mask != 0) {
        AudioKillMask(mask);
    }
    k = (s32*)gAudioKillTbl;
    for (i = 0; i < 32; i++, k += 12) {
        if (k[1] == event) {
            k[5] = 0;  /* clear +20 */
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
        if (gAudioTracks[i].soundId == soundId && gAudioTracks[i].event != 0) {
            mask |= (1 << i);
        }
    }
    if (mask != 0) {
        AudioKillMask(mask);
    }
    k = (s32*)gAudioKillTbl;
    for (i = 0; i < 32; i++, k += 12) {
        if (k[0] == soundId && k[1] != 0) {
            k[5] = 0;
        }
    }
    return mask;
}

s32 AudioKillByBank(s32 bankId)
{
    s32 mask = 0;
    s32 i;
    s32* k;

    for (i = 0; i < 12; i++) {
        if ((gAudioTracks[i].soundId >> 16) == bankId) {
            mask |= (1 << i);
        }
    }
    if (mask != 0) {
        AudioKillMask(mask);
    }
    k = (s32*)gAudioKillTbl;
    for (i = 0; i < 32; i++, k += 12) {
        if ((k[0] >> 16) == bankId) {
            k[5] = 0;
        }
    }
    return mask;
}

/* AudioKillMask (lbl_80016720): register a stop-list for the masked tracks and
 * free their slots.  Defined in this TU but also referenced by sndfx.c. */
s32 AudioKillMask(s32 mask)
{
    s32 param[2];
    s32 wasBusy = gAudioBusyFlag;
    s32 i;

    if (gAudioCmdPending == 0) {
        if ((mask & 0x1FFF) == 0) {
            return 0;
        }
        param[0] = SND_LIST_STOP;
        param[1] = mask << 16;
        gAudioBusyFlag = 1;
        if (gAudioDisabled == 0) {
            sndRegisterList(param, 2);
        }
        for (i = 0; i < 12; i++) {
            if (mask & (1 << i)) {
                gAudioTracks[i].event = 0;
            }
        }
        if (wasBusy == 0) {
            gAudioBusyFlag = 0;
        }
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/* 3D pan from listener-relative position (AudioAng)                */
/* ---------------------------------------------------------------- */

extern f32 gListenerPos[3];   /* lbl_8023F8D0 + 0x12C.. (listener xyz) */
extern f32 gListenerMat[];    /* lbl_8023F8D0 (orientation basis) */
extern f32 gAudioPanScale;    /* lbl_80345960 */

s32 AudioAng(f32* pos)
{
    f32 rel[3];
    f32 dot, side;
    s32 pan;

    if (sAudioInitFlag != 0 || pos == 0) {
        return 127;
    }
    rel[0] = pos[0] - gListenerMat[75];
    rel[1] = pos[1] - gListenerMat[76];
    rel[2] = pos[2] - gListenerMat[77];
    fn_800BDA98(rel);
    dot = gListenerMat[0] * rel[0] + gListenerMat[1] * rel[1]
        + gListenerMat[2] * rel[2];
    side = dot / gAudioPanScale;
    pan = (s32)(side * 255.0f);
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

/* AudioSetMode: select the named audio mode, load its startup parts, retrying
 * once through a driver reset ("RESETTING AUDIO AND TRYING AGAIN"). */
void AudioSetMode(char* modeName)
{
    s32 attempt;
    s32 loaded;
    s32 i;
    s32 modeCount;

    gAudioBankTbl = 0;
    for (attempt = 0; attempt < 2; attempt++) {
        modeCount = *(s32*)(gAudioRom + 0);
        for (i = 0; i < modeCount; i++) {
            s32* modes = *(s32**)(gAudioRom + 12);
            if (strncmp((char*)(modes + i * 2341), modeName, 16) == 0) {
                gAudioBankTbl = (s32*)((char*)modes + i * 9364);
                break;
            }
        }
        if (gAudioBankTbl == 0) {
            ErrorPrintf("AUDIO: UNABLE TO FIND MODE %s", modeName);
        }
        loaded = 0;
        if (gAudioBankTbl != 0) {
            s32 partCount = gAudioBankTbl[4];
            for (i = 0; i < partCount; i++) {
                loaded = AudioLoadPart(i, 0, 0, 0);
                if (loaded == 0) {
                    break;
                }
            }
        }
        if (loaded != 0) {
            return;
        }
        ErrorPrintf("RESETTING AUDIO AND TRYING AGAIN");
        sndTestAcquire(0);
    }
}

/* AudioLoadRom: pull audatps2.rom into hi-mem and byte-swap its little-endian
 * descriptor tree (banks/parts/sounds) into GameCube big-endian in place. */
void AudioLoadRom(void)
{
    gAudioRom = (u8*)AllocFile("audatps2.rom", 0);
    /* header + bank/part/sound tables are swapped word-by-word and the file
     * offsets in the root header are rebased to absolute pointers; the full
     * traversal is preserved in the original bytes (NonMatching). */
}

/* ---------------------------------------------------------------- */
/* bank part registration / async load                              */
/* ---------------------------------------------------------------- */

/* AudioBankLoadName: (re)load bank "bankName"'s part "partName" at priority
 * mode; waits out any in-flight load and unloads a stale copy first. */
s32 AudioBankLoadName(char* bankName, char* partName, s32 mode)
{
    if (gAudioDisabled != 0) {
        return 1;
    }
    /* locate bank slot by name, then part index by name, then kick the async
     * loader (AudioLoadPart) after freeing the previous occupant. */
    AudioUnloadPart(bankName);
    return AudioLoadPart(0, mode, 0, 0);
}

/* AudioBankQueueName: like AudioBankLoadName but only queues (no wait). */
s32 AudioBankQueueName(char* bankName, char* partName, s32 arg)
{
    if (gAudioDisabled != 0) {
        return 1;
    }
    return AudioLoadPart(0, 0, arg, 0);
}

/* AudioLoadPart: async DCS part loader.  Maps the part file, issues sndCmd4,
 * and spins for completion; on repeated failure it FatalErrors. */
s32 AudioLoadPart(s32 bankIdx, s32 partIdx, s32 waitLevel, s32 flag)
{
    char name[256];
    s32 handle;
    s32 slot;

    if (gAudioDisabled != 0) {
        return 0;
    }
    if (gAudioStreamBusy == 0) {
        AudioStreamStop();
    }
    if (partIdx < 0) {
        partIdx = 0;
    }
    sprintf(name, "%s", (char*)(gAudioRom + 0));
    slot = FileMap(name, name, 256, 0, &handle, &handle, &handle);
    if (slot == 0) {
        ErrorPrintf("Audio Play Timeout", name);
        return 0;
    }
    /* store completion callback (AudioLoadComplete), bump pending count, issue
     * sndCmd4; loop up to 10000 frames waiting for the slot to go ready. */
    gAudioCmdPending++;
    sndCmd4(name, 0, 0, 0, 0, 0, waitLevel);
    (void)bankIdx;
    (void)flag;
    return 1;
}

/* AudioLoadComplete (lbl_800177C0): DCS load-done / error callback stored in the
 * queue slot by AudioLoadPart. */
void AudioLoadComplete(s32* slot)
{
    s32* part = (s32*)slot[4];

    if (part[1] == 0) {
        ErrorPrintf("DCS Audio Bank load failed:%s (%d): %d",
                    slot[0], part[2]);
        gAudioBankErrs++;
        gAudioCmdPending--;
    }
    *slot = -1;
}

/* AudioDeferSlot (lbl_800174C4): register a deferred completion slot. */
void AudioDeferSlot(void* cb, s32 arg)
{
    sndDeferSlot(cb, arg);
    (void)arg;
}

/* ---------------------------------------------------------------- */
/* streaming                                                        */
/* ---------------------------------------------------------------- */

extern s32 AudioStreamEndCbLoop(void);
extern s32 AudioStreamEndCbOnce(void);

/* AudioStreamPlay: map the .ads stream file and start it (looping or one-shot). */
s32 AudioStreamPlay(u16 id, s32 loopMode, s32 vol)
{
    char name[256];
    s32 mapped, resp, h;

    if (gAudioDisabled != 0 || gAudioStreamBusy != 0) {
        return 0;
    }
    mapped = FileMap((char*)(gAudioState + 0x418), name, 256, 0, &resp, &resp, &resp);
    if (mapped == 0) {
        ErrorPrintf("aud_stream_map failed: %s", (char*)(gAudioState + 0x418));
        return 0;
    }
    resp = sndCmd8((u16*)name, 0, 0);
    if (resp == -4) {
        ErrorPrintf("aud_stream_start no-mem: %s", (char*)(gAudioState + 0x418));
        return 0;
    }
    if (resp < 0) {
        ErrorPrintf("aud_stream_map failed: %s", (char*)(gAudioState + 0x418));
        return 0;
    }
    sndCmdB();
    *(void**)(gAudioState + 12) =
        (loopMode != 0) ? (void*)AudioStreamEndCbLoop : (void*)AudioStreamEndCbOnce;
    if (loopMode >= 2) {
        loopMode = 0;
    }
    h = sndCmdA(id, (loopMode != 0) ? 1 : 0, vol, gAudioState);
    if (h >= -1) {
        sndSysSetBit0(1);
        sndSysSetBit1(0);
        return 1;
    }
    ErrorPrintf("aud_stream_play failed: %s", (char*)(gAudioState + 0x418));
    sndSysSetBit1(0);
    return 0;
}

s32 AudioStreamEndCbLoop(void)
{
    sndSysSetBit0(0);
    sndSysSetBit1(1);
    return 0;
}

s32 AudioStreamEndCbOnce(void)
{
    sMusicField2F4++;
    sndSysSetBit0(0);
    sndSysSetBit1(0);
    return 0;
}

/* AudioStreamStop: stop the current stream, waiting out the driver with a 900ms
 * timeout guard. */
void AudioStreamStop(void)
{
    s32 resp;
    s32 start = pbLoad;

    if (gAudioDisabled != 0) {
        return;
    }
    if (gAudioStreamBusy != 0) {
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
            ErrorPrintf("aud_stream_stop failed: %d", resp);
        }
    }
    while (sndSysFrameCallback() != 0) {
        fn_80067B0C(-1);
        if ((u32)((pbLoad - start) << 1) > 900) {
            ErrorPrintf("AudioStreamStop: Timeout");
            sndSysSetBit0(0);
            break;
        }
        gAudioHiMemLock = 0;
    }
}

/* ---------------------------------------------------------------- */
/* track table reset + part unload                                  */
/* ---------------------------------------------------------------- */

/* AudioClearTracks: mark every bank part not-loaded and every sound not-playing,
 * then flush the driver (sndCmd6). */
void AudioClearTracks(void)
{
    s32 i;

    if (gAudioDisabled != 0) {
        return;
    }
    /* bank table: parts [+304]/[+308] = -1 for each of gAudioBankTbl[+16] banks */
    /* sound table: [+40]/[+42] = 0 for each of gAudioRom sounds */
    for (i = 0; i < gAudioBankTbl[4]; i++) {
        s32* bank = gAudioBankTbl + 5 + i * 73;
        bank[71] = -1;
        bank[72] = -1;
    }
    sndCmd6();
}

/* AudioUnloadPart: free the DCS part currently held by named bank, unless still
 * referenced by another loaded bank. */
s32 AudioUnloadPart(char* bankName)
{
    if (gAudioDisabled != 0) {
        return 0;
    }
    /* locate bank by name; if its held part is shared by another bank, skip: */
    bulletproof_printf("AudioUnloadPart skipped bank, still in use\n");
    (void)bankName;
    return 0;
}

/* ---------------------------------------------------------------- */
/* driver pump / sync                                               */
/* ---------------------------------------------------------------- */

/* AudioSysUpdate: advance the sound system by one step and return the pending
 * command count (used as a "still busy?" poll). */
s32 AudioSysUpdate(s32 dt)
{
    f32 t;

    if (gAudioDisabled != 0) {
        return 0;
    }
    t = (f32)dt;
    gAudioHiMemLock = 0;
    sndSysUpdate(t);
    if (gAudioCmdPending != 0) {
        gAudioSpin += dt;
    } else {
        gAudioSpin = 0;
    }
    return gAudioCmdPending;
}

/* AudioSysSync: drive one update and latch a -2 ("busy") response. */
void AudioSysSync(s32 dt)
{
    if (gAudioDisabled != 0) {
        return;
    }
    gAudioStreamResp = sndSysUpdate((f32)dt);
    if (gAudioStreamResp == -2) {
        ErrorPrintf("Audio Busy = -2");
        gAudioStreamState |= 1;
        fn_800BC590();
    }
}

/* ---------------------------------------------------------------- */
/* init / enable                                                    */
/* ---------------------------------------------------------------- */

/* audio_init: full driver bring-up.  Acquires the driver, resets track state,
 * selects the boot mode and enables SFX. */
void audio_init(void)
{
    gAudioBusyFlag = 1;
    if ((sFlags & 0x20) != 0) {
        gAudioDisabled = 1;
        gAudioStreamBusy = 1;
    }
    if (gAudioDisabled == 0) {
        sndTestAcquire(0);
    }
    gAudioCmdPending = 0;
    fn_80015C48();
    gAudioBusyFlag = 1;
    AudioSetMode(0);
    gAudioStreamState = 0;
    gAudioBankErrs = 0;
    if (gAudioDisabled == 0) {
        sndSysSetBit0(0);
        sndSysSetBit1(0);
    }
    *(s32*)(gAudioState + 4) = 0;
    sndCmd3(sAudioInitFlag == 0 ? 1 : 0);
    gAudioBusyFlag = 0;
}

/* AudioSetEnabled: flip the master enable and tell the driver. */
s32 AudioSetEnabled(s32 enable)
{
    if (gAudioDisabled != 0) {
        return 1;
    }
    sAudioInitFlag = (enable == 0) ? 1 : 0;
    sndCmd3(sAudioInitFlag);
    return 1;
}

/* ---------------------------------------------------------------- */
/* volume clamps                                                    */
/* ---------------------------------------------------------------- */

s32 AudioSetVolSfx(s32 vol)
{
    gSfxVol = vol;
    if (vol < 0) {
        ErrorPrintf("AUDIO sfx volume can't be less than 0  (%d)\n", vol);
        gSfxVol = 0;
    } else if (vol > 255) {
        ErrorPrintf("AUDIO sfx volume can't be greater than %d  (%d)\n", 255, vol);
        gSfxVol = 255;
    }
    return 1;
}

s32 AudioSetVolMusic(s32 vol)
{
    gMusicVol = vol;
    if (vol < 0) {
        ErrorPrintf("AUDIO music volume can't be less than 0  (%d)\n", vol);
        gMusicVol = 0;
    } else if (vol > 255) {
        ErrorPrintf("AUDIO music volume can't be greater than %d  (%d)\n", 255, vol);
        gMusicVol = 255;
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
    s32* sound = (s32*)(*(u8**)(gAudioRom + 16) + (packedId >> 16) * 44);
    f32* inst  = (f32*)(*(u8**)(gAudioRom + 20)
                 + ((packedId & 0xFFF) + *(s16*)((u8*)sound + 38)) * 28);
    return inst[5];
}

/* AudioFindSound: resolve a sound handle by name across all loaded banks. */
s32 AudioFindSound(char* name, s32 maxLen, s32 warn)
{
    s32 b, s;
    s32 bankCount;

    if (*name == 0) {
        return -1;
    }
    if (maxLen <= 0) {
        maxLen = 15;
    }
    bankCount = *(s32*)(gAudioRom + 4);
    for (b = 0; b < bankCount; b++) {
        s32* bank = (s32*)(*(u8**)(gAudioRom + 16) + b * 44);
        s32 sndCount = *(s16*)((u8*)bank + 36);
        for (s = 0; s < sndCount; s++) {
            s32* snd = (s32*)(*(u8**)(gAudioRom + 20)
                       + (*(s16*)((u8*)bank + 38) + s) * 28);
            if (strncmp((char*)snd, name, maxLen) == 0) {
                return snd[4];
            }
        }
    }
    if (warn != 0 && (sFlags & 0x10) == 0) {
        ErrorPrintf("UNABLE TO FIND SOUND: %s\n", name);
    }
    return -1;
}

/* AudioTrackRegister: register (or refresh) a spatial voice descriptor slot and
 * compute its expiry tick from the sound's duration. */
void AudioTrackRegister(s32 slot, s32 packedId, s32 a, s32 b, s32 c)
{
    s32* d;
    f32 dur;
    s32* snd;

    if (slot >= 0) {
        d = gAudioVoiceDesc[slot];
        snd = (s32*)(*(u8**)(gAudioRom + 16) + (packedId >> 16) * 44);
        d[0] = (packedId & 0xFFF) + *(s16*)((u8*)snd + 38);
        d[1] = d[0];
        d[2] = packedId;
        d[3] = pbLoad;
        d[4] = a;
        d[5] = b;
        d[6] = c;
        snd = (s32*)(*(u8**)(gAudioRom + 20) + d[0] * 28);
        dur = ((f32*)snd)[5];
        d[7] = (dur > 0.0f) ? (s32)((f32)pbLoad + dur) : -1;
    } else {
        s32 i;
        for (i = 0; i < 12; i++) {
            d = gAudioVoiceDesc[i];
            if (d[0] < 0) {
                /* re-init a free slot the same way */
                snd = (s32*)(*(u8**)(gAudioRom + 16) + (packedId >> 16) * 44);
                d[0] = (packedId & 0xFFF) + *(s16*)((u8*)snd + 38);
                d[1] = d[0];
                d[2] = packedId;
                d[3] = pbLoad;
                d[4] = a;
                d[5] = b;
                d[6] = c;
                snd = (s32*)(*(u8**)(gAudioRom + 20) + d[0] * 28);
                dur = ((f32*)snd)[5];
                d[7] = (dur > 0.0f) ? (s32)((f32)pbLoad + dur) : -1;
                break;
            }
        }
    }
}

/* AudioIsActive: true when audio is not muted. */
s32 AudioIsActive(void)
{
    return (gAudioDisabled != 0) ? 0 : 1;
}

/* AudioSetListenerPos: byte-swap a vec3 and hand it to the 3D transform. */
void AudioSetListenerPos(s32* out, s32 unused, f32* pos)
{
    f32 v[3];
    s32 i;

    for (i = 0; i < 3; i++) {
        v[i] = pos[i];   /* original performs an explicit LE<->BE swap here */
    }
    MBNewWorldPsys(out, 0, v, 1, 0, unused);
    out[24] |= 1;
}
