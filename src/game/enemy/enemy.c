#include "game/enemy.h"
#include "game/worldobj.h"
#include "game/dyngrid.h"
#include "game/leveldata.h"

#ifndef offsetof
#define offsetof(type, memb) ((u32) & ((type*)0)->memb)
#endif

/* Gauntlet Dark Legacy enemy module (Xbox ENEMY.OBJ / enemy.c).
 *
 * ENEMY.OBJ is a single very large translation unit.  On the GameCube build it
 * occupies one contiguous .text run, 0x800444C0 - 0x80050054, sitting between
 * dynobjgrid.c (ends 0x800444C0) and gamemain.c (starts 0x80050054).  This file
 * remains wired NonMatching while its large bodies are recovered incrementally;
 * dtk substitutes the original DOL bytes until the complete object is ready.
 * Per-function byte matching is used throughout so each recovered slice can be
 * verified independently.
 *
 * Data used throughout:
 *   gEnemies        0x80251C18  active enemy records, stride 0x394 (916) bytes.
 *                               The compiler often addresses them via the base
 *                               lbl_80250E00 (= gEnemies - 0xE18) + 0xE18.
 *   gNumEnemies     0x80344744  number of active enemy slots.
 *   generator table 0x80275AE0  four 0x335C (13148) byte "generator/head" recs.
 *   jumptable_8011C0EC          32-entry AI move-logic dispatch table (see below).
 *   enemy type names 0x80112370 "SCORPION","SORCERER","LIZARDMAN","TREEFOLK",
 *                               "ICE GRUNT","ICE DEMON","SKELETON","WHIRLWIND",
 *                               "GARGOYLE" ... (debug names, GetEnemyType).
 *
 * ---------------------------------------------------------------------------
 * AI MOVE-LOGIC DISPATCH  (do_ai @0x80046854)
 * ---------------------------------------------------------------------------
 * do_ai reads the enemy's behaviour/state index at record offset 0x310 (0..31)
 * and jumps through jumptable_8011C0EC[state] to the matching move_logicNN
 * handler.  Each handler sets a desired facing/velocity and calls do_enemy_move
 * to commit the move + resolve collisions.  Verified state -> handler mapping
 * (jumptable read directly from the DOL):
 *
 *   state  0 -> move_logic00  0x80046B54      state 16 -> move_logic16 0x8004A78C
 *   state  1 -> move_logic01  0x80046F24      state 17 -> move_logic23 (shared)
 *   state  2 -> move_logic02  0x800471A4      state 18 -> move_logic18 0x8004AB20
 *   state  3 -> move_logic03  0x800473E8      state 19 -> move_logic19 0x8004AE94
 *   state  4 -> move_logic04  0x800475FC      state 20 -> move_logic20 0x8004B19C
 *   state  5 -> move_logic05  0x80047844      state 21 -> move_logic21 0x8004B5AC
 *   state  6 -> move_logic06  0x80047BF0      state 22 -> move_logic22 0x8004B788
 *   state  7 -> move_logic07  0x80047F9C      state 23 -> move_logic23 0x8004BB14
 *   state  8 -> move_logic08  0x80048408      state 24 -> move_logic24 0x8004BC5C
 *   state  9 -> move_logic00 (shared)         state 25 -> kill_enemy (inline)
 *   state 10 -> move_logic10  0x80048928      state 26 -> move_logic16 (shared)
 *   state 11 -> (inlined into do_ai)          state 27 -> (inlined into do_ai)
 *   state 12 -> move_logic12  0x80049A1C      state 28 -> move_logic28 0x8004BDDC
 *   state 13 -> move_logic13  0x80049C70      state 29 -> move_logic29 0x8004BF9C
 *   state 14 -> move_logic14  0x80049FD4      state 30 -> move_logic30 0x8004C3E4
 *   state 15 -> move_logic15  0x8004A430      state 31 -> move_logic31 0x8004C650
 *
 * move_logic09/11/17/25/26/27 have no distinct GC function: 09 reuses 00, 17
 * reuses 23, 26 reuses 16, and 11/25/27 are inlined into do_ai.  Sizes cross-
 * check against the Xbox PDB (e.g. move_logic05 and move_logic06 are 0x218 in
 * Xbox and both 0x3AC on GC; move_logic10 is the biggest handler in both).
 * move_logic13 ErrorPrintf's an enemy type name from the 0x80112370 table.
 *
 * ---------------------------------------------------------------------------
 * IDENTIFIED FUNCTIONS (real Xbox-PDB names, mapped in config symbols.txt)
 * ---------------------------------------------------------------------------
 *   closest_enemy     0x800444C0 - grid-iterate gEnemies within a firing cone;
 *                                  returns the closest enemy + its offset.
 *                                  Global (weapon/targeting callers).
 *   do_enemy_move     0x80044664 - central per-enemy move + collision commit,
 *                                  called by every move_logicNN and do_enemies
 *                                  (28 callers). Local. REAL BODY BELOW
 *                                  (829/905 insns, Ghidra-driven draft).
 *   do_enemy_collide  0x80045488 - collision core called by do_enemy_move;
 *                                  drives the EnemyCollide* helpers. Local.
 *   EnemyWorldDamage  0x80045FE4 - enemy-vs-world-object damage/push
 *                                  (WorldObjGetAllFlags -> damage_enemy). Local.
 *   do_ai             0x80046854 - the jumptable move-logic dispatcher above.
 *   move_logic00..31  0x80046B54..0x8004C650 - per-behaviour handlers (map above).
 *   find_neighbor_milestone 0x8004C9DC     turn_enemy_ang        0x8004CBB8
 *   do_enemies        0x8004D078 - per-frame master loop; global.
 *   update_enemy_milestone 0x8004E514
 *   damage_enemy      0x8004E6F8 - apply damage + knockback; global, called from
 *                                  weapons/effects/world (cross-module).
 *   kill_enemy        0x8004EFE4      uncouple_enemy   0x8004F2D8
 *   generate_enemy    0x8004F4B4      check_enemy_pos  0x8004F9AC
 *   find_enemy_slot   0x8004FD00      init_enemy      0x8004FE34
 *
 * Still fn_XXXXXXXX (behaviour understood, exact PDB name not yet pinned):
 *   fn_80045C30 - distance/proximity collision + damage helper called by
 *                 do_enemy_collide (EnemyCollidePlayer or EnemyCollideEnemy).
 *   fn_80046140 - per-type (type==30) generator/timer tick with audio + msgPost.
 *   fn_8004646C - world-grid query (StartItemGrid/NextGridItem).
 *   fn_80046680 - small gEnemies helper called by do_enemy_move.
 *   fn_8004C8CC - wall/object collision probe shared by the move_logic set.
 *   set_enemy_trans - accelerate along an angle (cos/sin -> velocity).
 *   fn_8004CE38 - left/right look-ahead probe angle helper.
 *   fn_8004D958 - per-enemy tick wrapper (drives do_ai).
 *   fn_8004DC2C/fn_8004DF58/fn_8004E448 - move post-processing / FX helpers.
 *   fn_8004F87C - generate_enemy type-resolution support.
 *   plus small state/timer pokes: fn_8004CFAC, fn_8004D030, fn_8004DB3C,
 *   fn_8004E5F8, fn_8004E67C, fn_8004F1DC, check_vacancy.
 *
 * Matching is intentionally not attempted here (high mismatch accepted); this
 * file exists to carry the symbol map and keep the tree green.
 */

/* --- enemy record array + counts (module data) ---
 * gEnemies[25] @0x80251C18 (stride 0x394) and gNumEnemies @0x80344744 are
 * declared by "game/enemy.h" (the reconstructed Enemy struct header). */

/* forward decls for the cross-referenced enemy entry points */
s32 find_enemy_slot(s32 type, s32 level);
void kill_enemy(s32 index);
void uncouple_enemy(s32 index);
void do_enemy_move(s32 index);
s32 do_enemy_collide(s32 index, f32 retryThreshold);
f32 turn_enemy_ang(Enemy* e, f32 want);
s32 do_ai(s32 index);
void move_logic00(s32 index);
void move_logic01(s32 index); void move_logic02(s32 index); void move_logic03(s32 index);
void move_logic04(s32 index); void move_logic05(s32 index); void move_logic06(s32 index);
void move_logic07(s32 index); void move_logic08(s32 index); void move_logic10(s32 index);
void move_logic12(s32 index); void move_logic13(s32 index); void move_logic14(s32 index);
void move_logic15(s32 index); void move_logic16(s32 index); void move_logic18(s32 index);
void move_logic19(s32 index); void move_logic20(s32 index); void move_logic21(s32 index);
void move_logic22(s32 index); void move_logic23(s32 index); void move_logic24(s32 index);
void move_logic28(s32 index); void move_logic29(s32 index); void move_logic30(s32 index);
void move_logic31(s32 index);
extern void CreateYPRMatrix(f32* mat, f32* pyr);        /* pyr -> rotation matrix (fwd) */
extern void CopyMat3(f32* src, f32* dst);           /* 0x800BE8C8 (fwd) */
extern s32 gGameOptions[];   /* 0x80257590 (lbl_80257598 = [2]) */

/* branchless-abs idiom (srawi/xor/subf at -O4) */
#define ABS(x) (((x) ^ ((x) >> 31)) - ((x) >> 31))
#define ABS_REVERSED(x) ((((x) >> 31) ^ (x)) - ((x) >> 31))

/* --- same-TU statics not yet reconstructed (extern until written) --- */
extern void EnemyWorldDamage(Enemy* e, void* wobj, f32* oldpos, f32* hitnrm);
extern void fn_80046140(s32 index);                 /* generator-contact retreat */
extern s32 fn_8004646C(f32 rad, f32 hht, s32 index, f32* oldc, f32* newc,
                       f32* newc2, s32* hitWorld);  /* enemy-vs-enemy probe */
extern s32 fn_80046680(f32 rad, f32 hht, s32 index, s32 b, f32* oldc,
                       f32* newc);                  /* generator-contact probe */
s32 fn_8004CFAC(f32* pos, f32* target);             /* turn direction (route) */
void fn_8004D030(s32 index, s32 ticks);             /* set dead_end/turn timer */
void fn_8004DB3C(Enemy* enemy, s32 delta);           /* fade enemy tree alpha */
void fn_8004E448(Enemy* enemy, s32 arg, f32* pos);   /* missile/audio dispatch */
void fn_8004E5F8(Enemy* enemy);                     /* update milestone history */
void fn_8004E67C(void);                             /* update enemy texmods */
s32 check_vacancy(s32 index, f32* pos);             /* validate spawn position */
void fn_8004F1DC(Enemy* enemy);                     /* garm2 death-direction FX */

/* --- cross-module callees --- */
extern f32 fqdist(f32 x, f32 z);               /* 2D magnitude */
extern f32 NormalVector(f32* vector);
extern void fn_8005A65C(f32* worldmat, f32* coll_offset); /* refresh coll_pos */
extern s32 DeleteEffect(s32 idx, s32 mode);         /* sfx.c 0x80097790 */
extern void* EnemyWallCollide(f32 rad, f32* from, f32* to, f32* hitnrm); /* world probe */
extern s32 SlideAlongWall(f32 rad, f32* pos, f32* trans, f32* hitnrm, f32* out);
                                                    /* wall slide/deflect */
extern s32 check_enemy_pos(f32* start, f32* out, s32 slot);
extern s32 fn_8005D20C(s32 index, f32* oldc, f32* newc, s32 moved);
                                                    /* player collide + damage */
extern void CreateYPRMatrix(f32* mat, f32* pyr);        /* pyr -> rotation matrix */
extern void CopyMat3(f32* src, f32* dst);           /* 0x800BE8C8 */
extern void MBTreeSetFlags(struct mbnode* n, s32 a, s32 b); /* node show/update */
extern void MBTreeClearFlags(struct mbnode* n, s32 a, s32 b); /* node update */
extern s32 MBTreeGetAlpha(struct mbnode* n);
extern void MBTreeSetAlpha(struct mbnode* n, s32 alpha, s32 propagate);
extern void DoTexMods(void* data);
extern s32 EnemyStartMissile(Enemy* enemy, s32 arg, f32* pos, s32 kind);
extern void fn_8009DCE4(f32* pos);
extern void fn_8009DDFC(f32* pos);
extern void fn_8009DE2C(f32* pos);
extern void CreateDirMatrix(f32* matrix, f32* direction, f32* up);
extern void StartEnemyDeathFX(f32* matrix);

/* --- module data shared with other enemy helpers --- */
extern s32 gFrameTicks;      /* frame ticks (game speed units this frame) */
extern s32 gGameBusy;
extern s32 gGameplayPauseTimer;
extern f32 gClockFrameStep;   /* knockback integration scale */
extern f32 lbl_80344720;      /* current retreat/turn base angle */
extern void* lbl_80344730;    /* last worldobj hit by an enemy move */
extern s32 lbl_80344728;
extern s32 default_gen_count;
extern s32 lbl_8034471C;
extern f32 lbl_80346820;
extern f64 lbl_80346878;
extern f32 lbl_803468F0;
extern f64 lbl_80346A20;
extern f64 lbl_80346810;
extern f64 lbl_80346818;
extern f64 lbl_803469F8;
extern f64 lbl_80346A00;
extern const f32 lbl_80346A4C;
extern const f32 lbl_80346A50;
extern const f32 lbl_80346A54;
extern const f64 lbl_80346A58;
extern const f32 lbl_80346A60;
extern const f32 lbl_80346A64;
extern const f32 lbl_80346A68;
extern const f32 lbl_80346A6C;
typedef struct EnemyPlayerCharacterStats {
    s32 kills;
    u8 _004[0x01C - 0x004];
} EnemyPlayerCharacterStats;

typedef struct EnemyPlayerView {
    s32 index;
    u8 _004[0x00C - 0x004];
    s32 character;
    u8 _010[0x054 - 0x010];
    f32 position[3];
    u8 _060[0x064 - 0x060];
    f32 damage_position[3];
    u8 _070[0x0E8 - 0x070];
    s32 state;
    u8 _0EC[0x120 - 0x0EC];
    u32 flags;
    u8 _124[0x954 - 0x124];
    s16 it_enemy;
    u8 _956[0xA1E - 0x956];
    s16 attack_reflect;
    s16 attack_heal;
    u8 _A22[0xC10 - 0xA22];
    EnemyPlayerCharacterStats character_stats[16];
    u8 _DD0[0x1EB4 - 0xDD0];
    f32 health;
    u8 _1EB8[0x3324 - 0x1EB8];
    s32 level;
    u8 _3328[0x335C - 0x3328];
} EnemyPlayerView;
typedef union EnemyPlayerArray {
    f32 words[4][3287];
    EnemyPlayerView view[4];
} EnemyPlayerArray;
extern EnemyPlayerArray gPlayers; /* 0x80275AE0: four 0x335C player records */
#define gPlayerWords gPlayers.words
#define gEnemyPlayers gPlayers.view
extern f32 lbl_8023CA98[][4];
extern f32 lbl_8011BED8[];  /* 0x8011BED8 per-type turn-rate table */ /* wall-slide scratch; [1] = output vector */

/* Enemy records ride at +0xE18 inside the lbl_80250E00 pool block.  The
 * compiler folds that constant into each field displacement off the
 * pool-relative pointer, so a pool-relative enemy field is expressed as one
 * additive constant.  Do NOT replace these with a typed `Enemy*` alias: with
 * several nearby fields read off one index-computed base that defeats the
 * combined index-register addressing and regresses the function (A/B'd on
 * move_logic05's flee block: real 0 -> 83). */
#define ENEMY_POOL_OFF 0xE18
#define OFF_E(field) (ENEMY_POOL_OFF + offsetof(Enemy, field))

/* --- TU .bss (declaration order = address order; the compiler addresses the
 * whole block off the first symbol, lbl_80250E00 - gEnemies rides at +0xE18,
 * the world-probe hit normal lbl_802510F4 at +0x2F4). --- */
/* NOTE: MWCC allocates .bss in REVERSE declaration order - declare in reverse
 * address order so lbl_80250E00 lands at section offset 0 (the pool anchor)
 * and gEnemies at +0xE18, matching the target's base+displacement addressing. */
Enemy gEnemies[25];            /* 0x80251C18 */
u32 gWadAtreeHeaders[0x8B4 / 4];   /* 0x80251364 */
s32 lbl_802512B0[45];          /* 0x802512B0 per-type spawn-allowed */
s32 lbl_802511FC[45];          /* 0x802511FC per-type min-level class */
s32 lbl_80251148[45];          /* 0x80251148 per-type generator-fx enable */
u32 lbl_80251100[0x48 / 4];    /* 0x80251100 */
f32 lbl_802510F4[3];           /* 0x802510F4 world-probe hit normal */
typedef union EnemyRuntimePool {
    u32 words[0x2B4 / 4];
    struct {
        u32 prefix[0xB4 / 4];
        s32 milestoneIds[(0x2B4 - 0xB4) / 4];
    } view;
} EnemyRuntimePool;
typedef union EnemyRuntimeOwner {
    u32 words[(0x40 + 0x2B4) / 4];
    struct {
        u32 prefix[0x40 / 4];
        EnemyRuntimePool pool;
    } view;
} EnemyRuntimeOwner;
EnemyRuntimePool lbl_80250E40;  /* 0x80250E40 */
s32 lbl_80250E00[0x40 / 4];    /* 0x80250E00 enemy-type pool anchor */

/* .bss first-use-order referencer.  MWCC allocates referenced bss symbols in
 * FIRST-USE order (then unreferenced ones in reverse declaration order); in
 * the original TU the earlier functions touch the scratch arrays before any
 * gEnemies access, anchoring the pool at lbl_80250E00 with gEnemies at +0xE18.
 * This unreferenced static reproduces that order and is stripped by mwld
 * (stripped functions still order the section - see docs/matching-recipes). */
static void enemy_bss_order(void)
{
    lbl_80250E00[0] = 0;
    lbl_80250E40.words[0] = 0;
    lbl_802510F4[0] = 0.0f;
    lbl_80251100[0] = 0;
    lbl_80251148[0] = 0;
    lbl_802511FC[0] = 0;
    lbl_802512B0[0] = 0;
    gWadAtreeHeaders[0] = 0;
    gEnemies[0].type = E_SCORP;
}

f32 closest_enemy(f32 width, f32 range, f32* position, f32* direction,
                  f32* offset, s32* enemy_index, s32 flags)
{
    s32 best_index;
    u8 unused_before[4];
    f32 delta[3];
    u8 unused_after[12];
    f32 best_x;
    f32 best_y;
    f32 best_z;
    f32 best_distance;
    f32 spread;
    f64 maximum_vertical;
    s32 allow_death;

    best_index = -1;
    best_distance = range;
    spread = (lbl_80346810 - width) / range;
    StartItemGrid(range, position);
    maximum_vertical = lbl_80346818;
    allow_death = flags & 0x80000;
    while ((flags = NextGridItem()) >= 0) {
        Enemy* enemy = &gEnemies[flags];

        if (enemy->state == ACTIVE || enemy->state == SLEEP) {
            if (enemy->type != E_IT &&
                (enemy->type != E_DEATH || allow_death != 0)) {
                f32 vertical;
                f32 distance;

                delta[0] = enemy->objgrp.coll_pos[0] - position[0];
                delta[1] = enemy->objgrp.coll_pos[1] - position[1];
                delta[2] = enemy->objgrp.coll_pos[2] - position[2];
                vertical = delta[1];
                *(u32*)&vertical &= 0x7FFFFFFF;
                if ((f64)vertical > maximum_vertical) {
                    goto next_enemy;
                }
                distance = NormalVector(delta) - enemy->rad;
                if (distance > range) {
                    goto next_enemy;
                }
                {
                    f32 horizontal = fqdist(delta[0], delta[2]);
                    f32 cone = horizontal * (distance * spread + width);
                    f32 dot = delta[0] * direction[0] +
                              delta[2] * direction[2];

                    if (dot < cone) {
                        goto next_enemy;
                    }
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_x = delta[0];
                        best_y = delta[1];
                        best_z = delta[2];
                        best_index = flags;
                    }
                }
            }
        }
next_enemy:
        ;
    }

    if (best_index >= 0) {
        offset[0] = best_x;
        offset[1] = best_y;
        offset[2] = best_z;
    }
    if (enemy_index != 0) {
        *enemy_index = best_index;
    }
    return best_distance;
}

extern f64 lbl_803468D8;        /* 100.0 */
extern f64 lbl_80346988;        /* 6.0 */
extern f64 lbl_803468F8;        /* 0.10471975513333334 */
extern f32 lbl_80346964;        /* 1.5f */
extern f64 lbl_80346968;        /* 240.0 */
extern f32 lbl_80346970;        /* 999.0f */
extern f64 lbl_803469A0;        /* 90.0 */
extern f64 lbl_80346900;        /* 8.0 */
extern f64 lbl_80346908;        /* pi/4 */
extern f64 lbl_80346918;        /* pi/2 */
extern f64 lbl_80346930;        /* 0.26179938783333334 */
extern f64 lbl_80346840;        /* pi wrap high */
extern f64 lbl_80346848;        /* 2*pi */
extern f64 lbl_80346850;        /* -pi wrap low */
extern f64 lbl_80346858;        /* 0.1 */
extern f64 lbl_80346830;        /* 0.5 */
extern f32 lbl_803468B0;        /* 100000.0f */
extern f64 lbl_803468B8;        /* 3.0 */
extern f32 lbl_80346984;        /* 0.1745329f milestone turn */
extern u8 lbl_8011AF48[];       /* enemy.c .data anchor (turn tables at +4444/+4412...) */
extern f64 lbl_80346948;        /* 4.0 */

void do_enemy_move(s32 index)
{
    u8* row = (u8*)lbl_80250E00 + index * 916;
    Enemy* e = (Enemy*)(row + 3608); /* = &gEnemies[index] via the pool anchor */
    s32 alg = e->algorithm;
    f32 rad = e->rad;
    f32 hht = e->hht;
    s32 blocked = 0;
    s32 collide;
    f32 moveDistance;
    s32 result;
    s32 n;
    Enemy* other;
    f32 mat[16];
    u8 unused0[8];
    f32 oldpos[3];
    f32 rad2;
    u8 unused1[4];
    f32 oldc[3];
    u8 unused2[4];
    f32 newc[3];
    s32 hitWorld;
    u8 unused3[4];
    f32 half[3];
    u8 unused4[20];

    /* stun freeze + knockback integration */
    if (e->stun_timer > 0) {
        e->stun_timer -= gFrameTicks;
        e->trans[0] = 0.0f;
        e->trans[1] = 0.0f;
        e->trans[2] = 0.0f;
    }
    if (e->action >= 28) {
        e->trans[0] = 0.0f;
        e->trans[1] = 0.0f;
        e->trans[2] = 0.0f;
    }
    e->trans[0] += e->pushed[0] * gClockFrameStep;
    e->trans[1] += e->pushed[1] * gClockFrameStep;
    e->trans[2] += e->pushed[2] * gClockFrameStep;
    moveDistance = fqdist(e->trans[0], e->trans[2]);
    if (moveDistance > 0.001) {
        e->moved = 1;
    } else {
        e->moved = 0;
    }

    collide = do_enemy_collide(index, moveDistance);

    /* commit the move to the world matrix + collision point */
    oldpos[0] = e->objgrp.worldmat[3][0];
    oldpos[1] = e->objgrp.worldmat[3][1];
    oldpos[2] = e->objgrp.worldmat[3][2];
    e->objgrp.worldmat[3][0] += e->trans[0];
    e->objgrp.worldmat[3][1] += e->trans[1];
    e->objgrp.worldmat[3][2] += e->trans[2];
    oldc[0] = e->objgrp.coll_pos[0];
    oldc[1] = e->objgrp.coll_pos[1];
    oldc[2] = e->objgrp.coll_pos[2];
    newc[0] = oldc[0] + e->trans[0];
    newc[1] = oldc[1] + e->trans[1];
    newc[2] = oldc[2] + e->trans[2];

    /* generator contact: full revert + retreat toward the generator */
    if (e->visactive != 0) {
        e->coll_pnum = fn_80046680((f32)(0.5 + rad), hht, index, 0, oldc, newc);
    } else {
        e->coll_pnum = -1;
    }
    if (e->coll_pnum >= 0) {
        e->coll_enenum = -1;
        e->coll_ip = 0;
        e->moved = 0;
        e->objgrp.worldmat[3][0] = oldpos[0];
        e->objgrp.worldmat[3][1] = oldpos[1];
        e->objgrp.worldmat[3][2] = oldpos[2];
        e->trans[0] = 0.0f;
        e->trans[1] = 0.0f;
        e->trans[2] = 0.0f;
        fn_8005A65C(&e->objgrp.worldmat[0][0], e->coll_offset);
        e->route = fn_8004CFAC(&e->objgrp.worldmat[3][0],
                               &gPlayerWords[e->coll_pnum][17]);
        fn_80046140(index);
    } else {
        hitWorld = 0;
        if (e->type == E_DEATH && e->specialfx >= 0) {
            e->specialfx = DeleteEffect(e->specialfx, 0);
        }
        if (e->attack_timer > 0) {
            if ((e->attack_timer -= gFrameTicks) <= 0) {
                e->attack_timer = 0;
            }
        }
        if (collide == 0) {
            e->coll_enenum = fn_8004646C(rad, hht, index, oldc, newc, newc, &hitWorld);
        } else {
            e->coll_enenum = fn_8004646C(rad, hht, index, oldc, newc, newc, 0);
        }
        if (e->coll_enenum >= 0) {
            /* hit another enemy */
            e->coll_ip = 0;
            other = 0;
            n = e->coll_enenum;
            if (n < 0x10000) {
                gEnemies[n].coll_enenum = index;
                other = (Enemy*)&gEnemies[n];
            }
            if (hitWorld != 0) {
                /* the probe clipped the move against the world: retry the
                 * clipped translation against world objects */
                e->trans[0] = newc[0] - e->objgrp.coll_pos[0];
                e->trans[1] = newc[1] - e->objgrp.coll_pos[1];
                e->trans[2] = newc[2] - e->objgrp.coll_pos[2];
                half[0] = oldpos[0] + e->trans[0];
                rad2 = (f32)(rad * 1.5);
                half[1] = oldpos[1] + e->trans[1];
                half[2] = oldpos[2] + e->trans[2];
                lbl_80344730 = EnemyWallCollide(rad2, oldpos, half, lbl_802510F4);
                if (lbl_80344730 != 0) {
                    EnemyWorldDamage(e, lbl_80344730, oldpos, lbl_802510F4);
                    if (*(u32*)((u8*)lbl_80344730 + 16) & 0x38) {
                        result = 0;
                    } else if (!(e->ai_flags & 1)
                               && SlideAlongWall(rad2, oldpos, e->trans,
                                              lbl_802510F4, lbl_8023CA98[1]) < 0) {
                        result = 2;
                        e->trans[2] = 0.0f;
                        e->trans[0] = 0.0f;
                    } else {
                        result = 1;
                    }
                } else {
                    result = 0;
                }
                if (result != 0) {
                    hitWorld = 0;
                } else {
                    /* free half-step along the clipped translation */
                    e->objgrp.worldmat[3][0] = oldpos[0] + 0.5 * e->trans[0];
                    e->objgrp.worldmat[3][1] = oldpos[1] + 0.5 * e->trans[1];
                    e->objgrp.worldmat[3][2] = oldpos[2] + 0.5 * e->trans[2];
                }
            }
            if (hitWorld == 0) {
                if (other != 0 && e->pushmag2 > 1.0 && e->action >= 28) {
                    /* being knocked back: transfer half the push */
                    other->pushed[0] = 0.5 * e->pushed[0] + other->pushed[0];
                    other->pushed[1] = 0.5 * e->pushed[1] + other->pushed[1];
                    other->pushed[2] = 0.5 * e->pushed[2] + other->pushed[2];
                    other->trans[0] = 0.5 * e->trans[0];
                    other->trans[1] = 0.5 * e->trans[1];
                    other->trans[2] = 0.5 * e->trans[2];
                } else {
                    /* blocked: full revert */
                    e->moved = 0;
                    blocked = 1;
                    e->objgrp.worldmat[3][0] = oldpos[0];
                    e->objgrp.worldmat[3][1] = oldpos[1];
                    e->objgrp.worldmat[3][2] = oldpos[2];
                    e->trans[0] = 0.0f;
                    e->trans[1] = 0.0f;
                    e->trans[2] = 0.0f;
                }
            }
            fn_8005A65C(&e->objgrp.worldmat[0][0], e->coll_offset);
            if (other != 0 && alg == 0) {
                e->route = fn_8004CFAC(&e->objgrp.worldmat[3][0],
                                       &other->objgrp.worldmat[3][0]);
                if (e->dead_end <= 0) {
                    e->dead_end = 60;
                    if (e->daction == 3 || e->daction == 4) {
                        e->daction = 0;
                    }
                }
            } else if (other != 0
                       && (alg == 7 || alg == 8 || alg == 10 || alg == 20)) {
                if (e->route == 0 || ABS(e->route) > 2) {
                    e->route = fn_8004CFAC(&e->objgrp.worldmat[3][0],
                                           &other->objgrp.worldmat[3][0]);
                    e->collided = 0;
                }
                if (alg == 7) {
                    if (ABS(e->route) <= 2) {
                        e->collided++;
                        fn_8004D030(index, 15);
                    } else {
                        fn_8004D030(index, 50);
                        e->ang = lbl_80344720;
                        e->pyr[1] = lbl_80344720;
                        e->collided = 0;
                        e->route = 0;
                    }
                    if (e->collided >= 7) {
                        e->route = -e->route * 2;
                        e->collided = 0;
                    }
                } else if (alg == 8) {
                    if (ABS(e->route) <= 2) {
                        e->collided++;
                        fn_8004D030(index, 10);
                    } else {
                        fn_8004D030(index, 60);
                        e->ang = lbl_80344720;
                        e->pyr[1] = lbl_80344720;
                        e->collided = 0;
                        e->route = 0;
                    }
                    if (e->collided >= 7) {
                        e->route = -e->route * 2;
                        e->collided = 0;
                    }
                } else if (alg == 10) {
                    if (ABS(e->route) <= 2) {
                        e->collided++;
                        fn_8004D030(index, 15);
                    } else {
                        fn_8004D030(index, 50);
                        e->ang = lbl_80344720;
                        e->pyr[1] = lbl_80344720;
                        e->collided = 0;
                        e->route = 0;
                    }
                    if (e->collided >= 7) {
                        e->route = -e->route * 2;
                        e->collided = 0;
                    }
                } else if (alg == 20) {
                    if (ABS(e->route) <= 2) {
                        e->collided++;
                        fn_8004D030(index, 10);
                    } else {
                        fn_8004D030(index, 30);
                        {
                            f64 a;
                            f64 hi = lbl_80346840;
                            f32 step = lbl_80344720;
                            e->ang = (f32)(hi + step);
                            a = e->ang;
                            if (a > hi) {
                                a -= lbl_80346848;
                            } else if (a <= lbl_80346850) {
                                a = lbl_80346848 + a;
                            }
                            e->ang = a;
                            e->pyr[1] = a;
                        }
                        e->collided = 0;
                        e->route = 0;
                    }
                    if (e->collided >= 7) {
                        e->route = -e->route * 2;
                        e->collided = 0;
                    }
                }
            } else {
                if (e->dead_end <= 0) {
                    e->dead_end = 20;
                }
            }
            e->area = 2;
        }
        if (blocked == 0
            && fn_8005D20C(index, oldc, newc, e->moved) != 0) {
            /* hit a player: full revert + per-algorithm turn logic */
            e->moved = 0;
            blocked = 1;
            e->objgrp.worldmat[3][0] = oldpos[0];
            e->objgrp.worldmat[3][1] = oldpos[1];
            e->objgrp.worldmat[3][2] = oldpos[2];
            e->trans[0] = 0.0f;
            e->trans[1] = 0.0f;
            e->trans[2] = 0.0f;
            fn_8005A65C(&e->objgrp.worldmat[0][0], e->coll_offset);
            if (alg == 0) {
                if (*(u32*)((u8*)e->coll_ip + 100) != 0) {
                    e->route = fn_8004CFAC(&e->objgrp.worldmat[3][0],
                                           (f32*)((u8*)e->coll_ip + 52));
                }
                if (e->dead_end <= 0) {
                    e->dead_end = 60;
                    if (e->daction == 3 || e->daction == 4) {
                        e->daction = 0;
                    }
                }
            } else if (alg == 7 || alg == 8 || alg == 10 || alg == 20) {
                if (*(u32*)((u8*)e->coll_ip + 100) != 0) {
                    if (e->route == 0 || ABS(e->route) > 2) {
                        e->route = fn_8004CFAC(&e->objgrp.worldmat[3][0],
                                               (f32*)((u8*)e->coll_ip + 52));
                        e->collided = 0;
                    }
                    if (alg == 7) {
                        if (ABS(e->route) <= 2) {
                            e->collided++;
                            fn_8004D030(index, 15);
                        } else {
                            fn_8004D030(index, 15);
                            e->ang = lbl_80344720;
                            e->pyr[1] = lbl_80344720;
                            e->collided = 0;
                            e->route = 0;
                        }
                        if (e->collided >= 7) {
                            e->route = -e->route * 2;
                            e->collided = 0;
                        }
                    } else if (alg == 8) {
                        if (ABS(e->route) <= 2) {
                            e->collided++;
                            fn_8004D030(index, 15);
                        } else {
                            fn_8004D030(index, 15);
                            e->ang = lbl_80344720;
                            e->pyr[1] = lbl_80344720;
                            e->collided = 0;
                            e->route = 0;
                        }
                        if (e->collided >= 7) {
                            e->route = -e->route * 2;
                            e->collided = 0;
                        }
                    } else if (alg == 10) {
                        if (ABS(e->route) <= 2) {
                            e->collided++;
                            fn_8004D030(index, 15);
                        } else {
                            fn_8004D030(index, 15);
                            e->ang = lbl_80344720;
                            e->pyr[1] = lbl_80344720;
                            e->collided = 0;
                            e->route = 0;
                        }
                        if (e->collided >= 7) {
                            e->route = -e->route * 2;
                            e->collided = 0;
                        }
                    } else if (alg == 20) {
                        if (ABS(e->route) <= 2) {
                            e->collided++;
                            fn_8004D030(index, 15);
                        } else {
                            fn_8004D030(index, 15);
                            {
                                f64 a;
                                f64 hi = lbl_80346840;
                                f32 step = lbl_80344720;
                                e->ang = (f32)(hi + step);
                                a = e->ang;
                                if (a > hi) {
                                    a -= lbl_80346848;
                                } else if (a <= lbl_80346850) {
                                    a = lbl_80346848 + a;
                                }
                                e->ang = a;
                                e->pyr[1] = a;
                            }
                            e->collided = 0;
                            e->route = 0;
                        }
                        if (e->collided >= 7) {
                            e->route = -e->route * 2;
                            e->collided = 0;
                        }
                    }
                } else {
                    if (e->dead_end <= 0) {
                        e->dead_end = 20;
                    }
                }
            } else {
                if (e->dead_end <= 0) {
                    e->dead_end = 20;
                }
            }
            e->area = 3;
        }
        if (blocked == 0) {
            if (alg == 0 && e->dead_end <= 0) {
                e->route = 1;
                e->collided = 0;
            } else if (alg == 2 || alg == 4) {
                if (--e->play <= 0) {
                    e->count = 0;
                }
            }
        }
    }

    /* rebuild the object matrix + service the shadow node */
    if (e->state != 0) {
        if (e->pushmag2 > 0.1) {
            e->pyr[1] = e->pushang;
        }
        CreateYPRMatrix(mat, e->pyr);
        CopyMat3(mat, &e->objgrp.worldmat[0][0]);
        if (e->shadow != 0) {
            *(f32*)((u8*)e->shadow + 48) = e->objgrp.worldmat[3][0];
            *(f32*)((u8*)e->shadow + 52) = e->objgrp.worldmat[3][1];
            *(f32*)((u8*)e->shadow + 56) = e->objgrp.worldmat[3][2];
            if (e->action == 1) {
                MBTreeSetFlags(e->shadow, 2, 0);
            } else {
                MBTreeClearFlags(e->shadow, 2, 0);
            }
        }
    }

    /* stuck-walk watchdog */
    if (e->moved != 0
        || (e->action != 3 && e->action != 4 && e->action != 0)) {
        e->stopped = 0;
    } else {
        e->stopped += gFrameTicks;
    }
    if (e->stopped > 180) {
        e->stopped = 0;
    }
    if (e->stopped > 60 && (e->daction == 3 || e->daction == 4)
        && alg != 18 && e->type != E_GOLEM) {
        e->daction = 0;
    }
}

