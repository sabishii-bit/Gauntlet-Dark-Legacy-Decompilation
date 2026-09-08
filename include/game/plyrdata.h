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
/* One SFXX row, stride 0x50 from the PDATA directory, == the Xbox PDB
 * `struct plyr_sfx // Size=0x50 (Id=3434)`.  The shipped payloads corroborate
 * every field read little-endian (build/d_lane/d_sfxrec.py): WAR sfx[0] has
 * fxdesc "DRAGKEY_PART" and snddesc "R_WRIST" as NUL-terminated ASCII at 0x10
 * and 0x20, nextfxidx -1, zmod 200, alphamod 150, offset {0,0,2}, maxlen 0.5,
 * radius 5, scale 2, color 0x7FFFFFFF; WIZ sfx[0] chains nextfxidx 1 to
 * sfx[1].  psfx.c reads each of these at the width shown. */
typedef struct plyr_sfx {
    /* 0x00 */ u32 flags;
    /* 0x04 */ u32 nextfxidx;      /* chained record index, -1 = none */
    /* 0x08 */ s32 sfxidx;         /* resolved effect/texture handle  */
    /* 0x0C */ s32 sndidx;         /* resolved sound/mbox handle      */
    /* 0x10 */ char fxdesc[16];
    /* 0x20 */ char snddesc[16];
    /* 0x30 */ s16 zmod;
    /* 0x32 */ s16 alphamod;
    /* 0x34 */ f32 offset[3];
    /* 0x40 */ f32 maxlen;
    /* 0x44 */ f32 radius;
    /* 0x48 */ f32 scale;
    /* 0x4C */ u32 color;
} plyr_sfx;                        /* 0x50 == the SFXX stride */

/* One DAMG row, stride 0x58 from the directory, == the Xbox PDB
 * `struct plyr_damage // Size=0x58 (Id=3432)`.  Shipped corroboration: WAR
 * dmg[0] reads type 4, dmgtype 33, hitrad 3, radius 5, minrad 0, delay 0.25,
 * amount -0.5, fxidx 1, hitfxidx/loopfxidx/next -1, startframe 6 - all sane
 * only little-endian.  psfx.c's PlyrSfxDoDamage family walks `next` as the
 * chain link and indexes plyr_data.sfx with fxidx/hitfxidx/loopfxidx. */
typedef struct plyr_damage {
    /* 0x00 */ s16 type;
    /* 0x02 */ s16 flags;
    /* 0x04 */ u32 dmgtype;        /* PDB: enum DMG_TYPE */
    /* 0x08 */ f32 hitrad;
    /* 0x0C */ f32 radius;
    /* 0x10 */ f32 minrad;
    /* 0x14 */ f32 delay;
    /* 0x18 */ f32 mintime;
    /* 0x1C */ f32 maxtime;
    /* 0x20 */ f32 angle;
    /* 0x24 */ f32 arc;
    /* 0x28 */ f32 pitch;
    /* 0x2C */ f32 offset[3];
    /* 0x38 */ f32 amount;
    /* 0x3C */ f32 speed_min;
    /* 0x40 */ f32 speed_max;
    /* 0x44 */ f32 weight;
    /* 0x48 */ s16 fxidx;
    /* 0x4A */ s16 hitfxidx;
    /* 0x4C */ s16 loopfxidx;
    /* 0x4E */ s16 next;
    /* 0x50 */ s16 startframe;
    /* 0x52 */ s16 endframe;
    /* 0x54 */ s16 helpidx;
    /* 0x56 */ s16 dummy;
} plyr_damage;                     /* 0x58 == the DAMG stride */

typedef struct plyr_data {
    /* 0x000 */ s16 numsfx;        /* psfx.c LoadPdataFile byte-swaps it as a
                                   * u16 and uses it as the plyr_sfx count   */
    /* 0x002 */ s16 numdamage;     /* same swap; the plyr_damage row count    */
    /* 0x004 */ u8* sfx;           /* plyr_sfx[numsfx]; MBGetFromWad("SFXX")  */
    /* 0x008 */ u8* damage;        /* plyr_damage[numdamage]; "DAMG"          */
    /* 0x00C */ s16 turboAclose;   /* every s16 from here to victory is       */
    /* 0x00E */ s16 turboAlow;     /* byte-swapped one u16 at a time by       */
    /* 0x010 */ s16 turboAstep;    /* LoadPdataFile, which is what fixes      */
    /* 0x012 */ s16 turboA360;     /* their width and their boundaries        */
    /* 0x014 */ s16 turboAthrow;
    /* 0x016 */ s16 turboB;
    /* 0x018 */ s16 turboC1;
    /* 0x01A */ s16 turboC2;
    /* 0x01C */ s16 combo1;
    /* 0x01E */ s16 combo2;
    /* 0x020 */ s16 combohit;
    /* 0x022 */ s16 victory;
    /* 0x024 */ s32 initflag;      /* psfx.c clears it per level              */
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
    /* 0x170 */ f32 fam_proj_offset[3];
    /* 0x17C */ f32 streakfwdmul;      /* combat.c SfxSetStreak's last f32 arg */
} plyr_data;                       /* 0x180 == the shipped PDAT stride */

#ifdef __MWERKS__
/* offset-exact size guard against the shipped record */
typedef char _plyr_data_size_check[sizeof(plyr_data) == 0x180 ? 1 : -1];
typedef char _plyr_sfx_size_check[sizeof(plyr_sfx) == 0x50 ? 1 : -1];
typedef char _plyr_damage_size_check[sizeof(plyr_damage) == 0x58 ? 1 : -1];
#endif

/* The loaded record for each active player lives at lbl_80282930[4]; the
 * symbol keeps its address name until a rename is coordinated, so consumers
 * declare it themselves rather than this header inventing a name. */

#endif /* GAME_PLYRDATA_H */
