#include "types.h"

/* Low-level sound-effect / voice engine (GC snd* driver family), first slice
 * 0x800150CC-0x800160F0.
 *
 * This is the sound-playback engine that sits above the DCS-style sound driver
 * (soundmgr.c: sndSysUpdate/sndCmdD/sndRegisterPair) and below the game-facing
 * SOUNDS module (sounds.c, 0x800A0xxx) and the AUDIO module (0x800D2xxx).  It
 * owns the active-voice table, the deferred sound-request queue, 3D panning
 * and per-frame servicing, and is called by sounds.c and 20+ gameplay TUs
 * (sndFxPlay3D alone has 36 caller objects).  The full engine TU continues
 * past this slice into 0x800160F0-0x800183FC (voice/track helpers
 * AudioSetTrackPan..fn_80018xxx, still auto).
 *
 * The sndFx* names are DESCRIPTIVE (exact Midway identifiers unconfirmed).  The
 * `Audio*` prefix is already taken by SOUNDS.OBJ and AUDIO.OBJ, and this module
 * belongs to the snd* driver family, hence the sndFx* prefix.
 *
 * Sound ids are (bank << 16) | index.  gCameras[0] holds the listener/camera
 * transform (basis at +4/+8/+12, position at +300/+304/+308); pan is the
 * horizontal projection of the source onto the listener's right axis.
 *
 * NonMatching: most bodies are byte-exact (sndFxInit/QueAdd/PlayHandle/PlayEx
 * fully match; QueEmpty/ResetVoices/Play3D/Play3DTracked/Play3DAtten have an
 * identical instruction stream, differing only in constant-pool symbol names).
 * The remainder (QueAddEx/QueUpdate/Update/InitVoices/StartVoice/VoiceUpdateCb)
 * are structurally faithful with register-allocation / addressing-mode residuals.
 * The original bytes stay linked (Object NonMatching) so the DOL is byte-exact.
 */

typedef struct {
    f32 x;
    f32 y;
    f32 z;
} Vec3;

/* Listener/camera transform (subset of the 0x948-byte sAudioListener block). */
typedef struct {
    u8 _pad0[4];
    f32 m4;  /* right/basis components used for the pan projection */
    f32 m8;
    f32 m12;
    u8 _pad1[300 - 16];
    f32 px;  /* +300 listener position */
    f32 py;
    f32 pz;
} AudioListener;

/* One active-voice record; table starts at sAudioState + 1320, stride 48. */
typedef struct {
    s32 soundId;   /* +0  */
    s32 flags;     /* +4  */
    s32 field8;    /* +8  */
    Vec3* pos;     /* +12 */
    s32 field10;   /* +16 */
    s32 seq;       /* +20 */
    u8 pad[12];    /* +24 */
    void* update;  /* +36 callback (sndFxVoiceUpdateCb) */
    void* self;    /* +40 */
    s32 field2C;   /* +44 */
} AudioVoice;

#define AUDIO_VOICES(st) ((AudioVoice*)((u8*)(st) + 1320))
#define AUDIO_NUM_VOICES 32

/* Struct view of the audio-core state block (sAudioState) for the init
 * sweeps: channel records at +24, track slots at +1176, the voice table at
 * +1320 and the per-channel update queue at +2860. */
typedef struct SndFxChan { s32 f0, f4, f8, fC, f10, f14, f18, f1C; } SndFxChan;
typedef struct SndFxTrack { s32 f0; u8 _p[32]; } SndFxTrack;
typedef struct SndFxQue20 { s32 f0; u8 _p[16]; } SndFxQue20;

typedef struct SndFxState {
    u8 _pad0[24];
    SndFxChan chan[12];   /* +24   */
    u8 _pad1[768];        /* +408  */
    SndFxTrack trk[4];    /* +1176 */
    AudioVoice voice[32]; /* +1320 */
    u8 _pad2[4];          /* +2856 */
    SndFxQue20 que[12];   /* +2860 */
} SndFxState;

