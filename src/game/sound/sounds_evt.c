#include "types.h"

/* ------------------------------------------------------------------------
 * Front slice of the SOUNDS audio module (Xbox SOUNDS.OBJ), covering the
 * game-event sound-trigger helpers in 0x8009C2CC-0x800A00A0 (~118 fns).
 * The tail name/speech/music slice lives in game/sound/sounds.c (0x800A00A0+);
 * together they are the single SOUNDS TU (shared .sdata2 float-literal pool
 * lbl_80348480..lbl_80348508 = -1,0.5,10,3,4,1,5,... ; shared sndFx callees).
 *
 * NonMatching: dtk supplies the original bytes so the DOL stays byte-exact;
 * the object is compiled only for per-function objdiff comparison.
 *
 * Each function is a thin wrapper that fires one/few sound events through the
 * sndFx* engine (game/audio/sndfx.c) or queues announcer voice via
 * sndFxQueAddEx (gated on good_wiz_state<=2).  Precise per-event Xbox PDB
 * names (AudioRotator/AudioElevator/AudioEnemyDies/...) could NOT be pinned
 * 1:1: no sound-id->name table exists and GC function order != Xbox order.
 * They are therefore left as fn_ pending an id table; the caller-domain and
 * play-primitive of every function are recorded in the scout report.
 *
 * STATUS (matching pass): 116/118 functions reconstructed; 102 byte-exact
 * after recovering the C8F0 call-argument temporaries, the D100/D16C/D1D8
 * one-case switches, the EEBC/EF04 argument locals, and SeverePain's
 * assignment-in-condition. Remaining residuals are semantically faithful;
 * F550/F860 now have exact instruction counts, and FD84 has a substantially
 * closer control-flow and argument-loading shape.
 * Deferred (too large for this light-touch pass, need dedicated sessions):
 *   fn_8009CB44           0x8009CB44 (0x23C) two-half lbl_8034476C<=1 range
 *                         dispatch over shared AudioWithName/QueAddEx bodies
 *   AudioSetupBossStreams 0x8009E108 (0xCE8) boss/wizard music+speech stream
 *                         name builder (sprintf/strcat, gBossType dispatch)
 * ---------------------------------------------------------------------- */

/* --- sound-engine callees (game/audio/sndfx.c + audio.c, 0x8001xxxx) --- */
extern f32 sndFxQueAddEx(int mode, int soundId, f32 vol, f32 param, int pri, int track, int flags);
extern f32 sndFxQueAdd(int soundId, f32 vol, f32 param, int pri, int track, int flags);
extern void sndFxPlay3D(int soundId, int pos, int p2, int flags);
extern void sndFxPlay3DAtten(int soundId, int pos, int p2, int flags);
extern void sndFxPlay3DTracked(int soundId, int pos, int p2, int flags);
extern int sndFxPlayEx(int soundId, int p1, int pan, int flags);
extern int sndFxPlayHandle(int a, int b, int c);
extern int AudioWithName(int id, int pidx, f32 vol, int s4, int s5);
extern void AudioKillBySound(int soundId);
extern f32 AudioGetSoundVol(int a);
extern int AudioSoundExists(int a);
extern int AudioMaskByEvent(int a);
extern void AudioKillMask(void);
extern int AudioAng(int a);
extern void AudioSetTrackPan(int a, int b);
extern int RandInt(int a);

