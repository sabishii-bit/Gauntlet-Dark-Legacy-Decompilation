#!/usr/bin/env python3
"""Bounded enemy source experiments, generated only under build/.

Uses the real Ninja compile edge and requires whole-object baseline fidelity.
No project source or postprocessor rule is changed. Raw body identity is not
a proof of relocation/data equality; retained candidates still need all gates.
"""
import argparse
import difflib
import json
import itertools
import re
from pathlib import Path

from cn_analyze import load, target_object
from cv_probe import REPO, compile_with, read_edges
from probe import function_span
from fndiff import parse

UNIT = "game/enemy/enemy"


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"expected unique source shape: {old!r}")
    return text.replace(old, new, 1)


def pointer_variants(body):
    # Axis: express the existing traversal by an index, letting strength
    # reduction create the induction pointer instead of declaring its home.
    indexed = replace_once(body, "    u8* p;\n", "")
    indexed = replace_once(indexed, "        p = (u8*)gPlayerWords;\n", "")
    indexed = indexed.replace("i++, p += PLAYER_STRIDE", "i++")
    yield "indexed_first_loop", re.sub(r"\bp \+ offsetof", "(u8*)gPlayerWords[i] + offsetof", indexed)
    # Positive control only: the world-approved compatibility idiom. NOT an
    # approved enemy implementation and never copied to src/ by this script.
    wrapped = replace_once(body, "    u8* p;", "    struct { u8* value; } player;")
    yield "EXPERIMENTAL_aggregate_pointer", re.sub(r"\bp\b", "player.value", wrapped)
    # Distinct source identity: one loop-scoped array owner, with its typed
    # member serving the walk. Statement chronology stays unchanged.
    owned = replace_once(body, "    u8* p;", "    EnemyPlayerArray* players;\n    u8* p;")
    owned = replace_once(owned, "        p = (u8*)gPlayerWords;", "        players = &gPlayers;\n        p = (u8*)players->words;")
    yield "array_owner_local", owned
    # Natural per-iteration player view, rather than a pointer live across
    # the loop backedge; this tests web identity, not cast spelling.
    scoped = replace_once(indexed, "for (i = 0; i < 4; i++) {", "for (i = 0; i < 4; i++) {\n            Player* player = (Player*)gPlayerWords[i];")
    scoped = scoped.replace("p + offsetof", "(u8*)player + offsetof")
    yield "indexed_scoped_player", scoped
    typed = replace_once(body, "    u8* p;", "    Player* p;")
    typed = replace_once(typed, "p = (u8*)gPlayerWords", "p = (Player*)gPlayerWords")
    typed = re.sub(r"\*\((?:s32|s16|f32)\*\)\(p \+ offsetof\(Player, (.*?)\)\)", r"p->\1", typed)
    yield "typed_player_increment", typed.replace("p += PLAYER_STRIDE", "p++")
    yield "typed_player_byte_stride", typed.replace("p += PLAYER_STRIDE", "p = (Player*)((u8*)p + PLAYER_STRIDE)")
    void_ptr = replace_once(body, "    u8* p;", "    void* p;")
    void_ptr = void_ptr.replace("p + offsetof", "(u8*)p + offsetof")
    void_ptr = void_ptr.replace("p += PLAYER_STRIDE", "p = (u8*)p + PLAYER_STRIDE")
    yield "void_pointer_carrier", void_ptr
    begin = body.index("        best1 = best;")
    finish = body.index("        start = last;", begin) + len("        start = last;")
    nearest_loop = body[begin:finish]
    nearest_loop = nearest_loop[nearest_loop.index("        last = -1;"):]
    nearest_loop = nearest_loop.replace("        start = last;", "        return last;")
    helper = """static inline s32 enemy_nearest_live_player(u8* e, f32 best1, u8* p)
{
    s32 last;
    s32 i;
    f32 d;
    f32 dy;
    f32 dx;
    f32 dz;
%s
}

""" % nearest_loop
    caller = body[:begin] + "        start = last = enemy_nearest_live_player(e, best, (u8*)gPlayerWords);" + body[finish:]
    for declaration in ("    s32 i;\n", "    u8* p;\n", "    f32 best1;\n", "    f32 dy;\n", "    f32 dx;\n", "    f32 dz;\n"):
        caller = replace_once(caller, declaration, "")
    yield "inline_nearest_player", helper + caller
    out_helper = helper.replace("s32 enemy_nearest_live_player(u8* e, f32 best1, u8* p)", "void enemy_nearest_live_player(u8* e, f32 best1, u8* p, s32* nearest)")
    out_helper = out_helper.replace("    s32 last;\n", "").replace("        return last;", "")
    out_helper = re.sub(r"\blast\b", "(*nearest)", out_helper)
    out_caller = caller.replace("start = last = enemy_nearest_live_player(e, best, (u8*)gPlayerWords);", "enemy_nearest_live_player(e, best, (u8*)gPlayerWords, &last);\n        start = last;")
    yield "inline_nearest_player_output", out_helper + out_caller
    reranked = replace_once(out_caller, "    s32 last;\n    s32 j;\n    u8* q;",
                           "    u8* q;\n    s32 j;\n    s32 last;")
    yield "inline_nearest_output_ranked", out_helper + reranked
    start_caller = out_caller.replace("gPlayerWords, &last);\n        start = last;", "gPlayerWords, &start);\n        last = start;")
    yield "inline_nearest_start_output", out_helper + start_caller
    order_helper = out_helper.replace("u8* e, f32 best1, u8* p, s32* nearest", "u8* p, u8* e, f32 best1, s32* nearest")
    order_caller = out_caller.replace("enemy_nearest_live_player(e, best, (u8*)gPlayerWords, &last)", "enemy_nearest_live_player((u8*)gPlayerWords, e, best, &last)")
    yield "inline_nearest_input_order", order_helper + order_caller


def normalization_variants(body):
    block = """            {
                f64 av;
                if ((av = cand) > 3.141592654) {
                    av -= 6.283185308;
                } else if (av <= -3.141592654) {
                    av = 6.283185308 + av;
                }
                cand = av;
            }"""
    if body.count(block) != 2:
        raise ValueError("expected the two in-place candidate normalizations")
    for typ in ("f64", "f32"):
        helper = """static inline f32 enemy_normalize_angle(%s angle)
{
    f64 a = angle;
    if (a > 3.141592654) a -= 6.283185308;
    else if (a <= -3.141592654) a = 6.283185308 + a;
    return a;
}

""" % typ
        yield "inline_normalize_" + typ, helper + body.replace(block, "            cand = enemy_normalize_angle(cand);")
        yield "inline_normalize_propagating_" + typ, "#pragma opt_propagation on\n" + helper + "#pragma opt_propagation off\n" + body.replace(block, "            cand = enemy_normalize_angle(cand);")
    yield "register_normalize_local", body.replace(block, block.replace("f64 av;", "register f64 av;"))
    # Same arithmetic precision as the starting source, but the working
    # angle is double until explicitly rounded back at the normalization.
    first, rest = body.split("    case 1: {", 1)
    rest = rest.replace("f32 cand;", "f64 cand;")
    rest = rest.replace("cand = cand + q[1095]", "cand = (f32)cand + q[1095]")
    rest = rest.replace("cand = cand - q[1095]", "cand = (f32)cand - q[1095]")
    rest = rest.replace("cand = av;", "cand = (f32)av;")
    rest = rest.replace("= cand - e->angbak", "= (f32)cand - e->angbak")
    yield "double_candidate_float_operations", first + "    case 1: {" + rest
    # Split the pre-normalization and post-normalization angle identities;
    # every operation and narrowing conversion is held fixed.
    first, rest = body.split("    case 1: {", 1)
    case1, default = rest.split("    default: {", 1)
    def split_candidate(part, declare=True):
        pre, post = part.split(block, 1)
        pre = pre.replace("f32 cand;", "f32 cand;\n        f32 raw_angle;" if declare else "f32 cand;")
        pre = re.sub(r"\bcand\b", "raw_angle", pre)
        pre = pre.replace("f32 raw_angle;", "f32 cand;", 1)
        return pre + block.replace("(av = cand)", "(av = raw_angle)") + post
    split = first + "    case 1: {" + split_candidate(case1) + "    default: {" + split_candidate(default)
    yield "separate_pre_post_angle", split
    shared = first.replace("    f32 speed;", "    f32 speed;\n    f32 raw_angle;")
    shared += "    case 1: {" + split_candidate(case1, False) + "    default: {" + split_candidate(default, False)
    yield "shared_pre_angle", shared
    helper = """static inline void enemy_normalize_angle_ref(f32* angle)
{
    f64 a = *angle;
    if (a > 3.141592654) a -= 6.283185308;
    else if (a <= -3.141592654) a = 6.283185308 + a;
    *angle = a;
}

"""
    yield "inline_normalize_reference", helper + body.replace(block, "            enemy_normalize_angle_ref(&cand);")
    yield "propagation_on", "#pragma opt_propagation on\n" + body + "\n#pragma opt_propagation off\n"
    yield "explicit_angle_widening", body.replace(block, block.replace("(av = cand)", "(av = (f64)cand)"))
    yield "explicit_angle_rounding", body.replace(block, block.replace("cand = av;", "cand = (f32)av;"))


