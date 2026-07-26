#include "types.h"

/* Slice of the SOUNDS audio module (Xbox SOUNDS.OBJ) covering the
 * name/speech and music/stream helper functions in 0x800A00A0-0x800A18E8.
 * The full SOUNDS TU spans ~0x8009C2CC-0x800A4870 (shared .sdata2 string
 * pool); this file is the middle slice assigned to this region.
 *
 * Names: AudioWelcome/AudioWelcomeBack/AudioWithName/InitNameAudio/
 * AudioSelect/ShopMusicStart/MapMusicStart/BGMusicStart are real Xbox-PDB
 * names (confident from string+call evidence); the remaining Audio-prefixed
 * names are descriptive (exact Midway identifiers unconfirmed).
 * The sound-engine callees now carry sndFx* names (game/audio/sndfx.c,
 * 0x8001xxxx); helpers still in the unmapped tail keep fn_XXXX externs.
 *
 * NonMatching residuals (parked, structurally correct):
 *   AudioWithName        - prologue register-allocation cascade (ops clean)
 *   ShopMusicStart/Map   - global scratch-buffer address scheduling
 *   AudioSelect          - 1-insn: peephole-off unfolded switch (bge/b)
 * Not yet decompiled (covered by original bytes; symbols mapped):
 *   InitNameAudio, AudioAmbientUpdate, AudioSecretProc,
 *   AudioMusicVolUpdate, AudioSetupLevelStreams, AudioBuildMusicName */

/* --- sound-engine callees (game/audio/sndfx.c, 0x8001xxxx) --- */
extern f32 sndFxQueAddEx(int a, int b, f32 c, f32 d, int e, int f, int g);
extern void sndFxQueEmpty(void);
extern int sndFxUpdate(int a);
extern void sndFxResetVoices(void);
extern void sndFxPlay3DTracked(int a, int b, int c, int d);
extern int sndFxPlayHandle(int a, int b, int c);
extern void sndFxPlay3D(int a, int b, int c, int d);
extern void fn_800160F0(int a, int b);
extern void fn_8001618C(int a, int b);
extern int fn_80016354(int a);
extern int fn_8001640C(int a);
extern void fn_800164EC(int a);
extern void fn_800165A0(int a);
extern void fn_80016720(void);
extern int fn_800167EC(int a);
extern void fn_80016F30(char* a, char* b, int c);
extern void fn_80017500(int a, int b, int c);
extern void fn_800176D8(void);
extern int fn_80017B18(int a);
extern void fn_80017DE4(void);
extern int fn_80018068(char* a, int b, int c);
extern int fn_80057A6C(int a);
extern void fn_80067B0C(int a);
extern void fn_800C031C(void* a, void* b, void* c, int d);
extern int sprintf(char* buf, const char* fmt, ...);
extern char* strcat(char* dst, const char* src);

/* --- module data --- */
extern s32 lbl_801232C8[]; /* per-player name/track id table, stride 4 */
extern u8 lbl_80275AE0[];  /* player array, stride 0x335C */
extern u8 sSpeechNameBuf[];  /* scratch name buffer; aliases per-class speech id tables at offsets */
extern u8 lbl_8028BCB8[];
extern u8 lbl_8028BCC0[];
extern u8 lbl_8028BDE8[];
extern u8 lbl_80124458[];
extern s32 sActiveTrackId[]; /* active-track id array (45 entries) */
extern char lbl_801200B0[][4]; /* 4-char class name table */
extern char sStreamNameBuf[];    /* stream-name scratch buffer */
extern u8* gCurLevel;       /* current-level descriptor pointer */
extern s32 lbl_80344370;   /* audio mode (<=2 => attract/menu path) */
extern s32 sAudioInitFlag;
extern s32 sCurSelectTrack;
extern s32 sCurMusicVol;
extern f32 sMusicVolPrev;
extern s32 sMusicTrackLo;
extern s32 sMusicTrackHi;
extern s32 sMusicSubState;
extern s32 sMusicSubIndex;
extern s32 sMusicField2F4;
extern s32 lbl_80343B4C;
extern s32 sSelectStreamHandle;
extern s32 sSelectStreamState;
extern f32 sMusicVolScale;
extern s32 sMusicSlot0;
extern s32 sMusicSlot1;
extern s32 sMusicSlot2;
extern f32 sMusicFadeBase;
extern f32 sMusicFadeCur;
extern u8 sInputFlag;

/* forward decls */
void AudioWithName(int id, int pidx, f32 vol, int s4, int s5);
void InitNameAudio(void);
void AudioRegisterNameBanks(char* name, int flag);
void AudioSetupLevelStreams(void);
void AudioBuildMusicName(void);

void AudioWelcome(int pidx, int flag)
{
    int extra = lbl_801232C8[pidx];

    if (flag != 0) {
        AudioWithName(0xC0084, pidx, 10.0f, -1, -1);
    } else if (lbl_80344370 <= 2) {
        sndFxQueAddEx(1, 0xC0084, -1.0f, 10.0f, 224, extra, 2);
    }
}

