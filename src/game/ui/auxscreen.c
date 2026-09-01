#include "types.h"
#include "game/gamemode.h"
#include "game/leveldata.h"
#include "game/mbobject.h"
#include "game/player.h"

#ifndef offsetof
#define offsetof(type, memb) ((u32) & ((type*)0)->memb)
#endif

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
extern Player gPlayers[4]; /* stride 0x335C (game/player.h) */
/* Big aux-screen scene object (blits + good-wizard model), engine owned. */
extern u8 lbl_8023DFD0[];
/* Base vec3 used to seed the averaged wizard position. */
extern f32 gBossPos[3];
/* .data tables referenced by the wizard/movie logic. */
extern f32 gIdentityMatrix[];
extern u8 lbl_80118250[];
/* Pooled rodata format/name strings (map-screen blit builders). */
extern char lbl_801116F0[];   /* "%s..." level-name format table base */
extern char lbl_801116FC[];   /* "LOADING" glow-text string */
/* .sdata float tunables (sda21). */
extern f32 lbl_80345A08;      /* route-height threshold */
extern f32 lbl_80345A40;      /* loading glow-text depth */
extern f32 lbl_80345A98;      /* route blit depth (do_mapscreen) */
extern f32 lbl_80345A9C;      /* bg blit depth */
extern f32 lbl_80345AA0;      /* fg blit depth */
extern f32 lbl_80345AB4;      /* route blit depth (init) */
/* .sdata strings passed by address (sda21). */
extern char lbl_80345AA4[7];  /* "%s..." route-model format */
extern char lbl_80345AAC[5];  /* route-model suffix appended by strcat */
extern s32 gGameBusy;
/* Per-frame time deltas (engine globals in the .sbss window). */
extern s32 gFrameTicks;  /* integer frame delta */
extern s32 gClockStepTicks; /* caption frame delta */
extern f32 sMusicFadeBase;  /* float frame delta */
extern f32 gClockFrameStep;
/* Assorted engine handles read by DoGoodWizard/init_gamemovie. */
extern s32 sMusicTrackHi;
extern void* lbl_80344BD4;
extern void* lbl_80344BEC;
extern void* sItemFile1Buf;
extern s32 lbl_803449A4;
extern s64 gControllerButtons;
extern s32 sFlags;
extern s32 lbl_8034481C;
extern s32 sLastWorldLevel;
extern s32 gGameMode;
extern level_data* gCurLevel; /* game/leveldata.h */
extern s32 gLanguageId;

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
extern s32 GetBossBeatFlag(s32 p);
extern s32 GetBossNumRunes(s32 p);
extern void* AtreeMatch(void* atree, char* name, s32 flag);
extern void* AtreeInit(void* header, void* tree, s32 flags, s32 size);
extern void* MBNewNode(void* p, void* tbl, s32 a);
extern void MBNodeSetParent(void* a, void* b);
extern void calc_wizard_pos(f32* out);
extern void CopyMat4(void* dst, void* src);
extern void add_target(void* p);
extern void calc_good_wiz_attn(s32 reset, s32 force);
extern s32 hide_rune_stones(void* p);
extern void MBTreeSetAlpha(void* p, s32 alpha, s32 a);
extern void AnimateATree(void* p, s32 a, s32 b);
extern void AudioGoodWizard(s32 speech, s32 arg);
extern s32 CaptionText(s32 a, s32 id, s32 idx, s32 frame, s32 flags);
extern s32 fn_800629B0(void);
extern s32 sndFxUpdate(s32 a);
extern void SetSkinFX(void* fx, void* tex, f32 rate, s32 a, s32 b); /* was mislabeled StartFXMat */
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
extern char* GetScrollText(s32 a, s32 b, s32 line, s32* out);
extern f32 GetScrollScale(s32 a, s32 b, s32 line);
extern s32 CaptionTextSub(char* text, f32 scale, s32 font, s32 rows, s32 y);
extern s32 ScrollTextNum(s32 a, s32 b);
extern void AudioEnterNextStage(void);
extern s32 fn_80055F68(s32 a, s32 b);
extern s32 AudioSysUpdate(s32 a);
extern void* AudioRegisterNameBanks(void* p, s32 a);
extern s32 sprintf(char* buf, const char* fmt, ...);
extern s32 MBOX_FindTexture_Sub(char* name, s32* p, s32 a, s32 b, s32 c);
extern void AudioMapDot(void);
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
extern char* strcpy(char* dst, char* src);
extern char* strchr(char* str, s32 ch);
extern void FontSetShadowColor(s32 color);
extern s32 DrawNormalText(f32 scale, char* text, s32 font);
extern void DrawTextKeepScale(f32 scale, s32 x, s32 y, s32 font, u32 color,
                              char* text);

/* Forward prototypes (helpers defined after their first caller). */
s32 do_gamemovie(void);