def generate_variants(body):
    old = "    gEnemies[slot].generator = gen;\n    e = &gEnemies[slot];"
    yield "assign_then_store", replace_once(body, old, "    e = &gEnemies[slot];\n    e->generator = gen;")
    yield "reuse_table_pointer", replace_once(body, old,
        "    tbl = (u8*)&gEnemies[slot];\n    ((Enemy*)tbl)->generator = gen;\n    e = (Enemy*)tbl;")
    # The action field was just set to E_START (=1); indexing through it is
    # the natural state-machine spelling of this call, not a new behavior.
    yield "index_current_action", body.replace("e->actionlist[1]", "e->actionlist[e->action]")
    old_anim = """    if (e->actionlist[1].animidx >= 0) {
        InitAnim(lbl_80346820, &e->atree.animinfo,
                 e->actionlist[1].animidx, 0, 1);
    }"""
    helper = """static inline void enemy_start_anim(Enemy* enemy, s32 animation)
{
    if (animation >= 0) {
        InitAnim(lbl_80346820, &enemy->atree.animinfo, animation, 0, 1);
    }
}

"""
    yield "inline_start_anim", helper + replace_once(body, old_anim,
        "    enemy_start_anim(e, e->actionlist[1].animidx);")
    for ctype in ("s32", "u32", "long", "unsigned long"):
        code = """    {
        %s animation = e->actionlist[1].animidx;
        if ((s32)animation >= 0) {
            InitAnim(lbl_80346820, &e->atree.animinfo, animation, 0, 1);
        }
    }""" % ctype
        yield "animation_type_" + ctype.replace(" ", "_"), replace_once(body, old_anim, code)
        if ctype in ("s32", "long"):
            no_cast = replace_once(body, old_anim, code.replace("(s32)animation", "animation"))
            yield "animation_plain_" + ctype, no_cast
            yield "joint_store_animation_" + ctype, replace_once(no_cast, old, "    e = &gEnemies[slot];\n    e->generator = gen;")
    yield "propagation_off", "#pragma opt_propagation off\n" + body + "\n#pragma opt_propagation reset\n"
    guarded = """    {
        u32 animation = e->actionlist[1].animidx;
        if ((s32)animation >= 0) {
            InitAnim(lbl_80346820, &e->atree.animinfo, animation, 0, 1);
        }
    }"""
    joint = replace_once(replace_once(body, old_anim, guarded), old,
                         "    e = &gEnemies[slot];\n    e->generator = gen;")
    yield "joint_signed_index_store", joint
    ranks = replace_once(joint, "    s32 otype;\n    s32 mask = 0;\n    s32 slot;",
                         "    s32 slot;\n    s32 otype;\n    s32 mask = 0;")
    yield "joint_slot_rank", ranks
    ranks = replace_once(ranks, "    s32 start;\n    s32 d;", "    s32 d;\n    s32 start;")
    yield "joint_loop_ranks", ranks
    earlier = replace_once(ranks, "    Enemy* e;\n", "")
    earlier = replace_once(earlier, "    u8* tbl = lbl_8011AF48;", "    Enemy* e;\n    u8* tbl = lbl_8011AF48;")
    yield "joint_enemy_decl_first", earlier
    delayed = replace_once(ranks, "    u8* tbl = lbl_8011AF48;", "    u8* tbl;")
    delayed = replace_once(delayed, "    if (gGameMode == MA_HSTABLE)", "    tbl = lbl_8011AF48;\n    if (gGameMode == MA_HSTABLE)")
    yield "joint_table_assigned", delayed
    direct = replace_once(ranks, "    u8* tbl = lbl_8011AF48;\n", "")
    yield "joint_direct_table", re.sub(r"\btbl\b", "lbl_8011AF48", direct)
    yield "joint_const_table", ranks.replace("u8* tbl =", "const u8* tbl =")
    words = replace_once(ranks, "u8* tbl = lbl_8011AF48", "s32* tbl = (s32*)lbl_8011AF48")
    yield "joint_word_table", words.replace("(tbl +", "((u8*)tbl +")
    point_use = replace_once(ranks, "    u8* tbl = lbl_8011AF48;", "    u8* tbl;")
    yield "joint_table_after_guards", replace_once(point_use, "    otype = type;", "    tbl = lbl_8011AF48;\n    otype = type;")
    for where in ("after", "before"):
        table_decl = "    u8* tbl = lbl_8011AF48;"
        pool_decl = "    u8* pool = (u8*)lbl_80250E00;"
        explicit = replace_once(ranks, table_decl, table_decl + "\n" + pool_decl if where == "after" else pool_decl + "\n" + table_decl)
        explicit = explicit.replace("e = &gEnemies[slot];", "e = (Enemy*)(pool + slot * 916 + ENEMY_POOL_OFF);")
        yield "joint_explicit_pool_" + where, explicit