/* --- module data --- */
extern s32 lbl_801232C8[]; /* per-player name/track id table + sibling id rows */
extern s32 lbl_8012406C[]; /* sound-id table indexed by sMusicTrackHi */
extern s32 lbl_80123454[]; /* sound-id table indexed by lbl_803448B4 */
extern s32 lbl_803448B4;   /* SDA index into lbl_80123454 */
extern u8 sSpeechNameBuf[]; /* speech scratch buffer; aliases id tables at offsets */
extern s32 lbl_8012348C[]; /* sound-id table indexed by sel-2 */
extern s32 lbl_801234E4[][4]; /* 2D sound-id table [player field8][rand], stride 16 */
extern s32 lbl_8012341C[]; /* sound-id table indexed by sMusicTrackHi */
extern s32 lbl_801234B8[]; /* sound-id table indexed by sel */
extern s32 lbl_80123564[]; /* sound-id table indexed by player field8 */
extern s32 lbl_80123624[]; /* sound-id table indexed by player field8 */
extern s32 lbl_80123644[]; /* sound-id table indexed by player field8 */
extern s32 lbl_801236A4[]; /* sound-id table indexed by player field8 */
extern s32 lbl_8012382C[][7]; /* 2D sound-id table [sMusicTrackHi][idx], stride 28 */
extern s32 lbl_801237BC[][4]; /* 2D sound-id table [row][pidx], stride 16 */
extern s32 lbl_8012380C[]; /* sound-id table indexed by player field8 */
extern s32 lbl_80123784[]; /* sound-id table indexed by sMusicTrackHi */
extern s32 lbl_801239B4[][5]; /* 2D sound-id table [row][col], stride 20 */
extern s32 lbl_801239DC[][5]; /* 2D sound-id table [row][col], stride 20 */
extern s32 lbl_80123A04[]; /* sound-id table indexed by sMusicTrackHi */
extern s32 lbl_80123A3C[]; /* sound-id table indexed by sMusicTrackHi */
extern s32 lbl_80123A74[]; /* sound-id table indexed by sMusicTrackHi */
extern s32 lbl_80123AAC[]; /* sound-id table indexed by sMusicTrackHi */
extern s32 lbl_80123AE4[]; /* sound-id table indexed by sMusicTrackHi */
extern s32 lbl_80123B1C[]; /* sound-id table indexed by sMusicTrackHi */
extern s32 lbl_80123B54[]; /* sound-id table indexed by sMusicTrackHi */
extern s32 lbl_80123B8C[][14]; /* 2D sound-id table [row][sMusicTrackHi], stride 56 */
extern s32 lbl_80123D7C[][9]; /* 2D speech-id table [arg2][val/10-1], stride 36 */
extern s32 lbl_80123BFC[]; /* sound-id table indexed by sMusicTrackHi */
extern s32 lbl_80123C34[]; /* sound-id table indexed by sMusicTrackHi */
extern s32 lbl_80123C6C[][4]; /* 2D sound-id table [sMusicTrackHi][col] */
extern s32 lbl_80124114[]; /* sound-id table indexed by sMusicTrackHi */
extern s32 lbl_80124148[]; /* sound-id table indexed by sMusicTrackHi */
extern s32 lbl_8012417C[][8]; /* 2D sound-id table [row][col], row stride 8 */
extern s32 lbl_801240A4[]; /* sound-id table indexed by sMusicTrackHi */
extern s32 lbl_801240DC[]; /* sound-id table indexed by sMusicTrackHi */
extern s32 lbl_801242DC[]; /* sound-id lookup table */
extern s32 lbl_80124300[]; /* sound-id lookup table */
extern s32 lbl_80124324[]; /* sound-id lookup table */
extern s32 lbl_80124330[]; /* announcer-voice id rotation table B */
extern s32 lbl_80124340[]; /* announcer-voice id rotation table A */
extern s32 lbl_8028B610[][2]; /* [idx] -> {id1, id2} event-follow pairs */
extern u8 gPlayers[];  /* player array, stride 0x335C (13148); pos at +84 */
extern s32 sVoiceRotIdxA;  /* 0..3 announcer-voice rotation counter */
extern s32 sVoiceRotIdxB;  /* 0..3 announcer-voice rotation counter */
extern s32 good_wiz_state; /* <=2 => attract/menu path (announcer allowed) */
extern s32 lbl_803447B4;   /* gate flag */
extern s32 lbl_803447B8;   /* gate flag */
extern s32 sMusicTrackHi;
extern s32 sMusicTrackLo;
extern s32 sMusicSlot0;
extern s32 sMusicSlot1;
extern s32 sMusicSlot2;
extern s32 sActiveTrackId[]; /* active-track id array (45 entries) */
extern s32 lbl_80343E24; /* SDA sound-id table base indexed by sel */
extern s32 lbl_80343E2C; /* SDA sound-id table base indexed by sel */
extern s32 lbl_80343E34; /* SDA sound-id table base indexed by rotation counter */
extern s32 sAudioOverride; /* audio override flag (saved/restored) */
extern s32 lbl_80344C30;   /* 0..1 rotation counter */
extern f32 sMusicFadeBase;
extern f32 sMusicFadeCur;
extern f32 sMusicVolPrev;
extern f32 lbl_8034832C; /* player-slot empty threshold */
extern f32 sMusicVolScale;
extern s32 gBossType;
extern u8* gCurLevel;
extern u8* gWorldData;

/* ----------------------------------------------------------------- */

int AudioFindPlayerSlot(int pidx, int class_, int type)
{
    u8* slot = &gPlayers[pidx * 13148] + 304;
    int i;

    for (i = 0; i < 11; i++) {
        if (*(f32*)slot <= lbl_8034832C) {
            continue;
        }
        if (*(int*)(slot + 12) == type && *(int*)(slot + 4) == class_) {
            return i;
        }
        slot += 16;
    }
    return -1;
}

void AudioPlay3DSel(int soundId, int p2, int pos, int sel)
{
    if (sel != 0) {
        sndFxPlay3DAtten(soundId, pos, p2, 15);
    } else {
        sndFxPlay3D(soundId, pos, p2, 15);
    }
}

void fn_8009C378(void)
{
    int i = sVoiceRotIdxA;
    int id = lbl_80124340[i];

    i++;
    sVoiceRotIdxA = i;
    if (i >= 4) {
        sVoiceRotIdxA = 0;
    }
    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, id, -1.0f, 0.5f, 224, 127, 2);
    }
}

void fn_8009C3EC(void)
{
    int i = sVoiceRotIdxB;
    int id = lbl_80124330[i];

    i++;
    sVoiceRotIdxB = i;
    if (i >= 4) {
        sVoiceRotIdxB = 0;
    }
    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, id, -1.0f, 0.5f, 224, 127, 2);
    }
}

void fn_8009C460(int sel)
{
    int id = -1;

    switch (sel) {
    case 1:  id = 0xE00A3; break;
    case 2:  id = 22; break;
    case 10: id = 0xE00AA; break;
    case 14: id = 0xE00AB; break;
    case 21: id = 0xE00A9; break;
    }
    if (id >= 0) {
        sndFxPlay3D(id, 0, 255, 10);
    }
}

int fn_8009C4F0(int sel)
{
    int id = -1;

    switch (sel) {
    case 0:  id = 0xE00A4; break;
    case 13: id = 0xE00A8; break;
    case 36: id = 0xE00A7; break;
    case 25: id = 0xE00A5; break;
    case 26: id = 0xE00A6; break;
    }
    if (id >= 0) {
        sndFxQueAddEx(1, id, -1.0f, 10.0f, 224, 127, 2);
    }
    return id;
}

