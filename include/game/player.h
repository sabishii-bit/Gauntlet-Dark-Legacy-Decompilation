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

typedef struct Player {
    /* 0x0000 */ s32 index;          /* player index (Xbox: player.index@0) */
    /* 0x0004 */ s32 class_id;       /* class index (VERIFIED: sounds cls, cls*16+type) */
    /* 0x0008 */ s32 char_type;      /* portrait/name-bank index (VERIFIED: sounds @+8) */
    /* 0x000C */ s32 character;      /* active PlayerCharType, selects char_save[] (VERIFIED @+12) */
    /* 0x0010 */ u8  pad_0010[0x34];
    /* 0x0044 */ f32 pos[3];         /* world position x,y,z (VERIFIED lfs @68/72/76) */
    /* 0x0050 */ u8  pad_0050[0x98];
    /* 0x00E8 */ s32 state;          /* player state: 0=none 1=active 2=... (VERIFIED @232) */
    /* 0x00EC */ u8  pad_00EC[0x38];
    /* 0x0124 */ u32 flags;          /* status flags; bit 0x400 tested (VERIFIED @292) */
    /* 0x0128 */ u8  pad_0128[0xCAC];
    /* 0x0DD4 */ PlayerCharSave char_save[16]; /* per-character progression (VERIFIED base+stride) */
    /* 0x1CD4 */ u8  pad_1CD4[0x1E4];
    /* 0x1EB8 */ s32 item_body_lo;   /* body-armor item flag (VERIFIED shopquery @7864) */
    /* 0x1EBC */ s32 item_body_hi;   /* body-armor item flag (VERIFIED shopquery @7868) */
    /* 0x1EC0 */ u8  pad_1EC0[8];
    /* 0x1EC8 */ u16 runes;          /* active-character rune count (documented) */
    /* 0x1ECA */ u16 shards;         /* active-character shard count (documented) */
    /* 0x1ECC */ u8  pad_1ECC[0x354];
    /* 0x2220 */ s16 crystals[8];    /* crystal counters (documented 0x2220..) */
    /* 0x2230 */ s16 gargoyle_items[3]; /* gargoyle-item counters (documented) */
    /* 0x2236 */ s16 legend_items[2];   /* legend-item counters (documented ..0x223A) */
    /* 0x223A */ u8  pad_223A[0x1122];
} Player;                            /* size 0x335C */

#endif /* GAME_PLAYER_H */