def movement_variants(body):
    # The most recent retained source has the entry's compiler-reserved
    # home removed and its eight bytes placed explicitly below the matrix.
    canonical_entry = "    u8* row = (u8*)lbl_80250E00 + index * 916;\n    Enemy* e = (Enemy*)(row + 3608); /* = &gEnemies[index] via the pool anchor */"
    live = body
    body = body.replace("    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + ENEMY_POOL_OFF);", canonical_entry)
    body = re.sub(r"^    u8 matrixGap\[8\];[^\n]*\n", "", body, flags=re.M)
    body = body.replace("ABS_REVERSED(", "ABS(")
    # Accept either the old separate assignment or the retained guard form.
    if "if ((a = e->ang) > hi)" in body:
        body = re.sub(r"if \(\(a = e->ang\) > hi\)", "a = e->ang;\n                            if (a > hi)", body)
    body = body.replace("                other = &gEnemies[n];\n                other->coll_enenum = index;", "                gEnemies[n].coll_enenum = index;\n                other = (Enemy*)&gEnemies[n];")
    no_mat = replace_once(body, "    f32 mat[16];\n", "")
    yield "matrix_block_scope", replace_once(no_mat, "    if (e->state != 0) {", "    if (e->state != 0) {\n        f32 mat[16];")
    matrix_calls = "        CreateYPRMatrix(mat, e->pyr);\n        CopyMat3(mat, &e->objgrp.worldmat[0][0]);"
    helper = """static inline void enemy_rebuild_rotation(Enemy* e)
{
    f32 mat[16];
    CreateYPRMatrix(mat, e->pyr);
    CopyMat3(mat, &e->objgrp.worldmat[0][0]);
}

"""
    yield "matrix_inline_helper", helper + replace_once(no_mat, matrix_calls, "        enemy_rebuild_rotation(e);")
    yield "explicit_normalize_widen", body.replace("a = e->ang;", "a = (f64)e->ang;")
    yield "assignment_normalize_guard", re.sub(r"a = e->ang;\n(\s*)if \(a > hi\)", r"if ((a = e->ang) > hi)", body)
    no_a = re.sub(r"^\s*f64 a;\n", "\n", body, flags=re.M)
    yield "normalize_decl_init", no_a.replace("a = e->ang;", "{ f64 a = e->ang;").replace("e->pyr[1] = a;", "e->pyr[1] = a; }")
    # Recovering an inline normalization can change the float-to-double
    # copy web while keeping the single rounding at the stores intact.
    helper = """static inline f64 enemy_wrapped_heading(f64 a, f64 hi)
{
    if (a > hi) a -= lbl_80346848;
    else if (a <= lbl_80346850) a = lbl_80346848 + a;
    return a;
}

"""
    pattern = r"a = e->ang;\n\s*if \(a > hi\) \{\n\s*a -= lbl_80346848;\n\s*\} else if \(a <= lbl_80346850\) \{\n\s*a = lbl_80346848 \+ a;\n\s*\}"
    replaced, n = re.subn(pattern, "a = enemy_wrapped_heading(e->ang, hi);", body)
    if n != 2:
        raise ValueError("expected two heading normalizations")
    yield "normalize_inline_helper", helper + replaced
    yield "normalize_float_input_helper", helper.replace("heading(f64 a, f64 hi)", "heading(f32 angle, f64 hi)").replace("    if (a > hi)", "    f64 a = angle;\n    if (a > hi)") + replaced
    # Opaque coll_ip fields are still addressed through byte casts in this
    # function. Separate full-expression loads test the missing reload at
    # target +0x9c4 without adding volatile or artificial side effects.
    yield "collision_route_local", body.replace("                    if (e->route == 0 || ABS(e->route) > 2) {\n", "                    s32 route = e->route;\n                    if (route == 0 || ABS(route) > 2) {\n")
    joint = re.sub(r"a = e->ang;\n(\s*)if \(a > hi\)", r"if ((a = e->ang) > hi)", body)
    shared = re.sub(r"^\s*f64 a;\n", "\n", joint, flags=re.M)
    yield "shared_normalization_before_mat", replace_once(shared, "    f32 mat[16];", "    f64 a;\n    f32 mat[16];")
    yield "shared_normalization_after_mat", replace_once(shared, "    f32 mat[16];", "    f32 mat[16];\n    f64 a;")
    other = "                gEnemies[n].coll_enenum = index;\n                other = (Enemy*)&gEnemies[n];"
    helper = """static inline Enemy* enemy_mark_collision(s32 n, s32 index)
{
    gEnemies[n].coll_enenum = index;
    return &gEnemies[n];
}

"""
    yield "collision_inline_return", helper + replace_once(joint, other, "                other = enemy_mark_collision(n, index);")
    helper2 = helper.replace("return &gEnemies[n];", "Enemy* enemy = &gEnemies[n];\n    return enemy;")
    # Keep declarations C89-compatible.
    helper2 = helper2.replace("    gEnemies[n].coll_enenum = index;\n    Enemy* enemy", "    Enemy* enemy").replace("    return enemy;", "    gEnemies[n].coll_enenum = index;\n    return enemy;")
    yield "collision_inline_named_return", helper2 + replace_once(joint, other, "                other = enemy_mark_collision(n, index);")
    yield "collision_assign_then_store", replace_once(joint, other, "                other = &gEnemies[n];\n                other->coll_enenum = index;")
    # Local prefix view is GC-verified: item.h gives OBJGRP at +4,
    # worldmat translation at +0x34 and node at +0x64. Avoid header conflicts.
    view = """typedef struct EnemyCollisionItemView {
    void* info;
    f32 worldmat[4][4];
    f32 attn_pos[4];
    f32 coll_pos[4];
    void* node;
} EnemyCollisionItemView;

"""
    typed = joint.replace("*(u32*)((u8*)e->coll_ip + 100)", "((EnemyCollisionItemView*)e->coll_ip)->node")
    yield "collision_named_node", view + typed
    typed = typed.replace("(f32*)((u8*)e->coll_ip + 52)", "((EnemyCollisionItemView*)e->coll_ip)->worldmat[3]")
    yield "collision_named_item", view + typed
    kept = replace_once(joint, other, "                other = &gEnemies[n];\n                other->coll_enenum = index;")
    node = "*(u32*)((u8*)e->coll_ip + 100)"
    helper = """static inline void* enemy_collision_item_node(Enemy* enemy)
{
    return ((EnemyCollisionItemView*)enemy->coll_ip)->node;
}

"""
    yield "collision_node_accessor", view + helper + kept.replace(node, "enemy_collision_item_node(e)")
    yield "collision_node_value_local", kept.replace("if (" + node + " != 0) {", "u32 node = " + node + ";\n                if (node != 0) {")
    needle = "                    if (e->route == 0 || ABS(e->route) > 2) {"
    yield "collision_unsigned_route_guard", kept.replace(needle, "                    if ((u32)e->route == 0 || ABS(e->route) > 2) {")
    yield "collision_signed_pointer_cast", view + kept.replace(node, "(u32)((EnemyCollisionItemView*)e->coll_ip)->node")
    entry = "    u8* row = (u8*)lbl_80250E00 + index * 916;\n    Enemy* e = (Enemy*)(row + 3608); /* = &gEnemies[index] via the pool anchor */"
    yield "direct_enemy_entry", replace_once(kept, entry, "    Enemy* e = &gEnemies[index];")
    yield "typed_pool_entry", replace_once(kept, entry, "    Enemy* e = &((EnemyMovePage05*)lbl_80250E00)->enemies[index];")
    yield "entry_pointer_advance", replace_once(kept, entry, "    u8* row = (u8*)lbl_80250E00 + index * 916;\n    Enemy* e = (Enemy*)(row += ENEMY_POOL_OFF);")
    gap = replace_once(kept, "    f32 mat[16];", "    f32 mat[16];\n    u8 matrixGap[8];")
    yield "typed_pool_and_matrix_gap", replace_once(gap, entry, "    Enemy* e = &((EnemyMovePage05*)lbl_80250E00)->enemies[index];")
    yield "direct_enemy_and_matrix_gap", replace_once(gap, entry, "    Enemy* e = &gEnemies[index];")
    yield "byte_pool_and_matrix_gap", replace_once(gap, entry, "    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + ENEMY_POOL_OFF);")
    yield "reversed_absolute_value", live.replace("ABS(", "ABS_REVERSED(")
    yield "common_subexpressions_off", "#pragma opt_common_subs off\n" + live + "\n#pragma opt_common_subs reset\n"
    yield "propagation_off", "#pragma opt_propagation off\n" + live + "\n#pragma opt_propagation reset\n"
    # Preserve the retail fallthrough order, but express the node guard as
    # a forward goto. The test is whether the CSE web survives this CFG form.
    node_guard = "                if (*(u32*)((u8*)e->coll_ip + 100) != 0) {"
    begin = live.rindex(node_guard)
    fallback = "                } else {\n                    if (e->dead_end <= 0) {\n                        e->dead_end = 20;\n                    }\n                }"
    finish = live.index(fallback, begin)
    goto = live[:begin] + "                if (*(u32*)((u8*)e->coll_ip + 100) == 0) goto no_collision_node;\n                {" + live[begin+len(node_guard):finish]
    goto += "                }\n                goto collision_node_done;\n            no_collision_node:\n                if (e->dead_end <= 0) e->dead_end = 20;\n            collision_node_done:\n                ;" + live[finish+len(fallback):]
    yield "node_guard_goto", goto
    # A local pointer for the first test, versus re-reading the owner for
    # the route call: separate source identities without volatile accesses.
    first_view = live[:begin] + "                u8* collided = (u8*)e->coll_ip;\n" + live[begin:]
    first_view = first_view[:begin] + first_view[begin:].replace("*(u32*)((u8*)e->coll_ip + 100)", "*(u32*)(collided + 100)", 1)
    yield "node_check_local_owner", first_view
    route_call = "e->route = fn_8004CFAC(&e->objgrp.worldmat[3][0],\n                                               (f32*)((u8*)e->coll_ip + 52));"
    yield "route_call_inline_helper", """static inline s32 enemy_item_route(Enemy* enemy)
{
    return fn_8004CFAC(&enemy->objgrp.worldmat[3][0],
                     (f32*)((u8*)enemy->coll_ip + 52));
}

""" + replace_once(live, route_call, "e->route = enemy_item_route(e);")
    live_entry = "    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + ENEMY_POOL_OFF);"
    yield "entry_register_direct", replace_once(live, live_entry, "    register Enemy* e = &gEnemies[index];")
    # The declaration-initializer axis is distinct from pointer spelling.
    separate = replace_once(live, live_entry, "    Enemy* e;")
    initializers = [("s32 alg = e->algorithm;", "s32 alg;", "alg = e->algorithm;"),
                    ("f32 rad = e->rad;", "f32 rad;", "rad = e->rad;"),
                    ("f32 hht = e->hht;", "f32 hht;", "hht = e->hht;"),
                    ("s32 blocked = 0;", "s32 blocked;", "blocked = 0;")]
    for old, decl, assignment in initializers:
        separate = replace_once(separate, old, decl)
    stmts = "    e = &gEnemies[index];\n" + "\n".join("    " + x[2] for x in initializers) + "\n\n"
    yield "entry_assigned_direct", replace_once(separate, "    /* stun freeze + knockback integration */", stmts + "    /* stun freeze + knockback integration */")


