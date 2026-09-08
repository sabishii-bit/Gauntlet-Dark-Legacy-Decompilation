#ifndef GAME_PLYRDATA_H
#define GAME_PLYRDATA_H

#include "types.h"

/* plyr_data - the per-character record the PDATA WADs ship, one per class.
 *
 * PROVENANCE.  orig/GUNE5D/Gauntlet/PDATA holds 16 WADs (ARC DWF FAL HYE JAC
 * JES KNI MED MIN OGR SOR TIG UNI VAL WAR WIZ) in the same container the
 * CRITTER WADs use: `u32 dirOffset; u32 sectionCount; u32 0; u32 0;` then the
 * payloads, then `sectionCount` 16-byte directory entries `char tag[4]`
 * (stored reversed) `u32 offset; u32 count; u32 count`.  Reading the strides
 * from the directory's own start/count pairs - never from spacing - gives
 * SFXX 0x50, DAMG 0x58 (NOT critter's 0x50) and PDAT exactly ONE record of
 * 0x180 bytes, in all 16 files (build/d_lane/d_pdata.py).
 *
 * That 0x180 is the Xbox PDB's `struct plyr_data // Size=0x180 (Id=3433)`
 * (research/xbox_symbols/misc.h), field for field.  The payloads are PS2
 * little-endian and are byte-swapped at load, so the shipped values only read
 * as sane numbers little-endian; every value quoted below is LE.
 *
 * A field is TYPED here only where a GameCube access in src/game/game/player.c
 * proves its width, and the citation is on the member.  weapon_offset, turboa_offset and streakfwdmul are typed on
 * src/game/game/combat.c's accesses instead.  Fields whose only reader is
 * src/game/sfx/psfx.c stay sized raw runs naming the PDB field.
 *
 * The shipped values corroborate the PDB's names independently:
 *   fight/speed/armor/magic min-max are per-class 100..999 pairs with
 *   min < max in every file; height is 5.0 and width 1.5 in all 16, and
 *   player.c stores them into Player.col_height (halved) and col_radius;
 *   attny 4.4 and coly 2.5 go to Player.anchor_pos[1]/anchor_fwd[1];
 *   weapon_fx_offset[t] is an XYZ offset whose x is 0 in every file and
 *   whose y is negative and z positive (WAR tier 0: 0, -0.35, 0.75), while
 *   weapon_fx_scale[t] is a 0.45..0.9 scale triple, often uniform (WIZ tier
 *   0: 0.6, 0.6, 0.6).  player.c had these two as an invented `TierColor`
 *   with an `rgb[3]` member; they are not colours. */
typedef struct plyr_data {
    /* 0x000 */ u8 _pdb_000[0x28]; /* PDB: short numsfx, numdamage; plyr_sfx*
                                    * sfx; plyr_damage* damage; short
                                    * turboAclose/turboAlow/turboAstep/
                                    * turboA360/turboAthrow/turboB/turboC1/
                                    * turboC2/combo1/combo2/combohit/victory;
                                    * int initflag - read by psfx.c, not by
                                    * player.c, so not typed here            */
    /* 0x028 */ f32 fight_min;     /* player_get_powerup_state base          */
    /* 0x02C */ f32 fight_max;     /* the cap it clamps against              */
    /* 0x030 */ f32 speed_min;
    /* 0x034 */ f32 speed_max;
    /* 0x038 */ f32 armor_min;
    /* 0x03C */ f32 armor_max;
    /* 0x040 */ f32 magic_min;
    /* 0x044 */ f32 magic_max;
    /* 0x048 */ f32 height;        /* load_player: Player.col_height = h*0.5 */
    /* 0x04C */ f32 width;         /* load_player: Player.col_radius         */
    /* 0x050 */ f32 attny;         /* load_player: Player.anchor_pos[1]      */
    /* 0x054 */ f32 coly;          /* load_player: Player.anchor_fwd[1]      */
    /* 0x058 */ f32 powerup_time;  /* scales the powerup strength            */
    /* 0x05C */ f32 weapon_offset[3];  /* combat.c aim_from_mode passes it to
                                       * MulVecMat3 as the f32 source vector  */
    /* 0x068 */ f32 weapon_fx_offset[10][3]; /* per weapon tier              */
    /* 0x0E0 */ f32 weapon_fx_scale[10][3];  /* per weapon tier              */
    /* 0x158 */ f32 turboa_offset[3];  /* combat.c, the mode-2 MulVecMat3 arm */
    /* 0x164 */ f32 familiar_offset[3];      /* familiar node placement      */
    /* 0x170 */ u8 _pdb_170[0xC];  /* PDB: float fam_proj_offset[3]          */
    /* 0x17C */ f32 streakfwdmul;      /* combat.c SfxSetStreak's last f32 arg */
} plyr_data;                       /* 0x180 == the shipped PDAT stride */

#ifdef __MWERKS__
/* offset-exact size guard against the shipped record */
typedef char _plyr_data_size_check[sizeof(plyr_data) == 0x180 ? 1 : -1];
#endif

/* The loaded record for each active player lives at lbl_80282930[4]; the
 * symbol keeps its address name until a rename is coordinated, so consumers
 * declare it themselves rather than this header inventing a name. */

#endif /* GAME_PLYRDATA_H */