int fn_8009C5B8(int idx)
{
    int id = lbl_80124324[idx];

    if (id >= 0) {
        sndFxQueAddEx(1, id, -1.0f, 10.0f, 224, 127, 2);
    }
    return id;
}

int fn_8009C620(int idx)
{
    int id = lbl_80124300[idx];

    if (id >= 0) {
        sndFxQueAddEx(1, id, -1.0f, 10.0f, 224, 127, 2);
    }
    return id;
}

void fn_8009C688(int idx)
{
    int id = -1;

    if (idx < 9) {
        id = lbl_801242DC[idx];
    } else if (idx == 15) {
        id = 0xE00AC;
    } else if (idx == 16) {
        id = 0x3000B;
    }
    if (id >= 0) {
        sndFxQueAddEx(1, id, -1.0f, 10.0f, 224, 127, 2);
    }
}

void fn_8009C710(int a, int b)
{
    int row = a - 34;
    int id = lbl_8012417C[row][b];

    if (id >= 0) {
        sndFxQueAddEx(1, id, -1.0f, 10.0f, 224, 127, 2);
    }
}

void fn_8009C774(int pos, int sel)
{
    int mt = sMusicTrackHi;
    int id = lbl_80124148[mt];

    if (mt == 10 && sel == 24) {
        id = 0x2C0024;
    }
    sndFxPlay3DAtten(id, pos, 180, 91);
}

void fn_8009C7D8(int pos, int sel)
{
    int mt = sMusicTrackHi;
    int id = lbl_80124114[mt];

    if (mt == 10 && sel == 24) {
        id = 0x2C0025;
    }
    if (gBossType < 0 && id >= 0) {
        sndFxPlay3DAtten(id, pos, 127, 81);
    }
}

void fn_8009C850(int pos)
{
    int id = lbl_801240DC[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 127, 100);
    }
}

void fn_8009C8A0(int pos)
{
    int id = lbl_801240A4[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 127, 101);
    }
}

void fn_8009C8F0(int pos, int flag)
{
    if (flag > 0) {
        int idx = *(s16*)(*(u8**)(gCurLevel + 100) + 18);
        if (idx >= 0) {
            u8* e = *(u8**)(gWorldData + 44) + idx * 24;
            if (*(s32*)(e + 16) >= 0) {
                int atten = *(s16*)(e + 20) != 0 ? *(s16*)(e + 20) : 224;
                int priority = *(s16*)(e + 22) != 0 ? *(s16*)(e + 22) : 126;

                sndFxPlay3DAtten(*(s32*)(e + 16), pos, atten, priority);
            }
        }
    } else {
        sndFxPlay3DAtten(46, pos, 127, 24);
    }
}

void fn_8009C98C(int pos)
{
    int id = lbl_8012406C[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 127, 68);
    }
}

void fn_8009C9DC(int sel, int pos)
{
    s32* t = lbl_801232C8;
    int k = sMusicTrackHi - 1;

    switch (sel) {
    case 0:
        sndFxPlay3D(100, pos, 224, 10);
        break;
    case 1:
        sndFxPlay3D(t[k + 829], pos, 224, 10);
        break;
    case 2:
        sndFxPlay3D(t[k + 840], pos, 224, 10);
        break;
    case 3:
        AudioKillBySound(t[k + 840]);
        sndFxPlay3D(t[k + 851], pos, 224, 10);
        break;
    case 4:
        sndFxPlay3D(t[k + 862], pos, 224, 10);
        break;
    }
}

void fn_8009D078(int pos)
{
    sndFxPlay3DAtten(44, pos, 127, 64);
}

void fn_8009D0A8(int pos, int col)
{
    int id = lbl_80123C6C[sMusicTrackHi][col];

    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 127, 64);
    }
}

void fn_8009D258(int pos)
{
    sndFxPlay3D(4, pos, 127, 9);
}

void fn_8009D288(void)
{
    sndFxPlayHandle(2, 224, 9);
}

void fn_8009D2B4(void)
{
    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, 0x2000D, -1.0f, 10.0f, 224, 127, 2);
    }
}

void fn_8009D300(void)
{
    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, 0x10006, -1.0f, 10.0f, 224, 127, 2);
    }
}

void fn_8009D34C(void)
{
}

void fn_8009D350(void)
{
    sndFxPlayHandle(12, 224, 4);
}

void fn_8009D37C(void)
{
    sndFxPlayHandle(19, 224, 3);
}

void AudioMenuExit(void)
{
    sndFxPlayHandle(16, 127, 3);
}

void AudioCursorSelect(void)
{
    sndFxPlayHandle(17, 127, 3);
}

void AudioCursorChar(void)
{
    sndFxPlayHandle(15, 127, 3);
}

void AudioCursorH(void)
{
    sndFxPlayHandle(14, 127, 3);
}

void AudioCursorV(void)
{
    sndFxPlayHandle(13, 127, 3);
}

void AudioBuzzer(void)
{
    sndFxPlayHandle(10, 127, 48);
}

void fn_8009D4B0(int pidx)
{
    sndFxPlay3D(80, (int)(&gPlayers[pidx * 13148] + 84), 224, 40);
}

void fn_8009D4F0(int pidx)
{
    sndFxPlay3D(89, (int)(&gPlayers[pidx * 13148] + 84), 224, 40);
}

