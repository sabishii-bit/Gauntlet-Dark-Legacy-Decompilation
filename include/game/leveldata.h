#ifndef GAME_LEVELDATA_H
#define GAME_LEVELDATA_H

#include "types.h"

/* level_data - the active level descriptor (gCurLevel @ 0x8034483C points
 * at one of these).
 *
 * Authoritative field names + offsets: Xbox shell3D.pdb dump,
 *   research/xbox_symbols/misc.h -> "struct level_data // Size=0x10c (Id=3267)".
 *
 * GC offset verification (2026-08-30), three independent anchors:
 *   +0x60 camera     matches newcam.c's documented "+0x60 = active CAMERA*"
 *   +0x9C plevel     matches fn_8005C1DC's gold-ramp f32 read (matched region)
 *   +0xCC..0xDC gen_health..trap_damage
 *                    match items.c's generator/trap tuning reads
 * GC offset verification (2026-08-30 secondary enemy.c de-fakematch pass),
 * four more anchors confirmed by fnasm.py displacement + byte-gate (two of
 * the five converting functions are byte-exact MATCHED, unchanged after):
 *   +0xAC ene_health fn_80046140(x0)/damage_enemy(fight-threshold)/
 *                    init_enemy(scale) - lfs ...,172(rN) in all three
 *   +0xB0 ene_speed  do_enemies per-type speed-table refresh -
 *                    lfs ...,176(rN)
 *   +0xBC ene_damage fn_80046140(x2, MATCHED fn unchanged)/damage_enemy(x3)
 *                    suicide-explosion scale - lfs ...,188(rN)
 *   +0xC0 ene_mrate  move_logic30 (MATCHED fn unchanged) dead-end timer -
 *                    lfs ...,192(rN)
 * See attempt.enemy-c-defakematch-tupass.20260830.v2.
 * GC offset verification (2026-08-30 gauntworld-defake2 pass), two more:
 *   +0xB4 ene_visrad fn_80060114 (game/world/gauntworld.c) - fnasm.py on the
 *                    TARGET showed `lwz r3,0(0) @gCurLevel; lfs f0,180(r3)`
 *                    at both call sites; the pre-existing source had
 *                    `*(f32*)(gCurLevel + 180)` with NO `(u8*)` cast, so it
 *                    scaled by sizeof(level_data) instead of by 1 - a real
 *                    latent bug, not just an unnamed offset. Casting/naming
 *                    it fixed the bug and improved fn_80060114's real diff
 *                    (299 -> 293); see attempt.gauntworld-defake2-tupass.
 *   +0xA8..0xDC (difficulty..trap_damage, all 14 consecutive f32 fields)
 *                    ResolveWorldDataPointers's per-level float-normalise
 *                    loop (byte-exact MATCHED, unchanged after conversion)
 *                    touches offsets 168/172/.../220 in lockstep 4-byte
 *                    steps with zero gaps - independently confirms this
 *                    whole span's stride/order against the struct below
 *                    (does not by itself confirm the Xbox NAMES for the
 *                    still-unmarked fields in this span, only the layout).
 * Remaining fields carry the Xbox names unverified; verify a field's
 * displacement against GC target asm before relying on it in matching work.
 */

struct camera_data;   /* Xbox camera_data 0x6C (misc.h Id=3269) */
struct audio_data;
struct map_data;
struct bosscam_data;

typedef struct level_data {
    s32   flags;              /* 0x00 */
    s16   enabled;            /* 0x04 */
    s16   setup;              /* 0x06 */
    char  name[4];            /* 0x08 */
    s16   wavetime;           /* 0x0C */
    s16   dummy;              /* 0x0E */
    char  prep[4];            /* 0x10 */
    char  title[16];          /* 0x14 */
    char  audbank[16];        /* 0x24 */
    char  movie[16];          /* 0x34 */
    s32   bosstype;           /* 0x44 */
    s32   earlyenemies;       /* 0x48 */
    s16   enemytype[6];       /* 0x4C */
    s16   camidx;             /* 0x58 */
    s16   audidx;             /* 0x5A */
    s16   mapidx;             /* 0x5C */
    u8    align0[2];          /* 0x5E */
    struct camera_data*  camera;   /* 0x60 GC-VERIFIED (newcam bounds) */
    struct audio_data*   audio;    /* 0x64 */
    struct map_data*     mapdata;  /* 0x68 */
    struct bosscam_data* bosscam;  /* 0x6C */
    u8    fog[0x1C];          /* 0x70 fog_data (misc.h Id near 3267) */
    s16   bosscamidx;         /* 0x8C */
    s16   maxenemies;         /* 0x8E */
    s16   rune;               /* 0x90 */
    s16   legend;             /* 0x92 */
    f32   musicvol;           /* 0x94 */
    f32   soundvol;           /* 0x98 */
    f32   plevel;             /* 0x9C GC-VERIFIED (fn_8005C1DC gold ramp) */
    f32   xpmul;              /* 0xA0 */
    f32   damagemul;          /* 0xA4 */
    f32   difficulty;         /* 0xA8 */
    f32   ene_health;         /* 0xAC GC-VERIFIED (enemy.c fight threshold/scale) */
    f32   ene_speed;          /* 0xB0 GC-VERIFIED (do_enemies speed-table refresh) */
    f32   ene_visrad;         /* 0xB4 GC-VERIFIED (fn_80060114 lfs 180(rN)) */
    f32   ene_attack;         /* 0xB8 */
    f32   ene_damage;         /* 0xBC GC-VERIFIED (enemy.c suicide-explosion scale) */
    f32   ene_mrate;          /* 0xC0 GC-VERIFIED (move_logic30 dead-end timer) */
    f32   ene_mspeed;         /* 0xC4 */
    f32   ene_macc;           /* 0xC8 */
    f32   gen_health;         /* 0xCC GC-VERIFIED (items generator tuning) */
    f32   gen_rate;           /* 0xD0 */
    f32   gen_max;            /* 0xD4 */
    f32   trap_rate;          /* 0xD8 */
    f32   trap_damage;        /* 0xDC */
    s32   shop_maxgold;       /* 0xE0 */
    s32   shop_maxkills;      /* 0xE4 */
    s32   shop_maxexp;        /* 0xE8 */
    f32   ambient;            /* 0xEC */
    f32   lightdir[3];        /* 0xF0 */
    f32   lightcolor_fp[3];   /* 0xFC */
    f32   lightinten;         /* 0x108 */
} level_data;                 /* size 0x10C */

#ifdef __MWERKS__
/* offset-exact size guard */
typedef char _level_data_size_check[sizeof(level_data) == 0x10C ? 1 : -1];
#endif

#endif /* GAME_LEVELDATA_H */
