#include "types.h"
#include "game/player.h"

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
 * Not yet decompiled (covered by original bytes; symbols mapped):
 *   InitNameAudio, AudioAmbientUpdate, AudioSecretProc,
 *   AudioSetupLevelStreams
 * AudioMusicVolUpdate is opcode-identical; AudioBuildMusicName is fully
 * translated with a three-instruction pointer/prologue codegen residual. */

/* --- sound-engine callees (game/audio/sndfx.c + audio.c, 0x8001xxxx) --- */
extern f32 sndFxQueAddEx(int a, int b, f32 c, f32 d, int e, int f, int g);
extern void sndFxQueEmpty(void);
extern int sndFxUpdate(int a);
extern void sndFxResetVoices(void);
extern int sndFxPlay3DTracked(int a, int b, int c, int d);
extern int sndFxPlayHandle(int a, int b, int c);
extern void sndFxPlay3D(int a, int b, int c, int d);
extern void AudioSetTrackPan(int a, int b);
extern void AudioSetTrackVolMusic(int a, int b);
extern int AudioMaskByEvent(int a);
extern u32 AudioMaskBySound(int sound);
extern u32 AudioMaskByInstance(u32 instance);
extern int AudioSoundExists(int a);
extern void AudioKillByEvent(int a);
extern void AudioKillBySound(int a);
extern void AudioKillMask(void);
extern int AudioAng(int a);
extern void AudioSetTrackVolSfx(u32 mask, s32 volume);
extern void AudioBankLoadName(char* a, char* b, int c);
extern int AudioStreamPlay();
extern void AudioStreamStop(void);
extern int AudioSysUpdate(int a);
extern void audio_init(void);
extern int AudioFindSound(char* a, int b, int c);
extern int AudioIsActive(void);
extern void AudioDeferSlot(void* cb, int arg);
extern int LevelLetter(int a);
extern void serve_busy(int a);
extern void fn_800C031C(void* a, void* b, void* c, int d);
extern int sprintf(char* buf, const char* fmt, ...);
extern char* strcat(char* dst, const char* src);

/* --- module data --- */
extern s32 lbl_801232C8[]; /* per-player name/track id table, stride 4 */
extern char lbl_801232DC[6][8]; /* material names used by music cues */
extern char lbl_80114A48[]; /* SOUNDS string table */
extern u8 gPlayers[];  /* player array, stride 0x335C */
extern u8 sSpeechNameBuf[];  /* scratch name buffer; aliases per-class speech id tables at offsets */
extern char lbl_80348534[8];  /* "SHOP_%c" fmt (sdata2) */
extern char lbl_80114C9C[];   /* "S_SHOP_%c" fmt (rodata) */
extern u8 lbl_8028BCB8[];
extern u8 lbl_8028BCC0[];
extern u8 lbl_8028BDE8[];
extern u8 lbl_80124458[];
extern s32 sActiveTrackId[]; /* active-track id array (45 entries) */
extern char lbl_801200B0[][4]; /* 4-char class name table */
extern char sStreamNameBuf[];    /* stream-name scratch buffer */
extern u8* gCurLevel;       /* current-level descriptor pointer */
extern s32 good_wiz_state;   /* audio mode (<=2 => attract/menu path) */
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
extern s32 gGameMode;
extern s32 gTriggerCameraState;
extern s32 lbl_803447B4;
extern s32 lbl_803447DC;
extern f32 lbl_80348484;
extern f32 lbl_80348494;
extern f64 lbl_803484A0;
extern f64 lbl_803484A8;
extern f32 lbl_803484B0;
extern f64 lbl_803484B8;
extern f32 lbl_803484C0;
extern f32 lbl_8034851C;
extern f64 lbl_80348520;
extern char lbl_80114C6C[];
extern s32 sumnerSpeechActive(void);
extern void FatalErrorf(const char* format, ...);

/* forward decls */
void AudioWithName(int id, int pidx, f32 vol, int s4, int s5);
void InitNameAudio(void);
void AudioRegisterNameBanks(char* name, int flag);
void AudioSetupLevelStreams(void);
void AudioBuildMusicName(void);

static inline int AudioFloatNotZero(f32 value)
{
    return value != lbl_803484B0;
}

void AudioWelcome(int pidx, int flag)
{
    int extra = lbl_801232C8[pidx];

    if (flag != 0) {
        AudioWithName(0xC0084, pidx, 10.0f, -1, -1);
    } else if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, 0xC0084, -1.0f, 10.0f, 224, extra, 2);
    }
}