void fn_8009D530(void)
{
    sndFxPlayEx(85, 127, 224, 40);
}

void fn_8009D560(int pidx)
{
    sndFxPlay3D(87, (int)(&gPlayers[pidx * 13148] + 84), 224, 40);
}

void fn_8009D5A0(int pidx)
{
    sndFxPlay3D(99, (int)(&gPlayers[pidx * 13148] + 84), 224, 40);
}

void fn_8009D5E0(int pos)
{
    sndFxPlay3DAtten(36, pos, 180, 120);
}

void fn_8009D610(int flag, int pos)
{
    int h;

    if (flag == 0) {
        if (AudioSoundExists(3) == 0) {
            sndFxPlay3DTracked(3, pos, 224, 118);
        }
        h = AudioMaskByEvent(118);
        if (h != 0) {
            AudioSetTrackPan(h, AudioAng(pos));
        }
    } else {
        AudioKillBySound(3);
    }
}

int fn_8009D694(int a, int pos, int idx)
{
    int id1;
    int ret = 0;
    int id2;

    if (a < 0) {
        if (AudioMaskByEvent(115) != 0) {
            AudioKillMask();
        }
        return 0;
    }
    if (idx < 0 || (u32)idx >= 6) {
        return 0;
    }
    id1 = lbl_8028B610[idx][0];
    id2 = lbl_8028B610[idx][1];
    if (id1 < 0 || id2 < 0) {
        return 0;
    }
    if (lbl_803447B8 != 0 || lbl_803447B4 != 0) {
        a = 0;
    }
    if (a == 0) {
        if (AudioSoundExists(id1) != 0) {
            ret = 1;
        } else {
            sndFxPlay3DTracked(id1, pos, 224, 115);
            ret = 2;
        }
        id1 = AudioMaskByEvent(115);
        if (id1 != 0) {
            AudioSetTrackPan(id1, AudioAng(pos));
        }
    } else {
        if (AudioSoundExists(id1) != 0) {
            AudioKillBySound(id1);
            if (a >= 2) {
                sndFxPlay3D(id2, pos, 224, 68);
            }
        }
    }
    return ret;
}

void fn_8009D8CC(int pos)
{
    int id = lbl_80123AE4[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 224, 18);
    }
}

void fn_8009D91C(int pos)
{
    int mt = sMusicTrackHi;
    int id = lbl_80123AAC[mt];

    if (mt == 6 && sMusicTrackLo == 1) {
        id = 0x390000;
    } else if (mt == 9 && sMusicTrackLo == 4) {
        id = 0x32000D;
    }
    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 224, 18);
    }
}

void fn_8009D9A4(int pos)
{
    sndFxPlay3DAtten(0x250001, pos, 224, 50);
}

void fn_8009D9D8(int pos)
{
    int id = lbl_80123A74[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 224, 85);
    }
}

void fn_8009DA28(int pos)
{
    int id = lbl_80123A3C[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 224, 85);
    }
}

void fn_8009DA78(int pos)
{
    int id = lbl_80123A04[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 224, 85);
    }
}

void fn_8009DAC8(int pos)
{
    sndFxPlay3DAtten(56, pos, 224, 50);
}

void fn_8009DAF8(void)
{
    sndFxPlayHandle(45, 224, 20);
}

void fn_8009DCB4(int pos)
{
    sndFxPlay3D(58, pos, 180, 20);
}

void fn_8009DCE4(int pos)
{
    int mt = sMusicTrackHi;
    int id = 0x260027;

    if (mt == 3) {
        id = 0x270032;
    }
    if (mt == 4) {
        id = 0x280000;
    }
    if (mt == 6) {
        id = 0x38001F;
    }
    sndFxPlay3DAtten(id, pos, 127, 50);
}

void fn_8009DD48(void)
{
    AudioKillBySound(55);
}

void fn_8009DD6C(int pos)
{
    sndFxPlay3DAtten(55, pos, 224, 52);
}

void fn_8009DD9C(int pos)
{
    sndFxPlay3DAtten(sMusicSlot2, pos, 224, 12);
}

void fn_8009DDCC(int pos)
{
    sndFxPlay3D(sMusicSlot1, pos, 224, 13);
}

void fn_8009DDFC(int pos)
{
    sndFxPlay3DAtten(50, pos, 127, 126);
}

void fn_8009DE2C(int pos)
{
    sndFxPlay3DAtten(48, pos, 127, 126);
}

void fn_8009DE5C(int a, int pos)
{
    sndFxPlay3DAtten(60, pos, 127, 120);
}

void fn_8009E03C(int pos)
{
    switch (*(int*)pos) {
    case 29:
    case 32:
        sndFxPlay3DAtten(sMusicSlot0, pos + 52, 127, 28);
        break;
    }
}