def resource_variants(body):
    """New joint alias/propagation axis; old index-order spelling is vetoed."""
    second = """    for (i = 0; i < 8; i++) {
        p[8 + i] = -1;
    }"""
    scoped = replace_once(body, second, """    for (i = 0; i < 8; i++) {
        s32* row = p + i;
        row[8] = -1;
    }""")
    both = replace_once(scoped, """        p[345 + i] = 0;
        p[300 + i] = -1;
        p[255 + i] = 0;""", """        s32* row = p + i;
        row[345] = 0;
        row[300] = -1;
        row[255] = 0;""")
    def pragma(text, name):
        return "#pragma " + name + " off\n" + text + "\n#pragma " + name + " reset\n"
    yield "propagation_off", pragma(body, "opt_propagation")
    yield "propagation_off_second_alias", pragma(scoped, "opt_propagation")
    yield "propagation_off_both_aliases", pragma(both, "opt_propagation")
    yield "common_subs_off_both_aliases", pragma(both, "opt_common_subs")
    # A second source induction variable has a distinct declaration identity,
    # unlike commuting the operands of the same old indexed expression.
    separate = replace_once(body, "    s32 i;", "    s32 i;\n    s32 j;")
    separate = replace_once(separate, second, re.sub(r"\bi\b", "j", second))
    yield "separate_loop_index", separate
    yield "separate_loop_index_decl_first", separate.replace("    s32 i;\n    s32 j;", "    s32 j;\n    s32 i;")
    # Separate invariant identities per store, with their existing values
    # and store chronology fixed. No register names or forced bytes.
    constants = replace_once(body, "    s32 i;", "    s32 i;\n    s32 empty = -1;\n    s32 unused = -1;")
    constants = constants.replace("p[300 + i] = -1", "p[300 + i] = empty")
    constants = constants.replace("p[8 + i] = -1", "p[8 + i] = unused")
    yield "distinct_invariant_identities", constants
    joint = pragma(scoped, "opt_propagation")
    yield "joint_pool_decl_last", replace_once(joint, "    s32* p = lbl_80250E00;\n    s32 i;", "    s32 i;\n    s32* p = lbl_80250E00;")
    yield "joint_pool_assigned", replace_once(joint, "    s32* p = lbl_80250E00;\n    s32 i;", "    s32* p;\n    s32 i;\n\n    p = lbl_80250E00;")
    for order in ("row_first", "row_last"):
        top_row = replace_once(joint, "        s32* row = p + i;", "        row = p + i;")
        decl = "    s32* row;\n"
        top_row = replace_once(top_row, "    s32* p = lbl_80250E00;\n    s32 i;", (decl if order == "row_first" else "") + "    s32* p = lbl_80250E00;\n    s32 i;" + ("\n" + decl.rstrip() if order == "row_last" else ""))
        yield "joint_" + order, top_row
    for typ in ("s32", "u32", "s16"):
        zeros = replace_once(joint, "    s32 i;", "    s32 i;\n    " + typ + " no_resource = 0;\n    " + typ + " unloaded = 0;")
        zeros = zeros.replace("p[345 + i] = 0", "p[345 + i] = no_resource")
        zeros = zeros.replace("p[255 + i] = 0", "p[255 + i] = unloaded")
        yield "joint_named_zeros_" + typ, zeros
    yield "joint_both_aliases_register_pool", pragma(both.replace("s32* p =", "register s32* p ="), "opt_propagation")
    first = """    for (i = 0; i < 45; i++) {
        p[345 + i] = 0;
        p[300 + i] = -1;
        p[255 + i] = 0;
    }"""
    helper = "static inline void enemy_reset_resource_rows(s32* p)\n{\n    s32 i;\n" + first + "\n}\n\n"
    yield "joint_inline_resource_reset", pragma(helper + scoped.replace(first, "    enemy_reset_resource_rows(p);"), "opt_propagation")
    helper = "static inline void enemy_reset_resource_indices(s32* p)\n{\n    s32 i;\n" + scoped[scoped.index("    for (i = 0; i < 8;"):scoped.index("    lbl_8034471C")] + "}\n\n"
    caller = scoped[:scoped.index("    for (i = 0; i < 8;")] + "    enemy_reset_resource_indices(p);\n" + scoped[scoped.index("    lbl_8034471C"):]
    yield "joint_inline_index_reset", pragma(helper + caller, "opt_propagation")


def milestone_variants(body):
    """Joint controlled propagation and per-access bases, not raw respelling."""
    def pragma(text):
        return "#pragma opt_propagation off\n" + text + "\n#pragma opt_propagation reset\n"
    first = replace_once(body, "        mp->slots[i] = -1;", "        u8* row = (u8*)mp + i * sizeof(s32);\n        *(s32*)(row + offsetof(MilestonePool, slots)) = -1;")
    store = replace_once(first, "        mp->slots[count] = cur;", "        {\n            u8* row = (u8*)mp + count * sizeof(s32);\n            *(s32*)(row + offsetof(MilestonePool, slots)) = cur;\n        }")
    all_sites = replace_once(store, "            if (mp->slots[k] == cur) {", "            u8* row = (u8*)mp + k * sizeof(s32);\n            if (*(s32*)(row + offsetof(MilestonePool, slots)) == cur) {")
    yield "propagation_off", pragma(body)
    yield "joint_first_alias", pragma(first)
    yield "joint_two_aliases", pragma(store)
    yield "joint_all_aliases", pragma(all_sites)
    yield "joint_previous_as_argument", pragma(all_sites.replace("cur = fn_800511D0(cur,", "cur = fn_800511D0(prev,"))
    yield "joint_comparison_operand", pragma(all_sites.replace("if (*(s32*)(row + offsetof(MilestonePool, slots)) == cur)", "if (cur == *(s32*)(row + offsetof(MilestonePool, slots)))"))
    explicit = all_sites.replace("        cur = fn_800511D0(cur, lbl_80346984);", "        {\n            s32 next = fn_800511D0(prev, lbl_80346984);\n            cur = next;\n        }")
    yield "joint_next_result", pragma(explicit)
    closed = all_sites.replace("cur = fn_800511D0(cur,", "cur = fn_800511D0(prev,")
    closed = closed.replace("if (*(s32*)(row + offsetof(MilestonePool, slots)) == cur)", "if (cur == *(s32*)(row + offsetof(MilestonePool, slots)))")
    yield "joint_previous_and_operand", pragma(closed)
    single = replace_once(closed, "            f32 d2 = dx * dx + dy * dy;\n\n            d2 = dz * dz + d2;", "            f32 d2 = dz * dz + (dx * dx + dy * dy);")
    yield "joint_single_distance_expression", pragma(single)
    for base, name in ((closed, "split_distance"), (single, "single_distance")):
        pointer_first = replace_once(base, "        f32 bestDist = lbl_803468B0;\n        u8* m = sMilestones;", "        u8* m = sMilestones;\n        f32 bestDist = lbl_803468B0;")
        yield "joint_pointer_first_" + name, pragma(pointer_first)
        top = replace_once(base, "    s32 best;", "    u8* m;\n    s32 best;")
        top = replace_once(top, "        u8* m = sMilestones;", "        m = sMilestones;")
        yield "joint_pointer_top_" + name, pragma(top)
    yield "joint_cur_decl_first", pragma(replace_once(single, "    s32 best;\n    s32 cur;\n    s32 i;", "    s32 cur;\n    s32 best;\n    s32 i;"))
    yield "joint_i_decl_first", pragma(replace_once(single, "    s32 best;\n    s32 cur;\n    s32 i;", "    s32 i;\n    s32 best;\n    s32 cur;"))
    count_top = replace_once(single, "    s32 best;", "    s32 count;\n    s32 best;")
    count_top = replace_once(count_top, "        s32 count = lbl_80344724;\n        s32 prev = cur;\n        s32 k;", "        s32 prev;\n        s32 k;\n        count = lbl_80344724;\n        prev = cur;")
    yield "joint_count_decl_top", pragma(count_top)
    prefix, tail = single.split("    cur = best;", 1)
    for old, new in (("count", "i"), ("k", "i"), ("cur", "best")):
        part = tail.replace("s32 " + old + " =", old + " =").replace("        s32 " + old + ";\n", "")
        if old == "count":
            part = replace_once(part, "        count = lbl_80344724;\n        s32 prev = cur;\n        s32 k;", "        s32 prev;\n        s32 k;\n        count = lbl_80344724;\n        prev = cur;")
        part = re.sub(r"\b" + old + r"\b", new, part)
        lead = prefix + "    cur = best;"
        if old == "cur":
            lead = prefix.replace("    s32 cur;\n", "")
        yield "joint_reuse_" + old + "_as_" + new, pragma(lead + part)