void AudioWelcomeBack(int pidx, int flag)
{
    int extra = lbl_801232C8[pidx];

    if (flag != 0) {
        AudioWithName(0xC0083, pidx, 10.0f, -1, -1);
    } else if (lbl_80344370 <= 2) {
        sndFxQueAddEx(1, 0xC0083, -1.0f, 10.0f, 224, extra, 2);
    }
}

void AudioWithName(int id, int pidx, f32 vol, int s4, int s5)
{
    u8* player = &lbl_80275AE0[pidx * 0x335C];
    s32* T = (s32*)sSpeechNameBuf;
    int track;
    int flag;
    int f12;
    int cls;
    int a;
    int b;

    flag = *(s32*)(player + 292) & 0x400;
    f12 = *(s32*)(player + 12);
    cls = *(s32*)(player + 4);
    track = lbl_801232C8[pidx];
    if (flag) {
        a = -1;
        b = (id != -1) ? 0x1002A : 0x1002B;
    } else if (sCurSelectTrack != 0) {
        a = (id != -1) ? T[cls + 164] : T[cls + 168];
        b = (id != -1) ? T[cls * 16 + f12 + 172] : T[cls * 16 + f12 + 236];
    } else {
        a = (id != -1) ? T[cls + 28] : T[cls + 32];
        b = (id != -1) ? T[cls * 16 + f12 + 36] : T[cls * 16 + f12 + 100];
    }

    if (id >= 0) {
        if (((lbl_80344370 > 2) ? 0.0f
                                : sndFxQueAddEx(1, id, -1.0f, vol, 224, track, 2)) != 0.0f) {
            vol = -1.0f;
        } else {
            return;
        }
    }
    if (a >= 0) {
        if (((lbl_80344370 > 2) ? 0.0f
                                : sndFxQueAddEx(1, a, -1.0f, vol, 224, track, 2)) != 0.0f) {
            vol = -1.0f;
        } else {
            return;
        }
    }
    if (b >= 0) {
        if (((lbl_80344370 > 2) ? 0.0f
                                : sndFxQueAddEx(1, b, -1.0f, vol, 224, track, 2)) != 0.0f) {
            vol = -1.0f;
        } else {
            return;
        }
    }
    if (s4 >= 0) {
        if (((lbl_80344370 > 2) ? 0.0f
                                : sndFxQueAddEx(1, s4, -1.0f, vol, 224, track, 2)) != 0.0f) {
            vol = -1.0f;
        } else {
            return;
        }
    }
    if (s5 >= 0 && lbl_80344370 <= 2) {
        sndFxQueAddEx(1, s5, -1.0f, vol, 224, track, 2);
    }
}

void AudioFootstep(int sel)
{
    int id;

    if (sel == 0) {
        id = 25;
    } else {
        id = (sel & 1) ? 24 : 23;
    }
    sndFxPlay3D(id, 0, 127, 4);
}

void AudioStopAll(void)
{
    fn_80016720();
}

void AudioPlayEvt101IfIdle(int arg)
{
    if (fn_8001640C(101) == 0) {
        sndFxPlay3D(101, arg, 127, 22);
    }
}

void AudioPlayEvt101(int arg)
{
    sndFxPlay3D(101, arg, 127, 22);
}

void AudioPlayEvt104(int arg)
{
    sndFxPlay3D(104, arg, 224, 12);
}

void AudioPlayEvt103(int arg)
{
    sndFxPlay3D(103, arg, 224, 13);
}

void AudioPlayEvt102(void)
{
    fn_800165A0(102);
}

void AudioPlayEvt102Follow(int arg)
{
    int h;

    if (fn_8001640C(102) == 0) {
        sndFxPlay3DTracked(102, arg, 127, 111);
    }
    h = fn_80016354(111);
    if (h != 0) {
        fn_800160F0(h, fn_800167EC(arg));
    }
}

void AudioBuildStreamName(char* name, int flag)
{
    sprintf(sStreamNameBuf, "%s", name);
    strcat(sStreamNameBuf, ".ads");
    fn_80017500(lbl_80343B4C & 0xFFFF, 0, flag);
}

void AudioClampMusicVol(f32 delta, f32 val)
{
    sMusicFadeCur = sMusicFadeBase + delta;

    if (val < 0.0) {
        val = 0.0f;
    } else if (val < 0.2) {
        val = 0.2f;
    } else if (val > 1.0) {
        val = 1.0f;
    }
    sMusicVolScale = val;
}

void AudioSetEvt1(int arg)
{
    int h = fn_80016354(1);

    if (h != 0) {
        fn_8001618C(h, arg);
    }
}

void AudioStopMusicA(void)
{
    fn_800164EC(1);
}