/* ================================================================== */
/* DoGoodWizard -- the on-map "good wizard" state machine (0x8D4).     */
/* Structural best-effort; NonMatching.                                */
/* ================================================================== */
extern s32 gBossType;
extern const f64 lbl_803459F0;
extern const f64 lbl_803459F8;
extern char lbl_80345A00[8];
extern const f32 lbl_80345A0C;
extern const f32 lbl_80345A10;
extern const f64 lbl_80345A18;
extern const f64 lbl_80345A20;
extern const f64 lbl_80345A28;
extern const f64 lbl_80345A48;
extern const f32 lbl_80345A30;

/*
 * AuxSceneView -- file-local reconstruction of the big aux-screen scene
 * object at lbl_8023DFD0 ("base" below). No PDB source: this is an
 * engine-internal scratch/state blob combining transient sprintf/caption
 * text buffers with persistent per-player map-card blit slots and the
 * good-wizard model's cached transform/atree state. Named purely from the
 * access patterns in this TU (each field's owning call site is noted);
 * unlabeled gaps are genuine unknowns never touched here, kept as explicit
 * padding so offsetof() folds to the exact byte displacements the target
 * uses (claim.law.offsetof-rename-preserves-protected-web). The [0,0x40)
 * scratch region and the [0x80,0x480) caption region are reused for
 * different transient strings at different times -- this is a raw byte
 * blob, not evidence of two co-resident typed fields.
 */
typedef struct AuxSceneView {
    /* 0x000 */ u8 path_scratch[0x40];        /* sprintf scratch: level/texture name strings (do_mapscreen, init_mapscreen) */
    /* 0x040 */ void* player_card_blit[4];    /* per-player map-card blit (init_mapscreen/delete_map_blits) */
    /* 0x050 */ void* player_card_fg_blit[4]; /* per-player map-card foreground overlay blit */
    /* 0x060 */ void* map_route_icon_blit[8]; /* route waypoint icon blits (do_mapscreen) */
    /* 0x080 */ u8 caption_scratch[0x400];    /* CaptionTextSub input scan buffer */
    /* 0x480 */ u8 caption_line_buf[0x108];   /* CaptionTextSub measured output line */
    /* 0x588 */ f32 wiz_mtx[4][4];            /* good-wizard model world matrix (CopyMat4/CreatePYRMatrix dst) */
    /* 0x5C8 */ f32 wiz_pos[3];               /* cached wizard node position (copied from node->mat row 3) */
    /* 0x5D4 */ u8 _pad5D4[4];
    /* 0x5D8 */ f32 wiz_target_pos[3];        /* camera-target position (add_target) */
    /* 0x5E4 */ u8 _pad5E4[4];
    /* 0x5E8 */ void* wiz_node_ptr;           /* good-wizard MBObject node */
    /* 0x5EC */ s32 field_5EC;                /* zeroed with the node; purpose unknown */
    /* 0x5F0 */ void* wiz_atree;              /* good-wizard atree handle (AtreeInit result) */
    /* 0x5F4 */ u8 _pad5F4[0x34];
    /* 0x628 */ u16 wiz_atree_ready;          /* set 1 once the atree is built */
} AuxSceneView;

