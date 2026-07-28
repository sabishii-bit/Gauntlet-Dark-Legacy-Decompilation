#ifndef GAME_PLAYER_H
#define GAME_PLAYER_H

#include "types.h"

/*
 * Player record  --  the per-player game/save state.
 *
 * GC base global : lbl_80275AE0  (.bss @ 0x80275AE0), an array [4] of Player,
 *                  stride 0x335C.  Candidate name: gPlayerRecords.
 *                  (The DOL splitter broke the 0xCD70 = 4*0x335C array into the
 *                  BSS chunks lbl_80275AE0 / lbl_802764D1 / lbl_80278B8B; the
 *                  base of the whole array is lbl_80275AE0.)
 *
 * Name source    : research/xbox_symbols/misc.h -- the Xbox (shell3D.pdb) dump.
 *                  The Xbox in-game record is `struct player` (0x6140) and its
 *                  saved sub-blocks `struct P_SAVE` / `struct P_SAVE_STUFF`.
 *                  The GC record (0x335C) has a DIFFERENT, more compact layout,
 *                  so field OFFSETS below come from the GC target asm, not from
 *                  Xbox.  Xbox names are adopted where the semantics match.
 *
 * Offset verification (against build/GUNE5D/obj target asm):
 *   0x004 class_id     VERIFIED  sounds.c AudioWithName   lwz  r,4(base)
 *   0x008 char_type    VERIFIED  sounds.c (indexes name/portrait bank)
 *   0x00C character    VERIFIED  lwz r,12(base); mulli *240 selects char_save[]
 *   0x044 pos[3]       VERIFIED  auxscreen.c calc_wizard_pos  lfs f,68/72/76(base)
 *   0x0E8 state        VERIFIED  lwz r,232(base)  (0=none 1=active 2=...)
 *   0x124 flags        VERIFIED  lwz r,292(base); rlwinm. bit 0x400
 *   0xDD4 char_save[]  VERIFIED  hide_rune_stones: mulli type*240; lhzx @+2/+0
 *   0x1EB8/0x1EBC item VERIFIED  shopquery.c PlayerItemState  lwz r,7864/7868
 *   0x1EC8 runes /
 *   0x1ECA shards          documented (prior verified research)
 *   0x2220..0x223A         documented crystal/gargoyle/legend counters
 *
 * GC-vs-Xbox deltas of note:
 *   - Whole record is 0x335C on GC vs 0x6140 on Xbox (GC drops the in-game-only
 *     runtime fields such as OBJGRP/atree/actionlist/save_backup).
 *   - Per-character save slot is 0xF0 (240) on GC vs P_SAVE_STUFF 0x254 on Xbox
 *     (GC stores the per-slot powerup arrays elsewhere); the GC slot begins with
 *     rune-stone bitmasks rather than Xbox's potions/keys.
 *   - Xbox order plyr_color@4 / type@8 / alttype@0xc does not carry over; on GC
 *     +4 is used as a class index, +12 selects the 16-entry char_save[] slot
 *     (see enum e_p_type below).
 *
 * Only fields confirmed by asm or prior research are named; the gaps between
 * them are explicit padding so the layout stays byte-exact at 0x335C.
 */

/* Character identity for char_save[] and the audio/portrait tables
 * (research/xbox_symbols/misc.h : enum e_p_type). */
enum PlayerCharType {
    PLYR_WARRIOR   = 0,
    PLYR_VALKYRIE  = 1,
    PLYR_WIZARD    = 2,
    PLYR_ARCHER    = 3,
    PLYR_DWARF     = 4,
    PLYR_KNIGHT    = 5,
    PLYR_SORCERESS = 6,
    PLYR_JESTER    = 7,
    PLYR_MINOTAUR  = 8,
    PLYR_FALCONESS = 9,
    PLYR_JACKAL    = 10,
    PLYR_TIGRESS   = 11,
    PLYR_OGRE      = 12,
    PLYR_UNICORN   = 13,
    PLYR_MEDUSA    = 14,
    PLYR_HYENA     = 15
};

/*
 * Per-character progression slot.  Player.char_save is an array of 16 of these
 * (one per enum PlayerCharType); Player.character selects the active one.
 * Size 0xF0 (240) -- the stride proven by `mulli type,240` in hide_rune_stones.
 * Xbox analogue: struct P_SAVE_STUFF (different, larger 0x254 layout).
 */
typedef struct PlayerCharSave {
    /* 0x00 */ u16 rune_stones;      /* rune-stone collection bitmask (VERIFIED lhzx @+0) */
    /* 0x02 */ u16 rune_stones2;     /* second collection bitmask     (VERIFIED lhzx @+2) */
    /* 0x04 */ u16 rune_near;        /* documented (abs 0xDD8) */
    /* 0x06 */ u8  pad_06[2];
    /* 0x08 */ u16 level_masks[4];   /* level/boss-beaten bitmasks, documented (abs 0xDDC-0xDE2) */
    /* 0x10 */ u8  pad_10[4];
    /* 0x14 */ u16 completion1;      /* completion record, documented (abs 0xDE8) */
    /* 0x16 */ u8  pad_16[4];
    /* 0x1A */ u16 completion2;      /* completion record, documented (abs 0xDEE) */
    /* 0x1C */ u8  pad_1C[0xD4];     /* remainder of slot (unmapped) */
} PlayerCharSave;                    /* size 0xF0 */

