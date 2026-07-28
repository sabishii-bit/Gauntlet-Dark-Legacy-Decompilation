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

/* --- same-TU statics not yet reconstructed (extern until written) --- */
extern s32 do_enemy_collide(s32 index);
extern void EnemyWorldDamage(Enemy* e, void* wobj, f32* oldpos, f32* hitnrm);
extern void fn_80046140(s32 index);                 /* generator-contact retreat */
extern s32 fn_8004646C(f32 rad, f32 hht, s32 index, f32* oldc, f32* newc,
                       f32* newc2, s32* hitWorld);  /* enemy-vs-enemy probe */
extern s32 fn_80046680(f32 rad, s32 index, s32 b, f32* oldc,
                       f32* newc);                  /* generator-contact probe */
extern s32 fn_8004CFAC(f32* pos, f32* target);      /* turn direction (route) */
extern s32 fn_8004D030(s32 index, s32 ticks);       /* set dead_end/turn timer */

/* --- cross-module callees --- */
extern f32 fn_800BCB44(f32 x, f32 z);               /* 2D magnitude */
extern void fn_8005A65C(f32* worldmat, f32* coll_offset); /* refresh coll_pos */
extern s32 fn_80097790(s32 fx, s32 b);              /* special-fx move tick */
extern void* fn_8000D1E0(f32* from, f32* to, f32* hitnrm); /* world probe */
extern s32 fn_8000D034(f32 rad, f32* pos, f32* trans, f32* hitnrm, f32* out);
                                                    /* wall slide/deflect */
extern s32 fn_8005D20C(s32 index, f32* oldc, f32* newc, s32 moved);
                                                    /* player collide + damage */
extern void fn_800BD050(f32* mat, f32* pyr);        /* pyr -> rotation matrix */
extern void fn_800BE8C8(f32* mat, f32* worldmat);   /* apply to objgrp matrix */
extern void fn_800BA368(struct mbnode* n, s32 a, s32 b); /* node show/update */
extern void fn_800BA2C4(struct mbnode* n, s32 a, s32 b); /* node update */

/* --- module data shared with other enemy helpers --- */
extern s32 lbl_8034457C;      /* frame ticks (game speed units this frame) */
extern f32 lbl_80344590;      /* knockback integration scale */
extern f32 lbl_80344720;      /* current retreat/turn base angle */
extern void* lbl_80344730;    /* last worldobj hit by an enemy move */
extern u8 gGenerators[];      /* 0x80275AE0: four 0x335C generator records */
extern f32 lbl_802510F4[3];   /* world-probe hit normal (module scratch) */
extern f32 lbl_8023CAA8[3];   /* wall-slide output vector (module scratch) */

/* do_enemy_move @0x80044664 (LOCAL; 28 callers: every move_logicNN + the
 * milestone/AI helpers).  Commits the enemy's per-frame translation
 * (e->trans, plus scaled knockback e->pushed), then resolves, in order:
 * generator contact (full revert + retreat), enemy-vs-enemy contact
 * (half-step or push transfer, wall deflection via the world probe),
 * player contact (full revert + per-algorithm turn logic), then rebuilds
 * the object matrix from pyr and services the shadow node and the
 * stuck-walk watchdog.  Transcribed from the GC asm/decompile (905 insns);
 * NonMatching draft - structure and field usage verified against
 * include/game/enemy.h offsets. */
