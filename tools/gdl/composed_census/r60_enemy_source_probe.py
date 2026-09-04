#!/usr/bin/env python3
"""Bounded enemy source experiments, generated only under build/.

Uses the real Ninja compile edge and requires whole-object baseline fidelity.
No project source or postprocessor rule is changed. Raw body identity is not
a proof of relocation/data equality; retained candidates still need all gates.
"""
import argparse
import difflib
import json
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


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("axis", choices=["pointer", "normalization", "generate", "movement"])
    ap.add_argument("--show", help="Print normalized target/candidate diff for one existing output")
    ap.add_argument("--compare-baseline", action="store_true", help="With --show, compare the stored raw baseline rather than the retail target")
    ap.add_argument("--source-patch", help="Print an apply_patch-formatted source diff; never writes src/")
    ap.add_argument("--source", type=Path, help="Historical full-TU source, paired with --baseline-object; needed after retaining a candidate")
    ap.add_argument("--baseline-object", type=Path, help="Fresh real-edge object built from --source, for whole-object fidelity")
    args = ap.parse_args()
    function = {"pointer": "fn_80046680", "normalization": "move_logic10", "generate": "generate_enemy", "movement": "do_enemy_move"}[args.axis]
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

    baseline, error = compile_with(dict(edge, src=str(source_path)), edge["mw"], edge["cflags"],
                                   out / "baseline.o", out)
    if error:
        raise RuntimeError(error)
    expected_baseline = args.baseline_object if args.baseline_object else REPO / edge["body_o"]
    if baseline.read_bytes() != expected_baseline.read_bytes():
        raise ValueError("whole-object baseline fidelity failed; rebuild first")
    print("FIDELITY: raw whole-object identity", flush=True)
    target = load(str(target_object(UNIT)), function)[3]
    base = load(str(baseline), function)[3]
    rows = []
    variants = {"pointer": pointer_variants, "normalization": normalization_variants, "generate": generate_variants, "movement": movement_variants}[args.axis]
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