def formatter_variants(body):
    """Recover the live integer vararg before testing code-shape levers."""
    arg = replace_once(body, 'sprintf(buf, "%s%d", findWorldName(world));', 'sprintf(buf, "%s%d", findWorldName(world), n);')
    yield "live_numeric_argument", arg
    old = """    if (lvl == 0) {
        n = 1;
    } else {
        n = lvl;
    }"""
    normalized = replace_once(arg, old, "    n = lvl;\n    if (lvl == 0) n = 1;")
    yield "argument_and_normalized_local", normalized
    yield "argument_normalized_buffer_decl_first", replace_once(normalized, "    s32 n;\n    u32 i;\n    char* buf;", "    char* buf;\n    s32 n;\n    u32 i;")
    # The two range-test branch pairs can be an inlined helper's returns;
    # a caller-level goto is specifically the previously measured dead axis.
    start, end = normalized.index("    if (lvl >= 4)"), normalized.index("suffix:\n")
    helper = """static inline void enemy_format_world_level(char* buf, s32 world, s32 lvl, s32 n)
{
    if (lvl < 4 || lvl >= 8) {
        sprintf(buf, "%s%d", findWorldName(world), n);
        return;
    }
    sprintf(buf, "%s%c", findWorldName(world), (&lbl_80343BF8[n])[-4]);
}

"""
    yield "inline_format_region", helper + normalized[:start] + "    enemy_format_world_level(buf, world, lvl, n);\n" + normalized[end+len("suffix:\n"):]
    choose = """static inline s32 enemy_lettered_level(s32 lvl)
{
    if (lvl < 4) return 0;
    if (lvl >= 8) return 0;
    return 1;
}

"""
    branch = normalized[:start] + """    if (enemy_lettered_level(lvl)) {
        sprintf(buf, "%s%c", findWorldName(world), (&lbl_80343BF8[n])[-4]);
    } else {
        sprintf(buf, "%s%d", findWorldName(world), n);
    }
""" + normalized[end+len("suffix:\n"):]
    yield "inline_range_predicate", choose + branch
    switch = """    switch (lvl) {
    default:
        sprintf(buf, "%s%d", findWorldName(world), n);
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        sprintf(buf, "%s%c", findWorldName(world), (&lbl_80343BF8[n])[-4]);
        break;
    }
"""
    switched = normalized[:start] + switch + normalized[end+len("suffix:\n"):]
    yield "switch_default_first", switched
    early = replace_once(switched, "    buf = (char*)lbl_80250E00;\n    n = lvl;\n    if (lvl == 0) n = 1;", "    n = lvl;\n    if (lvl == 0) n = 1;\n    buf = (char*)lbl_80250E00;")
    yield "switch_normalize_before_buffer", early
    yield "switch_propagation_off", "#pragma opt_propagation off\n" + switched + "\n#pragma opt_propagation reset\n"
    init = replace_once(switched, "    char* buf;", "    char* buf = (char*)lbl_80250E00;")
    init = replace_once(init, "    buf = (char*)lbl_80250E00;\n", "")
    yield "switch_buffer_initializer", init
    # Full formatting helper with a pointer result: unlike a predicate it
    # returns the actual value the caller consumes, without a synthetic flag.
    format_helper = """static inline char* enemy_format_world_level(s32 world, s32 lvl)
{
    s32 n = lvl;
    char* buf = (char*)lbl_80250E00;
    if (lvl == 0) n = 1;
    if (lvl < 4 || lvl >= 8) {
        sprintf(buf, "%s%d", findWorldName(world), n);
        return buf;
    }
    sprintf(buf, "%s%c", findWorldName(world), (&lbl_80343BF8[n])[-4]);
    return buf;
}

"""
    consumer = "char* fn_80051E1C(s32 world, s32 lvl, s32 flag)\n{\n    u32 i;\n    char* buf = enemy_format_world_level(world, lvl);\n" + normalized[end+len("suffix:\n"):]
    yield "inline_return_buffer", format_helper + consumer
    # Explicit inline lookup inside the formatter. The existing bare-static
    # lookup is not auto-inlined at this additional nesting depth.
    lookup = """static inline char* enemy_world_label(s32 world)
{
    s32 off = 0;
    s32 i;
    for (i = 0; i < 44; i++) {
        u8* e = (u8*)((Row36*)lbl_8011AF48) + off;
        if (world == *(s32*)e) return (char*)(e + 4);
        off += 36;
    }
    return 0;
}

"""
    yield "inline_return_buffer_explicit_lookup", lookup + format_helper.replace("findWorldName(world)", "enemy_world_label(world)") + consumer
    yield "inline_void_explicit_lookup", lookup + helper.replace("findWorldName(world)", "enemy_world_label(world)") + normalized[:start] + "    enemy_format_world_level(buf, world, lvl, n);\n" + normalized[end+len("suffix:\n"):]
    conditional = normalized[:start] + """    if (lvl < 4) {
        sprintf(buf, "%s%d", findWorldName(world), n);
    } else if (lvl < 8) {
        sprintf(buf, "%s%c", findWorldName(world), (&lbl_80343BF8[n])[-4]);
    } else {
        sprintf(buf, "%s%d", findWorldName(world), n);
    }
""" + normalized[end+len("suffix:\n"):]
    yield "explicit_three_way_intervals", conditional
    # Preserve the explicit normalizer while varying only the range region's
    # optimizer boundary. This is a local control, not a compiler patch.
    yield "switch_scheduling_off", "#pragma scheduling off\n" + switched + "\n#pragma scheduling reset\n"
    for low in (0, 1):
        labels = "".join("    case %d:\n" % i for i in range(low, 4))
        yield "switch_numbered_cases_" + str(low), replace_once(switched, "    default:\n", labels + "    default:\n")
    numbered = replace_once(switched, "    default:\n", "    case 1:\n    case 2:\n    case 3:\n    default:\n")
    yield "numbered_const_buffer", replace_once(numbered, "    char* buf;\n\n    buf = (char*)lbl_80250E00;", "    char* const buf = (char*)lbl_80250E00;")
    void_buffer = replace_once(numbered, "    char* buf;", "    void* buf;")
    void_buffer = void_buffer.replace("buf[i]", "((char*)buf)[i]")
    yield "numbered_void_buffer", void_buffer
    unsigned_buffer = replace_once(numbered, "    char* buf;", "    u8* buf;")
    unsigned_buffer = unsigned_buffer.replace("buf[i]", "((char*)buf)[i]")
    unsigned_buffer = unsigned_buffer.replace("buf = (char*)lbl_80250E00", "buf = (u8*)lbl_80250E00")
    for callee in ("sprintf", "strcat", "strlen"):
        unsigned_buffer = unsigned_buffer.replace(callee + "(buf", callee + "((char*)buf")
    unsigned_buffer = unsigned_buffer.replace("return buf;", "return (char*)buf;")
    yield "numbered_unsigned_buffer", unsigned_buffer
    before_test = replace_once(numbered, "    buf = (char*)lbl_80250E00;\n    n = lvl;", "    n = lvl;\n    buf = (char*)lbl_80250E00;")
    yield "numbered_n_before_buffer_before_test", before_test
    # Nested-inline experiment: only retained if the compiler actually emits
    # the nested lookup (the previous controls retained a bl to it).
    yield "nested_inline_depth_two", "#pragma inline_depth(2)\n" + format_helper + consumer + "\n#pragma inline_depth(0)\n"
    hstart = format_helper.index("    if (lvl < 4 || lvl >= 8)")
    hswitch = """    switch (lvl) {
    case 1:
    case 2:
    case 3:
    default:
        sprintf(buf, "%s%d", findWorldName(world), n);
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        sprintf(buf, "%s%c", findWorldName(world), (&lbl_80343BF8[n])[-4]);
        break;
    }
    return buf;
}

"""
    nested_switch = format_helper[:hstart] + hswitch
    yield "nested_inline_numbered_switch", "#pragma inline_depth(2)\n" + nested_switch + consumer + "\n#pragma inline_depth(0)\n"
    upper = """static inline void enemy_uppercase_level_name(char* buf)
{
    u32 i;
    for (i = 0; i < strlen(buf); i++) {
        buf[i] = toupper(buf[i]);
    }
}

"""
    loop = """    for (i = 0; i < strlen(buf); i++) {
        buf[i] = toupper(buf[i]);
    }"""
    upper_consumer = consumer.replace("    u32 i;\n", "").replace(loop, "    enemy_uppercase_level_name(buf);")
    yield "nested_switch_and_uppercase_helper", "#pragma inline_depth(2)\n" + nested_switch + upper + upper_consumer + "\n#pragma inline_depth(0)\n"
    yield "nested_switch_caller_strength_off", "#pragma inline_depth(2)\n" + nested_switch + "#pragma opt_strength_reduction off\n" + consumer + "\n#pragma opt_strength_reduction reset\n#pragma inline_depth(0)\n"
    addressed_consumer = consumer.replace(loop, """    for (i = 0; i < strlen(buf); i++) {
        char* character = buf + i;
        *character = toupper(*character);
    }""")
    yield "nested_switch_character_alias", "#pragma inline_depth(2)\n" + nested_switch + addressed_consumer + "\n#pragma inline_depth(0)\n"
    yield "nested_switch_alias_propagation_off", "#pragma inline_depth(2)\n" + nested_switch + "#pragma opt_propagation off\n" + addressed_consumer + "\n#pragma opt_propagation reset\n#pragma inline_depth(0)\n"
    assigned = addressed_consumer.replace("    char* buf = enemy_format_world_level(world, lvl);", "    char* buf;\n    buf = enemy_format_world_level(world, lvl);")
    yield "nested_switch_buffer_assigned", "#pragma inline_depth(2)\n" + nested_switch + assigned + "\n#pragma inline_depth(0)\n"
    declared_first = assigned.replace("    u32 i;\n    char* buf;", "    char* buf;\n    u32 i;")
    yield "nested_switch_buffer_first_assigned", "#pragma inline_depth(2)\n" + nested_switch + declared_first + "\n#pragma inline_depth(0)\n"
    scope_char = assigned.replace("    u32 i;", "    char* character;\n    u32 i;").replace("        char* character = buf + i;", "        character = buf + i;")
    yield "nested_switch_character_function_scope", "#pragma inline_depth(2)\n" + nested_switch + scope_char + "\n#pragma inline_depth(0)\n"


