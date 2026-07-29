#include "types.h"

/*
 * game/ui/auxscreen.c -- the "aux screen" TU: the between-level map screen,
 * the pre-rendered game movies (victory / garm), on-screen caption text and
 * the "good wizard" character that appears on the map to hand out rune stones.
 *
 * Identified from the GUNE5D linker map (StartGoodWizard folds with
 * LooseBallAnims::Destroy at 0x80019964) and the Xbox PDB module auxscreen.obj.
 * The GameCube build emits these functions in reverse source order, so this
 * file is laid out DoGoodWizard-first to match the DOL address order.
 *
 * Frontier game code: no reference source. NonMatching -- the small leaf
 * helpers match byte-exact; the large state machines are structural best-effort.
 * Names follow the auxscreen.obj roster where behaviour confirms them.
 */

/* ------------------------------------------------------------------ */
/* Shared engine globals owned by other TUs (referenced, not renamed) */
/* ------------------------------------------------------------------ */

/* Player/team array, stride 0x335C (13148). Owned elsewhere. */
extern u8 gPlayers[];
/* Big aux-screen scene object (blits + good-wizard model), engine owned. */
extern u8 lbl_8023DFD0[];
/* Base vec3 used to seed the averaged wizard position. */
extern f32 gBossPos[3];
/* .data tables referenced by the wizard/movie logic. */
extern u8 gIdentityMatrix[];
extern u8 lbl_80118250[];
/* Per-frame time deltas (engine globals in the .sbss window). */
extern s32 gFrameTicks;  /* integer frame delta */
extern s32 lbl_80344578;  /* caption frame delta */
extern f32 sMusicFadeBase;  /* float frame delta */
/* Assorted engine handles read by DoGoodWizard/init_gamemovie. */
extern void* sMusicTrackHi;
extern void* lbl_80344BD4;
extern void* lbl_80344BEC;
extern void* sItemFile1Buf;
extern s32 lbl_803449A4;
extern s32 gControllerButtons;
extern s32 sFlags;
extern s32 lbl_8034481C;
extern s32 sLastWorldLevel;
extern s32 gGameMode;
extern void* gCurLevel;

/* ------------------------------------------------------------------ */
/* Aux-screen owned .sdata (tunables) and .sbss (state) globals        */
/* ------------------------------------------------------------------ */

/* .sdata tunables (initialised) */
s32 map_fade_a = 60;
s32 map_fade_b = 10;
s32 map_load_len = 210;
s32 map_load_step = 180;
s32 wiz_exit_min = 35;
s32 WizDelayGoldLeft = 600;
s32 WizDelayNoGold = 120;

/* .sbss state (zero-initialised) */
void* map_route_blit;
void* map_bg_blit;
s32 map_load_timer;
s32 map_load_progress;
s32 map_load_delay;
s32 map_load_state;
s32 map_fade_frame;
s32 map_fade_alpha;
s32 caption_line;
s32 caption_page;
s32 caption_timer;
s32 movieactive;
s32 movie_state;
s32 kill_gamemovie;
s32 wiz_mode;
f32 good_wiz_timer;
s32 all_rune_stones;
s32 good_wiz_alpha;
s32 good_wiz_speech_idx;
s32 good_wiz_speech_frame;
s32 good_wiz_speech_pause;
f32 good_wiz_yaw;
s32 good_wiz_plyr_attn;
s32 good_wiz_state;
s32 good_wiz_enabled;
s32 good_wiz_exit_timer;

