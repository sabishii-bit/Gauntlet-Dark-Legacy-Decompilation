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
 *   0x2220..0x224C         char_save checkpoint-shadow scalars (see 0x1ECC note)
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
    /*
     * Boss pass-1/pass-2 attempt bitmasks, tested as 1 << crystal_order[i]
     * (abs 0xDE4/0xDE6) by options.c next_boss_hint -- the third member of the
     * hint-tier family whose rune and legend pairs sit in level_masks[0..3].
     * Names are Midway's own from the Xbox P_SAVE_STUFF analogue, adopted on
     * ROLE evidence (next_boss_hint), not on position: the GC slot is compacted
     * relative to Xbox, so this pair sits at 0x10/0x12 here vs 0x14/0x16 there.
     */
    /* 0x10 */ u16 boss_attempt1;    /* pass-1 tier [options.c next_boss_hint] */
    /* 0x12 */ u16 boss_attempt2;    /* pass-2 tier [options.c next_boss_hint] */
    /*
     * Per-level completion records, indexed by level.  SIGNED: every consumer
     * tests `< 0` for "not yet attempted", and the target loads them with lha /
     * lhax (load halfword ALGEBRAIC), never lhz -- see towerGetLevelRecord
     * `lha r3,3560(r3)` and towerBossStatus `lha r0,3566(r3)`.
     *
     * Widths are proven by byte fit plus consumer index bounds:
     *   completion1[3] fills 0x14-0x19 exactly, terminating where completion2
     *   begins; tower.c clamps `if (level > 2)` (towerAllPlayersMetLevelReq)
     *   and walks `for (j = 0; j < 3; j++)` against the 3-entry requirement
     *   tables lbl_80124D94[3] / lbl_80124CDC[3].
     *   completion2[9] fills 0x1A-0x2B, pairing one-for-one with the 9-entry
     *   requirement table lbl_80124C70[9].  The upper bound is proven by
     *   screensaver.c, which walks `for (i = 0; i < 8; i++)` with `off += 2`
     *   from 0x1C (= completion2[1]) -- last access 0x2A = completion2[8] --
     *   reading lbl_80124C70[i+1] each iteration.  tower.c's own `j < 8` loops
     *   reach only [0..7] and guard `lbl_80124C70[j] != 0`, skipping the
     *   table's index-0 sentinel; screensaver.c is what fixes the width at 9.
     * Both are arrays of a SCALAR type, so the aggregate-member cascade law
     * (claim.law.embedded-struct-member-whole-tu-cascade) does not apply.
     */
    /* 0x14 */ s16 completion1[3];   /* per-level record A (abs 0xDE8) */
    /* 0x1A */ s16 completion2[9];   /* per-level record B (abs 0xDEE) */
    /* 0x2C */ u8  pad_2C[0xC4];     /* remainder of slot (unmapped) */
} PlayerCharSave;                    /* size 0xF0 */

/*
 * Per-character run/session stat tally.  Player.char_stats is an array of 16 of
 * these (one per enum PlayerCharType); Player.character selects the active one,
 * exactly like char_save[].
 *
 * Xbox analogue: struct P_SAVE_STATS (Id=3351), size 0x1c -- IDENTICAL size
 * here, unlike its sibling P_SAVE_STUFF/PlayerCharSave which GC compacted from
 * 0x254 to 0xF0.  The pairing is structural, not just nominal: the Xbox `player`
 * record carries `P_SAVE_STATS stats[16]` at 0x190 immediately followed by
 * `P_SAVE_STUFF stuff[16]` at 0x350, and GC reproduces exactly that adjacency --
 * this block[16] spans 0xC10..0xDD0 (0x1C0, the same array size Xbox reports)
 * and terminates 4 bytes before char_save[16] at 0xDD4.
 *
 * GC offset verification (gamemain.c do_stats_display, the stats-screen tally):
 *   +0x00 enemies_killed        VERIFIED  read as s32, animated /60 per frame
 *   +0x10 generators_destroyed  VERIFIED  read as s32, animated /60 per frame
 *   +0x14 gold_found            VERIFIED  read as s32, animated /60 per frame
 *   +0x18 total_playtime        VERIFIED  read as f32 and divided by 60.0f --
 *         the FLOAT at exactly +0x18 is what fixes this struct's identity: it
 *         is the only 28-byte game record in the PDB with an f32 there, and
 *         frames/60.0f -> seconds is precisely a playtime conversion.
 * The three fields at +0x04/+0x08/+0x0C are NOT GC-attested (the GC stats
 * screen tallies only the four rows above); their names are carried over from
 * the size-identical Xbox record and should be re-verified before a consumer
 * relies on them.
 */