/* do_enemy_collide @0x80045488 - the enemy collision core.  Sweeps the pending
 * move (e->trans) against the world: probes walls (splitting the swept box for
 * the 0x1d flyer type), resolves wall hits by damage + slide-or-stop, tests
 * enemy-vs-enemy via fn_80045C30, snaps to the floor, reparents the mb-node to
 * whatever surface it landed on, and - when the residual slide is tiny - runs
 * the per-behaviour dead-end timers (turn/reverse).  Finally applies gravity
 * toward the floor target, dealing fall damage past the drop threshold.
 * Returns the collision class (0 none, 1 wall, 2 blocked). */
extern void* fn_80045C30(Enemy* e, f32 rad, f32 arg, f32* oldpos, f32* trans,
                         s32 collided);
extern void MBNodeSetParent(void* node, void* parent);
extern void* FloorCollide(f32* pos, s32 a, s32 b, s32 mode, f32 x, f32 y, f32 z);
extern s32 damage_enemy(Enemy* e, f32 amount, s32 dtype, s32 a, s32 b, s32 c,
                        s32 d);
extern f64 lbl_80346860;
extern f64 lbl_80346868;
extern f64 lbl_80346838;
extern f64 lbl_80346870;
extern f64 lbl_80346880;
extern f32 lbl_80346888;
extern s32 lbl_8034473C;
extern f64 lbl_80346830;
extern s32 AddExp(s32 player, s32 amount, s32 mode);
extern s32 damage_player(s32 player, f32 amount, s32 mode, u32 flags,
                         f32* direction);
extern s32 msgPost(s32 message, s32 player, void* position);
extern s32 StartDeathFX(void* node, s32 kind, s32 flags);
extern void AudioPlayEvt102Follow(f32* position, s32 player);
extern void AudioPlayEvt104(f32* position);
extern s32 SuicideExplosion(f32* position, f32 damage);
extern void fn_8009DAC8(f32* position);
extern s32 lbl_80344718;
extern s32 lbl_803447E4;
extern s32 lbl_80344B24;
extern f64 lbl_80346898;
extern f64 lbl_803468A8;
extern f64 lbl_80346858;
extern f64 lbl_80346890;
extern f32 lbl_80344880;
extern level_data* gCurLevel;
extern void RequestEnemyAction(Enemy* enemy, s32 action);

/* file-local view of world/worldcol.c's FloorCollisionResult; only the
 * floorY field (0x34) is needed here. Layout verified against worldcol.c's
 * FloorCollisionResult typedef (_pad00[0x34]; f32 floorY; ...). Used both
 * for the scratch FloorCollide() output buffer below and (later in this
 * file) for the shared gFloorCollisionResult global -- same shape, two
 * different instances. */
typedef struct FloorCollisionResultView {
    u8 _pad00[0x34];
    f32 floorY;
} FloorCollisionResultView;

s32 do_enemy_collide(s32 index, f32 retryThreshold)
{
    u8* pool = (u8*)lbl_80250E00;
    u8* e0;
    u8* e;
    Enemy* enemy;
    s32 type;
    f32* tr;
    f32 rad;
    f32 dt;
    s32 behavior;
    s32 result = 0;
    f32 slideRad;
    WorldObj* hit = NULL;
    u8 framePad[4];
    f32 oldpos[3];
    f32 dh;
    u8 unused[4];

    (void)framePad;
    (void)unused;

    e0 = pool + index * 916;
    type = *(s32*)(e0 += 3608);
    e = e0;
    enemy = (Enemy*)e0;
    tr = enemy->trans;
    dt = (f32)(lbl_80346860 * gClockFrameStep);
    behavior = enemy->algorithm;

    if (type == 0x1F || enemy->dead_end <= 0) {
        enemy->area = 0;
    }
    if (enemy->moved == 0) {
        WorldObj* mp = enemy->floor_wobj;
        if (mp != NULL && !(mp->flags & 0x1000)) {
            goto gravity;
        }
    }

    rad = enemy->rad;
    oldpos[0] = enemy->objgrp.coll_pos[0];
    oldpos[1] = enemy->objgrp.coll_pos[1];
    oldpos[2] = enemy->objgrp.coll_pos[2];
    oldpos[1] = (f32)((lbl_80346868 - enemy->flooroffset) + oldpos[1]);

    if (enemy->moved != 0) {
        if (enemy->type == 0x1D) {
            f32 np[3];
            u8 npPad[4];
            s32 wallResult;

            (void)npPad;
            slideRad = (f32)(lbl_80346830 * rad);
            slideRad *= lbl_80346838;
            np[0] = oldpos[0] + tr[0];
            np[1] = oldpos[1] + tr[1];
            np[2] = oldpos[2] + tr[2];
            lbl_80344730 = EnemyWallCollide(slideRad, oldpos, np,
                                                 (f32*)(pool + 0x2F4));
            if (lbl_80344730 != 0) {
                EnemyWorldDamage(enemy, lbl_80344730, oldpos,
                                 (f32*)(pool + 0x2F4));
                if (*(u32*)((u8*)lbl_80344730 + 0x10) & 0x38) {
                    wallResult = 0;
                } else {
                    if (!(*(u32*)(e + offsetof(Enemy, ai_flags)) & 1) &&
                        SlideAlongWall(slideRad, oldpos, tr,
                                       (f32*)(pool + 0x2F4),
                                       lbl_8023CA98[1]) < 0) {
                        tr[2] = 0.0f;
                        tr[0] = 0.0f;
                        wallResult = 2;
                    } else {
                        wallResult = 1;
                    }
                }
            } else {
                wallResult = 0;
            }
            result = wallResult;
        } else {
            f32 np[3];
            u8 npPad[4];
            s32 wallResult;

            (void)npPad;
            slideRad = (f32)(rad * lbl_80346838);
            np[0] = oldpos[0] + tr[0];
            np[1] = oldpos[1] + tr[1];
            np[2] = oldpos[2] + tr[2];
            lbl_80344730 = EnemyWallCollide(slideRad, oldpos, np,
                                                 (f32*)(pool + 0x2F4));
            if (lbl_80344730 != 0) {
                EnemyWorldDamage(enemy, lbl_80344730, oldpos,
                                 (f32*)(pool + 0x2F4));
                if (*(u32*)((u8*)lbl_80344730 + 0x10) & 0x38) {
                    wallResult = 0;
                } else {
                    if (!(*(u32*)(e + offsetof(Enemy, ai_flags)) & 1) &&
                        SlideAlongWall(slideRad, oldpos, tr,
                                       (f32*)(pool + 0x2F4),
                                       lbl_8023CA98[1]) < 0) {
                        tr[2] = 0.0f;
                        tr[0] = 0.0f;
                        wallResult = 2;
                    } else {
                        wallResult = 1;
                    }
                }
            } else {
                wallResult = 0;
            }
            result = wallResult;
        }
    }

    hit = fn_80045C30(enemy, rad, retryThreshold, oldpos, tr, result);
    if (hit != NULL) {
        enemy->floor_wobj = hit;
        if ((f64)enemy->hht <= lbl_80346868) {
            if (hit->flags & 0x38) {
                result = 2;
                tr[2] = 0.0f;
                tr[0] = 0.0f;
            }
        }
    } else {
        WorldObj* mp = enemy->floor_wobj;
        if (mp != NULL && !(mp->flags & 0x1000)) {
            goto reparent;
        }
        tr[2] = 0.0f;
        tr[0] = 0.0f;
        hit = FloorCollide(oldpos, (s32)(pool + 0x300), 0, 2,
                           (f32)(lbl_80346830 * rad), enemy->hht,
                           (f32)(-enemy->hht - lbl_80346870));
        if (hit != NULL) {
            enemy->floory = ((FloorCollisionResultView*)(pool + 0x300))->floorY +
                            enemy->flooroffset;
            if (enemy->shadow != NULL) {
                CopyMat3((f32*)(pool + 0x300), (f32*)enemy->shadow);
            }
        }
    }

reparent:
    if (hit != NULL) {
        void* parent;
        if ((parent = hit->nodeptr) != NULL && (hit->flags & 0x1000)) {
            MBNodeSetParent(enemy->objgrp.node, parent);
        } else {
            MBNodeSetParent(enemy->objgrp.node, (void*)lbl_8034473C);
        }
    }
    enemy->floor_surf = (hit != NULL) ? hit->flags : 0;

    if (!((f64)fqdist(tr[0], tr[2]) < lbl_80346878)) {
        goto gravity;
    }

    if (behavior == 0) {
        if (ABS_REVERSED(*(s32*)(e + offsetof(Enemy, route))) <= 2) {
            (*(s16*)(e + offsetof(Enemy, collided)))++;
            fn_8004D030(index, 5);
        } else {
            (*(s16*)(e + offsetof(Enemy, collided)))++;
            fn_8004D030(index, 0x3C);
        }
        if (*(s16*)(e + offsetof(Enemy, collided)) >= 9) {
            *(s32*)(e + offsetof(Enemy, route)) = -*(s32*)(e + offsetof(Enemy, route)) * 2;
            *(s16*)(e + offsetof(Enemy, collided)) = 0;
            if (ABS_REVERSED(*(s32*)(e + offsetof(Enemy, route))) > 2) {
                *(f32*)(e + offsetof(Enemy, ang)) = lbl_80344720;
                *(f32*)(e + offsetof(Enemy, pyr[1])) = lbl_80344720;
            }
        }
    } else if (behavior == 7) {
        if (ABS_REVERSED(*(s32*)(e + offsetof(Enemy, route))) <= 2) {
            (*(s16*)(e + offsetof(Enemy, collided)))++;
            fn_8004D030(index, 0xA);
        } else {
            fn_8004D030(index, 0x3C);
            *(f32*)(e + offsetof(Enemy, ang)) = lbl_80344720;
            *(f32*)(e + offsetof(Enemy, pyr[1])) = lbl_80344720;
            *(s16*)(e + offsetof(Enemy, collided)) = 0;
            *(s32*)(e + offsetof(Enemy, route)) = 0;
        }
        if (*(s16*)(e + offsetof(Enemy, collided)) >= 7) {
            *(s32*)(e + offsetof(Enemy, route)) = -*(s32*)(e + offsetof(Enemy, route)) * 2;
            *(s16*)(e + offsetof(Enemy, collided)) = 0;
        }
    } else if (behavior == 8) {
        if (ABS_REVERSED(*(s32*)(e + offsetof(Enemy, route))) <= 2) {
            (*(s16*)(e + offsetof(Enemy, collided)))++;
            fn_8004D030(index, 5);
        } else {
            fn_8004D030(index, 0x3C);
            *(f32*)(e + offsetof(Enemy, ang)) = lbl_80344720;
            *(f32*)(e + offsetof(Enemy, pyr[1])) = lbl_80344720;
            *(s16*)(e + offsetof(Enemy, collided)) = 0;
            *(s32*)(e + offsetof(Enemy, route)) = 0;
        }
        if (*(s16*)(e + offsetof(Enemy, collided)) >= 7) {
            *(s32*)(e + offsetof(Enemy, route)) = -*(s32*)(e + offsetof(Enemy, route)) * 2;
            *(s16*)(e + offsetof(Enemy, collided)) = 0;
        }
    } else if (behavior == 0xA) {
        if (ABS_REVERSED(*(s32*)(e + offsetof(Enemy, route))) <= 2) {
            (*(s16*)(e + offsetof(Enemy, collided)))++;
            fn_8004D030(index, 0xA);
        } else {
            fn_8004D030(index, 0x3C);
            *(f32*)(e + offsetof(Enemy, ang)) = lbl_80344720;
            *(f32*)(e + offsetof(Enemy, pyr[1])) = lbl_80344720;
            *(s16*)(e + offsetof(Enemy, collided)) = 0;
            *(s32*)(e + offsetof(Enemy, route)) = 0;
        }
        if (*(s16*)(e + offsetof(Enemy, collided)) >= 7) {
            *(s32*)(e + offsetof(Enemy, route)) = -*(s32*)(e + offsetof(Enemy, route)) * 2;
            *(s16*)(e + offsetof(Enemy, collided)) = 0;
        }
    } else if (behavior == 0x14) {
        if (ABS_REVERSED(*(s32*)(e + offsetof(Enemy, route))) <= 2) {
            (*(s16*)(e + offsetof(Enemy, collided)))++;
            fn_8004D030(index, 3);
        } else {
            f64 high;

            fn_8004D030(index, 0x1E);
            high = lbl_80346840;
            *(f32*)(e + offsetof(Enemy, ang)) = (f32)(high + lbl_80344720);
            {
                f64 a;

                if ((a = *(f32*)(e + offsetof(Enemy, ang))) > high) {
                    a -= lbl_80346848;
                } else if (a <= lbl_80346850) {
                    a = lbl_80346848 + a;
                }
                *(f32*)(e + offsetof(Enemy, ang)) = (f32)a;
                *(f32*)(e + offsetof(Enemy, pyr[1])) = (f32)a;
            }
            *(s16*)(e + offsetof(Enemy, collided)) = 0;
            *(s32*)(e + offsetof(Enemy, route)) = 0;
        }
        if (*(s16*)(e + offsetof(Enemy, collided)) >= 7) {
            *(s32*)(e + offsetof(Enemy, route)) = -*(s32*)(e + offsetof(Enemy, route)) * 2;
            *(s16*)(e + offsetof(Enemy, collided)) = 0;
        }
    } else {
        if (*(s32*)(e + offsetof(Enemy, dead_end)) <= 0) {
            *(s32*)(e + offsetof(Enemy, dead_end)) = 0x14;
        }
    }
    *(s16*)(e + offsetof(Enemy, area)) = 1;

gravity:
    dh = *(f32*)(e + offsetof(Enemy, floory)) - *(f32*)(e + offsetof(Enemy, objgrp.worldmat[3][1]));
    if ((f64)dh < lbl_80346880) {
        damage_enemy(enemy, lbl_80346888, -1, 0, 0, 0, 0);
    }
    if (dh < dt) {
        dh = dt;
    }
    tr[1] += dh;
    *(f32*)(e + offsetof(Enemy, floory)) = *(f32*)(e + offsetof(Enemy, objgrp.worldmat[3][1])) + dh;
    return result;
}

static s32 EnemyMovingAwayFromBirth(Enemy* enemy, f32* oldPosition,
                                    f32* translation)
{
    f32 movedY;
    f32 dy;
    f32 movedX;
    f32 dz;
    f32 dx;
    f32 movedZ;

    dy = enemy->birth_pos[1] - oldPosition[1];
    dx = enemy->birth_pos[0] - oldPosition[0];
    movedY = dy + translation[1];
    dz = enemy->birth_pos[2] - oldPosition[2];
    movedX = dx + translation[0];
    movedZ = dz + translation[2];
    return movedZ * movedZ + (movedX * movedX + movedY * movedY) >
           dz * dz + (dx * dx + dy * dy);
}

void* fn_80045C30(Enemy* enemy, f32 radius, f32 retryThreshold,
                  f32* oldPosition, f32* translation, s32 collisionClass)
{
    u8* pool;
    f32* floorYAddress;
    void* floorObject;
    f32 tolerance;
    f32 distance;
    f32 probe[3];
    f32 step[3];
    u8 unused[20];
    f32 baseY;
    f64 halfRadius;
    f32 floorY;

    (void)unused;
    pool = (u8*)lbl_80250E00;
    tolerance = (f32)(lbl_80346868 *
                      (lbl_80346858 + (f64)(retryThreshold + radius)));
    if (enemy->type == E_GOLEM || (f64)enemy->hht <= lbl_80346868) {
        if (EnemyMovingAwayFromBirth(enemy, oldPosition, translation)) {
            tolerance = (f32)(lbl_80346830 *
                              (retryThreshold + radius));
        }
    }

    step[0] = translation[0];
    step[1] = translation[1];
    step[2] = translation[2];
    distance = radius + NormalVector(step);
    step[0] *= distance;
    step[1] *= distance;
    step[2] *= distance;
    probe[0] = oldPosition[0] + step[0];
    probe[1] = oldPosition[1] + step[1];
    probe[2] = oldPosition[2] + step[2];

    halfRadius = lbl_80346830 * radius;
    floorObject = (void*)FloorCollide(
        probe, (s32)(pool + 0x300), 0, 2, (f32)halfRadius, enemy->hht,
        (f32)(-enemy->hht - lbl_80346870));
    if (floorObject != 0) {
        EnemyWorldDamage(enemy, floorObject, oldPosition,
                         (f32*)(pool + 0x330));
    } else {
        if ((f64)enemy->pushmag2 < lbl_80346878) {
            translation[0] = translation[2] = lbl_80346820;
        } else {
            enemy->floory = (f32)((f64)lbl_80344880 - lbl_80346890);
        }
        return 0;
    }

    floorYAddress = (f32*)(pool + 0x334);
    baseY = enemy->floory - enemy->flooroffset;
    floorY = *floorYAddress;
    distance = floorY - baseY;
    if (distance < lbl_80346820) {
        distance = -distance;
    }
    if (distance > tolerance) {
        if ((f64)enemy->pushmag2 < lbl_80346878) {
            f32 zero = *(volatile f32*)&lbl_80346820;
            translation[2] = zero;
            translation[0] = zero;
        } else {
            enemy->floory = floorY + enemy->flooroffset;
        }
        return 0;
    }

    if ((f64)retryThreshold > lbl_80346898) {
        if ((f64)distance > lbl_80346858 * retryThreshold) {
            probe[0] = oldPosition[0] + translation[0];
            probe[1] = oldPosition[1] + translation[1];
            probe[2] = oldPosition[2] + translation[2];
            floorObject = (void*)FloorCollide(
                probe, (s32)(pool + 0x300), 0, 2, (f32)halfRadius,
                enemy->hht, (f32)(-enemy->hht - lbl_80346870));
            if (floorObject == 0) {
                translation[0] = translation[2] = lbl_80346820;
                return 0;
            }
            floorY = *floorYAddress;
        }
    }

    if ((u32)(collisionClass - 1) <= 1 ||
        (collisionClass == 3 && lbl_80344730 != floorObject)) {
        if (collisionClass != 1 && collisionClass != 3) {
            goto collision_blocked;
        }
        if ((enemy->ai_flags & 1) != 0) {
            goto collision_blocked;
        }
        if (SlideAlongWall(radius, oldPosition, translation,
                           (f32*)(pool + 0x2F4), lbl_8023CA98[1]) < 0) {
            f32 zero = lbl_80346820;
            translation[2] = zero;
            translation[0] = zero;
            return 0;
        }
        enemy->floory = floorY + enemy->flooroffset;
        goto collision_done;
    collision_blocked:
        {
            f32 zero = lbl_80346820;
            translation[2] = zero;
            translation[0] = zero;
            return 0;
        }
    }
collision_done:
    enemy->floory = floorY + enemy->flooroffset;
    if (enemy->shadow != 0) {
        CopyMat3((f32*)(pool + 0x300), (f32*)enemy->shadow);
    }
    return floorObject;
}

void fn_80046140(s32 index)
{
    u8* pool = (u8*)lbl_80250E00;
    Enemy* enemy = (Enemy*)(pool + index * 0x394 + 0xE18);
    s32 playerIndex;
    s32 damaged;

    if (enemy->type == E_DEATH) {
        if (enemy->stun_timer <= 0 &&
            (gEnemyPlayers[enemy->coll_pnum].flags & 0x80000) == 0) {
            if (enemy->endurance > 0) {
                enemy->endurance--;
            } else {
                RequestEnemyAction(enemy, E_ATTACK);
                if ((enemy->attack_timer -= gFrameTicks) <= 0) {
                    enemy->attack_timer += 3;
                    if (enemy->org_lvl == 2) {
                        AddExp(enemy->coll_pnum, -1, -2);
                        damaged = 0;
                    } else {
                        damaged = damage_player(enemy->coll_pnum, -enemy->atts.fight,
                                                1, 0x1000, 0);
                    }
                    if (enemy->org_lvl == 2) {
                        msgPost(0x80, enemy->coll_pnum, &enemy->objgrp.attn_pos[0]);
                    } else {
                        msgPost(0x82, enemy->coll_pnum, &enemy->objgrp.attn_pos[0]);
                    }
                    lbl_803447E4 = 1;
                    enemy->attack_count++;
                    if (damaged != 0) {
                        enemy->health = lbl_80346820;
                    } else if (enemy->specialfx < 0) {
                        enemy->specialfx = StartDeathFX(enemy->objgrp.node,
                                                       enemy->org_lvl, 0);
                    }
                    if ((f64)(enemy->health -= enemy->atts.fight) >= lbl_80346898) {
                        AudioPlayEvt102Follow(&enemy->objgrp.worldmat[3][0],
                                              enemy->coll_pnum);
                        lbl_80344718 = 1;
                    } else {
                        enemy->flag2 = 1;
                        AudioPlayEvt104(&enemy->objgrp.worldmat[3][0]);
                        playerIndex = enemy->coll_pnum;
                        pool = (u8*)(enemy - (Enemy*)(pool + 0xE18));
                        enemy->health = lbl_80346820;
                        enemy->state = DYING;
                        enemy->area = (s16)playerIndex;
                        if (enemy->algorithm == 18) {
                            SuicideExplosion(&enemy->objgrp.coll_pos[0],
                                (f32)(lbl_803468A8 * gCurLevel->ene_damage));
                            fn_8009DAC8(&enemy->objgrp.coll_pos[0]);
                        }
                        uncouple_enemy((s32)pool);
                    }
                }
            }
        }
    } else if (enemy->type == E_IT) {
        if (lbl_80344B24 >= 0) {
            gEnemyPlayers[lbl_80344B24].it_enemy = 0;
        }
        lbl_80344B24 = enemy->coll_pnum;
        gEnemyPlayers[lbl_80344B24].it_enemy = 1;
        msgPost(0x32, gEnemyPlayers[lbl_80344B24].index,
                &gEnemyPlayers[lbl_80344B24].position[0]);

        playerIndex = lbl_80344B24;
        pool = (u8*)(enemy - (Enemy*)(pool + 0xE18));
        enemy->health = lbl_80346820;
        enemy->state = DYING;
        enemy->area = (s16)playerIndex;
        if (enemy->algorithm == 18) {
            SuicideExplosion(&enemy->objgrp.coll_pos[0],
                (f32)(lbl_803468A8 * gCurLevel->ene_damage));
            fn_8009DAC8(&enemy->objgrp.coll_pos[0]);
        }
        uncouple_enemy((s32)pool);
    } else if (enemy->algorithm != 31) {
        enemy->attack_index = (s16)enemy->coll_pnum;
        if ((enemy->attack_count & 7) == 7) {
            RequestEnemyAction(enemy, E_ATTACK_PWR);
        } else {
            RequestEnemyAction(enemy, E_ATTACK);
        }
    }
}

/* ===================================================================== *
 *  AI MOVE-LOGIC STATE HANDLERS  (do_ai jumptable, 0x80046B54..0x8004C650)
 *  Each takes the enemy slot index, sets a desired facing/velocity, then
 *  calls do_enemy_move(index) to commit the move + resolve collisions.
 * ===================================================================== */

/* --- move_logic shared externs --- */
extern void RequestEnemyAction(Enemy* e, s32 action);
extern f32 get_yaw(f32* to, f32* from);       /* dir angle from->to */
extern void format_brain(s32 index);           /* AI-change transition hook */
extern void set_enemy_trans(Enemy* e, f32 spd, f32 ang); /* accel along angle */
extern f32 lbl_8011BF60[];    /* 0x8011BF60 imp retreat-speed ramp table */
extern const f64 lbl_803469B8; /* 1.25 action-speed threshold */
extern s32 lbl_80344748;      /* 0x80344748 current "IT" enemy slot */
extern s32 RandInt(s32 n);
extern level_data* gCurLevel;         /* 0x8034483C active level record */
extern f32 sin(f32 x);
extern f32 cos(f32 x);
extern void fn_8009DDCC(f32* pos);   /* skeleton assemble fx */
extern void fn_8009DD9C(f32* pos);   /* skeleton attack fx */
extern void fn_8009DD6C(f32* pos);   /* dog pounce-ready fx */
s32 damage_enemy(Enemy* e, f32 amount, s32 dtype, s32 a, s32 b, s32 c, s32 d);
extern void fn_8009E03C(Enemy* e);   /* skeleton bone-toss fx */
extern s32 fn_8004C8CC(f32* pos, s32 index);   /* wall/object clearance probe */
extern s32 FastWallCollide(f32* from, f32* to, void* hit, s32 mode); /* ray wall probe */
extern s32 fn_8004CE38(Enemy* e);   /* 0x8004CE38 left/right look-ahead probe dir */
extern void fn_80051568(s32 index); /* 0x80051568 guard-target refresh */
extern u8* sItems;                  /* item array base (stride 0xF0) */
extern f32 lbl_8011C0C4[];    /* 0x8011C0C4 wander search-angle offsets */
extern f32 lbl_8011C0A4[];    /* 0x8011C0A4 corner search-angle offsets */
extern f32 lbl_8011C084[];    /* 0x8011C084 guard corner search-angle offsets */
extern f32 lbl_8011C044[];    /* 0x8011C044 flee corner search-angle offsets */
extern f64 __frsqrte(f64 x);
extern s32 ErrorPrintf(const char* fmt, ...);
extern s32 sFlags;            /* 0x803445CC packed config flags */
extern u64 gControllerButtons;      /* 0x803445C8 config-word pair (hi) + sFlags (lo) */
extern u8 lbl_80112370[];           /* enemy debug string table */
extern u8 sLookoutParams[];     /* 0x802584A8 prowl-node table (stride 0x6C) */
extern s32 sNumLookoutParams;      /* 0x80344900 prowl-node count */
extern u8 sMilestones[];     /* 0x8025B604 milestone-node table (stride 0x68) */
extern s32 sNumMilestones;      /* 0x8034491C milestone-node count */
extern void GetMilestonePos(s32 idx, f32* out);  /* 0x80066054 */
extern s32 fn_800511D0(s32 idx, f32 turn);        /* 0x800511D0 next-node picker */

/* float magnitude via sign-bit clear (matches the enemy AI's inline fabs). */
static f32 fabsf_(f32 x)
{
    *(u32*)&x &= 0x7FFFFFFF;
    return x;
}