/* One deferred-request slot; 16 per mode, at lbl_8023D398[mode][slot]. */
typedef struct {
    s32 soundId; /* +0  */
    s32 f4;      /* +4  */
    s32 f8;      /* +8  */
    s32 fC;      /* +12 */
    f32 f10;     /* +16 */
} QueSlot;

/* Mode-relative view of the request queue (slots ride at st+mode*320+408). */
typedef struct QueSlotView {
    u8 _p[408];
    s32 soundId; /* +408 */
    s32 f4;      /* +412 */
    s32 f8;      /* +416 */
    s32 fC;      /* +420 */
    f32 f10;     /* +424 */
} QueSlotView;

/* Sound-bank header + per-sound descriptor records. */
typedef struct SndFxMode { u8 _p[320]; } SndFxMode;
typedef struct V48 { u8 _p[48]; } V48;
typedef struct VoiceView { u8 _p[1324]; s32 flags; } VoiceView;
typedef struct SndBankHdr { u8 _p[16]; u8* tbl16; u8* tbl20; } SndBankHdr;
typedef struct SndDesc44 { u8 _p[38]; s16 base; u16 _x40; u16 h42; } SndDesc44; /* 44 */
typedef struct SndDescRec { u8 _p[20]; f32 vol; f32 fade; } SndDescRec; /* 28 */

/* --- low-level sound driver (soundmgr.c) --- */
extern void sndSysUpdate(f32 a);
extern void sndCmdD(void);
extern int sndRegisterPair(s32* rec, int chans, void* out);

/* --- other cross-region callees --- */
extern void ErrorPrintf(const char* fmt, ...);
extern void fn_800C0310(void);
extern int fn_800BC418(int a, int b);
extern s32 fn_80053D08(s32 wave, s32 mode, s32 loadResult);
extern void bulletproof_printf(const char* fmt, ...);
extern void init_moving_objects(int a);
extern f32 DistanceToClosestPlayer(Vec3* a);     /* distance from listener */
extern f32 NormalVector(Vec3* a);    /* normalize vector, return approximate length */
extern int AudioAng(Vec3* pos);      /* position -> spatial descriptor */
extern void AudioKillMask(int mask); /* stop voices by channel mask */
extern void AudioTrackRegister(int a, int b, int c, int d, int e); /* per-channel apply */

/* --- audio-core state (see symbols.txt) --- */
extern u8 sAudioState[];
extern QueSlot lbl_8023D398[2][16]; /* deferred-request slots (sAudioState+408) */
extern u8 sAudioChanUpdate[]; /* 12 * 20 */
extern f32 gCameras[]; /* camera array; listener transform lives at gCameras[0] */
extern s32 sAudioInitFlag;
extern s32 sAudioOverride;
extern s32 lbl_803442A8;
extern s32 sAudioQueBusy;
extern s32 sAudioQueCount[2];
extern f32 sAudioQueFade[2];
extern s32 lbl_803442EC;
extern void* sAudioBankTable;
extern u16 sAudioVoiceSeq;
extern s32 sAudioErrFlags;
extern s32 sAudioTimeoutAcc;
extern s32 sAudioTimeoutErrs;
extern s32 sAudioSuspend;
extern s32 sAudioReady;
extern s32 sAudioMute;
extern volatile f32 sMusicFadeBase;
extern u32 pbLoad;
extern const char sAudioWatchdogName[];
extern const char sAudioTimeoutMsg[];
extern const char sAudioBankNotLoadedMsg[];

/* raw driver-side globals kept as lbl_ (shared broadly, unnamed) */
extern s32 gGameMode; /* current e_mode id; bit 0x8000 = attract-loop group */
extern s32 lbl_80344290; /* mode id latched at init (see attract.c) */
extern s32 gGameBusy;
extern s32 lbl_8034481C;
extern s32 lbl_803447D0;
extern s32 welcome_timer;
extern s32 lbl_8034429C;
extern s32 lbl_80344778;
extern s32 lbl_80344774;
extern s32 lbl_8034420C;
extern s32 lbl_80343B04;
extern s32 lbl_80343B08;
extern s32 lbl_80343B0C;
extern s32 lbl_80343B10;
extern s32 lbl_80343B48; /* p2 scale (=128): field8 = (p2 * this) >> 8 */

