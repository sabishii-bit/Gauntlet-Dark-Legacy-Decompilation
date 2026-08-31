#ifndef GAME_PLAYER_H
#define GAME_PLAYER_H

#include "types.h"

struct worldobj; /* game/worldobj.h (pointer-only here) */

/*
 * Player record  --  the per-player game/save state.
 *
 * GC base global : gPlayers  (.bss @ 0x80275AE0), an array [4] of Player,
 *                  stride 0x335C.  Candidate name: gPlayerRecords.
 *                  (The DOL splitter broke the 0xCD70 = 4*0x335C array into the
 *                  BSS chunks gPlayers / lbl_802764D1 / lbl_80278B8B; the
 *                  base of the whole array is gPlayers.)
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
 * Active powerup slot (11 per player at +0x130).  GC-proven by
 * player_get_powerup_state / PlayerAddPowerup / PlayerProcessMikeyPUP
 * (type 9 mask 0x100000 = mikey, mask 8 = x-ray range feed).
 */
/* Field names are Midway's own (Xbox shell3D.pdb struct P_POWERUP).  Note the
 * two f32s are overloaded by powerup class: for timed buffs `timeleft` is the
 * real-time countdown (drained by gClockFrameStep) and `attributeadd` is 0; for
 * charge items `attributeadd` is the use/charge count (?1.0 per use) and
 * `timeleft` is pinned < 0 as a permanent/occupied flag. */