/* Choose the turn direction on the axis with the larger separation. */
s32 fn_8004CFAC(f32* pos, f32* target)
{
    u8 framePad[8];
    f32 x = pos[0];
    f32 targetX = target[0];
    f32 dx = x - targetX;
    f32 z;
    f32 targetZ;
    f32 dz;
    u8 unused[12];

    *(u32*)&dx &= 0x7FFFFFFF;
    z = pos[2];
    targetZ = target[2];
    dz = z - targetZ;
    *(u32*)&dz &= 0x7FFFFFFF;

    if (dx >= dz) {
        if (z < targetZ) {
            return 1;
        }
        return -1;
    }
    if (x < targetX) {
        return -1;
    }
    return 1;
}

/* Arm an enemy's dead-end timer and clear blocked desired actions. */
void fn_8004D030(s32 index, s32 ticks)
{
    Enemy* enemy = &gEnemies[index];

    if (enemy->dead_end > 0) {
        return;
    }
    enemy->dead_end = ticks;
    if (ticks < 60) {
        return;
    }
    if (enemy->daction == 3 || enemy->daction == 4) {
        enemy->daction = 0;
    }
}

/* Fade an enemy and its shadow, hiding both when fully opaque. */
void fn_8004DB3C(Enemy* enemy, s32 delta)
{
    s32 alpha = MBTreeGetAlpha(enemy->objgrp.node);
    s32 value;

    if (delta > 0 && alpha >= 255) {
        return;
    }
    if (delta < 0 && alpha == 0) {
        return;
    }
    value = alpha + delta * gFrameTicks;
    if (value < 0) {
        delta = 0;
    } else if (value > 255) {
        delta = 255;
    } else {
        delta = value;
    }
    if (delta == 255) {
        MBTreeSetFlags(enemy->objgrp.node, 2, 0);
        MBTreeSetFlags(enemy->shadow, 2, 0);
    } else {
        MBTreeSetAlpha(enemy->objgrp.node, delta, 1);
        MBTreeSetAlpha(enemy->shadow, delta, 1);
        MBTreeClearFlags(enemy->objgrp.node, 2, 0);
        MBTreeClearFlags(enemy->shadow, 2, 0);
    }
}

/* Select and launch an enemy missile, then dispatch its positional sound. */
void fn_8004E448(Enemy* enemy, s32 arg, f32* pos)
{
    s32 kind;

    if (enemy->algorithm == 28 || enemy->algorithm == 29) {
        kind = 2;
    } else if (enemy->algorithm == 16 || enemy->algorithm == 23) {
        kind = 0;
    } else if (enemy->algorithm == 17 || enemy->algorithm == 26) {
        kind = 1;
    } else {
        kind = 2;
    }

    if (EnemyStartMissile(enemy, arg, pos, kind) != 0) {
        if (kind == 0) {
            fn_8009DE2C(pos);
        }
        if (kind == 2) {
            if (enemy->type == 2) {
                fn_8009DCE4(pos);
            } else {
                fn_8009DDFC(pos);
            }
        }
    }
}

/* Advance an enemy after it reaches its assigned milestone. */
void update_enemy_milestone(Enemy* enemy)
{
    u8 frame_pad[12];
    f32 milestone[3];
    f32 vertical;
    u8 local_pad[12];
    f32 dx;
    f32 dz;

    if (enemy->plr_ms >= 0) {
        GetMilestonePos(enemy->plr_ms, milestone);
        dx = *(f32*)((u8*)enemy + 0x34) - milestone[0];
        dz = *(f32*)((u8*)enemy + 0x3C) - milestone[2];
        vertical = *(f32*)((u8*)enemy + 0x38) - milestone[1];
        *(u32*)&vertical &= 0x7FFFFFFF;
        if ((f64)vertical < lbl_803469F8 &&
            (f64)fqdist(dx, dz) < lbl_80346A00) {
            fn_8004E5F8(enemy);
            enemy->plr_ms = -1;
            enemy->operation_count = enemy->operation_speed;
            if (enemy->ms_idx > 0) {
                if (enemy->ms_idx > 0) {
                    enemy->ms_idx--;
                }
                enemy->max_msidx = enemy->ms_idx;
            }
            if (enemy->algorithm == 10) {
                enemy->collided = 0;
                enemy->route = 0;
                enemy->stuck_count = 0;
            }
        }
    }
}

/* Track this enemy's target milestone in the player's recent-history ring. */
void fn_8004E5F8(Enemy* enemy)
{
    s32* player = (s32*)((u8*)gPlayerWords + enemy->closest * 0x335C);
    s32 i;

    for (i = 0; i < 5; i++) {
        if (enemy->plr_ms == player[0xA34 / 4 + i]) {
            break;
        }
    }
    if (i < 5) {
        enemy->ms_idx = i;
        if (enemy->max_msidx < enemy->ms_idx) {
            enemy->max_msidx = enemy->ms_idx;
        }
    } else {
        enemy->ms_idx = 0;
        enemy->max_msidx = 4;
        enemy->plr_ms = -1;
    }
}

/* Advance texture modifiers for each loaded enemy type while gameplay runs.
 *
 * `resources` walks the lbl_80250E00 combined bss pool (see the TU .bss
 * layout note above this file's declarations).  Identification attempt
 * (2026-08-30, secondary de-fakematch pass):
 *   +0x564  EXACT match for gWadAtreeHeaders (0x80251364 - 0x80250E00 =
 *           0x564; verified by address arithmetic against the declared
 *           bss layout) - this reads gWadAtreeHeaders[index] as a void*,
 *           the per-type WAD/atree texmod-owner pointer DoTexMods() wants.
 *           NOT rewritten to the `gWadAtreeHeaders` symbol: fnasm.py shows
 *           this function's own base address relocates against
 *           lbl_80250E00 directly, so switching to the sub-object's own
 *           extern name would change the relocation target even though the
 *           resolved address is identical (claim.law.walked-base-symbol-
 *           identity.20260830.v1) - kept as the raw blob-base + literal
 *           offset for that reason, now with the identity documented.
 *   +0x20   first-level index table, inside lbl_80250E00's own declared
 *           0x40-byte anchor.  No covering struct found: gdlmem.py struct
 *           probes for gen_head/generator/enemy_gen/gen_record/gentable/
 *           gen_table/texmod_owner/enemy_texmod all returned no PDB match,
 *           and no other function in the TU references this slot range.
 *           Left as a raw offset - no name to adopt without inventing one.
 */
void fn_8004E67C(void)
{
    u8* resources;
    u8* cursor;
    s32 i;

    resources = (u8*)lbl_80250E00;
    if ((gGameBusy | gGameplayPauseTimer) == 0) {
        for (i = 0; i < lbl_8034471C; i++) {
            cursor = resources + i * sizeof(s32);
            cursor = resources + *(s32*)(cursor + 0x20) * sizeof(void*);
            if (*(void**)(cursor + 0x564) != 0) {
                DoTexMods(*(void**)(cursor + 0x564));
            }
        }
    }
}

/* Test a proposed enemy location and hide the slot again when it is blocked. */
s32 check_vacancy(s32 index, f32* pos)
{
    Enemy* enemy = &gEnemies[index];
    f32 adjusted[3];

    adjusted[0] = pos[0];
    adjusted[1] = pos[1];
    adjusted[2] = pos[2];
    adjusted[1] += enemy->coll_offset[1];

    if (check_enemy_pos(adjusted, 0, index) <= 0) {
        MBTreeSetFlags(enemy->objgrp.node, 2, 0);
        if (enemy->shadow != 0) {
            MBTreeSetFlags(enemy->shadow, 2, 0);
        }
        enemy->state = INACTIVE;
        return 0;
    }
    return -1;
}

/* do_ai @0x80046854 - central AI dispatcher.  Zeroes the per-frame translation,
 * then jumps through jumptable_8011C0EC[algorithm] to the matching move_logicNN
 * handler (0xB idle-wobble and 0x1B spawn are handled inline).  Returns -1 if the
 * enemy dropped to INACTIVE this tick, else 0. */
s32 do_ai(s32 index)
{
    Enemy* e = &gEnemies[index];
    f32 mat[16];
    u8 unused[8];

    e->trans[0] = 0.0f;
    e->trans[1] = 0.0f;
    e->trans[2] = 0.0f;
    switch (e->algorithm) {
    case 0:
        move_logic00(index);
        break;
    case 1:
        move_logic01(index);
        break;
    case 2:
        move_logic02(index);
        break;
    case 3:
        move_logic03(index);
        break;
    case 4:
        move_logic04(index);
        break;
    case 5:
        move_logic05(index);
        break;
    case 6:
        move_logic06(index);
        break;
    case 7:
        move_logic07(index);
        break;
    case 8:
        move_logic08(index);
        break;
    case 9:
        move_logic00(index);
        break;
    case 10:
        move_logic10(index);
        break;
    case 11:
        e->daction = 0;
        e->ang = 0.005555555555555556 * (3.141592654 * (f32)(u32)gFrameTicks) + e->ang;
        {
            f64 a;

            if ((a = e->ang) > 3.141592654) {
                a -= 6.283185308;
            } else if (a <= -3.141592654) {
                a = 6.283185308 + a;
            }
            e->ang = a;
        }
        e->pyr[1] = turn_enemy_ang(e, e->ang);
        CreateYPRMatrix(mat, &e->pyr[0]);
        CopyMat3(mat, &e->objgrp.worldmat[0][0]);
        if (e->generator == 0 || (e->state == 7 && gGameOptions[2] > 1)) {
            kill_enemy(index);
        }
        break;
    case 12:
        move_logic12(index);
        break;
    case 13:
        move_logic13(index);
        break;
    case 14:
        move_logic14(index);
        break;
    case 15:
        move_logic15(index);
        break;
    case 16:
        move_logic16(index);
        break;
    case 17:
        move_logic23(index);
        break;
    case 18:
        move_logic18(index);
        break;
    case 19:
        move_logic19(index);
        break;
    case 20:
        move_logic20(index);
        break;
    case 21:
        move_logic21(index);
        break;
    case 22:
        move_logic22(index);
        break;
    case 23:
        move_logic23(index);
        break;
    case 24:
        move_logic24(index);
        break;
    case 26:
        move_logic16(index);
        break;
    case 27:
        if (e->closest < 0) {
            e->trans[0] = 0.0f;
            e->trans[1] = 0.0f;
            e->trans[2] = 0.0f;
            do_enemy_move(index);
        } else {
            e->sight = 999999.0f;
            e->algorithm = 0;
            move_logic00(index);
        }
        break;
    case 28:
        move_logic28(index);
        break;
    case 29:
        move_logic29(index);
        break;
    case 30:
        move_logic30(index);
        break;
    case 31:
        move_logic31(index);
        break;
    default:
        kill_enemy(index);
        break;
    }
    if (e->state == 0) {
        return -1;
    }
    return 0;
}

/* move_logic00 @0x80046B54 (state 0 + 9, base wander/seek).  IT-flee / chase
 * gates, then face the closest player and sweep up to 9 offset headings,
 * projecting each with sin/cos and probing for wall clearance; commit the first
 * clear heading (recording the try count), else keep the straight bearing. */
void move_logic00(s32 index)
{
    u8* basep = (u8*)lbl_80250E00;
    u8* e0 = basep + index * 916;
    Enemy* e;
    s32 type;
    f32 speed;
    s32 it = lbl_80344748;
    s32 flee;
    f32 base;
    u8* t;
    f32 probe[3];
    u8 unused[24];

    type = *(s32*)(e0 += 3608);
    e = (Enemy*)(u8*)e0;
    t = basep;
    t += type * 4;
    speed = *(f32*)(t + 64);
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = basep + it * 916;
        if (*(s32*)(other + OFF_E(state)) != ACTIVE) {
            flee = 0;
        } else if (*(f32*)(other + OFF_E(actual_dist)) > *(f32*)(e0 + offsetof(Enemy, sight))) {
            flee = 0;
        } else if (index == it || *(s16*)(e0 + offsetof(Enemy, birth_style)) != 0 || *(s32*)(e0 + offsetof(Enemy, dead_end)) > 0) {
            goto flee_zero00;
        } else {
            f32 dx = *(f32*)(other + OFF_E(objgrp.worldmat[3][0])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0]));
            f32 dy = *(f32*)(other + OFF_E(objgrp.worldmat[3][1])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][1]));
            f32 dz = *(f32*)(other + OFF_E(objgrp.worldmat[3][2])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][2]));
            if (dx * dx + dy * dy + dz * dz < lbl_803468D8) {
                flee = -1;
            } else {
            flee_zero00:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    if (e->dead_end <= 0) {
        {
            s16 c = e->closest;
            f32 f;
            if (c >= 0) {
                if (*(s16*)&gPlayerWords[c][647] > 2) {
                    f = get_yaw(&gPlayerWords[c][633], &e->objgrp.worldmat[3][0]);
                } else {
                    f = get_yaw(&gPlayerWords[c][17], &e->objgrp.worldmat[3][0]);
                }
            } else {
                f = e->ang;
            }
            base = f;
        }
        lbl_80344720 = base;
        {
            f32 cand = base;
            s32 found = 0;
            s32 off = 0;
            do {
                f32 d;
                if (e->route > 0) {
                    cand = cand + *(f32*)((u8*)lbl_8011C0C4 + off);
                } else {
                    cand = cand - *(f32*)((u8*)lbl_8011C0C4 + off);
                }
                {
                    f64 nv;
                    if (cand > 3.141592654) {
                        nv = cand - 6.283185308;
                    } else if (cand <= -3.141592654) {
                        nv = 6.283185308 + cand;
                    } else {
                        nv = cand;
                    }
                    cand = nv;
                }
                probe[0] = e->objgrp.worldmat[3][0];
                probe[1] = e->objgrp.worldmat[3][1];
                probe[2] = e->objgrp.worldmat[3][2];
                probe[1] += 0.1 + e->rad;
                probe[0] += speed * sin(cand);
                probe[2] += speed * cos(cand);
                d = cand - e->ang;
                {
                    f64 nd;
                    if (d > 3.141592654) {
                        nd = d - 6.283185308;
                    } else if (d <= -3.141592654) {
                        nd = 6.283185308 + d;
                    } else {
                        nd = d;
                    }
                    d = nd;
                }
                if ((!(fabsf_(e->ang - e->angbak) > 0.034906585044444445)
                     || !(fabsf_(cand - e->angbak) <= 0.034906585044444445))
                    && !(fabsf_(d) >= 3.106686068955556)
                    && fn_8004C8CC(probe, index) != 0) {
                    break;
                }
                found++;
                off += 4;
            } while (found < 9);
            if (found >= 9) {
                cand = lbl_80344720;
            } else {
                e->collided = found;
            }
            e->angbak = e->ang;
            e->ang = cand;
        }
    }
    set_enemy_trans(e, 1.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* move_logic01 @0x80046F24 (state 1, patrol-toward-milestone).  IT-flee / chase
 * gates, then face a milestone only after an operation-speed cadence, run down a
 * short area-2 timer, and re-lock the heading when it drifts past pi/30. */
void move_logic01(s32 index)
{
    s32 stuck;
    u8* base = (u8*)lbl_80250E00;
    u8* row01;
    u8* e0;
    Enemy* e;
    s32 it;
    s32 dead0;
    s32 flee;
    f32 a;
    u8 unused[24];

    row01 = base + index * 916;
    dead0 = *(s32*)(row01 + 4464);
    e0 = row01 + 3608;
    e = (Enemy*)(u8*)e0;
    if (dead0 > 0) {
        stuck = 1;
    } else {
        stuck = 0;
    }
    it = lbl_80344748;
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = base + it * 916;
        if (*(s32*)(other + OFF_E(state)) != ACTIVE) {
            flee = 0;
        } else if (*(f32*)(other + OFF_E(actual_dist)) > *(f32*)(e0 + offsetof(Enemy, sight))) {
            flee = 0;
        } else if (index == it || *(s16*)(e0 + offsetof(Enemy, birth_style)) != 0 || dead0 > 0) {
            goto flee_zero01;
        } else {
            f32 dx = *(f32*)(other + OFF_E(objgrp.worldmat[3][0])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0]));
            f32 dy = *(f32*)(other + OFF_E(objgrp.worldmat[3][1])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][1]));
            f32 dz = *(f32*)(other + OFF_E(objgrp.worldmat[3][2])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][2]));
            if (dx * dx + dy * dy + dz * dz < lbl_803468D8) {
                flee = -1;
            } else {
            flee_zero01:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->closest < 0 || e->operation_count < e->operation_speed) {
        a = e->ang;
    } else {
        if (*(s16*)&gPlayerWords[e->closest][647] > 2) {
            e->ang = get_yaw(&gPlayerWords[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            e->ang = get_yaw(&gPlayerWords[e->closest][17], &e->objgrp.worldmat[3][0]);
        }
        a = e->ang;
    }
    e->ang = a;
    if (e->area == 2 && e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    {
        f32 d;
        u8 _blk01[8];
        if (e->dead_end <= 0
            || ((d = e->ang - e->anghit), (*(u32*)&d &= 0x7FFFFFFF),
                d >= 0.10471975513333334)) {
            e->dead_end = 0;
            set_enemy_trans(e, 1.0f, e->ang);
        }
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
    if (stuck == 0 && e->dead_end > 0) {
        e->anghit = e->ang;
    }
}

/* move_logic02 @0x800471A4 (state 2, demon-flee-from-IT).  If the "IT" enemy is
 * active, close, at the same height and this enemy is fresh, flee it (algorithm
 * 24); once it has closed on a player switch to chase; otherwise drift on a
 * timer, wobbling the heading by pi/4 and periodically re-rolling to ON_EXIT. */
void move_logic02(s32 index)
{
    u8* e0;
    u8* base = (u8*)lbl_80250E00;
    Enemy* e;
    s32 it;
    s32 flee;
    u8 unused[16];

    e0 = base + index * 916 + 3608;
    it = lbl_80344748;
    e = (Enemy*)e0;
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = base + it * 916;
        if (*(s32*)(other + OFF_E(state)) != ACTIVE) {
            flee = 0;
        } else if (*(f32*)(other + OFF_E(actual_dist)) > *(f32*)(e0 + offsetof(Enemy, sight))) {
            flee = 0;
        } else if (index == it || *(s16*)(e0 + offsetof(Enemy, birth_style)) != 0 || *(s32*)(e0 + offsetof(Enemy, dead_end)) > 0) {
            goto flee_zero;
        } else {
            f32 dx = *(f32*)(other + OFF_E(objgrp.worldmat[3][0])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0]));
            f32 dy = *(f32*)(other + OFF_E(objgrp.worldmat[3][1])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][1]));
            f32 dz = *(f32*)(other + OFF_E(objgrp.worldmat[3][2])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][2]));
            if (dx * dx + dy * dy + dz * dz < lbl_803468D8) {
                flee = -1;
            } else {
            flee_zero:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->closest >= 0 && e->close_dist <= lbl_80346900) {
        e->algorithm = 0;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->dead_end > 0) {
        if ((e->dead_end -= gFrameTicks) <= 0) {
            e->ang += lbl_80346908;
            {
                f64 a = e->ang;
                if (a > lbl_80346840) {
                    a -= lbl_80346848;
                } else if (a <= lbl_80346850) {
                    a = lbl_80346848 + a;
                }
                e->ang = a;
            }
            if (++e->count >= 4) {
                e->count = 0;
                e->algorithm = 4;
                e->old_ai = 4;
                e->play = 4;
            }
        }
    }
    if ((it = e->coll_pnum) >= 0) {
        e->ang = get_yaw(&gPlayerWords[it][17], &e->objgrp.worldmat[3][0]);
    }
    set_enemy_trans(e, lbl_803468F0, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* move_logic03 @0x800473E8 (state 3, rat-scatter flee).  Bounce to the chase
 * or wander algorithms unless it is actively fleeing a milestone target; ramp
 * a retreat speed, face away from that target (facing + pi), normalize,
 * accelerate, damp the resulting velocity to 0.9, turn + move, refresh. */
void move_logic03(s32 index)
{
    Enemy* e = &gEnemies[index];
    f32 spd;
    s32 stuck;
    s32 c;
    f32 face;

    if (e->recognized == 0 || e->closest < 0) {
        if (e->counter2 < 0) {
            e->algorithm = (index & 1) + 5;
            do_ai(index);
            return;
        }
        spd = 0.0f;
        if (e->dead_end > 0) {
            stuck = 1;
        } else {
            stuck = 0;
        }
        if (e->dead_end > 0 && (c = e->counter1) < 8) {
            e->counter1 = c + 1;
            spd = lbl_8011BF60[c];
            e->dead_end = 0;
        }
        if (e->counter2 >= 0) {
            if (*(s16*)&gPlayerWords[e->counter2][647] > 2) {
                face = get_yaw(&gPlayerWords[e->counter2][633], &e->objgrp.worldmat[3][0]);
            } else {
                face = get_yaw(&gPlayerWords[e->counter2][17], &e->objgrp.worldmat[3][0]);
            }
        } else {
            face = e->ang;
        }
        e->ang = spd + (3.141592654 + face);
        {
            f64 a;

            if ((a = e->ang) > 3.141592654) {
                a -= 6.283185308;
            } else if (a <= -3.141592654) {
                a = 6.283185308 + a;
            }
            e->ang = a;
        }
        set_enemy_trans(e, 1.0f, e->ang);
        e->pyr[1] = turn_enemy_ang(e, e->ang);
        e->trans[0] *= 0.9;
        e->trans[1] *= 0.9;
        e->trans[2] *= 0.9;
        do_enemy_move(index);
        if (e->moved != 0) {
            e->dead_end = 0;
        }
        if (stuck == 0 && e->dead_end > 0) {
            e->anghit = e->ang;
            e->counter1 = 0;
        }
    } else {
        e->algorithm = 0;
        do_ai(index);
    }
}

/* move_logic04 @0x800475FC (state 4, sibling of move_logic02).  Same IT-flee /
 * chase gating, but the drift-timer wobbles the heading the other way (-pi/4)
 * and, on expiry, rolls to the ON_EXIT taunt (algorithm/old_ai = 2, play = 4). */
void move_logic04(s32 index)
{
    u8* e0;
    u8* base = (u8*)lbl_80250E00;
    Enemy* e;
    s32 it;
    s32 flee;
    u8 unused[16];

    e0 = base + index * 916 + 3608;
    it = lbl_80344748;
    e = (Enemy*)e0;
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = base + it * 916;
        if (*(s32*)(other + OFF_E(state)) != ACTIVE) {
            flee = 0;
        } else if (*(f32*)(other + OFF_E(actual_dist)) > *(f32*)(e0 + offsetof(Enemy, sight))) {
            flee = 0;
        } else if (index == it || *(s16*)(e0 + offsetof(Enemy, birth_style)) != 0 || *(s32*)(e0 + offsetof(Enemy, dead_end)) > 0) {
            goto flee_zero;
        } else {
            f32 dx = *(f32*)(other + OFF_E(objgrp.worldmat[3][0])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0]));
            f32 dy = *(f32*)(other + OFF_E(objgrp.worldmat[3][1])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][1]));
            f32 dz = *(f32*)(other + OFF_E(objgrp.worldmat[3][2])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][2]));
            if (dx * dx + dy * dy + dz * dz < lbl_803468D8) {
                flee = -1;
            } else {
            flee_zero:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->closest >= 0 && e->close_dist <= lbl_80346900) {
        e->algorithm = 0;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->dead_end > 0) {
        if ((e->dead_end -= gFrameTicks) <= 0) {
            e->ang -= lbl_80346908;
            {
                f64 a = e->ang;
                if (a > lbl_80346840) {
                    a -= lbl_80346848;
                } else if (a <= lbl_80346850) {
                    a = lbl_80346848 + a;
                }
                e->ang = a;
            }
            if (++e->count >= 4) {
                e->count = 0;
                e->algorithm = 2;
                e->old_ai = 2;
                e->play = 4;
            }
        }
    }
    if (e->coll_pnum >= 0) {
        e->ang = get_yaw(&gPlayerWords[e->coll_pnum][17], &e->objgrp.worldmat[3][0]);
    }
    set_enemy_trans(e, 1.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* move_logic05 @0x80047844 (state 5, one of the two "wander" fallbacks reached
 * from the recognized/closest gate).  IT-flee, drift a heading on a dead_end
 * timer (rotating -pi/2 and cycling a 4-count), then sweep two clearance probes:
 * a near ray (rad+0.5) via FastWallCollide and a far ray (speed) via the shared
 * object probe; a block on either rotates the heading and re-arms the timer. */
typedef struct EnemyMovePage05 {
    u8 _0000[64];
    f32 speed[45];
    u8 _00f4[3364];
    Enemy enemies[25];
} EnemyMovePage05;
#pragma opt_propagation off
void move_logic05(s32 index)
{
    EnemyMovePage05* page = (EnemyMovePage05*)lbl_80250E00;
    Enemy* e;
    u8* e0;
    s32 it = lbl_80344748;
    s32 type;
    f32 dist;
    f32 speed;
    s32 flee;
    f32 probe2[3];
    f32 probe[3];
    f32 probeEnd[3];
    u8 _pad05[56];

    e0 = (u8*)page + index * 916;
    type = *(s32*)(e0 += 3608);
    e = (Enemy*)e0;
    dist = e->rad;
    speed = page->speed[type];
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = (u8*)page + it * 916;
        if (*(s32*)(other + OFF_E(state)) != ACTIVE) {
            flee = 0;
        } else if (*(f32*)(other + OFF_E(actual_dist)) > e->sight) {
            flee = 0;
        } else if (index == it || e->birth_style != 0 || e->dead_end > 0) {
            goto flee_zero05;
        } else {
            f32 dx = *(f32*)(other + OFF_E(objgrp.worldmat[3][0])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0]));
            f32 dy = *(f32*)(other + OFF_E(objgrp.worldmat[3][1])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][1]));
            f32 dz = *(f32*)(other + OFF_E(objgrp.worldmat[3][2])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][2]));
            if (dx * dx + dy * dy + dz * dz < lbl_803468D8) {
                flee = -1;
            } else {
            flee_zero05:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->dead_end > 0) {
        if ((e->dead_end -= gFrameTicks) <= 0) {
            e->ang = e->ang - lbl_80346918;
            {
                f64 a = e->ang;
                if (a > lbl_80346840) {
                    a -= lbl_80346848;
                } else if (a <= lbl_80346850) {
                    a = lbl_80346848 + a;
                }
                e->ang = a;
            }
            if (++e->count >= 4) {
                e->count = 0;
            }
        }
    }
    probe[0] = *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0]));
    probe[1] = *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][1]));
    probe[2] = *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][2]));
    probe[1] += lbl_80346858 + e->rad;
    probeEnd[0] = probe[0];
    probeEnd[1] = probe[1];
    probeEnd[2] = probe[2];
    dist += lbl_80346830;
    probeEnd[0] += dist * sin(e->ang);
    probeEnd[2] += dist * cos(e->ang);
    if (FastWallCollide(probe, probeEnd, 0, 2) != 0) {
        e->ang = e->ang - lbl_80346918;
        {
            f64 a = e->ang;
            if (a > lbl_80346840) {
                a -= lbl_80346848;
            } else if (a <= lbl_80346850) {
                a = lbl_80346848 + a;
            }
            e->ang = a;
        }
        if (e->dead_end <= 0) {
            e->dead_end = 20;
        }
    } else {
        probe2[0] = probe[0];
        probe2[1] = probe[1];
        probe2[2] = probe[2];
        probe2[0] += speed * sin(e->ang);
        probe2[2] += speed * cos(e->ang);
        if (fn_8004C8CC(probe2, index) == 0) {
            e->ang = e->ang - lbl_80346918;
            {
                f64 a = e->ang;
                if (a > lbl_80346840) {
                    a -= lbl_80346848;
                } else if (a <= lbl_80346850) {
                    a = lbl_80346848 + a;
                }
                e->ang = a;
            }
            if (e->dead_end <= 0) {
                e->dead_end = 20;
            }
        }
    }
    set_enemy_trans(e, lbl_803468F0, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}
#pragma opt_propagation reset

/* move_logic06 @0x80047BF0 (state 6, mirror of move_logic05).  Same IT-flee and
 * two-probe clearance search, but every heading correction rotates +pi/2 instead
 * of -pi/2, so it sweeps the opposite way around an obstacle. */
#pragma opt_propagation off
void move_logic06(s32 index)
{
    EnemyMovePage05* page = (EnemyMovePage05*)lbl_80250E00;
    Enemy* e;
    u8* e0;
    s32 it = lbl_80344748;
    s32 type;
    f32 dist;
    f32 speed;
    s32 flee;
    f32 probe2[3];
    f32 probe[3];
    f32 probeEnd[3];
    u8 _pad06[56];

    e0 = (u8*)page + index * 916;
    type = *(s32*)(e0 += 3608);
    e = (Enemy*)e0;
    dist = e->rad;
    speed = page->speed[type];
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = (u8*)page + it * 916;
        if (*(s32*)(other + OFF_E(state)) != ACTIVE) {
            flee = 0;
        } else if (*(f32*)(other + OFF_E(actual_dist)) > e->sight) {
            flee = 0;
        } else if (index == it || e->birth_style != 0 || e->dead_end > 0) {
            goto flee_zero06;
        } else {
            f32 dx = *(f32*)(other + OFF_E(objgrp.worldmat[3][0])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0]));
            f32 dy = *(f32*)(other + OFF_E(objgrp.worldmat[3][1])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][1]));
            f32 dz = *(f32*)(other + OFF_E(objgrp.worldmat[3][2])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][2]));
            if (dx * dx + dy * dy + dz * dz < lbl_803468D8) {
                flee = -1;
            } else {
            flee_zero06:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->dead_end > 0) {
        if ((e->dead_end -= gFrameTicks) <= 0) {
            e->ang = e->ang + lbl_80346918;
            {
                f64 a = e->ang;
                if (a > lbl_80346840) {
                    a -= lbl_80346848;
                } else if (a <= lbl_80346850) {
                    a = lbl_80346848 + a;
                }
                e->ang = a;
            }
            if (++e->count >= 4) {
                e->count = 0;
            }
        }
    }
    probe[0] = *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0]));
    probe[1] = *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][1]));
    probe[2] = *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][2]));
    probe[1] += lbl_80346858 + e->rad;
    probeEnd[0] = probe[0];
    probeEnd[1] = probe[1];
    probeEnd[2] = probe[2];
    dist += lbl_80346830;
    probeEnd[0] += dist * sin(e->ang);
    probeEnd[2] += dist * cos(e->ang);
    if (FastWallCollide(probe, probeEnd, 0, 2) != 0) {
        e->ang = e->ang + lbl_80346918;
        {
            f64 a = e->ang;
            if (a > lbl_80346840) {
                a -= lbl_80346848;
            } else if (a <= lbl_80346850) {
                a = lbl_80346848 + a;
            }
            e->ang = a;
        }
        if (e->dead_end <= 0) {
            e->dead_end = 20;
        }
    } else {
        probe2[0] = probe[0];
        probe2[1] = probe[1];
        probe2[2] = probe[2];
        probe2[0] += speed * sin(e->ang);
        probe2[2] += speed * cos(e->ang);
        if (fn_8004C8CC(probe2, index) == 0) {
            e->ang = e->ang + lbl_80346918;
            {
                f64 a = e->ang;
                if (a > lbl_80346840) {
                    a -= lbl_80346848;
                } else if (a <= lbl_80346850) {
                    a = lbl_80346848 + a;
                }
                e->ang = a;
            }
            if (e->dead_end <= 0) {
                e->dead_end = 20;
            }
        }
    }
    set_enemy_trans(e, lbl_803468F0, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}