def enemy_type_variants(body):
    if 'ErrorPrintf(lbl_801124EC, name, w, l);' not in body:
        raise ValueError("restore the target's two live diagnostic arguments first")
    yield "index_decl_before_name", replace_once(body, "        char* name;\n        s32 i;", "        s32 i;\n        char* name;")
    start, end = body.index("        char* name;"), body.index("    return result;")
    direct = body[:start] + "        ErrorPrintf(lbl_801124EC, findWorldName(w), w, l);\n    }\n" + body[end:]
    yield "existing_inline_name_argument", direct
    yield "existing_inline_name_local", body[:start] + "        char* name = findWorldName(w);\n        ErrorPrintf(lbl_801124EC, name, w, l);\n    }\n" + body[end:]
    named_base = replace_once(body, "        char* name;", "        char* name = (char*)lbl_8011AF48;")
    named_base = named_base.replace("((Row36*)lbl_8011AF48)[i]", "((Row36*)name)[i]")
    yield "reuse_name_as_table_base", named_base
    for control in ("scheduling", "opt_propagation", "opt_common_subs"):
        yield "inline_argument_" + control + "_off", "#pragma " + control + " off\n" + direct + "\n#pragma " + control + " reset\n"


def enemy_type_control_variants(body):
    if 'ErrorPrintf(lbl_801124EC, findWorldName(w), w, l);' not in body:
        raise ValueError("controls require the retained live-argument inline lookup")
    for control in ("scheduling", "opt_propagation", "opt_common_subs"):
        yield control + "_off", "#pragma " + control + " off\n" + body + "\n#pragma " + control + " reset\n"