/* ------------------------------------------------------------------ */
/* External subroutines                                                */
/* ------------------------------------------------------------------ */
extern s32 GetBossBeatFlag(void* p);
extern s32 GetBossNumRunes(void* p);
extern void* AtreeMatch(void* atree, char* name, s32 flag);
extern void* fn_80012F78(void* dst, void* src, s32 a);
extern void* MBNewNode(void* p, void* tbl, s32 a);
extern void MBNodeSetParent(void* a, void* b);
extern void calc_wizard_pos(f32* out);
extern void CopyMat4(void* p);
extern void fn_8002C53C(void* p);
extern void calc_good_wiz_attn(s32 reset, s32 force);
extern s32 hide_rune_stones(void* p);
extern void MBTreeSetAlpha(void* p, s32 alpha, s32 a);
extern void fn_80011104(void* p, s32 a, s32 b);
extern void fn_8009C710(s32 speech);
extern s32 CaptionText(char* a, char* b, s32 line, s32 page, s32 flags);
extern s32 fn_800629B0(void);
extern s32 sndFxUpdate(s32 a);
extern void SetSkinFX(void* fx, s32 frames, f32 rate, f32 base, f32 loops); /* was mislabeled StartFXMat */
extern void PlayVQMovie(char* name);
extern void AudioStopSelect(void);
extern void AudioSelectReset(void);
extern void MBOX_ResetUnlockedModels(s32 a);
extern void delete_map_blits(void);
extern s32 active_player_edge(s32 flag);
extern void MBRemoveBlit(void* blit);
extern void del_player_blits(s32 i);
extern f32 atan2(f32 y, f32 x);
extern void CreatePYRMatrix(void* mtx, void* v);
extern void UpdateObjWorldMat(void* mtx);
extern s32 GetScrollText(char* a, char* b, s32 line, s32* out);
extern s32 GetScrollScale(char* a, char* b, s32 line);
extern s32 CaptionTextSub(s32 x, f32 w, s32 y, s32 page, s32 flags);
extern s32 ScrollTextNum(char* a, char* b);
extern void AudioEnterNextStage(void);
extern s32 fn_80055F68(s32 a, s32 b);
extern s32 AudioSysUpdate(s32 a);
extern void* AudioRegisterNameBanks(void* p, s32 a);
extern s32 sprintf(char* buf, const char* fmt, ...);
extern s32 MBOX_FindTexture_Sub(char* name, s32 a, s32 b, s32 c);
extern void* MBCreateBlit(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f);
extern void mbBlitCvtCoord(void* blit, f32 z);
extern void MBBlitSetAlpha(void* blit, s32 alpha);
extern void DrawGlowText(s32 x, s32 y, char* s, f32 z);
extern void mbBlitInit3414(void* blit, s32 a);
extern void AudioStopMusicB(void);
extern void AudioEmptyCb2(void);
extern void MapMusicStart(void);
extern void next_world(void);
extern void fn_80053D08(s32 a, s32 b, s32 c);
extern void setup_player_display(s32 i);
extern s32 init_next_level(s32 a);
extern void* LoadModel(char* name, s32 a, s32 b, s32 c);
extern void* MBNewBlit(void* base, s32 a, s32 b);
extern void* MBOX_FindTexture(char* name, s32 a);
extern void strcat(char* d, char* s);
extern void fn_80053C70(void);

/* Forward prototypes (helpers defined after their first caller). */
s32 do_gamemovie(void);