void fn_8009DB24(int sel, int arg)
{
    s32* t = lbl_801232C8;
    int soundId;
    int pan;
    int flags;

    pan = 224;
    flags = 126;
    soundId = -1;
    switch (sel) {
    case 1:
        soundId = 0x30000B;
        flags = 14;
        break;
    case 2:
        soundId = 57;
        flags = 54;
        break;
    case 3:
        soundId = 0x30000D;
        flags = 14;
        break;
    case 5: {
        int idx = *(s16*)(*(u8**)(gCurLevel + 100) + 18);

        if (idx >= 0) {
            u8* e = *(u8**)(gWorldData + 44) + idx * 24;

            if (*(s32*)(e + 16) >= 0) {
                sndFxPlay3DAtten(*(s32*)(e + 16), arg,
                                 *(s16*)(e + 20) != 0 ? *(s16*)(e + 20) : 224,
                                 *(s16*)(e + 22) != 0 ? *(s16*)(e + 22) : 126);
            }
        }
        break;
    }
    case 6:
        soundId = 63;
        pan = 127;
        flags = 40;
        break;
    case 7:
        soundId = 64;
        pan = 127;
        flags = 40;
        break;
    case 4:
        soundId = 56;
        flags = 50;
        break;
    case 8:
        soundId = t[444];
        pan = 127;
        flags = 15;
        break;
    case 9:
        soundId = t[445];
        pan = 127;
        flags = 15;
        break;
    case 10:
        soundId = t[446];
        pan = 127;
        flags = 15;
        break;
    case 11:
        soundId = t[447];
        pan = 127;
        flags = 15;
        break;
    case 12:
        soundId = 0x310011;
        pan = 127;
        flags = 14;
        break;
    case 0:
    default:
        soundId = sel;
        break;
    }
    if (soundId >= 0) {
        sndFxPlay3DAtten(soundId, arg, pan, flags);
    }
}

void fn_8009DE88(int p, int mode)
{
    s32* T = (s32*)sSpeechNameBuf;
    int id0 = T[*(int*)p + 300];
    int id;

    if (id0 >= 0) {
        if (mode == 1) {
            if (*(s16*)(p + 518) >= 2) {
                if (*(s16*)(p + 516) <= 1) {
                    id = T[id0 + 393];
                } else {
                    id = T[id0 + 401];
                }
            } else {
                id = T[id0 + 377];
            }
            sndFxPlay3DAtten(id, p + 52, 224, 35);
        } else {
            if (*(s16*)(p + 518) >= 2) {
                if (*(s16*)(p + 516) <= 1) {
                    id = T[id0 + 409];
                } else {
                    id = T[id0 + 417];
                }
            } else {
                id = T[id0 + 385];
            }
            sndFxPlay3DAtten(id, p + 52, 224, 95);
        }
    }
}

void fn_8009DF7C(int p, int mode)
{
    s32* T = (s32*)sSpeechNameBuf;
    int id0 = T[*(int*)p + 300];
    int id;

    if (id0 >= 0) {
        if (mode == 1) {
            if (*(s16*)(p + 518) >= 2) {
                id = T[id0 + 353];
            } else {
                id = T[id0 + 345];
            }
            sndFxPlay3DAtten(id, p + 52, 224, 25);
        } else {
            if (*(s16*)(p + 518) >= 2) {
                id = T[id0 + 369];
            } else {
                id = T[id0 + 361];
            }
            sndFxPlay3DAtten(id, p + 52, 224, 85);
        }
    }
}

void fn_8009E08C(int p)
{
    s32* T = (s32*)sSpeechNameBuf;
    int id0 = T[*(int*)p + 300];
    int id;

    if (id0 >= 0) {
        if (*(s16*)(p + 518) >= 2) {
            id = T[id0 + 433];
        } else {
            id = T[id0 + 425];
        }
        sndFxPlay3DAtten(id, p + 52, 180, 75);
    }
}

void fn_8009FCA8(int flag)
{
    int id = lbl_80123454[lbl_803448B4];

    if (id >= 0) {
        if (flag != 0) {
            if (AudioSoundExists(id) == 0) {
                sndFxPlay3DTracked(id, 0, 224, 123);
            }
        } else {
            if (AudioSoundExists(id) != 0) {
                AudioKillBySound(id);
            }
        }
    }
}

void AudioClearActiveTracks(void)
{
    int i;

    for (i = 0; i < 45; i++) {
        sActiveTrackId[i] = -1;
    }
    sMusicSlot0 = -1;
    sMusicSlot1 = -1;
    sMusicSlot2 = -1;
}

void fn_8009EE2C(int flag)
{
    int save = sAudioOverride;

    sAudioOverride = 1;
    if (flag != 0) {
        sndFxPlayEx(20, 127, 127, 1);
    } else {
        u32 c;

        sndFxPlayEx((&lbl_80343E34)[lbl_80344C30], 127, 127, 1);
        c = lbl_80344C30 + 1;
        lbl_80344C30 = c;
        if (c >= 2) {
            lbl_80344C30 = 0;
        }
    }
    sAudioOverride = save;
}

void fn_8009EEBC(int pidx, int sel)
{
    int track = lbl_801232C8[pidx];
    int id = (&lbl_80343E2C)[sel];

    sndFxPlayEx(id, track, 127, 125);
}

void fn_8009EF04(int pidx, int sel)
{
    int track = lbl_801232C8[pidx];
    int id = (&lbl_80343E24)[sel];

    sndFxPlayEx(id, track, 127, 125);
}

void fn_8009EF4C(int pos)
{
    sndFxPlay3DAtten(60, pos, 127, 120);
}

void AudioPlayerXray(int pidx)
{
    sndFxPlay3D(81, (int)(&gPlayers[pidx * 13148] + 84), 127, 60);
}

void fn_8009F158(int pidx)
{
    sndFxPlay3D(82, (int)(&gPlayers[pidx * 13148] + 84), 224, 60);
}

void fn_8009F390(int pidx)
{
    sndFxPlay3D(5, (int)(&gPlayers[pidx * 13148] + 100), 127, 19);
}

void fn_8009F3D0(int pidx)
{
    sndFxPlay3D(93, (int)(&gPlayers[pidx * 13148] + 84), 224, 40);
}