void AudioWelcomeBack(int pidx, int flag)
{
    int extra = lbl_801232C8[pidx];

    if (flag != 0) {
        AudioWithName(0xC0083, pidx, 10.0f, -1, -1);
    } else if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, 0xC0083, -1.0f, 10.0f, 224, extra, 2);
    }
}

void AudioWithName(int id, int pidx, f32 vol, int s4, int s5)
{
    Player* player = &((Player*)gPlayers)[pidx];
    s32* T = (s32*)sSpeechNameBuf;
    s32 (*T16)[16] = (s32(*)[16])sSpeechNameBuf;
    int track;
    int flag;
    int f12;
    int cls;
    int a;
    int b;

    flag = player->flags & 0x400;
    f12 = player->character;
    cls = player->class_id;
    track = lbl_801232C8[pidx];
    if (flag) {
        a = -1;
        b = (id != -1) ? 0x1002A : 0x1002B;
    } else if (sCurSelectTrack != 0) {
        a = (id != -1) ? T[cls + 164] : T[cls + 168];
        b = (id != -1) ? T16[cls][f12 + 172] : T16[cls][f12 + 236];
    } else {
        a = (id != -1) ? T[cls + 28] : T[cls + 32];
        b = (id != -1) ? T16[cls][f12 + 36] : T16[cls][f12 + 100];
    }

    if (id >= 0) {
        if (AudioFloatNotZero((good_wiz_state > 2) ? 0.0f
                                : sndFxQueAddEx(1, id, -1.0f, vol, 224, track, 2))) {
            vol = -1.0f;
        } else {
            return;
        }
    }
    if (a >= 0) {
        if (AudioFloatNotZero((good_wiz_state > 2) ? 0.0f
                                : sndFxQueAddEx(1, a, -1.0f, vol, 224, track, 2))) {
            vol = -1.0f;
        } else {
            return;
        }
    }
    if (b >= 0) {
        if (AudioFloatNotZero((good_wiz_state > 2) ? 0.0f
                                : sndFxQueAddEx(1, b, -1.0f, vol, 224, track, 2))) {
            vol = -1.0f;
        } else {
            return;
        }
    }
    if (s4 >= 0) {
        if (AudioFloatNotZero((good_wiz_state > 2) ? 0.0f
                                : sndFxQueAddEx(1, s4, -1.0f, vol, 224, track, 2))) {
            vol = -1.0f;
        } else {
            return;
        }
    }
    if (s5 >= 0 && good_wiz_state <= 2) {
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
    AudioKillMask();
}

void AudioPlayEvt101IfIdle(int arg)
{
    if (AudioSoundExists(101) == 0) {
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
    AudioKillBySound(102);
}

void AudioPlayEvt102Follow(int arg)
{
    int h;

    if (AudioSoundExists(102) == 0) {
        sndFxPlay3DTracked(102, arg, 127, 111);
    }
    h = AudioMaskByEvent(111);
    if (h != 0) {
        AudioSetTrackPan(h, AudioAng(arg));
    }
}

void AudioBuildStreamName(char* name, int flag)
{
    sprintf(sStreamNameBuf, "%s", name);
    strcat(sStreamNameBuf, ".ads");
    AudioStreamPlay(lbl_80343B4C & 0xFFFF, 0, flag);
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
    int h = AudioMaskByEvent(1);

    if (h != 0) {
        AudioSetTrackVolMusic(h, arg);
    }
}

void AudioStopMusicA(void)
{
    AudioKillByEvent(1);
}

void ShopMusicStart(void)
{
    int ch;
    int h;

    AudioKillBySound(0xC0000);
    if (sSelectStreamHandle >= 0) {
        AudioStreamStop();
    }
    sSelectStreamHandle = -1;
    sSelectStreamState = -1;
    ch = LevelLetter(0);
    if ((signed char)ch >= 76) {
        ch = 65;
    }
    sprintf((char*)sSpeechNameBuf, lbl_80348534, (signed char)ch);
    AudioRegisterNameBanks((char*)sSpeechNameBuf, 0);
    sprintf((char*)sSpeechNameBuf, lbl_80114C9C, (signed char)ch);
    h = AudioFindSound((char*)sSpeechNameBuf, -1, 1);
    if (h >= 0) {
        sndFxPlayHandle(h, lbl_80343B4C, 1);
    }
}

void AudioStopMusicB(void)
{
    AudioKillByEvent(1);
}

void MapMusicStart(void)
{
    char* buf;
    int ch;
    int ch2;
    int h;

    buf = (char*)sSpeechNameBuf;
    ch = LevelLetter(0);
    AudioKillBySound(0xC0000);
    if (sSelectStreamHandle >= 0) {
        AudioStreamStop();
    }
    sSelectStreamHandle = -1;
    sSelectStreamState = -1;
    if ((signed char)ch == 83) {
        return;
    }
    if ((signed char)ch == 76) {
        return;
    }
    sprintf(buf, "MAP_%c", (signed char)ch);
    AudioRegisterNameBanks(buf, 0);
    ch2 = LevelLetter(0);
    sprintf(buf, "S_MAP_%c", (signed char)ch2);
    h = AudioFindSound(buf, -1, 1);
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
    /* Keeping the explicit zero case recreates the target's beq/bge/b
     * dispatch even though it shares the default action. */
    switch (track) {
    case 1:
        sndFxPlayHandle(0xC0000, lbl_80343B4C, 1);
        break;
    case 0:
    default:
        AudioKillBySound(0xC0000);
        break;
    }
    sCurSelectTrack = track;
}

void AudioMusicVolUpdate(void)
{
    int target;
    int current;
    int delta;

    if (AudioIsActive() == 0) {
        return;
    }
    if (sSelectStreamHandle < 0) {
        return;
    }
    AudioSetupLevelStreams();
    if (sMusicSubState == 1 && sMusicSubIndex != sSelectStreamState &&
        *(s16*)(*(u8**)(gCurLevel + 100) + 40) > 1) {
        target = sCurMusicVol;
        if (target > 3) {
            target -= 3;
        }
    } else if (sMusicFadeBase < sMusicFadeCur) {
        target = (s32)((f32)lbl_80343B4C * sMusicVolScale);
    } else {
        target = lbl_80343B4C;
    }

    current = sCurMusicVol;
    if (target == current) {
        return;
    }
    delta = target - current;
    if (delta > 8) {
        target = current + 8;
    }
    if (delta < -8) {
        target = current - 8;
    }
    sCurMusicVol = target;
    AudioDeferSlot((void*)(s32)((f32)target * *(f32*)(gCurLevel + 148)), target);
}

void AudioStopSelect(void)
{
    if (sSelectStreamHandle >= 0) {
        AudioStreamStop();
    }
    sSelectStreamHandle = -1;
    sSelectStreamState = -1;
}

void BGMusicStart(void)
{
    int v = (sMusicTrackHi << 8) | (sMusicTrackLo & 0xFF);

    while (sndFxUpdate(1) != 0) {
        serve_busy(-1);
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
        p = &gPlayers[i * 13148];
        st = *(s32*)(p + 232);
        if (st == 0) {
            continue;
        }
        if (st == 2) {
            continue;
        }
        sprintf(nbuf, "PLAYER%d", i + 1);
        AudioBankLoadName(nbuf, lbl_801200B0[*(s32*)(p + 8)], mode);
    }
    AudioBankLoadName("LEVELS", name, mode);
    if (sMusicTrackHi == 13) {
        AudioBankLoadName("VOICE2", "TOWAMB", mode);
    } else {
        AudioBankLoadName("VOICE2", "VOICE2", mode);
    }
    if (flag == 0) {
        while (AudioSysUpdate(1) != 0) {
            serve_busy(-1);
        }
    }
}

extern s32 sAudioMute;
extern s32 lbl_803442F0;
extern s32 lbl_803442F8;
extern char lbl_8012330C[];      /* per-select stream suffix chars */
extern char lbl_80348528[4];     /* "%s" */
extern char lbl_80348570[8];
extern char lbl_80348578[8];
extern char lbl_8034852C[6];     /* ".ads" */
extern char lbl_80348580[4];
extern char lbl_80114CC0[];      /* missing-stream error fmt */
extern char lbl_80114CE0[];      /* stream-play error fmt */
int FileExists(char* mode, char* name);
void ErrorPrintf(const char* fmt, ...);
void sndSysSetBit0(int v);
int sndSysFrameCallback(void);
char* strcat(char* dst, const char* src);

void AudioSetupLevelStreams(void)
{
    char* nb;
    char buf[16];
    u8* lvl;
    u8* lvl2;
    s32 err;
    s32 idx;
    s32 chans;
    s32 mode;
    s32 tmp;

    nb = (char*)sSpeechNameBuf;
    err = 0;
    mode = 0;
    if (sndSysFrameCallback() != 0) {
        if (sMusicSubIndex == sSelectStreamState) {
            return;
        }
        if (sMusicSubState < 1) {
            return;
        }
        if (sMusicSubState < 2 && sCurMusicVol > 3) {
            return;
        }
        if (*(s16*)(*(u8**)(gCurLevel + 100) + 40) <= 1) {
            sMusicSubIndex = 0;
            sMusicSubState = 0;
            return;
        }
    }
    lvl = *(u8**)(gCurLevel + 100);
    if (lvl == NULL) {
        return;
    }
    if (sAudioMute != 0) {
        return;
    }
    idx = sMusicSubIndex;
    if (idx < 0) {
        idx = 0;
    }
    if (idx >= *(s16*)(lvl + 40)) {
        idx = *(s16*)(lvl + 40) - 1;
    }
    if (idx != sSelectStreamState) {
        sMusicField2F4 = 0;
    }
    if (idx != sSelectStreamState || sMusicField2F4 != lbl_803442F8) {
        lbl_803442F0 = 0;
    } else {
        lbl_803442F0 = lbl_803442F0 + 1;
    }
    tmp = sMusicField2F4;
    sSelectStreamState = idx;
    lbl_803442F8 = tmp;
    lvl2 = *(u8**)(gCurLevel + 100);
    chans = *(s16*)(lvl2 + 42);
    if (*(s16*)(lvl2 + 40) == 1) {
        sprintf(buf, lbl_80348528, (char*)(lvl2 + 24));
    } else {
        sprintf(buf, lbl_80348570, (char*)(lvl2 + 24),
                (signed char)lbl_8012330C[sSelectStreamState]);
    }
    lvl2 = *(u8**)(gCurLevel + 100);
    lvl2 += sSelectStreamState * 2;
    if (*(s16*)(lvl2 + 44) > 1) {
        sprintf(nb, lbl_80348578, buf, sMusicField2F4 + 1);
    } else {
        strcpy(nb, buf);
    }
    strcat(nb, lbl_8034852C);
    lvl2 = *(u8**)(gCurLevel + 100);
    if (sMusicField2F4 + 1 ==
        *(s16*)(lvl2 + sSelectStreamState * 2 + 44)) {
        mode = 1;
    }
    if (mode != 0 && *(s16*)(lvl2 + 40) == 1) {
        mode = 2;
    }
    if (FileExists(lbl_80348580, nb) == 0) {
        ErrorPrintf(lbl_80114CC0, nb);
        err = -1;
    } else {
        sprintf(sStreamNameBuf, lbl_80348528, nb);
        tmp = 0;
        sMusicSubState = tmp;
        if (AudioStreamPlay((u16)(s32)((f32)sCurMusicVol *
                                       *(f32*)(gCurLevel + 148)),
                            mode, chans, tmp) < 0) {
            ErrorPrintf(lbl_80114CE0, nb);
            err = -2;
        }
    }
    if (err != 0) {
        AudioStreamStop();
        if (mode != 0) {
            sndSysSetBit0(1);
        } else {
            sMusicField2F4 = sMusicField2F4 + 1;
            sndSysSetBit0(0);
        }
    }
}

typedef struct MusicCuePair {
    s32 rotate;
    s32 stop;
} MusicCuePair;

#pragma opt_propagation off
void AudioBuildMusicName(void)
{
    char* strings = lbl_80114A48;
    char* buf = (char*)sSpeechNameBuf;
    char* material;
    MusicCuePair best = { -1, -1 };
    u32 i;
    int off;

    for (i = 0, off = 0; i < 6; i++, off += 8) {
        s32* cue;
        s32 found;

        material = (char*)lbl_801232DC + off;
        if (material[0] == '*') {
            sprintf(buf, strings + 692, material + 1);
        } else if (*(s32*)(gCurLevel + 68) >= 0) {
            sprintf(buf, strings + 704, material, (s8)LevelLetter(0));
        } else {
            sprintf(buf, strings + 716, material, (s8)LevelLetter(0));
        }
        found = AudioFindSound(buf, -1, 0);
        cue = (s32*)(buf + off);
        cue[16] = found;
        if (*(cue += 16) < 0) {
            continue;
        }

        if (material[0] == '*') {
            sprintf(buf, strings + 728, material + 1);
        } else if (*(s32*)(gCurLevel + 68) >= 0) {
            sprintf(buf, strings + 740, material, (s8)LevelLetter(0));
        } else {
            sprintf(buf, strings + 756, material, (s8)LevelLetter(0));
        }
        {
            s32* stop;
            *(stop = &cue[1]) = AudioFindSound(buf, -1, 0);
            if (best.rotate < 0) {
                s32 rotate = cue[0];
                if (rotate >= 0) {
                    s32 stopValue = *stop;
                    if (stopValue >= 0) {
                        best.rotate = rotate;
                        best.stop = stopValue;
                    }
                }
            }
        }
    }
}
#pragma opt_propagation reset

void AudioSelectReset(void)
{
    sndFxResetVoices();
    sCurSelectTrack = 0;
}

void AudioInit(void)
{
    int i;

    sAudioInitFlag = 0;
    audio_init();
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

/* 0x8009EFA0 - resolve the per-class speech id tables by sound name */
extern char* lbl_80120104[];    /* per-class character name ptrs */
extern char lbl_8034850C[8];    /* "%s_%s"-style fmt A (sdata2) */
extern char lbl_80348514[8];    /* fmt B (sdata2) */
extern char lbl_80114C54[];     /* fmt C (.rodata) */
extern char lbl_80114C60[];     /* fmt D (.rodata) */

typedef struct SpeechNameTables {
    char scratch[112];
    s32 missingA[4];
    s32 missingB[4];
    s32 tableA[4][16];
    s32 tableB[4][16];
    s32 missingC[4];
    s32 missingD[4];
    s32 tableC[4][16];
    s32 tableD[4][16];
} SpeechNameTables;

void InitNameAudio(void)
{
    SpeechNameTables* tables = (SpeechNameTables*)sSpeechNameBuf;

    {
        char (*suf)[4] = lbl_801200B0;
        char** names = lbl_80120104;
        s32 cls;
        s32 i;

        for (cls = 0; cls < 4; cls++) {
            s32* tableA = tables->tableA[cls];
            s32* tableB = tables->tableB[cls];
            for (i = 0; i < 16; i++) {
                sprintf(tables->scratch, lbl_8034850C, names[cls], suf[i]);
                tableA[i] = AudioFindSound(tables->scratch, -1, 1);
                sprintf(tables->scratch, lbl_80348514, names[cls], suf[i]);
                tableB[i] = AudioFindSound(tables->scratch, -1, 1);
            }
            tables->missingA[cls] = -1;
            tables->missingB[cls] = -1;
        }
    }

    {
        char (*suf)[4] = lbl_801200B0;
        char** names = lbl_80120104;
        s32 cls;
        s32 i;

        for (cls = 0; cls < 4; cls++) {
            s32* tableC = tables->tableC[cls];
            s32* tableD = tables->tableD[cls];
            for (i = 0; i < 16; i++) {
                sprintf(tables->scratch, lbl_80114C54, names[cls], suf[i]);
                tableC[i] = AudioFindSound(tables->scratch, -1, 1);
                sprintf(tables->scratch, lbl_80114C60, names[cls], suf[i]);
                tableD[i] = AudioFindSound(tables->scratch, -1, 1);
            }
            tables->missingC[cls] = -1;
            tables->missingD[cls] = -1;
        }
    }
}

/* 0x8009F1AC - drive the underwater/ambient loop (sound 83 / event 113) */
extern u32 lbl_80257630[4];     /* ambient blit ids */
extern f32 lbl_80344B20;        /* ambient fade rate */
extern void mbBlitInit3414(u32 blit, s32 mode);
extern void fn_800552A4(u8* p, f32 rate, f32 v);

void AudioAmbientUpdate(void)
{
    s32 mode;
    s32 idx;
    s32 pi;
    s32 j;
    s32 joff;
    s32 k;
    s32 koff;
    s32 poff;
    s32 t;
    u8* p;
    s32 tr;
    s32 pan;

    mode = 0;
    idx = 0;
    poff = 0;
    for (pi = 0; pi < 4; pi++) {
        p = (u8*)gPlayers + poff;
        if (*(s32*)(p + 232) == 1 && *(s16*)(p + 2400) != 0) {
            j = 0;
            joff = 0;
            for (; j < 4; j++, joff += 4) {
                if (*(u32*)((u8*)lbl_80257630 + joff) != 0) {
                    mbBlitInit3414(*(u32*)((u8*)lbl_80257630 + joff), 0);
                }
            }
            t = 0;
            for (j = 0; j < 11; j++) {
                if (*(u32*)(p + t + 316) & 8) {
                    fn_800552A4((u8*)((u32)gPlayers + poff + t), lbl_80344B20,
                                *(f32*)((u32)gPlayers + poff + t + 304));
                    break;
                }
                t += 16;
            }
            mode = 2;
            break;
        }
        idx++;
        poff += 13148;
    }
    if (mode != 0) {
        if (AudioSoundExists(83) == 0) {
            if (mode == 1) {
                sndFxPlay3DTracked(83, 0, 127, 113);
            } else {
                sndFxPlay3DTracked(83, (s32)((u8*)gPlayers + idx * 13148 + 84),
                                   127, 113);
            }
        }
        tr = AudioMaskByEvent(113);
        if (tr != 0) {
            if (mode == 1) {
                pan = AudioAng(0);
            } else {
                pan = AudioAng((s32)((u8*)gPlayers + idx * 13148 + 84));
            }
            AudioSetTrackPan(tr, pan);
        }
    } else {
        if (sMusicTrackHi != 12) {
            k = 0;
            koff = 0;
            for (; k < 4; k++, koff += 4) {
                if (*(u32*)((u8*)lbl_80257630 + koff) != 0) {
                    mbBlitInit3414(*(u32*)((u8*)lbl_80257630 + koff), 1);
                }
            }
        }
        AudioKillBySound(83);
    }
}

s32 AudioSecretProc(f32 scale, s32 sound, f32* position, u32 flags,
                    s32* instance, s32* mask)
{
    s32 result;
    s32 volume;
    s32 mode;
    f32 scaled;

    if (sound < 0) {
        return 0;
    }

    scaled = lbl_8034851C * scale;
    volume = (s32)(scaled * *(f32*)(gCurLevel + 0x98));
    if (sumnerSpeechActive() != 0 || gTriggerCameraState != 0 ||
        lbl_803447DC != 0) {
        volume = 0x10;
    }
    mode = 0x70;
    if (gGameMode == 0x4014 || gGameMode == 0x400C || lbl_803447B4 != 0) {
        goto stop;
    }

    if (volume < 0) {
        volume = 0;
    }
    if (sMusicTrackHi == 12 && sMusicTrackLo == 0) {
        s32 adjusted;
        volume *= 4;
        if (volume < 0x40) {
            adjusted = 0x40;
        } else if (volume > 0xFF) {
            adjusted = 0xFF;
        } else {
            adjusted = volume;
        }
        volume = adjusted;
    }
    if ((flags & 2) != 0) {
        mode = 3;
    }

    result = 1;
    if (*mask != 0 && (*mask & AudioMaskBySound(sound)) == 0) {
        *mask = 0;
    }
    if (*mask == 0) {
        if (*instance == 0) {
            *instance = sndFxPlay3DTracked(sound, (s32)position, volume, mode) & 0xFFFF;
            result = 3;
        }
        if (*instance != 0) {
            *mask = AudioMaskByInstance(*instance);
            if ((*mask & (*mask - 1)) != 0) {
                FatalErrorf(lbl_80114C6C, sound, *instance, *mask);
            }
            result = 2;
        }
    }
    if (*mask != 0) {
        s32 pan = AudioAng((s32)position);
        AudioSetTrackVolSfx(*mask, volume);
        AudioSetTrackPan(*mask, pan);
    }
    if ((flags & 1) != 0) {
        f64 maximum;
        f64 computed;
        f32 value;
        sMusicFadeCur = sMusicFadeBase + lbl_80348484;
        computed = -(lbl_80348520 * scale - (maximum = lbl_803484A0));
        value = (f32)computed;
        if (value < lbl_803484A8) {
            value = lbl_803484B0;
        } else if (value < lbl_803484B8) {
            value = lbl_803484C0;
        } else if (value > maximum) {
            value = lbl_80348494;
        }
        sMusicVolScale = value;
    }
    return result;

stop:
    *instance = 0;
    *mask = 0;
    AudioKillBySound(sound);
    return 0;
}
