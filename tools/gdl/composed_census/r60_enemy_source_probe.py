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
import struct
from pathlib import Path

from cn_analyze import load, target_object, wf
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


def normalization_arm_result(part, block):
    """Route f32 arithmetic results to the f64 join without retyping inputs."""
    pre, post = part.split(block, 1)
    pre = replace_once(pre, "        f32 cand;", "        f32 cand;\n        f64 normalAngle;")
    for op in ("+", "-"):
        if pre.count("cand = cand " + op + " q[1095];") != 3:
            raise ValueError("expected three float-result arms per operation")
        pre = pre.replace("cand = cand " + op + " q[1095];", "normalAngle = cand " + op + " q[1095];")
    fallback = "            } else {\n                cand = lbl_80344720;\n            }\n"
    pre = replace_once(pre, fallback, fallback.replace("cand =", "normalAngle ="))
    normal = block.replace("                f64 av;\n", "").replace("(av = cand)", "normalAngle")
    normal = re.sub(r"\bav\b", "normalAngle", normal)
    return pre + normal + post


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
    # New width axis: the PRE-normalization arithmetic is explicitly rounded
    # at its old f32 assignment boundaries, not merely at one operand. The
    # earlier double-candidate probe kept a separate double av and permitted
    # context-driven widening of the arithmetic result.
    prefix, remainder = body.split("    case 1: {", 1)
    case1, default = remainder.split("    default: {", 1)
    def rounded_carrier(part, split):
        pre, post = part.split(block, 1)
        name = "workingAngle" if split else "cand"
        pre = re.sub(r"\bcand\b", name, pre)
        pre = replace_once(pre, "f32 " + name + ";", "f64 " + name + ";" + ("\n        f32 cand;" if split else ""))
        for op in ("+", "-"):
            pre = pre.replace(name + " = " + name + " " + op + " q[1095];", name + " = (f32)((f32)" + name + " " + op + " q[1095]);")
        normalized = """            if (%s > 3.141592654) {
                %s -= 6.283185308;
            } else if (%s <= -3.141592654) {
                %s = 6.283185308 + %s;
            }
            cand = (f32)%s;""" % (name, name, name, name, name, name)
        if not split:
            # Post-join single-precision consumers remain single precision.
            post = post.replace("cand - e->angbak", "(f32)cand - e->angbak")
            post = post.replace("sin(cand)", "sin((f32)cand)").replace("cos(cand)", "cos((f32)cand)")
        return pre + normalized + post
    for split in (False, True):
        label = "split" if split else "inplace"
        pieces = [rounded_carrier(case1, split), rounded_carrier(default, split)]
        yield "rounded_" + label + "_both", prefix + "    case 1: {" + pieces[0] + "    default: {" + pieces[1]
        yield "rounded_" + label + "_case1", prefix + "    case 1: {" + pieces[0] + "    default: {" + default
        yield "rounded_" + label + "_default", prefix + "    case 1: {" + case1 + "    default: {" + pieces[1]
        # The aligned six-frsp regression comes from explicitly narrowing
        # the INPUT a second time even though every reaching definition is
        # an f32 load or a result already rounded to f32. Keep the result
        # narrowing but test omitting that redundant input conversion.
        name = "workingAngle" if split else "cand"
        output_only = [part.replace("(f32)((f32)" + name + " + q[1095])", "(f32)(" + name + " + q[1095])").replace("(f32)((f32)" + name + " - q[1095])", "(f32)(" + name + " - q[1095])") for part in pieces]
        yield "rounded_output_" + label + "_both", prefix + "    case 1: {" + output_only[0] + "    default: {" + output_only[1]
        yield "rounded_output_" + label + "_case1", prefix + "    case 1: {" + output_only[0] + "    default: {" + default
        yield "rounded_output_" + label + "_default", prefix + "    case 1: {" + case1 + "    default: {" + output_only[1]
    # Return precision and formal ownership were held fixed in the earlier
    # helper probe; test both explicitly without widening the caller's cand.
    for input_type in ("f32", "f64"):
        for formal in (False, True):
            if formal and input_type == "f32":
                continue  # Would round arithmetic per arm, a semantic change.
            working = "angle" if formal else "a"
            helper = "static inline f64 enemy_normalize_wide(" + input_type + " angle)\n{\n"
            if not formal:
                helper += "    f64 a = angle;\n"
            helper += "    if (" + working + " > 3.141592654) " + working + " -= 6.283185308;\n"
            helper += "    else if (" + working + " <= -3.141592654) " + working + " = 6.283185308 + " + working + ";\n"
            helper += "    return " + working + ";\n}\n\n"
            label = input_type + ("_formal" if formal else "_local")
            yield "wide_return_" + label, helper + body.replace(block, "            cand = enemy_normalize_wide(cand);")
    # Feed each arm's float arithmetic RESULT directly into the double
    # normalization web. Unlike retyping cand, both arithmetic operands
    # remain float. Unlike copying at the join, there is no separate
    # float-result home between the fadds/fsubs and the normalizer.
    arms = [normalization_arm_result(case1, block), normalization_arm_result(default, block)]
    arm_joint = prefix + "    case 1: {" + arms[0] + "    default: {" + arms[1]
    yield "float_arm_result_to_double_both", arm_joint
    entry = "    e = (Enemy*)(e0 + ENEMY_POOL_OFF);\n    e0 += ENEMY_POOL_OFF;"
    yield "float_arm_and_advance_before_alias", replace_once(arm_joint, entry, "    e0 += ENEMY_POOL_OFF;\n    e = (Enemy*)e0;")
    yield "float_arm_and_assignment_advance", replace_once(arm_joint, entry, "    e = (Enemy*)(e0 += ENEMY_POOL_OFF);")
    entry_forms = {
        "chain_enemy_then_bytes": "    e0 = (u8*)(e = (Enemy*)(e0 + ENEMY_POOL_OFF));",
        "typed_array_and_byte_advance": "    e = &gEnemies[index];\n    e0 += ENEMY_POOL_OFF;",
        "named_page_and_byte_advance": "    e = &((EnemyMovePage05*)base)->enemies[index];\n    e0 += ENEMY_POOL_OFF;",
        "advance_typed_view_then_bytes": "    e = (Enemy*)e0;\n    e = (Enemy*)((u8*)e + ENEMY_POOL_OFF);\n    e0 = (u8*)e;",
        "byte_advance_from_enemy": "    e = (Enemy*)(e0 + ENEMY_POOL_OFF);\n    e0 = (u8*)e;",
    }
    for label, replacement in entry_forms.items():
        yield "float_arm_entry_" + label, replace_once(arm_joint, entry, replacement)
    yield "float_arm_entry_register_enemy", replace_once(arm_joint, "    Enemy* e;", "    register Enemy* e;")
    late_alias = replace_once(arm_joint, entry, "    e0 += ENEMY_POOL_OFF;")
    for label, anchor in (("after_base", "    t = base;"), ("after_type_index", "    t += type * 4;"), ("after_speed", "    speed = *(f32*)(t + offsetof(EnemyMovePage05, speed));")):
        yield "float_arm_entry_alias_" + label, replace_once(late_alias, anchor, anchor + "\n    e = (Enemy*)e0;")
    # Do not retype the public parameter in isolation: the TU's two earlier
    # declarations use s32 (long), and an int definition conflicts with them.
    setup = "    type = *(s32*)(e0 + OFF_E(type));\n" + entry
    for output_bytes in (False, True):
        helper = "static inline s32 enemy_type_and_record(u8* base, s32 index, "
        helper += "u8** record" if output_bytes else "Enemy** record"
        helper += ")\n{\n    Enemy* enemy = &((EnemyMovePage05*)base)->enemies[index];\n    *record = "
        helper += "(u8*)enemy" if output_bytes else "enemy"
        helper += ";\n    return enemy->type;\n}\n\n"
        caller = replace_once(arm_joint, "    u8* e0 = base + index * 916;", "    u8* e0;")
        call = "    type = enemy_type_and_record(base, index, " + ("&e0" if output_bytes else "&e") + ");\n"
        call += "    e = (Enemy*)e0;" if output_bytes else "    e0 = (u8*)e;"
        yield "float_arm_entry_type_record_" + ("bytes" if output_bytes else "typed"), helper + replace_once(caller, setup, call)
        ordered_helper = "static inline s32 enemy_type_and_record(u8* base, s32 index, " + ("u8** record" if output_bytes else "Enemy** record") + ")\n{\n"
        ordered_helper += "    u8* row = base + index * sizeof(Enemy);\n    s32 type = *(s32*)(row + OFF_E(type));\n"
        ordered_helper += "    *record = " + ("row + ENEMY_POOL_OFF" if output_bytes else "(Enemy*)(row + ENEMY_POOL_OFF)") + ";\n    return type;\n}\n\n"
        yield "float_arm_entry_type_before_record_" + ("bytes" if output_bytes else "typed"), ordered_helper + replace_once(caller, setup, call)
    yield "float_arm_result_to_double_case1", prefix + "    case 1: {" + arms[0] + "    default: {" + default
    yield "float_arm_result_to_double_default", prefix + "    case 1: {" + case1 + "    default: {" + arms[1]
    shared = replace_once(body, "    f32 speed;", "    f32 speed;\n    f64 normalizedAngle;")
    shared = shared.replace(block, block.replace("                f64 av;\n", "").replace("av", "normalizedAngle"))
    yield "shared_function_normalizer", shared
    if body.count("        f32 cand;\n") != 3:
        raise ValueError("expected one candidate declaration per case")
    for all_cases in (False, True):
        if all_cases:
            hoisted = body.replace("        f32 cand;\n", "")
        else:
            first, tail = body.split("    case 1: {", 1)
            hoisted = first + "    case 1: {" + tail.replace("        f32 cand;\n", "")
        label = "all" if all_cases else "last_two"
        for position in ("before", "after"):
            decls = "    f32 cand;\n    f32 speed;" if position == "before" else "    f32 speed;\n    f32 cand;"
            yield "shared_candidate_" + label + "_" + position, replace_once(hoisted, "    f32 speed;", decls)