/* ================================================================== */
/* DoGoodWizard -- the on-map "good wizard" state machine (0x8D4).     */
/* Structural best-effort; NonMatching.                                */
/* ================================================================== */
void DoGoodWizard(void)
{
    s32 i;
    u16 acc3540 = 0;
    u16 acc3542 = 0;
    s32 quality;
    s32 have;
    s32 want;
    f32 pos[3];

    for (i = 0; i < 4; i++) {
        u8* p = gPlayers + i * 0x335C;
        if (*(s32*)(p + 232) == 1) {
            u8* slot = p + *(s32*)(p + 12) * 240;
            acc3542 |= *(u16*)(slot + 3542);
            acc3540 |= *(u16*)(slot + 3540);
        }
    }

    have = GetBossBeatFlag(sMusicTrackHi);
    want = GetBossNumRunes(sMusicTrackHi);
    if (have != (have & acc3542)) {
        if ((acc3542 & have) != 0) {
            quality = 1;
        } else {
            quality = 0;
        }
    } else if (want > 1) {
        quality = 3;
    } else {
        quality = 2;
    }

    if (good_wiz_state > 11) {
        good_wiz_enabled = 0;
        goto tail;
    }

    switch (good_wiz_state) {
    case 0:
        if (wiz_mode == 42 || wiz_mode == 44) {
            good_wiz_timer = 10.0f + sMusicFadeBase;
        } else {
            good_wiz_timer = 5.0f + sMusicFadeBase;
        }
        good_wiz_state++;
        if (sMusicFadeBase >= good_wiz_timer) {
            good_wiz_state++;
        }
        break;

    case 1:
        AtreeMatch(sItemFile1Buf, "WIZARD", 1);
        *(void**)(lbl_8023DFD0 + 1520) =
            fn_80012F78(lbl_8023DFD0 + 1520, (void*)0x88001880, 0);
        *(u16*)(lbl_8023DFD0 + 1576) = 1;
        *(void**)(lbl_8023DFD0 + 1512) =
            MBNewNode(lbl_80344BD4, gIdentityMatrix, 1);
        MBNodeSetParent(*(void**)(*(void**)(lbl_8023DFD0 + 1520)),
                    *(void**)(lbl_8023DFD0 + 1512));
        *(s32*)(lbl_8023DFD0 + 1516) = 0;
        if (wiz_mode - 42 > 1) {
            calc_wizard_pos(pos);
            pos[1] = pos[1] + 3.0f;
        } else {
            pos[0] = 0.0f;
            pos[1] = -12.0f;
            pos[2] = 6.0f;
        }
        good_wiz_state++;
        break;

    default:
        break;
    }

tail:
    if (good_wiz_state >= 9 && fn_800629B0() == 0) {
        if (WizDelayNoGold < good_wiz_exit_timer) {
            good_wiz_exit_timer = WizDelayNoGold;
        }
    }
    (void)quality;
    (void)acc3540;
    (void)have;
    (void)want;
}

/* ================================================================== */
/* StartGoodWizard (0x10) -- arms the good-wizard sequence.           */
/* Folds with LooseBallAnims::Destroy in the DOL.                     */
/* ================================================================== */
void StartGoodWizard(void)
{
    good_wiz_state = 1;
    good_wiz_enabled = 1;
}

/* ================================================================== */
/* hide_rune_stones (0x90) -- true when every rune-stone bit is set.  */
/* ================================================================== */
s32 hide_rune_stones(void* unused)
{
    s32 result = 1;
    s32 acc = 0;
    s32 i;
    s32 j;

    for (i = 0; i < 4; i++) {
        u8* p = gPlayers + i * 0x335C;
        if (*(s32*)(p + 232) == 1) {
            u8* slot = p + *(s32*)(p + 12) * 240;
            acc |= *(u16*)(slot + 3542);
        }
    }

    /* NonMatching: opcodes identical to target; only two GPRs (acc vs the
     * inner counter) are transposed -- a pure register-colouring residual. */
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 3; j++) {
            if ((acc & (1 << (i * 3 + j))) == 0) {
                result = 0;
            }
        }
    }
    return result;
}

/* ================================================================== */
/* calc_good_wiz_attn (0x158) -- rotate the wizard to face a player.  */
/* ================================================================== */
void calc_good_wiz_attn(s32 reset, s32 force)
{
    u8* base = lbl_8023DFD0;
    f32 target;
    f32 delta;
    f32 mtxbuf[3];

    if (force == 0) {
        if (good_wiz_plyr_attn >= 0) {
            u8* p = gPlayers + good_wiz_plyr_attn * 0x335C;
            if (*(s32*)(p + 232) == 1) {
                goto have_target;
            }
        }
        {
            s32 tries = 4;
            s32 found = 0;
            do {
                s32 n = good_wiz_plyr_attn + 1;
                if (n >= 4) {
                    n = 0;
                }
                good_wiz_plyr_attn = n;
                {
                    u8* p = gPlayers + good_wiz_plyr_attn * 0x335C;
                    if (*(s32*)(p + 232) == 1) {
                        break;
                    }
                }
                found++;
            } while (--tries);
            if (found == 4) {
                return;
            }
        }
    }

have_target:
    {
        u8* p = gPlayers + good_wiz_plyr_attn * 0x335C;
        f32 dx = *(f32*)(p + 68) - *(f32*)(base + 1464);
        f32 dz = *(f32*)(p + 76) - *(f32*)(base + 1472);
        target = atan2(dx, dz);
    }
    delta = target - good_wiz_yaw;
    if (reset == 0) {
        f32 lim = (f32)(1.5707963267948966 * (double)sMusicFadeBase);
        if (delta > lim) {
            delta = lim;
        }
        if (delta < -lim) {
            delta = -lim;
        }
    }
    good_wiz_yaw = good_wiz_yaw + delta;
    mtxbuf[0] = 0.0f;
    mtxbuf[1] = good_wiz_yaw;
    mtxbuf[2] = 0.0f;
    CreatePYRMatrix(base + 1416, mtxbuf);
    UpdateObjWorldMat(base + 1416);
}

