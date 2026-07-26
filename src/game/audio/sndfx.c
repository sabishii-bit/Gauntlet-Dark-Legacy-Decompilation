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
 * fn_800160F0..fn_80018xxx, still auto).
 *
 * The sndFx* names are DESCRIPTIVE (exact Midway identifiers unconfirmed).  The
 * `Audio*` prefix is already taken by SOUNDS.OBJ and AUDIO.OBJ, and this module
 * belongs to the snd* driver family, hence the sndFx* prefix.
 *
 * Sound ids are (bank << 16) | index.  sAudioListener holds the camera
 * transform (basis at +4/+8/+12, position at +300/+304/+308); pan is the
 * horizontal projection of the source onto the listener's right axis.
 *
 * NonMatching: bodies are structurally faithful reconstructions; the original
 * bytes are linked (Object marked NonMatching) so the DOL stays byte-exact.
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

/* --- low-level sound driver (soundmgr.c) --- */
extern void sndSysUpdate(void);
extern void sndCmdD(void);
extern int sndRegisterPair(void* voice, int chans, s32* a, s32* b, s32* c, s32* d);

/* --- other cross-region callees --- */
extern void ErrorPrintf(const char* fmt, ...);
extern void fn_800C0310(void);
extern int fn_800BC418(int a, int b);
extern void fn_80053D08(void* a, int b, int c);
extern void fn_800BC2EC(void* a);
extern void fn_80053A10(int a);
extern f32 fn_8006366C(Vec3* a);     /* distance from listener */
extern f32 fn_800BDA98(Vec3* a);     /* xz-plane vector length */
extern int fn_800167EC(Vec3* pos);   /* position -> spatial descriptor */
extern void fn_80016720(int mask);   /* stop voices by channel mask */
extern void fn_8001817C(int a, int b, int c, int d); /* per-channel apply */

/* --- audio-core state (see symbols.txt) --- */
extern u8 sAudioState[];
extern u8 sAudioChanUpdate[]; /* 12 * 20 */
extern AudioListener sAudioListener;
extern s32 sAudioInitFlag;
extern s32 sAudioOverride;
extern s32 sAudioQueBusy;
extern s32 sAudioQueCount[];
extern f32 sAudioQueFade[];
extern void* sAudioBankTable;
extern s32 sAudioVoiceSeq;
extern s32 sAudioErrFlags;
extern s32 sAudioTimeoutAcc;
extern s32 sAudioTimeoutErrs;
extern s32 sAudioSuspend;
extern s32 sAudioReady;
extern s32 sAudioMute;
extern f32 sMusicFadeBase;
extern f32 pbLoad;
extern const char sAudioWatchdogName[];
extern const char sAudioTimeoutMsg[];
extern const char sAudioBankNotLoadedMsg[];

/* raw driver-side globals kept as lbl_ (shared broadly, unnamed) */
extern s32 lbl_8034477C; /* audio enable/pause flags (bit 0x8000 = paused) */
extern void* lbl_80344290;
extern s32 lbl_80344568;
extern s32 lbl_8034481C;
extern s32 lbl_803447D0;
extern s32 lbl_80344B1C;
extern s32 lbl_8034429C;
extern s32 lbl_80344778;
extern s32 lbl_80344774;
extern s32 lbl_8034420C;
extern s32 lbl_80343B04;
extern s32 lbl_80343B08;
extern s32 lbl_80343B0C;
extern s32 lbl_80343B10;
extern s32 lbl_80343B48;

/* forward decls */
void sndFxVoiceUpdateCb(AudioVoice* v);
int sndFxStartVoice(int handle, int soundId, int p2, Vec3* pos, int pan, int flags);
void sndFxInitVoices(void);
f32 sndFxQueAddEx(int mode, int soundId, f32 vol, f32 param, int pri, int track, int flags);
int sndFxQueUpdate(void);

/* Is audio currently silenced (paused and not overridden)? */
static int sndFxPaused(void)
{
    return (lbl_8034477C & 0x8000) && sAudioOverride == 0;
}

/* Horizontal stereo pan (-256..255) for a world position, relative to the
 * listener transform. */
static int sndFxComputePan(Vec3* pos)
{
    AudioListener* L = &sAudioListener;
    Vec3 rel;
    f32 len;
    f32 dot;
    f32 crossA;
    f32 crossB;
    int pan;

    if (sAudioInitFlag != 0 || pos == 0) {
        return 127;
    }
    rel.x = pos->x - L->px;
    rel.y = 0.0f;
    rel.z = pos->z - L->pz;
    len = fn_800BDA98(&rel) / 1000.0f;
    if (len > 1.0f) {
        len = 1.0f;
    }
    dot = rel.x * L->m4 + rel.z * L->m12;
    crossA = L->m12 * rel.x;
    crossB = L->m4 * rel.z;
    pan = (int)(dot * len * 256.0f);
    if (crossA <= crossB) {
        pan = -pan;
    }
    if (pan < -256) {
        pan = -256;
    } else if (pan > 255) {
        pan = 255;
    }
    return pan;
}