def move_entry_variants(body):
    if body.count("f64 normalAngle;") != 2:
        raise ValueError("entry probes require both recovered normalization result webs")
    entry = "    e = (Enemy*)(e0 + ENEMY_POOL_OFF);\n    e0 += ENEMY_POOL_OFF;"
    advanced = replace_once(body, entry, "    e0 += ENEMY_POOL_OFF;\n    e = (Enemy*)e0;")
    for label, source in (("repeated_address", body), ("advance_alias", advanced)):
        for control in ("opt_lifetimes", "opt_common_subs", "opt_strength_reduction", "scheduling"):
            yield label + "_" + control + "_off", "#pragma " + control + " off\n" + source + "\n#pragma " + control + " reset\n"
    # Two copies of the same collision-response gate are evidence for a
    # genuine inline boundary. Reconstruct that complete operation, rather
    # than an empty pointer wrapper solely intended to inhibit coalescing.
    start = body.index("        if (*(s32*)(e0 + offsetof(Enemy, coll_pnum)) >= 0) {")
    end = body.index("        if (skip != 0)", start)
    gate = body[start:end]
    if body.count(gate) != 2:
        raise ValueError("expected the two identical collision-response gates")
    for pointer_argument in (False, True):
        helper = "static inline s32 enemy_respond_to_player(s32 index" + (", u8* e0" if pointer_argument else "") + ")\n{\n"
        if not pointer_argument:
            helper += "    u8* e0 = (u8*)&gEnemies[index];\n"
        helper += gate.replace("skip = -1;", "return -1;").replace("skip = 0;", "return 0;") + "}\n\n"
        for entry_label, source in (("repeated", body), ("advanced", advanced)):
            call = "        skip = enemy_respond_to_player(index" + (", (u8*)e" if pointer_argument else "") + ");\n"
            caller = source.replace(gate, call)
            yield "collision_gate_" + ("pointer" if pointer_argument else "index") + "_" + entry_label, helper + caller
            output_helper = helper.replace("static inline s32 enemy_respond_to_player(s32 index", "static inline void enemy_respond_to_player(s32 index, s32* skip")
            output_helper = output_helper.replace("return -1;", "*skip = -1;").replace("return 0;", "*skip = 0;")
            output_call = "        enemy_respond_to_player(index, &skip" + (", (u8*)e" if pointer_argument else "") + ");\n"
            yield "collision_gate_output_" + ("pointer" if pointer_argument else "index") + "_" + entry_label, output_helper + source.replace(gate, output_call)


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


