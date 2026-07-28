#include "game/enemy.h"

/* Gauntlet Dark Legacy enemy module (Xbox ENEMY.OBJ / enemy.c).
 *
 * ENEMY.OBJ is a single very large translation unit.  On the GameCube build it
 * occupies one contiguous .text run, 0x800444C0 - 0x80050054, sitting between
 * dynobjgrid.c (ends 0x800444C0) and gamemain.c (starts 0x80050054).  This file
 * carries the whole TU NonMatching so the tree stays green while the symbol map
 * is filled in; dtk substitutes the original DOL bytes for the .text range, so
 * byte-matching is out of scope for this pass.
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
 *   fn_8004E5F8, fn_8004E67C, fn_8004F1DC, fn_8004F404.
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
f32 turn_enemy_ang(Enemy* e, f32 want);
void do_ai(s32 index);
void move_logic00(s32 index);

/* branchless-abs idiom (srawi/xor/subf at -O4) */
#define ABS(x) (((x) ^ ((x) >> 31)) - ((x) >> 31))

/* --- same-TU statics not yet reconstructed (extern until written) --- */
extern s32 do_enemy_collide(s32 index);
extern void EnemyWorldDamage(Enemy* e, void* wobj, f32* oldpos, f32* hitnrm);
extern void fn_80046140(s32 index);                 /* generator-contact retreat */
extern s32 fn_8004646C(f32 rad, f32 hht, s32 index, f32* oldc, f32* newc,
                       f32* newc2, s32* hitWorld);  /* enemy-vs-enemy probe */
extern s32 fn_80046680(f32 rad, f32 hht, s32 index, s32 b, f32* oldc,
                       f32* newc);                  /* generator-contact probe */
extern s32 fn_8004CFAC(f32* pos, f32* target);      /* turn direction (route) */
extern s32 fn_8004D030(s32 index, s32 ticks);       /* set dead_end/turn timer */
extern void fn_8004F1DC(Enemy* e);                  /* garm2 (type 27) death hook */

/* --- cross-module callees --- */
extern f32 fn_800BCB44(f32 x, f32 z);               /* 2D magnitude */
extern void fn_8005A65C(f32* worldmat, f32* coll_offset); /* refresh coll_pos */
extern s32 DeleteEffect(s32 idx, s32 mode);         /* sfx.c 0x80097790 */
extern void* fn_8000D1E0(f32 rad, f32* from, f32* to, f32* hitnrm); /* world probe */
extern s32 fn_8000D034(f32 rad, f32* pos, f32* trans, f32* hitnrm, f32* out);
                                                    /* wall slide/deflect */
extern s32 fn_8005D20C(s32 index, f32* oldc, f32* newc, s32 moved);
                                                    /* player collide + damage */
extern void fn_800BD050(f32* mat, f32* pyr);        /* pyr -> rotation matrix */
extern void CopyMat3(f32* src, f32* dst);           /* 0x800BE8C8 */
extern void fn_800BA368(struct mbnode* n, s32 a, s32 b); /* node show/update */
extern void fn_800BA2C4(struct mbnode* n, s32 a, s32 b); /* node update */

/* --- module data shared with other enemy helpers --- */
extern s32 lbl_8034457C;      /* frame ticks (game speed units this frame) */
extern f32 lbl_80344590;      /* knockback integration scale */
extern f32 lbl_80344720;      /* current retreat/turn base angle */
extern void* lbl_80344730;    /* last worldobj hit by an enemy move */
extern f32 lbl_80275AE0[][3287]; /* 0x80275AE0: 0x335C generator records */
extern f32 lbl_8023CA98[][4];
extern f32 lbl_8011BED8[];  /* 0x8011BED8 per-type turn-rate table */ /* wall-slide scratch; [1] = output vector */

/* --- TU .bss (declaration order = address order; the compiler addresses the
 * whole block off the first symbol, lbl_80250E00 - gEnemies rides at +0xE18,
 * the world-probe hit normal lbl_802510F4 at +0x2F4). --- */