typedef struct PlayerCharStats {
    /* 0x00 */ s32 enemies_killed;       /* VERIFIED gamemain do_stats_display */
    /* 0x04 */ s32 generals_killed;      /* Xbox P_SAVE_STATS, not GC-attested */
    /* 0x08 */ s32 golems_killed;        /* Xbox P_SAVE_STATS, not GC-attested */
    /* 0x0C */ s32 bosses_killed;        /* Xbox P_SAVE_STATS, not GC-attested */
    /* 0x10 */ s32 generators_destroyed; /* VERIFIED gamemain do_stats_display */
    /* 0x14 */ s32 gold_found;           /* VERIFIED gamemain do_stats_display */
    /* 0x18 */ f32 total_playtime;       /* VERIFIED f32, /60.0f -> seconds */
} PlayerCharStats;                       /* size 0x1C */

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
    /*
     * Mikey powerup block (0x096C..0x0A22): the GC image of Xbox player's
     * `struct atree mikey_obj_atree` (0x48) + `struct OBJGRP mikey_objgrp`
     * (0x68) + `short mikey_dropped_flag` (misc.h player @0xAD0/0xB18/0xB80;
     * constant GC->Xbox delta +0x164 on both flanks: anchor_pos 0x838->0x99C,
     * col_radius 0x850->0x9B4, field_A1C 0xA1C->0xB80, weakening_elapsed
     * 0xA2C->heartbeat 0xB90).  GC keeps both embeds at full Xbox size; the
     * per-member picture below is fixed by player.c's own PlayerMikeyState
     * view struct (atree@0x96C, anim_state@0x9A4, matrix@0x9B4,
     * saved_pos@0x9F4, fx_pos@0xA04, node@0xA14, field_A18@0xA18,
     * state@0xA1C) and by the enemy.c/gamemain.c range-to-player readers.
     * Declared as scalars/arrays-of-scalars per
     * claim.law.embedded-struct-member-whole-tu-cascade (no new aggregate
     * member may enter this shared header).
     */
    /* 0x096C */ void* field_96C;    /* mikey atree embed head = atree.root; AtreeDelete/AtreeInit
                                      * take its address (VERIFIED: not padding -- remove_player_geo
                                      * teardown, dup call site; PlayerMikeyState.atree) [player.c] */
    /* 0x0970 */ u8  pad_0970[0x34]; /* atree.animinfo interior 0x970..0x9A4 (no GC consumer) */
    /* 0x09A4 */ s16 mikey_anim_state; /* PlayerMikeyState.anim_state (= atree.animinfo.repeat,
                                        * +0x38 into the embed); set 1 at hatch [player.c] */
    /* 0x09A6 */ u8  pad_09A6[0xE];  /* atree tail: animinfo.stage(2) + nanodes(4) +
                                      * firstanode(4) + anodeinfo(4) (no GC consumer) */
    /* 0x09B4 */ f32 mikey_worldmat[4][4]; /* mikey OBJGRP.worldmat; [3][0]/[3][2] are the
                                            * chase-bearing alt position read when field_A1C > 2
                                            * [enemy.c fn_8004CE38; player.c PlayerMikeyState.matrix] */
    /* 0x09F4 */ f32 mikey_attn_pos[4];    /* mikey OBJGRP.attn_pos (PlayerMikeyState.saved_pos
                                            * covers [0..2]) [player.c] */
    /* 0x0A04 */ f32 mikey_coll_pos[4];    /* mikey OBJGRP.coll_pos; [0..2] is the alt position
                                            * enemies range against when field_A1C > 2
                                            * [enemy.c fn_80046680, gamemain.c; player.c
                                            * PlayerMikeyState.fx_pos feeds StartGemFX] */
    /* 0x0A14 */ void* field_A14;    /* mikey OBJGRP.node, MBRemoveNode(type=1)-cleaned (VERIFIED:
                                      * not padding -- remove_player_geo teardown; the "mikey objgrp"
                                      * ErrorPrintf names it) [player.c] */
    /* 0x0A18 */ s32 mikey_objgrp_flags; /* mikey OBJGRP.flags; zeroed at hatch
                                          * (PlayerMikeyState.field_A18) [player.c] */
    /* 0x0A1C */ s16 field_A1C;      /* mikey powerup state machine (Xbox mikey_dropped_flag):
                                      * 0 = off, 1 = hatch pending (set when flags &
                                      * SPECIAL_MIKEY=0x100000), 2 = hatched, ++ per live tick,
                                      * 300 = despawn; enemy.c/gamemain.c read > 2 as "mikey live,
                                      * range against mikey_coll_pos/mikey_worldmat[3] instead of
                                      * the player" [player.c PlayerProcessMikeyPUP] */
    /* 0x0A1E */ s16 field_A1E;      /* gem-object latch, flags 0x200000 (Xbox analogue slot:
                                      * hand_of_death_flag) [player.c] */
    /* 0x0A20 */ s16 field_A20;      /* gem-object latch, flags 0x400000 (Xbox analogue slot:
                                      * health_vamp_flag) [player.c] */
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
    /*
     * pad_0A88's original 0x34C run splits with exact byte accounting:
     *   0x0A88 + 0x188 = 0x0C10   leading pad (still unmodelled; it contains
     *                             the s16 char-type at 0x0A88 and player.c's
     *                             CHAR_STATS block, base 0x0A90 stride 0x18,
     *                             whose 16 entries end exactly at 0x0C10)
     *   0x0C10 + 0x1C0 = 0x0DD0   char_stats[16], 16 * 0x1C
     *   0x0DD0 +   0x4 = 0x0DD4   trailing pad, up to char_save[]
     * 0x188 + 0x1C0 + 0x4 = 0x34C, so Player stays 0x335C.
     */
    /* 0x0A88 */ u8  pad_0A88[0x188];
    /* 0x0C10 */ PlayerCharStats char_stats[16]; /* per-character stat tally
                                        * (VERIFIED base 3088 + character*28) */
    /* 0x0DD0 */ u8  pad_0DD0[4];
    /* 0x0DD4 */ PlayerCharSave char_save[16]; /* per-character progression (VERIFIED base+stride) */
    /* 0x1CD4 */ u8  pad_1CD4[0x1E0];
    /* 0x1EB4 */ f32 health;         /* hit points, 9999 display cap [player.c] */
    /* 0x1EB8 */ s32 item_body_lo;   /* body-armor item flag (VERIFIED shopquery @7864) */
    /* 0x1EBC */ s32 item_body_hi;   /* body-armor item flag (VERIFIED shopquery @7868) */
    /* 0x1EC0 */ s32 exp;            /* experience points [player.c ModifyExp] */
    /* 0x1EC4 */ s32 gold;           /* gold, 99999 cap [player.c PlayerGiveGold] */
    /* 0x1EC8 */ u16 runes;          /* active-character rune count (documented) */
    /* 0x1ECA */ u16 shards;         /* active-character shard count (documented) */
    /*
     * Checkpoint shadow of the persistent block (claim.player-0x2220-is-char-
     * save-checkpoint-shadow): [0x1ECC, 0x3300) mirrors [0xA80, 0x1EB4) at
     * delta +0x144C.  gamemain.c:3798 and select.c:1273 copy the whole 0x1434
     * block on level entry (select.c spells the source offsetof(Player,
     * name)); player.c:3473 memsets both at 0x1434.  The char_save[16] image
     * begins at 0x2220 (= 0xDD4 + 0x144C) and is modelled as char_save_ckpt
     * below; consumers add character * 240 exactly like the live side
     * (tower.c's paired live/shadow completion writes, shop.c's gold
     * snapshot at slot +0x2C).  The previous crystals[8]/gargoyle_items[3]/
     * legend_items[2] names here were misfiled Xbox P_SAVE_STUFF fields with
     * zero consumers.  Adding this second PlayerCharSave member gated clean
     * across the heavy includers (incl. shop.c) -- the embedded-cascade law's
     * hazard is introducing a NEW aggregate type to the header, and
     * PlayerCharSave was already a member type via char_save[16].
     */
    /* 0x1ECC */ u8  pad_1ECC[0x354]; /* shadow of name[8] + pad_0A88[0x34C] */
    /* 0x2220 */ PlayerCharSave char_save_ckpt[16]; /* checkpoint shadow of
                                        * char_save[16] (0x2220..0x3120) */
    /* 0x3120 */ u8  pad_3120[0x1E0];  /* shadow of pad_1CD4 */
    /* 0x3300 */ u8  pad_3300[0x24];   /* non-shadow bytes before level */
    /* 0x3324 */ s32 level;          /* character level 1..99 [player.c] */
    /* 0x3328 */ s32 intower;        /* set while active in tower [player.c] */
    /* 0x332C */ s32 world_text_active; /* overhead text state [gauntworld] */
    /* 0x3330 */ s32 world_name_len;    /* capped display-name length [gauntworld] */
    /* 0x3334 */ s32 world_name_tail;   /* displaced sixth char or '@' [gauntworld] */
    /* 0x3338 */ s32 motion_state;   /* motion/tower-exit state [player.c] */
    /* 0x333C */ s32 motion_state_save; /* motion_state stashed across a select
                                        * sub-menu; restored at every exit
                                        * [select.c], cleared beside
                                        * motion_state [player.c] */
    /* 0x3340 */ s32 meter_flash;    /* power-meter flash state [player.c] */
    /* 0x3344 */ s32 meter_timer;    /* power-meter flash timer [player.c] */
    /*
     * Select-screen memory-card sub-menu state (0x3348..0x335B).  Names are
     * behaviour-derived from src/game/ui/select.c; the four-word teardown block
     * in src/game/game/player.c (0x334C/0x3350/0x3354 = 0, 0x3358 = -1) is the
     * padding evidence that fixes the run as exactly five s32 words.
     */
    /* 0x3348 */ s32 sel_step;       /* sub-menu step selector AND tick timer:
                                      * switched on, stepped += 1, accumulated
                                      * += gFrameTicks vs 0x78/0x460, driven
                                      * negative vs -0x78, -1 = failed,
                                      * 1000 = card full [select.c] */
    /* 0x334C */ s32 sel_card_chan;  /* memory-card channel: 1st arg of
                                      * saveMount/saveGetFreeBytes/
                                      * MemCardCreateGaunt/add_vmu_file
                                      * [select.c] */
    /* 0x3350 */ s32 sel_card_slot;  /* memory-card slot: 2nd arg of the same
                                      * four calls, always paired with
                                      * sel_card_chan [select.c] */
    /* 0x3354 */ s32 sel_save_file;  /* save-file index: 3rd arg of
                                      * add_vmu_file, sole file arg of
                                      * PlayerLoadSaveFile/PlayerWriteSaveFile
                                      * [select.c] */
    /* 0x3358 */ s32 sel_file_cursor;/* file-list menu cursor, copied into
                                      * OPTMENU.sel by setup_sel_menu; -1 when
                                      * no entry is selected [select.c] */
} Player;                            /* size 0x335C */

/*
 * Record stride of the gPlayers array, as a named LITERAL for the walked
 * `p += 13148` loops.  Deliberately an object-like #define and not
 * sizeof(Player): claim.law.sizeof-defeats-loop-stride-induction records
 * that a sizeof() stride operand changes MWCC's loop-induction choice even
 * though it folds to the same constant; a macro literal is codegen-identical
 * to the bare number.
 */
#define PLAYER_STRIDE 0x335C         /* == sizeof(Player), 13148; spelled exactly as
                                      * combat.c's pre-existing local definition so that
                                      * definition stays a benign identical redefinition */

#endif /* GAME_PLAYER_H */
