#include "types.h"
#include "game/leveldata.h"
#include "game/player.h"

#define offsetof(type, member) ((u32)&(((type*)0)->member))

/* struct audio_data -- the per-level audio descriptor level_data.audio points
 * at.  leveldata.h only forward-declares it, so the body is completed here as
 * a file-local view rather than in the shared header.
 * Layout from the Xbox PDB (audio_data); the three fields this TU actually
 * touches are corroborated on GC by BOTH their access width and their use
 * site: entersnd@0x10 is the s16 read by AudioEnterNextStage, hitsnd@0x12 the
 * s16 read by AudioExplodeWall, namesnd@0x14 the s32 feeding
 * AudioEnterNextStage's announcer queue -- each matching the PDB field's
 * declared size (2/2/4).  The remaining fields carry PDB names unverified on
 * GC; verify a displacement against target asm before relying on one. */
struct audio_data {
    /* 0x00 */ char bank[16];
    /* 0x10 */ s16  entersnd;
    /* 0x12 */ s16  hitsnd;
    /* 0x14 */ s32  namesnd;
    /* 0x18 */ char stream[16];
    /* 0x28 */ s16  nareas;
    /* 0x2A */ s16  stereo;
    /* 0x2C */ s16  nparts[8];
};

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
 * after recovering the AudioExplodeWall call-argument temporaries, the
 * bridge/world-motion one-case switches, the AudioClick argument locals,
 * and SeverePain's
 * assignment-in-condition. Remaining residuals are semantically faithful;
 * AudioPlayerTurbo/AudioPlayerEatFood now have exact instruction counts,
 * and AudioEnterNextStage has a substantially closer control-flow and
 * argument-loading shape.
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
extern Player gPlayers[4]; /* 0x80275AE0 player records, stride 0x335C */
extern s32 sVoiceRotIdxA;  /* 0..3 announcer-voice rotation counter */
extern s32 sVoiceRotIdxB;  /* 0..3 announcer-voice rotation counter */
extern s32 good_wiz_state; /* <=2 => attract/menu path (announcer allowed) */
extern s32 lbl_803447B4;   /* gate flag */
extern s32 lbl_803447B8;   /* gate flag */
extern s32 lbl_8034476C;   /* alternate player-audio dispatch mode */
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
extern level_data* gCurLevel;
extern u8* gWorldData;
extern f32 lbl_80348480;
extern f32 lbl_80348484;
extern f32 lbl_80348490;
extern f32 lbl_80348494;
extern f32 lbl_80348498;

/* ----------------------------------------------------------------- */

#pragma opt_propagation off
int AudioFindPlayerSlot(int pidx, int class_, int type)
{
    u8* slot = (u8*)gPlayers[pidx].powerup;
    f32 value;
    f32 threshold;
    int i;

    for (i = 0; i < 11; i++) {
        value = *(f32*)slot;
        threshold = lbl_8034832C;
        if (value <= threshold) {
            continue;
        }
        if (*(int*)(slot + 12) == type && *(int*)(slot + 4) == class_) {
            return i;
        }
        slot += 16;
    }
    return -1;
}
#pragma opt_propagation reset

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

void AudioTowerFX(int sel)
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

int AudioRuneSpeech(int sel)
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

void AudioShardSpeech(int idx)
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

void AudioGoodWizard(int a, int b)
{
    int row = a - 34;
    int id = lbl_8012417C[row][b];

    if (id >= 0) {
        sndFxQueAddEx(1, id, -1.0f, 10.0f, 224, 127, 2);
    }
}

void AudioGeneratorDamaged(int pos, int sel)
{
    int mt = sMusicTrackHi;
    int id = lbl_80124148[mt];

    if (mt == 10 && sel == 24) {
        id = 0x2C0024;
    }
    sndFxPlay3DAtten(id, pos, 180, 91);
}

void AudioGeneratorDies(int pos, int sel)
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

void AudioWorldHitPlyr(int pos)
{
    int id = lbl_801240DC[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 127, 100);
    }
}

void AudioWorldExplosion(int pos)
{
    int id = lbl_801240A4[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 127, 101);
    }
}