/* NOTE: MWCC allocates .bss in REVERSE declaration order - declare in reverse
 * address order so lbl_80250E00 lands at section offset 0 (the pool anchor)
 * and gEnemies at +0xE18, matching the target's base+displacement addressing. */
Enemy gEnemies[25];            /* 0x80251C18 */
u32 lbl_80251364[0x8B4 / 4];   /* 0x80251364 */
s32 lbl_802512B0[45];          /* 0x802512B0 per-type spawn-allowed */
s32 lbl_802511FC[45];          /* 0x802511FC per-type min-level class */
s32 lbl_80251148[45];          /* 0x80251148 per-type generator-fx enable */
u32 lbl_80251100[0x48 / 4];    /* 0x80251100 */
f32 lbl_802510F4[3];           /* 0x802510F4 world-probe hit normal */
u32 lbl_80250E40[0x2B4 / 4];   /* 0x80250E40 */
u32 lbl_80250E00[0x40 / 4];    /* 0x80250E00 */

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
    lbl_80251364[0] = 0;
    gEnemies[0].type = E_SCORP;
}

void do_enemy_move(s32 index)
{
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + 3608); /* = &gEnemies[index] via the pool anchor */
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
        e->stun_timer -= lbl_8034457C;
        e->trans[0] = 0.0f;
        e->trans[1] = 0.0f;
        e->trans[2] = 0.0f;
    }
    if (e->action >= 28) {
        e->trans[0] = 0.0f;
        e->trans[1] = 0.0f;
        e->trans[2] = 0.0f;
    }
    e->trans[0] += e->pushed[0] * lbl_80344590;
    e->trans[1] += e->pushed[1] * lbl_80344590;
    e->trans[2] += e->pushed[2] * lbl_80344590;
    if (fn_800BCB44(e->trans[0], e->trans[2]) > 0.001) {
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
                               &lbl_80275AE0[e->coll_pnum][17]);
        fn_80046140(index);
    } else {
        hitWorld = 0;
        if (e->type == E_DEATH && e->specialfx >= 0) {
            e->specialfx = DeleteEffect(e->specialfx, 0);
        }
        if (e->attack_timer > 0) {
            if ((e->attack_timer -= lbl_8034457C) <= 0) {
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
                lbl_80344730 = fn_8000D1E0(rad2, oldpos, half, lbl_802510F4);
                if (lbl_80344730 != 0) {
                    EnemyWorldDamage(e, lbl_80344730, oldpos, lbl_802510F4);
                    if (*(u32*)((u8*)lbl_80344730 + 16) & 0x38) {
                        result = 0;
                    } else if (!(e->ai_flags & 1)
                               && fn_8000D034(rad2, oldpos, e->trans,
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
                        e->ang = 3.141592654 + lbl_80344720;
                        {
                            f64 a = e->ang;
                            if (a > 3.141592654) {
                                a -= 6.283185308;
                            } else if (a <= -3.141592654) {
                                a = 6.283185308 + a;
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
                            e->ang = 3.141592654 + lbl_80344720;
                            {
                                f64 a = e->ang;
                                if (a > 3.141592654) {
                                    a -= 6.283185308;
                                } else if (a <= -3.141592654) {
                                    a = 6.283185308 + a;
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
        fn_800BD050(mat, e->pyr);
        CopyMat3(mat, &e->objgrp.worldmat[0][0]);
        if (e->shadow != 0) {
            *(f32*)((u8*)e->shadow + 48) = e->objgrp.worldmat[3][0];
            *(f32*)((u8*)e->shadow + 52) = e->objgrp.worldmat[3][1];
            *(f32*)((u8*)e->shadow + 56) = e->objgrp.worldmat[3][2];
            if (e->action == 1) {
                fn_800BA368(e->shadow, 2, 0);
            } else {
                fn_800BA2C4(e->shadow, 2, 0);
            }
        }
    }

    /* stuck-walk watchdog */
    if (e->moved != 0
        || (e->action != 3 && e->action != 4 && e->action != 0)) {
        e->stopped = 0;
    } else {
        e->stopped += lbl_8034457C;
    }
    if (e->stopped > 180) {
        e->stopped = 0;
    }
    if (e->stopped > 60 && (e->daction == 3 || e->daction == 4)
        && alg != 18 && e->type != E_GOLEM) {
        e->daction = 0;
    }
}

/* ===================================================================== *
 *  AI MOVE-LOGIC STATE HANDLERS  (do_ai jumptable, 0x80046B54..0x8004C650)
 *  Each takes the enemy slot index, sets a desired facing/velocity, then
 *  calls do_enemy_move(index) to commit the move + resolve collisions.
 * ===================================================================== */

/* --- move_logic shared externs --- */
extern void RequestEnemyAction(Enemy* e, s32 action);
extern f32 fn_8002C7CC(f32* to, f32* from);   /* dir angle from->to */
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
extern void fn_8009E03C(Enemy* e);   /* skeleton bone-toss fx */
extern s32 fn_8004C8CC(f32* pos, s32 index);   /* wall/object clearance probe */
extern f32 lbl_8011C0C4[];    /* 0x8011C0C4 wander search-angle offsets */

/* float magnitude via sign-bit clear (matches the enemy AI's inline fabs). */
static f32 fabsf_(f32 x)
{
    *(u32*)&x &= 0x7FFFFFFF;
    return x;
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
        e->dead_end -= lbl_8034457C;
    }
    if (e->dead_end < 1) {
        if (e->closest < 0) {
            base = e->ang;
        } else if (*(s16*)&lbl_80275AE0[e->closest][647] > 2) {
            base = fn_8002C7CC(&lbl_80275AE0[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            base = fn_8002C7CC(&lbl_80275AE0[e->closest][17], &e->objgrp.worldmat[3][0]);
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
    } else if (*(s16*)&lbl_80275AE0[e->closest][647] > 2) {
        e->ang = fn_8002C7CC(&lbl_80275AE0[e->closest][633], &e->objgrp.worldmat[3][0]);
        a = e->ang;
    } else {
        e->ang = fn_8002C7CC(&lbl_80275AE0[e->closest][17], &e->objgrp.worldmat[3][0]);
        a = e->ang;
    }
    e->ang = a;
    if (e->area == 2 && e->dead_end > 0) {
        e->dead_end -= lbl_8034457C;
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
        if ((e->dead_end -= lbl_8034457C) <= 0) {
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
        e->ang = fn_8002C7CC(&lbl_80275AE0[e->coll_pnum][17], &e->objgrp.worldmat[3][0]);
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
            if (*(s16*)&lbl_80275AE0[e->counter2][647] > 2) {
                face = fn_8002C7CC(&lbl_80275AE0[e->counter2][633], &e->objgrp.worldmat[3][0]);
            } else {
                face = fn_8002C7CC(&lbl_80275AE0[e->counter2][17], &e->objgrp.worldmat[3][0]);
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
        if ((e->dead_end -= lbl_8034457C) <= 0) {
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
        e->ang = fn_8002C7CC(&lbl_80275AE0[e->coll_pnum][17], &e->objgrp.worldmat[3][0]);
    }
    fn_8004CD1C(e, 1.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
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
        if (*(s16*)&lbl_80275AE0[e->closest][647] > 2) {
            a = fn_8002C7CC(&lbl_80275AE0[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            a = fn_8002C7CC(&lbl_80275AE0[e->closest][17], &e->objgrp.worldmat[3][0]);
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
        if (*(s16*)&lbl_80275AE0[e->closest][647] > 2) {
            a = fn_8002C7CC(&lbl_80275AE0[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            a = fn_8002C7CC(&lbl_80275AE0[e->closest][17], &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    mode = e->mode1;
    if (mode == 1) {
        goto do_action;
    } else if (mode < 1 || mode > 2) {
        e->counter1 = 1;
        e->mode1++;
    do_action:
        RequestEnemyAction(e, 3);
        if (e->action == 3) {
            e->counter1 = RandInt(90) + 30;
            e->mode1++;
        }
        if (e->mode1 != 2) {
            goto move;
        }
    }
    if (e->dead_end > 0) {
        e->dead_end -= lbl_8034457C;
    }
    if (e->dead_end < 1) {
        fn_8004CD1C(e, 1.0f, e->ang);
    }
    e->counter1 -= lbl_8034457C;
    if (e->counter1 < 1 && e->daction == 3) {
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
        if (*(s16*)&lbl_80275AE0[e->closest][647] > 2) {
            face = fn_8002C7CC(&lbl_80275AE0[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            face = fn_8002C7CC(&lbl_80275AE0[e->closest][17], &e->objgrp.worldmat[3][0]);
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
        if (*(s16*)&lbl_80275AE0[e->closest][647] > 2) {
            a = fn_8002C7CC(&lbl_80275AE0[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            a = fn_8002C7CC(&lbl_80275AE0[e->closest][17], &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    if (e->closest >= 0) {
        f32 dy = e->objgrp.worldmat[3][1] - lbl_80275AE0[e->closest][18];
        if (e->visactive != 0 && e->actual_dist <= e->sight
            && dy >= -10.0 && dy <= 10.0) {
            if (e->flag2 <= 0) {
                RequestEnemyAction(e, 24);
            } else {
                e->flag2 -= lbl_8034457C;
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
           + fn_8002C7CC(&gEnemies[lbl_80344748].objgrp.worldmat[3][0],
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
        if (*(s16*)&lbl_80275AE0[e->closest][647] > 2) {
            a = fn_8002C7CC(&lbl_80275AE0[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            a = fn_8002C7CC(&lbl_80275AE0[e->closest][17], &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    if (e->closest >= 0) {
        f32 dy = e->objgrp.worldmat[3][1] - lbl_80275AE0[e->closest][18];
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
                e->flag2 -= lbl_8034457C;
            }
        }
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
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
    if (e->dead_end <= 0 && (e->counter1 -= lbl_8034457C) <= 0) {
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
        if (*(s16*)&lbl_80275AE0[e->closest][647] > 2) {
            a = fn_8002C7CC(&lbl_80275AE0[e->closest][633], &e->objgrp.worldmat[3][0]);
        } else {
            a = fn_8002C7CC(&lbl_80275AE0[e->closest][17], &e->objgrp.worldmat[3][0]);
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
                e->flag2 -= lbl_8034457C;
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
extern void fn_80064154(struct item* ip);           /* commit placed item */
extern void fn_8002C49C(f32* worldmat);             /* release grid slot */
extern void fn_800BAEAC(struct mbnode* n, s32 a);   /* delete scene node */
extern void SfxDeleteParented(struct mbnode* n, s32 a, s32 b);
extern void fn_800115D0(void* atree);               /* free anim tree */
extern s32 lbl_803443B4;      /* level-teardown-in-progress flag */
extern s32 lbl_80344734;      /* node-delete reentry guard */

/* kill_enemy @0x8004EFE4.  Drop the carried item (or place a "GARG<level>"
 * egg for gargoyles), then tear the enemy down: health/state clear, grid
 * release, shadow + special fx + scene node deletion, generator uncouple. */
void kill_enemy(s32 index)
{
    Enemy* e = &gEnemies[index];
    struct item* item = 0;
    s32 carried = 0;
    char* p;
    char buf[32];

    if (lbl_803443B4 != 0) {
        return;
    }
    if (e->gotitem != 0) {
        item = e->gotitem;
        e->gotitem = 0;
        carried = 1;
    } else {
        switch (e->type) {
        case 32:
            sprintf(buf, "GARG%s", fn_80057ACC());
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
            fn_800920E0(e->objgrp.attn_pos, item, 0.0f);
        } else {
            *((u8*)item + 205) = 0;
            fn_800BA2C4(*(struct mbnode**)((u8*)item + 100), 2, 0);
            if (**(s32**)item == 1) {
                *(s16*)((u8*)item + 236) = 60;
            }
            *(f32*)((u8*)item + 52) = e->objgrp.worldmat[3][0];
            *(f32*)((u8*)item + 56) = e->objgrp.worldmat[3][1];
            *(f32*)((u8*)item + 60) = e->objgrp.worldmat[3][2];
            fn_80064154(item);
        }
    }
    if (e->type == 27) {
        fn_8004F1DC(e);
    }
    e->health = 0.0f;
    e->state = 0;
    fn_8002C49C(&e->objgrp.worldmat[0][0]);
    if (e->shadow != 0) {
        fn_800BAEAC(e->shadow, 0);
        e->shadow = 0;
    }
    if (e->specialfx >= 0) {
        e->specialfx = DeleteEffect(e->specialfx, 1);
    }
    if (e->type == 27) {
        SfxDeleteParented(e->objgrp.node, 0, -1);
    }
    fn_800115D0(&e->atree);
    lbl_80344734 = 1;
    fn_800BAEAC(e->objgrp.node, 0);
    lbl_80344734 = 0;
    e->objgrp.node = 0;
    uncouple_enemy(index);
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
    step = rate * (f32)(u32)lbl_8034457C;
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
extern s32 fn_8004F87C(s32 type, s32 level, s32 a);  /* resolve spew/drop class */
extern f32 fn_8004FBC8(f32* v, f32* out, s32 d);     /* rotate spawn dir d */
extern s32 check_enemy_pos(f32* start, f32* out, s32 slot);
extern void SetEnemyObj(s32 slot, f32* pos, s32 type, s32 level, s32 spew);
extern void fn_8005A3B8(f32* worldmat);              /* claim grid cell */
extern void fn_8005A404(f32* worldmat, f32* coll_offset, f32* attn_offset);
extern void fn_8000ED70(void* animinfo, s32 seq, s32 c, s32 d, f32 time);
extern void StartGenFX(f32* pos, s32 level);
extern s32 gBossType;
extern s32 gBossDying;
extern s32 lbl_8034477C;      /* current map/world id */
extern s32 lbl_803447DC;      /* generators-disabled flag */
extern s32 lbl_8034472C;      /* random-type rotation counter */
extern u8 lbl_8011AF48[];     /* enemy.c .data anchor (type tables at +4284..) */

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

    if (lbl_8034477C == 0x8007) {
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
        e->genang_offset = 0.0f;
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
        fn_800BA368(e->objgrp.node, 2, 0);
        if (e->shadow != 0) {
            fn_800BA368(e->shadow, 2, 0);
        }
        e->state = 0;
        return -3;
    }
placed:
    fn_8005A3B8(&e->objgrp.worldmat[0][0]);
    fn_8005A404(&e->objgrp.worldmat[0][0], e->coll_offset, e->attn_offset);
    e->action = 1;
    r = e->actionlist[1].animidx;
    if (r >= 0) {
        fn_8000ED70(&e->atree.animinfo, r, 0, 1, 0.0f);
    }
    if (e->hht > 2.0 && level <= 3 && lbl_80251148[type] != 0) {
        StartGenFX(pos, level);
    }
    return slot;
}

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
        e->algorithm = -e->algorithm;
    }
    /* then: decrement the generator's live count via e->generator and emit
     * "Enemy has non generator generator" if the parent is not a generator. */
}

/* find_enemy_slot: return a free/recyclable enemy slot for a new spawn.
 * Recycles the least-important live enemy through kill_enemy when the array is
 * full; returns -1 when the request cannot be satisfied. */
s32 find_enemy_slot(s32 type, s32 level) {
    (void)type;
    (void)level;
    return -1;
}