#pragma opt_lifetimes off
void DoGoodWizard(void)
{
    u8* base = lbl_8023DFD0;
    s32 acc3542 = 0;
    s32 quality;
    s32 acc3540 = 0;
    s32 want;
    s32 cap;
    s32 c;
    s32 frame2;
    f32 pos[3];
    u8 unused[8];
    void* node;

    {
        s32 i;
        s32 off;

        for (i = 0, off = 0; i < 4; i++, off += sizeof(Player)) {
            u8* p = (u8*)gPlayers + off;

            if (*(s32*)(p + offsetof(Player, state)) == 1) {
                u8* slot = p + *(s32*)(p + offsetof(Player, character)) *
                                   sizeof(PlayerCharSave);

                acc3542 |= *(u16*)(slot + offsetof(Player, char_save) +
                                    offsetof(PlayerCharSave, rune_stones2));
                acc3540 |= *(u16*)(slot + offsetof(Player, char_save) +
                                    offsetof(PlayerCharSave, rune_stones));
            }
        }
    }
    quality = GetBossBeatFlag(sMusicTrackHi);
    want = GetBossNumRunes(sMusicTrackHi);
    if (quality == (quality & acc3542)) {
        if (want > 1) {
            quality = 3;
        } else {
            quality = 2;
        }
    } else if (acc3542 & quality) {
        quality = 1;
    } else {
        quality = 0;
    }
    switch (good_wiz_state) {
    case 1:
        if (gBossType == 0x2a || gBossType == 0x2c) {
            good_wiz_timer = (f32)(lbl_803459F0 + sMusicFadeBase);
        } else {
            good_wiz_timer = (f32)(lbl_803459F8 + sMusicFadeBase);
        }
        good_wiz_state++;
    case 2:
        if (sMusicFadeBase >= good_wiz_timer) {
            good_wiz_state++;
        }
        break;
    case 3: {
        void** pp;

        *(void**)(base + offsetof(AuxSceneView, wiz_atree)) =
            AtreeInit(AtreeMatch(sItemFile1Buf, lbl_80345A00, 1),
                      base + offsetof(AuxSceneView, wiz_atree), 0, 0x881880);
        *(u16*)(base + offsetof(AuxSceneView, wiz_atree_ready)) = 1;
        node = MBNewNode(lbl_80344BD4, gIdentityMatrix, 1);
        pp = (void**)(base + offsetof(AuxSceneView, wiz_node_ptr));
        *pp = node;
        MBNodeSetParent(**(void***)(base + offsetof(AuxSceneView, wiz_atree)),
                         *pp);
        *(s32*)(base + offsetof(AuxSceneView, field_5EC)) = 0;
        if ((u32)(gBossType - 0x2a) <= 1) {
            pos[0] = lbl_80345A08;
            pos[1] = lbl_80345A0C;
            pos[2] = lbl_80345A10;
        } else {
            calc_wizard_pos(pos);
            pos[1] = (f32)(pos[1] + lbl_80345A18);
        }
        node = *pp;
        *(f32*)((u8*)node + offsetof(MBObject, mat[3][0])) = pos[0];
        node = *pp;
        *(f32*)((u8*)node + offsetof(MBObject, mat[3][1])) = pos[1];
        node = *pp;
        *(f32*)((u8*)node + offsetof(MBObject, mat[3][2])) = pos[2];
        node = *pp;
        CopyMat4(node, base + offsetof(AuxSceneView, wiz_mtx));
        node = *pp;
        *(f32*)(base + offsetof(AuxSceneView, wiz_pos[0])) =
            *(f32*)((u8*)node + offsetof(MBObject, mat[3][0]));
        node = *pp;
        *(f32*)(base + offsetof(AuxSceneView, wiz_pos[1])) =
            *(f32*)((u8*)node + offsetof(MBObject, mat[3][1]));
        node = *pp;
        *(f32*)(base + offsetof(AuxSceneView, wiz_pos[2])) =
            *(f32*)((u8*)node + offsetof(MBObject, mat[3][2]));
        *(f32*)(base + offsetof(AuxSceneView, wiz_pos[1])) =
            (f32)(*(f32*)(base + offsetof(AuxSceneView, wiz_pos[1])) +
                  lbl_803459F0);
        *(f32*)(base + offsetof(AuxSceneView, wiz_target_pos[0])) =
            *(f32*)(base + offsetof(AuxSceneView, wiz_pos[0]));
        *(f32*)(base + offsetof(AuxSceneView, wiz_target_pos[1])) =
            *(f32*)(base + offsetof(AuxSceneView, wiz_pos[1]));
        *(f32*)(base + offsetof(AuxSceneView, wiz_target_pos[2])) =
            *(f32*)(base + offsetof(AuxSceneView, wiz_pos[2]));
        if (gBossType < 0x2a) {
            add_target(base + offsetof(AuxSceneView, wiz_mtx));
        }
        good_wiz_yaw = lbl_80345A08;
        good_wiz_plyr_attn = sMusicTrackHi % 4;
        calc_good_wiz_attn(1, 1);
        all_rune_stones = 0;
        if (gBossType == 0x2a) {
            all_rune_stones = hide_rune_stones(
                base + offsetof(AuxSceneView, caption_line_buf) + 0x40);
        }
        good_wiz_alpha = 255;
        good_wiz_state++;
    }
    case 4:
        if (good_wiz_alpha > 0) {
            good_wiz_alpha -= 4;
        }
        if (good_wiz_alpha < 0) {
            good_wiz_alpha = 0;
        }
        MBTreeSetAlpha(**(void***)(base + offsetof(AuxSceneView, wiz_atree)),
                        good_wiz_alpha, 1);
        AnimateATree(base + offsetof(AuxSceneView, wiz_atree), 0, 0);
        calc_good_wiz_attn(0, 0);
        if (good_wiz_alpha != 0) {
            break;
        }
        good_wiz_speech_idx = 0;
        good_wiz_state++;
        good_wiz_speech_frame = 0;
        good_wiz_speech_pause = 0;
        if (gBossType >= 0x2a) {
            AudioGoodWizard(gBossType, all_rune_stones);
        } else {
            AudioGoodWizard(gBossType, 0);
        }
    case 5:
        c = good_wiz_speech_idx;
        good_wiz_speech_frame += gFrameTicks;
        if (c >= 0) {
            frame2 = good_wiz_speech_frame >> 1;
            switch (gBossType) {
            case 0x23:
                cap = 0x93;
                break;
            case 0x22:
                cap = 0x94;
                break;
            case 0x24:
                cap = 0x95;
                break;
            case 0x25:
                cap = 0x96;
                break;
            case 0x29:
                cap = 0x99;
                break;
            case 0x27:
                cap = 0x97;
                break;
            case 0x28:
                cap = 0x9a;
                break;
            case 0x26:
                cap = 0x98;
                break;
            case 0x2a:
                cap = 0x9f;
                break;
            case 0x2b:
                cap = 0xa2;
                break;
            case 0x2c:
                cap = 0xa3;
                break;
            default:
                cap = -1;
                break;
            }
            if (cap >= 0) {
                frame2 = CaptionText(-1, cap, c, frame2, 0x10);
                if (frame2 > 0) {
                    if (good_wiz_speech_pause < 60) {
                        good_wiz_speech_pause += gFrameTicks;
                    } else {
                        good_wiz_speech_pause = 0;
                        good_wiz_speech_idx++;
                        good_wiz_speech_frame = 0;
                        calc_good_wiz_attn(0, 1);
                    }
                } else if (frame2 < 0) {
                    good_wiz_speech_idx = -1;
                    good_wiz_timer = (f32)(lbl_80345A20 + sMusicFadeBase);
                }
            } else {
                good_wiz_speech_idx = -1;
                good_wiz_timer = sMusicFadeBase;
            }
        } else if (sMusicFadeBase >= good_wiz_timer) {
            good_wiz_speech_idx = 0;
            good_wiz_state++;
            good_wiz_speech_frame = 0;
            good_wiz_speech_pause = 0;
            if (gBossType < 0x2a) {
                AudioGoodWizard(gBossType, quality + 1);
            }
        }
        AnimateATree(base + offsetof(AuxSceneView, wiz_atree), 0, 0);
        calc_good_wiz_attn(0, 0);
        break;
    case 6:
        c = good_wiz_speech_idx;
        good_wiz_speech_frame += gFrameTicks;
        if (c >= 0) {
            frame2 = good_wiz_speech_frame >> 1;
            switch (gBossType) {
            case 0x22:
            case 0x23:
            case 0x24:
            case 0x25:
            case 0x26:
            case 0x27:
            case 0x28:
            case 0x29:
                if (quality == 0) {
                    cap = 0x9b;
                } else if (quality == 1) {
                    cap = 0x9c;
                } else if (quality == 2) {
                    cap = 0x9d;
                } else {
                    cap = 0x9e;
                }
                break;
            case 0x2a:
                if (all_rune_stones) {
                    cap = 0xa0;
                } else {
                    cap = 0xa1;
                }
                break;
            default:
                cap = -1;
                break;
            }
            if (cap >= 0) {
                frame2 = CaptionText(-1, cap, c, frame2, 0x10);
                if (frame2 > 0) {
                    if (good_wiz_speech_pause < 60) {
                        good_wiz_speech_pause += gFrameTicks;
                    } else {
                        good_wiz_speech_pause = 0;
                        good_wiz_speech_idx++;
                        good_wiz_speech_frame = 0;
                        calc_good_wiz_attn(0, 1);
                    }
                } else if (frame2 < 0) {
                    good_wiz_speech_idx = -1;
                    good_wiz_timer = (f32)(lbl_80345A28 + sMusicFadeBase);
                }
            } else {
                good_wiz_speech_idx = -1;
                good_wiz_timer = sMusicFadeBase;
            }
        } else if (sMusicFadeBase >= good_wiz_timer) {
            s32 boss;

            good_wiz_speech_idx = 0;
            good_wiz_state++;
            good_wiz_speech_frame = 0;
            good_wiz_speech_pause = 0;
            boss = gBossType;
            if (boss < 0x2a) {
                s32 sel = 5;

                if ((acc3540 & 0x3FE) == 0x3FE && acc3542 == 0xFFF) {
                    sel = 7;
                } else if ((acc3540 & 0x1FE) == 0x1FE) {
                    sel = 6;
                }
                AudioGoodWizard(boss, sel);
            }
        }
        AnimateATree(base + offsetof(AuxSceneView, wiz_atree), 0, 0);
        calc_good_wiz_attn(0, 0);
        break;
    case 7:
        good_wiz_speech_frame += gFrameTicks;
        if (good_wiz_speech_idx >= 0) {
            good_wiz_speech_idx = -1;
            good_wiz_timer = sMusicFadeBase;
        } else if (sMusicFadeBase >= good_wiz_timer) {
            good_wiz_speech_idx = 0;
            good_wiz_state++;
            good_wiz_speech_frame = 0;
            good_wiz_speech_pause = 0;
        }
        AnimateATree(base + offsetof(AuxSceneView, wiz_atree), 0, 0);
        calc_good_wiz_attn(0, 0);
        break;
    case 8:
        if (fn_800629B0() != 0 || all_rune_stones != 0) {
            good_wiz_exit_timer = WizDelayGoldLeft;
        } else {
            good_wiz_exit_timer = WizDelayNoGold;
        }
        good_wiz_state++;
    case 9:
        AnimateATree(base + offsetof(AuxSceneView, wiz_atree), 0, 0);
        calc_good_wiz_attn(0, 0);
        if (sndFxUpdate(1) == 0) {
            good_wiz_state++;
        }
        break;
    case 10:
        AnimateATree(base + offsetof(AuxSceneView, wiz_atree), 0, 0);
        calc_good_wiz_attn(0, 0);
        if (good_wiz_exit_timer <= wiz_exit_min) {
            s32 off;
            s32 i;

            for (i = 0, off = 0; i < 4; i++, off += sizeof(Player)) {
                u8* p = (u8*)gPlayers + off;

                if (*(s32*)(p + offsetof(Player, state)) == 1) {
                    /* +0x7dc falls inside Player's unmapped pad_07A4 gap
                     * (delta 0x38); no GC-verified field covers it, left
                     * raw per AGENTS.md. */
                    SetSkinFX(p + 0x7dc, lbl_80344BEC, lbl_80345A30, 10, 1);
                }
            }
            good_wiz_state++;
        }
        break;
    case 11:
        AnimateATree(base + offsetof(AuxSceneView, wiz_atree), 0, 0);
        calc_good_wiz_attn(0, 0);
        break;
    case 0:
    default:
        good_wiz_enabled = 0;
        break;
    }
    if (good_wiz_state >= 9 && fn_800629B0() == 0) {
        good_wiz_exit_timer = (WizDelayNoGold < good_wiz_exit_timer)
                                  ? WizDelayNoGold
                                  : good_wiz_exit_timer;
    }
}
#pragma opt_lifetimes reset
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
    s32 j;
    s32 acc = 0;
    s32 i;

    for (i = 0; i < 4; i++) {
        u8* p = (u8*)gPlayers + i * sizeof(Player);
        if (*(s32*)(p + offsetof(Player, state)) == 1) {
            u8* slot = p + *(s32*)(p + offsetof(Player, character)) *
                               sizeof(PlayerCharSave);
            acc |= *(u16*)(slot + offsetof(Player, char_save) +
                            offsetof(PlayerCharSave, rune_stones2));
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
    u8 unused[16];
    f32 mtxbuf[3];
    u8 unused2[16];

    if (force == 0 && good_wiz_plyr_attn >= 0) {
        u8* p = (u8*)gPlayers;
        p += good_wiz_plyr_attn * sizeof(Player);
        if (*(s32*)(p + offsetof(Player, state)) == 1) {
            goto have_target;
        }
    }
    {
        s32 tries;
        s32 found = 0;
        for (tries = 4; tries != 0; tries--) {
            if ((good_wiz_plyr_attn = good_wiz_plyr_attn + 1) >= 4) {
                good_wiz_plyr_attn = 0;
            }
            if (*(s32*)((u8*)gPlayers + good_wiz_plyr_attn * sizeof(Player) +
                        offsetof(Player, state)) == 1) {
                break;
            }
            found++;
        }
        if (found == 4) {
            return;
        }
    }

have_target:
    {
        u8* p = (u8*)gPlayers;
        f32 dx;
        f32 dz;
        p += good_wiz_plyr_attn * sizeof(Player);
        dx = *(f32*)(p + offsetof(Player, pos[0])) -
             *(f32*)(base + offsetof(AuxSceneView, wiz_mtx[3][0]));
        dz = *(f32*)(p + offsetof(Player, pos[2])) -
             *(f32*)(base + offsetof(AuxSceneView, wiz_mtx[3][2]));
        target = atan2(dx, dz);
    }
    delta = target - good_wiz_yaw;
    if (reset == 0) {
        f32 lim = (f32)(1.5707963267948966 * (double)gClockFrameStep);
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
    CreatePYRMatrix(base + offsetof(AuxSceneView, wiz_mtx), mtxbuf);
    UpdateObjWorldMat(base + offsetof(AuxSceneView, wiz_mtx));
}

/* ================================================================== */
/* calc_wizard_pos (0xDC) -- averaged position of the active players. */
/* ================================================================== */
void calc_wizard_pos(f32* out)
{
    u8* arr = (u8*)gPlayers;
    f32 count = lbl_80345A40;
    s32 i;

    out[0] = gBossPos[0];
    out[1] = gBossPos[1];
    out[2] = gBossPos[2];

    for (i = 0; i < 4; i++) {
        s32 st;
        u8* p = arr + i * sizeof(Player);
        st = *(s32*)(p + offsetof(Player, state));
        if (st == 1 || st == 8) {
            f32* py = (f32*)(p + offsetof(Player, pos[1]));
            out[0] = out[0] + *(f32*)(p + offsetof(Player, pos[0]));
            out[1] = out[1] + *(f32*)(p + offsetof(Player, pos[1]));
            out[2] = out[2] + *(f32*)(p + offsetof(Player, pos[2]));
            if (lbl_80345A48 == count) {
                out[1] = (f32)(lbl_80345A28 * (f64)*py);
            }
            count = (f32)(count + lbl_80345A48);
        }
    }
    for (i = 0; i < 3; i++) {
        out[i] = out[i] / count;
    }
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
s32 init_gamemovie(s32 type)
{
    gGameMode = MG_GAMEMOVIE;
    movieactive = 0;
    AudioStopSelect();
    AudioSelectReset();
    MBOX_ResetUnlockedModels(2);
    delete_map_blits();
    movie_state = 1;
    if (lbl_803449A4 == 0 &&
        (gControllerButtons & 16) == 0 &&
        lbl_8034481C == 0) {
        if (type == 44) {
            PlayVQMovie("victory");
            movieactive = 0;
            movie_state = 2;
        } else if (type == 43) {
            PlayVQMovie("garm");
            movieactive = 0;
            movie_state = 2;
        } else if (*((s8*)gCurLevel + offsetof(level_data, movie)) != 0) {
            PlayVQMovie((char*)gCurLevel + offsetof(level_data, movie));
            movieactive = 0;
        }
    }
    kill_gamemovie = 0;
    return movieactive;
}

/* ================================================================== */
/* delete_map_blits (0xD8) -- free every blit built for the map.      */
/* ================================================================== */
/* Keep the advancing blit pointer explicit so MWCC selects the retail lwzu
 * update form across the removal calls. */
#pragma opt_common_subs off
#pragma opt_propagation off
void delete_map_blits(void)
{
    u8* base = lbl_8023DFD0;
    u8* p;
    void* blit;
    void** other;
    s32 k;
    s32 i;

    for (i = 0; i < 4; i++) {
        p = base + i * 4;
        blit = *(void**)(p += offsetof(AuxSceneView, player_card_blit));
        if (blit != 0) {
            MBRemoveBlit(blit);
            other = (void**)(base + i * 4);
            MBRemoveBlit(other[20]);
        }
        *(void**)p = 0;
        other = (void**)(base + i * 4);
        other[20] = 0;
    }
    if (map_route_blit != 0) {
        MBRemoveBlit(map_route_blit);
    }
    map_route_blit = 0;
    for (i = 0; i < 8; i++) {
        p = base + i * 4;
        blit = *(void**)(p += offsetof(AuxSceneView, map_route_icon_blit));
        if (blit != 0) {
            MBRemoveBlit(blit);
        }
        *(void**)p = 0;
    }
    for (k = 0; k < 4; k++) {
        del_player_blits(k);
    }
}
#pragma opt_propagation reset
#pragma opt_common_subs reset

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
#pragma opt_lifetimes off
s32 CaptionText(s32 a, s32 id, s32 idx, s32 frame, s32 flags)
{
    f32 w = 0.6669999957f;
    s32 n;
    s32 tmp;

    if (idx >= 0) {
        caption_line = idx;
        caption_page = 0;
    }
    n = (s32)GetScrollText(a, id, caption_line, &tmp);
    w = w * GetScrollScale(a, id, caption_line);
    n = CaptionTextSub((char*)n, w, tmp, frame, flags);
    if (n == 0) {
        goto done;
    }
    if (idx >= 0) {
        goto done;
    }
    {
        s32 total = ScrollTextNum(a, id);
        if (caption_line + 1 < total) {
            caption_timer = caption_timer - gClockStepTicks;
            if (caption_timer <= 0) {
                caption_line = caption_line + 1;
                caption_page = frame;
                caption_timer = 60;
            }
            n = 0;
        }
    }
done:
    return n;
}
#pragma opt_lifetimes reset

/* ================================================================== */
/* CaptionTextSub (0x2C4) -- draw one measured caption line.          */
/* Structural best-effort; NonMatching.                               */
/* ================================================================== */
#pragma opt_common_subs off
s32 CaptionTextSub(char* text, f32 scale, s32 font, s32 rows, s32 y)
{
    AuxSceneView* base = (AuxSceneView*)lbl_8023DFD0;
    char* source;
    char* line;
    f32 line_height = 32.0f;
    f32 remaining = (f32)(rows - caption_page);
    f32 glyph_width = 1.75f;
    s32 draw_font;
    s32 output_len = 0;
    s32 done = 0;
    s8 ch;
    s32 color_base;

    if (text == 0) {
        return -1;
    }
    if (remaining < 0.0) {
        remaining = 0.0f;
    }
    if (gLanguageId == 1) {
        glyph_width = 5.5f;
    }
    line_height *= scale;

    base->caption_line_buf[0] = 0;
    strcpy((char*)base->caption_scratch, text);
    source = (char*)base->caption_scratch;
    line = source;
    FontSetShadowColor(0);
    draw_font = font | 0x100;
    color_base = 0x1000000;

    if (gGameMode != MA_MOVIE && remaining > 0.0) {
        do {
            ch = *source++;

            switch (ch) {
            case '\r':
                base->caption_line_buf[output_len] = 0;
                remaining = (f32)((f64)remaining - 30.0);
                if (remaining < 0.0f) {
                    break;
                }
                base->caption_line_buf[0] = 0;
                output_len = 0;
                if (*source == '\n') {
                    source++;
                }
                line = source;
                break;

            case '\0':
            case '\n':
                base->caption_line_buf[output_len] = 0;
                if (output_len > 0) {
                    s32 width;
                    s32 x;
                    width = DrawNormalText(scale, (char*)base->caption_line_buf, font);
                    x = 256 - (width >> 1);
                    DrawTextKeepScale(scale, x, y, draw_font,
                                      color_base - 1, (char*)base->caption_line_buf);
                }
                line = source;
                output_len = 0;
                base->caption_line_buf[0] = 0;
                y = (s32)((f32)y + line_height);
                break;

            case '\t':
                remaining = (f32)((f64)remaining - 5.0);
                break;

            case ',':
            case '.':
                remaining -= 2.0f;
                /* fall through */
            default:
                remaining -= glyph_width;
                base->caption_line_buf[output_len] = ch;
                output_len++;
                break;
            }

            if (ch == 0) {
                done = 1;
                break;
            }
        } while (gGameMode != MA_MOVIE && remaining > 0.0);
    }

    base->caption_line_buf[output_len] = 0;
    if (output_len > 0) {
        char* newline = strchr(line, '\n');
        s32 width;
        s32 x;

        if (newline != 0) {
            *newline = 0;
        }
        width = DrawNormalText(scale, line, font);
        x = 256 - (width >> 1);
        DrawTextKeepScale(scale, x, y, draw_font,
                          0xffffff, (char*)base->caption_line_buf);
    }
    return done;
}
#pragma opt_common_subs reset

/* ================================================================== */
/* do_mapscreen (0x410) -- per-frame map / level-load state machine.  */
/* Structural best-effort; NonMatching.                               */
/* ================================================================== */
s32 do_mapscreen(s32 skip)
{
    u8* base = lbl_8023DFD0;
    s32 done = 1;
    s32 i;
    s32 j;
    f32 thresh;
    u8 unused[8];

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
        AudioRegisterNameBanks(
            *(void**)((u8*)gCurLevel + offsetof(level_data, audio)), 1);
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
    if (skip == 0 && *(void**)((u8*)gCurLevel + offsetof(level_data, mapdata)) != 0) {
        thresh = lbl_80345A08;
        for (i = 0; i < 8; i++) {
            void* p104 = *(void**)((u8*)gCurLevel + offsetof(level_data, mapdata));
            f32* ent = p104 ? (f32*)((u8*)p104 + i * 8 + 8) : (f32*)0;
            void** row;
            if (ent == 0 || *ent < thresh) {
                i = 8;
                break;
            }
            row = (void**)(base + i * 4);
            if (row[24] == 0) {
                s32 tex;
                if (map_load_progress <= i * 30) {
                    break;
                }
                sprintf((char*)base, lbl_801116F0, (char*)gCurLevel + offsetof(level_data, name), i + 1);
                tex = MBOX_FindTexture_Sub((char*)base, 0, (s32)map_bg_blit,
                                           (s32)map_bg_blit, 1);
                row[24] = MBCreateBlit(0, tex, (s32)ent[0], (s32)ent[1], -1, -1);
                mbBlitCvtCoord(row[24], lbl_80345A98);
                AudioMapDot();
            }
        }
        if (map_route_blit != 0 && i == 8) {
            map_fade_frame += gFrameTicks;
            if (map_fade_frame > map_load_len) {
                s32 over = map_fade_frame - map_load_len;
                s32 clamped;
                if (over < map_load_step) {
                    done = 0;
                }
                map_fade_alpha = over << 2;
                clamped = map_fade_alpha;
                if (clamped < 0) {
                    clamped = 0;
                } else if (clamped > 255) {
                    clamped = 255;
                }
                map_fade_alpha = clamped;
                MBBlitSetAlpha(map_route_blit, map_fade_alpha);
                for (j = 0; j < 4; j++) {
                    void** frow = (void**)(base + j * 4);
                    MBBlitSetAlpha(frow[20], 255 - map_fade_alpha);
                }
                if (map_fade_alpha == 255) {
                    DrawGlowText(340, 320, lbl_801116FC, lbl_80345A40);
                } else {
                    map_load_timer = 60;
                }
            } else {
                s32 a = map_fade_a;
                s32 period = map_fade_b + a * 2;
                s32 rem = map_fade_frame % period;
                s32 alpha;
                s32 inv;
                if (rem > a * 2) {
                    rem = 0;
                } else if (rem > a) {
                    rem = a * 2 - rem;
                }
                alpha = (a + rem * 255 - 1) / a;
                if (alpha < 4) {
                    alpha = 4;
                } else if (alpha > 250) {
                    alpha = 250;
                }
                inv = 255 - alpha;
                mbBlitInit3414(map_route_blit, 0);
                MBBlitSetAlpha(map_route_blit, inv);
                map_load_timer = 60;
            }
        } else {
            map_load_timer = 60;
        }
    }

    map_load_timer -= gFrameTicks;
    if (gGameBusy == 0 && map_load_state == 99 && done != 0 &&
        map_load_timer <= 0) {
        AudioStopMusicB();
        return 1;
    }
    return 0;
}

/* ================================================================== */
/* init_mapscreen (0x2E4) -- build all blits for the map screen.      */
/* Structural best-effort; NonMatching.                               */
/* ================================================================== */
#pragma opt_common_subs off
static inline void setupMapForeground(void** slot, void* blit)
{
    slot[20] = blit;
    mbBlitCvtCoord(*(slot += 20), lbl_80345AA0);
    MBBlitSetAlpha(*slot, 255);
}

#pragma opt_common_subs reset

#pragma opt_propagation off
s32 init_mapscreen(s32 timer, s32 movie)
{
    u8* base = lbl_8023DFD0;
    char* fmt = lbl_801116F0;
    s32 i;
    s32 lvl;
    s32 rv;
    void* route;
    s32* ent;
    void* blit;
    u8 unused[16];

    AudioEmptyCb2();
    MapMusicStart();
    next_world();
    gGameMode = MG_MAPSCREEN;
    lvl = sLastWorldLevel;
    fn_80053D08(-2, 1, -1);
    for (i = 0; i < 4; i++) {
        setup_player_display(i);
    }
    rv = init_next_level(lvl);

    if (movie == 0 && *(void**)((u8*)gCurLevel + offsetof(level_data, mapdata)) != 0) {
        sprintf((char*)base, fmt + 24, (char*)gCurLevel + offsetof(level_data, name));
        map_bg_blit = LoadModel((char*)base, 0, 0, -1);
        for (i = 0; i < 4; i++) {
            sprintf((char*)base, fmt + 40, (char*)gCurLevel + offsetof(level_data, name), i);
            ent = (s32*)(lbl_80118250 + i * 8);
            blit = MBNewBlit(base, ent[0], ent[1]);
            *(void**)(base + i * 4 + offsetof(AuxSceneView, player_card_blit)) =
                blit;
            mbBlitCvtCoord(*(void**)(base + i * 4 +
                                      offsetof(AuxSceneView, player_card_blit)),
                            lbl_80345A9C);
            sprintf((char*)base, fmt + 52, (char*)gCurLevel + offsetof(level_data, name), i);
            blit = MBNewBlit(base, ent[0], ent[1]);
            setupMapForeground((void**)(base + i * 4), blit);
        }
    } else {
        void* blit = MBCreateBlit(0, (s32)MBOX_FindTexture(fmt + 68, 0),
                                  0, 0, 512, 320);
        *(void**)(base + offsetof(AuxSceneView, player_card_blit)) = blit;
        mbBlitCvtCoord(*(void**)(base + offsetof(AuxSceneView, player_card_blit)),
                        lbl_80345A9C);
        *(s32*)(base + offsetof(AuxSceneView, player_card_blit[3])) = 0;
        *(s32*)(base + offsetof(AuxSceneView, player_card_blit[2])) = 0;
        *(s32*)(base + offsetof(AuxSceneView, player_card_blit[1])) = 0;
        *(s32*)(base + offsetof(AuxSceneView, player_card_fg_blit[3])) = 0;
        *(s32*)(base + offsetof(AuxSceneView, player_card_fg_blit[2])) = 0;
        *(s32*)(base + offsetof(AuxSceneView, player_card_fg_blit[1])) = 0;
        *(s32*)(base + offsetof(AuxSceneView, player_card_fg_blit[0])) = 0;
    }

    map_load_timer = timer;
    map_load_progress = 0;
    map_load_delay = 2;
    map_load_state = 0;
    map_route_blit = 0;
    map_fade_frame = 0;
    map_fade_alpha = 0;

    route = *(void**)((u8*)gCurLevel + offsetof(level_data, mapdata));
    if (route != 0) {
        route = route;
    } else {
        route = 0;
    }
    if (movie == 0 && route != 0 && *(f32*)route >= lbl_80345A08) {
        sprintf((char*)base, lbl_80345AA4, (char*)gCurLevel + offsetof(level_data, name));
        strcat((char*)base, lbl_80345AAC);
        /* route points into level_data::mapdata's pointee (struct map_data),
         * forward-declared only -- no header exists for its internal layout
         * (do_mapscreen's parallel i*8+8-stride waypoint scan is the same
         * unresolved struct). Left raw: no GC- or PDB-verified field name
         * to adopt for this displacement. */
        map_route_blit = MBNewBlit(base, (s32)*(f32*)route,
                                   (s32)*(f32*)((u8*)route + 4));
        if (map_route_blit != 0) {
            mbBlitInit3414(map_route_blit, 1);
        }
        MBBlitSetAlpha(map_route_blit, 255);
        mbBlitCvtCoord(map_route_blit, lbl_80345AB4);
    }

    for (i = 0; i < 8; i++) {
        *(s32*)(base + i * 4 + offsetof(AuxSceneView, map_route_icon_blit)) = 0;
    }
    fn_80053C70();
    (void)movie;
    return rv;
}
#pragma opt_propagation reset