void AudioExplodeWall(int pos, int flag)
{
    if (flag > 0) {
        int idx = gCurLevel->audio->hitsnd;
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

/* Dispatch player event sounds between named playback and the announcer
 * queue.  The accepted event ranges differ in the alternate audio mode. */
#pragma opt_propagation off
void fn_8009CB44(s32 pidx, u32 sound, u32 extra)
{
    s32 track = lbl_801232C8[pidx];
    u32 event = sound;
    u32 tail = extra;
    f32 volume;

    if (lbl_8034476C <= 1) {
        switch (event) {
        case 0x1003D:
        case 0x2002C:
            volume = lbl_80348490;
            if (event == 0x1003D) {
                volume = lbl_80348494;
            }
            AudioWithName(-1, pidx, volume, 0x20010, event);
            return;
        case 0x2000F:
            AudioWithName(-1, pidx, lbl_80348498, event, tail);
            return;
        case 0x1002C:
            AudioWithName(-1, pidx, lbl_80348498, event, tail);
            return;
        }
        if (good_wiz_state <= 2) {
            sndFxQueAddEx(1, event, lbl_80348480, lbl_80348484, 224,
                          track, 2);
        }
        return;
    }

    switch (event) {
    case 0x1003D:
    case 0x20011:
    case 0x20012:
    case 0x20013:
    case 0x20014:
    case 0x20015:
    case 0x20016:
    case 0x20017:
    case 0x20018:
    case 0x20019:
    case 0x2001A:
    case 0x2001B:
    case 0x2001C:
    case 0x2001D:
    case 0x2001E:
    case 0x2001F:
    case 0x20020:
    case 0x20021:
    case 0x20022:
    case 0x20023:
    case 0x20024:
    case 0x20025:
    case 0x20026:
    case 0x20027:
    case 0x20028:
    case 0x2002B:
    case 0x2002C:
    case 0x2002D:
    case 0x2002E:
    case 0x2002F:
    case 0x20030:
    case 0x20031:
    case 0x20035:
    case 0x20036:
    case 0x20037:
    case 0x20038:
    case 0x20039:
    case 0x2003A:
    case 0x2003B:
    case 0x2003C:
    case 0x2003D:
        volume = lbl_80348490;
        if (event == 0x1003D) {
            volume = lbl_80348494;
        }
        AudioWithName(-1, pidx, volume, 0x20010, event);
        return;
    case 0x2000F:
        AudioWithName(-1, pidx, lbl_80348498, event, tail);
        return;
    case 0x1002C:
        AudioWithName(-1, pidx, lbl_80348498, event, tail);
        return;
    }
    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, event, lbl_80348480, lbl_80348484, 224, track, 2);
    }
}
#pragma opt_propagation reset

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

void fn_8009D350(s32 player)
{
    (void)player;
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
    sndFxPlay3D(80, (int)gPlayers[pidx].col_pos, 224, 40);
}

void fn_8009D4F0(int pidx)
{
    sndFxPlay3D(89, (int)gPlayers[pidx].col_pos, 224, 40);
}

void fn_8009D530(void)
{
    sndFxPlayEx(85, 127, 224, 40);
}

void fn_8009D560(int pidx)
{
    sndFxPlay3D(87, (int)gPlayers[pidx].col_pos, 224, 40);
}