def target_player_variants(body):
    start = body.index("                    if (*(s16*)(p + offsetof(Player, field_A1C)) > 2)")
    end = body.index("                    range = dist;", start)
    selection = body[start:end]
    scoped = body[:start] + "                    {\n                        f32 selectedDist;\n" + selection.replace("DIST3(dist,", "DIST3(selectedDist,") + "                        dist = selectedDist;\n                    }\n" + body[end:]
    yield "selection_block_local", scoped
    yield "range_assignment_condition", replace_once(body, "                    range = dist;\n                    if (range >", "                    if ((range = dist) >")
    helper = """static inline f32 enemy_player_distance(u8* e, u8* p, f32 kZero, f64 kHalf, f64 kThree)
{
    f32 distance;
""" + selection.replace("DIST3(dist,", "DIST3(distance,") + "    return distance;\n}\n\n"
    called = body[:start] + "                    dist = enemy_player_distance(e, p, kZero, kHalf, kThree);\n" + body[end:]
    yield "distance_return_helper", helper + called
    yield "distance_return_helper_chained_assignment", helper + replace_once(called, "                    dist = enemy_player_distance(e, p, kZero, kHalf, kThree);\n                    range = dist;", "                    range = dist = enemy_player_distance(e, p, kZero, kHalf, kThree);")
    yield "distance_return_helper_chained_condition", helper + replace_once(called, "                    dist = enemy_player_distance(e, p, kZero, kHalf, kThree);\n                    range = dist;\n                    if (range >", "                    if ((range = dist = enemy_player_distance(e, p, kZero, kHalf, kThree)) >")
    yield "distance_return_helper_condition", helper + replace_once(called, "                    range = dist;\n                    if (range >", "                    if ((range = dist) >")
    yield "distance_return_helper_f64", helper.replace("inline f32 enemy_player_distance", "inline f64 enemy_player_distance") + called
    yield "range_block_scope", body.replace("    f32 range;\n", "").replace("                for (; i < 4; i++, p += 13148) {", "                for (; i < 4; i++, p += 13148) {\n                    f32 range;")
    yield "dist_block_scope", body.replace("    f32 dist;\n", "").replace("                for (; i < 4; i++, p += 13148) {", "                for (; i < 4; i++, p += 13148) {\n                    f32 dist;")
    # The four sqrt truncation slots are below every function-scope local
    # in the macro baseline. Explicit hoisting changes scope, not memory
    # accesses, and leaves the existing nominal local allocation unchanged.
    macro = re.search(r"^#define DIST3\([^\n]+", (REPO / "src/game/enemy/enemy.c").read_text(encoding="utf-8"), re.M).group(0)
    expansion = macro[macro.index("     {"):].strip()
    calls = list(re.finditer(r"DIST3\((.*?)\);", body, re.S))
    if len(calls) != 4:
        raise ValueError("expected exactly four distance expansions")
    hoisted = body
    for number, match in reversed(list(enumerate(calls))):
        parts, depth, at = [], 0, 0
        for pos, char in enumerate(match[1]):
            if char == "(": depth += 1
            if char == ")": depth -= 1
            if char == "," and depth == 0:
                parts.append(match[1][at:pos].strip()); at = pos + 1
        parts.append(match[1][at:].strip())
        if len(parts) != 6:
            raise ValueError("DIST3 argument split failed")
        text = expansion.replace("volatile f32 tmp_;", "")
        for key, value in zip(("dst", "av", "bv", "kZ", "kH", "kT"), parts):
            text = re.sub(r"\b" + key + r"\b", lambda unused: value, text)
        text = text.replace("tmp_", "distanceScratch" + str(number))
        hoisted = hoisted[:match.start()] + text + hoisted[match.end():]
    decls = "    volatile f32 distanceScratch0, distanceScratch1, distanceScratch2, distanceScratch3;\n"
    hoisted = replace_once(hoisted, "    f32 ad;\n", "    f32 ad;\n" + decls)
    yield "sqrt_scratch_function_scope", hoisted
    yield "scratch_scope_and_range_condition", replace_once(hoisted, "                    range = dist;\n                    if (range >", "                    if ((range = dist) >")
    for control in ("opt_propagation", "opt_common_subs", "scheduling"):
        yield control + "_off", "#pragma " + control + " off\n" + hoisted + "\n#pragma " + control + " reset\n"
        yield "return_helper_" + control + "_off", "#pragma " + control + " off\n" + helper + called + "\n#pragma " + control + " reset\n"
        yield "helper_only_" + control + "_off", "#pragma " + control + " off\n" + helper + "\n#pragma " + control + " reset\n" + called
    # One expression gives the sum a transient identity instead of writing
    # the final distance home twice before sqrt. Floating grouping is fixed.
    summed = hoisted.replace("(dist) = dx_ * dx_ + dy_ * dy_;         (dist) = dz_ * dz_ + (dist);", "(dist) = dz_ * dz_ + (dx_ * dx_ + dy_ * dy_);")
    yield "single_distance_expression", summed
    yield "single_expression_range_condition", replace_once(summed, "                    range = dist;\n                    if (range >", "                    if ((range = dist) >")
    out_helper = helper.replace("inline f32 enemy_player_distance(u8* e", "inline void enemy_player_distance(f32* distance, u8* e").replace("    f32 distance;\n", "").replace("DIST3(distance,", "DIST3(*distance,").replace("    return distance;\n", "")
    out_caller = called.replace("dist = enemy_player_distance(e, p, kZero, kHalf, kThree);", "enemy_player_distance(&dist, e, p, kZero, kHalf, kThree);")
    yield "distance_output_pointer", out_helper + out_caller
    yield "distance_output_pointer_propagation_off", "#pragma opt_propagation off\n" + out_helper + out_caller + "\n#pragma opt_propagation reset\n"
    for name, variant in (("hoisted", hoisted), ("return_helper", helper + called)):
        yield name + "_dist_range_decl_exchange", variant.replace("    f32 dist;", "    f32 range;").replace("    f32 range;\n    f32 bestSpecial;", "    f32 dist;\n    f32 bestSpecial;")
        # DIAGNOSTIC ONLY: self-assignment isolates assignment-expression
        # lowering but is artificial and MUST NOT be retained in src/.
        # The later measuredDistance block is the ordinary-source control.
        yield name + "_dist_range_chained", variant.replace("                    range = dist;", "                    range = (dist = dist);")
    one_sum = summed.replace("(dist) = dz_ * dz_ + (dx_ * dx_ + dy_ * dy_);         if ((dist) >", "if (((dist) = dz_ * dz_ + (dx_ * dx_ + dy_ * dy_)) >")
    yield "summed_distance_assignment_condition", one_sum
    hs = hoisted.index("                    if (*(s16*)(p + offsetof(Player, field_A1C)) > 2)")
    he = hoisted.index("                    range = dist;", hs)
    staged = hoisted[:hs] + "                    {\n                        f32 measuredDistance;\n" + re.sub(r"\bdist\b", "measuredDistance", hoisted[hs:he]) + "                        range = dist = measuredDistance;\n                    }\n" + hoisted[he+len("                    range = dist;\n"):]
    yield "caller_staged_distance_chain", staged
    yield "caller_staged_distance_chain_no_low_pad", staged.replace("    u8 padLo[4];\n", "")
    transit = hoisted.replace("    f32 range;", "    f32 measuredRange;\n    f32 range;").replace("                    range = dist;", "                    range = measuredRange = dist;").replace("if (range > *(f32*)(e + offsetof(Enemy, sight)))", "if (measuredRange > *(f32*)(e + offsetof(Enemy, sight)))")
    yield "caller_measurement_guard_chain", transit
    yield "caller_measurement_guard_chain_no_low_pad", transit.replace("    u8 padLo[4];\n", "")
    chain_called = replace_once(called, "                    dist = enemy_player_distance(e, p, kZero, kHalf, kThree);\n                    range = dist;", "                    range = dist = enemy_player_distance(e, p, kZero, kHalf, kThree);")
    expanded_helper = helper
    for number, match in reversed(list(enumerate(re.finditer(r"DIST3\((.*?)\);", helper, re.S)))):
        parts, depth, at = [], 0, 0
        for pos, char in enumerate(match[1]):
            if char == "(": depth += 1
            if char == ")": depth -= 1
            if char == "," and depth == 0:
                parts.append(match[1][at:pos].strip()); at = pos + 1
        parts.append(match[1][at:].strip())
        text = expansion.replace("volatile f32 tmp_;", "")
        for key, value in zip(("dst", "av", "bv", "kZ", "kH", "kT"), parts):
            text = re.sub(r"\b" + key + r"\b", lambda unused: value, text)
        text = text.replace("tmp_", "helperScratch" + str(number))
        expanded_helper = expanded_helper[:match.start()] + text + expanded_helper[match.end():]
    expanded_helper = expanded_helper.replace("    f32 distance;", "    volatile f32 helperScratch0, helperScratch1;\n    f32 distance;")
    yield "chain_helper_scratch_scope", expanded_helper + chain_called
    for moved in (4, 8, 12):
        # Repartition existing nominal allocation; never add dead bytes.
        h = expanded_helper.replace("    f32 distance;", "    f32 distance;\n    u8 localGap[" + str(moved) + "];")
        c = chain_called.replace("u8 unused[44]", "u8 unused[" + str(44 - moved) + "]")
        yield "chain_helper_partition_" + str(moved), h + c
    h = expanded_helper.replace("    f32 distance;", "    f32 distance;\n    u8 localGap[4];")
    yield "chain_move_existing_low_pad", h + chain_called.replace("    u8 padLo[4];\n", "")
    external_scratch = expanded_helper.replace("f64 kThree)", "f64 kThree, volatile f32* scratch0, volatile f32* scratch1)").replace("    volatile f32 helperScratch0, helperScratch1;\n", "").replace("helperScratch0", "(*scratch0)").replace("helperScratch1", "(*scratch1)")
    explicit_caller = chain_called.replace("    f32 ad;", "    f32 ad;\n    volatile f32 scanScratch0, scanScratch1;").replace("enemy_player_distance(e, p, kZero, kHalf, kThree)", "enemy_player_distance(e, p, kZero, kHalf, kThree, &scanScratch0, &scanScratch1)")
    yield "chain_caller_owns_scratch", external_scratch + explicit_caller
    yield "chain_caller_scratch_before_ad", external_scratch + explicit_caller.replace("    f32 ad;\n    volatile f32 scanScratch0, scanScratch1;", "    volatile f32 scanScratch0, scanScratch1;\n    f32 ad;")
    hs = hoisted.index("                    if (*(s16*)(p + offsetof(Player, field_A1C)) > 2)")
    he = hoisted.index("                    range = dist;", hs) + len("                    range = dist;")
    all_scratch = hoisted[:hs] + "                    range = dist = enemy_player_distance(e, p, kZero, kHalf, kThree, &distanceScratch2, &distanceScratch3);" + hoisted[he:]
    yield "chain_all_caller_scratch", external_scratch + all_scratch
    yield "chain_all_caller_scratch_no_low_pad", external_scratch + all_scratch.replace("    u8 padLo[4];\n", "")
    # Nested natural helpers: the per-vector operation and the player-view
    # selection own separate locals, with no caller-supplied rounding slots.
    vector_helper = """static inline f32 enemy_distance3(f32* av, f32* bv, f32 zero, f64 half, f64 three)
{
    f32 distance;
    DIST3(distance, av, bv, zero, half, three);
    return distance;
}

"""
    nested_helper = helper
    for match in reversed(list(re.finditer(r"DIST3\((.*?)\);", helper, re.S))):
        args = match[1].split(",", 1)[1]
        nested_helper = nested_helper[:match.start()] + "distance = enemy_distance3(" + args + ");" + nested_helper[match.end():]
    yield "chain_nested_vector_helper", "#pragma inline_depth(2)\n" + vector_helper + nested_helper + chain_called + "\n#pragma inline_depth(0)\n"
    yield "chain_nested_vector_no_low_pad", "#pragma inline_depth(2)\n" + vector_helper + nested_helper + chain_called.replace("    u8 padLo[4];\n", "") + "\n#pragma inline_depth(0)\n"
    # The target's exact point of divergence is the value transfer at the
    # join. Keep that transfer as a real chained result while allowing the
    # nested distance calculation to write its caller-owned scratch slots.
    for name, variant in (("plain", body), ("hoisted", hoisted)):
        yield name + "_range_double", variant.replace("    f32 range;", "    f64 range;").replace("range += *(f32*)(p + 2600);", "range = (f32)(range + *(f32*)(p + 2600));")
        # A meaningful inline split: evaluate the chosen player's distance,
        # visibility and special-shield priority. Each former continue exits
        # only this one player's consideration, never the caller's loop.
        tail_start = variant.index("                    range = dist;")
        tail_end = variant.index("\n                }", tail_start)
        tail = variant[tail_start:tail_end].replace("continue;", "return bestSpecial;")
        consumer = """static inline f32 enemy_consider_player(u8* e, u8* p, s32 i, f32 dist, f32 bestSpecial, f64 kK, f64 kPi)
{
    f32 ad;
    f32 range;
""" + tail + "\n    return bestSpecial;\n}\n\n"
        caller = variant[:tail_start] + "                    bestSpecial = enemy_consider_player(e, p, i, dist, bestSpecial, kK, kPi);" + variant[tail_end:]
        # Leave the now-dead caller declarations: allocation identity is a
        # separate measured axis, not silently coupled to helper extraction.
        yield name + "_visibility_helper", consumer + caller
        yield name + "_visibility_helper_remove_dead", consumer + caller.replace("    f32 ad;\n", "").replace("    f32 range;\n", "")