/* ================================================================== */
/* calc_wizard_pos (0xDC) -- averaged position of the active players. */
/* ================================================================== */
void calc_wizard_pos(f32* out)
{
    u8* arr = gPlayers;
    f32 count = 1.0f;
    s32 i;

    out[0] = gBossPos[0];
    out[1] = gBossPos[1];
    out[2] = gBossPos[2];

    for (i = 0; i < 4; i++) {
        u8* p = arr + i * 0x335C;
        s32 st = *(s32*)(p + 232);
        if (st == 1 || st == 8) {
            out[0] = out[0] + *(f32*)(p + 68);
            out[1] = out[1] + *(f32*)(p + 72);
            out[2] = out[2] + *(f32*)(p + 76);
            if (count == 1.0f) {
                out[1] = (f32)(2.0 * (double)*(f32*)(p + 72));
            }
            count = count + 1.0f;
        }
    }
    out[0] = out[0] / count;
    out[1] = out[1] / count;
    out[2] = out[2] / count;
}

/* ================================================================== */
/* do_gamemovie (0x94) -- advance the currently-playing game movie.   */
/* ================================================================== */
s32 do_gamemovie(void)
{
    if (movieactive == 0) {
        return movie_state;
    }
    if (active_player_edge(0x02000000) != 0 && movie_state == 1) {
        kill_gamemovie = 1;
    }
    if (kill_gamemovie != 0) {
        movieactive = 0;
    }
    if (movieactive != 0) {
        movieactive = 0;
    }
    if (movieactive == 0) {
        return movie_state;
    }
    return 0;
}

/* ================================================================== */
/* init_gamemovie (0xF4) -- start a pre-rendered game movie.          */
/* ================================================================== */
void init_gamemovie(s32 type)
{
    gGameMode = 16398;
    movieactive = 0;
    AudioStopSelect();
    AudioSelectReset();
    MBOX_ResetUnlockedModels(2);
    delete_map_blits();
    movie_state = 1;
    if (lbl_803449A4 == 0 &&
        (gControllerButtons & 0) == 0 &&
        (sFlags & 16) == 0 &&
        lbl_8034481C == 0) {
        if (type == 44) {
            PlayVQMovie("victory");
            movieactive = 0;
            movie_state = 2;
        } else if (type == 43) {
            PlayVQMovie("garm");
            movieactive = 0;
            movie_state = 2;
        } else if (*((s8*)gCurLevel + 52) != 0) {
            PlayVQMovie((char*)gCurLevel + 52);
            movieactive = 0;
        }
    }
    kill_gamemovie = 0;
}

/* ================================================================== */
/* delete_map_blits (0xD8) -- free every blit built for the map.      */
/* ================================================================== */
/* NonMatching: 54/54 insns, opcodes match; target folds the +64 read into an
 * lwzu (update form) and reuses one fewer GPR -- a scheduling/regalloc residual. */
void delete_map_blits(void)
{
    u8* base = lbl_8023DFD0;
    s32 i;

    for (i = 0; i < 4; i++) {
        if (*(void**)(base + i * 4 + 64) != 0) {
            MBRemoveBlit(*(void**)(base + i * 4 + 64));
            MBRemoveBlit(*(void**)(base + i * 4 + 80));
        }
        *(void**)(base + i * 4 + 64) = 0;
        *(void**)(base + i * 4 + 80) = 0;
    }
    if (map_route_blit != 0) {
        MBRemoveBlit(map_route_blit);
    }
    map_route_blit = 0;
    for (i = 0; i < 8; i++) {
        if (*(void**)(base + i * 4 + 96) != 0) {
            MBRemoveBlit(*(void**)(base + i * 4 + 96));
        }
        *(void**)(base + i * 4 + 96) = 0;
    }
    for (i = 0; i < 4; i++) {
        del_player_blits(i);
    }
}

