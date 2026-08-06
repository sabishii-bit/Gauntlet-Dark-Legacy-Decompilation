#include "game/enemy.h"
#include "game/dyngrid.h"

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
 *   find_enemy_slot   0x8004FD00      SetEnemyObj      0x8004FE34
 *
 * Still fn_XXXXXXXX (behaviour understood, exact PDB name not yet pinned):
 *   fn_80045C30 - distance/proximity collision + damage helper called by
 *                 do_enemy_collide (EnemyCollidePlayer or EnemyCollideEnemy).
 *   fn_80046140 - per-type (type==30) generator/timer tick with audio + msgPost.
 *   fn_8004646C - world-grid query (StartItemGrid/NextGridItem).
 *   fn_80046680 - small gEnemies helper called by do_enemy_move.
 *   fn_8004C8CC - wall/object collision probe shared by the move_logic set.
 *   fn_8004CD1C - accelerate along an angle (cos/sin -> velocity).
 *   fn_8004CE38 - left/right look-ahead probe angle helper.
 *   fn_8004D958 - per-enemy tick wrapper (drives do_ai).
 *   fn_8004DC2C/fn_8004DF58/fn_8004E448 - move post-processing / FX helpers.
 *   fn_8004F87C/fn_8004FBC8 - generate_enemy support (type resolve / gendir).
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
s32 do_enemy_collide(s32 index);
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
extern f32 gPlayers[][3287]; /* 0x80275AE0: 0x335C generator records */
extern f32 lbl_8023CA98[][4];
extern f32 lbl_8011BED8[];  /* 0x8011BED8 per-type turn-rate table */ /* wall-slide scratch; [1] = output vector */

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
u32 lbl_80250E40[0x2B4 / 4];   /* 0x80250E40 */
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
    lbl_80250E40[0] = 0;
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

extern f64 lbl_80346840;        /* pi wrap high */
extern f64 lbl_80346848;        /* 2*pi */
extern f64 lbl_80346850;        /* -pi wrap low */

void do_enemy_move(s32 index)
{
    u8* row = (u8*)lbl_80250E00 + index * 916;
    Enemy* e = (Enemy*)(row + 3608); /* = &gEnemies[index] via the pool anchor */
    s32 alg = e->algorithm;
    f32 rad = e->rad;
    f32 hht = e->hht;
    s32 blocked = 0;
    s32 collide;
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
    if (fqdist(e->trans[0], e->trans[2]) > 0.001) {
        e->moved = 1;
    } else {
        e->moved = 0;
    }

    collide = do_enemy_collide(index);

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
                               &gPlayers[e->coll_pnum][17]);
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
extern s32 FloorCollide(f32* pos, s32 a, s32 b, s32 mode, f32 x, f32 y, f32 z);
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

s32 do_enemy_collide(s32 index)
{
    u8* pool = (u8*)lbl_80250E00;
    u8* e = (u8*)&gEnemies[index];
    f32* tr = (f32*)(e + 0x210);
    f32 dt = (f32)(lbl_80346860 * gClockFrameStep);
    s16 behavior = *(s16*)(e + 0x310);
    s32 result = 0;
    f32 rad;
    f32 slideRad;
    void* hit = NULL;
    f32 oldpos[3];
    f32 dh;

    if (*(s32*)e == 0x1F || *(s32*)(e + 0x358) <= 0) {
        *(s16*)(e + 0x1FE) = 0;
    }
    if (*(s16*)(e + 0x280) == 0) {
        void* mp = *(void**)(e + 0x298);
        if (mp != NULL && !(*(u32*)((u8*)mp + 0x10) & 0x1000)) {
            goto gravity;
        }
    }

    rad = *(f32*)(e + 0x238);
    oldpos[0] = *(f32*)(e + 0x54);
    oldpos[1] = *(f32*)(e + 0x58);
    oldpos[2] = *(f32*)(e + 0x5C);
    oldpos[1] = (f32)((lbl_80346868 - *(f32*)(e + 0x21C)) + oldpos[1]);

    if (*(s16*)(e + 0x280) != 0) {
        if (*(s32*)e == 0x1D) {
            f32 np[3];
            slideRad = (f32)(lbl_80346830 * rad);
            slideRad = (f32)(slideRad * lbl_80346838);
            np[0] = oldpos[0] + tr[0];
            np[1] = oldpos[1] + tr[1];
            np[2] = oldpos[2] + tr[2];
            lbl_80344730 = EnemyWallCollide(slideRad, oldpos, np,
                                                 (f32*)(pool + 0x2F4));
            if (lbl_80344730 != 0) {
                EnemyWorldDamage((Enemy*)e, lbl_80344730, oldpos,
                                 (f32*)(pool + 0x2F4));
                if (*(u32*)((u8*)lbl_80344730 + 0x10) & 0x38) {
                    result = 0;
                } else if (!(*(u32*)(e + 0x330) & 1)) {
                    if (SlideAlongWall(slideRad, oldpos, tr,
                                       (f32*)(pool + 0x2F4),
                                       lbl_8023CA98[1]) < 0) {
                        tr[2] = 0.0f;
                        tr[0] = 0.0f;
                        result = 2;
                    } else {
                        result = 1;
                    }
                } else {
                    result = 1;
                }
            } else {
                result = 0;
            }
        } else {
            f32 np[3];
            slideRad = (f32)(rad * lbl_80346838);
            np[0] = oldpos[0] + tr[0];
            np[1] = oldpos[1] + tr[1];
            np[2] = oldpos[2] + tr[2];
            lbl_80344730 = EnemyWallCollide(slideRad, oldpos, np,
                                                 (f32*)(pool + 0x2F4));
            if (lbl_80344730 != 0) {
                EnemyWorldDamage((Enemy*)e, lbl_80344730, oldpos,
                                 (f32*)(pool + 0x2F4));
                if (*(u32*)((u8*)lbl_80344730 + 0x10) & 0x38) {
                    result = 0;
                } else if (!(*(u32*)(e + 0x330) & 1)) {
                    if (SlideAlongWall(slideRad, oldpos, tr,
                                       (f32*)(pool + 0x2F4),
                                       lbl_8023CA98[1]) < 0) {
                        tr[2] = 0.0f;
                        tr[0] = 0.0f;
                        result = 2;
                    } else {
                        result = 1;
                    }
                } else {
                    result = 1;
                }
            } else {
                result = 0;
            }
        }
    }

    hit = fn_80045C30((Enemy*)e, rad, 0.0f, oldpos, tr, result);
    if (hit != NULL) {
        *(void**)(e + 0x298) = hit;
        if ((f64)*(f32*)(e + 0x23C) <= lbl_80346868) {
            if (*(u32*)((u8*)hit + 0x10) & 0x38) {
                result = 2;
                tr[2] = 0.0f;
                tr[0] = 0.0f;
            }
        }
    } else {
        void* mp = *(void**)(e + 0x298);
        if (mp != NULL && (*(u32*)((u8*)mp + 0x10) & 0x1000)) {
            goto reparent;
        }
        tr[2] = 0.0f;
        tr[0] = 0.0f;
        hit = (void*)FloorCollide(oldpos, (s32)(pool + 0x300), 0, 2,
                                  (f32)(lbl_80346830 * rad), *(f32*)(e + 0x23C),
                                  (f32)(-*(f32*)(e + 0x23C) - lbl_80346870));
        if (hit != NULL) {
            *(f32*)(e + 0x294) = *(f32*)(pool + 0x334) + *(f32*)(e + 0x21C);
            if (*(void**)(e + 0x1DC) != NULL) {
                CopyMat3((f32*)(pool + 0x300), *(f32**)(e + 0x1DC));
            }
        }
    }

reparent:
    if (hit != NULL) {
        void* parent = *(void**)((u8*)hit + 0x28);
        if (parent != NULL && (*(u32*)((u8*)hit + 0x10) & 0x1000)) {
            MBNodeSetParent(*(void**)(e + 0x64), parent);
        } else {
            MBNodeSetParent(*(void**)(e + 0x64), (void*)lbl_8034473C);
        }
    }
    *(s32*)(e + 0x29C) = (hit != NULL) ? *(s32*)((u8*)hit + 0x10) : 0;

    if ((f64)fqdist(tr[0], tr[2]) >= lbl_80346878) {
        goto gravity;
    }

    if (behavior == 0) {
        if (ABS(*(s32*)(e + 0x354)) > 2) {
            (*(s16*)(e + 0x364))++;
            fn_8004D030(index, 0x3C);
        } else {
            (*(s16*)(e + 0x364))++;
            fn_8004D030(index, 5);
        }
        if (*(s16*)(e + 0x364) >= 9) {
            *(s32*)(e + 0x354) = -*(s32*)(e + 0x354) * 2;
            *(s16*)(e + 0x364) = 0;
            if (ABS(*(s32*)(e + 0x354)) > 2) {
                *(f32*)(e + 0x24C) = lbl_80344720;
                *(f32*)(e + 0x244) = lbl_80344720;
            }
        }
    } else if (behavior == 7) {
        if (ABS(*(s32*)(e + 0x354)) > 2) {
            fn_8004D030(index, 0x3C);
            *(f32*)(e + 0x24C) = lbl_80344720;
            *(f32*)(e + 0x244) = lbl_80344720;
            *(s16*)(e + 0x364) = 0;
            *(s32*)(e + 0x354) = 0;
        } else {
            (*(s16*)(e + 0x364))++;
            fn_8004D030(index, 0xA);
        }
        if (*(s16*)(e + 0x364) >= 7) {
            *(s32*)(e + 0x354) = -*(s32*)(e + 0x354) * 2;
            *(s16*)(e + 0x364) = 0;
        }
    } else if (behavior == 8) {
        if (ABS(*(s32*)(e + 0x354)) > 2) {
            fn_8004D030(index, 0x3C);
            *(f32*)(e + 0x24C) = lbl_80344720;
            *(f32*)(e + 0x244) = lbl_80344720;
            *(s16*)(e + 0x364) = 0;
            *(s32*)(e + 0x354) = 0;
        } else {
            (*(s16*)(e + 0x364))++;
            fn_8004D030(index, 5);
        }
        if (*(s16*)(e + 0x364) >= 7) {
            *(s32*)(e + 0x354) = -*(s32*)(e + 0x354) * 2;
            *(s16*)(e + 0x364) = 0;
        }
    } else if (behavior == 0xA) {
        if (ABS(*(s32*)(e + 0x354)) > 2) {
            fn_8004D030(index, 0x3C);
            *(f32*)(e + 0x24C) = lbl_80344720;
            *(f32*)(e + 0x244) = lbl_80344720;
            *(s16*)(e + 0x364) = 0;
            *(s32*)(e + 0x354) = 0;
        } else {
            (*(s16*)(e + 0x364))++;
            fn_8004D030(index, 0xA);
        }
        if (*(s16*)(e + 0x364) >= 7) {
            *(s32*)(e + 0x354) = -*(s32*)(e + 0x354) * 2;
            *(s16*)(e + 0x364) = 0;
        }
    } else if (behavior == 0x14) {
        if (ABS(*(s32*)(e + 0x354)) > 2) {
            (*(s16*)(e + 0x364))++;
            fn_8004D030(index, 3);
        } else {
            f64 a;
            fn_8004D030(index, 0x1E);
            *(f32*)(e + 0x24C) = (f32)(lbl_80346840 + lbl_80344720);
            a = *(f32*)(e + 0x24C);
            if (a > lbl_80346840) {
                a -= lbl_80346848;
            } else if (a <= lbl_80346850) {
                a += lbl_80346848;
            }
            *(f32*)(e + 0x24C) = (f32)a;
            *(f32*)(e + 0x244) = (f32)a;
            *(s16*)(e + 0x364) = 0;
            *(s32*)(e + 0x354) = 0;
        }
        if (*(s16*)(e + 0x364) >= 7) {
            *(s32*)(e + 0x354) = -*(s32*)(e + 0x354) * 2;
            *(s16*)(e + 0x364) = 0;
        }
    } else {
        if (*(s32*)(e + 0x358) <= 0) {
            *(s32*)(e + 0x358) = 0x14;
        }
    }
    *(s16*)(e + 0x1FE) = 1;

gravity:
    dh = *(f32*)(e + 0x294) - *(f32*)(e + 0x38);
    if ((f64)dh < lbl_80346880) {
        damage_enemy((Enemy*)e, lbl_80346888, -1, 0, 0, 0, 0);
    }
    if (dh < dt) {
        dh = dt;
    }
    tr[1] += dh;
    *(f32*)(e + 0x294) = *(f32*)(e + 0x38) + dh;
    return result;
}

/* ===================================================================== *
 *  AI MOVE-LOGIC STATE HANDLERS  (do_ai jumptable, 0x80046B54..0x8004C650)
 *  Each takes the enemy slot index, sets a desired facing/velocity, then
 *  calls do_enemy_move(index) to commit the move + resolve collisions.
 * ===================================================================== */