/* fn_800150CC: one-time audio-core init (resets state, starts driver). */
void sndFxInit(void* a, void* b)
{
    lbl_80344290 = a;
    lbl_8034477C = (s32)a;
    lbl_80344568 = 0;
    lbl_8034481C = 0;
    lbl_803447D0 = 0;
    lbl_80344B1C = 0;
    lbl_8034429C = 0;
    lbl_80344778 = 0;
    lbl_80344774 = 0;
    lbl_8034420C = 0;
    fn_800C0310();
    fn_800BC418(2, -1);
    fn_80053D08(b, 0, lbl_80343B08);
    lbl_80343B04 = -1;
    lbl_80343B0C = -1;
    lbl_80343B10 = -1;
    lbl_80343B08 = -1;
    fn_800BC2EC((void*)sAudioWatchdogName);
    fn_80053A10(1);
}

/* fn_8001516C: enqueue helper (default mode 0). */
int sndFxQueAdd(int soundId, f32 vol, f32 param, int track)
{
    return (int)sndFxQueAddEx(0, soundId, vol, param, 0, track, param);
}

/* fn_800151A8: enqueue a sound request with volume/priority/track; returns the
 * effective volume (0.0 = rejected). */
f32 sndFxQueAddEx(int mode, int soundId, f32 vol, f32 param, int pri, int track, int flags)
{
    s32* count = &sAudioQueCount[mode];
    f32* fade = &sAudioQueFade[mode];
    u8* bank = (u8*)sAudioState + mode * 320;
    int n;
    int i;
    f32 acc;

    if (sndFxPaused()) {
        return 0.0f;
    }
    n = *count;
    if (n >= 16) {
        return 0.0f;
    }
    if (n > 0) {
        acc = 0.0f;
        if (*fade == 0.0) {
            *count = 1;
            acc = pbLoad - param;
        }
        for (i = 0; i < n; i++) {
            acc += *(f32*)(bank + 408 + i * 20 + 16);
        }
        if (vol > 0.0 && acc > vol * 0.001f) {
            return 0.0f;
        }
    }
    /* record the request at slot n */
    {
        u8* slot = bank + 408 + n * 20;
        *(s32*)(slot + 0) = soundId;
        *(s32*)(slot + 4) = pri;
        *(s32*)(slot + 8) = track;
        *(s32*)(slot + 12) = flags;
        *(f32*)(slot + 16) = param;
    }
    *count = n + 1;
    if (mode == 0) {
        sAudioQueBusy = 0;
    }
    return vol;
}

/* fn_80015394: empty the pending request queue. */
void sndFxQueEmpty(void)
{
    int busy = sAudioQueBusy;

    sAudioQueBusy = 1;
    sAudioQueCount[0] = 0;
    sAudioQueFade[0] = 0.0f;
    sAudioQueFade[1] = 0.0f;
    if (busy == 0) {
        sAudioQueBusy = 0;
    }
}

/* fn_800154C8: process both pending queues, starting the queued voices. */
int sndFxQueUpdate(void)
{
    u8* q = sAudioChanUpdate;
    int did = 0;
    int mode;

    if (sAudioSuspend != 0) {
        return 0;
    }
    if (sAudioMute != 0 || sAudioQueBusy != 0) {
        return did;
    }
    for (mode = 0; mode < 2; mode++) {
        s32* count = &sAudioQueCount[mode];
        f32* fade = &sAudioQueFade[mode];
        u8* bank = (u8*)sAudioState + mode * 320;
        int n = *count;

        if (n <= 0) {
            continue;
        }
        did = 1;
        if (*fade == 0.0) {
            u8* slot = bank + 408;
            if (*(s32*)(slot + 0) >= 0) {
                sndFxStartVoice(-1, *(s32*)(slot + 4), *(s32*)(slot + 8), 0,
                                0, *(s32*)(slot + 12));
            }
            *fade = pbLoad - 1.0e12f + *(f32*)(slot + 16);
        } else if (pbLoad - 1.0e12f >= *fade) {
            /* shift remaining requests down */
            int j;
            for (j = 1; j < n; j++) {
                u8* dst = bank + 408 + (j - 1) * 20;
                u8* src = bank + 408 + j * 20;
                *(s32*)(dst + 0) = *(s32*)(src + 0);
                *(s32*)(dst + 4) = *(s32*)(src + 4);
                *(s32*)(dst + 8) = *(s32*)(src + 8);
                *(s32*)(dst + 12) = *(s32*)(src + 12);
                *(f32*)(dst + 16) = *(f32*)(src + 16);
            }
            *fade = 0.0f;
            *count = n - 1;
        }
    }
    return did;
}