/* ================================================================== */
/* CaptionTextReset (0x18) -- reset the caption scroller.             */
/* ================================================================== */
void CaptionTextReset(void)
{
    caption_line = 0;
    caption_page = 0;
    caption_timer = 60;
}

/* ================================================================== */
/* CaptionText (0xF8) -- lay out and advance a caption block.         */
/* ================================================================== */
s32 CaptionText(char* a, char* b, s32 line, s32 page, s32 flags)
{
    f32 w = 0.6669999957f;
    s32 n;
    s32 measured;
    s32 tmp;

    if (line >= 0) {
        caption_line = line;
        caption_page = 0;
    }
    n = GetScrollText(a, b, caption_line, &tmp);
    w = w * GetScrollScale(a, b, caption_line);
    measured = CaptionTextSub(n, w, caption_page, flags, page);
    if (measured == 0) {
        return 0;
    }
    if (line < 0) {
        s32 total = ScrollTextNum(a, b);
        if (caption_line + 1 < total) {
            caption_timer = caption_timer - lbl_80344578;
            if (caption_timer <= 0) {
                caption_line = caption_line + 1;
                caption_page = page;
                caption_timer = 60;
            }
        }
        return 0;
    }
    return measured;
}

/* ================================================================== */
/* CaptionTextSub (0x2C4) -- draw one measured caption line.          */
/* Structural best-effort; NonMatching.                               */
/* ================================================================== */
s32 CaptionTextSub(s32 x, f32 w, s32 y, s32 page, s32 flags)
{
    u8* base = lbl_8023DFD0;
    (void)base;
    (void)x;
    (void)w;
    (void)y;
    (void)page;
    (void)flags;
    gGameMode = 16398;
    return 1;
}

/* ================================================================== */
/* do_mapscreen (0x410) -- per-frame map / level-load state machine.  */
/* Structural best-effort; NonMatching.                               */
/* ================================================================== */
s32 do_mapscreen(s32 skip)
{
    u8* base = lbl_8023DFD0;
    s32 done = 1;

    switch (map_load_state) {
    case 0:
        if (skip == 0) {
            AudioEnterNextStage();
        }
        map_load_state++;
        break;
    case 1:
        if (map_load_delay != 0) {
            map_load_delay--;
        } else if (fn_80055F68(1, 0) != 0) {
            map_load_state++;
        }
        break;
    case 2:
        if (sndFxUpdate(10) == 0) {
            map_load_state++;
        }
        break;
    case 3:
        AudioRegisterNameBanks(*(void**)((u8*)gCurLevel + 100), 1);
        map_load_state++;
        break;
    case 4:
        if (AudioSysUpdate(10) == 0) {
            map_load_state++;
        }
        break;
    default:
        map_load_state = 99;
        break;
    }

    map_load_progress += gFrameTicks;
    (void)done;
    return 0;
}

/* ================================================================== */
/* init_mapscreen (0x2E4) -- build all blits for the map screen.      */
/* Structural best-effort; NonMatching.                               */
/* ================================================================== */
s32 init_mapscreen(s32 timer, s32 movie)
{
    u8* base = lbl_8023DFD0;
    s32 i;
    s32 rv;

    AudioEmptyCb2();
    MapMusicStart();
    next_world();
    gGameMode = 16399;
    fn_80053D08(-2, 1, -1);
    for (i = 0; i < 4; i++) {
        setup_player_display(i);
    }
    rv = init_next_level(sLastWorldLevel);

    map_load_timer = timer;
    map_load_progress = 0;
    map_load_delay = 2;
    map_load_state = 0;
    map_route_blit = 0;
    map_fade_frame = 0;
    map_fade_alpha = 0;

    for (i = 0; i < 8; i++) {
        *(s32*)(base + i * 4 + 96) = 0;
    }
    fn_80053C70();
    (void)movie;
    return rv;
}