#pragma opt_propagation reset
/* move_logic07 @0x80047F9C (state 7, rat-style corner-hugging chase).  IT-flee /
 * recognized gates, face a milestone/player, then when the dead_end timer allows
 * pick a corner-avoidance heading (fn_8004CE38 route + lbl_8011C0A4 offset table),
 * normalize it, probe clearance, and count consecutive stuck frames; bail back to
 * the straight bearing after 10.  Rats (type 3) poke a walk action at the end. */
#pragma opt_propagation off
void move_logic07(s32 index)
{
    u8* tbl = (u8*)lbl_8011AF48;
    EnemyMovePage05* page = (EnemyMovePage05*)lbl_80250E00;
    Enemy* e;
    u8* e0;
    s32 it = lbl_80344748;
    s32 type;
    f32 speed;
    s32 flee;
    s32 found = 0;
    f32 cand;
    f32* q;
    f32 probe[3];
    u8 unusedA[20];
    f32 d1;
    f32 d2;
    u8 unusedB[16];

    e0 = (u8*)page + index * 916;
    type = *(s32*)(e0 + 3608);
    e0 += 3608;
    e = (Enemy*)(u8*)e0;
    speed = page->speed[type];
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = (u8*)page + it * 916;
        if (*(s32*)(other + OFF_E(state)) != ACTIVE) {
            flee = 0;
        } else if (*(f32*)(other + OFF_E(actual_dist)) > *(f32*)(e0 + offsetof(Enemy, sight))) {
            flee = 0;
        } else if (index == it || *(s16*)(e0 + offsetof(Enemy, birth_style)) != 0 || *(s32*)(e0 + offsetof(Enemy, dead_end)) > 0) {
            goto flee_zero07;
        } else {
            f32 dx = *(f32*)(other + OFF_E(objgrp.worldmat[3][0])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0]));
            f32 dy = *(f32*)(other + OFF_E(objgrp.worldmat[3][1])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][1]));
            f32 dz = *(f32*)(other + OFF_E(objgrp.worldmat[3][2])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][2]));
            if (dx * dx + dy * dy + dz * dz < lbl_803468D8) {
                flee = -1;
            } else {
            flee_zero07:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    {
        s16 c = e->closest;
        f32 f;
        if (c >= 0) {
            if (*(s16*)&gPlayerWords[c][647] > 2) {
                f = get_yaw(&gPlayerWords[c][633], &e->objgrp.worldmat[3][0]);
            } else {
                f = get_yaw(&gPlayerWords[c][17], &e->objgrp.worldmat[3][0]);
            }
        } else {
            f = e->ang;
        }
        lbl_80344720 = f;
    }
    if (e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    if (e->dead_end <= 0) {
        if (e->coll_pnum >= 0) {
            cand = lbl_80344720;
        } else if (e->area == 1) {
            s32 col;
            cand = lbl_80344720;
            col = e->collided;
            if (e->route == 0) {
                e->route = fn_8004CE38(e);
            }
            if (e->route > 0) {
                q = (f32*)(tbl + col * 4);
                cand = cand + q[1111];
            } else {
                q = (f32*)(tbl + col * 4);
                cand = cand - q[1111];
            }
        } else if (e->coll_ip != 0 || e->coll_enenum >= 0) {
            s32 col2;
            cand = e->ang;
            col2 = e->collided;
            if (e->route > 0) {
                q = (f32*)(tbl + col2 * 4);
                cand = cand + q[1111];
            } else {
                q = (f32*)(tbl + col2 * 4);
                cand = cand - q[1111];
            }
        } else {
            cand = lbl_80344720;
        }
        {
            f64 a;
            if (cand > 3.141592654) {
                a = cand - 6.283185308;
            } else if (cand <= -3.141592654) {
                a = 6.283185308 + cand;
            } else {
                a = cand;
            }
            cand = a;
        }
        probe[0] = e->objgrp.worldmat[3][0];
        probe[1] = e->objgrp.worldmat[3][1];
        probe[2] = e->objgrp.worldmat[3][2];
        probe[1] += 0.1 + e->rad;
        probe[0] += speed * sin(cand);
        probe[2] += speed * cos(cand);
        d1 = e->ang - e->angbak;
        *(u32*)&d1 &= 0x7FFFFFFF;
        if ((d1 > 0.034906585044444445
             && ((d2 = cand - e->angbak), (*(u32*)&d2 &= 0x7FFFFFFF),
                 d2 <= 0.034906585044444445))
            || fn_8004C8CC(probe, index) == 0) {
            found = 1;
            e->stuck_count++;
        } else {
            e->stuck_count = 0;
        }
        if (e->stuck_count > 10) {
            cand = lbl_80344720;
            e->ang = cand;
        }
        if (found == 0) {
            e->angbak = e->ang;
            e->ang = cand;
        }
    } else {
        cand = e->ang;
    }
    set_enemy_trans(e, 1.0f, cand);
    if (found == 0 || cand == lbl_80344720) {
        e->pyr[1] = turn_enemy_ang(e, cand);
    }
    do_enemy_move(index);
    if (e->type == E_RAT && e->daction == 0) {
        RequestEnemyAction(e, 3);
    }
}
#pragma opt_propagation reset

/* move_logic08 @0x80048408 (state 8, guard/warlock corner-hug chase).  Sibling of
 * move_logic07 but with a guard target: fn_80051568 refreshes guard_closest, which
 * (when >=0) overrides the milestone bearing with the guarded item's heading.  When
 * cornered it also validates the collided item (a live spawner of type 2) before
 * choosing a corner-avoidance offset from lbl_8011C084. */
#pragma opt_propagation off
void move_logic08(s32 index)
{
    u8* tbl = (u8*)lbl_8011AF48;
    EnemyMovePage05* page = (EnemyMovePage05*)lbl_80250E00;
    Enemy* e;
    u8* e0;
    s32 it = lbl_80344748;
    s32 type;
    f32 speed;
    s32 flee;
    s32 found = 0;
    f32 cand;
    f32* q;
    f32 probe[3];
    u8 unusedA[20];
    f32 d1;
    f32 d2;
    u8 unusedB[16];

    e0 = (u8*)page + index * 916;
    type = *(s32*)(e0 + 3608);
    e0 += 3608;
    e = (Enemy*)(u8*)e0;
    speed = page->speed[type];
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = (u8*)page + it * 916;
        if (*(s32*)(other + OFF_E(state)) != ACTIVE) {
            flee = 0;
        } else if (*(f32*)(other + OFF_E(actual_dist)) > *(f32*)(e0 + offsetof(Enemy, sight))) {
            flee = 0;
        } else if (index == it || *(s16*)(e0 + offsetof(Enemy, birth_style)) != 0 || *(s32*)(e0 + offsetof(Enemy, dead_end)) > 0) {
            goto flee_zero08;
        } else {
            f32 dx = *(f32*)(other + OFF_E(objgrp.worldmat[3][0])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0]));
            f32 dy = *(f32*)(other + OFF_E(objgrp.worldmat[3][1])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][1]));
            f32 dz = *(f32*)(other + OFF_E(objgrp.worldmat[3][2])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][2]));
            if (dx * dx + dy * dy + dz * dz < lbl_803468D8) {
                flee = -1;
            } else {
            flee_zero08:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    fn_80051568(index);
    if (e->guard_closest >= 0) {
        lbl_80344720 = get_yaw((f32*)(sItems + e->guard_closest * 240 + 52),
                               &e->objgrp.worldmat[3][0]);
    } else {
        s16 c = e->closest;
        f32 f;
        if (c >= 0) {
            if (*(s16*)&gPlayerWords[c][647] > 2) {
                f = get_yaw(&gPlayerWords[c][633], &e->objgrp.worldmat[3][0]);
            } else {
                f = get_yaw(&gPlayerWords[c][17], &e->objgrp.worldmat[3][0]);
            }
        } else {
            f = e->ang;
        }
        lbl_80344720 = f;
    }
    if (e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    if (e->dead_end <= 0) {
        if (e->coll_pnum >= 0) {
            s16 c = e->closest;
            f32 f;
            if (c >= 0) {
                if (*(s16*)&gPlayerWords[c][647] > 2) {
                    f = get_yaw(&gPlayerWords[c][633], &e->objgrp.worldmat[3][0]);
                } else {
                    f = get_yaw(&gPlayerWords[c][17], &e->objgrp.worldmat[3][0]);
                }
            } else {
                f = e->ang;
            }
            cand = f;
            lbl_80344720 = f;
        } else {
            u8* ip = (u8*)e->coll_ip;
            s32 valid;
            if (ip == 0) {
                valid = 0;
            } else if (*(s16*)(ip + 196) == -1 || **(s32**)ip != 2
                       || *(s8*)(ip + 205) != 0) {
                valid = 0;
            } else {
                valid = -1;
            }
            if (valid != 0) {
                cand = lbl_80344720;
            } else if (e->area == 1) {
                s32 col;
                cand = lbl_80344720;
                col = e->collided;
                if (e->route == 0) {
                    e->route = fn_8004CE38(e);
                }
                if (e->route > 0) {
                    q = (f32*)(tbl + col * 4);
                    cand = cand + q[1103];
                } else {
                    q = (f32*)(tbl + col * 4);
                    cand = cand - q[1103];
                }
            } else if (ip != 0 || e->coll_enenum >= 0) {
                s32 col2;
                cand = e->ang;
                col2 = e->collided;
                if (e->route > 0) {
                    q = (f32*)(tbl + col2 * 4);
                    cand = cand + q[1103];
                } else {
                    q = (f32*)(tbl + col2 * 4);
                    cand = cand - q[1103];
                }
            } else {
                cand = lbl_80344720;
            }
        }
        {
            f64 a;
            if (cand > 3.141592654) {
                a = cand - 6.283185308;
            } else if (cand <= -3.141592654) {
                a = 6.283185308 + cand;
            } else {
                a = cand;
            }
            cand = a;
        }
        probe[0] = e->objgrp.worldmat[3][0];
        probe[1] = e->objgrp.worldmat[3][1];
        probe[2] = e->objgrp.worldmat[3][2];
        probe[1] += 0.1 + e->rad;
        probe[0] += speed * sin(cand);
        probe[2] += speed * cos(cand);
        d1 = e->ang - e->angbak;
        *(u32*)&d1 &= 0x7FFFFFFF;
        if ((d1 > 0.034906585044444445
             && ((d2 = cand - e->angbak), (*(u32*)&d2 &= 0x7FFFFFFF),
                 d2 <= 0.034906585044444445))
            || fn_8004C8CC(probe, index) == 0) {
            found = 1;
            e->stuck_count++;
        } else {
            e->stuck_count = 0;
        }
        if (e->stuck_count > 10) {
            cand = lbl_80344720;
            e->ang = cand;
        }
        if (found == 0) {
            e->angbak = e->ang;
            e->ang = cand;
        }
    } else {
        cand = e->ang;
    }
    set_enemy_trans(e, 1.0f, cand);
    if (found == 0 || cand == lbl_80344720) {
        e->pyr[1] = turn_enemy_ang(e, cand);
    }
    do_enemy_move(index);
}
#pragma opt_propagation reset

/* move_logic10 @0x80048928 (state 10, lizardman "captain") - SKELETON + NOTES.
 * This is the largest AI handler in the game (1086 GC insns, biggest on Xbox too).
 * Only the shared gate opening is reconstructed; the full body is a multi-mode
 * pack-hunter that has NOT been transcribed (deliberate under the light-touch pass).
 *
 * Full-body call inventory (from tools/gdl/fnasm.py game/enemy/enemy move_logic10):
 *   get_yaw x18  - face/milestone bearings for each sub-mode
 *   sin/cos     x7   - heading projection for the wander + corner probes
 *   GetMilestonePos x6, find_neighbor_milestone x2, update_enemy_milestone x1
 *                     - milestone-network navigation (shares the move_logic22 stack)
 *   turn_enemy_ang x5, set_enemy_trans x5, do_enemy_move x5 - one move per sub-mode
 *   format_brain x5   - AI-change transition per sub-mode
 *   fqdist x4, fn_8004CE38 x3, fn_8004C8CC x3 - dist checks + corner probes
 *   do_ai x2         - the flee/chase bail-outs
 * Frame: 392 bytes, saves r25-r31 (_savefpr_25), pool base lbl_8011AF48 held in a
 * nonvolatile.  Uses the lbl_80250E00 + index*916 + 3608 anchor like the others.
 * TODO: transcribe the sub-mode state machine (Ghidra decompile_function 0x80048928
 * then split the modes by e->mode1 / e->flag1 as in move_logic22 + move_logic07). */
extern f64 lbl_80346920;      /* pi/2 turn step */
extern f64 lbl_80346858;      /* 0.1 probe lift */
extern f64 lbl_803468E0;      /* stuck-angle threshold */
extern f32 lbl_8011C064[];    /* 0x8011C064 pack-hunter turn-step ramp */
extern s32 find_neighbor_milestone(s32 ms, s32 nth); /* 0x8004C9DC */

#pragma opt_propagation off
void move_logic10(s32 index)
{
    u8* base = (u8*)lbl_80250E00;
    u8* tbl = (u8*)lbl_8011AF48;
    u8* e0 = base + index * 916;
    Enemy* e;
    s32 type;
    f32 speed;
    s32 it = lbl_80344748;
    s32 flee;
    s32 found = 0;
    f32* q;
    u8* t;
    u8* other;

    type = *(s32*)(e0 + 3608);
    e = (Enemy*)(e0 + 3608);
    e0 += 3608;
    t = base;
    t += type * 4;
    speed = *(f32*)(t + 64);
    if (it < 0) {
        flee = 0;
    } else {
        other = base + it * 916;
        if (*(s32*)(other + OFF_E(state)) != ACTIVE) {
            flee = 0;
        } else if (*(f32*)(other + OFF_E(actual_dist)) > *(f32*)(e0 + offsetof(Enemy, sight))) {
            flee = 0;
        } else if (index == it || *(s16*)(e0 + offsetof(Enemy, birth_style)) != 0 || *(s32*)(e0 + offsetof(Enemy, dead_end)) > 0) {
            goto flee_zero10;
        } else {
            f32 dx = *(f32*)(other + OFF_E(objgrp.worldmat[3][0])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0]));
            f32 dy = *(f32*)(other + OFF_E(objgrp.worldmat[3][1])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][1]));
            f32 dz = *(f32*)(other + OFF_E(objgrp.worldmat[3][2])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][2]));
            if (dx * dx + dy * dy + dz * dz < lbl_803468D8) {
                flee = -1;
            } else {
            flee_zero10:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    update_enemy_milestone(e);
    switch (e->mode1) {
        f32 probe[3];
    case 0: {
        s32 skip;
        f32 cand;
        u8 _g1[24];
        if (*(s32*)(e0 + offsetof(Enemy, coll_pnum)) >= 0) {
            if (*(s16*)(e0 + offsetof(Enemy, algorithm)) != *(s16*)(e0 + offsetof(Enemy, prev_ai))) {
                format_brain(index);
            }
            {
                s16 c = *(s16*)(e0 + offsetof(Enemy, closest));
                f32 f;
                if (c >= 0) {
                    if (*(s16*)&gPlayerWords[c][647] > 2) {
                        f = get_yaw(&gPlayerWords[c][633], (f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0])));
                    } else {
                        f = get_yaw(&gPlayerWords[c][17], (f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0])));
                    }
                } else {
                    f = *(f32*)(e0 + offsetof(Enemy, ang));
                }
                *(f32*)(e0 + offsetof(Enemy, ang)) = f;
            }
            *(s32*)(e0 + offsetof(Enemy, dead_end)) = 0;
            set_enemy_trans((Enemy*)e0, lbl_803468F0, *(f32*)(e0 + offsetof(Enemy, ang)));
            {
                f32 aa = *(f32*)(e0 + offsetof(Enemy, ang));
                *(f32*)(e0 + offsetof(Enemy, pyr[1])) = turn_enemy_ang((Enemy*)e0, aa);
            }
            do_enemy_move(index);
            skip = -1;
        } else {
            skip = 0;
        }
        if (skip != 0) {
            return;
        }
        if (e->algorithm != e->prev_ai) {
            format_brain(index);
        }
        if (e->collided >= 5) {
            e->stuck_count = 0;
            e->plr_ms = *(s32*)&gPlayerWords[e->closest][653];
            if (e->plr_ms >= 0) {
                e->mode1++;
                e->mode2 = 0;
                e->collided = 0;
            }
        } else {
            s16 c = e->closest;
            f32 f;
            if (c >= 0) {
                if (*(s16*)&gPlayerWords[c][647] > 2) {
                    f = get_yaw(&gPlayerWords[c][633], &e->objgrp.worldmat[3][0]);
                } else {
                    f = get_yaw(&gPlayerWords[c][17], &e->objgrp.worldmat[3][0]);
                }
            } else {
                f = e->ang;
            }
            lbl_80344720 = f;
        }
        if (e->dead_end > 0) {
            e->dead_end -= gFrameTicks;
        }
        if (e->dead_end <= 0) {
            if (e->area == 1) {
                s32 col;
                cand = lbl_80344720;
                col = e->collided;
                if (e->route == 0) {
                    e->route = fn_8004CE38(e);
                }
                if (e->route > 0) {
                    q = (f32*)(tbl + col * 4);
                    cand = cand + q[1095];
                } else {
                    q = (f32*)(tbl + col * 4);
                    cand = cand - q[1095];
                }
            } else if (e->coll_ip != 0 || e->coll_enenum >= 0) {
                s32 col2;
                cand = e->ang;
                col2 = e->collided;
                if (e->route > 0) {
                    q = (f32*)(tbl + col2 * 4);
                    cand = cand + q[1095];
                } else {
                    q = (f32*)(tbl + col2 * 4);
                    cand = cand - q[1095];
                }
            } else {
                cand = lbl_80344720;
            }
            {
                f64 a;
                if (cand > 3.141592654) {
                    a = cand - 6.283185308;
                } else if (cand <= -3.141592654) {
                    a = 6.283185308 + cand;
                } else {
                    a = cand;
                }
                cand = a;
            }
            probe[0] = e->objgrp.worldmat[3][0];
            probe[1] = e->objgrp.worldmat[3][1];
            probe[2] = e->objgrp.worldmat[3][2];
            probe[1] += 0.1 + e->rad;
            probe[0] += speed * sin(cand);
            probe[2] += speed * cos(cand);
            {
                f32 d1;
                f32 d2;

                d1 = e->ang - e->angbak;
                *(u32*)&d1 &= 0x7FFFFFFF;
                if ((d1 > 0.034906585044444445
                     && ((d2 = cand - e->angbak), (*(u32*)&d2 &= 0x7FFFFFFF),
                         d2 <= 0.034906585044444445))
                    || fn_8004C8CC(probe, index) == 0) {
                    found = 1;
                    e->stuck_count++;
                } else {
                    e->stuck_count = 0;
                }
            }
            if (e->stuck_count > 10) {
                cand = lbl_80344720;
                e->ang = cand;
            }
            if (found == 0) {
                e->angbak = e->ang;
                e->ang = cand;
            }
        } else {
            cand = e->ang;
        }
        set_enemy_trans(e, 1.0f, cand);
        if (found == 0 || cand == lbl_80344720) {
            e->pyr[1] = turn_enemy_ang(e, cand);
        }
        do_enemy_move(index);
        break;
    }
    case 1: {
        s32 skip;
        f32 cand;
        if (*(s32*)(e0 + offsetof(Enemy, coll_pnum)) >= 0) {
            if (*(s16*)(e0 + offsetof(Enemy, algorithm)) != *(s16*)(e0 + offsetof(Enemy, prev_ai))) {
                format_brain(index);
            }
            {
                s16 c = *(s16*)(e0 + offsetof(Enemy, closest));
                f32 f;
                if (c >= 0) {
                    if (*(s16*)&gPlayerWords[c][647] > 2) {
                        f = get_yaw(&gPlayerWords[c][633], (f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0])));
                    } else {
                        f = get_yaw(&gPlayerWords[c][17], (f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0])));
                    }
                } else {
                    f = *(f32*)(e0 + offsetof(Enemy, ang));
                }
                *(f32*)(e0 + offsetof(Enemy, ang)) = f;
            }
            *(s32*)(e0 + offsetof(Enemy, dead_end)) = 0;
            set_enemy_trans((Enemy*)e0, lbl_803468F0, *(f32*)(e0 + offsetof(Enemy, ang)));
            {
                f32 aa = *(f32*)(e0 + offsetof(Enemy, ang));
                *(f32*)(e0 + offsetof(Enemy, pyr[1])) = turn_enemy_ang((Enemy*)e0, aa);
            }
            do_enemy_move(index);
            skip = -1;
        } else {
            skip = 0;
        }
        if (skip != 0) {
            return;
        }
        if (e->algorithm != e->prev_ai) {
            format_brain(index);
        }
        {
            s32 ms = e->plr_ms;
            if (ms >= 0) {
                f32 b1[3];
                if (e->stuck_count >= 5) {
                    e->plr_ms = find_neighbor_milestone(ms, ++e->mode2);
                    if (e->plr_ms < 0) {
                        e->plr_ms = ms;
                    }
                    e->stuck_count = 0;
                    e->collided = 0;
                }
                GetMilestonePos(e->plr_ms, b1);
                lbl_80344720 = get_yaw(b1, &e->objgrp.worldmat[3][0]);
            } else {
                s32 got = 0;
                if (--e->mode2 > 0) {
                    e->plr_ms = find_neighbor_milestone(e->plr_ms, e->mode2);
                    if (e->plr_ms >= 0) {
                        got = 1;
                    }
                    if (got != 0) {
                        f32 b2[3];
                        e->stuck_count = 0;
                        e->collided = 0;
                        GetMilestonePos(e->plr_ms, b2);
                        lbl_80344720 = get_yaw(b2, &e->objgrp.worldmat[3][0]);
                    }
                }
                if (got == 0) {
                    {
                        s16 c = e->closest;
                        f32 f;
                        if (c >= 0) {
                            if (*(s16*)&gPlayerWords[c][647] > 2) {
                                f = get_yaw(&gPlayerWords[c][633], &e->objgrp.worldmat[3][0]);
                            } else {
                                f = get_yaw(&gPlayerWords[c][17], &e->objgrp.worldmat[3][0]);
                            }
                        } else {
                            f = e->ang;
                        }
                        lbl_80344720 = f;
                    }
                    e->mode1 = 0;
                    e->mode2 = 0;
                    e->stuck_count = 0;
                    e->collided = 0;
                }
            }
        }
        if (e->dead_end > 0) {
            e->dead_end -= gFrameTicks;
        }
        if (e->dead_end <= 0) {
            if (e->plr_ms >= 0) {
                s32 col;
                cand = lbl_80344720;
                col = e->collided;
                if (e->route == 0) {
                    u8 _g2[24];
                    f32 b3[3];
                    f32 dx;
                    f32 dz;
                    f32 aw;
                    f32 dist1;
                    f32 sn;
                    GetMilestonePos(e->plr_ms, b3);
                    dx = e->objgrp.worldmat[3][0] - b3[0];
                    dz = e->objgrp.worldmat[3][2] - b3[2];
                    {
                        f64 av = (f32)(lbl_80346920 + e->pyr[1]);
                        if (av > 3.141592654) {
                            av -= 6.283185308;
                        } else if (av <= -3.141592654) {
                            av = 6.283185308 + av;
                        }
                        aw = av;
                    }
                    sn = sin(aw);
                    dist1 = fqdist(sn + dx, cos(aw) + dz);
                    {
                        f64 av = (f32)(e->pyr[1] - lbl_80346920);
                        if (av > 3.141592654) {
                            av -= 6.283185308;
                        } else if (av <= -3.141592654) {
                            av = 6.283185308 + av;
                        }
                        aw = av;
                    }
                    sn = sin(aw);
                    e->route = (fqdist(sn + dx, cos(aw) + dz) <= dist1)
                                   ? -1 : 1;
                }
                if (e->route > 0) {
                    q = (f32*)(tbl + col * 4);
                    cand = cand + q[1095];
                } else {
                    q = (f32*)(tbl + col * 4);
                    cand = cand - q[1095];
                }
            } else if (e->area == 1) {
                s32 col;
                cand = lbl_80344720;
                col = e->collided;
                if (e->route == 0) {
                    e->route = fn_8004CE38(e);
                }
                if (e->route > 0) {
                    q = (f32*)(tbl + col * 4);
                    cand = cand + q[1095];
                } else {
                    q = (f32*)(tbl + col * 4);
                    cand = cand - q[1095];
                }
            } else if (e->coll_ip != 0 || e->coll_enenum >= 0) {
                s32 col2;
                cand = e->ang;
                col2 = e->collided;
                if (e->route > 0) {
                    q = (f32*)(tbl + col2 * 4);
                    cand = cand + q[1095];
                } else {
                    q = (f32*)(tbl + col2 * 4);
                    cand = cand - q[1095];
                }
            } else {
                cand = lbl_80344720;
            }
            {
                f64 av;
                if ((av = cand) > 3.141592654) {
                    av -= 6.283185308;
                } else if (av <= -3.141592654) {
                    av = 6.283185308 + av;
                }
                cand = av;
            }
            probe[0] = e->objgrp.worldmat[3][0];
            probe[1] = e->objgrp.worldmat[3][1];
            probe[2] = e->objgrp.worldmat[3][2];
            probe[1] += 0.1 + e->rad;
            probe[0] += speed * sin(cand);
            probe[2] += speed * cos(cand);
            {
                f32 d3;
                f32 d4;

                d3 = e->ang - e->angbak;
                *(u32*)&d3 &= 0x7FFFFFFF;
                if ((d3 > 0.034906585044444445
                     && ((d4 = cand - e->angbak), (*(u32*)&d4 &= 0x7FFFFFFF),
                         d4 <= 0.034906585044444445))
                    || fn_8004C8CC(probe, index) == 0) {
                    found = 1;
                    e->stuck_count++;
                } else {
                    e->stuck_count = 0;
                }
            }
            if (e->stuck_count > 10) {
                cand = lbl_80344720;
                e->ang = cand;
            }
            if (found == 0) {
                e->angbak = e->ang;
                e->ang = cand;
            }
        } else {
            cand = e->ang;
        }
        set_enemy_trans(e, 1.0f, cand);
        if (found == 0 || cand == lbl_80344720) {
            e->pyr[1] = turn_enemy_ang(e, cand);
        }
        do_enemy_move(index);
        break;
    }
    default: {
        f32 cand;
        if (e->algorithm != e->prev_ai) {
            format_brain(index);
        }
        if (e->plr_ms >= 0) {
            if (e->stuck_count < 5) {
                f32 b4[3];
                GetMilestonePos(e->plr_ms, b4);
                lbl_80344720 = get_yaw(b4, &e->objgrp.worldmat[3][0]);
            } else {
                s32 v;
                e->ms_idx++;
                if (e->ms_idx > e->max_msidx
                    || (v = ((s32*)((u8*)&gPlayers + e->closest * 13148
                                    + e->ms_idx * 4))[653]) < 0) {
                    e->ms_idx = 0;
                    e->max_msidx = 4;
                    e->plr_ms = -1;
                    {
                        s16 c = e->closest;
                        f32 f;
                        if (c >= 0) {
                            if (*(s16*)&gPlayerWords[c][647] > 2) {
                                f = get_yaw(&gPlayerWords[c][633], &e->objgrp.worldmat[3][0]);
                            } else {
                                f = get_yaw(&gPlayerWords[c][17], &e->objgrp.worldmat[3][0]);
                            }
                        } else {
                            f = e->ang;
                        }
                        lbl_80344720 = f;
                    }
                } else {
                    e->plr_ms = v;
                    e->stuck_count = 0;
                }
            }
        } else {
            if (e->stuck_count >= 5) {
                e->plr_ms = ((s32*)((u8*)&gPlayers + e->closest * 13148
                                    + e->ms_idx * 4))[653];
                if (e->plr_ms >= 0) {
                    f32 b5[3];
                    GetMilestonePos(e->plr_ms, b5);
                    lbl_80344720 = get_yaw(b5, &e->objgrp.worldmat[3][0]);
                } else {
                    s16 c = e->closest;
                    f32 f;
                    if (c >= 0) {
                        if (*(s16*)&gPlayerWords[c][647] > 2) {
                            f = get_yaw(&gPlayerWords[c][633], &e->objgrp.worldmat[3][0]);
                        } else {
                            f = get_yaw(&gPlayerWords[c][17], &e->objgrp.worldmat[3][0]);
                        }
                    } else {
                        f = e->ang;
                    }
                    lbl_80344720 = f;
                }
            } else {
                s16 c = e->closest;
                f32 f;
                if (c >= 0) {
                    if (*(s16*)&gPlayerWords[c][647] > 2) {
                        f = get_yaw(&gPlayerWords[c][633], &e->objgrp.worldmat[3][0]);
                    } else {
                        f = get_yaw(&gPlayerWords[c][17], &e->objgrp.worldmat[3][0]);
                    }
                } else {
                    f = e->ang;
                }
                lbl_80344720 = f;
            }
        }
        if (e->dead_end > 0) {
            e->dead_end -= gFrameTicks;
        }
        if (e->dead_end <= 0) {
            if (e->plr_ms >= 0) {
                s32 col;
                cand = lbl_80344720;
                col = e->collided;
                if (e->route == 0) {
                    u8 _g3[24];
                    f32 b6[3];
                    f32 dx;
                    f32 dz;
                    f32 aw;
                    f32 dist1;
                    f32 sn;
                    GetMilestonePos(e->plr_ms, b6);
                    dx = e->objgrp.worldmat[3][0] - b6[0];
                    dz = e->objgrp.worldmat[3][2] - b6[2];
                    {
                        f64 av = (f32)(lbl_80346920 + e->pyr[1]);
                        if (av > 3.141592654) {
                            av -= 6.283185308;
                        } else if (av <= -3.141592654) {
                            av = 6.283185308 + av;
                        }
                        aw = av;
                    }
                    sn = sin(aw);
                    dist1 = fqdist(sn + dx, cos(aw) + dz);
                    {
                        f64 av = (f32)(e->pyr[1] - lbl_80346920);
                        if (av > 3.141592654) {
                            av -= 6.283185308;
                        } else if (av <= -3.141592654) {
                            av = 6.283185308 + av;
                        }
                        aw = av;
                    }
                    sn = sin(aw);
                    e->route = (fqdist(sn + dx, cos(aw) + dz) <= dist1)
                                   ? -1 : 1;
                }
                if (e->route > 0) {
                    q = (f32*)(tbl + col * 4);
                    cand = cand + q[1095];
                } else {
                    q = (f32*)(tbl + col * 4);
                    cand = cand - q[1095];
                }
            } else if (e->area == 1) {
                s32 col;
                cand = lbl_80344720;
                col = e->collided;
                if (e->route == 0) {
                    e->route = fn_8004CE38(e);
                }
                if (e->route > 0) {
                    q = (f32*)(tbl + col * 4);
                    cand = cand + q[1095];
                } else {
                    q = (f32*)(tbl + col * 4);
                    cand = cand - q[1095];
                }
            } else if (e->coll_ip != 0 || e->coll_enenum >= 0) {
                s32 col2;
                cand = e->ang;
                col2 = e->collided;
                if (e->route > 0) {
                    q = (f32*)(tbl + col2 * 4);
                    cand = cand + q[1095];
                } else {
                    q = (f32*)(tbl + col2 * 4);
                    cand = cand - q[1095];
                }
            } else {
                cand = lbl_80344720;
            }
            {
                f64 av;
                if ((av = cand) > 3.141592654) {
                    av -= 6.283185308;
                } else if (av <= -3.141592654) {
                    av = 6.283185308 + av;
                }
                cand = av;
            }
            probe[0] = e->objgrp.worldmat[3][0];
            probe[1] = e->objgrp.worldmat[3][1];
            probe[2] = e->objgrp.worldmat[3][2];
            probe[1] += 0.1 + e->rad;
            probe[0] += speed * sin(cand);
            probe[2] += speed * cos(cand);
            {
                f32 d5;
                f32 d6;
                u8 _g4[120];
                d5 = e->ang - e->angbak;
                *(u32*)&d5 &= 0x7FFFFFFF;
                if ((d5 > 0.034906585044444445
                     && ((d6 = cand - e->angbak), (*(u32*)&d6 &= 0x7FFFFFFF),
                         d6 <= 0.034906585044444445))
                    || fn_8004C8CC(probe, index) == 0) {
                    found = 1;
                    e->stuck_count++;
                } else {
                    e->stuck_count = 0;
                }
            }
            if (e->stuck_count > 10) {
                cand = lbl_80344720;
                e->ang = cand;
            }
            if (found == 0) {
                e->angbak = e->ang;
                e->ang = cand;
            }
        } else {
            cand = e->ang;
        }
        set_enemy_trans(e, 1.0f, cand);
        if (found == 0 || cand == lbl_80344720) {
            e->pyr[1] = turn_enemy_ang(e, cand);
        }
        do_enemy_move(index);
        break;
    }
    }
}
#pragma opt_propagation reset