typedef struct PlayerPowerup {
    /* 0x00 */ f32 timeleft;         /* time remaining; 0 = free slot, < 0 = permanent */
    /* 0x04 */ s32 type;             /* powerup class (9 = flagged specials) */
    /* 0x08 */ f32 attributeadd;     /* stat-boost amount; also charge count for weapons */
    /* 0x0C */ u32 specialflags;     /* subtype/flags mask */
} PlayerPowerup;                     /* size 0x10 */

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
    /* 0x0060 */ u8  pad_0060[4];
    /* 0x0064 */ f32 effectpos[3];     /* effect/attach position [sfx.c grid pass] */
    /* 0x0070 */ u8  pad_0070[4];
    /* 0x0074 */ u8* node;           /* scene node (parent/grab reparent target) [player.c] */
    /* 0x0078 */ s32 field_078;      /* cleared beside p->node in load_player_geo /
                                      * remove_player_geo (VERIFIED: not padding --
                                      * teardown zero-block) [player.c] */
    /* 0x007C */ s32** platform;     /* moving-platform record [player.c do_players] */
    /* 0x0080 */ u8  pad_0080[0x18];
    /* 0x0098 */ f32 combo_time;     /* combo window timestamp read at the motion
                                      * phase exit [pmotion.c] */
    /* 0x009C */ u8  pad_009C[4];
    /* 0x00A0 */ f32 select_timer;   /* advanced while select overlay is up [player.c] */
    /* 0x00A4 */ u8  pad_00A4[0x20];
    /* 0x00C4 */ f32 angles[3];      /* orientation angles fed to CreateYPRMatrix(&mat,
                                      * &angles); angles[1] is the heading/yaw written
                                      * beside p->move_yaw (VERIFIED: pmotion.c passes
                                      * (f32*)(p+0xC4) as the second CreateYPRMatrix
                                      * argument, and stores `heading` at +0xC8).
                                      * pmotion.c's own sites stay raw on purpose: the
                                      * target shares one p+0xC4 base between the store
                                      * and the call argument, and any member form
                                      * splits that web (+45 insns in PlayerMotion --
                                      * identical delta for array-index, scalar-index
                                      * and three-scalar forms) [pmotion.c/player.c] */
    /* 0x00D0 */ f32 beacon_pos[3];  /* platform-relative display pos [player.c] */
    /* 0x00DC */ f32 saved_pos[3];   /* pos saved across parent/grab [player.c] */
    /* 0x00E8 */ s32 state;          /* player state: 0=none 1=active 2=... (VERIFIED @232) */
    /* 0x00EC */ s32 prev_state;     /* last frame's state [player.c do_players] */
    /* 0x00F0 */ char* hidden_code;  /* active hidden-char code ptr, cmp lbl_80343D6C [player.c] */
    /* 0x00F4 */ f32 att_fight;      /* attribute norms 0..999 (check_player_atts) [player.c] */
    /* 0x00F8 */ f32 att_armor;
    /* 0x00FC */ f32 att_magic;
    /* 0x0100 */ f32 att_speed;
    /* 0x0104 */ f32 stat_damage;    /* derived stats (PlayerUpdateAtts) [player.c] */
    /* 0x0108 */ f32 stat_armor;     /* absorbs in damage_player via ModifyDamage */
    /* 0x010C */ f32 magic_power;    /* potion magic power/radius (= derived magic stat) */
    /* 0x0110 */ f32 light_range;    /* derived speed stat (front slice used as light range) */
    /* 0x0114 */ f32 stat_missile_dmg;  /* missile stats; magic-based for classes 2/6 */
    /* 0x0118 */ f32 stat_missile_spd;
    /* 0x011C */ s32 field_11C;      /* cleared on load_player [player.c] */
    /* 0x0120 */ u32 shield_flags;   /* 0x110000 masks the low-health siren [player.c] */
    /* 0x0124 */ u32 flags;          /* status flags; bit 0x400 tested (VERIFIED @292) */
    /* 0x0128 */ s32 field_128;      /* cleared with the powerup table [player.c] */
    /* 0x012C */ s32 field_12C;
    /* 0x0130 */ PlayerPowerup powerup[11];  /* active powerup slots [player.c] */
    /* 0x01E0 */ u8  powerup_state[11];      /* 1 fresh, 2 use-requested, 3 used [player.c] */
    /* 0x01EB */ u8  pad_01EB[5];
    /* 0x01F0 */ s16 timer_1F0;      /* generic countdowns, dec by frame delta [player.c] */
    /* 0x01F2 */ s16 field_1F2;      /* cleared when the motion driver forces state 4 and
                                      * at both DoExit resets [pmotion.c] */
    /* 0x01F4 */ s16 respawn_timer;  /* state-0 display rebuild delay [player.c] */
    /* 0x01F6 */ s16 heartbeat_timer; /* low-health heartbeat countdown [player.c] */
    /* 0x01F8 */ s16 name_timer;     /* overhead-name show timer [player.c] */
    /* 0x01FA */ s16 timer_1FA;
    /* 0x01FC */ s16 timer_1FC;
    /* 0x01FE */ s16 timer_1FE;
    /* 0x0200 */ s16 vibe_timer;     /* vibration accumulators [player.c] */
    /* 0x0202 */ s16 vibe_timer2;
    /* 0x0204 */ s32 vibe_on;
    /* 0x0208 */ s32 anim_208;       /* 0x7E handshake with action anim [player.c] */
    /* 0x020C */ s32 anim_20C;
    /* 0x0210 */ u8  pad_0210[0x4A8];
    /* 0x06B8 */ struct Player* grab_partner; /* grabbed/carried partner [pmotion.c] */
    /* 0x06BC */ struct Player* grab_pending; /* pending grab request [pmotion.c] */
    /* 0x06C0 */ u8  pad_06C0[8];
    /* 0x06C8 */ void* mbnode;       /* secondary MBTree node (weapon/shadow) [pmotion.c] */
    /* 0x06CC */ void* mbnode2;      /* effect anchor node [pmotion.c] */
    /* 0x06D0 */ void* hand_node;    /* off-hand FX parent: shield objects, weapon-hold tree [player.c] */
    /* 0x06D4 */ void* weapon_node;  /* weapon FX parent: wand/gem objects, breath familiars [player.c] */
    /* 0x06D8 */ s32 field_6D8;      /* cleared with grab_node in the teardown block [player.c] */
    /* 0x06DC */ void* grab_node;    /* node handed to PlayerSetGrabbed as the carry parent;
                                      * stored from the model's node in load_player_geo and
                                      * zeroed beside it (VERIFIED: not padding -- teardown
                                      * store/clear pair) [pmotion.c/player.c] */
    /* 0x06E0 */ void* weaphold_node;   /* node hidden/shown with the weapon-hold FX tree [player.c] */
    /* 0x06E4 */ void* weaphold_atree;  /* weapon-hold FX atree handle [player.c] */
    /* 0x06E8 */ u8  pad_06E8[4];
    /* 0x06EC */ u32 weaphold_src_id;   /* weapon-hold source id, stale-tree check [player.c] */
    /* 0x06F0 */ u8  pad_06F0[0x3C];
    /* 0x072C */ void* pup_object;      /* held powerup weapon model [player.c] */
    /* 0x0730 */ void* shield_object;   /* reflect/x-ray shield model (parent hand_node) [player.c] */
    /* 0x0734 */ void* wand_object;     /* levitate/anti-death wand model (parent weapon_node) [player.c] */
    /* 0x0738 */ s32 death_effect;      /* StartDeathFX effect id, < 0 when off [player.c] */
    /* 0x073C */ void* field_73C;    /* node handle, MBRemoveNode-cleaned (VERIFIED: not padding -- remove_player_geo teardown) [player.c] */
    /* 0x0740 */ void* marker_object;   /* overhead marker model [player.c] */
    /* 0x0744 */ u8  pad_0744[4];
    /* 0x0748 */ void* field_748;    /* atree handle, AtreeDelete-cleaned (VERIFIED: not padding -- remove_player_geo teardown) [player.c] */
    /* 0x074C */ u8  pad_074C[0x44];
    /* 0x0790 */ void* atree;        /* familiar/overlay atree handle [pmotion.c/player.c] */
    /* 0x0794 */ u8  pad_0794[4];
    /* 0x0798 */ u32 atree_src_id;   /* familiar source id, stale-tree check [player.c] */
    /* 0x079C */ u8  pad_079C[4];
    /* 0x07A0 */ s16 field_7A0;      /* familiar anim gate counter [player.c] */
    /* 0x07A2 */ s16 field_7A2;      /* familiar transition lock [player.c] */
    /* 0x07A4 */ u8  pad_07A4[0x50];
    /* 0x07F4 */ s32 geo_handle;     /* loaded model/geo handle (load_player_model) [player.c] */
    /* 0x07F8 */ s32 texmod_id;      /* AddSpecialTexmod result, -1 when none [player.c] */
    /* 0x07FC */ f32 pulse_7FC;      /* rune-near display pulse [player.c] */
    /* 0x0800 */ u8  pad_0800[0x28];
    /* 0x0828 */ f32 power_target;   /* power-meter target [player.c] */
    /* 0x082C */ f32 power_level;    /* power-meter shown level [player.c] */
    /* 0x0830 */ s32 exit_dest;      /* next-level id picked by do_exit [player.c] */
    /* 0x0834 */ s32 quest_state;    /* quest icon: 0 off, 1 pending, 2 on [player.c] */
    /* 0x0838 */ f32 anchor_pos[3];  /* attach anchor (SetParent offset base) [player.c] */
    /* 0x0844 */ f32 anchor_fwd[3];  /* attach forward vector [player.c] */
    /* 0x0850 */ f32 col_radius;     /* collision radius (FloorCollide rad) [pmotion.c] */
    /* 0x0854 */ f32 col_height;     /* collision height (FloorCollide height) [pmotion.c] */
    /* 0x0858 */ f32 light_vec[3];   /* beacon light vector (decayed) [player.c] */
    /* 0x0864 */ f32 light_vel[3];   /* beacon light velocity [player.c] */
    /* 0x0870 */ f32 vel[3];         /* world velocity: scaled by gClockFrameStep into the
                                      * frame delta, decayed each frame, and impulse-loaded
                                      * from hit_force by PlayerKnockback (VERIFIED: not
                                      * padding -- integration triad + teardown zero-block)
                                      * [pmotion.c/player.c] */
    /* 0x087C */ f32 prev_pos[3];    /* position saved at the motion phase exit immediately
                                      * before the live position is advanced by dpos
                                      * [pmotion.c] */
    /* 0x0888 */ f32 dpos[3];        /* this frame's clamped position delta, stored back
                                      * after the move-limit clamps [pmotion.c/player.c] */
    /* 0x0894 */ f32 move_yaw;       /* commanded movement yaw [pmotion.c] */
    /* 0x0898 */ f32 field_898;      /* reaction deadline on the sMusicFadeBase time base:
                                      * set to base+1.0 (flag 0x800) or base+4.0 (flag
                                      * 0x1000) by damage_player, tested `base < field` to
                                      * force reaction 100 [pmotion.c/player.c] */
    /* 0x089C */ f32 timer_89C;      /* motion-state elapsed time accumulator [pmotion.c] */
    /* 0x08A0 */ f32 floor_hi;       /* clamped floor probe results [player.c] */
    /* 0x08A4 */ f32 floor_lo;
    /* 0x08A8 */ void* collision_item; /* nearest item hit by the motion sweep */
    /* 0x08AC */ void* special_collision_item; /* nearest special pickup */
    /* 0x08B0 */ s32* speech_req;    /* queued speech request [player.c do_players] */
    /* 0x08B4 */ f32 floor_base;     /* terrain floor height under player [pmotion.c] */
    /* 0x08B8 */ f32 field_8B8;      /* floor_base snapshot; compared against the player's
                                      * own height to gate the exit check [player.c] */
    /* 0x08BC */ f32 field_8BC;      /* floor slope term: dpos[1]/planar-distance, or the
                                      * forward-vector cross used by PlayerNewFloor, read
                                      * back by ModifyPlayerDpos [pmotion.c] */
    /* 0x08C0 */ u8  pad_08C0[4];
    /* 0x08C4 */ struct worldobj* floor_name2; /* floor world-object cache [pmotion.c]; desc[16] at +0 doubles as the debug string */
    /* 0x08C8 */ struct worldobj* floor_name;  /* wall/floor hit object [pmotion.c] */
    /* 0x08CC */ f32 floor_cur;      /* active floor-fx height [pmotion.c] */
    /* 0x08D0 */ f32 hit_damage;     /* queued damage accumulator: `+= dmg` in damage_player,
                                      * gated (> eps) and re-dispatched then cleared by
                                      * PlayerKnockback (VERIFIED: not padding -- accumulate/
                                      * consume/clear triad) [pmotion.c/player.c] */
    /* 0x08D4 */ u32 obj_flags;      /* world-obj flags; 0x4000 = parented [player.c] */
    /* 0x08D8 */ u32 act_flags;      /* action state flags [pmotion.c/player.c] */
    /* 0x08DC */ f32 hit_force[3];   /* queued knockback impulse: accumulated from the `dir`
                                      * argument of damage_player, consumed as the velocity
                                      * impulse and cleared by PlayerKnockback (VERIFIED: not
                                      * padding -- accumulate/consume/clear triad)
                                      * [pmotion.c/player.c] */
    /* 0x08E8 */ f32 fxhittime;      /* last critter-effect hit time [critter.c] */
    /* 0x08EC */ f32 floor_fx_time; /* floor hazard damage cooldown [pmotion.c] */
    /* 0x08F0 */ s32 action;         /* current action id (PlayerAttacking) [player.c] */
    /* 0x08F4 */ s32 field_8F4;      /* cleared with field_8F8 in the teardown block [player.c] */
    /* 0x08F8 */ s32 field_8F8;      /* zero-gate on the melee/attack branch of the motion
                                      * state machine [pmotion.c/player.c] */
    /* 0x08FC */ f32 combo_fade;     /* combo fade timestamp [pmotion.c] */
    /* 0x0900 */ u32 act_bits;       /* action request bits (SV view writes) [pmotion.c] */
    /* 0x0904 */ f32 melee_yaw;      /* melee target yaw offset [pmotion.c] */
    /* 0x0908 */ s32 field_908;      /* non-zero selects the moving-hit anim (32) when the
                                      * player is moving and coll_flags & 5 [pmotion.c] */
    /* 0x090C */ u32 coll_flags;     /* collision state bits [pmotion.c] */
    /* 0x0910 */ f32 coll_score;     /* collision score/severity [pmotion.c] */
    /* 0x0914 */ f32 bossdamage;     /* accumulated boss damage [critter.c] */
    /* 0x0918 */ s32 hit_streak;     /* consecutive-hit counter vs. a non-birthing
                                      * enemy; reset on pickup/respawn, drives the
                                      * msgPost(22,...) taunt at streak>=10 while a
                                      * boss is active [combat.c PlayerDamagedEnemy/
                                      * PlayerDamagedItem; also reset in player.c] */
    /* 0x091C */ s32 count_91C;      /* per-frame countdown [player.c do_players] */
    /* 0x0920 */ s32 count_920;      /* exit-anim countdown [player.c do_players] */
    /* 0x0924 */ u8  pad_0924[4];
    /* 0x0928 */ s32 got_type;       /* last pickup: crystal/boss-item/coin id [player.c] */
    /* 0x092C */ f32 got_timer;      /* pickup ticker time left [player.c] */
    /* 0x0930 */ s32 got_count;      /* pickup running count [player.c] */
    /* 0x0934 */ s32 fall_frames;    /* fall grunt selector [player.c do_players] */
    /* 0x0938 */ f32 fall_time;      /* fall start timestamp [player.c do_players] */
    /* 0x093C */ s32 field_93C;      /* cleared in the do_players per-frame block [player.c] */
    /* 0x0940 */ s32 field_940;
    /* 0x0944 */ f32 transport_pos[3]; /* transporter destination: copied from the transporter
                                      * object's position, then floor-collided and used as the
                                      * warp target by DoTransporter [pmotion.c] */
    /* 0x0950 */ s16 idle_timer;     /* idle speech timer [player.c do_players] */
    /* 0x0952 */ u8  pad_0952[2];
    /* 0x0954 */ s16 speak_timer;    /* idle speech timer [player.c/pmotion.c] */
    /* 0x0956 */ s16 field_956;      /* magic/throw request bitmask: reset to 0x10, ORed with
                                      * 2 on a queued cast, tested for 2, and set to 128 once
                                      * start_magic has run [pmotion.c/player.c] */
    /* 0x0958 */ s16 throw_str;      /* potion-throw strength [player.c start_magic] */
    /* 0x095A */ u8  pad_095A[2];
    /* 0x095C */ s16 speak_kind;     /* queued speech category [pmotion.c] */
    /* 0x095E */ s16 speak_done;     /* speech-already-played guard [pmotion.c/player.c] */
    /* 0x0960 */ u8  pad_0960[2];
    /* 0x0962 */ s16 grab_flags;     /* grab variant flags [pmotion.c/player.c] */
    /* 0x0964 */ s16 hud_flags;      /* 0x20 = attached (lha in target) [player.c] */
    /* 0x0966 */ s16 hud_flags2;     /* 1 = info written, 2 = runes written [player.c] */
    /* 0x0968 */ void* gem_object;   /* thunder/lightning gem model [player.c] */
    /* 0x096C */ void* field_96C;    /* atree handle, AtreeDelete-cleaned (VERIFIED: not padding -- remove_player_geo teardown, dup call site) [player.c] */
    /* 0x0970 */ u8  pad_0970[0xA4];
    /* 0x0A14 */ void* field_A14;    /* "mikey objgrp" node, MBRemoveNode(type=1)-cleaned (VERIFIED: not padding -- remove_player_geo teardown) [player.c] */
    /* 0x0A18 */ u8  pad_0A18[4];
    /* 0x0A1C */ s16 field_A1C;      /* weapon-flash one-shot latch [player.c] */
    /* 0x0A1E */ s16 field_A1E;      /* gem-object latch, flags 0x200000 [player.c] */
    /* 0x0A20 */ s16 field_A20;      /* gem-object latch, flags 0x400000 [player.c] */
    /* 0x0A22 */ u8  pad_0A22[0x0A];
    /* 0x0A2C */ s32 weakening_elapsed; /* elapsed ticks in weakening cycle [player.c] */
    /* 0x0A30 */ s32 weakening_period; /* weakening cycle duration [player.c] */
    /* 0x0A34 */ s32 milestone[5];   /* recently visited milestone nodes [items.c] */
    /* 0x0A48 */ f32 field_A48;      /* four motion tuning scales, all reset to 10000.0f as
                                      * four separate stores; field_A48 scales the forward
                                      * move amount and field_A50 scales the control movement
                                      * (VERIFIED: not padding -- four-store reset block)
                                      * [pmotion.c/player.c] */
    /* 0x0A4C */ f32 field_A4C;
    /* 0x0A50 */ f32 field_A50;
    /* 0x0A54 */ f32 field_A54;
    /* 0x0A58 */ f32 combo_cd;       /* combo effect cooldown timestamp [pmotion.c] */
    /* 0x0A5C */ s32 camera_limit;   /* camera dpos-limit result [pmotion.c] */
    /* 0x0A60 */ s32 display_mode;   /* HUD display mode (get_display_mode) [player.c] */
    /* 0x0A64 */ s32 field_A64;      /* shop-screen state machine mode (do_shop switch) [shop.c] */
    /* 0x0A68 */ s32 field_A68;      /* shop item-list cursor index (see shop.c DSPlayerView) [shop.c] */
    /* 0x0A6C */ s32 field_A6C;      /* shop staged-reveal UI timer (level-up/final-stats panels) [shop.c] */
    /* 0x0A70 */ f32 field_A70;      /* shop displayed-att-fight snapshot (pre-reveal animation) [shop.c] */
    /* 0x0A74 */ f32 field_A74;      /* shop displayed-att-armor snapshot [shop.c] */
    /* 0x0A78 */ f32 field_A78;      /* shop displayed-att-magic snapshot [shop.c] */
    /* 0x0A7C */ f32 field_A7C;      /* shop displayed-att-speed snapshot [shop.c] */
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
    /* 0x223A */ u8  pad_223A[0x12];
    /* 0x224C */ s32 field_224C;     /* per-character(*240) gold checkpoint [shop.c fn_8009A0AC] */
    /* 0x2250 */ u8  pad_2250[0x10D4];
    /* 0x3324 */ s32 level;          /* character level 1..99 [player.c] */
    /* 0x3328 */ s32 intower;        /* set while active in tower [player.c] */
    /* 0x332C */ s32 world_text_active; /* overhead text state [gauntworld] */
    /* 0x3330 */ s32 world_name_len;    /* capped display-name length [gauntworld] */
    /* 0x3334 */ s32 world_name_tail;   /* displaced sixth char or '@' [gauntworld] */
    /* 0x3338 */ s32 motion_state;   /* motion/tower-exit state [player.c] */
    /* 0x333C */ u8  pad_333C[4];
    /* 0x3340 */ s32 meter_flash;    /* power-meter flash state [player.c] */
    /* 0x3344 */ s32 meter_timer;    /* power-meter flash timer [player.c] */
    /* 0x3348 */ u8  pad_3348[0x14];
} Player;                            /* size 0x335C */

#endif /* GAME_PLAYER_H */