/* --- move_logic shared externs --- */
extern void RequestEnemyAction(Enemy* e, s32 action);
extern f32 get_yaw(f32* to, f32* from);       /* dir angle from->to */
extern void fn_80050394(s32 index);           /* AI-change transition hook */
extern void fn_8004CD1C(Enemy* e, f32 spd, f32 ang); /* accel along angle */
extern f32 lbl_8011BF60[];    /* 0x8011BF60 imp retreat-speed ramp table */
extern s32 lbl_80344748;      /* 0x80344748 current "IT" enemy slot */
extern s32 RandInt(s32 n);
extern u8* gCurLevel;         /* 0x8034483C active level record */
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
extern s32 gControllerButtons;      /* 0x803445C8 companion config word */
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

    if (delta <= 0 || alpha < 255) {
        if (delta < 0 && alpha == 0) {
            asm {
                b fade_done
            }
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
    asm {
    fade_done:
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
    struct {
        u8 pad[12];
        f32 vertical;
        f32 milestone[3];
    } local;
    f32 dx;
    f32 dz;

    if (enemy->plr_ms >= 0) {
        GetMilestonePos(enemy->plr_ms, local.milestone);
        dx = *(f32*)((u8*)enemy + 0x34) - local.milestone[0];
        dz = *(f32*)((u8*)enemy + 0x3C) - local.milestone[2];
        local.vertical = *(f32*)((u8*)enemy + 0x38) - local.milestone[1];
        *(u32*)&local.vertical &= 0x7FFFFFFF;
        if ((f64)local.vertical < lbl_803469F8 &&
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
    s32* player = (s32*)((u8*)gPlayers + enemy->closest * 0x335C);
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

/* Advance texture modifiers for each loaded enemy type while gameplay runs. */
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
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608);
    f32 speed = ((f32*)lbl_80250E40)[e->type];
    s32 it = lbl_80344748;
    s32 flee;
    f32 base;
    u8 unused[24];

    if (it < 0) {
        flee = 0;
    } else {
        Enemy* other = (Enemy*)((u8*)lbl_80250E00 + it * 916 + 3608);
        if (other->state != 1) {
            flee = 0;
        } else if (other->actual_dist > e->sight) {
            flee = 0;
        } else {
            f32 dy = other->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
            f32 dx = other->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
            f32 dz = other->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
            if (index != it && e->birth_style == 0 && e->dead_end <= 0
                && dy * dy + dx * dx + dz * dz < 100.0) {
                flee = -1;
            } else {
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
        fn_80050394(index);
    }
    if (e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    if (e->dead_end < 1) {
        if (e->closest < 0) {
            base = e->ang;
        } else if (*(s16*)&gPlayers[e->closest][647] > 2) {
            base = get_yaw(&gPlayers[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            base = get_yaw(&gPlayers[e->closest][17], &e->objgrp.worldmat[3][0]);
        }
        lbl_80344720 = base;
        {
            s32 found = 0;
            s32 off = 0;
            f32 cand = base;
            f32 probe[3];
            do {
                f32 d;
                if (e->route < 1) {
                    cand = cand - *(f32*)((u8*)lbl_8011C0C4 + off);
                } else {
                    cand = cand + *(f32*)((u8*)lbl_8011C0C4 + off);
                }
                if (cand > 3.141592654) {
                    cand = cand - 6.283185308;
                } else if (cand <= -3.141592654) {
                    cand = 6.283185308 + cand;
                }
                probe[0] = e->objgrp.worldmat[3][0];
                probe[1] = e->objgrp.worldmat[3][1] + 0.1 + e->rad;
                probe[2] = e->objgrp.worldmat[3][2];
                probe[0] += speed * sin(cand);
                probe[2] += speed * cos(cand);
                d = cand - e->ang;
                if (d > 3.141592654) {
                    d = d - 6.283185308;
                } else if (d <= -3.141592654) {
                    d = 6.283185308 + d;
                }
                if ((fabsf_(e->ang - e->angbak) <= 0.034906585044444445
                     || fabsf_(cand - e->angbak) > 0.034906585044444445)
                    && fabsf_(d) < 3.106686068955556
                    && fn_8004C8CC(probe, index) != 0) {
                    break;
                }
                found++;
                off += 4;
            } while (found < 9);
            if (found < 9) {
                e->collided = found;
            } else {
                cand = lbl_80344720;
            }
            e->angbak = e->ang;
            e->ang = cand;
        }
    }
    fn_8004CD1C(e, 1.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* move_logic01 @0x80046F24 (state 1, patrol-toward-milestone).  IT-flee / chase
 * gates, then face a milestone only after an operation-speed cadence, run down a
 * short area-2 timer, and re-lock the heading when it drifts past pi/30. */
void move_logic01(s32 index)
{
    s32 stuck;
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608);
    s32 dead0 = *(s32*)((u8*)lbl_80250E00 + index * 916 + 4464);
    s32 it;
    s32 flee;
    f32 a;
    u8 unused[32];

    if (dead0 > 0) {
        stuck = 1;
    } else {
        stuck = 0;
    }
    it = lbl_80344748;
    if (it < 0) {
        flee = 0;
    } else {
        Enemy* other = (Enemy*)((u8*)lbl_80250E00 + it * 916 + 3608);
        if (other->state != 1) {
            flee = 0;
        } else if (other->actual_dist > e->sight) {
            flee = 0;
        } else {
            f32 dy = other->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
            f32 dx = other->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
            f32 dz = other->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
            if (index != it && e->birth_style == 0 && dead0 <= 0
                && dy * dy + dx * dx + dz * dz < 100.0) {
                flee = -1;
            } else {
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
        fn_80050394(index);
    }
    if (e->closest < 0 || e->operation_count < e->operation_speed) {
        a = e->ang;
    } else if (*(s16*)&gPlayers[e->closest][647] > 2) {
        e->ang = get_yaw(&gPlayers[e->closest][633], &e->objgrp.worldmat[3][0]);
        a = e->ang;
    } else {
        e->ang = get_yaw(&gPlayers[e->closest][17], &e->objgrp.worldmat[3][0]);
        a = e->ang;
    }
    e->ang = a;
    if (e->area == 2 && e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    {
        f32 d = e->ang - e->anghit;
        *(u32*)&d &= 0x7FFFFFFF;
        if (e->dead_end <= 0 || d >= 0.10471975513333334) {
            e->dead_end = 0;
            fn_8004CD1C(e, 1.0f, e->ang);
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
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608);
    s32 it = lbl_80344748;
    s32 flee;
    u8 unused[16];

    if (it < 0) {
        flee = 0;
    } else {
        Enemy* other = (Enemy*)((u8*)lbl_80250E00 + it * 916 + 3608);
        if (other->state != 1) {
            flee = 0;
        } else if (other->actual_dist > e->sight) {
            flee = 0;
        } else {
            f32 dy = other->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
            f32 dx = other->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
            f32 dz = other->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
            if (index != it && e->birth_style == 0 && e->dead_end <= 0
                && dy * dy + dx * dx + dz * dz < 100.0) {
                flee = -1;
            } else {
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->closest >= 0 && e->close_dist <= 8.0) {
        e->algorithm = 0;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        fn_80050394(index);
    }
    if (e->dead_end > 0) {
        if ((e->dead_end -= gFrameTicks) <= 0) {
            e->ang += 0.7853981635;
            {
                f64 a = e->ang;
                if (a > 3.141592654) {
                    a -= 6.283185308;
                } else if (a <= -3.141592654) {
                    a = 6.283185308 + a;
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
    if (e->coll_pnum >= 0) {
        e->ang = get_yaw(&gPlayers[e->coll_pnum][17], &e->objgrp.worldmat[3][0]);
    }
    fn_8004CD1C(e, 1.0f, e->ang);
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
            if (*(s16*)&gPlayers[e->counter2][647] > 2) {
                face = get_yaw(&gPlayers[e->counter2][633], &e->objgrp.worldmat[3][0]);
            } else {
                face = get_yaw(&gPlayers[e->counter2][17], &e->objgrp.worldmat[3][0]);
            }
        } else {
            face = e->ang;
        }
        e->ang = spd + (3.141592654 + face);
        {
            f64 a = e->ang;
            if (a > 3.141592654) {
                a -= 6.283185308;
            } else if (a <= -3.141592654) {
                a = 6.283185308 + a;
            }
            e->ang = a;
        }
        fn_8004CD1C(e, 1.0f, e->ang);
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
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608);
    s32 it = lbl_80344748;
    s32 flee;
    u8 unused[16];

    if (it < 0) {
        flee = 0;
    } else {
        Enemy* other = (Enemy*)((u8*)lbl_80250E00 + it * 916 + 3608);
        if (other->state != 1) {
            flee = 0;
        } else if (other->actual_dist > e->sight) {
            flee = 0;
        } else {
            f32 dy = other->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
            f32 dx = other->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
            f32 dz = other->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
            if (index != it && e->birth_style == 0 && e->dead_end <= 0
                && dy * dy + dx * dx + dz * dz < 100.0) {
                flee = -1;
            } else {
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->closest >= 0 && e->close_dist <= 8.0) {
        e->algorithm = 0;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        fn_80050394(index);
    }
    if (e->dead_end > 0) {
        if ((e->dead_end -= gFrameTicks) <= 0) {
            e->ang -= 0.7853981635;
            {
                f64 a = e->ang;
                if (a > 3.141592654) {
                    a -= 6.283185308;
                } else if (a <= -3.141592654) {
                    a = 6.283185308 + a;
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
        e->ang = get_yaw(&gPlayers[e->coll_pnum][17], &e->objgrp.worldmat[3][0]);
    }
    fn_8004CD1C(e, 1.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* move_logic05 @0x80047844 (state 5, one of the two "wander" fallbacks reached
 * from the recognized/closest gate).  IT-flee, drift a heading on a dead_end
 * timer (rotating -pi/2 and cycling a 4-count), then sweep two clearance probes:
 * a near ray (rad+0.5) via FastWallCollide and a far ray (speed) via the shared
 * object probe; a block on either rotates the heading and re-arms the timer. */
void move_logic05(s32 index)
{
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608);
    f32 dist = e->rad;
    f32 speed = ((f32*)lbl_80250E40)[e->type];
    s32 it = lbl_80344748;
    s32 flee;
    f32 probe[3];
    f32 probeEnd[3];
    f32 probe2[3];
    u8 unused[56];

    if (it < 0) {
        flee = 0;
    } else {
        Enemy* other = (Enemy*)((u8*)lbl_80250E00 + it * 916 + 3608);
        if (other->state != 1) {
            flee = 0;
        } else if (other->actual_dist > e->sight) {
            flee = 0;
        } else {
            f32 dy = other->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
            f32 dx = other->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
            f32 dz = other->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
            if (index != it && e->birth_style == 0 && e->dead_end <= 0
                && dy * dy + dx * dx + dz * dz < 100.0) {
                flee = -1;
            } else {
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
        fn_80050394(index);
    }
    if (e->dead_end > 0) {
        if ((e->dead_end -= gFrameTicks) <= 0) {
            e->ang = e->ang - 1.570796327;
            {
                f64 a = e->ang;
                if (a > 3.141592654) {
                    a -= 6.283185308;
                } else if (a <= -3.141592654) {
                    a = 6.283185308 + a;
                }
                e->ang = a;
            }
            if (++e->count >= 4) {
                e->count = 0;
            }
        }
    }
    probe[0] = e->objgrp.worldmat[3][0];
    probe[1] = e->objgrp.worldmat[3][1];
    probe[2] = e->objgrp.worldmat[3][2];
    probe[1] += 0.1 + e->rad;
    probeEnd[0] = probe[0];
    probeEnd[1] = probe[1];
    probeEnd[2] = probe[2];
    dist = dist + 0.5;
    probeEnd[0] += dist * sin(e->ang);
    probeEnd[2] += dist * cos(e->ang);
    if (FastWallCollide(probe, probeEnd, 0, 2) != 0) {
        e->ang = e->ang - 1.570796327;
        {
            f64 a = e->ang;
            if (a > 3.141592654) {
                a -= 6.283185308;
            } else if (a <= -3.141592654) {
                a = 6.283185308 + a;
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
            e->ang = e->ang - 1.570796327;
            {
                f64 a = e->ang;
                if (a > 3.141592654) {
                    a -= 6.283185308;
                } else if (a <= -3.141592654) {
                    a = 6.283185308 + a;
                }
                e->ang = a;
            }
            if (e->dead_end <= 0) {
                e->dead_end = 20;
            }
        }
    }
    fn_8004CD1C(e, 1.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* move_logic06 @0x80047BF0 (state 6, mirror of move_logic05).  Same IT-flee and
 * two-probe clearance search, but every heading correction rotates +pi/2 instead
 * of -pi/2, so it sweeps the opposite way around an obstacle. */
void move_logic06(s32 index)
{
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608);
    f32 dist = e->rad;
    f32 speed = ((f32*)lbl_80250E40)[e->type];
    s32 it = lbl_80344748;
    s32 flee;
    f32 probe[3];
    f32 probeEnd[3];
    f32 probe2[3];
    u8 unused[56];

    if (it < 0) {
        flee = 0;
    } else {
        Enemy* other = (Enemy*)((u8*)lbl_80250E00 + it * 916 + 3608);
        if (other->state != 1) {
            flee = 0;
        } else if (other->actual_dist > e->sight) {
            flee = 0;
        } else {
            f32 dy = other->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
            f32 dx = other->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
            f32 dz = other->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
            if (index != it && e->birth_style == 0 && e->dead_end <= 0
                && dy * dy + dx * dx + dz * dz < 100.0) {
                flee = -1;
            } else {
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
        fn_80050394(index);
    }
    if (e->dead_end > 0) {
        if ((e->dead_end -= gFrameTicks) <= 0) {
            e->ang = e->ang + 1.570796327;
            {
                f64 a = e->ang;
                if (a > 3.141592654) {
                    a -= 6.283185308;
                } else if (a <= -3.141592654) {
                    a = 6.283185308 + a;
                }
                e->ang = a;
            }
            if (++e->count >= 4) {
                e->count = 0;
            }
        }
    }
    probe[0] = e->objgrp.worldmat[3][0];
    probe[1] = e->objgrp.worldmat[3][1];
    probe[2] = e->objgrp.worldmat[3][2];
    probe[1] += 0.1 + e->rad;
    probeEnd[0] = probe[0];
    probeEnd[1] = probe[1];
    probeEnd[2] = probe[2];
    dist = dist + 0.5;
    probeEnd[0] += dist * sin(e->ang);
    probeEnd[2] += dist * cos(e->ang);
    if (FastWallCollide(probe, probeEnd, 0, 2) != 0) {
        e->ang = e->ang + 1.570796327;
        {
            f64 a = e->ang;
            if (a > 3.141592654) {
                a -= 6.283185308;
            } else if (a <= -3.141592654) {
                a = 6.283185308 + a;
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
            e->ang = e->ang + 1.570796327;
            {
                f64 a = e->ang;
                if (a > 3.141592654) {
                    a -= 6.283185308;
                } else if (a <= -3.141592654) {
                    a = 6.283185308 + a;
                }
                e->ang = a;
            }
            if (e->dead_end <= 0) {
                e->dead_end = 20;
            }
        }
    }
    fn_8004CD1C(e, 1.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* move_logic07 @0x80047F9C (state 7, rat-style corner-hugging chase).  IT-flee /
 * recognized gates, face a milestone/player, then when the dead_end timer allows
 * pick a corner-avoidance heading (fn_8004CE38 route + lbl_8011C0A4 offset table),
 * normalize it, probe clearance, and count consecutive stuck frames; bail back to
 * the straight bearing after 10.  Rats (type 3) poke a walk action at the end. */
void move_logic07(s32 index)
{
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608);
    f32 speed = ((f32*)lbl_80250E40)[e->type];
    s32 it = lbl_80344748;
    s32 flee;
    s32 found = 0;
    f32 cand;
    f32 face;
    f32 probe[3];
    u8 unused[32];

    if (it < 0) {
        flee = 0;
    } else {
        Enemy* other = (Enemy*)((u8*)lbl_80250E00 + it * 916 + 3608);
        if (other->state != 1) {
            flee = 0;
        } else if (other->actual_dist > e->sight) {
            flee = 0;
        } else {
            f32 dy = other->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
            f32 dx = other->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
            f32 dz = other->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
            if (index != it && e->birth_style == 0 && e->dead_end <= 0
                && dy * dy + dx * dx + dz * dz < 100.0) {
                flee = -1;
            } else {
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
        fn_80050394(index);
    }
    if (e->closest < 0) {
        face = e->ang;
    } else if (*(s16*)&gPlayers[e->closest][647] > 2) {
        face = get_yaw(&gPlayers[e->closest][633], &e->objgrp.worldmat[3][0]);
    } else {
        face = get_yaw(&gPlayers[e->closest][17], &e->objgrp.worldmat[3][0]);
    }
    lbl_80344720 = face;
    if (e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    if (e->dead_end < 1) {
        if (e->coll_pnum < 0) {
            if (e->area == 1) {
                s32 col = e->collided;
                cand = lbl_80344720;
                if (e->route == 0) {
                    e->route = fn_8004CE38(e);
                }
                if (e->route > 0) {
                    cand = cand + lbl_8011C0A4[col];
                } else {
                    cand = cand - lbl_8011C0A4[col];
                }
            } else if (e->coll_ip == 0 && e->coll_enenum < 0) {
                cand = lbl_80344720;
            } else {
                cand = e->ang;
                if (e->route > 0) {
                    cand = cand + lbl_8011C0A4[e->collided];
                } else {
                    cand = cand - lbl_8011C0A4[e->collided];
                }
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
        if ((fabsf_(e->ang - e->angbak) > 0.034906585044444445
             && fabsf_(cand - e->angbak) <= 0.034906585044444445)
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
    fn_8004CD1C(e, 1.0f, cand);
    if (found == 0 || cand == lbl_80344720) {
        e->pyr[1] = turn_enemy_ang(e, cand);
    }
    do_enemy_move(index);
    if (e->type == E_RAT && e->daction == 0) {
        RequestEnemyAction(e, 3);
    }
}

/* move_logic08 @0x80048408 (state 8, guard/warlock corner-hug chase).  Sibling of
 * move_logic07 but with a guard target: fn_80051568 refreshes guard_closest, which
 * (when >=0) overrides the milestone bearing with the guarded item's heading.  When
 * cornered it also validates the collided item (a live spawner of type 2) before
 * choosing a corner-avoidance offset from lbl_8011C084. */
void move_logic08(s32 index)
{
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608);
    f32 speed = ((f32*)lbl_80250E40)[e->type];
    s32 it = lbl_80344748;
    s32 flee;
    s32 found = 0;
    f32 cand;
    f32 face;
    f32 probe[3];
    u8 unused[32];

    if (it < 0) {
        flee = 0;
    } else {
        Enemy* other = (Enemy*)((u8*)lbl_80250E00 + it * 916 + 3608);
        if (other->state != 1) {
            flee = 0;
        } else if (other->actual_dist > e->sight) {
            flee = 0;
        } else {
            f32 dy = other->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
            f32 dx = other->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
            f32 dz = other->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
            if (index != it && e->birth_style == 0 && e->dead_end <= 0
                && dy * dy + dx * dx + dz * dz < 100.0) {
                flee = -1;
            } else {
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
        fn_80050394(index);
    }
    fn_80051568(index);
    if (e->guard_closest >= 0) {
        face = get_yaw((f32*)(sItems + e->guard_closest * 240 + 52),
                           &e->objgrp.worldmat[3][0]);
    } else if (e->closest < 0) {
        face = e->ang;
    } else if (*(s16*)&gPlayers[e->closest][647] > 2) {
        face = get_yaw(&gPlayers[e->closest][633], &e->objgrp.worldmat[3][0]);
    } else {
        face = get_yaw(&gPlayers[e->closest][17], &e->objgrp.worldmat[3][0]);
    }
    lbl_80344720 = face;
    if (e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    if (e->dead_end < 1) {
        if (e->coll_pnum >= 0) {
            if (e->closest < 0) {
                cand = e->ang;
            } else if (*(s16*)&gPlayers[e->closest][647] > 2) {
                cand = get_yaw(&gPlayers[e->closest][633], &e->objgrp.worldmat[3][0]);
            } else {
                cand = get_yaw(&gPlayers[e->closest][17], &e->objgrp.worldmat[3][0]);
            }
            lbl_80344720 = cand;
        } else {
            void* ip = e->coll_ip;
            s32 valid;
            if (ip == 0) {
                valid = 0;
            } else if (*(s16*)((u8*)ip + 196) == -1 || **(s32**)ip != 2
                       || *(s8*)((u8*)ip + 205) != 0) {
                valid = 0;
            } else {
                valid = -1;
            }
            if (valid != 0) {
                cand = lbl_80344720;
            } else if (e->area == 1) {
                s32 col = e->collided;
                cand = lbl_80344720;
                if (e->route == 0) {
                    e->route = fn_8004CE38(e);
                }
                if (e->route > 0) {
                    cand = cand + lbl_8011C084[col];
                } else {
                    cand = cand - lbl_8011C084[col];
                }
            } else if (e->coll_ip == 0 && e->coll_enenum < 0) {
                cand = lbl_80344720;
            } else {
                cand = e->ang;
                if (e->route > 0) {
                    cand = cand + lbl_8011C084[e->collided];
                } else {
                    cand = cand - lbl_8011C084[e->collided];
                }
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
        if ((fabsf_(e->ang - e->angbak) > 0.034906585044444445
             && fabsf_(cand - e->angbak) <= 0.034906585044444445)
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
    fn_8004CD1C(e, 1.0f, cand);
    if (found == 0 || cand == lbl_80344720) {
        e->pyr[1] = turn_enemy_ang(e, cand);
    }
    do_enemy_move(index);
}

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
 *   turn_enemy_ang x5, fn_8004CD1C x5, do_enemy_move x5 - one move per sub-mode
 *   fn_80050394 x5   - AI-change transition per sub-mode
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

void move_logic10(s32 index)
{
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608);
    f32 speed = ((f32*)lbl_80250E40)[e->type];
    s32 it = lbl_80344748;
    s32 flee;
    s32 skip;
    s32 blocked = 0;
    f32 face;
    f32 cand;
    f32 probe[3];

    if (it < 0) {
        flee = 0;
    } else {
        Enemy* other = (Enemy*)((u8*)lbl_80250E00 + it * 916 + 3608);
        if (other->state != 1) {
            flee = 0;
        } else if (other->actual_dist > e->sight) {
            flee = 0;
        } else {
            f32 dy = other->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
            f32 dx = other->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
            f32 dz = other->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
            if (index != it && e->birth_style == 0 && e->dead_end <= 0
                && dy * dy + dx * dx + dz * dz < 100.0) {
                flee = -1;
            } else {
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

    if (e->mode1 == 1) {
        /* -- mode 1: walking a neighbor-milestone chain back toward the pack -- */
        if (e->coll_pnum < 0) {
            skip = 0;
        } else {
            if (e->algorithm != e->prev_ai) {
                fn_80050394(index);
            }
            if (e->closest < 0) {
                face = e->ang;
            } else if (*(s16*)&gPlayers[e->closest][647] > 2) {
                face = get_yaw(&gPlayers[e->closest][633],
                               &e->objgrp.worldmat[3][0]);
            } else {
                face = get_yaw(&gPlayers[e->closest][17],
                               &e->objgrp.worldmat[3][0]);
            }
            e->ang = face;
            e->dead_end = 0;
            fn_8004CD1C(e, 1.0f, e->ang);
            e->pyr[1] = turn_enemy_ang(e, e->ang);
            do_enemy_move(index);
            skip = -1;
        }
        if (skip == 0) {
            if (e->algorithm != e->prev_ai) {
                fn_80050394(index);
            }
            if (e->plr_ms < 0) {
                s32 got = 0;
                e->mode2--;
                if (e->mode2 > 0) {
                    e->plr_ms = find_neighbor_milestone(e->plr_ms, e->mode2);
                    got = e->plr_ms >= 0;
                    if (got) {
                        f32 mspos[3];
                        e->stuck_count = 0;
                        e->collided = 0;
                        GetMilestonePos(e->plr_ms, mspos);
                        lbl_80344720 =
                            get_yaw(mspos, &e->objgrp.worldmat[3][0]);
                    }
                }
                if (!got) {
                    if (e->closest < 0) {
                        face = e->ang;
                    } else if (*(s16*)&gPlayers[e->closest][647] > 2) {
                        face = get_yaw(&gPlayers[e->closest][633],
                                       &e->objgrp.worldmat[3][0]);
                    } else {
                        face = get_yaw(&gPlayers[e->closest][17],
                                       &e->objgrp.worldmat[3][0]);
                    }
                    lbl_80344720 = face;
                    e->mode1 = 0;
                    e->mode2 = 0;
                    e->stuck_count = 0;
                    e->collided = 0;
                }
            } else {
                f32 mspos[3];
                if (e->stuck_count > 4) {
                    s32 old = e->plr_ms;
                    e->mode2++;
                    e->plr_ms = find_neighbor_milestone(old, e->mode2);
                    if (e->plr_ms < 0) {
                        e->plr_ms = old;
                    }
                    e->stuck_count = 0;
                    e->collided = 0;
                }
                GetMilestonePos(e->plr_ms, mspos);
                lbl_80344720 = get_yaw(mspos, &e->objgrp.worldmat[3][0]);
            }
            if (e->dead_end > 0) {
                e->dead_end -= gFrameTicks;
            }
            if (e->dead_end < 1) {
                if (e->plr_ms < 0) {
                    if (e->area == 1) {
                        s16 c = e->collided;
                        cand = lbl_80344720;
                        if (e->route == 0) {
                            e->route = fn_8004CE38(e);
                        }
                        if (e->route < 1) {
                            cand = cand - lbl_8011C064[c];
                        } else {
                            cand = cand + lbl_8011C064[c];
                        }
                    } else if (e->coll_ip == NULL && e->coll_enenum < 0) {
                        cand = lbl_80344720;
                    } else if (e->route < 1) {
                        cand = e->ang - lbl_8011C064[e->collided];
                    } else {
                        cand = e->ang + lbl_8011C064[e->collided];
                    }
                } else {
                    s16 c = e->collided;
                    cand = lbl_80344720;
                    if (e->route == 0) {
                        f32 mp[3];
                        f32 dx;
                        f32 dz;
                        f32 a;
                        f32 d1;
                        f32 d2;
                        GetMilestonePos(e->plr_ms, mp);
                        dx = e->objgrp.worldmat[3][0] - mp[0];
                        dz = e->objgrp.worldmat[3][2] - mp[2];
                        a = (f32)(lbl_80346920 + e->pyr[1]);
                        if (a > lbl_80346840) {
                            a = a - lbl_80346848;
                        } else if (a <= lbl_80346850) {
                            a = lbl_80346848 + a;
                        }
                        d1 = fqdist((f32)(sin(a) + dx), (f32)(cos(a) + dz));
                        a = (f32)(e->pyr[1] - lbl_80346920);
                        if (a > lbl_80346840) {
                            a = a - lbl_80346848;
                        } else if (a <= lbl_80346850) {
                            a = lbl_80346848 + a;
                        }
                        d2 = fqdist((f32)(sin(a) + dx), (f32)(cos(a) + dz));
                        if (d1 < d2) {
                            e->route = 1;
                        } else {
                            e->route = -1;
                        }
                    }
                    if (e->route < 1) {
                        cand = cand - lbl_8011C064[c];
                    } else {
                        cand = cand + lbl_8011C064[c];
                    }
                }
                if (cand > lbl_80346840) {
                    cand = cand - lbl_80346848;
                } else if (cand <= lbl_80346850) {
                    cand = lbl_80346848 + cand;
                }
                probe[0] = e->objgrp.worldmat[3][0];
                probe[2] = e->objgrp.worldmat[3][2];
                probe[1] = (f32)(e->objgrp.worldmat[3][1] + lbl_80346858
                                 + e->rad);
                probe[0] += speed * sin(cand);
                probe[2] += speed * cos(cand);
                {
                    f32 d0 = fabsf_(e->ang - e->angbak);
                    f32 d1;
                    if ((d0 > lbl_803468E0
                         && (d1 = fabsf_(cand - e->angbak),
                             d1 <= lbl_803468E0))
                        || fn_8004C8CC(probe, index) == 0) {
                        blocked = 1;
                        e->stuck_count++;
                    } else {
                        e->stuck_count = 0;
                    }
                }
                if (e->stuck_count > 10) {
                    cand = lbl_80344720;
                    e->ang = lbl_80344720;
                }
                if (!blocked) {
                    e->angbak = e->ang;
                    e->ang = cand;
                }
            } else {
                cand = e->ang;
            }
            fn_8004CD1C(e, 1.0f, cand);
            if (!blocked || cand == lbl_80344720) {
                e->pyr[1] = turn_enemy_ang(e, cand);
            }
            do_enemy_move(index);
        }
    } else if (e->mode1 < 1 && e->mode1 > -1) {
        /* -- mode 0: straight pursuit; hop onto the player's milestone list
         * once we have been bumped enough times -- */
        if (e->coll_pnum < 0) {
            skip = 0;
        } else {
            if (e->algorithm != e->prev_ai) {
                fn_80050394(index);
            }
            if (e->closest < 0) {
                face = e->ang;
            } else if (*(s16*)&gPlayers[e->closest][647] > 2) {
                face = get_yaw(&gPlayers[e->closest][633],
                               &e->objgrp.worldmat[3][0]);
            } else {
                face = get_yaw(&gPlayers[e->closest][17],
                               &e->objgrp.worldmat[3][0]);
            }
            e->ang = face;
            e->dead_end = 0;
            fn_8004CD1C(e, 1.0f, e->ang);
            e->pyr[1] = turn_enemy_ang(e, e->ang);
            do_enemy_move(index);
            skip = -1;
        }
        if (skip == 0) {
            if (e->algorithm != e->prev_ai) {
                fn_80050394(index);
            }
            if (e->collided < 5) {
                if (e->closest < 0) {
                    face = e->ang;
                } else if (*(s16*)&gPlayers[e->closest][647] > 2) {
                    face = get_yaw(&gPlayers[e->closest][633],
                                   &e->objgrp.worldmat[3][0]);
                } else {
                    face = get_yaw(&gPlayers[e->closest][17],
                                   &e->objgrp.worldmat[3][0]);
                }
                lbl_80344720 = face;
            } else {
                e->stuck_count = 0;
                e->plr_ms = *(s32*)&gPlayers[e->closest][653];
                if (e->plr_ms >= 0) {
                    e->mode1++;
                    e->mode2 = 0;
                    e->collided = 0;
                }
            }
            if (e->dead_end > 0) {
                e->dead_end -= gFrameTicks;
            }
            if (e->dead_end < 1) {
                if (e->area == 1) {
                    s16 c = e->collided;
                    cand = lbl_80344720;
                    if (e->route == 0) {
                        e->route = fn_8004CE38(e);
                    }
                    if (e->route < 1) {
                        cand = cand - lbl_8011C064[c];
                    } else {
                        cand = cand + lbl_8011C064[c];
                    }
                } else if (e->coll_ip == NULL && e->coll_enenum < 0) {
                    cand = lbl_80344720;
                } else if (e->route < 1) {
                    cand = e->ang - lbl_8011C064[e->collided];
                } else {
                    cand = e->ang + lbl_8011C064[e->collided];
                }
                if (cand > lbl_80346840) {
                    cand = cand - lbl_80346848;
                } else if (cand <= lbl_80346850) {
                    cand = lbl_80346848 + cand;
                }
                probe[0] = e->objgrp.worldmat[3][0];
                probe[2] = e->objgrp.worldmat[3][2];
                probe[1] = (f32)(e->objgrp.worldmat[3][1] + lbl_80346858
                                 + e->rad);
                probe[0] += speed * sin(cand);
                probe[2] += speed * cos(cand);
                {
                    f32 d0 = fabsf_(e->ang - e->angbak);
                    f32 d1;
                    if ((d0 > lbl_803468E0
                         && (d1 = fabsf_(cand - e->angbak),
                             d1 <= lbl_803468E0))
                        || fn_8004C8CC(probe, index) == 0) {
                        blocked = 1;
                        e->stuck_count++;
                    } else {
                        e->stuck_count = 0;
                    }
                }
                if (e->stuck_count > 10) {
                    cand = lbl_80344720;
                    e->ang = lbl_80344720;
                }
                if (!blocked) {
                    e->angbak = e->ang;
                    e->ang = cand;
                }
            } else {
                cand = e->ang;
            }
            fn_8004CD1C(e, 1.0f, cand);
            if (!blocked || cand == lbl_80344720) {
                e->pyr[1] = turn_enemy_ang(e, cand);
            }
            do_enemy_move(index);
        }
    } else {
        /* -- mode 2+: follow the player's recorded milestone trail -- */
        if (e->algorithm != e->prev_ai) {
            fn_80050394(index);
        }
        if (e->plr_ms < 0) {
            if (e->stuck_count < 5) {
                if (e->closest < 0) {
                    face = e->ang;
                } else if (*(s16*)&gPlayers[e->closest][647] > 2) {
                    face = get_yaw(&gPlayers[e->closest][633],
                                   &e->objgrp.worldmat[3][0]);
                } else {
                    face = get_yaw(&gPlayers[e->closest][17],
                                   &e->objgrp.worldmat[3][0]);
                }
                lbl_80344720 = face;
            } else {
                e->plr_ms =
                    *((s32*)&gPlayers[e->closest][653] + e->ms_idx);
                if (e->plr_ms < 0) {
                    if (e->closest < 0) {
                        face = e->ang;
                    } else if (*(s16*)&gPlayers[e->closest][647] > 2) {
                        face = get_yaw(&gPlayers[e->closest][633],
                                       &e->objgrp.worldmat[3][0]);
                    } else {
                        face = get_yaw(&gPlayers[e->closest][17],
                                       &e->objgrp.worldmat[3][0]);
                    }
                    lbl_80344720 = face;
                } else {
                    f32 mspos[3];
                    GetMilestonePos(e->plr_ms, mspos);
                    lbl_80344720 =
                        get_yaw(mspos, &e->objgrp.worldmat[3][0]);
                }
            }
        } else if (e->stuck_count < 5) {
            f32 mspos[3];
            GetMilestonePos(e->plr_ms, mspos);
            lbl_80344720 = get_yaw(mspos, &e->objgrp.worldmat[3][0]);
        } else {
            e->ms_idx++;
            if (e->max_msidx < e->ms_idx
                || *((s32*)&gPlayers[e->closest][653] + e->ms_idx) < 0) {
                e->ms_idx = 0;
                e->max_msidx = 4;
                e->plr_ms = -1;
                if (e->closest < 0) {
                    face = e->ang;
                } else if (*(s16*)&gPlayers[e->closest][647] > 2) {
                    face = get_yaw(&gPlayers[e->closest][633],
                                   &e->objgrp.worldmat[3][0]);
                } else {
                    face = get_yaw(&gPlayers[e->closest][17],
                                   &e->objgrp.worldmat[3][0]);
                }
                lbl_80344720 = face;
            } else {
                e->plr_ms =
                    *((s32*)&gPlayers[e->closest][653] + e->ms_idx);
                e->stuck_count = 0;
            }
        }
        if (e->dead_end > 0) {
            e->dead_end -= gFrameTicks;
        }
        if (e->dead_end < 1) {
            if (e->plr_ms < 0) {
                if (e->area == 1) {
                    s16 c = e->collided;
                    cand = lbl_80344720;
                    if (e->route == 0) {
                        e->route = fn_8004CE38(e);
                    }
                    if (e->route < 1) {
                        cand = cand - lbl_8011C064[c];
                    } else {
                        cand = cand + lbl_8011C064[c];
                    }
                } else if (e->coll_ip == NULL && e->coll_enenum < 0) {
                    cand = lbl_80344720;
                } else if (e->route < 1) {
                    cand = e->ang - lbl_8011C064[e->collided];
                } else {
                    cand = e->ang + lbl_8011C064[e->collided];
                }
            } else {
                s16 c = e->collided;
                cand = lbl_80344720;
                if (e->route == 0) {
                    f32 mp[3];
                    f32 dx;
                    f32 dz;
                    f32 a;
                    f32 d1;
                    f32 d2;
                    GetMilestonePos(e->plr_ms, mp);
                    dx = e->objgrp.worldmat[3][0] - mp[0];
                    dz = e->objgrp.worldmat[3][2] - mp[2];
                    a = (f32)(lbl_80346920 + e->pyr[1]);
                    if (a > lbl_80346840) {
                        a = a - lbl_80346848;
                    } else if (a <= lbl_80346850) {
                        a = lbl_80346848 + a;
                    }
                    d1 = fqdist((f32)(sin(a) + dx), (f32)(cos(a) + dz));
                    a = (f32)(e->pyr[1] - lbl_80346920);
                    if (a > lbl_80346840) {
                        a = a - lbl_80346848;
                    } else if (a <= lbl_80346850) {
                        a = lbl_80346848 + a;
                    }
                    d2 = fqdist((f32)(sin(a) + dx), (f32)(cos(a) + dz));
                    if (d1 < d2) {
                        e->route = 1;
                    } else {
                        e->route = -1;
                    }
                }
                if (e->route < 1) {
                    cand = cand - lbl_8011C064[c];
                } else {
                    cand = cand + lbl_8011C064[c];
                }
            }
            if (cand > lbl_80346840) {
                cand = cand - lbl_80346848;
            } else if (cand <= lbl_80346850) {
                cand = lbl_80346848 + cand;
            }
            probe[0] = e->objgrp.worldmat[3][0];
            probe[2] = e->objgrp.worldmat[3][2];
            probe[1] = (f32)(e->objgrp.worldmat[3][1] + lbl_80346858
                             + e->rad);
            probe[0] += speed * sin(cand);
            probe[2] += speed * cos(cand);
            if ((fabsf_(e->ang - e->angbak) > lbl_803468E0
                 && fabsf_(cand - e->angbak) <= lbl_803468E0)
                || fn_8004C8CC(probe, index) == 0) {
                blocked = 1;
                e->stuck_count++;
            } else {
                e->stuck_count = 0;
            }
            if (e->stuck_count > 10) {
                cand = lbl_80344720;
                e->ang = lbl_80344720;
            }
            if (!blocked) {
                e->angbak = e->ang;
                e->ang = cand;
            }
        } else {
            cand = e->ang;
        }
        fn_8004CD1C(e, 1.0f, cand);
        if (!blocked || cand == lbl_80344720) {
            e->pyr[1] = turn_enemy_ang(e, cand);
        }
        do_enemy_move(index);
    }
}


/* move_logic12 @0x80049A1C (state 12, maggot-egg tether).  Shares the IT-flee /
 * chase gate, then runs a small generator-egg state machine: snap to the dest,
 * flag the egg, and hatch back when the egg reports ready. */
void move_logic12(s32 index)
{
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608);
    struct item* gen = *(struct item**)((u8*)lbl_80250E00 + index * 916 + 4264);
    s32 it = lbl_80344748;
    s32 flee;
    f32 a;
    u8 unused[16];

    if (it < 0) {
        flee = 0;
    } else {
        Enemy* other = (Enemy*)((u8*)lbl_80250E00 + it * 916 + 3608);
        if (other->state != 1) {
            flee = 0;
        } else if (other->actual_dist > e->sight) {
            flee = 0;
        } else {
            f32 dy = other->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
            f32 dx = other->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
            f32 dz = other->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
            if (index != it && e->birth_style == 0 && e->dead_end <= 0
                && dy * dy + dx * dx + dz * dz < 100.0) {
                flee = -1;
            } else {
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
        fn_80050394(index);
    }
    if (e->closest >= 0) {
        if (*(s16*)&gPlayers[e->closest][647] > 2) {
            a = get_yaw(&gPlayers[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            a = get_yaw(&gPlayers[e->closest][17], &e->objgrp.worldmat[3][0]);
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
            e->dest[0] = 0.0f;
            e->dest[1] = 0.0f;
            e->dest[2] = 0.0f;
            e->mode1 = 2;
        }
        break;
    default:
        e->mode1 = 2;
        break;
    }
}

/* move_logic13 @0x80049C70 (state 13, zombie chain-follower).  Follows its
 * prev_enemy link toward the chain head; if the debug flag is set it dumps the
 * prev/next indices.  Each frame it faces its parent, measures the gap (inline
 * sqrt), and after 180 frames "lost" (parent too far) snaps the whole downstream
 * chain to the idle algorithm and cuts itself loose. */
void move_logic13(s32 index)
{
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608);
    struct item* gen = e->generator;
    s32 it = lbl_80344748;
    s32 flee;

    if (it < 0) {
        flee = 0;
    } else {
        Enemy* other = (Enemy*)((u8*)lbl_80250E00 + it * 916 + 3608);
        if (other->state != 1) {
            flee = 0;
        } else if (other->actual_dist > e->sight) {
            flee = 0;
        } else {
            f32 dy = other->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
            f32 dx = other->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
            f32 dz = other->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
            if (index != it && e->birth_style == 0 && e->dead_end <= 0
                && dy * dy + dx * dx + dz * dz < 100.0) {
                flee = -1;
            } else {
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
    if (sFlags & 0x10) {
        if (index == e->prev_enemy) {
            ErrorPrintf("E%02X==PREV", index);
        }
        if (index == e->next_enemy) {
            ErrorPrintf("E%02X==NEXT", index);
        }
        if (e->prev_enemy >= 0 && e->prev_enemy == e->next_enemy) {
            ErrorPrintf("E%02X: prev==next (%02X)", index, e->prev_enemy);
        }
    }
    if (gen == 0 || e->prev_enemy < 0) {
        e->algorithm = 7;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        fn_80050394(index);
    }
    {
        Enemy* prev = (Enemy*)((u8*)lbl_80250E00 + e->prev_enemy * 916 + 3608);
        f32 dy;
        f32 dx;
        f32 dz;
        f32 dist2;

        e->ang = get_yaw(&prev->objgrp.worldmat[3][0], &e->objgrp.worldmat[3][0]);
        fn_8004CD1C(e, 1.0f, e->ang);
        e->pyr[1] = turn_enemy_ang(e, e->ang);
        do_enemy_move(index);
        dy = prev->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
        dx = prev->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
        dz = prev->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
        dist2 = dy * dy + dx * dx + dz * dz;
        if (dist2 > 0.0f) {
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
                s32 n = e->prev_enemy;
                Enemy* p = (Enemy*)((u8*)lbl_80250E00 + n * 916 + 3608);
                do {
                    p->algorithm = 7;
                    p->old_ai = 7;
                    p->next_enemy = -1;
                    n = p->prev_enemy;
                    p->prev_enemy = -1;
                    p = (Enemy*)((u8*)lbl_80250E00 + n * 916 + 3608);
                } while (n >= 0);
                e->prev_enemy = -1;
            }
        }
    }
}

/* move_logic14 @0x80049FD4 (state 14, plague zig-zag skirmisher).  If a player is
 * within 8 units it switches to the chase algorithm; otherwise it strafes: swing
 * the heading +/-pi/2 on a count timer, and once mode1 has built up (and the
 * drift has opened past pi/2) re-seed the strafe with a flag2-scaled offset. */
void move_logic14(s32 index)
{
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608);
    s32 it = lbl_80344748;
    s32 flee;
    f32 face;
    f32 drift;
    f32 diff;

    if (it < 0) {
        flee = 0;
    } else {
        Enemy* other = (Enemy*)((u8*)lbl_80250E00 + it * 916 + 3608);
        if (other->state != 1) {
            flee = 0;
        } else if (other->actual_dist > e->sight) {
            flee = 0;
        } else {
            f32 dy = other->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
            f32 dx = other->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
            f32 dz = other->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
            if (index != it && e->birth_style == 0 && e->dead_end <= 0
                && dy * dy + dx * dx + dz * dz < 100.0) {
                flee = -1;
            } else {
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
    if (e->closest >= 0 && e->close_dist <= 8.0) {
        e->algorithm = 0;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        fn_80050394(index);
    }
    if (e->closest < 0) {
        face = e->ang;
    } else if (*(s16*)&gPlayers[e->closest][647] > 2) {
        face = get_yaw(&gPlayers[e->closest][633], &e->objgrp.worldmat[3][0]);
    } else {
        face = get_yaw(&gPlayers[e->closest][17], &e->objgrp.worldmat[3][0]);
    }
    lbl_80344720 = face;
    if ((e->count -= gFrameTicks) <= 0) {
        e->flag1 = -e->flag1;
        if (e->flag1 > 0) {
            e->ang = e->ang + 1.570796327;
        } else {
            e->ang = e->ang - 1.570796327;
        }
        {
            f64 a = e->ang;
            if (a > 3.141592654) {
                a -= 6.283185308;
            } else if (a <= -3.141592654) {
                a = 6.283185308 + a;
            }
            e->ang = a;
        }
        e->count += 45;
        e->mode1++;
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    fn_8004CD1C(e, 1.0f, e->ang);
    do_enemy_move(index);
    diff = lbl_80344720 - e->ang;
    if (diff > 3.141592654) {
        drift = diff - 6.283185308;
    } else if (diff <= -3.141592654) {
        drift = 6.283185308 + diff;
    } else {
        drift = diff;
    }
    if (e->counter1 <= 0
        && (e->dead_end > 0
            || (e->mode1 >= 4 && fabsf_(drift) > 1.570796327))) {
        f32 scale = (f32)(0.26179938783333334 * (f64)e->flag2 + 0.7853981635);
        if (e->mode1 > 3) {
            e->flag1 = -e->flag1;
        }
        e->dead_end = 0;
        e->ang = get_yaw(&gPlayers[e->closest][17], &e->objgrp.worldmat[3][0]);
        if (e->flag1 > 0) {
            e->ang = e->ang + scale;
        } else {
            e->ang = e->ang - scale;
        }
        {
            f64 a = e->ang;
            if (a > 3.141592654) {
                a -= 6.283185308;
            } else if (a <= -3.141592654) {
                a = 6.283185308 + a;
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
void move_logic15(s32 index)
{
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608);
    s32 it = lbl_80344748;
    s32 flee;

    if (it < 0) {
        flee = 0;
    } else {
        Enemy* other = (Enemy*)((u8*)lbl_80250E00 + it * 916 + 3608);
        if (other->state != 1) {
            flee = 0;
        } else if (other->actual_dist > e->sight) {
            flee = 0;
        } else {
            f32 dy = other->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
            f32 dx = other->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
            f32 dz = other->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
            if (index != it && e->birth_style == 0 && e->dead_end <= 0
                && dy * dy + dx * dx + dz * dz < 100.0) {
                flee = -1;
            } else {
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
        f32 d = fqdist(gPlayers[e->closest][17] - e->objgrp.worldmat[3][0],
                            gPlayers[e->closest][19] - e->objgrp.worldmat[3][2]);
        if (d <= 0.8 * e->sight) {
            e->algorithm = 0;
            do_ai(index);
            e->mode1 = 0;
            return;
        }
    }
    if (e->algorithm != e->prev_ai) {
        fn_80050394(index);
    }
    {
        s32 mode = e->mode1;
    if (mode != 1) {
        s32 best_idx = -1;
        f32 best_dist = 100000.0;
        u8* n = sLookoutParams;
        s32 i;

        if (mode > 0 || mode < 0) {
            goto move;
        }
        for (i = 0; i < sNumLookoutParams; i++) {
            if (*(s16*)(n + 0x68) >= 0) {
                f32 dy = *(f32*)(n + 0x34) - e->objgrp.worldmat[3][1];
                f32 dx = *(f32*)(n + 0x30) - e->objgrp.worldmat[3][0];
                f32 dz = *(f32*)(n + 0x38) - e->objgrp.worldmat[3][2];
                f32 d = dy * dy + dx * dx + dz * dz;
                if (d > 0.0f) {
                    volatile f32 tmp;
                    f64 y = __frsqrte(d);
                    y = 0.5 * y * (3.0 - y * y * d);
                    y = 0.5 * y * (3.0 - y * y * d);
                    y = 0.5 * y * (3.0 - y * y * d);
                    tmp = (f32)(d * (0.5 * y * (3.0 - y * y * d)));
                    d = tmp;
                }
                if (d < best_dist) {
                    best_dist = d;
                    best_idx = i;
                }
            }
            n += 108;
        }
        e->flag1 = best_idx;
        e->mode1 = 1;
    }
    {
        u8* n = sLookoutParams + e->flag1 * 108;
        f32 dy;
        f32 dx;
        f32 dz;

        e->ang = get_yaw((f32*)(n + 0x30), &e->objgrp.worldmat[3][0]);
        dy = *(f32*)(n + 0x34) - e->objgrp.worldmat[3][1];
        dx = *(f32*)(n + 0x30) - e->objgrp.worldmat[3][0];
        dz = *(f32*)(n + 0x38) - e->objgrp.worldmat[3][2];
        if (fabsf_(dy) < 4.0 && fqdist(dx, dz) < 1.0) {
            e->flag1 = *(s16*)(n + 0x68);
        }
    }
    }
move:
    fn_8004CD1C(e, 1.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* move_logic16 @0x8004A78C (state 16, ice leap-attacker).  Faces the target, and
 * when the player is at a shallow height difference it arms (flag1) inside 0.6*
 * sight; once armed and inside 0.8*sight it charges a leap speed off lbl_8011BF60,
 * then fires a run-attack action, aiming the leap 180deg + a ramp offset. */
void move_logic16(s32 index)
{
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608);
    s32 dend = *(s32*)((u8*)lbl_80250E00 + index * 916 + 4464);
    f32 leapspeed = 0.0f;
    s32 it;
    s32 flee;
    s32 stuck;
    f32 a;

    if (dend > 0) {
        stuck = 1;
    } else {
        stuck = 0;
    }
    it = lbl_80344748;
    if (it < 0) {
        flee = 0;
    } else {
        Enemy* other = (Enemy*)((u8*)lbl_80250E00 + it * 916 + 3608);
        if (other->state != 1) {
            flee = 0;
        } else if (other->actual_dist > e->sight) {
            flee = 0;
        } else {
            f32 dy = other->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
            f32 dx = other->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
            f32 dz = other->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
            if (index != it && e->birth_style == 0 && dend <= 0
                && dy * dy + dx * dx + dz * dz < 100.0) {
                flee = -1;
            } else {
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
        fn_80050394(index);
    }
    if (e->closest < 0) {
        a = e->ang;
    } else if (*(s16*)&gPlayers[e->closest][647] > 2) {
        a = get_yaw(&gPlayers[e->closest][633], &e->objgrp.worldmat[3][0]);
    } else {
        a = get_yaw(&gPlayers[e->closest][17], &e->objgrp.worldmat[3][0]);
    }
    e->ang = a;
    if (e->closest >= 0) {
        f32 dvert = e->objgrp.worldmat[3][1] - gPlayers[e->closest][18];
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
                    fn_8004CD1C(e, 0.8f, la);
                }
                e->dead_end = 0;
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
    Enemy* e = &gEnemies[index];
    f32 leapspeed = 0.0f;
    s32 dend = e->dead_end;
    s32 stuck;
    s16 sVar1;
    f32 a;

    if (dend > 0) {
        stuck = 1;
    } else {
        stuck = 0;
    }
    if (e->algorithm != e->prev_ai) {
        fn_80050394(index);
    }
    if (e->closest < 0) {
        a = e->ang;
    } else if (*(s16*)&gPlayers[e->closest][647] > 2) {
        a = get_yaw(&gPlayers[e->closest][633], &e->objgrp.worldmat[3][0]);
    } else {
        a = get_yaw(&gPlayers[e->closest][17], &e->objgrp.worldmat[3][0]);
    }
    e->ang = a;
    sVar1 = e->mode1;
    if (sVar1 == 1) {
        goto crouch;
    }
    if (sVar1 < 1 || sVar1 > 2) {
        if (e->closest >= 0 && e->actual_dist <= e->sight) {
            e->mode1++;
            e->flag1 = 60;
            e->flag2 = 0;
        }
        goto move;
    }
    goto leap;
crouch:
    if (e->closest >= 0 && e->action != 4
        && (e->flag1 -= gFrameTicks) <= 0) {
        RequestEnemyAction(e, 9);
    }
    if (e->action != 4) {
        goto move;
    }
    e->mode1++;
    fn_8009DD6C(&e->objgrp.attn_pos[0]);
leap:
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
            if (av > 3.141592654) {
                av -= 6.283185308;
            } else if (av <= -3.141592654) {
                av = 6.283185308 + av;
            }
            e->ang = av;
        }
    }
    if (e->dead_end <= 0
        || fabsf_(e->ang - e->anghit) >= 0.10471975513333334) {
        e->dead_end = 0;
        fn_8004CD1C(e, 1.5f, e->ang);
    }
move:
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
    if (e->moved != 0) {
        e->dead_end = 0;
    }
    if ((f64)e->flag2 >= 240.0 || e->coll_pnum >= 0) {
        lbl_80344748 = -1;
        damage_enemy(e, 999.0f, -2, 1, 0, 0, 0);
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
        fn_80050394(index);
    }
    if (e->closest >= 0) {
        if (*(s16*)&gPlayers[e->closest][647] > 2) {
            a = get_yaw(&gPlayers[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            a = get_yaw(&gPlayers[e->closest][17], &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    mode = e->mode1;
    switch (mode) {
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
        fn_8004CD1C(e, 1.0f, e->ang);
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
void move_logic20(s32 index)
{
    Enemy* e = &gEnemies[index];
    f32 speed = ((f32*)lbl_80250E40)[e->type];
    s32 found = 0;
    f32 cand;
    f32 face;
    f32 probe[3];

    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        fn_80050394(index);
    }
    if (e->closest < 0) {
        face = e->ang;
    } else if (*(s16*)&gPlayers[e->closest][647] > 2) {
        face = get_yaw(&gPlayers[e->closest][633], &e->objgrp.worldmat[3][0]);
    } else {
        face = get_yaw(&gPlayers[e->closest][17], &e->objgrp.worldmat[3][0]);
    }
    lbl_80344720 = face;
    if (e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    if (e->dead_end <= 0) {
        if (e->coll_pnum < 0) {
            if (e->area == 1) {
                s32 col = e->collided;
                cand = 3.141592654 + lbl_80344720;
                if (e->route == 0) {
                    e->route = fn_8004CE38(e);
                }
                if (e->route > 0) {
                    cand = cand + lbl_8011C044[col];
                } else {
                    cand = cand - lbl_8011C044[col];
                }
            } else if (e->coll_ip == 0 && e->coll_enenum < 0) {
                cand = 3.141592654 + lbl_80344720;
            } else {
                cand = e->ang;
                if (e->route > 0) {
                    cand = cand + lbl_8011C044[e->collided];
                } else {
                    cand = cand - lbl_8011C044[e->collided];
                }
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
        if ((fabsf_(e->ang - e->angbak) > 0.034906585044444445
             && fabsf_(cand - e->angbak) <= 0.034906585044444445)
            || fn_8004C8CC(probe, index) == 0) {
            found = 1;
            e->stuck_count++;
        } else {
            e->stuck_count = 0;
        }
        if (e->stuck_count > 10) {
            f32 t = 3.141592654 + lbl_80344720;
            f64 a = t;
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
    fn_8004CD1C(e, 2.0f, cand);
    {
        f64 fn = 3.141592654 + lbl_80344720;
        if (fn > 3.141592654) {
            fn = fn - 6.283185308;
        } else if (fn <= -3.141592654) {
            fn = 6.283185308 + fn;
        }
        if (found == 0 || cand == fn) {
            e->pyr[1] = turn_enemy_ang(e, cand);
        }
    }
    do_enemy_move(index);
}

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
        fn_80050394(index);
    }
    if (e->dead_end > 0 && (c = e->counter1) < 8) {
        e->counter1 = c + 1;
        spd = lbl_8011BF60[c];
        e->dead_end = 0;
    }
    if (e->closest >= 0) {
        if (*(s16*)&gPlayers[e->closest][647] > 2) {
            face = get_yaw(&gPlayers[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            face = get_yaw(&gPlayers[e->closest][17], &e->objgrp.worldmat[3][0]);
        }
    } else {
        face = e->ang;
    }
    e->ang = spd + (3.141592654 + face);
    {
        f64 a = e->ang;
        if (a > 3.141592654) {
            a -= 6.283185308;
        } else if (a <= -3.141592654) {
            a = 6.283185308 + a;
        }
        e->ang = a;
    }
    fn_8004CD1C(e, 2.0f, e->ang);
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
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608);
    s32 it = lbl_80344748;
    s32 flee;
    s32 mode1;
    f32 buf1[19];
    f32 buf2[3];

    if (it < 0) {
        flee = 0;
    } else {
        Enemy* other = (Enemy*)((u8*)lbl_80250E00 + it * 916 + 3608);
        if (other->state != 1) {
            flee = 0;
        } else if (other->actual_dist > e->sight) {
            flee = 0;
        } else {
            f32 dy = other->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
            f32 dx = other->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
            f32 dz = other->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
            if (index != it && e->birth_style == 0 && e->dead_end <= 0
                && dy * dy + dx * dx + dz * dz < 100.0) {
                flee = -1;
            } else {
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
        fn_80050394(index);
    }
    mode1 = e->mode1;
    if (mode1 == 0) {
        s32 best_idx = -1;
        f32 best_dist = 100000.0;
        u8* node = sMilestones;
        s32 i;

        for (i = 0; i < sNumMilestones; i++) {
            f32 dy = e->objgrp.worldmat[3][1] - *(f32*)(node + 0x34);
            f32 dx = e->objgrp.worldmat[3][0] - *(f32*)(node + 0x30);
            f32 dz = e->objgrp.worldmat[3][2] - *(f32*)(node + 0x38);
            f32 d = dy * dy + dx * dx + dz * dz;
            if (d > 0.0f) {
                volatile f32 tmp;
                f64 y = __frsqrte(d);
                y = 0.5 * y * (3.0 - y * y * d);
                y = 0.5 * y * (3.0 - y * y * d);
                y = 0.5 * y * (3.0 - y * y * d);
                tmp = (f32)(d * (0.5 * y * (3.0 - y * y * d)));
                d = tmp;
            }
            if (d < best_dist) {
                best_dist = d;
                best_idx = i;
            }
            node += 104;
        }
        e->flag1 = best_idx;
        GetMilestonePos(e->flag1, buf1);
        e->ang = get_yaw(buf1, &e->objgrp.worldmat[3][0]);
        e->mode1 = 1;
    } else if (mode1 < 0 && mode1 > -2) {
        if (e->closest < 0) {
            e->ang = e->ang;
        } else if (*(s16*)&gPlayers[e->closest][647] > 2) {
            e->ang = get_yaw(&gPlayers[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            e->ang = get_yaw(&gPlayers[e->closest][17], &e->objgrp.worldmat[3][0]);
        }
        goto move;
    }
    {
        u8* node = sMilestones + e->flag1 * 104;
        f32 dist = fqdist(*(f32*)(node + 0x30) - e->objgrp.worldmat[3][0],
                               *(f32*)(node + 0x38) - e->objgrp.worldmat[3][2]);
        if (dist <= 1.5) {
            s32 old = e->flag1;
            e->flag1 = fn_800511D0(old, 0.1745329201221466f);
            if (e->flag1 == old) {
                e->mode1 = -1;
            } else {
                e->mode1++;
            }
        }
        GetMilestonePos(e->flag1, buf2);
        e->ang = get_yaw(buf2, &e->objgrp.worldmat[3][0]);
    }
move:
    if (e->mode1 >= 0) {
        fn_8004CD1C(e, 1.0f, e->ang);
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
        fn_80050394(index);
    }
    if (e->closest >= 0) {
        if (*(s16*)&gPlayers[e->closest][647] > 2) {
            a = get_yaw(&gPlayers[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            a = get_yaw(&gPlayers[e->closest][17], &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    if (e->closest >= 0) {
        f32 dy = e->objgrp.worldmat[3][1] - gPlayers[e->closest][18];
        if (e->visactive != 0 && e->actual_dist <= e->sight
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
        fn_80050394(index);
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
        f64 a = e->ang;
        if (a > 3.141592654) {
            a -= 6.283185308;
        } else if (a <= -3.141592654) {
            a = 6.283185308 + a;
        }
        e->ang = a;
    }
    fn_8004CD1C(e, 2.0f, e->ang);
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
        fn_80050394(index);
    }
    if (e->closest >= 0) {
        if (*(s16*)&gPlayers[e->closest][647] > 2) {
            a = get_yaw(&gPlayers[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            a = get_yaw(&gPlayers[e->closest][17], &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    if (e->closest >= 0) {
        f32 dy = e->objgrp.worldmat[3][1] - gPlayers[e->closest][18];
        if (e->visactive != 0 && e->actual_dist <= e->sight
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
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608);
    s32 dend = *(s32*)((u8*)lbl_80250E00 + index * 916 + 4464);
    f32 leapspeed = 0.0f;
    s32 it;
    s32 flee;
    s32 stuck;
    f32 a;

    if (dend > 0) {
        stuck = 1;
    } else {
        stuck = 0;
    }
    it = lbl_80344748;
    if (it < 0) {
        flee = 0;
    } else {
        Enemy* other = (Enemy*)((u8*)lbl_80250E00 + it * 916 + 3608);
        if (other->state != 1) {
            flee = 0;
        } else if (other->actual_dist > e->sight) {
            flee = 0;
        } else {
            f32 dy = other->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
            f32 dx = other->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
            f32 dz = other->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
            if (index != it && e->birth_style == 0 && dend <= 0
                && dy * dy + dx * dx + dz * dz < 100.0) {
                flee = -1;
            } else {
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
        fn_80050394(index);
    }
    if (e->closest < 0) {
        a = e->ang;
    } else if (*(s16*)&gPlayers[e->closest][647] > 2) {
        a = get_yaw(&gPlayers[e->closest][633], &e->objgrp.worldmat[3][0]);
    } else {
        a = get_yaw(&gPlayers[e->closest][17], &e->objgrp.worldmat[3][0]);
    }
    e->ang = a;
    if (e->closest >= 0) {
        f32 dvert = e->objgrp.worldmat[3][1] - gPlayers[e->closest][18];
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
                fn_8004CD1C(e, 0.8f, la);
                RequestEnemyAction(e, 3);
                e->dead_end = 0;
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
        } else {
            f32 dy = *(f32*)(op + 3664) - e->objgrp.worldmat[3][1];
            f32 dx = *(f32*)(op + 3660) - e->objgrp.worldmat[3][0];
            f32 dz = *(f32*)(op + 3668) - e->objgrp.worldmat[3][2];
            if (index != it && e->birth_style == 0 && e->dead_end <= 0
                && dy * dy + dx * dx + dz * dz < 100.0) {
                flee = -1;
            } else {
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
    if (e->closest >= 0 && e->actual_dist <= 6.0) {
        e->algorithm = 7;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        fn_80050394(index);
    }
    if (e->dead_end <= 0 && (e->counter1 -= gFrameTicks) <= 0) {
        s32 n = (s32)(90.0 * *(f32*)(gCurLevel + 192));
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
    spd = ((f32*)lbl_80250E40)[e->type];
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
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608);
    f32 a;
    u8 unused[16];

    if (*(s16*)((u8*)lbl_80250E00 + index * 916 + 4392)
        != *(s16*)((u8*)lbl_80250E00 + index * 916 + 4396)) {
        fn_80050394(index);
    }
    if (e->closest >= 0) {
        if (*(s16*)&gPlayers[e->closest][647] > 2) {
            a = get_yaw(&gPlayers[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            a = get_yaw(&gPlayers[e->closest][17], &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    if (e->closest >= 0) {
        e->daction = 0;
        if (e->visactive != 0) {
            s32 act = e->action;
            if (act == 12 || act == 13) {
                update_vel(e, 1.0f);
                e->flag2 = RandInt(30) + 30;
                if (e->actual_dist <= 7.5) {
                    e->attack_index = e->closest;
                }
            } else if (act == 16 || act == 17) {
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
        }
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* --- kill_enemy externs --- */
extern int sprintf(char* buf, const char* fmt, ...);
extern int toupper(int c);
extern char* fn_80057ACC(void);                     /* current-level tag string */
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

/* damage_enemy @0x8004E6F8 - SKELETON + NOTES.  Apply damage + knockback to an
 * enemy and run the on-hit FX / heal / death cascade.  Global; called from the
 * world-collision damage path and from move_logic18's "IT" catch.  572 GC insns;
 * only the state guard is reconstructed under the light-touch pass.
 *
 * Signature (from asm): s32 damage_enemy(Enemy* e, f32 amount, s32 dtype,
 *   s32 knock, s32 srcflags, s32 arg6, s32 arg7).  f1=amount is stashed to the
 *   frame, r5(dtype) stashed, r6/r7/r8 held in nonvolatiles, e->health cached in
 *   f31 (0x200).  Returns -1 when the target is already DECORATION(7)/DYING(8).
 *
 * Full-body call inventory (tools/gdl/fnasm.py game/enemy/enemy damage_enemy):
 *   SetSkinFX x4                 - hit-flash skin FX ramps
 *   SuicideExplosion x2, fn_8009DAC8 x2, fn_800945D0 x2 - death / burst FX
 *   uncouple_enemy x2, CopyMat4 x2 - detach from the generator chain, snapshot xform
 *   heal_player / do_heal_players  - life-steal heal on kill
 *   fn_8009DE88/DE5C/DF7C/DD48, MBTreeSetAmbientAdd, MBTreeClearFlags - node FX
 *   AudioPlayEvt103, msgPost, fn_80050618/8002F5D8/8005A3B8/8005A404 - sfx + msgs
 * Frame 176, saves r26-r31 + f31.
 * TODO: transcribe damage application + knockback integration + the death branch
 * (Ghidra decompile_function 0x8004E6F8). */
s32 damage_enemy(Enemy* e, f32 amount, s32 dtype, s32 knock, s32 srcflags, s32 arg6, s32 arg7)
{
    if (e->state == DECORATION) {
        return -1;
    }
    if (e->state == DYING) {
        return -1;
    }
    /* --- remaining ~560 insns of damage application / FX / death not transcribed --- */
    return 0;
}

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
    s32 t;
    char buf[32];

    if (gTriggerCameraState != 0) {
        return;
    }
    if (e->gotitem != 0) {
        item = e->gotitem;
        e->gotitem = 0;
        carried = 1;
    } else {
        t = e->type;
        switch (t) {
        case 32:
            sprintf(buf, lbl_80346A38, fn_80057ACC());
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
    s32 count = lbl_80344724;
    s32 idx = 0;
    s32 lo;
    s32 hi;
    s32 i;

    for (i = 0; i < count; i++) {
        if (*(s32*)((u8*)lbl_80250E00 + i * 4 + 0xF4) == ms) {
            break;
        }
        idx++;
    }
    if (idx >= count) {
        return -1;
    }
    lo = idx - nth;
    hi = idx + nth;
    if (lo < 0) {
        return *(s32*)((u8*)lbl_80250E00 + hi * 4 + 0xF4);
    }
    if (hi > count - 1) {
        return *(s32*)((u8*)lbl_80250E00 + lo * 4 + 0xF4);
    }
    {
        s32 m_lo = *(s32*)((u8*)lbl_80250E00 + lo * 4 + 0xF4);
        s32 m_hi = *(s32*)((u8*)lbl_80250E00 + hi * 4 + 0xF4);
        u8* pl = sMilestones + m_lo * 0x68;
        u8* ph = sMilestones + m_hi * 0x68;
        f32 dlo = *(f32*)(pl + 0x34) * *(f32*)(pl + 0x34) +
                  *(f32*)(pl + 0x30) * *(f32*)(pl + 0x30) +
                  *(f32*)(pl + 0x38) * *(f32*)(pl + 0x38);
        f32 dhi;

        if (dlo > 0.0f) {
            volatile f32 tmp;
            f64 y = __frsqrte(dlo);
            y = 0.5 * y * (3.0 - y * y * dlo);
            y = 0.5 * y * (3.0 - y * y * dlo);
            y = 0.5 * y * (3.0 - y * y * dlo);
            tmp = (f32)(dlo * (0.5 * y * (3.0 - y * y * dlo)));
            dlo = tmp;
        }
        dhi = *(f32*)(ph + 0x34) * *(f32*)(ph + 0x34) +
              *(f32*)(ph + 0x30) * *(f32*)(ph + 0x30) +
              *(f32*)(ph + 0x38) * *(f32*)(ph + 0x38);
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
extern void SetEnemyObj(s32 slot, f32* pos, s32 type, s32 level, s32 spew);
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

/* Accelerate an enemy along an angle, caching the trig pair between calls. */
void fn_8004CD1C(Enemy* enemy, f32 speed, f32 angle)
{
    if (enemy->type != gBossType && enemy->action != 1) {
        s32 action;

        if (speed >= 1.25) {
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
            typeSpeed = ((f32*)lbl_80250E40)[enemy->type];
            dx = enemy->xspd * typeSpeed;
            dz = enemy->zspd * typeSpeed;
            dx = speed * dx;
            dz = speed * dz;
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
f32 fn_8004FBC8(f32* input, f32* output, s32 direction)
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
        player = gPlayers[enemy->closest];
    } else {
        for (i = 0; i < 4; i++) {
            if (((s32*)gPlayers[i])[0xE8 / 4] == 1) {
                break;
            }
        }
        if (i < 4) {
            player = gPlayers[i];
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
    SetEnemyObj(slot, pos, type, level, spew);
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
                e->genang_offset = fn_8004FBC8(v, out, d);
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
extern s32 FloorCollide(f32* pos, s32 a, s32 b, s32 mode, f32 x, f32 y, f32 z);
extern u8 gFloorCollisionResult[]; /* 0x8023CAE0, floor Y at +0x34 */
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
    f32 half;
    void* obj;

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
        == 0) {
        return -1;
    }
    {
        f32 floorY = *(f32*)(gFloorCollisionResult + 0x34);
        f32 dy = floorY - start[1];

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
    half = (f32)(lbl_80346830 * rad);
    if (fn_8004646C(half, hht, slot, start, pos, 0, 0) >= 0) {
        return 0;
    }
    obj = fn_8005EFAC(half, start, pos, 0, 0);
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
    Enemy* enemy = gEnemies;
    s32 best_visible = 1;
    s32 index = 0;
    s32 best_index = 0;
    s32 count = gNumEnemies;
    f32 best_distance = lbl_80346820;

    for (index = 0; index < count; index++, enemy++) {
        s32 enemy_state = enemy->state;

        if (enemy_state == INACTIVE) {
            return index;
        }
        if (enemy->type != E_IT) {
            f32 distance = enemy->actual_dist;
            s32 visible = enemy->visactive;

            if (enemy_state == DYING || enemy_state == SLEEP ||
                enemy->birth_style != 0) {
                distance *= lbl_80346878;
            } else if (visible == 0) {
                distance += lbl_80346A20;
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
        f32 reset_distance = lbl_803468F0;

        gEnemies[best_index].close_dist = reset_distance;
        gEnemies[best_index].actual_dist = reset_distance;
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
void EnemyWorldDamage(Enemy* e, void* wobj, f32* oldpos, f32* hitnrm)
{
    u32 flags;
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
    switch (flags & 0xF0000) {
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

    p = (u8*)&gPlayers[*(s16*)((u8*)e + 628)];
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
extern void fn_8004DC2C(Enemy* e);
extern void fn_8004DF58(Enemy* e);
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

void do_enemies(void)
{
    u8* pool = (u8*)lbl_80250E00;
    s32 shown = 0;
    s32 i;

    ProcessCritterList();
    if (gBoss398 >= 0) {
        *(s32*)(pool + gBoss398 * 0x394 + 0xECC) = 1;
    }
    if ((gGameBusy | gGameplayPauseTimer) != 0) {
        return;
    }

    if (gScriptedCameraState != 0) {
        u8* e;

        if (lbl_803447B8 == 0) {
            return;
        }
        e = pool + 0xE18;
        for (i = 0; i < gNumEnemies; i++, e += 0x394) {
            s32 type;

            if (*(s32*)(e + 0xB4) != 1) {
                continue;
            }
            type = *(s32*)e;
            if (type == gBossType) {
                continue;
            }
            if (type == 0x1D) {
                *(s32*)(e + 0xD0) = 1;
            } else if (type == 0) {
                *(s32*)(e + 0xD0) = 3;
            } else {
                *(s32*)(e + 0xD0) = 0;
            }
            if (*(u32*)(e + 0x6C) != 0) {
                *(s32*)(e + 0xCC) = DoEnemyAction((Enemy*)e);
            }
        }
        return;
    }

    {
        u8* pl = (u8*)gPlayers;

        for (i = 0; i < 4; i++, pl += 0x335C) {
            if (*(s32*)(pl + 0xE8) == 1) {
                *(s32*)(pl + 0xA24) = 0;
                *(f32*)(pl + 0xA28) = 0.0f;
            }
        }
    }

    {
        f32 rate = *(f32*)((u8*)gCurLevel + 0xB0) * (f32)gFrameTicks;

        lbl_80344718 = 0;
        for (i = 0; i < 45; i++) {
            *(f32*)(pool + 0x40 + i * 4) = rate * lbl_8011B878[i];
        }
    }

    {
        u8* e = pool + 0xE18;

        for (i = 0; i < gNumEnemies; i++, e += 0x394) {
            if (*(s32*)(e + 0xB4) == 1 && *(s16*)(e + 0x310) == 0x12 &&
                *(s16*)(e + 0x2DC) != 0) {
                if (*(s32*)(e + 0xCC) == 4 || *(s32*)(e + 0xD0) == 4) {
                    lbl_80344748 = i;
                    break;
                }
            }
        }
    }

    {
        u8* e = pool + 0xE18;

        lbl_80344740 = 0;
        for (i = 0; i < gNumEnemies; i++, e += 0x394) {
            f32 r;

            if (*(s32*)(e + 0xB4) == 0) {
                continue;
            }
            r = lbl_80346980 * *(f32*)(e + 0x238);
            *(s16*)(e + 0x2DA) =
                (s16)MBWorldSphereVisible3((f32*)(e + 0x44), r);
            r = (f32)((f64)r + lbl_80346928);
            *(s16*)(e + 0x2DC) =
                (s16)MBWorldSphereVisible3((f32*)(e + 0x44), r);
            if (*(s16*)(e + 0x2DA) != 0) {
                lbl_80344740++;
            }
        }
    }

    {
        u8* e = pool + 0xE18;

        for (i = 0; i < gNumEnemies; i++, e += 0x394) {
            s32 state;

            *(s16*)(e + 0x312) = *(s16*)(e + 0x310);
            *(s32*)(e + 0x36C) += gFrameTicks;
            if (*(f32*)(e + 0x37C) > 0.0f) {
                *(f32*)(e + 0x37C) -= gClockFrameStep;
            }
            if (*(s32*)e == 0) {
                *(s32*)(e + 0xD0) = 3;
            } else {
                *(s32*)(e + 0xD0) = 0;
            }

            state = *(s32*)(e + 0xB4);
            if (state == 1) {
                if (*(s32*)e == gBossType) {
                    shown++;
                    goto tail;
                }
                shown++;
                fn_8005A338((f32*)(e + 0x4), (f32*)(e + 0x22C),
                            (f32*)(e + 0x220));
                if (lbl_803447DC != 0) {
                    *(f32*)(e + 0x90) += gClockFrameStep;
                    fn_8004DC2C((Enemy*)e);
                    do_enemy_collide(i);
                } else if (gTriggerCameraState != 0) {
                    *(s32*)(e + 0xD0) = 0;
                    *(s32*)(e + 0xCC) = DoEnemyAction((Enemy*)e);
                    do_enemy_collide(i);
                    fn_8004DC2C((Enemy*)e);
                } else {
                    fn_800516F8(i);
                    fn_8004DF58((Enemy*)e);
                    fn_8004DC2C((Enemy*)e);
                    if (fn_8004D958(i) != 0) {
                        goto tail;
                    }
                    if (*(u32*)(e + 0x6C) != 0) {
                        *(s32*)(e + 0xCC) = DoEnemyAction((Enemy*)e);
                    }
                    *(s32*)(e + 0x390) =
                        (*(s32*)(e + 0xCC) == *(s32*)(e + 0xD0)) ? -1 : 0;
                }
                ProcessSkinFX((f32*)(e + 0x1E4), *(void**)(e + 0x64), 0);
                UpdateObjWorldMat((f32*)(e + 0x4));
                goto tail;
            } else if (state == 6) {
                fn_8005A338((f32*)(e + 0x4), (f32*)(e + 0x22C),
                            (f32*)(e + 0x220));
                shown++;
                goto tail;
            } else if (state == 8) {
                fn_8005A338((f32*)(e + 0x4), (f32*)(e + 0x22C),
                            (f32*)(e + 0x220));
                shown++;
                if (*(s32*)e == gBossType) {
                    goto sync;
                }
                if (*(s32*)e == 0x1E) {
                    s32 eff = *(s32*)(e + 0x1E0);
                    s32 alpha = *(s32*)(e + 0x388);

                    if (eff >= 0) {
                        *(s32*)(e + 0x1E0) = DeleteEffect(eff, 1);
                    }
                    if (alpha < 0xFF) {
                        MBTreeSetAlpha(*(struct mbnode**)(e + 0x64), alpha, 1);
                        *(s32*)(e + 0x388) = alpha + gFrameTicks * 4;
                        *(f32*)(e + 0x38) = (f32)(lbl_80346818 * gClockFrameStep +
                                                  *(f32*)(e + 0x38));
                        UpdateObjWorldMat((f32*)(e + 0x4));
                        goto sync;
                    }
                    if (*(s32*)(e + 0x320) != 0) {
                        if (*(s16*)(e + 0x206) == 2) {
                            msgPost(0x81, *(s32*)(e + 0x284), (f32*)(e + 0x44));
                        } else {
                            msgPost(0x83, *(s32*)(e + 0x284), (f32*)(e + 0x44));
                        }
                    }
                    kill_enemy(i);
                    goto tail;
                } else {
                    s32 cc;

                    fn_8004DC2C((Enemy*)e);
                    *(s32*)(e + 0xD0) = 0x20;
                    *(f32*)(e + 0x244) =
                        turn_enemy_ang((Enemy*)e, *(f32*)(e + 0x24C));
                    do_enemy_move(i);
                    if (*(u32*)(e + 0x6C) != 0) {
                        if (*(s32*)e != gBossType) {
                            *(s32*)(e + 0xCC) = DoEnemyAction((Enemy*)e);
                        }
                        *(s32*)(e + 0x390) =
                            (*(s32*)(e + 0xCC) == *(s32*)(e + 0xD0)) ? -1 : 0;
                    }
                    ProcessSkinFX((f32*)(e + 0x1E4), *(void**)(e + 0x64), 0);
                    cc = *(s32*)(e + 0xCC);
                    if ((cc == 0x1C || cc == 0x1D || cc == 0x20) &&
                        *(f32*)(e + 0x1E4) <= 0.0f) {
                        UpdateObjWorldMat((f32*)(e + 0x4));
                        goto sync;
                    }
                    if (*(s32*)e == 0x1D) {
                        if (RandInt(2) == 0) {
                            fn_8009FEFC(*(s16*)(e + 0x1FE));
                        } else {
                            fn_8009FEA0(*(s16*)(e + 0x1FE));
                        }
                    }
                    kill_enemy(i);
                    goto tail;
                }
            sync:
                if (*(u32*)(e + 0x1DC) != 0) {
                    u8* n = *(u8**)(e + 0x1DC);

                    *(f32*)(n + 0x30) = *(f32*)(e + 0x34);
                    *(f32*)(n + 0x34) = *(f32*)(e + 0x38);
                    *(f32*)(n + 0x38) = *(f32*)(e + 0x3C);
                }
            }

        tail:
            *(s16*)(e + 0x314) = *(s16*)(e + 0x310);
            *(s16*)(e + 0x310) = *(s16*)(e + 0x312);
            if (*(s32*)(e + 0x36C) >= *(s32*)(e + 0x368)) {
                *(s32*)(e + 0x36C) -= *(s32*)(e + 0x368);
            }
            *(f32*)(e + 0x25C) = (f32)(lbl_80346940 * *(f32*)(e + 0x25C));
            *(f32*)(e + 0x260) = (f32)(lbl_80346940 * *(f32*)(e + 0x260));
            *(f32*)(e + 0x264) = (f32)(lbl_80346940 * *(f32*)(e + 0x264));
            {
                f32 v = *(f32*)(e + 0x25C);
                *(u32*)&v &= 0x7FFFFFFF;
                if (v < lbl_80346878) {
                    *(f32*)(e + 0x25C) = 0.0f;
                }
            }
            {
                f32 v = *(f32*)(e + 0x260);
                *(u32*)&v &= 0x7FFFFFFF;
                if (v < lbl_80346878) {
                    *(f32*)(e + 0x260) = 0.0f;
                }
            }
            {
                f32 v = *(f32*)(e + 0x264);
                *(u32*)&v &= 0x7FFFFFFF;
                if (v < lbl_80346878) {
                    *(f32*)(e + 0x264) = 0.0f;
                }
            }
            if (*(f32*)(e + 0x260) > 0.0f) {
                f32 nv = *(f32*)(e + 0x260) - lbl_803469C0 * gClockFrameStep;
                *(f32*)(e + 0x260) = nv;
                if (nv < 0.0f) {
                    *(f32*)(e + 0x260) = 0.0f;
                }
            }
            if (gBossType < 0) {
                if ((f64)lbl_803447D8 == lbl_80346810) {
                    if (*(u32*)(e + 0x64) != 0) {
                        u8* n = *(u8**)(e + 0x64);
                        MBTreeSetFlags((struct mbnode*)n, 8, 0);
                        *(f32*)(n + 0x40) = lbl_803447D8;
                        *(f32*)(n + 0x44) = lbl_803447D8;
                        *(f32*)(n + 0x48) = lbl_803447D8;
                    }
                    if (*(u32*)(e + 0x1DC) != 0) {
                        u8* n = *(u8**)(e + 0x1DC);
                        MBTreeSetFlags((struct mbnode*)n, 8, 0);
                        *(f32*)(n + 0x40) = lbl_803447D8;
                        *(f32*)(n + 0x44) = lbl_803447D8;
                        *(f32*)(n + 0x48) = lbl_803447D8;
                    }
                } else {
                    if (*(u32*)(e + 0x64) != 0) {
                        u8* n = *(u8**)(e + 0x64);
                        MBTreeClearFlags((struct mbnode*)n, 8, 0);
                        *(f32*)(n + 0x40) = lbl_803468F0;
                        *(f32*)(n + 0x44) = lbl_803468F0;
                        *(f32*)(n + 0x48) = lbl_803468F0;
                    }
                    if (*(u32*)(e + 0x1DC) != 0) {
                        u8* n = *(u8**)(e + 0x1DC);
                        MBTreeClearFlags((struct mbnode*)n, 8, 0);
                        *(f32*)(n + 0x40) = lbl_803468F0;
                        *(f32*)(n + 0x44) = lbl_803468F0;
                        *(f32*)(n + 0x48) = lbl_803468F0;
                    }
                }
            }
        }
    }

    if (lbl_80344718 == 0) {
        AudioPlayEvt102();
    }
    if ((sFlags & 0x10) != 0 && (sFlags & 1) != 0) {
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
    u8* pool = (u8*)lbl_80250E00;
    s32 startNode = *(s32*)(pool + index * 0x394 + 0x10A0);
    f32 best = lbl_803468B0;
    s32 result = -1;
    s32 hint = -1;
    void* nodeCol;
    s32 node;
    f32 delta[3];
    u8 scratch[36];

    if (hitWorld == NULL && startNode < 0x10000) {
        hint = startNode;
    }
    CritterCollideStart(rad, newc, 0);
    nodeCol = CritterMoveNodeCol(rad, 0.0f, oldc, newc, scratch, -1, 2);
    if (nodeCol != NULL) {
        return *(s16*)nodeCol | 0x10000;
    }
    StartItemGrid(rad, newc);
    for (;;) {
        Enemy* other;
        s32 st;
        s32 linked;

        if (hint >= 0) {
            node = hint;
            hint = -1;
        } else {
            node = NextGridItem();
        }
        if (node < 0) {
            break;
        }
        if (node == index) {
            continue;
        }
        other = &gEnemies[node];
        st = *(s32*)((u8*)other + 0xB4);
        if (st == 0 || st == 8) {
            continue;
        }
        {
            Enemy* c = &gEnemies[index];
            s32 v;

            linked = 0;
            for (;;) {
                v = *(s32*)((u8*)c + 0x338);
                if (v < 0) {
                    break;
                }
                if (v == node) {
                    linked = -1;
                    break;
                }
                c = &gEnemies[v];
            }
        }
        if (linked != 0) {
            continue;
        }
        if ((f64)*(f32*)((u8*)other + 0x23C) <= lbl_80346868) {
            if (*(s32*)other == 0x1D) {
                continue;
            }
        }
        delta[0] = *(f32*)((u8*)other + 0x54) - newc[0];
        delta[1] = *(f32*)((u8*)other + 0x58) - newc[1];
        delta[2] = *(f32*)((u8*)other + 0x5C) - newc[2];
        {
            f32 dist = NormalVector2D(delta);

            if (dist >= best) {
                continue;
            }
            if (LineCylinderCollide((f32*)((u8*)other + 0x54),
                                    rad + *(f32*)((u8*)other + 0x238),
                                    hht + *(f32*)((u8*)other + 0x23C), oldc,
                                    newc, (f32*)scratch, 1) == 0) {
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
    s32 i;
    s32 j;
    s32 last;
    u8* p;
    u8* q;
    u8* e = (u8*)gEnemies + index * 916;
    s32 ret = -1;
    s32 start;
    f32 best1;
    f32 best = lbl_803468B0;
    f32 hit[4];
    f32 d;
    f32 dy;
    f32 dx;
    f32 dz;
    u8 _spare[40];

    if (b != 0) {
        start = 0;
        last = 3;
    } else {
        if (*(s16*)(e + 628) < 0) {
            return -1;
        }
        best1 = best;
        last = -1;
        p = (u8*)gPlayers;
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
    q = (u8*)gPlayers + start * 13148;
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
extern void fn_80050618(u8* e, s32 type, s32 level, s32 one);
extern void fn_80050054(s32 slot, s32 spew, f32 scale);
extern void fn_8005A338(f32* worldmat, f32* coll_offset, f32* attn_offset);
extern u16 AnimateATree(void* tree, s32 sequence, s32 transition);

/* 0x8004FE34 - initialise a freshly claimed enemy slot's object state. */
void SetEnemyObj(s32 slot, f32* pos, s32 type, s32 level, s32 spew)
{
    u8* e = (u8*)gEnemies + slot * 916;
    u8* tbl = (u8*)lbl_8011AF48;
    s32 t4 = type * 4;
    f32 z = lbl_80346820;
    f32 scale;

    *(s32*)e = type;
    *(f32*)(e + 544) = z;
    *(f32*)(e + 548) = *(f32*)(tbl + t4 + 2080);
    *(f32*)(e + 552) = z;
    *(f32*)(e + 556) = z;
    *(f32*)(e + 560) = *(f32*)(tbl + t4 + 2216);
    *(f32*)(e + 564) = z;
    *(f32*)(e + 576) = z;
    *(f32*)(e + 580) = z;
    *(f32*)(e + 584) = z;
    *(s32*)(e + 180) = 1;
    *(s16*)(e + 724) = 0;
    fn_80050618(e, type, level, 1);
    if (level > 3) {
        level = 2;
    }
    if (spew == 18) {
        level = 1;
    }
    {
        u8* r = (u8*)((u32)tbl + t4);
        scale = *(f32*)(r + 2760);
    }
    if (type != 30) {
        scale = scale * *(f32*)(gCurLevel + 172);
    }
    if (type < 28) {
        scale = (f32)(lbl_80346A28 * scale * level);
    }
    fn_8005A338((f32*)(e + 4), (f32*)(e + 556), (f32*)(e + 544));
    if (*(u32*)(e + 100) != 0) {
        *(f32*)(e + 52) = pos[0];
        *(f32*)(e + 56) = pos[1];
        *(f32*)(e + 60) = pos[2];
        *(f32*)(e + 56) = FloorPos(lbl_80344880, lbl_80346A40, pos, 2);
        MBTreeClearFlags(*(struct mbnode**)(e + 100), 2, 0);
        *(f32*)(e + 512) = scale;
        fn_80050054(slot, spew, scale);
        if (type == 30) {
            *(s16*)(e + 518) = level;
        }
        *(f32*)(e + 56) = *(f32*)(e + 56) + *(f32*)(e + 540);
    }
    UpdateObjWorldMat((f32*)(e + 4));
    fn_8005A404((f32*)(e + 4), (f32*)(e + 556), (f32*)(e + 544));
    *(f32*)(e + 660) = *(f32*)(e + 56);
    if (*(u32*)(e + 476) != 0) {
        *(f32*)(*(u32*)(e + 476) + 48) = *(f32*)(*(u32*)(e + 100) + 48);
        *(f32*)(*(u32*)(e + 476) + 52) = *(f32*)(*(u32*)(e + 100) + 52);
        *(f32*)(*(u32*)(e + 476) + 56) = *(f32*)(*(u32*)(e + 100) + 56);
    }
    if (*(u32*)(e + 108) != 0) {
        AnimateATree((void*)(e + 108), 0, 2);
    }
}