void ShopMusicStart(void)
{
    signed char ch;
    int h;

    fn_800165A0(0xC0000);
    if (sSelectStreamHandle >= 0) {
        fn_800176D8();
    }
    sSelectStreamHandle = -1;
    sSelectStreamState = -1;
    ch = (signed char)fn_80057A6C(0);
    if (ch >= 76) {
        ch = 65;
    }
    sprintf((char*)sSpeechNameBuf, "SHOP_%c", ch);
    AudioRegisterNameBanks((char*)sSpeechNameBuf, 0);
    sprintf((char*)sSpeechNameBuf, "S_SHOP_%c", ch);
    h = fn_80018068((char*)sSpeechNameBuf, -1, 1);
    if (h >= 0) {
        sndFxPlayHandle(h, lbl_80343B4C, 1);
    }
}

void AudioStopMusicB(void)
{
    fn_800164EC(1);
}

void MapMusicStart(void)
{
    char* buf = (char*)sSpeechNameBuf;
    signed char ch = (signed char)fn_80057A6C(0);
    int h;

    fn_800165A0(0xC0000);
    if (sSelectStreamHandle >= 0) {
        fn_800176D8();
    }
    sSelectStreamHandle = -1;
    sSelectStreamState = -1;
    if (ch == 83) {
        return;
    }
    if (ch == 76) {
        return;
    }
    sprintf(buf, "MAP_%c", ch);
    AudioRegisterNameBanks(buf, 0);
    sprintf(buf, "S_MAP_%c", (signed char)fn_80057A6C(0));
    h = fn_80018068(buf, -1, 1);
    if (h >= 0) {
        sndFxPlayHandle(h, lbl_80343B4C, 1);
    }
}

void AudioSelect(int track)
{
    int cur = sCurSelectTrack;

    if (cur == track) {
        return;
    }
    if (cur == 0) {
        AudioRegisterNameBanks("SELECT", 0);
    }
    /* NOTE: target emits an unfolded `beq/bge/b` 3-way for this dispatch
     * (peephole-off single-case switch); MWCC folds our bge -> 1-insn diff. */
    switch (track) {
    case 1:
        sndFxPlayHandle(0xC0000, lbl_80343B4C, 1);
        break;
    default:
        fn_800165A0(0xC0000);
        break;
    }
    sCurSelectTrack = track;
}

void AudioStopSelect(void)
{
    if (sSelectStreamHandle >= 0) {
        fn_800176D8();
    }
    sSelectStreamHandle = -1;
    sSelectStreamState = -1;
}

void BGMusicStart(void)
{
    int v = (sMusicTrackHi << 8) | (sMusicTrackLo & 0xFF);

    while (sndFxUpdate(1) != 0) {
        fn_80067B0C(-1);
    }
    sndFxResetVoices();
    sCurSelectTrack = 0;
    AudioRegisterNameBanks(*(char**)(gCurLevel + 100), 0);
    sSelectStreamHandle = v;
    sCurMusicVol = lbl_80343B4C;
    sSelectStreamState = 0;
    sMusicFadeCur = 0.0f;
    sMusicSubIndex = 0;
    sMusicSubState = 0;
    sMusicVolPrev = 0.0f;
    sMusicField2F4 = 0;
    AudioSetupLevelStreams();
    AudioBuildMusicName();
}

void AudioRegisterNameBanks(char* name, int flag)
{
    char nbuf[12];
    int i;
    int mode = (flag != 0) ? 2 : 1;
    u8* p;
    int st;

    if (flag == 0) {
        sndFxQueEmpty();
    }
    for (i = 0; i < 4; i++) {
        p = &lbl_80275AE0[i * 13148];
        st = *(s32*)(p + 232);
        if (st == 0) {
            continue;
        }
        if (st == 2) {
            continue;
        }
        sprintf(nbuf, "PLAYER%d", i + 1);
        fn_80016F30(nbuf, lbl_801200B0[*(s32*)(p + 8)], mode);
    }
    fn_80016F30("LEVELS", name, mode);
    if (sMusicTrackHi == 13) {
        fn_80016F30("VOICE2", "TOWAMB", mode);
    } else {
        fn_80016F30("VOICE2", "VOICE2", mode);
    }
    if (flag == 0) {
        while (fn_80017B18(1) != 0) {
            fn_80067B0C(-1);
        }
    }
}

void AudioSelectReset(void)
{
    sndFxResetVoices();
    sCurSelectTrack = 0;
}

void AudioInit(void)
{
    int i;

    sAudioInitFlag = 0;
    fn_80017DE4();
    InitNameAudio();
    for (i = 0; i < 45; i++) {
        sActiveTrackId[i] = -1;
    }
    sMusicSlot0 = -1;
    sMusicSlot1 = -1;
    sMusicSlot2 = -1;
}

void AudioClearInputFlag(void)
{
    if (sInputFlag == 0) {
        return;
    }
    sInputFlag = 0;
}

void AudioResetInput(void)
{
    int i;

    sInputFlag = 0;
    for (i = 0; i < 4; i++) {
        lbl_8028BCB8[i] = 0;
    }
}

void AudioRegisterMenu(void)
{
    fn_800C031C(lbl_8028BDE8, lbl_80124458, lbl_8028BCC0, 74);
}