/* fn_800153D4: per-frame audio service (drives the driver + queues). */
int sndFxUpdate(int frames)
{
    int i;
    u8* ch;

    if (sAudioSuspend != 0) {
        return 0;
    }
    sndSysUpdate();
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
        if (*(s32*)ch != 0 && *(f32*)(ch + 8) > 0.0f
            && sMusicFadeBase < *(f32*)(ch + 8)) {
            /* channel still fading */
        }
    }
    sAudioTimeoutAcc = 0;
    return 0;
}

/* fn_80015660: stop everything and reinitialise the voice tables. */
void sndFxResetVoices(void)
{
    int busy;

    sAudioQueBusy = 1;
    sndCmdD();
    busy = sAudioQueBusy;
    sAudioQueCount[0] = 0;
    sAudioQueBusy = 1;
    sAudioQueFade[0] = 0.0f;
    sAudioQueFade[1] = 0.0f;
    if (busy == 0) {
        sAudioQueBusy = 0;
    }
    fn_80016720(8191);
    sndFxInitVoices();
    sAudioQueBusy = 0;
}

/* fn_800156DC: play a positioned 3D sound (pan from position). */
void sndFxPlay3DTracked(int soundId, Vec3* pos, int p2, int flags)
{
    int pan;

    if (sndFxPaused()) {
        return;
    }
    pan = sndFxComputePan(pos);
    sndFxStartVoice(-1, soundId, p2, pos, pan, flags);
}

/* fn_80015834: play a sound (voice handle result). */
int sndFxPlayHandle(int soundId, Vec3* pos, int flags)
{
    if (sndFxPaused()) {
        return 0;
    }
    if (soundId < 0) {
        return 0;
    }
    return sndFxStartVoice(-1, soundId, (int)pos, 0, 127, flags);
}

/* fn_80015870: positioned play with distance attenuation and pan. */
void sndFxPlay3DAtten(int soundId, Vec3* pos, int p2, int flags)
{
    f32 atten;
    int pan;

    if (sndFxPaused()) {
        return;
    }
    if (soundId < 0) {
        return;
    }
    if (pos == 0) {
        atten = 1.0f;
    } else {
        f32 d = fn_8006366C(pos) / 100000.0f;
        atten = 1.0f - d;
        if (atten < 0.0f) {
            atten = 0.0f;
        } else if (atten > 1.0f) {
            atten = 1.0f;
        }
    }
    if (atten <= 0.0f) {
        return;
    }
    pan = sndFxComputePan(pos);
    sndFxStartVoice(-1, soundId, p2, pos, (int)((atten) * pan), flags);
}

/* fn_80015A78: play a sound with explicit id/params (no positioning). */
void sndFxPlayEx(int soundId, int p1, int pan, int flags)
{
    if (sndFxPaused()) {
        return;
    }
    if (soundId < 0) {
        return;
    }
    sndFxStartVoice(-1, soundId, p1, 0, pan, flags);
}

/* fn_80015ADC: play a positioned 3D sound (pan from position). */
void sndFxPlay3D(int soundId, Vec3* pos, int p2, int flags)
{
    int pan;

    if (sndFxPaused()) {
        return;
    }
    if (soundId < 0) {
        return;
    }
    pan = sndFxComputePan(pos);
    sndFxStartVoice(-1, soundId, p2, pos, pan, flags);
}

/* fn_80015C48: (re)initialise voice, channel and track tables. */
void sndFxInitVoices(void)
{
    u8* st = sAudioState;
    int i;

    for (i = 0; i < 12; i++) {
        *(s32*)(st + 2860 + i * 20) = 0;
    }
    for (i = 0; i < 32; i++) {
        *(s32*)(st + 1324 + i * 48) = 0;
    }
    for (i = 0; i < 4; i++) {
        *(s32*)(st + 1176 + i * 36) = -1;
    }
    for (i = 0; i < 12; i++) {
        u8* c = st + 24 + i * 32;
        *(s32*)(c + 0) = -1;
        *(s32*)(c + 4) = -1;
        *(s32*)(c + 8) = 0;
        *(s32*)(c + 12) = 0;
        *(s32*)(c + 16) = 0;
        *(s32*)(c + 20) = 127;
        *(s32*)(c + 24) = 0;
        *(s32*)(c + 28) = -1;
    }
}