def target_player_color_variants(body):
    block = "    f32 dist;\n    f64 kPi;\n    f32 range;\n    f32 bestSpecial;"
    if block not in body:
        raise ValueError("expected the retained distance/constant/range declaration block")
    decls = block.splitlines()
    for order in itertools.permutations(range(4)):
        if order == (0, 1, 2, 3):
            continue
        yield "declarations_" + "".join(map(str, order)), replace_once(body, block, "\n".join(decls[i] for i in order))
    yield "condition_chain", replace_once(body, "                        range = dist = measuredDistance;\n                    }\n                    if (range >", "                        dist = measuredDistance;\n                    }\n                    if ((range = dist) >")
    yield "scoped_chain_reversed", replace_once(body, "range = dist = measuredDistance;", "dist = range = measuredDistance;")


def compile_baseline(edge, source_path, baseline_path, expected_path, out):
    # Read BEFORE compiling: a historical --baseline-object can be the same
    # pathname as baseline_path. Reading afterward would compare the new
    # output with itself and silently defeat the fidelity gate.
    expected_bytes = expected_path.read_bytes()
    baseline, error = compile_with(dict(edge, src=str(source_path)), edge["mw"],
                                   edge["cflags"], baseline_path, out)
    if error:
        raise RuntimeError(error)
    if baseline.read_bytes() != expected_bytes:
        raise ValueError("whole-object baseline fidelity failed; rebuild first")
    return baseline


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("axis", choices=["pointer", "normalization", "generate", "movement", "resources", "milestones", "formatter", "gettype", "gettype_controls", "target_player", "target_player_colors"])
    ap.add_argument("--show", help="Print normalized target/candidate diff for one existing output")
    ap.add_argument("--compare-baseline", action="store_true", help="With --show, compare the stored raw baseline rather than the retail target")
    ap.add_argument("--source-patch", help="Print an apply_patch-formatted source diff; never writes src/")
    ap.add_argument("--source", type=Path, help="Historical full-TU source, paired with --baseline-object; needed after retaining a candidate")
    ap.add_argument("--baseline-object", type=Path, help="Fresh real-edge object built from --source, for whole-object fidelity")
    args = ap.parse_args()
    function = {"pointer": "fn_80046680", "normalization": "move_logic10", "generate": "generate_enemy", "movement": "do_enemy_move", "resources": "fn_80051164", "milestones": "fn_80051C78", "formatter": "fn_80051E1C", "gettype": "GetEnemyType", "gettype_controls": "GetEnemyType", "target_player": "fn_800516F8", "target_player_colors": "fn_800516F8"}[args.axis]
    edge = read_edges()[UNIT]
    if bool(args.source) != bool(args.baseline_object):
        ap.error("--source and --baseline-object must be provided together")
    if args.source and args.source_patch:
        ap.error("--source-patch only applies to the live TU")
    source_path = args.source.resolve() if args.source else REPO / edge["src"]
    source = source_path.read_text(encoding="utf-8")
    first, last = function_span(source, function)
    lines = source.splitlines(keepends=True)
    start, end = len("".join(lines[:first])), len("".join(lines[:last]))
    body = source[start:end]
    out = REPO / "build" / "r60_enemy_probes" / args.axis
    out.mkdir(parents=True, exist_ok=True)
    if args.source_patch:
        if not re.fullmatch(r"[A-Za-z0-9_]+", args.source_patch):
            raise ValueError("candidate must be a simple name")
        candidate = (out / (args.source_patch + ".c")).read_text(encoding="utf-8")
        # Refuse collateral outside this function: helper insertion may
        # precede its signature, but every pre/post-function byte is fixed.
        if not candidate.startswith(source[:start]) or not candidate.endswith(source[end:]):
            raise ValueError("candidate no longer shares the live TU prefix/suffix; re-run probes")
        rows = list(difflib.unified_diff(source.splitlines(), candidate.splitlines(), n=3))[2:]
        rows = ["@@" if row.startswith("@@") else row for row in rows]
        print("*** Begin Patch\n*** Update File: " + str(REPO / edge["src"]) + "\n" + "\n".join(rows) + "\n*** End Patch")
        return
    if args.show:
        expected = parse(out / "baseline.o" if args.compare_baseline else Path(target_object(UNIT)))[function]
        actual = parse(out / (args.show + ".o"))[function]
        print("\n".join(difflib.unified_diff(expected, actual, fromfile="RAW BASELINE" if args.compare_baseline else "TARGET", tofile=args.show)))
        return

    def compile_source(text, name):
        generated = out / (name + ".c")
        generated.write_text(text, encoding="utf-8")
        obj, error = compile_with(dict(edge, src=str(generated)), edge["mw"],
                                  edge["cflags"], out / (name + ".o"), out)
        if error:
            raise RuntimeError(error)
        return obj

    expected_baseline = args.baseline_object if args.baseline_object else REPO / edge["body_o"]
    baseline = compile_baseline(edge, source_path, out / "baseline.o", expected_baseline, out)
    print("FIDELITY: raw whole-object identity", flush=True)
    (out / "baseline.c").write_text(source, encoding="utf-8")
    target = load(str(target_object(UNIT)), function)[3]
    base = load(str(baseline), function)[3]
    rows = []
    variants = {"pointer": pointer_variants, "normalization": normalization_variants, "generate": generate_variants, "movement": movement_variants, "resources": resource_variants, "milestones": milestone_variants, "formatter": formatter_variants, "gettype": enemy_type_variants, "gettype_controls": enemy_type_control_variants, "target_player": target_player_variants, "target_player_colors": target_player_color_variants}[args.axis]
    for name, candidate in [("baseline", body), *variants(body)]:
        obj = baseline if name == "baseline" else compile_source(source[:start] + candidate + source[end:], name)
        actual = load(str(obj), function)[3]
        row = dict(name=name, function=function, target_insns=len(target)//4,
                   ours_insns=len(actual)//4, raw_exact=actual == target,
                   baseline_identical=actual == base,
                   object=str(obj))
        if len(actual) == len(target):
            row["differing_words"] = sum(actual[i:i+4] != target[i:i+4] for i in range(0, len(target), 4))
        print(json.dumps(row), flush=True)
        rows.append(row)
    (out / "results.json").write_text(json.dumps(rows, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