/* forward decls */
void sndFxVoiceUpdateCb(void* v);
int sndFxStartVoice(int handle, int soundId, int p2, Vec3* pos, int pan, int flags);
void sndFxInitVoices(void);
f32 sndFxQueAddEx(int mode, int soundId, f32 vol, f32 param, int pri, int track, int flags);
int sndFxQueUpdate(void);

/* Is audio currently silenced (paused and not overridden)? */
#define sndFxPaused() ((gGameMode & 0x8000) && sAudioOverride == 0)

/* Horizontal stereo pan (-256..255) for a world position, relative to the
 * listener transform. */
static inline int sndFxComputePan(Vec3* pos)
{
    AudioListener* L;
    Vec3 rel;
    f32 len;
    f32 dot;
    int pan;

    if (sAudioInitFlag != 0 || pos == 0) {
        return 127;
    }
    L = (AudioListener*)gCameras;
    rel.x = pos->x - L->px;
    rel.y = pos->y - L->py;
    rel.z = pos->z - L->pz;
    rel.y = 0.0f;
    len = NormalVector(&rel) / 20.0f;
    dot = rel.x * L->m4 + rel.y * L->m8 + rel.z * L->m12;
    pan = (int)(127.5 * dot * (1.0 < len ? 1.0 : len) + 127.5);
    if (L->m12 * rel.x > L->m4 * rel.z) {
        pan = -pan;
    }
    if (pan < -256) {
        pan = -256;
    } else if (pan > 255) {
        pan = 255;
    }
    return pan;
}

/* sndFxInit: one-time audio-core init (resets state, starts driver). */
void sndFxInit(s32 mode, s32 wave)
{
    lbl_80344290 = mode;
    gGameMode = mode;
    gGameBusy = 0;
    lbl_8034481C = 0;
    lbl_803447D0 = 0;
    welcome_timer = 0;
    lbl_8034429C = 0;
    lbl_80344778 = 0;
    lbl_80344774 = 0;
    lbl_8034420C = 0;
    fn_800C0310();
    fn_800BC418(2, -1);
    fn_80053D08(wave, 0, lbl_80343B08);
    lbl_80343B04 = -1;
    lbl_80343B0C = -1;
    lbl_80343B10 = -1;
    lbl_80343B08 = -1;
    bulletproof_printf(sAudioWatchdogName);
    init_moving_objects(1);
}

/* sndFxQueAdd: enqueue helper (default mode 0). */
f32 sndFxQueAdd(int soundId, f32 vol, f32 param, int pri, int track, int flags)
{
    return sndFxQueAddEx(0, soundId, vol, param, pri, track, flags);
}

/* sndFxQueAddEx: enqueue a sound request with volume/priority/track; returns the
 * effective volume (0.0 = rejected). */
f32 sndFxQueAddEx(int mode, int soundId, f32 vol, f32 param, int pri, int track, int flags)
{
    u8* st = sAudioState;
    int busy = sAudioQueBusy;
    int n;
    int i;
    f32 acc;
    QueSlot* q;
    QueSlotView* sl;

    if (sndFxPaused()) {
        return 0.0f;
    }
    n = sAudioQueCount[mode];
    if (n >= 16) {
        return 0.0f;
    }
    if (n > 0) {
        acc = sAudioQueFade[mode];
        if (sAudioQueFade[mode] == 0.0) {
            sl = (QueSlotView*)&((SndFxMode*)st)[mode];
            acc = (f32)pbLoad + sl->f10;
        }
        sl = (QueSlotView*)&((SndFxMode*)st)[mode];
        q = (QueSlot*)&sl->soundId;
        for (i = 1; i < n; i++) {
            acc += q[i].f10;
        }
        if (param >= 0.0 && acc - (f32)pbLoad > 60.0f * param) {
            return 0.0f;
        }
    } else {
        acc = (f32)pbLoad;
    }
    if (soundId >= 0) {
        SndBankHdr* bt = (SndBankHdr*)sAudioBankTable;
        SndDesc44* dt;
        SndDescRec* rt;
        int di;
        dt = (SndDesc44*)bt->tbl16;
        di = (soundId & 0xFFF) + dt[soundId >> 16].base;
        if (vol <= 0.0) {
            rt = (SndDescRec*)bt->tbl20;
            vol = 60.0f * rt[di].vol;
        }
        rt = (SndDescRec*)bt->tbl20;
        rt[di].fade = acc;
    }
    sAudioQueBusy = 1;
    st += mode * 320;
    q = (QueSlot*)st;
    sl = (QueSlotView*)&q[n];
    sl->soundId = soundId;
    sl->f4 = pri;
    sl->f8 = track;
    sl->fC = flags;
    sl->f10 = vol;
    sAudioQueCount[mode]++;
    if (busy == 0) {
        sAudioQueBusy = 0;
    }
    return vol;
}