/* fn_80015CF4: core voice start.  Looks up the bank/sound, allocates a free
 * voice slot and registers it with the driver (update callback =
 * sndFxVoiceUpdateCb).  Returns the 16-bit voice sequence id. */
int sndFxStartVoice(int handle, int soundId, int p2, Vec3* pos, int pan, int flags)
{
    u8* banks = (u8*)sAudioBankTable;
    int bank = soundId >> 16;
    int idx = soundId & 0xFFFFF;
    AudioVoice* voices = AUDIO_VOICES(sAudioState);
    int prevBusy = sAudioQueBusy;
    int seq = 0;
    int slot;
    s16* desc;

    if (sAudioMute != 0 || sAudioReady < 0) {
        if (handle < 0 && pos == 0) {
            sndFxQueAddEx(0, soundId, 0.0f, 1.0f, p2, flags, 0);
        }
        return seq & 0xFFFF;
    }
    desc = (s16*)(*(u8**)(banks + 16) + bank * 44);
    if (desc[21] == 0 || (u16)desc[21] == 0xFFFF) {
        if (desc[21] == 0) {
            ErrorPrintf(sAudioBankNotLoadedMsg,
                        *(u8**)(banks + 16) + bank * 44 + 16,
                        *(u8**)(banks + 20) + (idx + desc[19]) * 28);
            desc[21] = (s16)0xFFFF;
        }
        return 0;
    }
    sAudioQueBusy = 1;
    if (sAudioInitFlag != 0) {
        pan = 127;
    }
    /* find a free voice slot */
    slot = -1;
    {
        int i;
        for (i = 0; i < AUDIO_NUM_VOICES; i++) {
            if (voices[i].flags == 0) {
                slot = i;
                break;
            }
        }
    }
    if (slot >= 0 && sAudioSuspend == 0) {
        int spatial = 0;
        if (pos != 0) {
            spatial = fn_800167EC(pos);
        }
        seq = ++sAudioVoiceSeq;
        if ((u16)seq > 16383) {
            sAudioVoiceSeq = 1;
            seq = 1;
        }
        voices[slot].soundId = soundId;
        voices[slot].flags = flags;
        voices[slot].field8 = p2;
        voices[slot].pos = pos;
        voices[slot].field10 = spatial;
        voices[slot].seq = seq;
        voices[slot].update = (void*)sndFxVoiceUpdateCb;
        voices[slot].self = &voices[slot].soundId;
        if (sndRegisterPair(&voices[slot], 3, &voices[slot].field8,
                            &voices[slot].seq, (s32*)&voices[slot].pad[0],
                            &voices[slot].field2C) <= 0) {
            sAudioErrFlags |= 2;
            voices[slot].flags = 0;
            voices[slot].update = 0;
        }
    }
    if (prevBusy == 0) {
        sAudioQueBusy = 0;
    }
    return seq & 0xFFFF;
}

/* fn_80015F6C: per-voice frame update callback (driver-invoked).  Recomputes
 * spatial parameters for each of the 12 driver channels the voice occupies. */
void sndFxVoiceUpdateCb(AudioVoice* v)
{
    u8* banks = (u8*)sAudioBankTable;
    s32* raw = (s32*)v->soundId; /* driver hands back the voice record */
    int soundId = raw[0];
    int chmask = ((u32)raw[2]) >> 16;
    s16* desc;
    int base;
    int active = 0;
    int ch;
    u8* work = (u8*)&sAudioChanUpdate[0];

    desc = (s16*)(*(u8**)(banks + 16) + (soundId >> 16) * 44);
    base = (soundId & 0xFFFFF) + desc[19];
    *(f32*)(*(u8**)(banks + 20) + base * 28 + 24) = pbLoad - 1.0e12f;

    for (ch = 0; ch < 12; ch++) {
        if (chmask & (1 << ch)) {
            s32* slot = (s32*)(work + ch * 20);
            slot[0] = soundId;
            slot[1] = raw[1];
            *(f32*)&slot[2] = (pbLoad - 1.0e12f)
                + 0.0f * *(f32*)(*(u8**)(banks + 20) + base * 28 + 20);
            slot[3] = raw[3];
            slot[4] = raw[5];
            fn_8001817C(ch, soundId, raw[2], raw[1]);
            active = 1;
            if (raw[5] != 0 && raw[1] == 0) {
                fn_80016720(1 << ch);
            }
        }
    }
    if (!active) {
        fn_8001817C(-1, soundId, raw[2], raw[1]);
    }
    raw[1] = 0;
    raw[5] = 0;
}