/*
 * Fields marked [player.c] were offset-verified 2026-07-27 against the
 * PLAYER.OBJ front-slice target asm (displacement loads in the matched fns).
 */
typedef struct Player {
    /* 0x0000 */ s32 index;          /* player index (Xbox: player.index@0) */
    /* 0x0004 */ s32 class_id;       /* class index (VERIFIED: sounds cls, cls*16+type) */
    /* 0x0008 */ s32 char_type;      /* portrait/name-bank index (VERIFIED: sounds @+8) */
    /* 0x000C */ s32 character;      /* active PlayerCharType, selects char_save[] (VERIFIED @+12) */
    /* 0x0010 */ s32 respawn_char;   /* character shown while dead/selecting [player.c] */
    /* 0x0014 */ f32 mat[12];        /* world orient+pos matrix (fn_8005A338 arg) [player.c] */
    /* 0x0044 */ f32 pos[3];         /* world position x,y,z (VERIFIED lfs @68/72/76) */
    /* 0x0050 */ u8  pad_0050[4];
    /* 0x0054 */ f32 col_pos[3];     /* collision/screen query position [player.c] */
    /* 0x0060 */ u8  pad_0060[0x14];
    /* 0x0074 */ u8* node;           /* scene node (parent/grab reparent target) [player.c] */
    /* 0x0078 */ u8  pad_0078[4];
    /* 0x007C */ s32** platform;     /* moving-platform record [player.c do_players] */
    /* 0x0080 */ u8  pad_0080[0x20];
    /* 0x00A0 */ f32 select_timer;   /* advanced while select overlay is up [player.c] */
    /* 0x00A4 */ u8  pad_00A4[0x2C];
    /* 0x00D0 */ f32 beacon_pos[3];  /* platform-relative display pos [player.c] */
    /* 0x00DC */ f32 saved_pos[3];   /* pos saved across parent/grab [player.c] */
    /* 0x00E8 */ s32 state;          /* player state: 0=none 1=active 2=... (VERIFIED @232) */
    /* 0x00EC */ s32 prev_state;     /* last frame's state [player.c do_players] */
    /* 0x00F0 */ u8  pad_00F0[0x1C];
    /* 0x010C */ f32 magic_power;    /* potion magic power [player.c start_magic] */
    /* 0x0110 */ f32 light_range;    /* beacon light range [player.c do_players] */
    /* 0x0114 */ u8  pad_0114[0x10];
    /* 0x0124 */ u32 flags;          /* status flags; bit 0x400 tested (VERIFIED @292) */
    /* 0x0128 */ u8  pad_0128[0xC8];
    /* 0x01F0 */ s16 timer_1F0;      /* generic countdowns, dec by frame delta [player.c] */
    /* 0x01F2 */ u8  pad_01F2[2];
    /* 0x01F4 */ s16 respawn_timer;  /* state-0 display rebuild delay [player.c] */
    /* 0x01F6 */ u8  pad_01F6[2];
    /* 0x01F8 */ s16 name_timer;     /* overhead-name show timer [player.c] */
    /* 0x01FA */ s16 timer_1FA;
    /* 0x01FC */ s16 timer_1FC;
    /* 0x01FE */ s16 timer_1FE;
    /* 0x0200 */ s16 vibe_timer;     /* vibration accumulators [player.c] */
    /* 0x0202 */ s16 vibe_timer2;
    /* 0x0204 */ s32 vibe_on;
    /* 0x0208 */ s32 anim_208;       /* 0x7E handshake with action anim [player.c] */
    /* 0x020C */ s32 anim_20C;
    /* 0x0210 */ u8  pad_0210[0x5EC];
    /* 0x07FC */ f32 pulse_7FC;      /* rune-near display pulse [player.c] */
    /* 0x0800 */ u8  pad_0800[0x28];
    /* 0x0828 */ f32 power_target;   /* power-meter target [player.c] */
    /* 0x082C */ f32 power_level;    /* power-meter shown level [player.c] */
    /* 0x0830 */ u8  pad_0830[4];
    /* 0x0834 */ s32 quest_state;    /* quest icon: 0 off, 1 pending, 2 on [player.c] */
    /* 0x0838 */ f32 anchor_pos[3];  /* attach anchor (SetParent offset base) [player.c] */
    /* 0x0844 */ f32 anchor_fwd[3];  /* attach forward vector [player.c] */
    /* 0x0850 */ u8  pad_0850[8];
    /* 0x0858 */ f32 light_vec[3];   /* beacon light vector (decayed) [player.c] */
    /* 0x0864 */ f32 light_vel[3];   /* beacon light velocity [player.c] */
    /* 0x0870 */ u8  pad_0870[0x30];
    /* 0x08A0 */ f32 floor_hi;       /* clamped floor probe results [player.c] */
    /* 0x08A4 */ f32 floor_lo;
    /* 0x08A8 */ u8  pad_08A8[8];
    /* 0x08B0 */ s32* speech_req;    /* queued speech request [player.c do_players] */
    /* 0x08B4 */ u8  pad_08B4[0x10];
    /* 0x08C4 */ char* floor_name2;  /* fallback floor name [player.c debug] */
    /* 0x08C8 */ char* floor_name;   /* current floor name [player.c debug] */
    /* 0x08CC */ u8  pad_08CC[8];
    /* 0x08D4 */ u32 obj_flags;      /* world-obj flags; 0x4000 = parented [player.c] */
    /* 0x08D8 */ u8  pad_08D8[0x18];
    /* 0x08F0 */ s32 action;         /* current action id (PlayerAttacking) [player.c] */
    /* 0x08F4 */ u8  pad_08F4[0x28];
    /* 0x091C */ s32 count_91C;      /* per-frame countdown [player.c do_players] */
    /* 0x0920 */ s32 count_920;      /* exit-anim countdown [player.c do_players] */
    /* 0x0924 */ u8  pad_0924[4];
    /* 0x0928 */ s32 got_type;       /* last pickup: crystal/boss-item/coin id [player.c] */
    /* 0x092C */ f32 got_timer;      /* pickup ticker time left [player.c] */
    /* 0x0930 */ s32 got_count;      /* pickup running count [player.c] */
    /* 0x0934 */ s32 fall_frames;    /* fall grunt selector [player.c do_players] */
    /* 0x0938 */ f32 fall_time;      /* fall start timestamp [player.c do_players] */
    /* 0x093C */ u8  pad_093C[0x14];
    /* 0x0950 */ s16 idle_timer;     /* idle speech timer [player.c do_players] */
    /* 0x0952 */ u8  pad_0952[6];
    /* 0x0958 */ s16 throw_str;      /* potion-throw strength [player.c start_magic] */
    /* 0x095A */ u8  pad_095A[0xA];
    /* 0x0964 */ s16 hud_flags;      /* 0x20 = attached (lha in target) [player.c] */
    /* 0x0966 */ s16 hud_flags2;     /* 1 = info written, 2 = runes written [player.c] */
    /* 0x0968 */ u8  pad_0968[0xF8];
    /* 0x0A60 */ s32 display_mode;   /* HUD display mode (get_display_mode) [player.c] */
    /* 0x0A64 */ u8  pad_0A64[0x1C];
    /* 0x0A80 */ char name[8];       /* player name, underscore shown as space [player.c] */
    /* 0x0A88 */ u8  pad_0A88[0x34C];
    /* 0x0DD4 */ PlayerCharSave char_save[16]; /* per-character progression (VERIFIED base+stride) */
    /* 0x1CD4 */ u8  pad_1CD4[0x1E0];
    /* 0x1EB4 */ f32 health;         /* hit points, 9999 display cap [player.c] */
    /* 0x1EB8 */ s32 item_body_lo;   /* body-armor item flag (VERIFIED shopquery @7864) */
    /* 0x1EBC */ s32 item_body_hi;   /* body-armor item flag (VERIFIED shopquery @7868) */
    /* 0x1EC0 */ s32 exp;            /* experience points [player.c ModifyExp] */
    /* 0x1EC4 */ s32 gold;           /* gold, 99999 cap [player.c PlayerGiveGold] */
    /* 0x1EC8 */ u16 runes;          /* active-character rune count (documented) */
    /* 0x1ECA */ u16 shards;         /* active-character shard count (documented) */
    /* 0x1ECC */ u8  pad_1ECC[0x354];
    /* 0x2220 */ s16 crystals[8];    /* crystal counters (documented 0x2220..) */
    /* 0x2230 */ s16 gargoyle_items[3]; /* gargoyle-item counters (documented) */
    /* 0x2236 */ s16 legend_items[2];   /* legend-item counters (documented ..0x223A) */
    /* 0x223A */ u8  pad_223A[0x10EA];
    /* 0x3324 */ s32 level;          /* character level 1..99 [player.c] */
    /* 0x3328 */ s32 intower;        /* set while active in tower [player.c] */
    /* 0x332C */ u8  pad_332C[0xC];
    /* 0x3338 */ s32 motion_state;   /* motion/tower-exit state [player.c] */
    /* 0x333C */ u8  pad_333C[4];
    /* 0x3340 */ s32 meter_flash;    /* power-meter flash state [player.c] */
    /* 0x3344 */ s32 meter_timer;    /* power-meter flash timer [player.c] */
    /* 0x3348 */ u8  pad_3348[0x14];
} Player;                            /* size 0x335C */

#endif /* GAME_PLAYER_H */