/* sndFxQueEmpty: empty the pending request queue. */
void sndFxQueEmpty(void)
{
    int busy = sAudioQueBusy;

    sAudioQueBusy = 1;
    sAudioQueCount[0] = 0;
    sAudioQueCount[1] = 0;
    sAudioQueFade[0] = 0.0f;
    sAudioQueFade[1] = 0.0f;
    lbl_803442EC = 0;
    if (busy == 0) {
        sAudioQueBusy = 0;
    }
}

/* sndFxQueUpdate: process both pending queues, starting the queued voices. */
int sndFxQueUpdate(void)
{
    int mode;
    int did = 0;
    u8 unused[8];

    if (sAudioSuspend != 0) {
        return 0;
    }
    if (sAudioMute == 0 && sAudioQueBusy == 0) {
        for (mode = 0; mode < 2; mode++) {
            int n = sAudioQueCount[mode];
            f32* fade;

            if (n <= 0) {
                continue;
            }
            did = 1;
            fade = &sAudioQueFade[mode];
            if (*fade == 0.0) {
                QueSlot* slots = lbl_8023D398[mode];
                if (slots[0].soundId >= 0) {
                    sndFxStartVoice(-1, slots[0].soundId, slots[0].f4, 0,
                                    slots[0].f8, slots[0].fC);
                }
                *fade = (f32)pbLoad + slots[0].f10;
            } else if ((f32)pbLoad >= *fade) {
                /* shift remaining requests down */
                int j;
                for (j = 1; j < n; j++) {
                    lbl_8023D398[mode][j - 1] = lbl_8023D398[mode][j];
                }
                *fade = 0.0f;
                sAudioQueCount[mode] = sAudioQueCount[mode] - 1;
            }
        }
    }
    return did;
}

/* sndFxUpdate: per-frame audio service (drives the driver + queues). */
int sndFxUpdate(int frames)
{
    int i;
    u8* ch;
    f32 v;

    lbl_803442A8 = 0;
    if (sAudioSuspend != 0) {
        return 0;
    }
    sndSysUpdate(1.0f);
    if (sndFxQueUpdate() != 0) {
        sAudioTimeoutAcc += frames;
        if (sAudioTimeoutAcc > 50000000) {
            ErrorPrintf(sAudioTimeoutMsg);
            sAudioTimeoutErrs++;
            return 0;
        }
        return 1;
    }
    for (i = 0; i < 12; i++) {
        ch = sAudioChanUpdate + i * 20;
        v = *(f32*)(ch + 8);
        if (*(s32*)ch != 0 && v > 0.0f && sMusicFadeBase < v) {
            break;
        }
    }
    sAudioTimeoutAcc = 0;
    return 0;
}

/* sndFxResetVoices: stop everything and reinitialise the voice tables. */
void sndFxResetVoices(void)
{
    int busy;

    sAudioQueBusy = 1;
    sndCmdD();
    busy = sAudioQueBusy;
    sAudioQueCount[0] = 0;
    sAudioQueBusy = 1;
    sAudioQueCount[1] = 0;
    sAudioQueFade[0] = 0.0f;
    sAudioQueFade[1] = 0.0f;
    lbl_803442EC = 0;
    if (busy == 0) {
        sAudioQueBusy = 0;
    }
    AudioKillMask(8191);
    sndFxInitVoices();
    sAudioQueBusy = 0;
}