void fn_8009F410(int pidx)
{
    sndFxPlay3D(92, (int)(&gPlayers[pidx * 13148] + 84), 224, 40);
}

void fn_8009F450(int pidx)
{
    sndFxPlay3D(91, (int)(&gPlayers[pidx * 13148] + 84), 224, 40);
}

void fn_8009F490(int pidx)
{
    sndFxPlay3D(90, (int)(&gPlayers[pidx * 13148] + 84), 224, 40);
}

void fn_8009EF7C(int a, int pos)
{
    if (sMusicFadeBase >= 1.0 + sMusicVolPrev) {
        sndFxPlay3DAtten(65, pos, 127, 40);
        sMusicVolPrev = sMusicFadeBase;
    }
}

void fn_8009EFCC(int pidx, int a, int b)
{
    int id = lbl_801239DC[a][b];

    sndFxPlay3DAtten(id, (int)(&gPlayers[pidx * 13148] + 68), 127, 125);
}

void AudioPotion(int a, int pos, int c)
{
    sndFxPlay3D(lbl_801239B4[c][a], pos, 127, 15);
}

void fn_8009F340(int pos)
{
    int id = lbl_80123784[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlay3D(id, pos, 224, 6);
    }
}

void AudioPlayerDies(int pidx)
{
    u8* player = &gPlayers[pidx * 13148];

    if (*(int*)(player + 232) == 1) {
        int id;

        sndFxPlay3D(1, (int)(player + 68), 127, 8);
        id = lbl_8012380C[*(int*)(player + 8)];
        if (*(int*)(player + 292) & 0x400) {
            id = 98;
        }
        sndFxPlay3D(id, (int)(player + 68), 224, 7);
    }
}

void AudioPlayerHit(int pidx, int a)
{
    u8* player = &gPlayers[pidx * 13148];

    if (*(int*)(player + 232) == 1) {
        sndFxPlay3D(lbl_801237BC[a][pidx], (int)(player + 68), 127, 74);
    }
}

void fn_8009FA84(void)
{
    sndFxPlayHandle(0xC0085, 224, 20);
}

void fn_8009FAB4(void)
{
    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, 0x10029, -1.0f, 1.0f, 224, 127, 2);
    }
}

void fn_8009FB00(void)
{
    sndFxPlayHandle(0x1005A, 224, 20);
}

int fn_8009FB30(void)
{
    int id = 0x3B0025;

    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, id, -1.0f, 1.0f, 224, 127, 2);
    }
    return id;
}

void fn_8009FB84(int sel)
{
    if (sel >= 0 && sel <= 10) {
        sndFxPlayHandle(lbl_801234B8[sel], 224, sel + 21);
    }
}

void fn_8009FD38(void)
{
    int id = lbl_8012341C[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlayHandle(id, 224, 30);
    }
}

void fn_8009FD84(void)
{
    u8* level = *(u8**)(gCurLevel + 100);
    int idx = *(s16*)(level + 16);
    u8* entry;

    if (idx >= 0) {
        entry = *(u8**)(gWorldData + 44) + idx * 24;
        if (*(int*)(entry + 16) >= 0) {
            goto entry_ready;
        }
    }
    entry = 0;
entry_ready:
    if (entry != 0) {
        if (*(int*)(level + 20) >= 0) {
            int sound_id = *(int*)(entry + 16);

            if (good_wiz_state <= 2) {
                sndFxQueAddEx(1, sound_id, -1.0f, -1.0f, 224, 127, 2);
            }
            {
                int next_id = *(int*)(*(u8**)(gCurLevel + 100) + 20);

                if (good_wiz_state <= 2) {
                    sndFxQueAddEx(1, next_id, -1.0f, -1.0f, 224, 127, 2);
                }
            }
        }
    }
}

void AudioPlayerBreath(int pidx)
{
    AudioWithName(-1, pidx, 5.0f, sMusicTrackHi == 13 ? 0x30014 : 0x2000C, -1);
}

void fn_8009FEA0(int pidx)
{
    int track = lbl_801232C8[pidx];

    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, 0x10001, -1.0f, 3.0f, 224, track, 2);
    }
}

void fn_8009FEFC(int pidx)
{
    int track = lbl_801232C8[pidx];

    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, 0x10000, -1.0f, 3.0f, 224, track, 2);
    }
}

void fn_8009FF54(int pos)
{
    int track = AudioAng(pos);

    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, 0x20033, -1.0f, 3.0f, 224, track, 2);
    }
}

void fn_8009FFA4(int pos)
{
    int track = AudioAng(pos);

    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, 0x20034, -1.0f, 3.0f, 224, track, 2);
    }
}

void fn_8009F06C(int pos, int idx)
{
    if (idx < 7) {
        int id = lbl_8012382C[sMusicTrackHi][idx];

        if (id == 0x260027 && gBossType == 34) {
            id = 0x2E0009;
        }
        if (id >= 0) {
            if (id == 0x250002 || id == 0x28002E) {
                sndFxPlay3D(id, pos, 180, 50);
            } else {
                sndFxPlay3D(id, pos, 127, 50);
            }
        }
    }
}