void fn_8009D5A0(int pidx)
{
    sndFxPlay3D(99, (int)gPlayers[pidx].col_pos, 224, 40);
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

static inline void sndFxPlay3DAttenOrdered(int soundId, int pos, int flags,
                                           int pan)
{
    sndFxPlay3DAtten(soundId, pos, pan, flags);
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
        int idx = gCurLevel->audio->hitsnd;

        if (idx >= 0) {
            u8* e = *(u8**)(gWorldData + 44) + idx * 24;

            if (*(s32*)(e + 16) >= 0) {
                sndFxPlay3DAttenOrdered(
                    *(s32*)(e + 16), arg,
                    *(s16*)(e + 22) != 0 ? *(s16*)(e + 22) : 126,
                    *(s16*)(e + 20) != 0 ? *(s16*)(e + 20) : 224);
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

    if (id0 >= 0) {
        if (mode == 1) {
            int id = *(s16*)(p + 518) >= 2 ? T[id0 + 353] : T[id0 + 345];

            sndFxPlay3DAtten(id, p + 52, 224, 25);
        } else {
            int id;

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

    if (id0 >= 0) {
        int id = *(s16*)(p + 518) >= 2 ? T[id0 + 433] : T[id0 + 425];

        sndFxPlay3DAtten(id, p + 52, 180, 75);
    }
}

/* --- AudioSetupBossStreams support --------------------------------- */
extern char* strcpy(char* dst, const char* src);
extern char* strcat(char* dst, const char* src);
extern int sprintf(char* buf, const char* fmt, ...);
extern int AudioFindSound(char* a, int b, int c);
extern int LevelLetter(int a);
extern s32 lbl_802577CC[]; /* level -> boss-stream select code (0..29) */
extern s32 lbl_8025778C[]; /* level -> boss rank/tier */
extern char lbl_80114A48[]; /* boss-stream format string pool */

/* gBossType 36/37/41 use a shared sample set: truncate the speech name
 * at 14 chars and append the variant letter before the lookup. */
#define BossNameFixup(buffer)                                               \
    if (gBossType > 0) {                                                    \
        switch (gBossType) {                                                \
        case 41:                                                            \
            (buffer)[14] = 0;                                               \
            strcat((buffer), "B");                                         \
            break;                                                          \
        case 37:                                                            \
            (buffer)[14] = 0;                                               \
            strcat((buffer), "D");                                         \
            break;                                                          \
        case 36:                                                            \
            (buffer)[14] = 0;                                               \
            strcat((buffer), "C");                                         \
            break;                                                          \
        }                                                                   \
    }

#pragma opt_propagation off
/* sSpeechNameBuf is only a 0x40-byte sprintf scratch buffer (symbols.txt:
 * .bss 0x8028B5D0 size 0x40).  Every `speech + 0x4B0..0x6C4` store below
 * therefore lands OUTSIDE it, in the symbol the linker places next:
 * sActiveTrackId (.bss 0x8028BA80, size 0x238 = 142 s32) -- 0x8028B5D0 +
 * 0x4B0 is exactly sActiveTrackId's base.  Layout implied by the accesses:
 * 45 active-track ids (0x4B0..0x563, matching this file's own "45 entries"
 * note), then a [12][8] boss-speech id table from element 45 (0x564) with
 * 0x20-byte rows, indexed `[row][idx]`.
 *
 * The base symbol is NOT rewritten to sActiveTrackId: the TARGET relocates
 * every one of these stores @sSpeechNameBuf(ADDR16_HA/LO) and keeps that
 * pointer in r30 for the whole function, so per
 * claim.law.walked-base-symbol-identity sSpeechNameBuf is the correct base
 * and re-basing would emit a different relocation.  Only the displacement
 * constants are named. */
#define SPEECH_ACTIVETRACK 0x4B0            /* sActiveTrackId - sSpeechNameBuf */
#define SPEECH_BOSSROW(n)  (0x564 + (n) * 0x20) /* sActiveTrackId[45] + row n */

void AudioSetupBossStreams(register int idx, register char* name)
{
    char bufA[32]; /* close-variant stream name */
    char bufB[32]; /* far-variant stream name */
    register int mode;
    register char* suffix = "DIE";
    register int sel;
    register char* speech = (char*)sSpeechNameBuf;
    register char* formats = lbl_80114A48;
    int nvar;

    sel = lbl_802577CC[idx];
    if (sel < 0) {
        return;
    }
    if (name == NULL || *name == 0) {
        idx = -1;
    }
    *(s32*)(speech + SPEECH_ACTIVETRACK + sel * 4) = idx;
    mode = 0;

    switch (sel) {
    case 29: /* golem/wizard */
        sprintf(bufA, "GOL%c", (signed char)LevelLetter(0));
        sprintf(bufB, "GOL%c", (signed char)LevelLetter(0));
        nvar = 0;
        sprintf(speech, formats + 348, (signed char)LevelLetter(0));
        sMusicSlot0 = AudioFindSound(speech, -1, 1);
        sprintf(speech, formats + 364, (signed char)LevelLetter(0));
        sMusicSlot1 = AudioFindSound(speech, -1, 1);
        sprintf(speech, formats + 376, (signed char)LevelLetter(0));
        sMusicSlot2 = AudioFindSound(speech, -1, 1);
        suffix = "KILL";
        break;
    default:
        strcpy(bufA, name);
        strcpy(bufB, name);
        nvar = 0;
        if (sel != 29) {
            mode = 1;
        }
        break;
    case 1:
    case 2:
    case 4:
    case 5:
    case 7:
    case 8:
    case 10:
    case 11:
    case 13:
    case 14:
    case 16:
    case 17:
    case 19:
    case 20:
    case 23:
    case 24:
    case 25:
    case 26: /* boss levels */
        if (lbl_8025778C[idx] >= 10 || gBossType >= 0) {
            sprintf(bufA, "%s2", name);
            sprintf(bufB, "%s2", name);
            nvar = 2;
        } else {
            sprintf(bufA, "%s1", name);
            sprintf(bufB, "%s2", name);
            nvar = 1;
        }
        if (sel == 2 || sel == 8 || sel == 19 || sel == 17 || sel == 24 || sel == 25) {
            mode = 1;
        } else if (sel == 11) {
            mode = 2;
        }
        break;
    case 27:
        sprintf(bufA, "%s1", name);
        sprintf(bufB, "%s1", name);
        nvar = 0;
        break;
    }

    sprintf(speech, formats + 392, bufA, suffix);
    BossNameFixup(speech);
    *(s32*)(speech + SPEECH_BOSSROW(0) + idx * 4) = AudioFindSound(speech, -1, 1);

    sprintf(speech, formats + 392, bufB, suffix);
    BossNameFixup(speech);
    *(s32*)(speech + SPEECH_BOSSROW(1) + idx * 4) = AudioFindSound(speech, -1, 1);

    sprintf(speech, formats + 404, bufA, suffix);
    BossNameFixup(speech);
    *(s32*)(speech + SPEECH_BOSSROW(2) + idx * 4) = AudioFindSound(speech, -1, 1);

    sprintf(speech, formats + 404, bufB, suffix);
    BossNameFixup(speech);
    *(s32*)(speech + SPEECH_BOSSROW(3) + idx * 4) = AudioFindSound(speech, -1, 1);

    if (nvar < 2) {
        sprintf(speech, formats + 416, bufA);
        BossNameFixup(speech);
        *(s32*)(speech + SPEECH_BOSSROW(4) + idx * 4) = AudioFindSound(speech, -1, 1);

        sprintf(speech, formats + 432, bufA);
        BossNameFixup(speech);
        *(s32*)(speech + SPEECH_BOSSROW(5) + idx * 4) = AudioFindSound(speech, -1, 1);
    }

    if (nvar != 0) {
        sprintf(speech, formats + 444, bufB);
        BossNameFixup(speech);
        *(s32*)(speech + SPEECH_BOSSROW(6) + idx * 4) = AudioFindSound(speech, -1, 1);

        sprintf(speech, formats + 460, bufB);
        BossNameFixup(speech);
        *(s32*)(speech + SPEECH_BOSSROW(7) + idx * 4) = AudioFindSound(speech, -1, 1);

        sprintf(speech, formats + 476, bufB);
        BossNameFixup(speech);
        *(s32*)(speech + SPEECH_BOSSROW(8) + idx * 4) = AudioFindSound(speech, -1, 1);

        sprintf(speech, formats + 488, bufB);
        BossNameFixup(speech);
        *(s32*)(speech + SPEECH_BOSSROW(9) + idx * 4) = AudioFindSound(speech, -1, 1);
    } else {
        sprintf(speech, formats + 416, bufB);
        BossNameFixup(speech);
        *(s32*)(speech + SPEECH_BOSSROW(6) + idx * 4) = AudioFindSound(speech, -1, 1);
        *(s32*)(speech + SPEECH_BOSSROW(7) + idx * 4) = *(s32*)(speech + SPEECH_BOSSROW(6) + idx * 4);

        sprintf(speech, formats + 432, bufB);
        BossNameFixup(speech);
        *(s32*)(speech + SPEECH_BOSSROW(8) + idx * 4) = AudioFindSound(speech, -1, 1);
        *(s32*)(speech + SPEECH_BOSSROW(9) + idx * 4) = *(s32*)(speech + SPEECH_BOSSROW(8) + idx * 4);
    }

    if (mode == 2) {
        sprintf(speech, formats + 500, bufA);
        BossNameFixup(speech);
        *(s32*)(speech + SPEECH_BOSSROW(10) + idx * 4) = AudioFindSound(speech, -1, 1);

        sprintf(speech, formats + 500, bufB);
        BossNameFixup(speech);
        *(s32*)(speech + SPEECH_BOSSROW(11) + idx * 4) = AudioFindSound(speech, -1, 1);
    } else if (mode == 1) {
        sprintf(speech, formats + 512, bufA);
        BossNameFixup(speech);
        *(s32*)(speech + SPEECH_BOSSROW(10) + idx * 4) = AudioFindSound(speech, -1, 1);

        sprintf(speech, formats + 512, bufB);
        BossNameFixup(speech);
        *(s32*)(speech + SPEECH_BOSSROW(11) + idx * 4) = AudioFindSound(speech, -1, 1);
    }
}
#pragma opt_propagation reset

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

void AudioClick(int pidx, int sel)
{
    int track = lbl_801232C8[pidx];
    int id = (&lbl_80343E2C)[sel];

    sndFxPlayEx(id, track, 127, 125);
}

void AudioClick2(int pidx, int sel)
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
    sndFxPlay3D(81, (int)gPlayers[pidx].col_pos, 127, 60);
}

void fn_8009F158(int pidx)
{
    sndFxPlay3D(82, (int)gPlayers[pidx].col_pos, 224, 60);
}

void fn_8009F390(int pidx)
{
    sndFxPlay3D(5, (int)gPlayers[pidx].effectpos, 127, 19);
}

void fn_8009F3D0(int pidx)
{
    sndFxPlay3D(93, (int)gPlayers[pidx].col_pos, 224, 40);
}

void fn_8009F410(int pidx)
{
    sndFxPlay3D(92, (int)gPlayers[pidx].col_pos, 224, 40);
}

void fn_8009F450(int pidx)
{
    sndFxPlay3D(91, (int)gPlayers[pidx].col_pos, 224, 40);
}

void fn_8009F490(int pidx)
{
    sndFxPlay3D(90, (int)gPlayers[pidx].col_pos, 224, 40);
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

    sndFxPlay3DAtten(id, (int)gPlayers[pidx].pos, 127, 125);
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
    Player* player = &gPlayers[pidx];

    if (player->state == 1) {
        int id;

        sndFxPlay3D(1, (int)player->pos, 127, 8);
        id = lbl_8012380C[player->char_type];
        if (player->flags & 0x400) {
            id = 98;
        }
        sndFxPlay3D(id, (int)player->pos, 224, 7);
    }
}

void AudioPlayerHit(int pidx, int a)
{
    Player* player = &gPlayers[pidx];

    if (player->state == 1) {
        sndFxPlay3D(lbl_801237BC[a][pidx], (int)player->pos, 127, 74);
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

void DoAudioTallySFX(int sel_)
{
    int sel;

    if ((sel = sel_) < 0) {
        goto done;
    }
    if (sel <= 10) {
        goto valid;
    }
    goto done;
valid:
    sndFxPlayHandle(lbl_801234B8[sel], 224, sel + 21);
done:
    return;
}

void AudioMapDot(void)
{
    int id = lbl_8012341C[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlayHandle(id, 224, 30);
    }
}

void AudioEnterNextStage(void)
{
    struct audio_data* level = gCurLevel->audio;
    int idx = level->entersnd;
    u8* entry;
    int entry_id;

    if (idx < 0) {
        goto invalid_entry;
    }
    entry = *(u8**)(gWorldData + 44) + idx * 24;
    entry_id = *(int*)(entry + 16);
    switch (entry_id) {
    case 0:
        goto valid_entry;
    default:
        if (entry_id < 0) {
            goto invalid_entry;
        }
        goto valid_entry;
    }
invalid_entry:
    entry = 0;
valid_entry:
    if (entry != 0) {
        if (level->namesnd >= 0) {
            int sound_id = *(int*)(entry + 16);

            if (good_wiz_state <= 2) {
                sndFxQueAddEx(1, sound_id, lbl_80348480, lbl_80348480, 224,
                              127, 2);
            }
            {
                int next_id = gCurLevel->audio->namesnd;

                if (good_wiz_state <= 2) {
                    sndFxQueAddEx(1, next_id, lbl_80348480, lbl_80348480, 224,
                                  127, 2);
                }
            }
        }
    }
}

void AudioPlayerBreath(int pidx)
{
    int sound = sMusicTrackHi == 13 ? 0x30014 : 0x2000C;

    AudioWithName(-1, pidx, 5.0f, sound, -1);
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

void AudioDamageTile(int pos, int idx)
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

#pragma opt_common_subs off
void AudioPlayerTurbo(int pidx, int sel, int arg3)
{
    typedef struct AudioTurboSoundIds {
        u8 pad_0000[1020];
        s32 snd_turbo_a[8][4];
        s32 snd_turbo_b[8];
        s32 snd_turbo_c[8];
    } AudioTurboSoundIds;
    AudioTurboSoundIds* t = (AudioTurboSoundIds*)lbl_801232C8;
    int f8;
    int slot;
    int flags;

    /* Three nearby fields off one index-computed base: a typed
     * `gPlayers[pidx].field` form regressed this function (real 0 -> 14,
     * schedule-class) per claim.law.multifield-alias-defeats-indexed-
     * addressing.  The law's verified counter-form is kept here -- the raw
     * single additive expression with offsetof()-spelled displacements. */
    flags = *(int*)((u8*)gPlayers + pidx * 13148 + offsetof(Player, flags));
    f8 = *(int*)((u8*)gPlayers + pidx * 13148 + offsetof(Player, char_type));
    slot = (int)((u8*)gPlayers + pidx * 13148 + offsetof(Player, pos));

    if (flags & 0x400) {
        sndFxPlay3D(95, slot, 224, 16);
    } else {
        switch (sel) {
        case 0:
            sndFxPlay3D(t->snd_turbo_a[f8][arg3], slot, 224, 19);
            break;
        case 1:
            sndFxPlay3D(t->snd_turbo_b[f8], slot, 224, 17);
            break;
        case 2:
            sndFxPlay3D(t->snd_turbo_c[f8], slot, 224, 16);
            break;
        }
    }
}
#pragma opt_common_subs reset

void AudioPlayerEatFood(int pidx, int foodType)
{
    typedef struct AudioFoodSoundIds {
        u8 pad_0000[700];
        s32 snd_eat[8][4];
        s32 snd_eat_default[1];
    } AudioFoodSoundIds;
    Player* player = &gPlayers[pidx];
    Player* p = player;
    AudioFoodSoundIds* t = (AudioFoodSoundIds*)lbl_801232C8;

    if (RandInt(4) == 0) {
        if (!(p->flags & 0x400)) {
            if (t->snd_eat[p->char_type][foodType] >= 0) {
                int pan = AudioAng((int)p->pos);

                sndFxQueAdd(t->snd_eat[p->char_type][foodType],
                            -1.0f, 1.0f, 192, pan, 66);
            }
        }
    } else {
        int id;

        if ((id = t->snd_eat_default[p->char_type]) >= 0) {
            int pan = AudioAng((int)p->pos);

            if (p->flags & 0x400) {
                id = 97;
            }
            sndFxQueAdd(id, -1.0f, 1.0f, 192, pan, 66);
        }
    }
}

void AudioPlayerEatSFX(int pidx)
{
    /* NOTE: this function's compiled body is pinned by a WebFrank rule
     * (config/GUNE5D/webfrank.json), so its source SHAPE must not change --
     * the byte-offset walk below is deliberately left in its original form
     * rather than converted to Player member access.  Only the base
     * derivation is respelled for the retyped gPlayers declaration;
     * `(u8*)gPlayers + playerOffset` is the same arithmetic the previous
     * `&gPlayers[playerOffset]` performed when gPlayers was `u8[]`. */
    int playerOffset = pidx * 13148;
    int f284;
    u8* player;

    player = (u8*)gPlayers + playerOffset;
    f284 = *(int*)(player + 284);

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
            player = (u8*)gPlayers + playerOffset;
            sndFxPlay3DAtten(lbl_801236A4[*(int*)(player + 8)],
                            (int)(player + 68), 127, 42);
            break;
        }
    }
}

#ifdef __MWERKS__
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif

void fn_8009F748(int pidx, int sourcePlayer)
{
    Player* player = &gPlayers[pidx];

    if (!(player->flags & 0x400)) {
        if (lbl_80123644[player->char_type] >= 0) {
            int pan = AudioAng((int)player->pos);

            sndFxQueAdd(lbl_80123644[player->char_type], -1.0f, 1.0f, 192, pan, 110);
        }
    }
}

void AudioPlayerSeverePain(int pidx)
{
    Player* player = &gPlayers[pidx];
    int id;

    if ((id = lbl_80123624[player->char_type]) >= 0) {
        int pan = AudioAng((int)player->pos);

        if (player->flags & 0x400) {
            id = 98;
        }
        sndFxQueAdd(id, -1.0f, 1.0f, 192, pan, 66);
    }
}

void AudioPlayerPoison(int pidx)
{
    Player* player = &gPlayers[pidx];

    if (player->state == 1) {
        if (!(player->shield_flags & 0x10000)) {
            int id = lbl_80123564[player->char_type];

            if (id >= 0) {
                if (player->flags & 0x400) {
                    id = 96;
                }
                sndFxPlay3D(id, (int)player->pos, 224, 62);
            }
        }
    }
}

typedef struct AudioPlayerRecord {
    u8 data[13148];
} AudioPlayerRecord;

void AudioHeartBeat(int pidx)
{
    Player* player = gPlayers;
    f32 v;
    int track = lbl_801232C8[pidx];

    player += pidx;
    v = player->health;

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
#pragma opt_propagation off
void AudioPlayerPain(int pidx)
{
    Player* player = &gPlayers[pidx];

    if (player->state == 1) {
        int pan = AudioAng((int)player->pos);
        int r = RandInt(4);
        u32 randomOffset = (u32)r << 2;
        int id = *(int*)((u8*)lbl_801234E4 +
                         (player->char_type << 4) + randomOffset);

        if (player->flags & 0x400) {
            id = 96;
        }
        sndFxQueAdd(id, -1.0f, 1.0f, 224, pan, 100);
    }
}
#pragma opt_propagation reset

void AudioNumRunesFound(int runeCount)
{
    if (runeCount <= 0) {
        return;
    }
    if (runeCount == 1) {
        if (good_wiz_state <= 2) {
            sndFxQueAddEx(1, 0x10030, -1.0f, -1.0f, 224, 127, 2);
        }
    } else if (runeCount <= 12) {
        int id = lbl_8012348C[runeCount - 2];

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

void AudioTurboDefense(int pidx)
{
    int f292 = gPlayers[pidx].flags;
    int id;
    int pos;

    if (f292 & 0x10) {
        id = 71;
    } else if (f292 & 0x20) {
        id = 72;
    } else if (f292 & 0x40) {
        id = 73;
    } else {
        return;
    }
    pos = (u32)gPlayers[pidx].col_pos;
    sndFxPlay3D(id, pos, 224, 40);
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

#pragma opt_propagation off
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
#pragma opt_propagation reset

void AudioBridgeOpen(int pos)
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

void AudioBridgeClose(int pos)
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

void AudioWorldObjectMotion(int pos, int sel)
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