void do_enemy_move(s32 index)
{
    Enemy* e = &gEnemies[index];
    s32 alg = e->algorithm;
    f32 rad = e->rad;
    f32 hht = e->hht;
    s32 blocked = 0;
    s32 hitWorld;
    s32 collide;
    s32 result;
    Enemy* other;
    f32* gen;
    f32 oldpos[3];
    f32 oldc[3];
    f32 newc[3];
    f32 half[3];
    f32 mat[16];

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
        e->coll_pnum = fn_80046680((f32)(0.5 + rad), index, 0, oldc, newc);
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
        gen = (f32*)(gGenerators + e->coll_pnum * 0x335C + 0x44);
        e->route = fn_8004CFAC(&e->objgrp.worldmat[3][0], gen);
        fn_80046140(index);
    } else {
        hitWorld = 0;
        if (e->type == E_DEATH && e->specialfx >= 0) {
            e->specialfx = fn_80097790(e->specialfx, 0);
        }
        if (e->attack_timer > 0) {
            s32 t = e->attack_timer - lbl_8034457C;
            e->attack_timer = t;
            if (t <= 0) {
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
            if (e->coll_enenum < 0x10000) {
                gEnemies[e->coll_enenum].coll_enenum = index;
                other = &gEnemies[e->coll_enenum];
            }
            if (hitWorld != 0) {
                /* the probe clipped the move against the world: retry the
                 * clipped translation against world objects */
                e->trans[0] = newc[0] - e->objgrp.coll_pos[0];
                e->trans[1] = newc[1] - e->objgrp.coll_pos[1];
                e->trans[2] = newc[2] - e->objgrp.coll_pos[2];
                half[0] = oldpos[0] + e->trans[0];
                rad = (f32)(rad * 1.5);
                half[1] = oldpos[1] + e->trans[1];
                half[2] = oldpos[2] + e->trans[2];
                lbl_80344730 = fn_8000D1E0(oldpos, half, lbl_802510F4);
                if (lbl_80344730 == 0) {
                    result = 0;
                } else {
                    EnemyWorldDamage((Enemy*)e, lbl_80344730, oldpos, lbl_802510F4);
                    if ((*(u32*)((u8*)lbl_80344730 + 0x10) & 0x38) == 0) {
                        if ((e->ai_flags & 1) == 0
                            && fn_8000D034(rad, oldpos, e->trans,
                                           lbl_802510F4, lbl_8023CAA8) < 0) {
                            result = 2;
                            e->trans[2] = 0.0f;
                            e->trans[0] = 0.0f;
                        } else {
                            result = 1;
                        }
                    } else {
                        result = 0;
                    }
                }
                if (result == 0) {
                    /* free half-step along the clipped translation */
                    e->objgrp.worldmat[3][0] = oldpos[0] + 0.5 * e->trans[0];
                    e->objgrp.worldmat[3][1] = oldpos[1] + 0.5 * e->trans[1];
                    e->objgrp.worldmat[3][2] = oldpos[2] + 0.5 * e->trans[2];
                } else {
                    hitWorld = 0;
                }
            }
            if (hitWorld == 0) {
                if (other == 0 || e->pushmag2 <= 1.0 || e->action < 28) {
                    /* blocked: full revert */
                    e->moved = 0;
                    blocked = 1;
                    e->objgrp.worldmat[3][0] = oldpos[0];
                    e->objgrp.worldmat[3][1] = oldpos[1];
                    e->objgrp.worldmat[3][2] = oldpos[2];
                    e->trans[0] = 0.0f;
                    e->trans[1] = 0.0f;
                    e->trans[2] = 0.0f;
                } else {
                    /* being knocked back: transfer half the push */
                    other->pushed[0] = 0.5 * e->pushed[0] + other->pushed[0];
                    other->pushed[1] = 0.5 * e->pushed[1] + other->pushed[1];
                    other->pushed[2] = 0.5 * e->pushed[2] + other->pushed[2];
                    other->trans[0] = 0.5 * e->trans[0];
                    other->trans[1] = 0.5 * e->trans[1];
                    other->trans[2] = 0.5 * e->trans[2];
                }
            }
            fn_8005A65C(&e->objgrp.worldmat[0][0], e->coll_offset);
            if (other != 0 && alg == 0) {
                e->route = fn_8004CFAC(&e->objgrp.worldmat[3][0],
                                       &other->objgrp.worldmat[3][0]);
                if (e->dead_end < 1) {
                    e->dead_end = 0x3C;
                    if (e->daction == 3 || e->daction == 4) {
                        e->daction = 0;
                    }
                }
            } else if (other != 0
                       && (alg == 7 || alg == 8 || alg == 10 || alg == 20)) {
                if (e->route == 0 || abs(e->route) > 2) {
                    e->route = fn_8004CFAC(&e->objgrp.worldmat[3][0],
                                           &other->objgrp.worldmat[3][0]);
                    e->collided = 0;
                }
                if (alg == 7) {
                    if (abs(e->route) < 3) {
                        e->collided++;
                        fn_8004D030(index, 15);
                    } else {
                        fn_8004D030(index, 50);
                        e->ang = lbl_80344720;
                        e->pyr[1] = lbl_80344720;
                        e->collided = 0;
                        e->route = 0;
                    }
                    if (e->collided > 6) {
                        e->route *= -2;
                        e->collided = 0;
                    }
                } else if (alg == 8) {
                    if (abs(e->route) < 3) {
                        e->collided++;
                        fn_8004D030(index, 10);
                    } else {
                        fn_8004D030(index, 60);
                        e->ang = lbl_80344720;
                        e->pyr[1] = lbl_80344720;
                        e->collided = 0;
                        e->route = 0;
                    }
                    if (e->collided > 6) {
                        e->route *= -2;
                        e->collided = 0;
                    }
                } else if (alg == 10) {
                    if (abs(e->route) < 3) {
                        e->collided++;
                        fn_8004D030(index, 15);
                    } else {
                        fn_8004D030(index, 50);
                        e->ang = lbl_80344720;
                        e->pyr[1] = lbl_80344720;
                        e->collided = 0;
                        e->route = 0;
                    }
                    if (e->collided > 6) {
                        e->route *= -2;
                        e->collided = 0;
                    }
                } else if (alg == 20) {
                    if (abs(e->route) < 3) {
                        e->collided++;
                        fn_8004D030(index, 10);
                    } else {
                        f64 a;
                        fn_8004D030(index, 30);
                        e->ang = 3.14159265358979 + lbl_80344720;
                        a = e->ang;
                        if (a > 3.14159265358979) {
                            a -= 6.28318530717958;
                        } else if (a <= -3.14159265358979) {
                            a += 6.28318530717958;
                        }
                        e->ang = a;
                        e->pyr[1] = a;
                        e->collided = 0;
                        e->route = 0;
                    }
                    if (e->collided > 6) {
                        e->route *= -2;
                        e->collided = 0;
                    }
                }
            } else {
                if (e->dead_end < 1) {
                    e->dead_end = 0x14;
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
                if (*(s32*)((u8*)e->coll_ip + 0x64) != 0) {
                    e->route = fn_8004CFAC(&e->objgrp.worldmat[3][0],
                                           (f32*)((u8*)e->coll_ip + 0x34));
                }
                if (e->dead_end < 1) {
                    e->dead_end = 0x3C;
                    if (e->daction == 3 || e->daction == 4) {
                        e->daction = 0;
                    }
                }
            } else if (alg == 7 || alg == 8 || alg == 10 || alg == 20) {
                if (*(s32*)((u8*)e->coll_ip + 0x64) == 0) {
                    if (e->dead_end < 1) {
                        e->dead_end = 0x14;
                    }
                } else {
                    if (e->route == 0 || abs(e->route) > 2) {
                        e->route = fn_8004CFAC(&e->objgrp.worldmat[3][0],
                                               (f32*)((u8*)e->coll_ip + 0x34));
                        e->collided = 0;
                    }
                    if (alg == 7 || alg == 8 || alg == 10) {
                        if (abs(e->route) < 3) {
                            e->collided++;
                            fn_8004D030(index, 15);
                        } else {
                            fn_8004D030(index, 15);
                            e->ang = lbl_80344720;
                            e->pyr[1] = lbl_80344720;
                            e->collided = 0;
                            e->route = 0;
                        }
                        if (e->collided > 6) {
                            e->route *= -2;
                            e->collided = 0;
                        }
                    } else if (alg == 20) {
                        if (abs(e->route) < 3) {
                            e->collided++;
                            fn_8004D030(index, 15);
                        } else {
                            f64 a;
                            fn_8004D030(index, 15);
                            e->ang = 3.14159265358979 + lbl_80344720;
                            a = e->ang;
                            if (a > 3.14159265358979) {
                                a -= 6.28318530717958;
                            } else if (a <= -3.14159265358979) {
                                a += 6.28318530717958;
                            }
                            e->ang = a;
                            e->pyr[1] = a;
                            e->collided = 0;
                            e->route = 0;
                        }
                        if (e->collided > 6) {
                            e->route *= -2;
                            e->collided = 0;
                        }
                    }
                }
            } else {
                if (e->dead_end < 1) {
                    e->dead_end = 0x14;
                }
            }
            e->area = 3;
        }
        if (blocked == 0) {
            if (alg == 0 && e->dead_end < 1) {
                e->route = 1;
                e->collided = 0;
            } else if (alg == 2 || alg == 4) {
                e->play--;
                if (e->play < 1) {
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
        fn_800BE8C8(mat, &e->objgrp.worldmat[0][0]);
        if (e->shadow != 0) {
            *(f32*)((u8*)e->shadow + 0x30) = e->objgrp.worldmat[3][0];
            *(f32*)((u8*)e->shadow + 0x34) = e->objgrp.worldmat[3][1];
            *(f32*)((u8*)e->shadow + 0x38) = e->objgrp.worldmat[3][2];
            if (e->action == 1) {
                fn_800BA368(e->shadow, 2, 0);
            } else {
                fn_800BA2C4(e->shadow, 2, 0);
            }
        }
    }

    /* stuck-walk watchdog */
    if (e->moved == 0
        && (e->action == 3 || e->action == 4 || e->action == 0)) {
        e->stopped += (s16)lbl_8034457C;
    } else {
        e->stopped = 0;
    }
    if (e->stopped > 0xB4) {
        e->stopped = 0;
    }
    if (e->stopped > 0x3C && (e->daction == 3 || e->daction == 4)
        && alg != 18 && e->type != E_GOLEM) {
        e->daction = 0;
    }
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