void fn_8009F550(int pidx, int sel, int arg3)
{
    typedef struct AudioPlayerEventIds {
        u8 pad_0000[1020];
        s32 case_0[8][4];
        s32 case_1[8];
        s32 case_2[8];
    } AudioPlayerEventIds;
    AudioPlayerEventIds* t = (AudioPlayerEventIds*)lbl_801232C8;
    int f8;
    int slot;
    int flags;

    flags = *(int*)&gPlayers[pidx * 13148 + 292];
    f8 = *(int*)&gPlayers[pidx * 13148 + 8];
    slot = (int)&gPlayers[pidx * 13148 + 68];

    if (flags & 0x400) {
        sndFxPlay3D(95, slot, 224, 16);
    } else {
        switch (sel) {
        case 0:
            sndFxPlay3D(t->case_0[f8][arg3], slot, 224, 19);
            break;
        case 1:
            sndFxPlay3D(t->case_1[f8], slot, 224, 17);
            break;
        case 2:
            sndFxPlay3D(t->case_2[f8], slot, 224, 16);
            break;
        }
    }
}

void fn_8009F860(int pidx, int arg2)
{
    typedef struct AudioEventIds {
        u8 pad_0000[700];
        s32 random_pain[8][4];
        s32 severe_pain[1];
    } AudioEventIds;
    u8* player = &gPlayers[pidx * 13148];
    AudioEventIds* t = (AudioEventIds*)lbl_801232C8;

    if (RandInt(4) == 0) {
        if (!(*(int*)(player + 292) & 0x400)) {
            if (t->random_pain[*(int*)(player + 8)][arg2] >= 0) {
                int pan = AudioAng((int)(player + 68));

                sndFxQueAdd(t->random_pain[*(int*)(player + 8)][arg2],
                            -1.0f, 1.0f, 192, pan, 66);
            }
        }
    } else {
        int id;

        if ((id = t->severe_pain[*(int*)(player + 8)]) >= 0) {
            int pan = AudioAng((int)(player + 68));

            if (*(int*)(player + 292) & 0x400) {
                id = 97;
            }
            sndFxQueAdd(id, -1.0f, 1.0f, 192, pan, 66);
        }
    }
}

void fn_8009F638(int pidx)
{
    u8* player = &gPlayers[pidx * 13148];
    int f284 = *(int*)(player + 284);

    if (f284 & 0x580000) {
        sndFxPlay3DAtten(66, (int)(player + 68), 127, 40);
    } else {
        switch (f284 & 0xF) {
        case 1:
            sndFxPlay3DAtten(68, (int)(player + 68), 127, 40);
            break;
        case 2:
            sndFxPlay3DAtten(70, (int)(player + 68), 127, 40);
            break;
        case 3:
            sndFxPlay3DAtten(69, (int)(player + 68), 127, 40);
            break;
        case 4:
            sndFxPlay3DAtten(67, (int)(player + 68), 127, 40);
            break;
        default:
            sndFxPlay3DAtten(lbl_801236A4[*(int*)(player + 8)], (int)(player + 68), 127, 42);
            break;
        }
    }
}

void fn_8009F748(int pidx)
{
    u8* player = &gPlayers[pidx * 13148];

    if (!(*(int*)(player + 292) & 0x400)) {
        if (lbl_80123644[*(int*)(player + 8)] >= 0) {
            int pan = AudioAng((int)(player + 68));

            sndFxQueAdd(lbl_80123644[*(int*)(player + 8)], -1.0f, 1.0f, 192, pan, 110);
        }
    }
}

void AudioPlayerSeverePain(int pidx)
{
    u8* player = &gPlayers[pidx * 13148];
    int id;

    if ((id = lbl_80123624[*(int*)(player + 8)]) >= 0) {
        int pan = AudioAng((int)(player + 68));

        if (*(int*)(player + 292) & 0x400) {
            id = 98;
        }
        sndFxQueAdd(id, -1.0f, 1.0f, 192, pan, 66);
    }
}

void AudioPlayerPoison(int pidx)
{
    u8* player = &gPlayers[pidx * 13148];

    if (*(int*)(player + 232) == 1) {
        if (!(*(int*)(player + 288) & 0x10000)) {
            int id = lbl_80123564[*(int*)(player + 8)];

            if (id >= 0) {
                if (*(int*)(player + 292) & 0x400) {
                    id = 96;
                }
                sndFxPlay3D(id, (int)(player + 68), 224, 62);
            }
        }
    }
}

void AudioHeartBeat(int pidx)
{
    u8* player = &gPlayers[pidx * 13148];
    f32 v = *(f32*)(player + 7860);
    int track = lbl_801232C8[pidx];

    if (v <= 10.0) {
        sndFxPlayEx(0, track, 202, 9);
    } else if (v < 25.0) {
        sndFxPlayEx(0, track, 177, 9);
    } else if (v < 100.0) {
        sndFxPlayEx(0, track, 152, 9);
    } else {
        sndFxPlayEx(0, track, 127, 9);
    }
}

void AudioPlayerPain(int pidx)
{
    u8* player = &gPlayers[pidx * 13148];

    if (*(int*)(player + 232) == 1) {
        int pan = AudioAng((int)(player + 68));
        int r = RandInt(4);
        int id = lbl_801234E4[*(int*)(player + 8)][r];

        if (*(int*)(player + 292) & 0x400) {
            id = 96;
        }
        sndFxQueAdd(id, -1.0f, 1.0f, 224, pan, 100);
    }
}