/* sndFxPlay3DTracked: play a positioned 3D sound (pan from position). */
int sndFxPlay3DTracked(int soundId, Vec3* pos, int p2, int flags)
{
    if (sndFxPaused()) {
        return 0;
    }
    return sndFxStartVoice(-1, soundId, p2, pos, sndFxComputePan(pos), flags);
}

/* sndFxPlayHandle: play a sound (voice handle result). */
int sndFxPlayHandle(int soundId, int p2, int flags)
{
    return sndFxStartVoice(-1, soundId, p2, 0, 127, flags);
}

/* sndFxPlay3DAtten: positioned play with distance attenuation and pan. */
int sndFxPlay3DAtten(int soundId, Vec3* pos, int p2, int flags)
{
    u8 frame_pad[8];
    f32 atten;
    int pan;

    if (sndFxPaused()) {
        return 0;
    }
    if (soundId < 0) {
        return 0;
    }
    if (pos == 0) {
        atten = 1.0f;
    } else {
        f32 t;
        f64 d;
        d = DistanceToClosestPlayer(pos);
        t = 1.4 - d / 50.0;
        atten = t < 0.0 ? 0.0 : t > 1.0 ? 1.0 : t;
    }
    if (atten <= 0.0) {
        return 0;
    }
    pan = sndFxComputePan(pos);
    return sndFxStartVoice(-1, soundId, (int)(p2 * atten), pos, pan, flags);
}

/* sndFxPlayEx: play a sound with explicit id/params (no positioning). */
int sndFxPlayEx(int soundId, int p1, int pan, int flags)
{
    if (sndFxPaused()) {
        return 0;
    }
    if (soundId < 0) {
        return 0;
    }
    return sndFxStartVoice(-1, soundId, pan, 0, p1, flags);
}

/* sndFxPlay3D: play a positioned 3D sound (pan from position). */
int sndFxPlay3D(int soundId, Vec3* pos, int p2, int flags)
{
    int pan;

    if (sndFxPaused()) {
        return 0;
    }
    if (soundId < 0) {
        return 0;
    }
    pan = sndFxComputePan(pos);
    return sndFxStartVoice(-1, soundId, p2, pos, pan, flags);
}

/* sndFxInitVoices: (re)initialise voice, channel and track tables. */
void sndFxInitVoices(void)
{
    SndFxState* st = (SndFxState*)sAudioState;
    SndFxChan* c;
    int i;

    for (i = 0; i < 12; i++) {
        st->que[i].f0 = 0;
    }
    for (i = 0; i < 32; i++) {
        st->voice[i].flags = 0;
    }
    for (i = 0; i < 4; i++) {
        st->trk[i].f0 = -1;
    }
    c = st->chan;
    for (i = 0; i < 12; i++) {
        c->f0 = -1;
        c->f4 = -1;
        c->f8 = 0;
        c->fC = 0;
        c->f10 = 0;
        c->f14 = 127;
        c->f18 = 0;
        c->f1C = -1;
        c++;
    }
}

/* sndFxStartVoice: core voice start.  Looks up the bank/sound, allocates a free
 * voice slot and registers it with the driver (update callback =
 * sndFxVoiceUpdateCb).  Returns the 16-bit voice sequence id. */