def generate_entry_variants(body):
    # Restored-count alignment: earlier control probes preceded the signed
    # animation test and store-order correction, so remeasure their effects.
    for control in ("opt_propagation", "opt_common_subs", "scheduling"):
        yield control + "_off", "#pragma " + control + " off\n" + body + "\n#pragma " + control + " reset\n"
    for symbol in ("lbl_802512B0", "lbl_802511FC", "lbl_80251148"):
        helper = "static inline s32 enemy_read_" + symbol + "(s32 type)\n{\n    return " + symbol + "[type];\n}\n\n"
        yield "inline_" + symbol, helper + replace_once(body, symbol + "[type]", "enemy_read_" + symbol + "(type)")
    # A block-scoped const index has a real use at an already indexed table,
    # unlike another spelling of the entry's base-pointer initializer.
    for symbol in ("lbl_802512B0", "lbl_802511FC"):
        value = "allowed" if symbol.endswith("2B0") else "minimum"
        guard = "        if (" + symbol + "[type]"
        scoped = replace_once(body, guard, "        s32 " + value + " = " + symbol + "[type];\n        if (" + value)
        # MWCC C permits declarations at block heads only; the second load
        # therefore needs an explicit nested scope covering its real guard.
        if value == "minimum":
            scoped = scoped.replace("        s32 minimum", "        {\n        s32 minimum")
            scoped = replace_once(scoped, "    slot = find_enemy_slot", "        }\n    slot = find_enemy_slot")
        yield "named_" + value + "_load", scoped
    helper = "static inline s32 enemy_random_choice(const s32* choices, s32 index)\n{\n    return choices[index & 3];\n}\n\n"
    random_calls = body
    for offset in (4284, 4300, 4316, 4332):
        random_calls = replace_once(random_calls, "*(s32*)(tbl + ((i & 3) << 2) + " + str(offset) + ")", "enemy_random_choice((s32*)(tbl + " + str(offset) + "), i)")
    yield "inline_random_choices", helper + random_calls
    local_tables = body
    for offset in (4284, 4300, 4316, 4332):
        local_tables = replace_once(local_tables, "*(s32*)(tbl + ((i & 3) << 2) + " + str(offset) + ")", "((s32*)tbl)[(i & 3) + " + str(offset // 4) + "]")
    yield "typed_random_index", local_tables
    # Give the only typed enemy pointer the actual array owner, independent
    # of the table's old byte-pointer cast/rank axis.
    array = replace_once(body, "    Enemy* e;", "    Enemy* enemies = gEnemies;\n    Enemy* e;")
    yield "enemy_array_owner", replace_once(array, "e = &gEnemies[slot];", "e = &enemies[slot];")
    yield "enemy_pointer_first_same_value", replace_once(body, "    Enemy* e;\n", "").replace("    u8* tbl = lbl_8011AF48;", "    Enemy* e;\n    u8* tbl = lbl_8011AF48;")


def generate_helper_variants(body):
    """Meaningful inline boundaries, keeping random calls and stores ordered."""
    begin = body.index("    if (type == -2) {")
    end = body.index("    if (type != 30 && type != 31)", begin)
    resolution = body[begin:end]
    negative = "    } else if (type < 0) {\n        return -6;\n    }\n"
    resolution = replace_once(resolution, negative, "    }\n")
    for passed_table in (False, True):
        helper = "static inline s32 enemy_resolve_random_type(s32 type, s32* level, s32* spew"
        helper += ", u8* tbl" if passed_table else ""
        helper += ")\n{\n    s32 i;\n"
        if not passed_table:
            helper += "    u8* tbl = lbl_8011AF48;\n"
        helper += re.sub(r"\b(level|spew)\b", r"(*\1)", resolution)
        helper += "    return type;\n}\n\n"
        call = "    type = enemy_resolve_random_type(type, &level, &spew" + (", tbl" if passed_table else "") + ");\n"
        call += "    if (type < 0) return -6;\n"
        caller = body[:begin] + call + body[end:]
        caller = replace_once(caller, "    s32 i;\n", "")
        if not passed_table:
            caller = replace_once(caller, "    u8* tbl = lbl_8011AF48;\n", "")
        yield "random_type_" + ("table_argument" if passed_table else "table_owner"), helper + caller
    # The source switch computes a real pair of spawn-direction results;
    # pointer outputs express those results without an artificial wrapper.
    begin = body.index("        switch (otype) {")
    end = body.index("        if (spew == 12)", begin)
    switch = body[begin:end]
    helper = "static inline s32 enemy_spawn_directions(s32 otype, s32* mask)\n{\n    s32 ndirs;\n"
    helper += re.sub(r"\bmask\b", "(*mask)", switch)
    helper += "    return ndirs;\n}\n\n"
    yield "direction_switch_return", helper + body[:begin] + "        ndirs = enemy_spawn_directions(otype, &mask);\n" + body[end:]


def generate_pool_variants(body):
    """Full BSS owner plus pre-offset store, on the pre-closure source.

    After retention, reproduce with ea159c52f source and its freshly built
    raw object via --source/--baseline-object. No live source is rewritten.
    The view names existing symbols, not newly inferred game fields.
    """
    view = """typedef struct EnemySpawnPoolView {
    u8 _000[0x348];
    s32 lbl_80251148[45];
    s32 lbl_802511FC[45];
    s32 lbl_802512B0[45];
    u32 gWadAtreeHeaders[0x8B4 / 4];
    Enemy gEnemies[25];
} EnemySpawnPoolView;

"""
    if "pool->" in body:
        raise ValueError("pool probe requires the historical pre-closure body")
    based = body
    for symbol in ("lbl_80251148", "lbl_802511FC", "lbl_802512B0", "gEnemies"):
        if symbol + "[" not in based:
            raise ValueError("missing spawn pool symbol: " + symbol)
        based = based.replace(symbol + "[", "pool->" + symbol + "[")
    declaration = "    EnemySpawnPoolView* pool = (EnemySpawnPoolView*)lbl_80250E00;"
    table = "    u8* tbl = lbl_8011AF48;"
    for where in ("before_table", "after_table", "after_enemy", "statement"):
        if where == "before_table":
            candidate = replace_once(based, table, declaration + "\n" + table)
        elif where == "after_table":
            candidate = replace_once(based, table, table + "\n" + declaration)
        elif where == "after_enemy":
            candidate = replace_once(based, "    Enemy* e;", "    Enemy* e;\n" + declaration)
        else:
            candidate = replace_once(based, "    Enemy* e;", "    Enemy* e;\n    EnemySpawnPoolView* pool;")
            candidate = replace_once(candidate, "    if (gGameMode", "    pool = (EnemySpawnPoolView*)lbl_80250E00;\n    if (gGameMode")
        yield "whole_pool_" + where, view + candidate
        if where != "after_table":
            continue
        store = "    e = &pool->gEnemies[slot];\n    e->generator = gen;"
        yield "whole_pool_store_first", view + replace_once(candidate, store,
            "    pool->gEnemies[slot].generator = gen;\n    e = &pool->gEnemies[slot];")
        row = replace_once(candidate, "    Enemy* e;", "    EnemySpawnPoolView* row;\n    Enemy* e;")
        row = replace_once(row, store, "    row = (EnemySpawnPoolView*)((u8*)pool + slot * sizeof(Enemy));\n    row->gEnemies[0].generator = gen;\n    e = row->gEnemies;")
        yield "whole_pool_row_then_store", view + row
        for location, anchor in (("before_slot", "    s32 slot;"), ("after_slot", "    s32 otype;"), ("before_table", table)):
            ranked = replace_once(row, "    Enemy* e;\n", "")
            yield "whole_pool_row_e_" + location, view + replace_once(ranked, anchor, "    Enemy* e;\n" + anchor)


def movement_helper_variants(body):
    """Give the node/route reaction one cohesive inline scope, not a barrier."""
    guard = "                if (*(u32*)((u8*)e->coll_ip + 100) != 0) {"
    begin = body.rindex(guard)
    fallback = "                } else {\n                    if (e->dead_end <= 0) {\n                        e->dead_end = 20;\n                    }\n                }"
    end = body.index(fallback, begin) + len(fallback)
    reaction = body[begin:end]
    parameters = ("Enemy* e", "s32 index", "s32 alg")
    arguments = ("e", "index", "alg")
    for order in itertools.permutations(range(3)):
        helper = "static inline void enemy_player_collision_reaction(" + ", ".join(parameters[i] for i in order) + ")\n{\n" + reaction + "\n}\n\n"
        call = "                enemy_player_collision_reaction(" + ", ".join(arguments[i] for i in order) + ");"
        yield "reaction_args_" + "".join(map(str, order)), helper + body[:begin] + call + body[end:]


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
    route_guard = "                    if (e->route == 0 || ABS_REVERSED(e->route) > 2) {"
    for named_sign in (False, True):
        helper = "static inline s32 enemy_route_magnitude(s32 route)\n{\n"
        if named_sign:
            helper += "    s32 sign = route >> 31;\n    return (sign ^ route) - sign;\n"
        else:
            helper += "    return ABS_REVERSED(route);\n"
        helper += "}\n\n"
        yield "route_abs_inline_" + ("sign" if named_sign else "expression"), helper + replace_once(live, route_guard, route_guard.replace("ABS_REVERSED(e->route)", "enemy_route_magnitude(e->route)"))
    helper = "static inline s32 enemy_route_needs_refresh(s32 route)\n{\n    return route == 0 || ABS_REVERSED(route) > 2;\n}\n\n"
    yield "route_condition_helper", helper + replace_once(live, route_guard, "                    if (enemy_route_needs_refresh(e->route)) {")
    # These values name the actual arithmetic; they are not side-effecting
    # barriers. Keep the short-circuit decision before computing magnitude.
    named = replace_once(live, route_guard, "                    s32 route = e->route;\n                    s32 magnitude;\n                    if (route == 0 || (magnitude = ABS_REVERSED(route)) > 2) {")
    yield "route_magnitude_condition_local", named
    local_group = "    s32 collide;\n    f32 moveDistance;\n    s32 result;\n    s32 n;\n    Enemy* other;"
    declarations = local_group.splitlines()
    for order in ((1, 0, 2, 3, 4), (4, 0, 1, 2, 3), (0, 1, 4, 2, 3), (3, 2, 1, 0, 4), (4, 3, 2, 1, 0)):
        yield "movement_decl_" + "".join(map(str, order)), replace_once(live, local_group, "\n".join(declarations[i] for i in order))


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


def init_vars_variants(body):
    for control in ("opt_propagation", "opt_common_subs"):
        yield control + "_off", "#pragma " + control + " off\n" + body + "\n#pragma " + control + " reset\n"
    counter_early = replace_once(body, "    enemy->action = 0;", "    i4 = 0;\n    enemy->action = 0;").replace("for (i4 = 0; i4 < 20;", "for (; i4 < 20;")
    yield "counter_defined_before_clear", counter_early
    tier_early = replace_once(body, "    z2 = lbl_80346820;", "    tier = 0;\n    z2 = lbl_80346820;").replace("    tier = 0;\n    if (scale", "    if (scale")
    yield "tier_defined_before_clear", tier_early
    yield "both_zero_definitions_early", replace_once(tier_early, "    enemy->action = 0;", "    i4 = 0;\n    enemy->action = 0;").replace("for (i4 = 0; i4 < 20;", "for (; i4 < 20;")
    helper = """static inline f32 enemy_tier_damage(s32 type, f32 health, f32 low, f32 damage)
{
    if (type == 30) {
        return damage;
    }
    if (health > low) {
        damage = (f32)(lbl_80346A30 * damage);
    } else {
        damage = (f32)(lbl_80346A28 * damage);
    }
    return damage;
}

"""
    start = body.index("    if (!(ht > hi2))")
    end = body.index("    enemy->atts.fight = spd;", start)
    lifted = body[:start] + "    if (!(ht > hi2)) {\n        spd = enemy_tier_damage(ty, ht, lo2, spd);\n    }\n" + body[end:]
    yield "damage_inline_return", helper + lifted
    yield "damage_inline_return_propagation_off", "#pragma opt_propagation off\n" + helper + lifted + "\n#pragma opt_propagation reset\n"
    # Preserve single-field table accesses as genuinely distinct block-local
    # typed element aliases. The previous cap tested raw single expressions,
    # never these aliases jointly with propagation control.
    aliased = body
    pattern = r"    row = tbl \+ \*\(s32\*\)e \* 4;\n(    [^\n]+\(\((?:f32|s32)\*\)row\)\[\d+\][^\n]*;)"
    def alias(match):
        stmt = match[1]
        ctype = "s32" if "((s32*)row)" in stmt else "f32"
        stmt = stmt.replace("((" + ctype + "*)row)", "tableRow")
        return "    {\n        " + ctype + "* tableRow = (" + ctype + "*)(tbl + *(s32*)e * 4);\n    " + stmt + "\n    }"
    aliased, count = re.subn(pattern, alias, aliased)
    if count != 8:
        raise ValueError("expected eight isolated table statements, got " + str(count))
    yield "typed_table_row_blocks", aliased
    yield "typed_table_row_blocks_propagation_off", "#pragma opt_propagation off\n" + aliased + "\n#pragma opt_propagation reset\n"
    # Initialization identity is not initializer placement: actually consume
    # the initialized counter/tier in an existing zero-valued state store.
    zeros = replace_once(body, "    enemy->action = 0;", "    i4 = 0;\n    enemy->action = i4;").replace("for (i4 = 0; i4 < 20;", "for (; i4 < 20;")
    zeros = replace_once(zeros, "    enemy->push_cnt = 0;", "    tier = 0;\n    enemy->push_cnt = tier;").replace("    tier = 0;\n    if (scale", "    if (scale")
    yield "zero_values_shared_with_stores", zeros
    yield "zero_values_shared_propagation_off", "#pragma opt_propagation off\n" + zeros + "\n#pragma opt_propagation reset\n"
    zstart, zend = zeros.index("    if (!(ht > hi2))"), zeros.index("    enemy->atts.fight = spd;")
    joint = zeros[:zstart] + "    if (!(ht > hi2)) {\n        spd = enemy_tier_damage(ty, ht, lo2, spd);\n    }\n" + zeros[zend:]
    yield "shared_zeros_return_helper_propagation_off", "#pragma opt_propagation off\n" + helper + joint + "\n#pragma opt_propagation reset\n"
    for ctype in ("u32", "s16"):
        yield "counter_" + ctype, body.replace("    s32 i4;", "    " + ctype + " i4;")
        yield "tier_" + ctype, body.replace("    s32 tier;", "    " + ctype + " tier;")
    # Keep the actual clear and tier-selection loops in distinct inlined
    # scopes; source only, and each standalone/helper-data cost is checked.
    clear_begin = body.index("    for (i4 = 0;")
    clear_end = body.index("    z2 = ", clear_begin)
    clear_helper = "static inline void enemy_clear_hit_times(u8* e, f32 z)\n{\n    s32 i4;\n" + body[clear_begin:clear_end] + "}\n\n"
    cleared = body[:clear_begin] + "    enemy_clear_hit_times(e, z);\n" + body[clear_end:]
    yield "clear_loop_helper", clear_helper + cleared
    clear_lifted = lifted[:clear_begin] + "    enemy_clear_hit_times(e, z);\n" + lifted[clear_end:]
    yield "clear_and_damage_helpers", clear_helper + helper + clear_lifted
    joint_helpers = clear_helper + helper + clear_lifted
    yield "clear_and_damage_helpers_propagation_off", "#pragma opt_propagation off\n" + joint_helpers + "\n#pragma opt_propagation reset\n"
    for label, base in (("plain", body), ("joint", joint_helpers)):
        tier_start = base.index("    tier = 0;\n    if (scale")
        tier_end = base.index("    enemy->org_lvl", tier_start)
        selection = base[tier_start:tier_end]
        tier_helper = "static inline s32 enemy_health_tier(f32 scale, f32 hi, f32 lo, f32 z2)\n{\n    s32 tier;\n" + selection + "    return tier;\n}\n\n"
        with_tier = tier_helper + base[:tier_start] + "    tier = enemy_health_tier(scale, hi, lo, z2);\n" + base[tier_end:]
        yield label + "_tier_helper", with_tier
        yield label + "_tier_helper_propagation_off", "#pragma opt_propagation off\n" + with_tier + "\n#pragma opt_propagation reset\n"
        if label == "joint":
            # Prefer the actual named five-element member to a helper whose
            # sole responsibility is a clear loop. Its induction width is
            # intentionally one array element, not a byte offset.
            named = replace_once(with_tier, clear_helper, "")
            named = replace_once(named, "    enemy_clear_hit_times(e, z);", "    for (i4 = 0; i4 < 5; i4++) {\n        enemy->fxhittime[i4] = z;\n    }")
            controlled = "#pragma opt_propagation off\n" + named + "\n#pragma opt_propagation reset\n"
            yield "named_clear_tier_damage_propagation_off", controlled
            for size in (4, 8):
                # Existing contract permits dead frame reservations for
                # unrecovered locals. Do not imply their original identity
                # is known: the target's save-area displacement proves size
                # but supplies no local name or type.
                yield "named_helpers_frame_" + str(size), replace_once(controlled, "    u8* e;", "    u8 unrecovered_locals[" + str(size) + "];\n    u8* e;")
            framed = replace_once(controlled, "    u8* e;", "    u8 unrecovered_locals[8];\n    u8* e;")
            for name, formals, actuals in (
                ("enemy_health_tier", ["f32 scale", "f32 hi", "f32 lo", "f32 z2"], ["scale", "hi", "lo", "z2"]),
                ("enemy_tier_damage", ["s32 type", "f32 health", "f32 low", "f32 damage"], ["ty", "ht", "lo2", "spd"]),
            ):
                for order in itertools.permutations(range(4)):
                    if order == (0, 1, 2, 3):
                        continue
                    reordered = replace_once(framed, name + "(" + ", ".join(formals) + ")", name + "(" + ", ".join(formals[i] for i in order) + ")")
                    reordered = replace_once(reordered, name + "(" + ", ".join(actuals) + ")", name + "(" + ", ".join(actuals[i] for i in order) + ")")
                    yield name + "_args_" + "".join(map(str, order)), reordered
            for control in ("scheduling", "opt_common_subs"):
                yield "framed_" + control + "_off", "#pragma " + control + " off\n" + framed + "\n#pragma " + control + " reset\n"
            for local in ("t", "hi", "lo", "t2", "hi2", "lo2"):
                yield "named_helpers_double_" + local, replace_once(controlled, "    f32 " + local + ";", "    f64 " + local + ";")
        # Block-scoped ownership, as distinct from moving the initializer.
        scoped = base.replace("    s32 tier;\n", "", 1)
        tier_start = scoped.index("    tier = 0;\n    if (scale")
        tier_end = scoped.index("    enemy->mode2", tier_start)
        scoped = scoped[:tier_start] + "    {\n        s32 tier;\n" + scoped[tier_start:tier_end] + "    }\n" + scoped[tier_end:]
        yield label + "_tier_scoped", scoped
        yield label + "_tier_scoped_propagation_off", "#pragma opt_propagation off\n" + scoped + "\n#pragma opt_propagation reset\n"
    named_clear = "    for (i4 = 0; i4 < 5; i4++) {\n        enemy->fxhittime[i4] = z;\n    }\n"
    yield "named_clear_loop", body[:clear_begin] + named_clear + body[clear_end:]
    scoped_clear = "    {\n        s32 i4;\n" + body[clear_begin:clear_end] + "    }\n"
    yield "scoped_clear_loop", (body[:clear_begin] + scoped_clear + body[clear_end:]).replace("    s32 i4;\n", "", 1)


def movement_reload_variants(body):
    """Separate the node test from the route query's named owner identity."""
    guard = "                if (*(u32*)((u8*)e->coll_ip + 100) != 0) {"
    if body.count(guard) != 2:
        raise ValueError("expected two collision-node guards")
    start = body.rindex(guard)
    before, region = body[:start], body[start:]
    if "const Enemy* contactOwner" in before or "const Enemy* contactOwner" in region:
        raise ValueError("use historical source before the retained owner split")
    declaration = "                const Enemy* contactOwner = e;\n"
    position = "(f32*)((u8*)e->coll_ip + 52)"
    route = before + declaration + replace_once(region, position, "(f32*)((u8*)contactOwner->coll_ip + 52)")
    yield "collision_owner_route", route
    yield "collision_owner_node", before + declaration + region.replace("e->coll_ip", "contactOwner->coll_ip", 1)
    radius = "                half[0] = oldpos[0] + e->trans[0];\n                rad2 = (f32)(rad * 1.5);"
    early = replace_once(route, radius, "                rad2 = (f32)(rad * 1.5);\n                half[0] = oldpos[0] + e->trans[0];")
    yield "owner_route_radius_before", early
    yield "owner_route_radius_copy_scale", replace_once(early, "                rad2 = (f32)(rad * 1.5);", "                rad2 = rad;\n                rad2 *= 1.5;")
    entry = "    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + ENEMY_POOL_OFF);"
    yield "owner_route_direct_entry", replace_once(route, entry, "    Enemy* e = &gEnemies[index];")
    staged = replace_once(route, entry, "    Enemy* e;")
    for old, declaration in (
        ("s32 alg = e->algorithm;", "s32 alg;"),
        ("f32 rad = e->rad;", "f32 rad;"),
        ("f32 hht = e->hht;", "f32 hht;"),
        ("s32 blocked = 0;", "s32 blocked;"),
    ):
        staged = replace_once(staged, old, declaration)
    setup = """    e = (Enemy*)((u8*)lbl_80250E00 + index * sizeof(Enemy));
    e = (Enemy*)((u8*)e + ENEMY_POOL_OFF);
    alg = e->algorithm;
    rad = e->rad;
    hht = e->hht;
    blocked = 0;

"""
    marker = "    /* stun freeze + knockback integration */"
    yield "owner_route_staged_entry", replace_once(staged, marker, setup + marker)


def move_page_variants(body):
    """Recover the existing pool member before testing alias composition.

    The page already owns speed[] and enemies[] in the source. This adds no
    storage or placeholder wrapper; it tests array-member lowering against
    the historical raw-byte offset expression. Use --source after retention.
    """
    old = "    type = *(s32*)(e0 + OFF_E(type));"
    page = replace_once(body, old, "    type = ((EnemyMovePage05*)e0)->enemies[0].type;")
    entry = "    e = (Enemy*)(e0 + ENEMY_POOL_OFF);\n    e0 += ENEMY_POOL_OFF;"
    for label, setup in (
        ("array_then_copy", "    e0 = (u8*)((EnemyMovePage05*)e0)->enemies;\n    e = (Enemy*)e0;"),
        ("array_twice", "    e = ((EnemyMovePage05*)e0)->enemies;\n    e0 = (u8*)((EnemyMovePage05*)e0)->enemies;"),
        ("array_and_byte_advance", "    e = ((EnemyMovePage05*)e0)->enemies;\n    e0 += ENEMY_POOL_OFF;"),
    ):
        yield label, replace_once(page, entry, setup)


def movement_address_variants(body):
    """Separate entry initializers from the two address-construction steps.

    Historical pre-staging source is required after retention. Reusing a
    different owner is a negative control, not an approved source endpoint.
    """
    entry = "    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + ENEMY_POOL_OFF);"
    staged = replace_once(body, entry, "    Enemy* e;")
    assignments = []
    for kind, name, value in (("s32", "alg", "e->algorithm"),
                              ("f32", "rad", "e->rad"),
                              ("f32", "hht", "e->hht"),
                              ("s32", "blocked", "0")):
        staged = replace_once(staged, "    " + kind + " " + name + " = " + value + ";",
                              "    " + kind + " " + name + ";")
        assignments.append("    " + name + " = " + value + ";")
    marker = "    /* stun freeze + knockback integration */"
    for label, owner in (("staged_self", "e"), ("staged_other", "other")):
        if owner == "other" and "    Enemy* other;" not in staged:
            raise ValueError("missing existing other-enemy owner")
        setup = ("    " + owner + " = (Enemy*)((u8*)lbl_80250E00 + index * sizeof(Enemy));\n"
                 "    e = (Enemy*)((u8*)" + owner + " + ENEMY_POOL_OFF);\n")
        yield label, replace_once(staged, marker, setup + "\n".join(assignments) + "\n\n" + marker)


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


def exception_records(data):
    """Resolve extabindex entries to function names and actual extab bytes.

    These sections have NO leading dot. A dot-only section-name regex is
    not evidence that objects lack exception metadata. Extra emitted helper
    records remain visible; this function makes no dead-strip/link claim.
    """
    if data[:6] != b"\x7fELF\x01\x02":
        raise ValueError("expected big-endian ELF32")
    sections = wf._sections(data)
    index = next(s for s in sections if s.name == "extabindex")
    extab = next(s for s in sections if s.name == "extab")
    if index.size % 12:
        raise ValueError("partial extabindex record")
    reloc = {}
    for rs in sections:
        if rs.section_type != wf.SHT_RELA or rs.info != index.index:
            continue
        table = sections[rs.link]
        strings = sections[table.link]
        for at in range(rs.offset, rs.offset + rs.size, rs.entry_size or 12):
            offset, info, addend = struct.unpack_from(">IIi", data, at)
            if info & 255 != 1 or offset in reloc:
                raise ValueError("expected unique ADDR32 index relocation")
            sp = table.offset + (info >> 8) * (table.entry_size or 16)
            if not table.offset <= sp < table.offset + table.size:
                raise ValueError("index relocation symbol out of range")
            name_at, value = struct.unpack_from(">II", data, sp)
            section = wf._u16(data, sp + 14)
            name = wf._cstring(data, strings.offset + name_at) if name_at else ""
            reloc[offset] = (name, value + addend, section, addend)
    expected_offsets = {o + k for o in range(0, index.size, 12) for k in (0, 8)}
    if set(reloc) != expected_offsets:
        raise ValueError("incomplete or unexpected index relocations")
    starts = sorted({reloc[o + 8][1] for o in range(0, index.size, 12)})
    ends = dict(zip(starts, starts[1:] + [extab.size]))
    result = {}
    for offset in range(0, index.size, 12):
        name, _, fn_section, addend = reloc[offset]
        _, start, section, _ = reloc[offset + 8]
        if not name or name in result or addend or sections[fn_section].name != ".text":
            raise ValueError("expected unique named function entry")
        if section != extab.index or not 0 <= start < ends[start] <= extab.size:
            raise ValueError("exception metadata outside extab")
        result[name] = {
            "length": wf._u32(data, index.offset + offset + 4),
            "metadata": data[extab.offset + start:extab.offset + ends[start]].hex(),
        }
    return result


def compare_exception_records(target, ours):
    return {
        "target_records": len(target), "ours_records": len(ours),
        "missing": sorted(set(target) - set(ours)),
        "extra": {k: ours[k] for k in sorted(set(ours) - set(target))},
        "changed": {k: {"target": target[k], "ours": ours[k]}
                    for k in target if k in ours and target[k] != ours[k]},
        "scope": "Function-indexed metadata only. Extra records require a separate link-reachability check; not a whole-TU flip verdict.",
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("axis", choices=["metadata", "pointer", "normalization", "move_entry", "move_page", "generate", "generate_entry", "generate_helpers", "generate_pool", "movement", "movement_helpers", "movement_reload", "movement_address", "resources", "milestones", "formatter", "gettype", "gettype_controls", "target_player", "target_player_colors", "init_vars"])
    ap.add_argument("--show", help="Print normalized target/candidate diff for one existing output")
    ap.add_argument("--compare-baseline", action="store_true", help="With --show, compare the stored raw baseline rather than the retail target")
    ap.add_argument("--source-patch", help="Print an apply_patch-formatted source diff; never writes src/")
    ap.add_argument("--source", type=Path, help="Historical full-TU source, paired with --baseline-object; preserve the original basename (enemy.c) for whole-object filename metadata fidelity")
    ap.add_argument("--baseline-object", type=Path, help="Fresh real-edge object built from --source, for whole-object fidelity")
    args = ap.parse_args()
    if args.axis == "metadata":
        if any((args.show, args.compare_baseline, args.source_patch, args.source, args.baseline_object)):
            ap.error("metadata reads the current objects and takes no variant options")
        target = exception_records(Path(target_object(UNIT)).read_bytes())
        ours = exception_records((REPO / "build/GUNE5D/src/game/enemy/enemy.o").read_bytes())
        result = compare_exception_records(target, ours)
        print(json.dumps(result, indent=2))
        if result["missing"] or result["changed"]:
            raise SystemExit(1)
        return
    functions = {"pointer": "fn_80046680", "normalization": "move_logic10", "move_entry": "move_logic10", "move_page": "move_logic10", "generate": "generate_enemy", "generate_entry": "generate_enemy", "generate_helpers": "generate_enemy", "generate_pool": "generate_enemy", "movement": "do_enemy_move", "movement_helpers": "do_enemy_move", "movement_reload": "do_enemy_move", "resources": "fn_80051164", "milestones": "fn_80051C78", "formatter": "fn_80051E1C", "gettype": "GetEnemyType", "gettype_controls": "GetEnemyType", "target_player": "fn_800516F8", "target_player_colors": "fn_800516F8", "init_vars": "init_enemy_vars"}
    functions["movement_address"] = "do_enemy_move"
    function = functions[args.axis]
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
    generators = {"pointer": pointer_variants, "normalization": normalization_variants, "move_entry": move_entry_variants, "move_page": move_page_variants, "generate": generate_variants, "generate_entry": generate_entry_variants, "generate_helpers": generate_helper_variants, "generate_pool": generate_pool_variants, "movement": movement_variants, "movement_helpers": movement_helper_variants, "movement_reload": movement_reload_variants, "resources": resource_variants, "milestones": milestone_variants, "formatter": formatter_variants, "gettype": enemy_type_variants, "gettype_controls": enemy_type_control_variants, "target_player": target_player_variants, "target_player_colors": target_player_color_variants, "init_vars": init_vars_variants}
    generators["movement_address"] = movement_address_variants
    variants = generators[args.axis]
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