void fn_8009FBD4(int sel)
{
    if (sel <= 0) {
        return;
    }
    if (sel == 1) {
        if (good_wiz_state <= 2) {
            sndFxQueAddEx(1, 0x10030, -1.0f, -1.0f, 224, 127, 2);
        }
    } else if (sel <= 12) {
        int id = lbl_8012348C[sel - 2];

        if (good_wiz_state <= 2) {
            sndFxQueAddEx(1, id, -1.0f, -1.0f, 224, 127, 2);
        }
        if (good_wiz_state <= 2) {
            sndFxQueAddEx(1, 0x10031, -1.0f, -1.0f, 224, 127, 2);
        }
    }
}

void fn_8009FFF4(int sel, int pidx)
{
    switch (sel) {
    case 0:
    default:
        AudioWithName(-1, pidx, 2.0f, 0x10004, -1);
        break;
    case 1:
        AudioWithName(-1, pidx, 1.0f, 0x10005, -1);
        break;
    case 2:
        AudioWithName(-1, pidx, 1.0f, 0x10003, -1);
        break;
    case 3:
        AudioWithName(-1, pidx, 0.5f, 0x20000, -1);
        break;
    }
}

void fn_8009F4D0(int pidx)
{
    int f292 = *(int*)&gPlayers[pidx * 13148 + 292];
    int id;

    if (f292 & 0x10) {
        id = 71;
    } else if (f292 & 0x20) {
        id = 72;
    } else if (f292 & 0x40) {
        id = 73;
    } else {
        return;
    }
    sndFxPlay3D(id, (int)&gPlayers[pidx * 13148 + 84], 224, 40);
}

void fn_8009D7E4(int a, int pos)
{
    int id1 = lbl_80123B1C[sMusicTrackHi];
    int id2 = lbl_80123B54[sMusicTrackHi];

    if (lbl_803447B8 != 0 || lbl_803447B4 != 0) {
        a = 0;
    }
    if (a == 0) {
        if (AudioSoundExists(id1) == 0) {
            sndFxPlay3DTracked(id1, pos, 224, 114);
        }
        id1 = AudioMaskByEvent(114);
        if (id1 != 0) {
            AudioSetTrackPan(id1, AudioAng(pos));
        }
    } else {
        AudioKillBySound(id1);
        if (a >= 2) {
            sndFxPlay3D(id2, pos, 224, 0);
        }
    }
}

void AudioExp(int pidx, int flag)
{
    if (flag > 0) {
        AudioWithName(-1, pidx, 3.0f, 0x20010, 0x1003D);
    } else if (flag < 0) {
        AudioWithName(-1, pidx, 3.0f, 0x1003E, -1);
    }
}

int fn_8009CD80(int a, int b, int val)
{
    int id;

    if (val >= 99) {
        id = 0x1003F;
    } else {
        id = lbl_80123D7C[b][val / 10 - 1];
    }
    AudioWithName(-1, a, 5.0f, id, -1);
    return id;
}

void fn_8009CDF8(int pidx)
{
    sndFxPlayEx(37, lbl_801232C8[pidx], 127, 66);
}

void fn_8009CE38(int pidx)
{
    f32 val;
    f32 delta;

    sndFxPlayEx(40, lbl_801232C8[pidx], 127, 40);
    val = 0.5f;
    delta = (f32)(AudioGetSoundVol(40) - 1.0);
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

void fn_8009D038(int pidx)
{
    sndFxPlayEx(38, lbl_801232C8[pidx], 127, 66);
}

void fn_8009CEE0(int pidx, int sel, int flags)
{
    int p1 = lbl_801232C8[pidx];
    int soundId = 39;
    int pan = 127;

    switch (sel) {
    case 15:
        soundId = 38;
        break;
    case 6:
        if (flags & 0x200000) {
            soundId = 41;
        }
        break;
    case 9:
        if (flags & 1) {
            soundId = 88;
        } else if (flags & 0x100) {
            soundId = 86;
            pan = 180;
        } else if (flags & 0x200) {
            soundId = 84;
            pan = 180;
        } else if (flags & 0x400) {
            soundId = 94;
        }
        break;
    }
    sndFxPlayEx(soundId, p1, pan, 66);
}

void fn_8009CFA8(int pidx, int sel)
{
    s32* t = lbl_801232C8;
    int p1 = t[pidx];
    int soundId = 38;

    if (sMusicTrackHi == 12) {
        switch (sel) {
        case 50:  soundId = t[pidx + 673]; break;
        case 100: soundId = t[pidx + 677]; break;
        case 500:
        default:  soundId = t[pidx + 681]; break;
        }
    }
    sndFxPlayEx(soundId, p1, 127, 66);
}

void fn_8009D100(int pos)
{
    int id = lbl_80123C34[sMusicTrackHi];

    if (lbl_803447B8 == 0) {
        switch (lbl_803447B4) {
        case 0:
            if (id >= 0) {
                sndFxPlay3D(id, pos, 224, 68);
            }
            break;
        }
    }
}

void fn_8009D16C(int pos)
{
    int id = lbl_80123BFC[sMusicTrackHi];

    if (lbl_803447B8 == 0) {
        switch (lbl_803447B4) {
        case 0:
            if (id >= 0) {
                sndFxPlay3D(id, pos, 224, 30);
            }
            break;
        }
    }
}

void fn_8009D1D8(int pos, int sel)
{
    int id = lbl_80123B8C[sel][sMusicTrackHi];

    if (lbl_803447B8 == 0) {
        switch (lbl_803447B4) {
        case 0:
            if (gBossType < 0 && id >= 0) {
                sndFxPlay3D(id, pos, 224, 30);
            }
            break;
        }
    }
}