int sndFxStartVoice(int handle, int soundId, int p2, Vec3* pos, int pan, int flags)
{
    SndFxState* stv = (SndFxState*)sAudioState;
    int prevBusy = sAudioQueBusy;
    int seq = 0;
    int slot;
    int di;
    int fl;
    s32 rec[3];

    if (sAudioMute == 0 && sAudioReady >= 0) {
        u8* banks = (u8*)sAudioBankTable;
        int bank = soundId >> 16;
        SndDesc44* dt;
        int h;

        dt = (SndDesc44*)*(u8**)(banks + 16);
        h = dt[bank].h42;
        if (h == 0 || h == 0xFFFF) {
            if (h == 0) {
                ErrorPrintf(sAudioBankNotLoadedMsg,
                            (u8*)dt + bank * 44 + 16,
                            *(u8**)(banks + 20)
                                + ((soundId & 0xFFF) + dt[bank].base) * 28);
                dt = (SndDesc44*)((SndBankHdr*)sAudioBankTable)->tbl16;
                dt[bank].h42 = 0xFFFF;
            }
            return 0;
        }
        di = (soundId & 0xFFF) + h;
        if (handle >= 0) {
            fl = (handle & 0x1FFF) | 0x8000;
        } else {
            fl = flags;
        }
        p2 = (p2 * lbl_80343B48) >> 8;
        sAudioQueBusy = 1;
        if (sAudioInitFlag != 0) {
            pan = 127;
        }
        rec[0] = di;
        rec[1] = (p2 << 16) | (pan & 0xFFFF);
        rec[2] = fl;
        for (slot = 0; slot < 32; slot++) {
            switch (stv->voice[slot].flags) {
            case 0:
                goto have_slot;
            }
        }
        slot = -1;
have_slot:
        if (slot >= 0 && sAudioSuspend == 0) {
            if (pos != 0) {
                pan = AudioAng(pos);
            }
            sAudioVoiceSeq += 1;
            if (sAudioVoiceSeq > 16383) {
                sAudioVoiceSeq = 1;
            }
            stv->voice[slot].update = (void*)sndFxVoiceUpdateCb;
            stv->voice[slot].self = &stv->voice[slot];
            stv->voice[slot].soundId = soundId;
            stv->voice[slot].field8 = p2;
            stv->voice[slot].pos = pos;
            stv->voice[slot].field10 = pan;
            stv->voice[slot].flags = flags;
            stv->voice[slot].seq = sAudioVoiceSeq;
            seq = stv->voice[slot].seq;
            if (sndRegisterPair(rec, 3, &stv->voice[slot].pad[0]) <= 0) {
                sAudioErrFlags |= 2;
                stv->voice[slot].flags = 0;
                stv->voice[slot].seq = 0;
            }
        }
        if (prevBusy == 0) {
            sAudioQueBusy = 0;
        }
    } else {
        if (handle < 0 && pos == 0) {
            sndFxQueAddEx(0, soundId, -1.0f, 2.0f, p2, pan, flags);
        }
    }
    return seq & 0xFFFF;
}

/* sndFxVoiceUpdateCb: per-voice frame update callback (driver-invoked).  Recomputes
 * spatial parameters for each of the 12 driver channels the voice occupies. */
void sndFxVoiceUpdateCb(void* v)
{
    s32* raw = *(s32**)((u8*)v + 16); /* driver hands back the voice record */
    int soundId = raw[0];
    int chmask = (int)(*(u32*)((u8*)v + 8) >> 16);
    int boff;
    int active = 0;
    int ch;
    int off;
    u8* work = sAudioChanUpdate;

    boff = (((SndDesc44*)((SndBankHdr*)sAudioBankTable)->tbl16)[soundId >> 16].base
            + (soundId & 0xFFF)) * 28;
    ((SndDescRec*)&((SndBankHdr*)sAudioBankTable)->tbl20[boff])->fade = (f32)pbLoad;
    for (ch = 0, off = 0; ch < 12; ch++, off += 20) {
        if (chmask & (1 << ch)) {
            s32* chan = (s32*)(work + off);
            chan[0] = soundId;
            chan[1] = raw[1];
            *(f32*)&chan[2] = 60.0f
                * *(f32*)((((SndBankHdr*)sAudioBankTable)->tbl20 + 20) + boff)
                + (f32)pbLoad;
            chan[3] = raw[3];
            chan[1] = raw[1];
            chan[4] = raw[5];
            AudioTrackRegister(ch, soundId, raw[2], raw[4], raw[1]);
            active = 1;
            if (raw[5] == 0 || raw[1] == 0) {
                AudioKillMask(1 << ch);
            }
        }
    }
    if (active == 0) {
        AudioTrackRegister(-1, soundId, raw[2], raw[4], raw[1]);
    }
    raw[1] = 0;
    raw[5] = 0;
}