/* move_logic12 @0x80049A1C (state 12, maggot-egg tether).  Shares the IT-flee /
 * chase gate, then runs a small generator-egg state machine: snap to the dest,
 * flag the egg, and hatch back when the egg reports ready. */
#pragma opt_propagation off
void move_logic12(s32 index)
{
    u8* base = (u8*)lbl_80250E00;
    Enemy* e;
    struct item* gen;
    s32 flee;
    f32 a;
    u8* p;
    s32 it;
    u8 unused[16];

    p = base + index * 916;
    it = lbl_80344748;
    gen = *(struct item**)(p + 4264);
    p += 3608;
    e = (Enemy*)(u8*)p;
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = base + it * 916;
        if (*(s32*)(other + OFF_E(state)) != ACTIVE) {
            flee = 0;
        } else if (*(f32*)(other + OFF_E(actual_dist)) > *(f32*)(p + 768)) {
            flee = 0;
        } else if (index == it || *(s16*)(p + 728) != 0 || *(s32*)(p + 856) > 0) {
            goto flee_zero;
        } else {
            f32 dx = *(f32*)(other + OFF_E(objgrp.worldmat[3][0])) - *(f32*)(p + 52);
            f32 dy = *(f32*)(other + OFF_E(objgrp.worldmat[3][1])) - *(f32*)(p + 56);
            f32 dz = *(f32*)(other + OFF_E(objgrp.worldmat[3][2])) - *(f32*)(p + 60);
            if (dx * dx + dy * dy + dz * dz < lbl_803468D8) {
                flee = -1;
            } else {
            flee_zero:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->mode1 >= 2 || gen == 0) {
        e->algorithm = 0;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->closest >= 0) {
        if (*(s16*)&gPlayerWords[e->closest][647] > 2) {
            a = get_yaw(&gPlayerWords[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            a = get_yaw(&gPlayerWords[e->closest][17], &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
    switch (e->mode1) {
    case 0:
        if (gen != 0) {
            *((u8*)gen + 230) |= e->flag1;
            e->objgrp.worldmat[3][0] = e->dest[0];
            e->objgrp.worldmat[3][1] = e->dest[1];
            e->objgrp.worldmat[3][2] = e->dest[2];
            fn_8005A65C(&e->objgrp.worldmat[0][0], e->coll_offset);
            e->mode1 = 1;
        }
        break;
    case 1:
        if (gen != 0 && *((u8*)gen + 230) == 7) {
            f32 z = lbl_80346820;
            e->dest[0] = z;
            e->dest[1] = z;
            e->dest[2] = z;
            e->mode1 = 2;
        }
        break;
    default:
        e->mode1 = 2;
        break;
    }
}
#pragma opt_propagation reset
/* move_logic13 @0x80049C70 (state 13, zombie chain-follower).  Follows its
 * prev_enemy link toward the chain head; if the debug flag is set it dumps the
 * prev/next indices.  Each frame it faces its parent, measures the gap (inline
 * sqrt), and after 180 frames "lost" (parent too far) snaps the whole downstream
 * chain to the idle algorithm and cuts itself loose. */
#pragma opt_propagation off
void move_logic13(s32 index)
{
    u8* strs = lbl_80112370;
    u8* base = (u8*)lbl_80250E00;
    Enemy* e;
    struct item* gen;
    u8* p;
    s32 it;
    s32 flee;
    u8 _pad[32];

    p = base + index * 916;
    it = lbl_80344748;
    gen = *(struct item**)(p + 4264);
    p += 3608;
    e = (Enemy*)(u8*)p;
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = base + it * 916;
        if (*(s32*)(other + OFF_E(state)) != ACTIVE) {
            flee = 0;
        } else if (*(f32*)(other + OFF_E(actual_dist)) > *(f32*)(p + 768)) {
            flee = 0;
        } else if (index == it || *(s16*)(p + 728) != 0 || *(s32*)(p + 856) > 0) {
            goto flee_zero13;
        } else {
            f32 dx = *(f32*)(other + OFF_E(objgrp.worldmat[3][0])) - *(f32*)(p + 52);
            f32 dy = *(f32*)(other + OFF_E(objgrp.worldmat[3][1])) - *(f32*)(p + 56);
            f32 dz = *(f32*)(other + OFF_E(objgrp.worldmat[3][2])) - *(f32*)(p + 60);
            if (dx * dx + dy * dy + dz * dz < lbl_803468D8) {
                flee = -1;
            } else {
            flee_zero13:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (gControllerButtons & 0x10) {
        if (index == e->prev_enemy) {
            ErrorPrintf((char*)(strs + 196), index);
        }
        if (index == e->next_enemy) {
            ErrorPrintf((char*)(strs + 208), index);
        }
        if (e->prev_enemy >= 0 && e->prev_enemy == e->next_enemy) {
            ErrorPrintf((char*)(strs + 220), index, e->prev_enemy);
        }
    }
    if (gen == 0 || e->prev_enemy < 0) {
        e->algorithm = 7;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    {
        Enemy* prev;
        f32 dy;
        f32 dx;
        f32 dz;
        f32 dist2;

        p = base + e->prev_enemy * 916;
        {
            f32* ysrc = (f32*)(p + 3660);
            prev = (Enemy*)(p += 3608);
            e->ang = get_yaw(ysrc, &e->objgrp.worldmat[3][0]);
        }
        set_enemy_trans(e, 1.0f, e->ang);
        e->pyr[1] = turn_enemy_ang(e, e->ang);
        do_enemy_move(index);
        dx = prev->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
        dy = prev->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
        dz = prev->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
        dist2 = dx * dx + dy * dy + dz * dz;
        if (dist2 > lbl_80346820) {
            volatile f32 tmp;
            f64 y = __frsqrte(dist2);
            y = 0.5 * y * (3.0 - y * y * dist2);
            y = 0.5 * y * (3.0 - y * y * dist2);
            y = 0.5 * y * (3.0 - y * y * dist2);
            tmp = (f32)(dist2 * (0.5 * y * (3.0 - y * y * dist2)));
            dist2 = tmp;
        }
        if (dist2 >= 15.0) {
            e->lost += gFrameTicks;
            if (e->lost >= 180) {
                s32 n;
                Enemy* q;
                q = (Enemy*)(base + e->prev_enemy * 916);
                q = (Enemy*)((u8*)q + 3608);
                do {
                    q->algorithm = 7;
                    q->old_ai = 7;
                    q->next_enemy = -1;
                    n = q->prev_enemy;
                    q->prev_enemy = -1;
                    q = (Enemy*)(base + n * 916);
                    q = (Enemy*)((u8*)q + 3608);
                } while (n >= 0);
                e->prev_enemy = -1;
            }
        }
    }
}
#pragma opt_propagation reset

/* move_logic14 @0x80049FD4 (state 14, plague zig-zag skirmisher).  If a player is
 * within 8 units it switches to the chase algorithm; otherwise it strafes: swing
 * the heading +/-pi/2 on a count timer, and once mode1 has built up (and the
 * drift has opened past pi/2) re-seed the strafe with a flag2-scaled offset. */
void move_logic14(s32 index)
{
    u8* e0;
    u8* base = (u8*)lbl_80250E00;
    Enemy* e;
    s32 it = lbl_80344748;
    s32 flee;
    f32 face;
    f32 drift;
    f32 diff;
    u8 _pad14[24];

    e0 = base + index * 916 + 3608;
    e = (Enemy*)(u8*)e0;
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = base + it * 916;
        if (*(s32*)(other + OFF_E(state)) != ACTIVE) {
            flee = 0;
        } else if (*(f32*)(other + OFF_E(actual_dist)) > *(f32*)(e0 + offsetof(Enemy, sight))) {
            flee = 0;
        } else if (index == it || *(s16*)(e0 + offsetof(Enemy, birth_style)) != 0 || *(s32*)(e0 + offsetof(Enemy, dead_end)) > 0) {
            goto flee_zero14;
        } else {
            f32 dx = *(f32*)(other + OFF_E(objgrp.worldmat[3][0])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0]));
            f32 dy = *(f32*)(other + OFF_E(objgrp.worldmat[3][1])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][1]));
            f32 dz = *(f32*)(other + OFF_E(objgrp.worldmat[3][2])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][2]));
            if (dx * dx + dy * dy + dz * dz < lbl_803468D8) {
                flee = -1;
            } else {
            flee_zero14:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->closest >= 0 && e->close_dist <= lbl_80346900) {
        e->algorithm = 0;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    {
        s16 c = e->closest;
        if (c >= 0) {
            if (*(s16*)&gPlayerWords[c][647] > 2) {
                face = get_yaw(&gPlayerWords[c][633], &e->objgrp.worldmat[3][0]);
            } else {
                face = get_yaw(&gPlayerWords[c][17], &e->objgrp.worldmat[3][0]);
            }
        } else {
            face = e->ang;
        }
    }
    lbl_80344720 = face;
    if ((e->count -= gFrameTicks) <= 0) {
        e->flag1 = -e->flag1;
        if (e->flag1 > 0) {
            e->ang = e->ang + lbl_80346918;
        } else {
            e->ang = e->ang - lbl_80346918;
        }
        {
            f64 a = e->ang;
            if (a > lbl_80346840) {
                a -= lbl_80346848;
            } else if (a <= lbl_80346850) {
                a = lbl_80346848 + a;
            }
            e->ang = a;
        }
        e->count += 45;
        e->mode1++;
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    set_enemy_trans(e, lbl_803468F0, e->ang);
    do_enemy_move(index);
    diff = lbl_80344720 - e->ang;
    {
        f64 d;
        if (diff > lbl_80346840) {
            d = diff - lbl_80346848;
        } else if (diff <= lbl_80346850) {
            d = lbl_80346848 + diff;
        } else {
            d = diff;
        }
        drift = d;
    }
    if (e->counter1 <= 0
        && (e->dead_end > 0
            || (e->mode1 >= 4 && fabsf_(drift) > lbl_80346918))) {
        f32 scale = (f32)(lbl_80346930 * (f64)e->flag2 + lbl_80346908);
        if (e->mode1 >= 4) {
            e->flag1 = -e->flag1;
        }
        e->dead_end = 0;
        e->ang = get_yaw(&gPlayerWords[e->closest][17], &e->objgrp.worldmat[3][0]);
        if (e->flag1 > 0) {
            e->ang = e->ang + scale;
        } else {
            e->ang = e->ang - scale;
        }
        {
            f64 a = e->ang;
            if (a > lbl_80346840) {
                a -= lbl_80346848;
            } else if (a <= lbl_80346850) {
                a = lbl_80346848 + a;
            }
            e->ang = a;
        }
        e->count = 0;
        e->mode1 = 0;
        e->flag2++;
        e->counter1 = 30;
    } else {
        if (e->flag2 > 0) {
            e->flag2--;
        }
        e->counter1 -= gFrameTicks;
    }
}

/* move_logic15 @0x8004A430 (state 15, wolf prowl patrol).  When it loses the
 * player it walks a linked prowl-node path: on entry (mode1==0) it scans the node
 * table for the nearest active node, then each frame faces the current node and
 * advances to the node's link when it arrives.  A player inside 0.8*sight (and its
 * milestone likewise) snaps it back to the chase algorithm. */
#pragma opt_propagation off
void move_logic15(s32 index)
{
    u8* base = (u8*)lbl_80250E00;
    u8* row15;
    Enemy* e;
    s32 it = lbl_80344748;
    s32 flee;
    f32 ady;
    u8 _pad15[48];

    row15 = base + index * 916;
    row15 += 3608;
    e = (Enemy*)(u8*)row15;
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = base + it * 916;
        if (*(s32*)(other + OFF_E(state)) != ACTIVE) {
            flee = 0;
        } else if (*(f32*)(other + OFF_E(actual_dist)) > *(f32*)(row15 + 768)) {
            flee = 0;
        } else if (index == it || *(s16*)(row15 + 728) != 0 || *(s32*)(row15 + 856) > 0) {
            goto flee_zero15;
        } else {
            f32 dx = *(f32*)(other + OFF_E(objgrp.worldmat[3][0])) - *(f32*)(row15 + 52);
            f32 dy = *(f32*)(other + OFF_E(objgrp.worldmat[3][1])) - *(f32*)(row15 + 56);
            f32 dz = *(f32*)(other + OFF_E(objgrp.worldmat[3][2])) - *(f32*)(row15 + 60);
            if (dx * dx + dy * dy + dz * dz < lbl_803468D8) {
                flee = -1;
            } else {
            flee_zero15:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->closest >= 0 && e->close_dist <= 0.8 * e->sight) {
        f32 d = fqdist(gPlayerWords[e->closest][17] - e->objgrp.worldmat[3][0],
                            gPlayerWords[e->closest][19] - e->objgrp.worldmat[3][2]);
        if (d <= 0.8 * e->sight) {
            e->algorithm = 0;
            do_ai(index);
            e->mode1 = 0;
            return;
        }
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    switch (e->mode1) {
    case 0: {
        u8* n = sLookoutParams;
        s32 i;
        s32 best_idx = -1;
        f32 best_dist = lbl_803468B0;
        f32 thresh = lbl_80346820;
        f32 d;
        f32 ex = e->objgrp.worldmat[3][0];
        f32 ey = e->objgrp.worldmat[3][1];
        f32 ez = e->objgrp.worldmat[3][2];

        for (i = 0; i < sNumLookoutParams; i++, n += 108) {
            if (*(s16*)(n + 104) >= 0) {
                f32 dx = *(f32*)(n + 48) - ex;
                f32 dy = *(f32*)(n + 52) - ey;
                f32 dz = *(f32*)(n + 56) - ez;
                if ((d = dx * dx + dy * dy + dz * dz) > thresh) {
                    volatile f32 tmp;
                    f64 y = __frsqrte(d);
                    y = lbl_80346830 * y * (lbl_803468B8 - y * y * d);
                    y = lbl_80346830 * y * (lbl_803468B8 - y * y * d);
                    y = lbl_80346830 * y * (lbl_803468B8 - y * y * d);
                    tmp = (f32)(d * (lbl_80346830 * y * (lbl_803468B8 - y * y * d)));
                    d = tmp;
                }
                if (d < best_dist) {
                    best_dist = d;
                    best_idx = i;
                }
            }
        }
        e->flag1 = best_idx;
        e->mode1 = 1;
    }
    case 1: {
        u8* n = sLookoutParams + e->flag1 * 108;
        f32 dy;
        f32 dx;
        f32 dz;

        e->ang = get_yaw((f32*)(n + 48), &e->objgrp.worldmat[3][0]);
        dy = *(f32*)(n + 52) - e->objgrp.worldmat[3][1];
        dx = *(f32*)(n + 48) - e->objgrp.worldmat[3][0];
        dz = *(f32*)(n + 56) - e->objgrp.worldmat[3][2];
        ady = dy;
        *(u32*)&ady &= 0x7FFFFFFF;
        if (ady < lbl_80346948 && fqdist(dx, dz) < lbl_80346810) {
            e->flag1 = *(s16*)(n + 104);
        }
        break;
    }
    default:
        break;
    }
move:
    set_enemy_trans(e, 1.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}
#pragma opt_propagation reset

/* move_logic16 @0x8004A78C (state 16, ice leap-attacker).  Faces the target, and
 * when the player is at a shallow height difference it arms (flag1) inside 0.6*
 * sight; once armed and inside 0.8*sight it charges a leap speed off lbl_8011BF60,
 * then fires a run-attack action, aiming the leap 180deg + a ramp offset. */
void move_logic16(s32 index)
{
    s32 stuck;
    u8* base = (u8*)lbl_80250E00;
    u8* row16;
    u8* e0;
    Enemy* e;
    s32 it;
    s32 dend;
    f32 leapspeed = lbl_80346820;
    s32 flee;
    f32 a;
    u8 _pad16[24];

    row16 = base + index * 916;
    dend = *(s32*)(row16 + 4464);
    e0 = row16 + 3608;
    e = (Enemy*)(u8*)e0;
    if (dend > 0) {
        stuck = 1;
    } else {
        stuck = 0;
    }
    it = lbl_80344748;
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = base + it * 916;
        if (*(s32*)(other + OFF_E(state)) != ACTIVE) {
            flee = 0;
        } else if (*(f32*)(other + OFF_E(actual_dist)) > *(f32*)(e0 + offsetof(Enemy, sight))) {
            flee = 0;
        } else if (index == it || *(s16*)(e0 + offsetof(Enemy, birth_style)) != 0 || dend > 0) {
            goto flee_zero16;
        } else {
            f32 dx = *(f32*)(other + OFF_E(objgrp.worldmat[3][0])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0]));
            f32 dy = *(f32*)(other + OFF_E(objgrp.worldmat[3][1])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][1]));
            f32 dz = *(f32*)(other + OFF_E(objgrp.worldmat[3][2])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][2]));
            if (dx * dx + dy * dy + dz * dz < lbl_803468D8) {
                flee = -1;
            } else {
            flee_zero16:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    {
        s16 c = e->closest;
        if (c >= 0) {
            if (*(s16*)&gPlayerWords[c][647] > 2) {
                a = get_yaw(&gPlayerWords[c][633], &e->objgrp.worldmat[3][0]);
            } else {
                a = get_yaw(&gPlayerWords[c][17], &e->objgrp.worldmat[3][0]);
            }
        } else {
            a = e->ang;
        }
    }
    e->ang = a;
    {
    s16 c16 = e->closest;
    if (c16 >= 0) {
        u8* gp = (u8*)&gPlayers + c16 * 13148;
        f32 dvert = e->objgrp.worldmat[3][1] - *(f32*)(gp + 72);
        if (e->visactive != 0 && dvert >= -10.0 && dvert <= 10.0) {
            if (e->flag1 == 0) {
                if (e->actual_dist <= 0.6 * e->sight) {
                    e->flag1 = 1;
                }
            } else if (e->actual_dist > 0.8 * e->sight) {
                e->flag1 = 0;
            } else if (e->dead_end > 0) {
                s32 leap = e->counter1;
                if (leap < 8) {
                    e->counter1 = leap + 1;
                    leapspeed = lbl_8011BF60[leap];
                    e->dead_end = 0;
                } else {
                    e->flag1 = 0;
                }
            }
            if (e->flag2 > 0) {
                e->flag2 -= gFrameTicks;
            } else if (e->flag1 == 0) {
                RequestEnemyAction(e, 24);
            } else {
                f32 v = 3.141592654 + e->ang + leapspeed;
                f32 la;
                {
                    f64 nv;
                    if (v > 3.141592654) {
                        nv = v - 6.283185308;
                    } else if (v <= -3.141592654) {
                        nv = 6.283185308 + v;
                    } else {
                        nv = v;
                    }
                    la = nv;
                }
                RequestEnemyAction(e, 22);
                if (e->action == 22 || e->action == 23) {
                    set_enemy_trans(e, 0.8f, la);
                }
                e->dead_end = 0;
            }
        }
    }
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
    if (e->moved != 0) {
        e->dead_end = 0;
    }
    if (stuck == 0 && e->dead_end > 0) {
        e->anghit = e->ang;
        e->counter1 = 0;
    }
}

/* move_logic18 @0x8004AB20 (state 18, dog stalk-and-pounce).  Creeps to a crouch
 * (mode1 1) inside sight, holds a ready pose (RequestEnemyAction 9) until the run
 * anim fires, then charges: it ramps a leap speed off lbl_8011BF60 and after 240
 * frames of pursuit (or a player contact) turns "IT" - clearing the IT slot and
 * dealing 999 damage to the caught player. */
void move_logic18(s32 index)
{
    s32 stuck;
    Enemy* e = &gEnemies[index];
    f32 leapspeed = lbl_80346820;
    s32 dend = e->dead_end;
    s16 sVar1;
    f32 a;

    if (dend > 0) {
        stuck = 1;
    } else {
        stuck = 0;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->closest >= 0) {
        if (*(s16*)&gPlayerWords[e->closest][647] > 2) {
            a = get_yaw(&gPlayerWords[e->closest][633],
                        &e->objgrp.worldmat[3][0]);
        } else {
            a = get_yaw(&gPlayerWords[e->closest][17],
                        &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    switch (e->mode1) {
    default:
    case 0:
        if (e->closest >= 0 && e->actual_dist <= e->sight) {
            e->mode1++;
            e->flag1 = 60;
            e->flag2 = 0;
        }
        goto move;
    case 1:
        if (e->closest >= 0 && e->action != 4
            && (e->flag1 -= gFrameTicks) <= 0) {
            RequestEnemyAction(e, 9);
        }
        if (e->action != 4) {
            goto move;
        }
        e->mode1++;
        fn_8009DD6C(&e->objgrp.attn_pos[0]);
    case 2:
        e->flag2 += gFrameTicks;
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->area == 2 && e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    if (e->dead_end > 0) {
        s32 leap = e->counter1;
        if (leap < 16) {
            e->counter1 = leap + 1;
            leapspeed = lbl_8011BF60[leap];
            e->dead_end = 0;
        }
        e->ang = e->ang + leapspeed;
        {
            f64 av = e->ang;
            if (av > lbl_80346840) {
                av -= lbl_80346848;
            } else if (av <= lbl_80346850) {
                av = lbl_80346848 + av;
            }
            e->ang = av;
        }
    }
    if (e->dead_end <= 0
        || fabsf_(e->ang - e->anghit) >= lbl_803468F8) {
        e->dead_end = 0;
        set_enemy_trans(e, lbl_80346964, e->ang);
    }
    }
move:
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
    if (e->moved != 0) {
        e->dead_end = 0;
    }
    if ((f64)e->flag2 >= lbl_80346968 || e->coll_pnum >= 0) {
        lbl_80344748 = -1;
        damage_enemy(e, lbl_80346970, -2, 1, 0, 0, 0);
    } else if (stuck == 0 && e->dead_end > 0) {
        e->anghit = e->ang;
        e->counter1 = 0;
    }
}

/* move_logic19 @0x8004AE94 (state 19, skeleton assemble-and-fight).  Wait
 * dormant until a player is in sight, then rise (flag1), face the closest
 * player, run a 3-phase attack cadence, and spawn bone FX on the anim frames. */
void move_logic19(s32 index)
{
    Enemy* e = &gEnemies[index];
    animinfo* ai = &e->atree.animinfo;
    s32 mode;
    f32 a;

    if (e->flag1 == 0 && e->closest >= 0 && e->actual_dist < e->sight
        && e->visactive != 0) {
        e->flag1 = 1;
        fn_8009DDCC(&e->objgrp.worldmat[3][0]);
    }
    if (e->flag1 == 0) {
        ai->active = 0;
        return;
    }
    ai->active = 1;
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->closest >= 0) {
        if (*(s16*)&gPlayerWords[e->closest][647] > 2) {
            a = get_yaw(&gPlayerWords[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            a = get_yaw(&gPlayerWords[e->closest][17], &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    mode = e->mode1;
    switch (mode) {
    case 0:
    default:
        e->counter1 = 1;
        e->mode1++;
    case 1:
        RequestEnemyAction(e, 3);
        if (e->action == 3) {
            e->counter1 = RandInt(90) + 30;
            e->mode1++;
        }
        if (e->mode1 != 2) {
            goto move;
        }
        break;
    case 2:
        break;
    }
    if (e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    if (e->dead_end <= 0) {
        set_enemy_trans(e, 1.0f, e->ang);
    }
    if ((e->counter1 -= gFrameTicks) <= 0 && e->daction == 3) {
        e->daction = 0;
    }
    if ((e->dead_end > 0 && e->area == 1) || e->action == 0) {
        e->mode1 = 0;
    }
move:
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
    if (e->action == 3) {
        if (e->dead_end > 0 && e->counter1 > 1) {
            e->counter1 = 1;
        }
        if ((e->prev_frame < 10.0 && ai->frame >= 10.0)
            || (e->prev_frame < 25.0 && ai->frame >= 25.0)) {
            fn_8009E03C(e);
        }
    }
    if ((e->action == 12 || e->action == 14 || e->action == 16)
        && e->prev_frame < 2.0 && ai->frame >= 2.0) {
        fn_8009DD9C(&e->objgrp.worldmat[3][0]);
    }
    e->prev_frame = ai->frame;
}

/* move_logic20 @0x8004B19C (state 20, ghost flee-drift).  Sibling of move_logic07
 * but it always heads 180deg away from the target (pi + the facing bearing) and
 * corner-hugs that flee heading, counting stuck frames and snapping back to the
 * straight flee bearing after 10. */
#pragma opt_propagation off
void move_logic20(s32 index)
{
    Enemy* e = &gEnemies[index];
    s32 found = 0;
    u8* tbl = (u8*)lbl_8011AF48;
    f32 speed = ((f32*)lbl_80250E40.words)[e->type];
    f32 cand;
    f32* q;
    f32 probe[3];
    f32 d1;
    f32 d2;
    s32 col;
    s32 col2;
    u8 unusedB[16];

    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    {
        s16 c = e->closest;
        f32 f;
        if (c >= 0) {
            if (*(s16*)&gPlayerWords[c][647] > 2) {
                f = get_yaw(&gPlayerWords[c][633], &e->objgrp.worldmat[3][0]);
            } else {
                f = get_yaw(&gPlayerWords[c][17], &e->objgrp.worldmat[3][0]);
            }
        } else {
            f = e->ang;
        }
        lbl_80344720 = f;
    }
    if (e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    if (e->dead_end <= 0) {
        if (e->coll_pnum >= 0) {
            cand = 3.141592654 + lbl_80344720;
        } else if (e->area == 1) {
            cand = 3.141592654 + lbl_80344720;
            col = e->collided;
            if (e->route == 0) {
                e->route = fn_8004CE38(e);
            }
            if (e->route > 0) {
                q = (f32*)(tbl + col * 4);
                cand = cand + q[1087];
            } else {
                q = (f32*)(tbl + col * 4);
                cand = cand - q[1087];
            }
        } else if (e->coll_ip != 0 || e->coll_enenum >= 0) {
            cand = e->ang;
            col2 = e->collided;
            if (e->route > 0) {
                q = (f32*)(tbl + col2 * 4);
                cand = cand + q[1087];
            } else {
                q = (f32*)(tbl + col2 * 4);
                cand = cand - q[1087];
            }
        } else {
            cand = 3.141592654 + lbl_80344720;
        }
        {
            f64 a;
            if (cand > 3.141592654) {
                a = cand - 6.283185308;
            } else if (cand <= -3.141592654) {
                a = 6.283185308 + cand;
            } else {
                a = cand;
            }
            cand = a;
        }
        probe[0] = e->objgrp.worldmat[3][0];
        probe[1] = e->objgrp.worldmat[3][1];
        probe[2] = e->objgrp.worldmat[3][2];
        probe[1] += 0.1 + e->rad;
        probe[0] += speed * sin(cand);
        probe[2] += speed * cos(cand);
        d1 = e->ang - e->angbak;
        *(u32*)&d1 &= 0x7FFFFFFF;
        if ((d1 > 0.034906585044444445
             && ((d2 = cand - e->angbak), (*(u32*)&d2 &= 0x7FFFFFFF),
                 d2 <= 0.034906585044444445))
            || fn_8004C8CC(probe, index) == 0) {
            found = 1;
            e->stuck_count++;
        } else {
            e->stuck_count = 0;
        }
        if (e->stuck_count > 10) {
            f64 sum = 3.141592654 + lbl_80344720;
            f64 a = (f32)sum;
            if (a > 3.141592654) {
                a -= 6.283185308;
            } else if (a <= -3.141592654) {
                a = 6.283185308 + a;
            }
            cand = a;
            e->ang = cand;
        }
        if (found == 0) {
            e->angbak = e->ang;
            e->ang = cand;
        }
    } else {
        cand = e->ang;
    }
    set_enemy_trans(e, 2.0f, cand);
    if (found != 0) {
        f64 w = 3.141592654 + lbl_80344720;
        if (w > 3.141592654) {
            w = w - 6.283185308;
        } else if (w <= -3.141592654) {
            w = 6.283185308 + w;
        }
        if (cand != w) {
            goto skip20;
        }
    }
    e->pyr[1] = turn_enemy_ang(e, cand);
skip20:
    do_enemy_move(index);
}
#pragma opt_propagation reset

/* move_logic21 @0x8004B5AC (state 21, acid-splat flee-and-face).  If it hasn't
 * recognized a target yet, bounce to a wander algorithm; otherwise ramp a
 * retreat speed, face away from the closest player (facing + pi), normalize,
 * accelerate + turn + move, and refresh the corner state. */
void move_logic21(s32 index)
{
    s32 stuck;
    Enemy* e = &gEnemies[index];
    f32 spd = 0.0f;
    s32 c;
    f32 face;

    if (e->dead_end > 0) {
        stuck = 1;
    } else {
        stuck = 0;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->dead_end > 0 && (c = e->counter1) < 8) {
        e->counter1 = c + 1;
        spd = lbl_8011BF60[c];
        e->dead_end = 0;
    }
    if (e->closest >= 0) {
        if (*(s16*)&gPlayerWords[e->closest][647] > 2) {
            face = get_yaw(&gPlayerWords[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            face = get_yaw(&gPlayerWords[e->closest][17], &e->objgrp.worldmat[3][0]);
        }
    } else {
        face = e->ang;
    }
    e->ang = spd + (3.141592654 + face);
    {
        f64 a;

        if ((a = e->ang) > 3.141592654) {
            a -= 6.283185308;
        } else if (a <= -3.141592654) {
            a = 6.283185308 + a;
        }
        e->ang = a;
    }
    set_enemy_trans(e, 2.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
    if (e->moved != 0) {
        e->dead_end = 0;
    }
    if (stuck == 0 && e->dead_end > 0) {
        e->anghit = e->ang;
        e->counter1 = 0;
    }
}

/* move_logic22 @0x8004B788 (state 22, hand milestone-crawler).  On entry (mode1 0)
 * it scans the milestone-node network for the nearest node, then walks the linked
 * path: when it reaches a node (within 1.5) it asks fn_800511D0 for the next node,
 * ending (mode1 -1) when the path loops back on itself. */
void move_logic22(s32 index)
{
    u8* row22;
    u8* base = (u8*)lbl_80250E00;
    u8* e0;
    Enemy* e;
    s32 it = lbl_80344748;
    s32 flee;
    u8 _pad22_hi[60];
    f32 buf1[3];
    f32 buf2[3];
    volatile f32 tmp;
    u8 _pad22_lo[12];

    e0 = (row22 = base + index * 916) + 3608;
    e = (Enemy*)(u8*)e0;
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = base + it * 916;
        if (*(s32*)(other + OFF_E(state)) != ACTIVE) {
            flee = 0;
        } else if (*(f32*)(other + OFF_E(actual_dist)) > *(f32*)(e0 + offsetof(Enemy, sight))) {
            flee = 0;
        } else if (index == it || *(s16*)(e0 + offsetof(Enemy, birth_style)) != 0 || *(s32*)(e0 + offsetof(Enemy, dead_end)) > 0) {
            goto flee_zero22;
        } else {
            f32 dx = *(f32*)(other + OFF_E(objgrp.worldmat[3][0])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0]));
            f32 dy = *(f32*)(other + OFF_E(objgrp.worldmat[3][1])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][1]));
            f32 dz = *(f32*)(other + OFF_E(objgrp.worldmat[3][2])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][2]));
            if (dx * dx + dy * dy + dz * dz < lbl_803468D8) {
                flee = -1;
            } else {
            flee_zero22:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    switch (e->mode1) {
    case 0: {
        s32 i;
        u8* node;
        s32 best_idx = -1;
        f32 best_dist = lbl_803468B0;

        for (node = sMilestones, i = 0; i < sNumMilestones;
             i++, node += 104) {
            f32 dx = e->objgrp.worldmat[3][0] - *(f32*)(node + 48);
            f32 dy = e->objgrp.worldmat[3][1] - *(f32*)(node + 52);
            f32 dz = e->objgrp.worldmat[3][2] - *(f32*)(node + 56);
            f32 d;
            if ((d = dx * dx + dy * dy + dz * dz) > lbl_80346820) {
                f64 y = __frsqrte(d);
                y = lbl_80346830 * y * (lbl_803468B8 - y * y * d);
                y = lbl_80346830 * y * (lbl_803468B8 - y * y * d);
                y = lbl_80346830 * y * (lbl_803468B8 - y * y * d);
                tmp = (f32)(d * (lbl_80346830 * y * (lbl_803468B8 - y * y * d)));
                d = tmp;
            }
            if (d < best_dist) {
                best_idx = i;
                best_dist = d;
            }
        }
        e->flag1 = best_idx;
        GetMilestonePos(e->flag1, buf1);
        e->ang = get_yaw(buf1, &e->objgrp.worldmat[3][0]);
        e->mode1 = 1;
    }
    default: {
        u8* node = sMilestones + e->flag1 * 104;
        f32 dist = fqdist(*(f32*)(node + 48) - e->objgrp.worldmat[3][0],
                          *(f32*)(node + 56) - e->objgrp.worldmat[3][2]);
        if (dist <= lbl_80346838) {
            s32 old = e->flag1;
            e->flag1 = fn_800511D0(old, lbl_80346984);
            if (e->flag1 != old) {
                e->mode1++;
            } else {
                e->mode1 = -1;
            }
        }
        GetMilestonePos(e->flag1, buf2);
        e->ang = get_yaw(buf2, &e->objgrp.worldmat[3][0]);
        break;
    }
    case -1: {
        s16 c = e->closest;
        f32 f;
        if (c >= 0) {
            if (*(s16*)&gPlayerWords[c][647] > 2) {
                f = get_yaw(&gPlayerWords[c][633], &e->objgrp.worldmat[3][0]);
            } else {
                f = get_yaw(&gPlayerWords[c][17], &e->objgrp.worldmat[3][0]);
            }
        } else {
            f = e->ang;
        }
        e->ang = f;
        break;
    }
    }
    if (e->mode1 >= 0) {
        set_enemy_trans(e, 1.0f, e->ang);
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* move_logic23 @0x8004BB14 (state 17 + 23 share it, imp behaviour).  Face the
 * closest player (via a milestone position when it has one), and if the player
 * is within sight and roughly the same height, count down flag2 / poke a
 * throw action; then turn + move. */
void move_logic23(s32 index)
{
    Enemy* e = &gEnemies[index];
    f32 a;

    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->closest >= 0) {
        if (*(s16*)&gPlayerWords[e->closest][647] > 2) {
            a = get_yaw(&gPlayerWords[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            a = get_yaw(&gPlayerWords[e->closest][17], &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    if (e->closest >= 0) {
        f32* player = gPlayerWords[e->closest];
        f32 sight = e->sight;
        f32 dy = e->objgrp.worldmat[3][1] - player[18];
        if (e->visactive != 0 && e->actual_dist <= sight
            && dy >= -10.0 && dy <= 10.0) {
            if (e->flag2 <= 0) {
                RequestEnemyAction(e, 24);
            } else {
                e->flag2 -= gFrameTicks;
            }
        }
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* move_logic24 @0x8004BC5C (state 24, "IT"/warlock chase).  Ramp a retreat
 * speed the first frames after being cornered, face the IT enemy plus pi,
 * normalize, then accelerate + turn + move; refresh the corner state after. */
void move_logic24(s32 index)
{
    s32 stuck;
    Enemy* e = &gEnemies[index];
    f32 spd = 0.0f;
    s32 c;

    if (e->dead_end > 0) {
        stuck = 1;
    } else {
        stuck = 0;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->dead_end > 0 && (c = e->counter1) < 8) {
        e->counter1 = c + 1;
        spd = lbl_8011BF60[c];
        e->dead_end = 0;
    }
    e->ang = spd
        + (3.141592654
           + get_yaw(&gEnemies[lbl_80344748].objgrp.worldmat[3][0],
                         &e->objgrp.worldmat[3][0]));
    {
        f64 a;

        if ((a = e->ang) > 3.141592654) {
            a -= 6.283185308;
        } else if (a <= -3.141592654) {
            a = 6.283185308 + a;
        }
        e->ang = a;
    }
    set_enemy_trans(e, 2.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
    if (e->moved != 0) {
        e->dead_end = 0;
    }
    if (stuck == 0 && e->dead_end > 0) {
        e->anghit = e->ang;
        e->counter1 = 0;
    }
}

/* move_logic28 @0x8004BDDC (state 28, imp close-quarters).  If the target is
 * in melee range, switch to the run algorithm and re-dispatch; otherwise face
 * the target and, when lined up, throw / power-attack on a cadence. */
void move_logic28(s32 index)
{
    Enemy* e = &gEnemies[index];
    f32 a;
    u8 unused[8];

    if (e->closest >= 0 && e->actual_dist <= 6.0) {
        e->algorithm = 7;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->closest >= 0) {
        if (*(s16*)&gPlayerWords[e->closest][647] > 2) {
            a = get_yaw(&gPlayerWords[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            a = get_yaw(&gPlayerWords[e->closest][17], &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    if (e->closest >= 0) {
        f32* player = gPlayerWords[e->closest];
        f32 sight = e->sight;
        f32 dy = e->objgrp.worldmat[3][1] - player[18];
        if (e->visactive != 0 && e->actual_dist <= sight
            && dy >= -10.0 && dy <= 10.0) {
            if (e->flag2 <= 0) {
                if (e->org_lvl >= 3) {
                    RequestEnemyAction(e, 16);
                } else if ((++e->counter2 & 1) != 0) {
                    RequestEnemyAction(e, 12);
                } else {
                    RequestEnemyAction(e, 14);
                }
            } else {
                e->flag2 -= gFrameTicks;
            }
        }
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* move_logic29 @0x8004BF9C (state 29, golem swing-attacker).  Approaches until the
 * player is within 6, then runs a range state machine (flag1): near hits swing
 * attacks (alternating 12/14, or a heavy 16 at high level); at leap range it charges
 * off lbl_8011BF60 and fires a lunge (action 3) aimed 180deg + ramp when flag1==1. */
void move_logic29(s32 index)
{
    s32 stuck;
    u8* base = (u8*)lbl_80250E00;
    u8* row29;
    u8* e0;
    Enemy* e;
    s32 it;
    s32 dend;
    f32 leapspeed = lbl_80346820;
    s32 flee;
    f32 a;
    u8 _pad29[32];

    row29 = base + index * 916;
    dend = *(s32*)(row29 + 4464);
    e0 = row29 + 3608;
    e = (Enemy*)(u8*)e0;
    if (dend > 0) {
        stuck = 1;
    } else {
        stuck = 0;
    }
    it = lbl_80344748;
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = base + it * 916;
        if (*(s32*)(other + OFF_E(state)) != ACTIVE) {
            flee = 0;
        } else if (*(f32*)(other + OFF_E(actual_dist)) > *(f32*)(e0 + offsetof(Enemy, sight))) {
            flee = 0;
        } else if (index == it || *(s16*)(e0 + offsetof(Enemy, birth_style)) != 0 || dend > 0) {
            goto flee_zero29;
        } else {
            f32 dx = *(f32*)(other + OFF_E(objgrp.worldmat[3][0])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0]));
            f32 dy = *(f32*)(other + OFF_E(objgrp.worldmat[3][1])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][1]));
            f32 dz = *(f32*)(other + OFF_E(objgrp.worldmat[3][2])) - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][2]));
            if (dx * dx + dy * dy + dz * dz < lbl_803468D8) {
                flee = -1;
            } else {
            flee_zero29:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->closest >= 0 && e->actual_dist <= 6.0) {
        e->algorithm = 7;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    {
        s16 c = e->closest;
        if (c >= 0) {
            if (*(s16*)&gPlayerWords[c][647] > 2) {
                a = get_yaw(&gPlayerWords[c][633], &e->objgrp.worldmat[3][0]);
            } else {
                a = get_yaw(&gPlayerWords[c][17], &e->objgrp.worldmat[3][0]);
            }
        } else {
            a = e->ang;
        }
    }
    e->ang = a;
    {
    s16 c29 = e->closest;
    if (c29 >= 0) {
        u8* gp = (u8*)&gPlayers + c29 * 13148;
        f32 dvert = e->objgrp.worldmat[3][1] - *(f32*)(gp + 72);
        if (e->visactive != 0 && dvert >= -10.0 && dvert <= 10.0) {
            if (e->flag1 == 0) {
                if (e->actual_dist <= 8.0) {
                    e->flag1 = 1;
                }
                if (e->actual_dist > 18.0) {
                    e->flag1 = 2;
                }
            } else {
                if (e->flag1 == 1) {
                    if (e->actual_dist > 10.0) {
                        e->flag1 = 0;
                    }
                } else if (e->actual_dist <= 16.0) {
                    e->flag1 = 0;
                }
                if (e->flag1 != 0 && e->dead_end > 0) {
                    s32 leap = e->counter1;
                    if (leap < 8) {
                        e->counter1 = leap + 1;
                        leapspeed = lbl_8011BF60[leap];
                        e->dead_end = 0;
                    } else {
                        e->flag1 = 0;
                    }
                }
            }
            if (e->flag2 > 0) {
                e->flag2 -= gFrameTicks;
            } else if (e->flag1 == 0 || e->area == 1) {
                if (e->org_lvl >= 3) {
                    RequestEnemyAction(e, 16);
                } else if ((++e->counter2 & 1) != 0) {
                    RequestEnemyAction(e, 12);
                } else {
                    RequestEnemyAction(e, 14);
                }
            } else {
                f32 v;
                f32 la;
                if (e->flag1 == 1) {
                    v = 3.141592654 + e->ang + leapspeed;
                } else {
                    v = e->ang + leapspeed;
                }
                {
                    f64 nv;
                    if (v > 3.141592654) {
                        nv = v - 6.283185308;
                    } else if (v <= -3.141592654) {
                        nv = 6.283185308 + v;
                    } else {
                        nv = v;
                    }
                    la = nv;
                }
                set_enemy_trans(e, 0.8f, la);
                RequestEnemyAction(e, 3);
                e->dead_end = 0;
            }
        }
    }
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
    if (e->moved != 0) {
        e->dead_end = 0;
    }
    if (stuck == 0 && e->dead_end > 0) {
        e->anghit = e->ang;
        e->counter1 = 0;
    }
}

/* move_logic30 @0x8004C3E4 (state 30, death-lure wander).  IT-flee / chase /
 * melee gates, then a randomized roam timer scaled by level, taunt cues, and a
 * delegated base wander (move_logic00) with the algorithm parked at 30. */
void move_logic30(s32 index)
{
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608);
    s32 it = lbl_80344748;
    s32 flee;
    u8 unused[24];

    if (it < 0) {
        flee = 0;
    } else {
        u8* op = (u8*)lbl_80250E00 + it * 916;
        if (*(s32*)(op + 3788) != 1) {
            flee = 0;
        } else if (*(f32*)(op + 4244) > e->sight) {
            flee = 0;
        } else if (index == it || e->birth_style != 0 || e->dead_end > 0) {
            goto flee_zero30;
        } else {
            f32 dx = *(f32*)(op + 3660) - e->objgrp.worldmat[3][0];
            f32 dy = *(f32*)(op + 3664) - e->objgrp.worldmat[3][1];
            f32 dz = *(f32*)(op + 3668) - e->objgrp.worldmat[3][2];
            if (dx * dx + dy * dy + dz * dz < lbl_803468D8) {
                flee = -1;
            } else {
            flee_zero30:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->closest >= 0 && e->actual_dist <= lbl_80346988) {
        e->algorithm = 7;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->dead_end <= 0 && (e->counter1 -= gFrameTicks) <= 0) {
        s32 n = (s32)(lbl_803469A0 * gCurLevel->ene_mrate);
        s32 r = RandInt(10) + 20;
        if (e->dead_end <= 0) {
            e->dead_end = r;
            if (r >= 60) {
                if (e->daction == 3 || e->daction == 4) {
                    e->daction = 0;
                }
            }
        }
        e->counter1 = e->dead_end + RandInt(n >> 1) + n;
    }
    if (e->dead_end > 0 && e->visactive != 0) {
        if (e->org_lvl >= 2) {
            RequestEnemyAction(e, 16);
        } else {
            RequestEnemyAction(e, 12);
        }
    }
    e->algorithm = 0;
    move_logic00(index);
    e->algorithm = 30;
}

/* Integrate the heading into a horizontal velocity, refreshing the cached
 * sin/cos when the heading changed; scaled by k and the per-type speed table
 * at lbl_80250E40 (= lbl_80250E00 + 64).  Inlined into move_logic31. */
static void update_vel(Enemy* e, f32 k)
{
    f32 ang = e->ang;
    f32 spd;
    f32 vx;
    f32 vz;

    if (e->prev_dir != ang) {
        e->xspd = sin(ang);
        e->zspd = cos(ang);
        e->prev_dir = ang;
    }
    spd = ((f32*)lbl_80250E40.words)[e->type];
    vx = k * (e->xspd * spd);
    vz = k * (e->zspd * spd);
    e->trans[0] += vx;
    e->trans[2] += vz;
}

/* move_logic31 @0x8004C650 (state 31, IT tag-runner).  Face the closest player,
 * then per current action integrate a sin/cos velocity along the heading (scaled
 * by a per-type speed table), roll cooldown timers, and cue attacks. */
void move_logic31(s32 index)
{
    Enemy* e = &gEnemies[index];
    f32 a;
    u8 unused[16];

    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->closest >= 0) {
        if (*(s16*)&gPlayerWords[e->closest][647] > 2) {
            a = get_yaw(&gPlayerWords[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            a = get_yaw(&gPlayerWords[e->closest][17], &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    if (e->closest >= 0) {
        e->daction = 0;
        if (e->visactive != 0) {
            s32 act = e->action;
            if (act == 12) {
                goto action_12_or_13;
            }
            if (act != 13) {
                goto other_action31;
            }
action_12_or_13:
            {
                update_vel(e, 1.0f);
                e->flag2 = RandInt(30) + 30;
                if (e->actual_dist <= 7.5) {
                    e->attack_index = e->closest;
                }
                goto action_done31;
            }
other_action31:
            if (act == 16 || act == 17) {
                e->flag2 = RandInt(30) + 30;
            } else if (e->flag2 <= 0) {
                if (e->actual_dist <= 10.0) {
                    RequestEnemyAction(e, 12);
                } else {
                    RequestEnemyAction(e, 16);
                }
            } else if (act != 1) {
                e->flag2 -= gFrameTicks;
                update_vel(e, 0.5f);
            }
action_done31:
            ;
        }
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* --- kill_enemy externs --- */
extern int sprintf(char* buf, const char* fmt, ...);
extern int toupper(int c);
extern char* fn_80057ACC(s32 slot);                 /* current-level tag string */
extern struct item* PlaceItem(s32 a, s32 b, char* name, s32 c);
extern void fn_800920E0(f32* pos, struct item* ip, f32 z); /* toss carried item */
extern void AddItemSub(struct item* ip);           /* commit placed item */
extern void del_target(f32* worldmat);               /* release camera target */
extern void MBRemoveNode(struct mbnode* n, s32 a);   /* delete scene node */
extern void SfxDeleteParented(struct mbnode* n, s32 a, s32 b);
extern void AtreeDelete(void* atree);               /* free anim tree */
extern s32 gTriggerCameraState;
extern s32 lbl_80344734;      /* node-delete reentry guard */
extern s32 ErrorPrintf(const char* fmt, ...);
extern char lbl_80112468[];

extern s32 heal_player(EnemyPlayerView* player, f32 amount);
extern void do_heal_players(void* player, f32* matrix, f32 amount);
extern void ModifyDamage(f32 armor, f32* damage, u32* damage_type, u32 shield);
extern void CopyMat4(f32* source, f32* destination);
extern void UpdateObjWorldMat(f32* matrix);
extern void fn_8005A404(f32* matrix, f32* coll_offset, f32* attn_offset);
extern void SetEnemyObj();
extern void AudioPlayEvt101(f32* position);
extern void AudioPlayEvt103(f32* position);
extern void fn_8009DD48(void);
extern void fn_8009DE5C(s32 type, f32* position);
extern void fn_8009DE88(Enemy* enemy, s32 mode);
extern void fn_8009DF7C(Enemy* enemy, s32 mode);
extern void fn_800945D0(u8* position, u8* matrix, s32 damage_type, s32 alt,
                        u32 type, f32 scale);
extern void MBTreeSetAmbientAdd(struct mbnode* node, s32 value, s32 recurse);
extern void SetSkinFX(skinfx* fx, s32 base, s32 frames, s32 loops, f32 rate);
extern s32 gBossType;
extern s32 lbl_80344768;
extern f32 lbl_803447D8;
extern s32 lbl_80344BE0;
extern s32 lbl_80344BE4;
extern s32 lbl_802897B8[];
extern f32 lbl_8011B900[];
extern f32 lbl_8011BA10[];
extern f64 lbl_80346938;
extern f64 lbl_80346948;
extern f32 lbl_803469B0;
extern f64 lbl_80346A08;
extern f64 lbl_80346A10;
extern f64 lbl_80346A18;
extern f64 lbl_80346A28;
extern f64 lbl_80346A30;

/* Apply damage and accumulated hit direction, then run the enemy-specific
 * heal, reaction, death, sound, skin and burst-effect cascades. */
#pragma dont_inline on
s32 damage_enemy(Enemy* e, f32 amount, s32 player_index, s32 damage_type,
                 s32 effect_position_arg, s32 hit_direction_arg,
                 s32 play_effects)
{
    EnemyPlayerView* player = NULL;
    f32* effect_position = (f32*)effect_position_arg;
    f32* hit_direction = (f32*)hit_direction_arg;
    f32 old_health = e->health;
    u8 unused0[4];
    f32 effect_pos[3];
    u8 unused1[4];
    f32 saved_matrix[16];
    f32 fight;
    s32 enemy_index;

    if (e->state == DECORATION) {
        return -1;
    }
    if (e->state == DYING) {
        return -1;
    }
    if (e->type == E_IT) {
        return -1;
    }

    if (player_index >= 0) {
        player = &gEnemyPlayers[player_index];
    }
    if (player != NULL) {
        lbl_803447E4 = 1;
    }

    if (e->type == E_DEATH) {
        if ((damage_type & 0x200) != 0) {
            if (player != NULL && player->level > 75) {
                f32 heal_scale = (f32)(lbl_80346A10 *
                    (f64)(player->level - 75) + lbl_80346A08);
                heal_player(player, e->health * heal_scale);
            }
            e->health = lbl_80346820;
        } else if (e->state == SLEEP) {
            if (play_effects != 0) {
                fn_8009DE5C(e->type, &e->objgrp.worldmat[3][0]);
            }
            {
                s16 endurance = e->endurance - 1;
                e->endurance = endurance;
                if (endurance <= 0) {
                    e->state = ACTIVE;
                    if (play_effects != 0) {
                        AudioPlayEvt103(&e->objgrp.worldmat[3][0]);
                    }
                    CopyMat4(&e->objgrp.worldmat[0][0], saved_matrix);
                    SetEnemyObj((u8*)e, e->type, 1);
                    CopyMat4(saved_matrix, &e->objgrp.worldmat[0][0]);
                    UpdateObjWorldMat(&e->objgrp.worldmat[0][0]);
                    fn_8005A404(&e->objgrp.worldmat[0][0], e->coll_offset,
                                 e->attn_offset);
                    MBTreeClearFlags(e->objgrp.node, 2, 0);
                }
            }
            return 0;
        } else if (player != NULL && (player->flags & 0x80000) != 0) {
            f64 one;

            e->health = (f32)(e->health - (one = lbl_80346810));
            if (e->org_lvl == 2) {
                AddExp(player_index, 1, -2);
            } else {
                player->health = (f32)(player->health + one);
            }
        } else {
            e->health = (f32)(e->health - lbl_80346810);
            if (player != NULL) {
                msgPost(0, player_index, player->position);
            }
        }

        if ((f64)e->health <= (f64)lbl_80346820) {
            if (play_effects != 0) {
                AudioPlayEvt101(&e->objgrp.worldmat[3][0]);
            }
            e->health = lbl_80346820;
            enemy_index = (s32)(e - gEnemies);
            e->state = DYING;
            e->area = (s16)player_index;
            if (e->algorithm == 18) {
                SuicideExplosion(e->objgrp.coll_pos,
                    (f32)(lbl_803468A8 * gCurLevel->ene_damage));
                fn_8009DAC8(e->objgrp.coll_pos);
            }
            uncouple_enemy(enemy_index);
            if (player != NULL) {
                player->character_stats[player->character].kills++;
            }
            return 1;
        }
        return 0;
    }

    e->watchdog = 0;
    if ((damage_type & 0x800000) != 0 && player != NULL) {
        f32 healed = amount;
        if (healed > e->health) {
            healed = e->health;
        }
        do_heal_players(player, &e->objgrp.worldmat[0][0], healed);
    }

    if (player_index >= 0 && gCurLevel->plevel > lbl_80346820) {
        f32 level = (f32)player->level;
        f32 target_level = gCurLevel->plevel;
        f32 scale = lbl_803468F0;

        if (level < target_level) {
            scale = (f32)(lbl_80346810 -
                          lbl_80346878 * (target_level - level));
        } else if (level > target_level) {
            scale = (f32)(lbl_80346810 +
                          lbl_80346858 * (level - target_level));
        }
        amount *= scale;
    }

    {
        u32 shield = e->atts.armortype;
        ModifyDamage(e->atts.armor, &amount, (u32*)&damage_type, shield);
    }
    if ((f64)lbl_803447D8 < lbl_80346810) {
        amount = (f32)(amount * lbl_80346868);
    }
    if (player_index >= 0 &&
        (f64)amount < *(volatile f64*)&lbl_80346810) {
        amount = lbl_80346820;
    }

    e->damage += amount;
    if ((damage_type & 0xF) != 0) {
        e->damagetype &= ~0xF;
    }
    e->damagetype |= damage_type;
    if (hit_direction != NULL) {
        e->damagedir[0] = hit_direction[0] + e->damagedir[0];
        e->damagedir[1] = hit_direction[1] + e->damagedir[1];
        e->damagedir[2] = hit_direction[2] + e->damagedir[2];
    }

    effect_pos[0] = e->objgrp.attn_pos[0];
    effect_pos[1] = e->objgrp.attn_pos[1];
    effect_pos[2] = e->objgrp.attn_pos[2];

    if (e->type == E_GOLEM) {
        if (lbl_80344768 >= 3) {
            amount = (f32)(amount * lbl_80346A18);
        } else if (lbl_80344768 >= 2) {
            amount = (f32)(amount * lbl_80346830);
        }
    }
    if (amount <= lbl_80346820) {
        play_effects = 0;
    } else {
        e->damage_count++;
    }

    {
        f64 applied;

        if (gGameOptions[0] == 3) {
            applied = lbl_80346A20;
        } else {
            applied = amount;
        }
        e->health = (f32)((f64)e->health - applied);
    }

    fight = gCurLevel->ene_damage * lbl_8011B900[e->type];
    {
        f32 threshold = gCurLevel->ene_health * lbl_8011BA10[e->type];
        f32 upper = (f32)(lbl_80346A30 * threshold);
        f32 lower = (f32)(lbl_80346A28 * threshold);

        if (e->health > upper) {
            goto store_fight;
        }
        if (e->type == E_DEATH) {
            goto store_fight;
        }
        if (e->health > lower) {
            fight = (f32)(lbl_80346A30 * fight);
        } else {
            fight = (f32)(lbl_80346A28 * fight);
        }
    }
store_fight:
    e->atts.fight = fight;

    if (e->algorithm == 12 && e->mode1 < 2 && e->generator != NULL) {
        ((u8*)e->generator)[0xE6] = 7;
        ((u8*)e->generator)[0xE0] = 3;
    } else if (e->algorithm == 15 && e->generator != NULL) {
        ((u8*)e->generator)[0xE3] = 0;
    }

    if ((f64)e->health <= lbl_80346898) {
        if (e->type == gBossType) {
            if ((f64)old_health > lbl_80346898 && player != NULL) {
                player->character_stats[player->character].kills++;
            }
            return 1;
        }

        if (play_effects != 0) {
            if (e->algorithm == 18) {
                fn_8009DD48();
            }
            fn_8009DF7C(e, play_effects);
        }
        e->health = lbl_80346820;
        enemy_index = (s32)(e - gEnemies);
        e->state = DYING;
        e->area = (s16)player_index;
        if (e->algorithm == 18) {
            SuicideExplosion(e->objgrp.coll_pos,
                (f32)(lbl_803468A8 * gCurLevel->ene_damage));
            fn_8009DAC8(e->objgrp.coll_pos);
        }
        uncouple_enemy(enemy_index);
        if (player != NULL) {
            player->character_stats[player->character].kills++;
        }

        if (player_index >= -1) {
            if (e->objgrp.node != NULL) {
                if (e->type == E_GOLEM && (damage_type & 0xF) == 0) {
                    SetSkinFX(&e->skinfx, lbl_80344BE4, 15, 0,
                              lbl_803469B0);
                } else if (e->type == E_TREEFOLK &&
                           (damage_type & 0xF) == 0) {
                    SetSkinFX(&e->skinfx, lbl_80344BE0, 10, 0,
                              lbl_803469B0);
                } else if (e->type == E_KNIGHT &&
                           (damage_type & 0xF) == 0) {
                    SetSkinFX(&e->skinfx, lbl_80344BE0, 10, 0,
                              lbl_803469B0);
                } else if ((f64)e->hht > lbl_80346868) {
                    SetSkinFX(&e->skinfx,
                              lbl_802897B8[damage_type & 0xF], 10, 0,
                              lbl_803469B0);
                }
                MBTreeSetAmbientAdd(e->objgrp.node, 999, 1);
            }
            if ((damage_type & 0x1000000) == 0 && e->type != gBossType) {
                fn_800945D0((u8*)effect_pos, (u8*)&e->objgrp,
                            damage_type, 1, e->type, e->hht);
            }
        }
        return 1;
    }

    if (play_effects != 0) {
        fn_8009DE88(e, play_effects);
    }
    if ((damage_type & 0x1000000) == 0 && e->type != gBossType) {
        if (effect_position != NULL && (f64)e->hht >= lbl_80346948) {
            effect_pos[0] = *(f32*)((u8*)effect_position + 0);
            effect_pos[1] = *(f32*)((u8*)effect_position + 4);
            effect_pos[2] = *(f32*)((u8*)effect_position + 8);
        }
        fn_800945D0((u8*)effect_pos, (u8*)&e->objgrp,
                    damage_type, 0, e->type, e->hht);
    }
    return 0;
}
#pragma dont_inline off

/* kill_enemy @0x8004EFE4.  Drop the carried item (or place a "GARG<level>"
 * egg for gargoyles), then tear the enemy down: health/state clear, grid
 * release, shadow + special fx + scene node deletion, generator uncouple. */
extern char lbl_80346A38[7]; /* "GARG%s" (sdata2) */

void kill_enemy(s32 index)
{
    Enemy* e = &gEnemies[index];
    struct item* item = 0;
    s32 carried = 0;
    char* p;
    char buf[32];

    if (gTriggerCameraState != 0) {
        return;
    }
    if (e->gotitem != 0) {
        item = e->gotitem;
        e->gotitem = 0;
        carried = 1;
    } else {
        switch (e->type) {
        case E_GARGOYLE:
            sprintf(buf, lbl_80346A38, fn_80057ACC(e->type));
            for (p = buf; *p != 0; p++) {
                *p = toupper(*p);
            }
            item = PlaceItem(1, 16, buf, 0);
            break;
        }
    }
    if (item != 0) {
        if (carried != 0) {
            *((u8*)item + 205) = 10;
            fn_800920E0(e->objgrp.attn_pos, item, lbl_80346820);
        } else {
            *((u8*)item + 205) = 0;
            MBTreeClearFlags(*(struct mbnode**)((u8*)item + 100), 2, 0);
            if (**(s32**)item == 1) {
                *(s16*)((u8*)item + 236) = 60;
            }
            *(f32*)((u8*)item + 52) = e->objgrp.worldmat[3][0];
            *(f32*)((u8*)item + 56) = e->objgrp.worldmat[3][1];
            *(f32*)((u8*)item + 60) = e->objgrp.worldmat[3][2];
            AddItemSub(item);
        }
    }
    if (e->type == 27) {
        fn_8004F1DC(e);
    }
    e->health = lbl_80346820;
    e->state = 0;
    del_target(&e->objgrp.worldmat[0][0]);
    if (e->shadow != 0) {
        MBRemoveNode(e->shadow, 0);
        e->shadow = 0;
    }
    if (e->specialfx >= 0) {
        e->specialfx = DeleteEffect(e->specialfx, 1);
    }
    if (e->type == 27) {
        SfxDeleteParented(e->objgrp.node, 0, -1);
    }
    AtreeDelete(&e->atree);
    lbl_80344734 = 1;
    MBRemoveNode(e->objgrp.node, 0);
    lbl_80344734 = 0;
    e->objgrp.node = 0;
    uncouple_enemy(index);
}

extern s32 lbl_80344724;   /* 0x80344724 active milestone count */
/* find_neighbor_milestone @0x8004C9DC -- locate milestone id `ms` in the active
 * milestone list, then return the id `nth` slots away (reflecting at the ends),
 * preferring whichever candidate sits closer to the world origin. */
s32 find_neighbor_milestone(s32 ms, s32 nth)
{
    u32* scanWord;
    EnemyRuntimeOwner* milestoneOwner = (EnemyRuntimeOwner*)lbl_80250E00;
    s32 count = lbl_80344724;
    s32 idx = 0;
    s32 lo;
    s32 hi;
    s32 i;
    u8 unused[24];

    for (i = 0; i < count; i++) {
        if (ms == *(s32*)((u8*)(scanWord = &milestoneOwner->words[i]) + 0xF4)) {
            break;
        }
        idx++;
    }
    if (idx >= count) {
        return -1;
    }
    lo = idx - nth;
    if (lo < 0) {
        return milestoneOwner->view.pool.view.milestoneIds[idx + nth];
    }
    hi = idx + nth;
    if (hi > count - 1) {
        return milestoneOwner->view.pool.view.milestoneIds[lo];
    }
    {
        u8* milestoneBase;
        u8* milestoneY;
        u8* milestoneX;
        u8* milestoneZ;
        s32 milestoneOffset;
        s32 m_lo;
        s32 m_hi;
        f32 x;
        f32 y;
        f32 z;
        f32 dlo;
        f32 dhi;

        m_lo = milestoneOwner->view.pool.view.milestoneIds[lo];
        milestoneBase = sMilestones;
        milestoneOffset = m_lo * 0x68;
        milestoneY = milestoneBase + 0x34;
        milestoneX = milestoneBase + 0x30;
        milestoneZ = milestoneBase + 0x38;
        y = *(f32*)(milestoneY + milestoneOffset);
        x = *(f32*)(milestoneX + milestoneOffset);
        z = *(f32*)(milestoneZ + milestoneOffset);
        dlo = y * y;
        dlo = x * x + dlo;
        dlo = z * z + dlo;

        if (dlo > 0.0f) {
            volatile f32 tmp;
            f64 y = __frsqrte(dlo);
            y = 0.5 * y * (3.0 - y * y * dlo);
            y = 0.5 * y * (3.0 - y * y * dlo);
            y = 0.5 * y * (3.0 - y * y * dlo);
            tmp = (f32)(dlo * (0.5 * y * (3.0 - y * y * dlo)));
            dlo = tmp;
        }
        m_hi = milestoneOwner->view.pool.view.milestoneIds[hi];
        milestoneOffset = m_hi * 0x68;
        y = *(f32*)(milestoneY + milestoneOffset);
        x = *(f32*)(milestoneX + milestoneOffset);
        z = *(f32*)(milestoneZ + milestoneOffset);
        dhi = y * y;
        dhi = x * x + dhi;
        dhi = z * z + dhi;
        if (dhi > 0.0f) {
            volatile f32 tmp;
            f64 y = __frsqrte(dhi);
            y = 0.5 * y * (3.0 - y * y * dhi);
            y = 0.5 * y * (3.0 - y * y * dhi);
            y = 0.5 * y * (3.0 - y * y * dhi);
            tmp = (f32)(dhi * (0.5 * y * (3.0 - y * y * dhi)));
            dhi = tmp;
        }
        if (dlo < dhi) {
            return m_lo;
        }
        return m_hi;
    }
}

/* turn_enemy_ang @0x8004CBB8 (14 callers: the move_logic set).  Rotate the
 * enemy's yaw toward `want` at the per-type turn rate (lbl_8011BED8 table,
 * 3x rate while running/reacting), clamping to `want` when within one step,
 * and normalize the result to (-pi, pi]. */
f32 turn_enemy_ang(Enemy* e, f32 want)
{
    s32 act = e->action;
    s32 hit = 0;
    s32 dir;
    f32 d;
    f32 d32;
    f32 cur;
    f32 rate;
    f32 step;
    f64 nd;
    f64 r;

    if (act >= 28) {
        return e->pyr[1];
    }
    cur = e->pyr[1];
    rate = lbl_8011BED8[e->type];
    if (act == 1) {
        return cur;
    }
    if (act == 4 || act == 28 || act == 29) {
        rate *= 3.0;
    }
    d = want - cur;
    if (d > 3.141592654) {
        nd = d - 6.283185308;
    } else if (d <= -3.141592654) {
        nd = 6.283185308 + d;
    } else {
        nd = d;
    }
    d32 = (f32)nd;
    step = rate * (f32)(u32)gFrameTicks;
    if (d32 >= 0.0f) {
        if (d32 <= step) {
            hit = 1;
        }
        dir = 1;
    } else {
        if (d32 >= -step) {
            hit = 1;
        }
        dir = -1;
    }
    switch (hit) {
    case 0:
        if (dir > 0) {
            want = cur + step;
        } else {
            want = cur - step;
        }
        break;
    }
    r = want;
    if (r > 3.141592654) {
        r -= 6.283185308;
    } else if (r <= -3.141592654) {
        r = 6.283185308 + r;
    }
    return r;
}

/* --- generate_enemy externs --- */
extern s32 RandInt(s32 n);
extern s32 check_enemy_pos(f32* start, f32* out, s32 slot);
extern void init_enemy(s32 slot, f32* pos, s32 type, s32 level, s32 spew);
extern void UpdateObjWorldMat(f32* worldmat);              /* claim grid cell */
extern void fn_8005A404(f32* worldmat, f32* coll_offset, f32* attn_offset);
extern s32 InitAnim(f32 time, animinfo* info, s32 seq, s32 frame, s32 active);
extern void StartGenFX(f32* pos, s32 level);
extern s32 gBossType;
extern s32 gBossDying;
extern s32 gGameMode;      /* current map/world id */
extern s32 lbl_803447DC;      /* generators-disabled flag */
extern s32 lbl_8034472C;      /* random-type rotation counter */
extern u8 lbl_8011AF48[];     /* enemy.c .data anchor (type tables at +4284..) */
extern u32 jumptable_8011C25C[];

/* Accelerate an enemy along an angle, caching the trig pair between calls. */
void set_enemy_trans(Enemy* enemy, f32 speed, f32 angle)
{
    if (enemy->type != gBossType && enemy->action != 1) {
        s32 action;

        if (speed >= lbl_803469B8) {
            action = 4;
        } else {
            action = 3;
        }
        RequestEnemyAction(enemy, action);

        if (enemy->action == 3 || enemy->action == 4 ||
            (u32)(enemy->action - 22) <= 1 || enemy->coll_pnum >= 0) {
            f32 typeSpeed;
            f32 dx;
            f32 dz;

            if (enemy->prev_dir != angle) {
                enemy->xspd = sin(angle);
                enemy->zspd = cos(angle);
                enemy->prev_dir = angle;
            }
            typeSpeed = ((f32*)lbl_80250E40.words)[enemy->type];
            dx = speed * (enemy->xspd * typeSpeed);
            dz = speed * (enemy->zspd * typeSpeed);
            enemy->trans[0] += dx;
            enemy->trans[2] += dz;
        }
    }
}

/* Resolve the generator/spew class shared by groups of enemy types. */
s32 fn_8004F87C(s32 type, s32 level, s32 spew)
{
    switch (type) {
    case 0:
    case 3:
    case 6:
    case 9:
    case 12:
    case 15:
    case 18:
    case 21:
    case 22:
        if (spew != 2 && spew != 4) {
            spew = RandInt(1) != 0 ? 4 : 2;
        }
        break;
    case 2:
    case 7:
    case 14:
    case 17:
    case 24:
        if (spew == 0) {
            switch (level) {
            default:
                spew = 7;
                break;
            case 3:
                spew = 30;
                break;
            case 4:
                spew = 23;
                break;
            case 5:
                spew = 17;
                break;
            case 6:
                spew = 18;
                break;
            }
        }
        break;
    case 1:
    case 4:
    case 5:
    case 8:
    case 10:
    case 11:
    case 13:
    case 16:
    case 19:
    case 20:
    case 25:
    case 32:
    case 33:
        if (spew == 0) {
            switch (level) {
            default:
                spew = 7;
                break;
            case 4:
                spew = 23;
                break;
            case 5:
                spew = 17;
                break;
            case 6:
                spew = 18;
                break;
            }
        }
        break;
    case 27:
        spew = 31;
        break;
    case 30:
        spew = 3;
        break;
    case 31:
        spew = 27;
        break;
    case 29:
        spew = 19;
        break;
    }
    return spew;
}

/* Rotate a horizontal direction into one of the eight generator octants. */
static f32 gendir_8004FBC8(f32* input, f32* output, s32 direction)
{
    f32 x = input[0];
    f32 z = input[2];
    f64 term;

    output[1] = input[1];
    switch (direction) {
    default:
        output[0] = x;
        output[2] = z;
        return lbl_80346820;
    case 1:
        output[0] = -x;
        output[2] = -z;
        return lbl_80346A4C;
    case 2:
        output[0] = -z;
        output[2] = x;
        return lbl_80346A50;
    case 3:
        output[0] = z;
        output[2] = -x;
        return lbl_80346A54;
    case 4:
        term = lbl_80346A58 * z;
        output[0] = lbl_80346A58 * x + term;
        output[2] = lbl_80346A58 * -x + term;
        return lbl_80346A60;
    case 5:
        term = lbl_80346A58 * x;
        output[0] = lbl_80346A58 * -z + term;
        output[2] = lbl_80346A58 * z + term;
        return lbl_80346A64;
    case 6:
        term = lbl_80346A58 * -x;
        output[0] = lbl_80346A58 * z + term;
        output[2] = lbl_80346A58 * -z + term;
        return lbl_80346A68;
    case 7:
        term = lbl_80346A58 * -z;
        output[0] = lbl_80346A58 * -x + term;
        output[2] = lbl_80346A58 * x + term;
        return lbl_80346A6C;
    }
}

/* Point the Garm death effect toward its target (or the first active player). */
void fn_8004F1DC(Enemy* enemy)
{
    volatile f32 enemyPos[3];
    f32 matrix[12];
    f32 direction[3];
    s32 i;
    f32* player = 0;

    if (enemy->closest >= 0) {
        player = gPlayerWords[enemy->closest];
    } else {
        for (i = 0; i < 4; i++) {
            if (((s32*)gPlayerWords[i])[0xE8 / 4] == 1) {
                break;
            }
        }
        if (i < 4) {
            player = gPlayerWords[i];
        }
    }

    if (player != 0) {
        enemyPos[0] = enemy->objgrp.worldmat[3][0];
        enemyPos[1] = enemy->objgrp.worldmat[3][1];
        enemyPos[2] = enemy->objgrp.worldmat[3][2];
        direction[0] = player[17] - enemyPos[0];
        direction[1] = player[18] - enemyPos[1];
        direction[2] = player[19] - enemyPos[2];
        NormalVector(direction);
        CreateDirMatrix(matrix, direction, 0);
        StartEnemyDeathFX(matrix);
    }
}

/* generate_enemy @0x8004F4B4 (global).  Spawn an enemy of `type` at `pos`:
 * validate world/boss state and per-type limits, resolve random types
 * (-2/-3), take a slot, then for generator spawns search the 8 (or 2)
 * directions around the generator for a free position; finish by claiming
 * the grid cell, starting the E_START anim and the generator fx. */
s32 generate_enemy(f32* pos, s32 type, s32 level, f32* dir, s32 spew,
                   struct item* gen, s32 imp, f32 ang)
{
    u8* tbl = lbl_8011AF48;
    s32 otype;
    s32 mask = 0;
    s32 slot;
    s32 ndirs;
    s32 start;
    s32 d;
    s32 i;
    s32 r;
    Enemy* e;
    f32 v[3];
    f32 out[3];
    f32 startv[3];

    if (gGameMode == 0x8007) {
        return -1;
    }
    if (lbl_803447DC != 0 && gen != 0) {
        return -4;
    }
    if (gBossType >= 0 && gBossDying != 0) {
        return -4;
    }
    spew = fn_8004F87C(type, level, spew);
    otype = type;
    if (type == -2) {
        RandInt(4);
        level = 2;
        i = lbl_8034472C;
        lbl_8034472C = i + 1;
        type = *(s32*)(tbl + ((i & 3) << 2) + 4284);
        spew = *(s32*)(tbl + ((i & 3) << 2) + 4300);
    } else if (type == -3) {
        RandInt(4);
        level = 3;
        i = lbl_8034472C;
        lbl_8034472C = i + 1;
        type = *(s32*)(tbl + ((i & 3) << 2) + 4316);
        spew = *(s32*)(tbl + ((i & 3) << 2) + 4332);
    } else if (type < 0) {
        return -6;
    }
    if (type != 30 && type != 31) {
        if (lbl_802512B0[type] < 0) {
            return -5;
        }
        if (lbl_802511FC[type] == 4 && level < 4) {
            return -5;
        }
    }
    slot = find_enemy_slot(type, imp);
    if (slot < 0) {
        return -2;
    }
    init_enemy(slot, pos, type, level, spew);
    gEnemies[slot].generator = gen;
    e = &gEnemies[slot];
    if (gen == 0 || type == 30) {
        e->genang_offset = lbl_80346820;
    } else {
        switch (otype) {
        case 1:
        case 4:
        case 5:
        case 7:
        case 8:
        case 10:
        case 11:
        case 14:
        case 15:
        case 19:
        case 24:
        case 25:
            mask = 0xFFCE;
            ndirs = 8;
            break;
        case 18:
            ndirs = 2;
            break;
        case 0:
        case 2:
        case 3:
        case 6:
        case 12:
        case 13:
        case 16:
        case 17:
        case 20:
        case 21:
        case 22:
        case 23:
            ndirs = 8;
            break;
        default:
            ndirs = 8;
            break;
        }
        if (spew == 12) {
            ndirs = 1;
        }
        startv[0] = pos[0];
        startv[1] = pos[1];
        startv[2] = pos[2];
        startv[1] = startv[1] + e->coll_offset[1];
        switch (type) {
        case 17:
            ang = 0.0f;
            break;
        default:
            ang = ang + e->rad;
            break;
        }
        v[0] = dir[0] * ang;
        v[1] = 0.0f;
        v[2] = dir[2] * ang;
        start = RandInt(ndirs);
        d = start;
        do {
            if ((mask & 0xFFFF & (1 << d)) == 0) {
                e->genang_offset = gendir_8004FBC8(v, out, d);
                r = check_enemy_pos(startv, out, slot);
                if (r > 0) {
                    goto placed;
                }
                if (r < 0) {
                    mask |= 1 << d;
                }
            }
            d = (d + 1) % ndirs;
        } while (d != start);
        MBTreeSetFlags(e->objgrp.node, 2, 0);
        if (e->shadow != 0) {
            MBTreeSetFlags(e->shadow, 2, 0);
        }
        e->state = 0;
        return -3;
    }
placed:
    UpdateObjWorldMat(&e->objgrp.worldmat[0][0]);
    fn_8005A404(&e->objgrp.worldmat[0][0], e->coll_offset, e->attn_offset);
    e->action = 1;
    if (e->actionlist[1].animidx >= 0) {
        InitAnim(lbl_80346820, &e->atree.animinfo,
                 e->actionlist[1].animidx, 0, 1);
    }
    if (e->hht > 2.0 && level <= 3 && lbl_80251148[type] != 0) {
        StartGenFX(pos, level);
    }
    return slot;
}

typedef struct EnemyGeneratorInfo {
    s32 type;
} EnemyGeneratorInfo;

typedef struct EnemyGenerator {
    EnemyGeneratorInfo* info;
    u8 _pad004[0xDA];
    s8 live_count;
    u8 _pad0DF[2];
    s8 first_enemy;
    u8 _pad0E2;
    s8 flag_e3;
} EnemyGenerator;

/* uncouple_enemy: detach enemy `index` from its generator's spawn list.
 * The prev_enemy/next_enemy relink below is transcribed from the verified GC
 * asm (uncouple_enemy @0x8004F2D8) and exercises the reconstructed Enemy
 * fields; the generator-record fixup (item*) is left as a comment because the
 * item struct belongs to another module.  NonMatching: shipped bytes come from
 * the original DOL. */
void uncouple_enemy(s32 index) {
    Enemy* e = &gEnemies[index];

    if (e->prev_enemy >= 0) {
        gEnemies[e->prev_enemy].next_enemy = e->next_enemy;
        e->prev_enemy = -1;
    }
    if (e->next_enemy >= 0) {
        gEnemies[e->next_enemy].prev_enemy = e->prev_enemy;
        e->next_enemy = -1;
    }
    if (e->algorithm == E_DOG) {
        lbl_80344748 = -1;
        e->algorithm = -e->algorithm;
    }
    if (e->generator != 0) {
        if (((EnemyGenerator*)e->generator)->first_enemy == index) {
            ((EnemyGenerator*)e->generator)->first_enemy =
                (s8)e->prev_enemy;
        }
        if (e->algorithm == 15) {
            ((EnemyGenerator*)e->generator)->flag_e3 = 0;
            ((EnemyGenerator*)e->generator)->live_count = 0;
        }
        if (((EnemyGenerator*)e->generator)->info->type == 3) {
            if (((EnemyGenerator*)e->generator)->live_count > 0) {
                ((EnemyGenerator*)e->generator)->live_count--;
            }
        } else {
            ErrorPrintf(lbl_80112468, e->generator);
        }
        e->generator = 0;
    }
}

/* check_enemy_pos @0x8004F9AC -- validate a candidate spawn point for enemy
 * `slot`: optionally offset it, reject wall/floor failures and steep drops,
 * snap Y to the floor, then reject overlaps with world objects or other
 * enemies (unless the overlap is the enemy's own generator).  Returns 1 when
 * the position is usable, 0 when blocked by geometry/occupant, -1 on failure. */
extern void* FloorCollide(f32* pos, s32 a, s32 b, s32 mode, f32 x, f32 y, f32 z);
/* FloorCollisionResultView is declared near do_enemy_collide above. */
extern FloorCollisionResultView gFloorCollisionResult; /* 0x8023CAE0 */
extern f32 lbl_80346A40;
extern f32 lbl_80346A44;
extern f32 lbl_80346A48;
extern f64 lbl_80346988;           /* max vertical snap distance */
extern f64 lbl_80346830;           /* 0.5 */
extern void* fn_8005EFAC(f32 rad, f32* probe, f32* pos, s32 a, s32 b);
extern s32 fn_8005D3D8(s32 a, void* obj);

s32 check_enemy_pos(f32* start, f32* out, s32 slot)
{
    Enemy* e = &gEnemies[slot];
    f32 rad = e->rad;
    f32 hht = e->hht;
    f32 pos[3];
    u8 _ppad[4];
    f64 half;
    void* obj;
    s32 grounded;

    if (out != NULL) {
        pos[0] = out[0] + start[0];
        pos[1] = out[1] + start[1];
        pos[2] = out[2] + start[2];
        if (EnemyWallCollide(rad, start, pos, 0) != 0) {
            return -1;
        }
    } else {
        pos[0] = start[0];
        pos[1] = start[1];
        pos[2] = start[2];
    }
    e->objgrp.worldmat[3][0] = pos[0];
    e->objgrp.worldmat[3][1] = pos[1];
    e->objgrp.worldmat[3][2] = pos[2];
    if (FloorCollide(pos, 0, 0, 2, lbl_80346A40, lbl_80346A44, lbl_80346A48)
        != 0) {
        grounded = 1;
    } else {
        grounded = 0;
    }
    if (grounded == 0) {
        return -1;
    }
    {
        f32 floorY = gFloorCollisionResult.floorY;
        f32 dy = floorY - start[1];
        u8 _dpad[8];

        *(u32*)&dy &= 0x7FFFFFFF;
        if (dy > lbl_80346988) {
            return -1;
        }
        e->objgrp.worldmat[3][1] = floorY;
    }
    fn_8005A65C(&e->objgrp.worldmat[0][0], e->coll_offset);
    if (fn_80046680(rad, hht, slot, 1, start, pos) >= 0) {
        return 0;
    }
    half = lbl_80346830 * rad;
    if (fn_8004646C((f32)half, hht, slot, start, pos, 0, 0) >= 0) {
        return 0;
    }
    obj = fn_8005EFAC((f32)half, start, pos, 0, 0);
    if (obj != NULL && obj != e->generator) {
        if (fn_8005D3D8(-1, obj) != 0) {
            return 0;
        }
    }
    return 1;
}

/* find_enemy_slot: return a free/recyclable enemy slot for a new spawn.
 * Recycles the least-important live enemy through kill_enemy when the array is
 * full; returns -1 when the request cannot be satisfied. */
s32 find_enemy_slot(s32 type, s32 level) {
    s32 enemy_state;
    Enemy* enemy;
    s32 index;
    s32 best_visible;
    s32 visible;
    f64 invis_add;
    f64 dying_mult;
    f32 distance;
    f32 best_distance;
    s32 best_index;
    s32 count;

    best_visible = 1;
    index = 0;
    best_index = 0;
    count = gNumEnemies;
    best_distance = lbl_80346820;
    dying_mult = lbl_80346878;
    invis_add = lbl_80346A20;
    enemy = gEnemies;

    for (index = 0; index < count; index++, enemy++) {
        enemy_state = enemy->state;

        if (enemy_state == INACTIVE) {
            return index;
        }
        if (enemy->type != E_IT) {
            distance = enemy->actual_dist;
            visible = enemy->visactive;

            if (enemy_state == DYING || enemy_state == SLEEP ||
                enemy->birth_style != 0) {
                distance *= dying_mult;
            } else if (visible == 0) {
                distance += invis_add;
            }
            if (distance > best_distance) {
                best_distance = distance;
                best_index = index;
                best_visible = visible;
            }
        }
    }

    default_gen_count = 1;
    if (type < E_NTYPES && level < best_visible) {
        return -1;
    }
    if (best_visible != 0) {
        lbl_80344728++;
    }
    kill_enemy(best_index);
    {
        Enemy* k = (Enemy*)((u8*)gEnemies + best_index * 916);
        f32 reset_distance = lbl_803468F0;

        k->close_dist = reset_distance;
        k->actual_dist = reset_distance;
    }
    return best_index;
}

/* 0x8004C8CC - wall/object clearance probe shared by the move_logic set */
extern void* fn_8005EFAC(f32 rad, f32* probe, f32* pos, s32 a, s32 b);
extern s32 fn_8005D3D8(s32 a, void* obj);
extern f64 lbl_80346858;

s32 fn_8004C8CC(f32* pos, s32 index)
{
    Enemy* e = &gEnemies[index];
    s32 result;
    f32 rad;
    f32 hht;
    f32 probe[3];
    void* obj;

    rad = (f32)(lbl_80346858 + *(f32*)((u8*)e + 568));
    hht = (f32)(lbl_80346858 + *(f32*)((u8*)e + 572));
    probe[0] = *(f32*)((u8*)e + 84);
    probe[1] = *(f32*)((u8*)e + 88);
    result = -1;
    probe[2] = *(f32*)((u8*)e + 92);
    probe[1] = pos[1];
    if (fn_8004646C(rad, hht, index, probe, pos, 0, 0) >= 0) {
        result = 0;
    }
    if (result != 0) {
        obj = fn_8005EFAC(rad, probe, pos, 0, 0);
        if (obj != 0) {
            if (fn_8005D3D8(-1, obj) != 0) {
                result = 0;
            }
        }
    }
    if (result != 0) {
        if (FastWallCollide(probe, pos, 0, 2) != 0) {
            result = 0;
        }
    }
    return result;
}

/* 0x80045FE4 - enemy-vs-world-object damage (type from wobj flag nibble) */
extern u32 WorldObjGetAllFlags(void* wobj);
extern f32 NormalVector2D(f32* v);
extern f32 lbl_80346820;
extern f32 lbl_803468A0;
extern f32 lbl_803468A4;

#pragma dont_inline on
#pragma opt_propagation off
#pragma opt_common_subs off
void EnemyWorldDamage(Enemy* e, void* wobj, f32* oldpos, f32* hitnrm)
{
    u32 flags;
    u32 damageType;
    f32 dir[3];

    flags = WorldObjGetAllFlags(wobj);
    if (*(s32*)e == 27) {
        return;
    }
    if ((flags & 0xF0000) == 0) {
        return;
    }
    if (flags & 0x02000000) {
        if (!(flags & 0x08000000)) {
            return;
        }
    }
    dir[0] = oldpos[0] - hitnrm[0];
    dir[1] = lbl_80346820;
    dir[2] = oldpos[2] - hitnrm[2];
    NormalVector2D(dir);
    damageType = flags & 0xF0000;
    switch (damageType) {
    case 0x10000:
        damage_enemy(e, lbl_803468A0, -1, 0, (s32)hitnrm, (s32)dir, 1);
        break;
    case 0x20000:
        damage_enemy(e, lbl_803468A0, -1, 16, (s32)hitnrm, (s32)dir, 1);
        break;
    case 0x30000:
    case 0x40000:
    case 0x50000:
        damage_enemy(e, lbl_803468A4, -1, 32, (s32)hitnrm, (s32)dir, 1);
        break;
    case 0x60000:
        break;
    }
}
#pragma opt_propagation reset
#pragma opt_common_subs reset
#pragma dont_inline off

/* 0x8004CE38 - pick turn direction: which of +/-step headings nears the player */
extern f64 lbl_80346920;        /* turn step */
extern f64 lbl_80346840;        /* wrap high */
extern f64 lbl_80346848;        /* full circle */
extern f64 lbl_80346850;        /* wrap low */

s32 fn_8004CE38(Enemy* e)
{
    u8* p;
    f32 dx;
    f32 dz;
    f32 a;
    f32 ang;
    f64 t;
    f32 x1;
    f32 z1;
    f32 x2;
    f32 z2;

    p = (u8*)&gPlayerWords[*(s16*)((u8*)e + 628)];
    if (*(s16*)(p + 2588) > 2) {
        dx = *(f32*)((u8*)e + 52) - *(f32*)(p + 2532);
        dz = *(f32*)((u8*)e + 60) - *(f32*)(p + 2540);
    } else {
        dx = *(f32*)((u8*)e + 52) - *(f32*)(p + 68);
        dz = *(f32*)((u8*)e + 60) - *(f32*)(p + 76);
    }
    a = (f32)(lbl_80346920 + *(f32*)((u32)e + 580));
    if (a > lbl_80346840) {
        t = a - lbl_80346848;
    } else if (a <= lbl_80346850) {
        t = lbl_80346848 + a;
    } else {
        t = a;
    }
    ang = (f32)t;
    x1 = dx + sin(ang);
    z1 = dz + cos(ang);
    a = (f32)(*(f32*)((u8*)e + 580) - lbl_80346920);
    if (a > lbl_80346840) {
        t = a - lbl_80346848;
    } else if (a <= lbl_80346850) {
        t = lbl_80346848 + a;
    } else {
        t = a;
    }
    ang = (f32)t;
    x2 = dx + sin(ang);
    z2 = dz + cos(ang);
    if (x2 * x2 + z2 * z2 <= x1 * x1 + z1 * z1) {
        return -1;
    }
    return 1;
}

/* do_enemies @0x8004D078 -- per-frame master enemy loop.  Runs the critter
 * list, then (when not paused) sweeps every enemy: a scripted-camera fast path,
 * otherwise a player-aggro reset, an aggro-decay ramp, a milestone/aim probe,
 * a two-radius visibility cull, and the main per-enemy state machine (alive /
 * stunned / dying) with knockback damping and boss fade-out.  Draws the live
 * enemy count when the debug flags are set. */
extern void ProcessCritterList(void);
extern s32 gScriptedCameraState;
s32 fn_8004D958(s32 index);
extern s32* DrawTextKeepScale(f32 scale, s32 x, s32 y, s32 font, s32 color, char* txt);
extern s32 MBWorldSphereVisible3(f32* center, f32 radius);
extern void fn_800516F8(s32 index);
extern void fn_8009FEFC(s16 sound);
extern void fn_8009FEA0(s16 sound);
extern void AudioPlayEvt102(void);
extern void ProcessSkinFX(f32* a, void* node, s32 c);
extern void fn_8005A338(f32* mat, f32* colloff, f32* attnoff);
extern void fn_8004DF58(Enemy* e);
extern f32 atan2(f32 y, f32 x);
extern void SetSkinFX(skinfx* fx, s32 base, s32 frames, s32 loops, f32 rate);
extern s32 gBoss398;
extern char gTextFormatBuf[];
extern s32 lbl_803447B8;
extern s32 lbl_80344718;
extern s32 lbl_80344740;
extern f32 lbl_803447D8;
extern s32 lbl_803447E4;
extern f32 lbl_8011B878[];        /* per-type aggro-decay ramp (0x88) */
extern f32 lbl_80346980;
extern f64 lbl_80346928;
extern f32 lbl_803468F0;
extern f64 lbl_80346810;
extern f64 lbl_80346818;
extern f64 lbl_80346940;
extern f64 lbl_80346878;
extern f32 lbl_803469C0;
extern char lbl_803469C4[3];
extern f32 lbl_803469C8;
extern f32 lbl_803469CC;
extern f32 lbl_803469D0;
extern f32 lbl_803469D4;
extern const f32 lbl_803469D8;
extern f64 lbl_803469E0;
extern f64 lbl_803469E8;
extern s32 lbl_80344BF8;
extern f64 lbl_80346830;
extern f64 lbl_80346838;
extern f64 lbl_80346868;
extern f64 lbl_803469F0;
extern s32 heal_player(EnemyPlayerView* player, f32 amount);
extern void StartGemFX(f32* position, s32 kind);
extern void fn_8009E08C(Enemy* enemy);
extern void AudioPlayerHit(s32 player, s32 kind);

void fn_8004DC2C(Enemy* enemy)
{
    f32 scale;
    f64 limit;
    u32 damageType;
    s32 type;
    s32 typeCopy;
    f64 angle;
    u8 unused[8];

    typeCopy = enemy->type;
    type = typeCopy;
    if (type == gBossType) {
        return;
    }
    if ((f64)enemy->damage >= lbl_80346810) {
        damageType = enemy->damagetype;
        if ((damageType & 0x10160) != 0 ||
            ((f64)enemy->damage > (f64)lbl_803469CC &&
             (damageType & 0x200) != 0)) {
            RequestEnemyAction(enemy, E_HIT_REACT2);
            if (enemy->type == E_GOLEM) {
                enemy->flag1 = 1;
            }
            if (enemy->type == E_ACID) {
                scale = lbl_80346820;
            } else if ((f64)enemy->hht <= lbl_80346868 && enemy->type != E_IT) {
                scale = lbl_803469D0;
            } else if (enemy->type == E_GOLEM) {
                scale = lbl_80346980;
            } else {
                scale = lbl_803469D4;
            }
            enemy->pushed[0] += enemy->damagedir[0] * scale;
            enemy->pushed[1] += enemy->damagedir[1] * scale;
            enemy->pushed[2] += enemy->damagedir[2] * scale;
            scale = enemy->damagedir[2];
            angle = atan2(enemy->damagedir[0], scale);
            limit = lbl_80346840;
            enemy->pushang = (f32)(limit + angle);
            angle = enemy->pushang;
            if (angle > limit) {
                angle -= lbl_80346848;
            } else if (angle <= lbl_80346850) {
                angle = lbl_80346848 + angle;
            }
            enemy->pushang = (f32)angle;
            scale = lbl_80346820;
            enemy->damagedir[0] = scale;
            enemy->damagedir[1] = scale;
            enemy->damagedir[2] = scale;
        } else if ((damageType & 0x10) != 0) {
            if (type == E_GOLEM || type == E_ACID) {
                enemy->flag1 = 1;
            } else {
                RequestEnemyAction(enemy, E_HIT_REACT1);
                enemy->pushed[0] += enemy->damagedir[0] * lbl_803469D8;
                enemy->pushed[1] += enemy->damagedir[1] * lbl_803469D8;
                enemy->pushed[2] += enemy->damagedir[2] * lbl_803469D8;
                scale = enemy->damagedir[2];
                angle = atan2(enemy->damagedir[0], scale);
                limit = lbl_80346840;
                enemy->pushang = (f32)(limit + angle);
                angle = enemy->pushang;
                if (angle > limit) {
                    angle -= lbl_80346848;
                } else if (angle <= lbl_80346850) {
                    angle = lbl_80346848 + angle;
                }
                enemy->pushang = (f32)angle;
                scale = lbl_80346820;
                enemy->damagedir[0] = scale;
                enemy->damagedir[1] = scale;
                enemy->damagedir[2] = scale;
            }
        } else {
            if (type != E_GOLEM) {
                RequestEnemyAction(enemy, E_HIT_REACT1);
            } else {
                enemy->flag1 = 1;
            }
        }

        if ((f64)(enemy->pushed[0] * enemy->pushed[0] +
                  enemy->pushed[1] * enemy->pushed[1] +
                  enemy->pushed[2] * enemy->pushed[2]) > lbl_803469E0) {
            NormalVector(enemy->pushed);
            limit = lbl_803469E8;
            enemy->pushed[0] = (f32)(limit * enemy->pushed[0]);
            enemy->pushed[1] = (f32)(limit * enemy->pushed[1]);
            enemy->pushed[2] = (f32)(limit * enemy->pushed[2]);
        }
        scale = lbl_80346820;
        enemy->damage = scale;
        enemy->damagetype = 0;
        if (enemy->health <= scale) {
            RequestEnemyAction(enemy, E_DYING);
        } else {
            SetSkinFX(&enemy->skinfx, lbl_80344BF8, 1, 1, lbl_803468F0);
        }
    }
    enemy->pushmag2 = enemy->pushed[0] * enemy->pushed[0] +
                      enemy->pushed[2] * enemy->pushed[2];
}

void fn_8004DF58(Enemy* enemy)
{
    u32 playerFlags;
    EnemyPlayerView* player;
    s32 damageMode;
    f32 amount;
    f32 direction[3];
    f32 missilePosition[3];
    f32 healedPosition[3];
    f32 reflectedPosition[3];
    u8 unused[16];
    s32 hitKind;

    (void)unused;
    playerFlags = 0;
    if (((enemy->algorithm == 28 ||
          (u16)(enemy->algorithm - 29) <= 1) &&
         (enemy->attack_flag & 0xF) != 0) ||
        (enemy->algorithm == 31 && (enemy->attack_flag & 2) != 0)) {
        if (enemy->coll_pnum < 0) {
            if (enemy->closest >= 0) {
                missilePosition[0] = gEnemyPlayers[enemy->closest].damage_position[0];
                missilePosition[1] = gEnemyPlayers[enemy->closest].damage_position[1];
                missilePosition[2] = gEnemyPlayers[enemy->closest].damage_position[2];
            } else {
                missilePosition[0] = enemy->objgrp.coll_pos[0];
                missilePosition[1] = enemy->objgrp.coll_pos[1];
                missilePosition[2] = enemy->objgrp.coll_pos[2];
                missilePosition[0] = (f32)(lbl_803469F0 * sin(enemy->pyr[1]) +
                                           missilePosition[0]);
                missilePosition[2] = (f32)(lbl_803469F0 * cos(enemy->pyr[1]) +
                                           missilePosition[2]);
            }
            fn_8004E448(enemy, (s32)missilePosition,
                        enemy->objgrp.coll_pos);
            enemy->attack_flag &= ~0xF;
        }
    }

    if (enemy->attack_index >= 0 && (enemy->attack_flag & 0xF) != 0) {
        player = &gEnemyPlayers[enemy->attack_index];
        if (player->state == 1) {
            damageMode = 1;
            amount = enemy->atts.fight;
            if ((f64)lbl_803447D8 < lbl_80346810) {
                playerFlags |= 0x40000000;
                amount = (f32)(amount * lbl_80346830);
            } else {
                if ((enemy->attack_flag & 2) != 0) {
                    amount = (f32)(amount * lbl_80346838);
                    if ((f64)enemy->hht > lbl_80346868) {
                        playerFlags |= 0x10;
                    }
                }
                if (enemy->type == E_GOLEM) {
                    playerFlags |= 0x20;
                }
                if ((f64)enemy->hht <= lbl_80346868) {
                    playerFlags |= 0x40000000;
                }
            }

            if (player->attack_heal != 0) {
                healedPosition[0] = enemy->objgrp.coll_pos[0];
                healedPosition[1] = enemy->objgrp.coll_pos[1];
                healedPosition[2] = enemy->objgrp.coll_pos[2];
                player->damage_position[0] =
                    healedPosition[0] - healedPosition[0];
                player->damage_position[1] =
                    healedPosition[1] - healedPosition[1];
                player->damage_position[2] =
                    healedPosition[2] - healedPosition[2];
                damage_enemy(enemy, amount, -1, 0x200,
                             (s32)player->damage_position,
                             (s32)healedPosition, 1);
                heal_player(player, amount);
                amount = lbl_80346820;
                playerFlags = 0x40000000;
                StartGemFX(player->position, 1);
            } else if (player->attack_reflect != 0) {
                reflectedPosition[0] = enemy->objgrp.coll_pos[0];
                reflectedPosition[1] = enemy->objgrp.coll_pos[1];
                reflectedPosition[2] = enemy->objgrp.coll_pos[2];
                player->damage_position[0] =
                    reflectedPosition[0] - reflectedPosition[0];
                player->damage_position[1] =
                    reflectedPosition[1] - reflectedPosition[1];
                player->damage_position[2] =
                    reflectedPosition[2] - reflectedPosition[2];
                damage_enemy(enemy, amount, -1, 0,
                             (s32)player->damage_position,
                             (s32)reflectedPosition, 1);
                amount = lbl_80346820;
                playerFlags = 0x40000000;
                StartGemFX(player->position, 1);
            }

            if ((f64)enemy->hht <= lbl_80346868) {
                if (enemy->type != E_IT) {
                    goto generic_hit_sound;
                }
            }
            if (enemy->type == E_DEMON || enemy->type == E_MUMMY ||
                enemy->type == E_TREEFOLK) {
            generic_hit_sound:
                fn_8009E08C(enemy);
                damageMode = 0;
            } else if (enemy->type == E_GRUNT || enemy->type == E_KNIGHT ||
                       enemy->type == E_LIZARDMAN) {
                if (enemy->org_lvl >= 2) {
                    hitKind = 3;
                } else {
                    hitKind = 4;
                }
                AudioPlayerHit(enemy->attack_index, hitKind);
                damageMode = 0;
            }
            if ((playerFlags & 0x130) != 0) {
                direction[0] = player->damage_position[0] -
                               enemy->objgrp.coll_pos[0];
                direction[1] = player->damage_position[1] -
                               enemy->objgrp.coll_pos[1];
                direction[2] = player->damage_position[2] -
                               enemy->objgrp.coll_pos[2];
                direction[1] = -direction[1];
                NormalVector(direction);
                damage_player(enemy->attack_index, amount, damageMode,
                              playerFlags, direction);
            } else {
                damage_player(enemy->attack_index, amount, damageMode,
                              playerFlags, 0);
            }
            lbl_803447E4 = 1;
            enemy->attack_count++;
            enemy->attack_flag &= ~0xF;
        }
        enemy->attack_index = -1;
    }

    if ((enemy->attack_flag & 0x10) != 0) {
        if (enemy->closest >= 0) {
            missilePosition[0] = gEnemyPlayers[enemy->closest].damage_position[0];
            missilePosition[1] = gEnemyPlayers[enemy->closest].damage_position[1];
            missilePosition[2] = gEnemyPlayers[enemy->closest].damage_position[2];
        } else {
            missilePosition[0] = enemy->objgrp.coll_pos[0];
            missilePosition[1] = enemy->objgrp.coll_pos[1];
            missilePosition[2] = enemy->objgrp.coll_pos[2];
            missilePosition[0] = (f32)(lbl_803469F0 * sin(enemy->pyr[1]) +
                                       missilePosition[0]);
            missilePosition[2] = (f32)(lbl_803469F0 * cos(enemy->pyr[1]) +
                                       missilePosition[2]);
        }
        fn_8004E448(enemy, (s32)missilePosition,
                    enemy->objgrp.coll_pos);
        enemy->flag1 = 1;
        enemy->attack_flag &= ~0x10;
    }
}

void do_enemies(void)
{
    u8* pool = (u8*)lbl_80250E00;
    s32 shown = 0;
    s32 i;
    u8 unused[32];

    (void)unused;

    ProcessCritterList();
#pragma opt_propagation off
    if (gBoss398 >= 0) {
        gEnemies[gBoss398].state = ACTIVE;
    }
#pragma reset
    if ((gGameBusy | gGameplayPauseTimer) != 0) {
        return;
    }

    if (gScriptedCameraState != 0) {
        Enemy* e;

        if (lbl_803447B8 == 0) {
            return;
        }
        e = gEnemies;
        for (i = 0; i < gNumEnemies; i++, e++) {
            s32 type;

            if (e->state != ACTIVE) {
                continue;
            }
            type = e->type;
            if (type == gBossType) {
                continue;
            }
            if (type == 0x1D) {
                e->daction = 1;
            } else if (type == 0) {
                e->daction = 3;
            } else {
                e->daction = 0;
            }
            if (e->atree.root != 0) {
                e->action = DoEnemyAction(e);
            }
        }
        return;
    }

    {
        u8* pl = (u8*)gPlayerWords;

        for (i = 0; i < 4; i++, pl += 0x335C) {
            if (((EnemyPlayerView*)pl)->state == 1) {
                *(s32*)(pl + offsetof(EnemyPlayerView, _A22) + 2) = 0;
                *(f32*)(pl + offsetof(EnemyPlayerView, _A22) + 6) = 0.0f;
            }
        }
    }

    {
        EnemyMovePage05* page = (EnemyMovePage05*)pool;
        f32 rate = gCurLevel->ene_speed * (f32)(u32)gFrameTicks;

        lbl_80344718 = 0;
        for (i = 0; i < 45; i++) {
            page->speed[i] = rate * lbl_8011B878[i];
        }
    }

    {
        Enemy* e = gEnemies;

        for (i = 0; i < gNumEnemies; i++, e++) {
            if (e->state == ACTIVE && e->algorithm == 0x12 &&
                e->visactive != 0) {
                if (e->action == 4 || e->daction == 4) {
                    lbl_80344748 = i;
                    break;
                }
            }
        }
    }

    {
        Enemy* e = gEnemies;
        f32 visibilityScale = lbl_80346980;
        f64 visibilityAdd = lbl_80346928;

        lbl_80344740 = 0;
        for (i = 0; i < gNumEnemies; i++, e++) {
            f32 r;

            if (e->state == 0) {
                continue;
            }
            r = visibilityScale * e->rad;
            e->visible =
                (s16)MBWorldSphereVisible3(e->objgrp.attn_pos, r);
            r += visibilityAdd;
            e->visactive =
                (s16)MBWorldSphereVisible3(e->objgrp.attn_pos, r);
            if (e->visible != 0) {
                lbl_80344740++;
            }
        }
    }

    {
        Enemy* e = gEnemies;
        f32 skinOne = lbl_803468F0;
        f64 zero = lbl_80346810;
        f64 bossRise = lbl_80346818;
        f32 zeroFloat = lbl_80346820;
        f64 pushDamping = lbl_80346940;
        f64 pushEpsilon = lbl_80346878;
        f32 verticalDamping = lbl_803469C0;

        for (i = 0; i < gNumEnemies; i++, e++) {
            s32 state;

            e->old_ai = e->algorithm;
            e->operation_count += gFrameTicks;
            if (e->idle_secs > zeroFloat) {
                e->idle_secs -= gClockFrameStep;
            }
            if (e->type == 0) {
                e->daction = 3;
            } else {
                e->daction = 0;
            }

            state = e->state;
            switch (state) {
            case 1:
            case 7:
                shown++;
                if (e->type == gBossType) {
                    goto tail;
                }
                fn_8005A338(&e->objgrp.worldmat[0][0], e->coll_offset,
                            e->attn_offset);
                if (lbl_803447DC != 0) {
                    e->atree.animinfo.starttime += gClockFrameStep;
                    fn_8004DC2C(e);
                    do_enemy_collide(i, lbl_80346820);
                } else if (gTriggerCameraState != 0) {
                    e->daction = 0;
                    e->action = DoEnemyAction(e);
                    do_enemy_collide(i, lbl_80346820);
                    fn_8004DC2C(e);
                } else {
                    fn_800516F8(i);
                    fn_8004DF58(e);
                    fn_8004DC2C(e);
                    if (fn_8004D958(i) != 0) {
                        goto tail;
                    }
                    if (e->atree.root != 0) {
                        e->action = DoEnemyAction(e);
                    }
                    e->anim_done = (e->action == e->daction) ? -1 : 0;
                }
                ProcessSkinFX((f32*)&e->skinfx, e->objgrp.node, 0);
                UpdateObjWorldMat(&e->objgrp.worldmat[0][0]);
                goto tail;
            case 6:
                fn_8005A338(&e->objgrp.worldmat[0][0], e->coll_offset,
                            e->attn_offset);
                shown++;
                goto tail;
            case 8:
                fn_8005A338(&e->objgrp.worldmat[0][0], e->coll_offset,
                            e->attn_offset);
                shown++;
                if (e->type == gBossType) {
                    goto sync;
                }
                if (e->type == 0x1E) {
                    s32 eff = e->specialfx;
                    s32 alpha = e->alpha;

                    if (eff >= 0) {
                        e->specialfx = DeleteEffect(eff, 1);
                    }
                    if (alpha >= 0xFF) {
                        if (e->flag2 != 0) {
                            if (e->org_lvl == 2) {
                                msgPost(0x81, e->coll_pnum,
                                        &e->objgrp.attn_pos[0]);
                            } else {
                                msgPost(0x83, e->coll_pnum,
                                        &e->objgrp.attn_pos[0]);
                            }
                        }
                        kill_enemy(i);
                        goto tail;
                    }
                    MBTreeSetAlpha(e->objgrp.node, alpha, 1);
                    e->alpha = e->alpha + gFrameTicks * 4;
                    e->objgrp.worldmat[3][1] =
                        (f32)(bossRise * gClockFrameStep +
                              e->objgrp.worldmat[3][1]);
                    UpdateObjWorldMat(&e->objgrp.worldmat[0][0]);
                    goto sync;
                } else {
                    s32 cc;

                    fn_8004DC2C(e);
                    e->daction = 0x20;
                    e->pyr[1] = turn_enemy_ang(e, e->ang);
                    do_enemy_move(i);
                    if (e->atree.root != 0) {
                        if (e->type != gBossType) {
                            e->action = DoEnemyAction(e);
                        }
                        if (e->action == e->daction) {
                            e->anim_done = -1;
                        } else {
                            e->anim_done = 0;
                        }
                    }
                    ProcessSkinFX((f32*)&e->skinfx, e->objgrp.node, 0);
                    cc = e->action;
                    if (cc == 0x1C) {
                        goto active_skin;
                    }
                    if (cc == 0x1D) {
                        goto active_skin;
                    }
                    if (cc != 0x20) {
                        goto finished_skin;
                    }
                active_skin:
                    if (e->skinfx.nframes <= zeroFloat) {
                        goto finished_skin;
                    }
                    goto update_skin;
                finished_skin:
                    if (e->type == 0x1D) {
                        if (RandInt(2) == 0) {
                            fn_8009FEFC(e->area);
                        } else {
                            fn_8009FEA0(e->area);
                        }
                    }
                    kill_enemy(i);
                    goto tail;
                update_skin:
                    UpdateObjWorldMat(&e->objgrp.worldmat[0][0]);
                    goto sync;
                }
            sync:
                if (e->shadow != 0) {
                    *(f32*)((u8*)e->shadow + 0x30) = e->objgrp.worldmat[3][0];
                    *(f32*)((u8*)e->shadow + 0x34) = e->objgrp.worldmat[3][1];
                    *(f32*)((u8*)e->shadow + 0x38) = e->objgrp.worldmat[3][2];
                }
                break;
            case 0:
            default:
                break;
            }

        tail:
            e->prev_ai = e->algorithm;
            e->algorithm = e->old_ai;
            if (e->operation_count >= e->operation_speed) {
                e->operation_count -= e->operation_speed;
            }
            e->pushed[0] = (f32)(pushDamping * e->pushed[0]);
            e->pushed[1] = (f32)(pushDamping * e->pushed[1]);
            e->pushed[2] = (f32)(pushDamping * e->pushed[2]);
            {
                f32 v = e->pushed[0];
                *(u32*)&v &= 0x7FFFFFFF;
                if (v < pushEpsilon) {
                    e->pushed[0] = zeroFloat;
                }
            }
            {
                f32 v = e->pushed[1];
                *(u32*)&v &= 0x7FFFFFFF;
                if (v < pushEpsilon) {
                    e->pushed[1] = zeroFloat;
                }
            }
            {
                f32 v = e->pushed[2];
                *(u32*)&v &= 0x7FFFFFFF;
                if (v < pushEpsilon) {
                    e->pushed[2] = zeroFloat;
                }
            }
            if (e->pushed[1] > zeroFloat) {
                e->pushed[1] =
                    e->pushed[1] - verticalDamping * gClockFrameStep;
                if (e->pushed[1] < zeroFloat) {
                    e->pushed[1] = zeroFloat;
                }
            }
            if (gBossType < 0) {
                if ((f64)lbl_803447D8 != zero) {
                    if (e->objgrp.node != 0) {
                        MBTreeSetFlags(e->objgrp.node, 8, 0);
                        *(f32*)((u8*)e->objgrp.node + 0x40) = lbl_803447D8;
                        *(f32*)((u8*)e->objgrp.node + 0x44) = lbl_803447D8;
                        *(f32*)((u8*)e->objgrp.node + 0x48) = lbl_803447D8;
                    }
                    if (e->shadow != 0) {
                        MBTreeSetFlags(e->shadow, 8, 0);
                        *(f32*)((u8*)e->shadow + 0x40) = lbl_803447D8;
                        *(f32*)((u8*)e->shadow + 0x44) = lbl_803447D8;
                        *(f32*)((u8*)e->shadow + 0x48) = lbl_803447D8;
                    }
                } else {
                    if (e->objgrp.node != 0) {
                        MBTreeClearFlags(e->objgrp.node, 8, 0);
                        *(f32*)((u8*)e->objgrp.node + 0x40) = skinOne;
                        *(f32*)((u8*)e->objgrp.node + 0x44) = skinOne;
                        *(f32*)((u8*)e->objgrp.node + 0x48) = skinOne;
                    }
                    if (e->shadow != 0) {
                        MBTreeClearFlags(e->shadow, 8, 0);
                        *(f32*)((u8*)e->shadow + 0x40) = skinOne;
                        *(f32*)((u8*)e->shadow + 0x44) = skinOne;
                        *(f32*)((u8*)e->shadow + 0x48) = skinOne;
                    }
                }
            }
        }
    }

    if (lbl_80344718 == 0) {
        AudioPlayEvt102();
    }
    if ((gControllerButtons & 0x10) != 0 &&
        (gControllerButtons & 1) != 0) {
        s32* blit;
        sprintf(gTextFormatBuf, lbl_803469C4, shown);
        blit = DrawTextKeepScale(lbl_803469C8, -0x100, 0x144, 0, 0xFF0000,
                                 gTextFormatBuf);
        *blit |= 0x40000;
    }
}

/* 0x8004D958 - per-enemy frame update: lifetime, owner change, boss-death
 * cull, AI step + type-24 hover bob timer */
extern s32 gBossDying;
extern void fn_800945D0(u8* pos, u8* a, s32 b, s32 c, u32 type, f32 scale);

s32 fn_8004D958(s32 index)
{
    Enemy* e = &gEnemies[index];
    s32 dir;
    s16 own;
    s16 t;
    s16 v;

    *(s16*)((u8*)e + 726) += gFrameTicks;
    if (*(f32*)((u8*)e + 636) > *(f32*)((u8*)e + 768)) {
        if (*(s16*)((u8*)e + 732) == 0) {
            return -1;
        }
    }
    own = *(s16*)((u8*)e + 630);
    if (own >= 0 && *(s16*)((u8*)e + 628) != own) {
        *(s32*)((u8*)e + 844) = 0;
        *(s32*)((u8*)e + 848) = 4;
    }
    if (gBossType >= 0 && gBossDying != 0) {
        fn_800945D0((u8*)e + 68, (u8*)e + 4, 0, 1, *(u32*)e,
                    *(f32*)((u8*)e + 572));
        kill_enemy(index);
        return -1;
    }
    index = do_ai(index);
    if (e->action == 1) {
        e->daction = 3;
    }
    switch (*(s32*)e) {
    case 24: {
        s32 st = e->action;
        dir = 16;
        if (st == 3) goto bob;
        if (st == 4) goto bob;
        if (st != 0) goto stop;
        {
bob:
            t = e->endurance;
            if (t == 0) {
                fn_8004DB3C(e, -dir);
                e->endurance = RandInt(60) + 60;
            } else if (t > 0) {
                fn_8004DB3C(e, -dir);
                v = e->endurance - gFrameTicks;
                e->endurance = v;
                if (v < 0) {
                    e->endurance = -(RandInt(60) + 60);
                }
            } else {
                fn_8004DB3C(e, 16);
                v = e->endurance + gFrameTicks;
                e->endurance = v;
                if (v > 0) {
                    e->endurance = 0;
                }
            }
        }
        break;
stop:
        fn_8004DB3C(e, -dir);
        e->endurance = 0;
        break;
    }
    }
    return index;
}

extern f32 lbl_803468B0;
extern f32 fn_80034C88(f32 x);
extern s32 LineCylinderCollide(f32* center, f32 radius, f32 halfHeight,
                               f32* from, f32* to, f32* hit, s32 directional);

/* fn_8004646C @0x8004646C -- sweep the enemy's move (oldc->newc) against the
 * critter-node mesh and the neighbouring enemies pulled from the item grid,
 * returning the id of the closest blocking enemy (or a node hit tagged with
 * 0x10000), skipping self, dead/idle occupants, already-linked pack members,
 * and short obstacles when charging.  -1 = clear. */
extern void CritterCollideStart(f32 rad, f32* pos, s32 a);
extern void* CritterMoveNodeCol(f32 rad, f32 zero, f32* from, f32* to,
                                void* hit, s32 a, s32 b);
extern s32 NextGridItem(void);
extern f32 lbl_803468B0;
extern f64 lbl_80346868;

s32 fn_8004646C(f32 rad, f32 hht, s32 index, f32* oldc, f32* newc, f32* newc2,
                s32* hitWorld)
{
    s32 startNode = gEnemies[index].coll_enenum;
    f64 minimum_hht;
    f32 dist;
    f32 best = lbl_803468B0;
    s32 result = -1;
    s32 hint = -1;
    void* nodeCol;
    s32 node;
    Enemy* self;
    u8 stack_top[16];
    f32 scratch[3];
    u8 stack_gap[12];
    f32 delta[3];
    u8 stack_bottom[28];

    if (hitWorld == NULL && startNode < 0x10000) {
        hint = startNode;
    }
    CritterCollideStart(rad, newc, 0);
    nodeCol = CritterMoveNodeCol(rad, 0.0f, oldc, newc, scratch, -1, 2);
    if (nodeCol != NULL) {
        return *(s16*)nodeCol | 0x10000;
    }
    StartItemGrid(rad, newc);
    self = &gEnemies[index];
    minimum_hht = lbl_80346868;
    for (;;) {
        Enemy* other;
        s32 st;
        s32 linked;

        if (hint < 0) {
            node = NextGridItem();
        } else {
            node = hint;
            hint = -1;
        }
        if (node < 0) {
            break;
        }
        other = &gEnemies[node];
        if (node == index) {
            continue;
        }
        st = other->state;
        if (st == 0 || st == 8) {
            continue;
        }
        {
            Enemy* c = self;

            goto load_linked_enemy;
check_linked_enemy:
            if (linked == node) {
                linked = -1;
                goto linked_enemy_done;
            }
            c = &gEnemies[linked];
load_linked_enemy:
            linked = c->next_enemy;
            if (linked >= 0) {
                goto check_linked_enemy;
            }
            linked = 0;
linked_enemy_done:
            ;
        }
        if (linked != 0) {
            continue;
        }
        if ((f64)other->hht <= minimum_hht) {
            if (other->type == E_GOLEM) {
                continue;
            }
        }
        delta[0] = other->objgrp.coll_pos[0] - newc[0];
        delta[1] = other->objgrp.coll_pos[1] - newc[1];
        delta[2] = other->objgrp.coll_pos[2] - newc[2];
        {
            dist = NormalVector2D(delta);

            if (dist >= best) {
                continue;
            }
            if (LineCylinderCollide(&other->objgrp.coll_pos[0],
                                    rad + other->rad,
                                    hht + other->hht, oldc,
                                    newc, scratch, 1) == 0) {
                continue;
            }
            best = dist;
        }
        result = node;
        if (hitWorld != NULL) {
            continue;
        }
        if (node != startNode) {
            continue;
        }
        break;
    }
    return result;
}

/* 0x80046680 - pick the player hit by the enemy's swept collision cylinder;
 * b!=0 restricts the sweep to the nearest live player. */
s32 fn_80046680(f32 rad, f32 hht, s32 index, s32 b, f32* oldc, f32* newc)
{
    s32 last;
    s32 i;
    s32 j;
    u8* p;
    u8* q;
    u8* e = (u8*)gEnemies + index * 916;
    s32 ret = -1;
    s32 start;
    f32 best1;
    f32 best = lbl_803468B0;
    f32 hit[4];
    u8 _pad4[4];
    f32 d;
    f32 dy;
    f32 dx;
    f32 dz;
    u8 _spare[36];

    if (b != 0) {
        start = 0;
        last = 3;
    } else {
        if (*(s16*)(e + 628) < 0) {
            return -1;
        }
        best1 = best;
        p = (u8*)gPlayerWords;
        last = -1;
        for (i = 0; i < 4; i++, p += 13148) {
            if (*(s32*)(p + 232) == 1) {
                if (*(s16*)(p + 2588) > 2) {
                    dx = *(f32*)(e + 84) - *(f32*)(p + 2564);
                    dy = *(f32*)(e + 88) - *(f32*)(p + 2568);
                    dz = *(f32*)(e + 92) - *(f32*)(p + 2572);
                    d = fn_80034C88(dx * dx + dy * dy + dz * dz);
                } else {
                    dx = *(f32*)(e + 84) - *(f32*)(p + 100);
                    dy = *(f32*)(e + 88) - *(f32*)(p + 104);
                    dz = *(f32*)(e + 92) - *(f32*)(p + 108);
                    d = fn_80034C88(dx * dx + dy * dy + dz * dz);
                }
                if (d < best1) {
                    best1 = d;
                    last = i;
                }
            }
        }
        start = last;
    }
    q = (u8*)gPlayerWords + start * 13148;
    for (j = start; j <= last; j++, q += 13148) {
        if (*(s32*)(q + 232) == 1) {
            if (LineCylinderCollide((f32*)(q + 100), rad + *(f32*)(q + 2128),
                                    hht + *(f32*)(q + 2132), oldc, newc, hit,
                                    1) != 0) {
                d = fqdist(hit[0] - newc[0], hit[2] - newc[2]);
                if (d < best) {
                    ret = j;
                    best = d;
                }
            }
        }
    }
    return ret;
}

extern u8 lbl_8011AF48[];
extern f32 lbl_80344880;
extern f32 lbl_80346A40;
extern f64 lbl_80346A28;
extern f32 FloorPos(f32 fallback, f32 radius, f32* position, s32 mode);
extern void SetEnemyObj(Enemy* e, s32 type, s32 level, s32 one);
extern void init_enemy_vars(s32 slot, s32 spew, f32 scale);
extern void fn_8005A338(f32* worldmat, f32* coll_offset, f32* attn_offset);
extern u16 AnimateATree(void* tree, s32 sequence, s32 transition);

/* 0x8004FE34 - initialise a freshly claimed enemy slot's object state. */
void init_enemy(s32 slot, f32* pos, s32 type, s32 level, s32 spew)
{
    Enemy* e = &gEnemies[slot];
    u8* tbl = (u8*)lbl_8011AF48;
    s32 t4;
    f32 z;
    f32 health;

    e->type = type;
    t4 = type * 4;
    z = lbl_80346820;
    e->attn_offset[0] = z;
    e->attn_offset[1] = *(f32*)(tbl + t4 + 2080);
    e->attn_offset[2] = z;
    e->coll_offset[0] = z;
    e->coll_offset[1] = *(f32*)(tbl + t4 + 2216);
    e->coll_offset[2] = z;
    e->pyr[0] = z;
    e->pyr[1] = z;
    e->pyr[2] = z;
    e->state = ACTIVE;
    e->endurance = 0;
    SetEnemyObj(e, type, level, e->state);
    if (level > 3) {
        level = 2;
    }
    if (spew == 18) {
        level = 1;
    }
    {
        u8* r = tbl;
        r += t4;
        health = *(f32*)(r + 2760);
    }
    if (type != E_DEATH) {
        health = health * gCurLevel->ene_health;
    }
    if (type < E_NTYPES) {
        health = (f32)(lbl_80346A28 * health * level);
    }
    fn_8005A338(&e->objgrp.worldmat[0][0], e->coll_offset, e->attn_offset);
    if (e->objgrp.node != NULL) {
        e->objgrp.worldmat[3][0] = pos[0];
        e->objgrp.worldmat[3][1] = pos[1];
        e->objgrp.worldmat[3][2] = pos[2];
        e->objgrp.worldmat[3][1] = FloorPos(lbl_80344880, lbl_80346A40, pos, 2);
        MBTreeClearFlags(e->objgrp.node, 2, 0);
        e->health = health;
        init_enemy_vars(slot, spew, health);
        if (type == E_DEATH) {
            e->org_lvl = level;
        }
        e->objgrp.worldmat[3][1] = e->objgrp.worldmat[3][1] + e->flooroffset;
    }
    UpdateObjWorldMat(&e->objgrp.worldmat[0][0]);
    fn_8005A404(&e->objgrp.worldmat[0][0], e->coll_offset, e->attn_offset);
    e->floory = e->objgrp.worldmat[3][1];
    if (e->shadow != NULL) {
        *(f32*)((u8*)e->shadow + 48) = *(f32*)((u8*)e->objgrp.node + 48);
        *(f32*)((u8*)e->shadow + 52) = *(f32*)((u8*)e->objgrp.node + 52);
        *(f32*)((u8*)e->shadow + 56) = *(f32*)((u8*)e->objgrp.node + 56);
    }
    if (e->atree.root != NULL) {
        AnimateATree(&e->atree, 0, 2);
    }
}
