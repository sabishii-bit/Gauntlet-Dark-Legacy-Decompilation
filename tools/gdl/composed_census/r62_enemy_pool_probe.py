"""R62 bounded enemy source experiments; generated files stay under build/.

Requires whole raw-object fidelity; never edits production source or pins.
Historical axes require their original source/object pair via --baseline-source
and --baseline-object. Current axes operate on the current production source.
The user approved the weak sqrt compatibility helper; original provenance is
still unproven. --verify replays existing pins in memory with all real guards.
"""
import argparse
import difflib
import json
import re
from pathlib import Path
import contextlib
import io
import itertools
import shlex
import subprocess
import struct
from t16_rederive_body import derive_slots, candidate_rule, _symbol_body

from r61_enemy_probe import (
    REPO, UNIT, POOL_START, POOL_END, pool_variants, read_datum, wf,
    read_edges, compile_baseline, compile_with, function_span, replace_once,
    target_object, parse, disassemble_words, compare_words,
)

OUT = REPO / "build/r62_enemy_pool"


def edit_function(source, name, operation):
    lines = source.splitlines(keepends=True)
    span = function_span(source, name)
    if span is None:
        raise ValueError("function not found: " + name)
    first, last = span
    return "".join(lines[:first]) + operation("".join(lines[first:last])) + "".join(lines[last:])


def seed(source):
    return next(s for n, s in pool_variants(source) if n == "literal_sqrt_damage_lower_first")


def fixes(source, stage=5):
    if stage == 0:
        return source
    # Measured coupled repair: real pool literals supply nominal space,
    # making the former 8-byte pad redundant; reuse the existing scale.
    def reaction(body):
        body = replace_once(body, "    u8 unused[8];\n", "")
        body = replace_once(body, "                enemy->pushed[0] += enemy->damagedir[0] * 8.0f;", "                scale = 8.0f;\n                enemy->pushed[0] += enemy->damagedir[0] * scale;")
        return body.replace("enemy->damagedir[1] * 8.0f", "enemy->damagedir[1] * scale").replace("enemy->damagedir[2] * 8.0f", "enemy->damagedir[2] * scale")
    source = edit_function(source, "fn_8004DC2C", reaction)
    source = edit_function(source, "move_logic14", lambda b: re.sub(r"e->ang = e->ang \+ ([0-9.]+);", r"e->ang += \1;", b))
    source = edit_function(source, "fn_8004DF58", lambda b: re.sub(r"amount = \(f32\)\(amount \* (0\.5|1\.5)\);", r"amount *= \1;", b))
    if stage == 1:
        return source
    for name, old, new in (
            ("closest_enemy", "unused_after[12]", "unused_after[4]"),
            ("fn_80045C30", "unused[20]", "unused[8]"),
            ("fn_8004646C", "stack_top[16]", "stack_top[8]"),
            ("fn_80051568", "    u8 _spare[24];\n", "")):
        source = edit_function(source, name, lambda b: replace_once(b, old, new))
    source = edit_function(source, "fn_800510A4", lambda b: b.replace("    f32 fv = 0.0f;\n", "").replace("e->flooroffset = fv;", "e->flooroffset = 0.0f;"))
    source = edit_function(source, "move_logic06", lambda b: re.sub(r"e->ang = e->ang \+ ([0-9.]+);", r"e->ang += \1;", b))
    def damage(b):
        b = replace_once(b, "(f64)e->health <= (f64)0.0f", "e->health <= 0.0f")
        return re.sub(r"amount = \(f32\)\(amount \* ([0-9.]+)\);", r"amount *= \1;", b)
    source = edit_function(source, "damage_enemy", damage)
    def collide(b):
        b = replace_once(b, "    oldpos[1] = (f32)((2.0 - enemy->flooroffset) + oldpos[1]);", "    oldpos[1] += 2.0 - enemy->flooroffset;")
        return replace_once(b, "            slideRad = (f32)(rad * 1.5);", "            slideRad = rad;\n            slideRad *= 1.5;")
    source = edit_function(source, "do_enemy_collide", collide)
    if stage == 2:
        return source
    def find_slot(b):
        for name in ("dying_mult", "invis_add", "reset_distance"):
            m = re.search(r"\b" + name + r" = ([0-9.]+f?);", b)
            literal = m[1]
            b = re.sub(r"^\s*(?:f32|f64) " + name + r"(?: = [^;]+)?;\n", "\n", b, flags=re.M)
            b = re.sub(r"^\s*" + name + r" = [^;]+;\n", "\n", b, flags=re.M)
            b = re.sub(r"\b" + name + r"\b", literal, b)
        return b
    source = edit_function(source, "find_enemy_slot", find_slot)
    if stage == 3:
        return source
    # Removing two folded pi locals restores every movement stack slot.
    def move(b):
        b = re.sub(r"^[ \t]*f64 hi = 3\.141592654;\n", "", b, flags=re.M)
        return re.sub(r"\bhi\b", "3.141592654", b)
    source = edit_function(source, "do_enemy_move", move)
    def item_sqrt(b):
        b = replace_once(b, "    u8 unused[8];", "    u8 _spare[24];\n    u8 unused[8];")
        return "#pragma opt_propagation off\n" + b + "#pragma opt_propagation reset\n"
    source = edit_function(source, "fn_80051568", item_sqrt)
    if stage == 4:
        return source
    source = dict(variants(source, "owners9"))["collide_no_high"]
    source = dict(variants(source, "distance10"))["no_big_top4"]
    return source


def variants(source, axis):
    if axis == "baseline":
        yield "candidate", source
    elif axis == "multiply":
        for form in ("compound", "separate", "local"):
            def change(body):
                for constant in ("0.5", "1.5"):
                    old = f"amount = (f32)(amount * {constant});"
                    if form == "compound":
                        new = f"amount *= {constant};"
                    elif form == "separate":
                        new = f"amount = amount * {constant};"
                    else:
                        new = f"{{ f64 factor = {constant}; amount *= factor; }}"
                    body = replace_once(body, old, new)
                return body
            yield form, edit_function(source, "fn_8004DF58", change)
    elif axis == "angle":
        for form in ("compound", "local_angle", "local_factor"):
            def change(body):
                m = re.search(r"e->ang = e->ang \+ ([0-9.]+);", body)
                if not m:
                    raise ValueError("missing angle update")
                c = m[1]
                new = {"compound": f"e->ang += {c};",
                       "local_angle": f"{{ f32 ang = e->ang; ang += {c}; e->ang = ang; }}",
                       "local_factor": f"{{ f64 delta = {c}; e->ang = e->ang + delta; }}"}[form]
                return replace_once(body, m[0], new)
            yield form, edit_function(source, "move_logic14", change)
    elif axis == "reaction":
        for form in ("pad", "scale", "both"):
            def change(body):
                if form != "scale":
                    body = replace_once(body, "    u8 unused[8];\n", "")
                if form != "pad":
                    body = replace_once(body, "                enemy->pushed[0] += enemy->damagedir[0] * 8.0f;", "                scale = 8.0f;\n                enemy->pushed[0] += enemy->damagedir[0] * scale;")
                    body = body.replace("enemy->damagedir[1] * 8.0f", "enemy->damagedir[1] * scale").replace("enemy->damagedir[2] * 8.0f", "enemy->damagedir[2] * scale")
                return body
            yield form, edit_function(source, "fn_8004DC2C", change)
    elif axis == "pads":
        for name, old, new in (
                ("closest_enemy", "unused_after[12]", "unused_after[4]"),
                ("fn_80045C30", "unused[20]", "unused[8]"),
                ("fn_8004646C", "stack_top[16]", "stack_top[8]"),
                ("fn_80051568", "    u8 _spare[24];\n", ""),
                ("do_enemy_move", "    u8 unused4[12];\n", ""),
                ("fn_800516F8", "    u8 padLo[4];\n", "")):
            yield name, edit_function(source, name, lambda b: replace_once(b, old, new))
    elif axis == "simple":
        for name in ("fn_800510A4", "find_enemy_slot", "move_logic06", "damage_enemy", "do_enemy_collide"):
            def change(b):
                if name == "fn_800510A4":
                    return b.replace("    f32 fv = 0.0f;\n", "").replace("e->flooroffset = fv;", "e->flooroffset = 0.0f;")
                if name == "find_enemy_slot":
                    b = b.replace("    f64 invis_add;\n", "").replace("    f64 dying_mult;\n", "")
                    b = b.replace("    dying_mult = 0.01;\n", "").replace("    invis_add = 10000.0;\n", "")
                    return b.replace("distance *= dying_mult;", "distance *= 0.01;").replace("distance += invis_add;", "distance += 10000.0;")
                if name == "move_logic06":
                    return re.sub(r"e->ang = e->ang \+ ([0-9.]+);", r"e->ang += \1;", b)
                if name == "damage_enemy":
                    b = replace_once(b, "(f64)e->health <= (f64)0.0f", "e->health <= 0.0f")
                    return re.sub(r"amount = \(f32\)\(amount \* ([0-9.]+)\);", r"amount *= \1;", b)
                if name == "do_enemy_collide":
                    b = replace_once(b, "    oldpos[1] = (f32)((2.0 - enemy->flooroffset) + oldpos[1]);", "    oldpos[1] += 2.0 - enemy->flooroffset;")
                    return replace_once(b, "            slideRad = (f32)(rad * 1.5);", "            slideRad = rad;\n            slideRad *= 1.5;")
            yield name, edit_function(source, name, change)
    elif axis == "stack2":
        for name, changes in (
                ("collide_nopads", [("do_enemy_collide", "    u8 framePad[4];\n", ""), ("do_enemy_collide", "    u8 unused[4];\n", ""), ("do_enemy_collide", "    (void)framePad;\n", ""), ("do_enemy_collide", "    (void)unused;\n", "")]),
                ("collide_removeunused_reuse_dh", [("do_enemy_collide", "    u8 unused[4];\n", ""), ("do_enemy_collide", "    (void)unused;\n", ""), ("do_enemy_collide", "    f32 dh;\n", "")]),
                ("move_remove_bottom", [("do_enemy_move", "    u8 unused4[12];\n", "")]),
                ("distance_remove_top", [("fn_800516F8", "    u8 unused[44];\n", "")]),
                ("distance_remove_both", [("fn_800516F8", "    u8 unused[44];\n", ""), ("fn_800516F8", "    u8 padLo[4];\n", "")])):
            candidate = source
            for fn, old, new in changes:
                candidate = edit_function(candidate, fn, lambda b: replace_once(b, old, new))
            if name == "collide_removeunused_reuse_dh":
                candidate = edit_function(candidate, "do_enemy_collide", lambda b: re.sub(r"\bdh\b", "dt", b))
            yield name, candidate
    elif axis == "constant_locals":
        for fn, names in (("fn_80051568", ["kHalf", "kZero", "kThree"]),
                          ("fn_800516F8", ["kK", "kThree", "kHalf", "kZero", "kPi"]),
                          ("find_enemy_slot", ["dying_mult", "invis_add", "count", "reset_distance"])):
            def change(b):
                for name in names:
                    m = re.search(r"\b" + name + r" = ([0-9.]+f?);", b)
                    if not m:
                        continue
                    literal = m[1]
                    b = re.sub(r"^\s*(?:f32|f64|s32) " + name + r"(?: = [^;]+)?;\n", "\n", b, flags=re.M)
                    b = re.sub(r"^\s*" + name + r" = [^;]+;\n", "\n", b, flags=re.M)
                    b = re.sub(r"\b" + name + r"\b", literal, b)
                return b
            yield fn, edit_function(source, fn, change)
    elif axis == "stack3":
        for mode in ("inner_pads", "inner_pads_outer", "distance_locals_bottom", "move_normal_constants", "move_normal_no_hi"):
            if mode.startswith("inner"):
                def change(b):
                    b = b.replace("            u8 npPad[4];\n", "").replace("            (void)npPad;\n", "")
                    if mode.endswith("outer"):
                        b = replace_once(b, "    u8 unused[4];\n", "")
                        b = replace_once(b, "    (void)unused;\n", "")
                    return b
                yield mode, edit_function(source, "do_enemy_collide", change)
            elif mode == "distance_locals_bottom":
                def change(b):
                    b = replace_once(b, "    u8 unused[44];\n", "")
                    b = replace_once(b, "    u8 padLo[4];\n", "")
                    decl = "    f32 ad;\n    volatile f32 distanceScratch0, distanceScratch1, distanceScratch2, distanceScratch3;\n"
                    b = replace_once(b, decl, "")
                    return replace_once(b, "    f32 bestSpecial;\n", "    f32 bestSpecial;\n" + decl)
                yield mode, edit_function(source, "fn_800516F8", change)
            else:
                def change(b):
                    b = replace_once(b, "    u8 unused4[12];\n", "")
                    if mode == "move_normal_no_hi":
                        b = re.sub(r"^[ \t]*f64 hi = 3\.141592654;\n", "", b, flags=re.M)
                        return re.sub(r"\bhi\b", "3.141592654", b)
                    # Keep the two real angle-normalization temporaries;
                    # declare their shared constants in the outer scope.
                    b = replace_once(b, "    Enemy* e;", "    f64 fullTurn = 6.283185308;\n    f64 negativePi = -3.141592654;\n    Enemy* e;")
                    return b.replace("a -= 6.283185308;", "a -= fullTurn;").replace("a <= (-3.141592654)", "a <= negativePi").replace("a = 6.283185308 + a;", "a = fullTurn + a;")
                yield mode, edit_function(source, "do_enemy_move", change)
    elif axis == "sqrt_order":
        declarations = {"half": "    f64 kHalf;\n", "zero": "    f32 kZero;\n", "three": "    f64 kThree;\n"}
        for order in itertools.permutations(declarations):
            def change(b):
                return replace_once(b, "".join(declarations.values()), "".join(declarations[k] for k in order))
            yield "_".join(order), edit_function(source, "fn_80051568", change)
    elif axis == "stack4":
        base = dict(variants(source, "stack3"))
        b = base["inner_pads"]
        old = "            f32 np[3];\n            s32 wallResult;"
        for mode in ("first", "second", "both"):
            def change(text):
                if text.count(old) != 2:
                    raise ValueError("expected two collision branches")
                parts = text.split(old)
                new = "            s32 wallResult;\n            f32 np[3];"
                return parts[0] + (new if mode != "second" else old) + parts[1] + (new if mode != "first" else old) + parts[2]
            yield "collide_" + mode, edit_function(b, "do_enemy_collide", change)
        b = base["distance_locals_bottom"]
        for mode in ("outer_block", "no_constants", "ad_last", "no_range"):
            def change(text):
                marker = "    f32 ad;\n    volatile f32 distanceScratch0, distanceScratch1, distanceScratch2, distanceScratch3;\n"
                if mode == "outer_block":
                    return replace_once(text, marker, "    {\n" + marker)[:-2] + "    }\n}\n"
                if mode == "ad_last":
                    return replace_once(text, marker, "    volatile f32 distanceScratch0, distanceScratch1, distanceScratch2, distanceScratch3;\n    f32 ad;\n")
                if mode == "no_range":
                    text = replace_once(text, "    f32 range;\n", "")
                    return re.sub(r"\brange\b", "dist", text)
                return dict(variants(edit_function(source, "fn_800516F8", lambda _: text), "constant_locals"))["fn_800516F8"]
            if mode == "no_constants":
                # This generator returns a whole TU, unlike function edits.
                yield "distance_" + mode, dict(variants(b, "constant_locals"))["fn_800516F8"]
            else:
                yield "distance_" + mode, edit_function(b, "fn_800516F8", change)
    elif axis == "no_propagation":
        for fn in ("fn_80051568", "fn_800516F8", "do_enemy_move"):
            yield fn, edit_function(source, fn, lambda b: "#pragma opt_propagation off\n" + b + "#pragma opt_propagation reset\n")
    elif axis == "joint5":
        b = dict(variants(source, "stack3"))["inner_pads"]
        for mode in ("second_reuse", "second_reuse_first_pad"):
            def change(text):
                first = text.index("            f32 np[3];")
                second = text.index("            f32 np[3];", first + 1)
                end = text.index("            result = wallResult;", second) + len("            result = wallResult;")
                part = text[second:end].replace("            s32 wallResult;\n", "").replace("            result = wallResult;", "")
                part = re.sub(r"\bwallResult\b", "result", part)
                text = text[:second] + part + text[end:]
                if mode.endswith("first_pad"):
                    text = text[:first] + text[first:].replace("            f32 np[3];", "            f32 np[3];\n            u8 npPad[4];", 1)
                return text
            yield "collide_" + mode, edit_function(b, "do_enemy_collide", change)
        b = dict(variants(source, "stack3"))["move_normal_no_hi"]
        for mode in ("restore_bottom", "reuse", "reuse_no_matrix_gap"):
            def change(text):
                text = replace_once(text, "    f32 half[3];\n", "    f32 half[3];\n    u8 unused4[12];\n")
                if mode != "restore_bottom":
                    for var in ("n", "result"):
                        text = replace_once(text, "    s32 " + var + ";\n", "")
                        text = re.sub(r"\b" + var + r"\b", "collide", text)
                if mode.endswith("no_matrix_gap"):
                    text = re.sub(r"^.*u8 matrixGap\[8\].*\n", "", text, flags=re.M)
                return text
            yield "move_" + mode, edit_function(b, "do_enemy_move", change)
        b = dict(variants(source, "no_propagation"))["fn_80051568"]
        yield "sqrt_restore_pad", edit_function(b, "fn_80051568", lambda text: replace_once(text, "    u8 unused[8];", "    u8 _spare[24];\n    u8 unused[8];"))
        b = dict(variants(source, "stack3"))["distance_locals_bottom"]
        for mode in ("compound", "plus_assign", "no_propagation"):
            def change(text):
                if mode == "no_propagation":
                    return "#pragma opt_propagation off\n" + text + "#pragma opt_propagation reset\n"
                pattern = r"(\*\(f32\*\)\(r \+ 2600\)) = \(f32\)\(\1 \+ 2\.0\);"
                found = re.search(pattern, text)
                if not found:
                    raise ValueError("missing final range increment")
                new = found[1] + (" += 2.0;" if mode == "compound" else " = 2.0 + " + found[1] + ";")
                return replace_once(text, found[0], new)
            yield "distance_" + mode, edit_function(b, "fn_800516F8", change)
    elif axis == "distance6":
        b = dict(variants(source, "stack3"))["distance_locals_bottom"]
        for mode in ("compound", "no_propagation", "both", "half_float", "scratch_inner", "outer_constants"):
            def change(text):
                if mode in ("compound", "both"):
                    text = replace_once(text, "*(f32*)(r + 2600) = (f32)(*(f32*)(r + 2600) + 2.0);", "*(f32*)(r + 2600) += 2.0;")
                if mode in ("no_propagation", "both"):
                    return "#pragma opt_propagation off\n" + text + "#pragma opt_propagation reset\n"
                if mode == "half_float":
                    return replace_once(text, "    f64 kHalf;", "    f32 kHalf;")
                if mode == "scratch_inner":
                    text = replace_once(text, "    volatile f32 distanceScratch0, distanceScratch1, distanceScratch2, distanceScratch3;\n", "")
                    for i in range(4):
                        pattern = r"ENEMY_DISTANCE3\([^;]+\bdistanceScratch" + str(i) + r"\);"
                        found = re.search(pattern, text)
                        if found is None:
                            raise ValueError("missing macro call")
                        text = replace_once(text, found[0], "{ volatile f32 distanceScratch" + str(i) + ";\n" + found[0] + "\n}")
                    return text
                if mode == "outer_constants":
                    # Move the two early distance-kernel constants out of
                    # their macro scopes without introducing volatility.
                    text = replace_once(text, "    u8* p;", "    f64 earlyHalf = 0.5;\n    f64 earlyThree = 3.0;\n    u8* p;")
                    return text.replace(", 0.0f, 0.5, 3.0, distanceScratch", ", 0.0f, earlyHalf, earlyThree, distanceScratch")
                return text
            yield mode, edit_function(b, "fn_800516F8", change)
    elif axis == "collide6":
        b = dict(variants(source, "stack3"))["inner_pads"]
        for mode in ("outer_factor", "second_factor", "reuse_outer_slide", "second_np_scope"):
            def change(text):
                if mode == "outer_factor":
                    text = replace_once(text, "    f32 slideRad;", "    f64 wallScale = 1.5;\n    f32 slideRad;")
                    return text.replace("            slideRad *= 1.5;", "            slideRad *= wallScale;")
                if mode == "second_factor":
                    text = replace_once(text, "            slideRad = rad;\n            slideRad *= 1.5;", "            f64 wallScale = 1.5;\n            slideRad = rad;\n            slideRad *= wallScale;")
                    return text
                if mode == "reuse_outer_slide":
                    old = "            slideRad = rad;\n            slideRad *= 1.5;"
                    return replace_once(text, old, "            slideRad = (f32)((f64)rad * 1.5);")
                first = text.index("            f32 np[3];")
                second = text.index("            f32 np[3];", first+1)
                end = text.index("            result = wallResult;", second)
                part = text[second:end]
                part = replace_once(part, "            f32 np[3];\n", "")
                part = replace_once(part, "            slideRad *= 1.5;", "            slideRad *= 1.5;\n            {\n            f32 np[3];")
                return text[:second] + part + "            }\n" + text[end:]
            yield mode, edit_function(b, "do_enemy_collide", change)
    elif axis == "hoist7":
        b = dict(variants(source, "stack3"))["inner_pads"]
        for mode in ("plain", "gaps", "no_outer_pad"):
            def change(text):
                first = text.index("            f32 np[3];")
                second = text.index("            f32 np[3];", first+1)
                end = text.index("            result = wallResult;", second)
                p2 = text[second:end].replace("            f32 np[3];\n", "")
                p2 = re.sub(r"\bnp\b", "next", p2)
                text = text[:second] + p2 + text[end:]
                end = text.index("            result = wallResult;", first)
                p1 = text[first:end].replace("            f32 np[3];\n", "")
                p1 = re.sub(r"\bnp\b", "ratNext", p1)
                text = text[:first] + p1 + text[end:]
                decl = "    f32 ratNext[3];\n    f32 next[3];\n"
                if mode == "gaps":
                    decl = "    f32 ratNext[3];\n    u8 ratGap[4];\n    f32 next[3];\n    u8 nextGap[4];\n"
                text = replace_once(text, "    u8 unused[4];", "    u8 unused[4];\n" + decl)
                if mode == "no_outer_pad":
                    text = text.replace("    u8 unused[4];\n", "").replace("    (void)unused;\n", "")
                return text
            yield "collide_" + mode, edit_function(b, "do_enemy_collide", change)
        for mode in ("reuse_fd", "reuse_fd_compound", "reuse_fd_inner", "outer_ad_only"):
            b = dict(variants(source, "stack3"))["distance_locals_bottom"]
            if mode.endswith("inner"):
                b = dict(variants(source, "distance6"))["scratch_inner"]
            def change(text):
                if mode == "outer_ad_only":
                    text = replace_once(text, "    f32 ad;\n", "")
                    return replace_once(text, "                        ad =", "                        f32 ad;")
                text = replace_once(text, "            f32 fd;\n", "")
                text = re.sub(r"\bfd\b", "dist", text)
                if mode.endswith("compound"):
                    text = replace_once(text, "*(f32*)(r + 2600) = (f32)(*(f32*)(r + 2600) + 2.0);", "*(f32*)(r + 2600) += 2.0;")
                return text
            if mode != "outer_ad_only":
                yield "distance_" + mode, edit_function(b, "fn_800516F8", change)
    elif axis == "slots8":
        b = dict(variants(source, "stack3"))["inner_pads"]
        for mode in ("vector4", "pad_declaration_before", "long_result", "blockless"):
            def change(text):
                first = text.index("            f32 np[3];")
                second = text.index("            f32 np[3];", first+1)
                end = text.index("            result = wallResult;", second)
                part = text[second:end]
                if mode == "vector4":
                    part = replace_once(part, "f32 np[3];", "f32 np[4];")
                if mode == "pad_declaration_before":
                    part = replace_once(part, "f32 np[3];", "u8 npPad[4];\n            f32 np[3];")
                if mode == "long_result":
                    part = replace_once(part, "s32 wallResult;", "int wallResult;")
                if mode == "blockless":
                    part = replace_once(part, "f32 np[3];", "f32 np[3];\n#pragma opt_lifetimes off")
                    part += "#pragma opt_lifetimes reset\n"
                return text[:second] + part + text[end:]
            yield "collide_" + mode, edit_function(b, "do_enemy_collide", change)
        b = dict(variants(source, "stack3"))["distance_locals_bottom"]
        for mode in ("array", "array_reverse", "ad_nested", "kzero_reuse"):
            def change(text):
                if mode.startswith("array"):
                    text = replace_once(text, "    volatile f32 distanceScratch0, distanceScratch1, distanceScratch2, distanceScratch3;", "    volatile f32 distanceScratch[4];")
                    for i in range(4):
                        text = re.sub(r"\bdistanceScratch" + str(i) + r"\b", "distanceScratch[" + str(3-i if mode == "array" else i) + "]", text)
                elif mode == "ad_nested":
                    text = replace_once(text, "    f32 ad;\n", "")
                    text = replace_once(text, "                        ad = get_yaw", "                        f32 ad = get_yaw")
                else:
                    text = replace_once(text, "    f32 kZero;\n", "")
                    text = replace_once(text, "            kZero = 0.0f;", "            bestSpecial = 0.0f;")
                    text = re.sub(r"\bkZero\b", "bestSpecial", text)
                return replace_once(text, "*(f32*)(r + 2600) = (f32)(*(f32*)(r + 2600) + 2.0);", "*(f32*)(r + 2600) += 2.0;")
            if mode != "kzero_reuse":
                yield "distance_" + mode, edit_function(b, "fn_800516F8", change)
    elif axis == "owners9":
        for mode in ("collide_no_high", "collide_no_high_inner_pads", "distance_r_outer", "distance_p_reuse"):
            if mode.startswith("collide"):
                b = dict(variants(source, "stack3"))["inner_pads"] if mode.endswith("inner_pads") else source
                def change(text):
                    text = replace_once(text, "            f64 high;\n", "")
                    text = replace_once(text, "            high = 3.141592654;\n", "")
                    return re.sub(r"\bhigh\b", "3.141592654", text)
                yield mode, edit_function(b, "do_enemy_collide", change)
            else:
                b = dict(variants(source, "distance6"))["compound"]
                def change(text):
                    old = "                u8* r = (u8*)base +"
                    text = replace_once(text, old, "                r = (u8*)base +")
                    if mode.endswith("outer"):
                        text = replace_once(text, "    u8* p;", "    u8* r;\n    u8* p;")
                    else:
                        text = re.sub(r"\br\b", "p", text)
                    return text
                yield mode, edit_function(b, "fn_800516F8", change)
    elif axis == "distance10":
        b = dict(variants(source, "distance6"))["compound"]
        for mode in ("no_big", "no_big_top4", "no_big_top8", "no_big_last", "no_big_first", "no_kpi"):
            def change(text):
                if mode.startswith("no_big"):
                    m = re.search(r"        f32 big = ([0-9.]+f);", text)
                    if not m:
                        raise ValueError("missing big local")
                    if mode == "no_big_last":
                        tail = text.index(m[0])
                        text = text[:tail] + re.sub(r"\bbig\b", m[1], text[tail:].replace(m[0] + "\n", "", 1))
                    elif mode == "no_big_first":
                        tail = text.index(m[0])
                        first = text[:tail].replace("            f32 big;\n", "").replace("            big = 100000.0f;\n", "")
                        text = re.sub(r"\bbig\b", m[1], first) + text[tail:]
                    else:
                        text = text.replace("            f32 big;\n", "").replace("            big = 100000.0f;\n", "")
                        text = replace_once(text, m[0] + "\n", "")
                        text = re.sub(r"\bbig\b", m[1], text)
                    if mode in ("no_big_top4", "no_big_top8"):
                        count = 4 if mode.endswith("4") else 8
                        text = text.replace("{\n", "{\n    u8 unused[" + str(count) + "];\n", 1)
                else:
                    text = replace_once(text, "    f64 kPi;\n", "")
                    text = replace_once(text, "            kPi = 3.141592654;\n", "")
                    text = re.sub(r"\bkPi\b", "3.141592654", text)
                return text
            yield mode, edit_function(b, "fn_800516F8", change)
    elif axis == "entry11":
        yield "noprop", edit_function(source, "do_enemy_move", lambda b: "#pragma opt_propagation off\n" + b + "#pragma opt_propagation reset\n")
        yield "memset_helper", replace_once(source, "    *(u32*)lbl_802510F4 = 0;", "    memset(lbl_802510F4, 0, sizeof(f32));")
    elif axis == "entry12":
        from r61_enemy_probe import ENTRY
        for mode in ("inner_owner", "inline_body", "struct_diagnostic", "array_diagnostic"):
            def change(body):
                if mode == "inner_owner":
                    body = replace_once(body, "    Enemy* e;\n", "")
                    body = replace_once(body, ENTRY, "    {\n    Enemy* e;\n" + ENTRY)
                    pos = body.rfind("}")
                    return body[:pos] + "    }\n" + body[pos:]
                if mode == "inline_body":
                    body = replace_once(body, "void do_enemy_move(s32 index)", "static inline void enemy_move_record(s32 index, Enemy* e)")
                    body = replace_once(body, "    Enemy* e;\n", "")
                    body = replace_once(body, ENTRY + "\n", "")
                    body = "#pragma dont_inline off\n#pragma inline_max_size(10000)\n" + body
                    return body + "\nvoid do_enemy_move(s32 index)\n{\n    enemy_move_record(index, &gEnemies[index]);\n}\n#pragma inline_max_size reset\n"
                # EXPERIMENTAL COMPATIBILITY ONLY, not recovered structure.
                body = replace_once(body, "    Enemy* e;", "    struct { Enemy* e; } owner;" if mode == "struct_diagnostic" else "    Enemy* owner[1];")
                start = body.index("    s32 alg;")
                return body[:start] + re.sub(r"\be\b", "owner.e" if mode == "struct_diagnostic" else "owner[0]", body[start:])
            yield mode, edit_function(source, "do_enemy_move", change)
    elif axis == "product13":
        from r61_enemy_probe import ENTRY
        for name in ("byteOffset", "n", "result", "collide", "alg"):
            for binding in ("statement", "expression"):
                def change(body):
                    if name == "byteOffset":
                        body = replace_once(body, "    Enemy* e;", "    u32 byteOffset;\n    Enemy* e;")
                    expr = "index * sizeof(Enemy)"
                    if binding == "statement":
                        entry = "    " + name + " = " + expr + ";\n" + ENTRY.replace(expr, name)
                    else:
                        entry = ENTRY.replace(expr, "(" + name + " = " + expr + ")")
                    return replace_once(body, ENTRY, entry)
                yield name + "_" + binding, edit_function(source, "do_enemy_move", change)
        def inline_tick(body):
            start = body.index("    if (e->stun_timer > 0)")
            end = body.index("    if (e->action >= 28)", start)
            helper = "static inline void enemy_update_stun(Enemy* e)\n{\n" + body[start:end] + "}\n\n"
            return helper + body[:start] + "    enemy_update_stun(e);\n" + body[end:]
        yield "inline_tick", edit_function(source, "do_enemy_move", inline_tick)
    elif axis == "stun14":
        from r61_enemy_probe import ENTRY
        for mode in ("returned", "returned_direct", "argument_from_row", "return_row", "prepared"):
            def change(body):
                start = body.index("    if (e->stun_timer > 0)")
                end = body.index("    if (e->action >= 28)", start)
                helper = "static inline Enemy* enemy_update_stun(Enemy* e)\n{\n"
                helper += body[start:end] + "    return e;\n}\n\n"
                prefix = body[:start]
                tail = body[end:]
                if mode == "returned":
                    call = "    e = enemy_update_stun(e);\n"
                elif mode == "prepared":
                    inits = "    alg = e->algorithm;\n    rad = e->rad;\n    hht = e->hht;\n    blocked = 0;\n"
                    prefix = replace_once(prefix, inits, "")
                    call = "    e = enemy_update_stun(e);\n" + inits
                else:
                    if mode == "returned_direct":
                        prefix = replace_once(prefix, ENTRY, "    e = &gEnemies[index];")
                        call = "    e = enemy_update_stun(e);\n"
                    else:
                        call = "    e = enemy_update_stun(e);\n"
                        if mode == "argument_from_row":
                            helper = helper.replace("{\n", "{\n    e = (Enemy*)((u8*)e + ENEMY_POOL_OFF);\n", 1)
                            call = "    e = enemy_update_stun((Enemy*)((u8*)e - ENEMY_POOL_OFF));\n"
                        else:
                            helper = helper.replace("    return e;", "    return (Enemy*)((u8*)e - ENEMY_POOL_OFF);")
                            call = "    e = (Enemy*)((u8*)enemy_update_stun(e) + ENEMY_POOL_OFF);\n"
                return helper + prefix + call + tail
            yield mode, edit_function(source, "do_enemy_move", change)
    elif axis == "pool15":
        base = replace_once(source, "    *(u32*)lbl_802510F4 = 0;", "    memset(lbl_802510F4, 0, sizeof(f32));")
        for kind in ("inline", "extern inline", "__declspec(weak)"):
            yield kind.replace(" ", "_"), replace_once(base, "__declspec(weak) f32 fn_80034C88", kind + " f32 fn_80034C88")
    elif axis == "current16":
        for mode, pragma in (("align4", "align 4"), ("mac68k4byte", "options align=mac68k4byte"), ("unpooled", "pooled_data off")):
            yield mode, "#pragma " + pragma + "\n" + source
        yield "byte_clear", replace_once(source, "    memset(lbl_802510F4, 0, sizeof(f32));", "    ((unsigned char*)lbl_802510F4)[0] = 0;\n    ((unsigned char*)lbl_802510F4)[1] = 0;\n    ((unsigned char*)lbl_802510F4)[2] = 0;\n    ((unsigned char*)lbl_802510F4)[3] = 0;")
    elif axis == "current17":
        # Reconstruct ordinary initialized tables immediately preceding the
        # jump tables. Their original input-section ownership was not claimed.
        addresses = wf.load_symbol_addresses(str(REPO / "config/GUNE5D/symbols.txt"))
        by_address = {v[1]: k for k, v in addresses.items() if v[0] == ".rodata"}
        values = struct.unpack(">3I", read_datum(0x8011BFF8, 12))
        names = []
        declared = set()
        for value in values:
            if not value:
                names.append("0")
                continue
            address = max(a for a in by_address if a <= value)
            name = by_address[address]
            declared.add(name)
            names.append(name + (" + " + str(value - address) if address != value else ""))
        declarations = "\n".join("extern char " + n + "[];" for n in sorted(declared))
        objects = declarations + "\nchar* lbl_8011BFF8[3] = {\n    " + ",\n    ".join(names) + "\n};\n"
        integers = struct.unpack(">16i", read_datum(0x8011C004, 64))
        objects += "s32 lbl_8011C004[16] = { " + ", ".join(str(v) for v in integers) + " };\n"
        for address in (0x8011C044, 0x8011C064, 0x8011C084, 0x8011C0A4, 0x8011C0C4):
            count = 10 if address == 0x8011C0C4 else 8
            floats = struct.unpack(">" + str(count) + "f", read_datum(address, count * 4))
            literals = [format(v, ".9g") for v in floats]
            literals = [v + (".0" if "." not in v and "e" not in v else "") + "f" for v in literals]
            objects += "f32 lbl_%08X[%d] = { %s };\n" % (address, count, ", ".join(literals))
        for mode in ("data_tables", "data_tables_unpooled"):
            candidate = replace_once(source, "/* forward decls for the cross-referenced enemy entry points */", objects + "\n/* forward decls for the cross-referenced enemy entry points */")
            if mode.endswith("unpooled"):
                candidate = "#pragma pooled_data off\n" + candidate
            yield mode, candidate
    elif axis == "current18":
        inits = "    alg = e->algorithm;\n    rad = e->rad;\n    hht = e->hht;\n    blocked = 0;\n"
        from r61_enemy_probe import ENTRY, GUARD
        for mode in ("stun_first", "algorithm_last", "hht_first", "after_stun", "after_action", "typed_staged", "array_pointer"):
            def change(body):
                if mode == "stun_first":
                    body = replace_once(body, "    s32 alg;", "    s32 timer;\n    s32 alg;")
                    body = replace_once(body, inits, "    timer = e->stun_timer;\n" + inits)
                    return replace_once(body, GUARD, "    if (timer > 0) {\n        e->stun_timer = timer - gFrameTicks;")
                if mode == "algorithm_last":
                    return replace_once(body, inits, "    rad = e->rad;\n    hht = e->hht;\n    blocked = 0;\n    alg = e->algorithm;\n")
                if mode == "hht_first":
                    return replace_once(body, inits, "    hht = e->hht;\n    rad = e->rad;\n    alg = e->algorithm;\n    blocked = 0;\n")
                if mode in ("after_stun", "after_action"):
                    body = replace_once(body, inits, "")
                    anchor = "    if (e->action >= 28)" if mode == "after_stun" else "    e->trans[0] += e->pushed[0] * gClockFrameStep;"
                    return replace_once(body, anchor, inits + anchor)
                if mode == "typed_staged":
                    body = replace_once(body, "    Enemy* e;", "    EnemyMovePage05* row;\n    Enemy* e;")
                    return replace_once(body, ENTRY, "    row = (EnemyMovePage05*)((u8*)lbl_80250E00 + index * sizeof(Enemy));\n    e = row->enemies;")
                body = replace_once(body, "    Enemy* e;", "    Enemy (*e)[1];")
                body = replace_once(body, ENTRY, "    e = (Enemy (*)[1])((u8*)lbl_80250E00 + index * sizeof(Enemy));\n    e = (Enemy (*)[1])((u8*)e + ENEMY_POOL_OFF);")
                body = body.replace("e->", "(*e)->")
                body = body.replace("contactOwner = e;", "contactOwner = (*e);")
                # Pointer-to-array preserves each use of the element pointer.
                body = re.sub(r"(?<=[,(])e(?=[,)])", "(*e)", body)
                return body
            yield mode, edit_function(source, "do_enemy_move", change)
    elif axis == "current19":
        for old, new in (("e", "enemy"), ("alg", "algorithm"), ("rad", "radius"), ("hht", "height"), ("n", "i"), ("index", "enemyIndex"), ("other", "otherEnemy"), ("blocked", "movementBlocked"), ("result", "collisionResult"), ("mat", "matrix")):
            yield new, edit_function(source, "do_enemy_move", lambda b: re.sub(r"(?<!->)(?<!\.)\b" + old + r"\b", new, b))
    elif axis == "current20":
        from r61_enemy_probe import ENTRY
        for field in ("algorithm", "stun_timer", "rad", "hht"):
            for mode in ("typed", "bytes"):
                def change(body):
                    typ = "EnemyMovePage05*" if mode == "typed" else "u8*"
                    body = replace_once(body, "    Enemy* e;", "    " + typ + " row;\n    Enemy* e;")
                    setup = "    row = (" + typ + ")((u8*)lbl_80250E00 + index * sizeof(Enemy));\n"
                    setup += "    e = row->enemies;" if mode == "typed" else "    e = (Enemy*)(row + ENEMY_POOL_OFF);"
                    body = replace_once(body, ENTRY, setup)
                    expr = "row->enemies[0]." + field if mode == "typed" else "((Enemy*)(row + ENEMY_POOL_OFF))->" + field
                    # Keep each read/modify/write on one consistent owner.
                    return body.replace("e->" + field, expr)
                yield mode + "_" + field, edit_function(source, "do_enemy_move", change)
        for setting in ("opt_strength_reduction", "opt_loop_invariants", "opt_dead_code", "opt_dead_stores"):
            yield setting, edit_function(source, "do_enemy_move", lambda b: "#pragma " + setting + " off\n" + b + "#pragma " + setting + " reset\n")
    elif axis == "current21":
        from r61_enemy_probe import ENTRY
        for mode in ("nopeephole_helper", "nopeephole_tick", "nopeephole_getter"):
            def change(body):
                if mode == "nopeephole_helper":
                    helper = "static inline Enemy* enemy_at_index(s32 index)\n{\n    Enemy* e;\n" + ENTRY + "\n    return e;\n}\n"
                    caller = replace_once(body, ENTRY, "    e = enemy_at_index(index);")
                elif mode == "nopeephole_getter":
                    helper = "static inline s32 enemy_stun_count(Enemy* e)\n{\n    return e->stun_timer;\n}\n"
                    caller = replace_once(body, "    s32 alg;", "    s32 timer;\n    s32 alg;")
                    caller = replace_once(caller, "    if (e->stun_timer > 0) {\n        e->stun_timer -= gFrameTicks;", "    timer = enemy_stun_count(e);\n    if (timer > 0) {\n        e->stun_timer = timer - gFrameTicks;")
                else:
                    start = body.index("    if (e->stun_timer > 0)")
                    end = body.index("    if (e->action >= 28)", start)
                    helper = "static inline void enemy_stun_tick(Enemy* e)\n{\n" + body[start:end] + "}\n"
                    caller = body[:start] + "    enemy_stun_tick(e);\n" + body[end:]
                return "#pragma peephole off\n" + helper + "#pragma peephole reset\n" + caller
            yield mode, edit_function(source, "do_enemy_move", change)
    elif axis == "current22":
        for mode in ("static", "int_parameter", "static_int", "int_locals", "int_locals_and_parameter"):
            candidate = source
            if mode in ("int_parameter", "static_int", "int_locals_and_parameter"):
                candidate = candidate.replace("void do_enemy_move(s32 index)", "void do_enemy_move(int index)")
            if mode in ("static", "static_int"):
                candidate = re.sub(r"(?m)^void do_enemy_move\(", "static void do_enemy_move(", candidate)
            if mode.startswith("int_locals"):
                candidate = edit_function(candidate, "do_enemy_move", lambda b: re.sub(r"(?m)^(    )s32( (?!hitWorld;)\w+;)$", r"\1int\2", b))
            yield mode, candidate


def compile_logged(edge, path, folder):
    """Compile once and preserve diagnostics on success as well as failure."""
    if "extab" in edge["rule"]:
        raise ValueError("this enemy-only driver does not reproduce extab-clean compile edges")
    obj = folder / "enemy.o"
    cmd = ([str(REPO / "build/tools/sjiswrap.exe")] if "sjis" in edge["rule"] else [])
    cmd += [str(REPO / "build/compilers" / edge["mw"] / "mwcceppc.exe")]
    cmd += shlex.split(edge["cflags"]) + ["-c", str(path), "-o", str(obj)]
    result = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    diagnostic = result.stdout + result.stderr
    (folder / "compiler-output.txt").write_text(diagnostic, encoding="utf-8")
    if diagnostic:
        print(diagnostic, flush=True)
    if result.returncode or not obj.exists():
        return None, "compiler failed; see compiler-output.txt (exit %d)" % result.returncode
    return obj, None


def summary(obj, baseline, target):
    functions = disassemble_words(str(obj))
    raw = obj.read_bytes()
    section = next(s for s in wf._sections(raw) if s.name == ".sdata2")
    data = raw[section.offset:section.offset+section.size]
    changes = []
    for n, k, d in compare_words(baseline, functions):
        rows = d[2]
        stack = [r for r in rows if "(r1)" in r[1] or "(r1)" in r[2] or re.search(r"\baddi\s+r\d+,r1,", r[1] + r[2])]
        changes.append(dict(function=n, kind=k, before=d[0], after=d[1], words=len(rows), stack_words=len(stack)))
    return dict(pool_size=len(data), pool_exact=data == read_datum(POOL_START, POOL_END-POOL_START),
                sections={s.name: dict(size=s.size, align=struct.unpack_from(">I", raw, struct.unpack_from(">I", raw, 32)[0] + s.index * 40 + 32)[0]) for s in wf._sections(raw) if s.name in (".data", "extab", "extabindex")},
                changes=changes, added=sorted(set(functions)-set(baseline)),
                target_counts={n: [len(target[n]), len(functions.get(n, []))] for n in target if len(target[n]) != len(functions.get(n, []))})


def verify_pins(obj):
    original = obj.read_bytes()
    current = bytearray(original)
    target = Path(target_object(UNIT)).read_bytes()
    rules = json.loads((REPO / "config/GUNE5D/webfrank.json").read_text())["units"][UNIT]
    addresses = wf.load_symbol_addresses(str(REPO / "config/GUNE5D/symbols.txt"))
    image = wf.RetailImage(str(REPO / "orig/GUNE5D/sys/main.dol"))
    failures = {}
    successful = []
    for rule in rules:
        fn = rule["function"]
        trial = bytearray(current)
        try:
            candidate = candidate_rule(rule, derive_slots(rule, trial, target, fn))
            with contextlib.redirect_stdout(io.StringIO()):
                wf.apply_patch(trial, candidate, target, addresses, image)
            if _symbol_body(trial, fn)[3] != _symbol_body(target, fn)[3]:
                raise ValueError("APPLIED-NOT-EQUAL")
            successful.append(fn)
            current = trial
        except ValueError as e:
            failures[fn] = str(e)
    # Generated proof output only: no production object or rule is touched.
    patched = obj.with_name("verified-pins.o")
    patched.write_bytes(current)
    rows = compare_words(disassemble_words(str(target_object(UNIT))), disassemble_words(str(patched)))
    return dict(pin_passed=successful, pin_refused=failures,
                remaining={n: len(d[2]) if k == "WORDS" else [d[0], d[1]] for n, k, d in rows})


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("axis", choices=["baseline", "multiply", "angle", "reaction", "pads", "simple", "stack2", "constant_locals", "stack3", "sqrt_order", "stack4", "no_propagation", "joint5", "distance6", "collide6", "hoist7", "slots8", "owners9", "distance10", "entry11", "entry12", "product13", "stun14", "pool15", "current16", "current17", "current18", "current19", "current20", "current21", "current22"])
    ap.add_argument("--show")
    ap.add_argument("--function")
    ap.add_argument("--source", action="store_true")
    ap.add_argument("--raw", action="store_true", help="Compare words to production raw instead of retail")
    ap.add_argument("--only")
    ap.add_argument("--verify", action="store_true")
    ap.add_argument("--emit-patch", action="store_true", help="Print an apply_patch source diff; never apply it")
    ap.add_argument("--baseline-source", type=Path)
    ap.add_argument("--baseline-object", type=Path)
    args = ap.parse_args()
    if bool(args.baseline_source) != bool(args.baseline_object):
        ap.error("historical fidelity requires both --baseline-source and --baseline-object")
    edge = read_edges()[UNIT]
    source_path = args.baseline_source or REPO / edge["src"]
    expected_path = args.baseline_object or REPO / edge["body_o"]
    source = source_path.read_text(encoding="utf-8")
    out = OUT / args.axis
    out.mkdir(parents=True, exist_ok=True)
    if args.show:
        folder = out / args.show
        if not args.function:
            raise ValueError("--show needs --function")
        if args.source:
            text = (folder / "enemy.c").read_text(encoding="utf-8")
            a, b = function_span(text, args.function)
            print("".join(text.splitlines(keepends=True)[a:b]))
        else:
            before = expected_path if args.raw else target_object(UNIT)
            a, b = parse(before)[args.function], parse(folder / "enemy.o")[args.function]
            # Retain reloc annotations when different, but suppress their
            # name-only churn for this body diagnosis (not a reloc proof).
            a = [r for r in a if "R_PPC" not in r]
            b = [r for r in b if "R_PPC" not in r]
            print("\n".join(difflib.unified_diff(a, b, fromfile="RAW" if args.raw else "TARGET", tofile=args.show)))
        return
    if args.emit_patch:
        candidate = fixes(seed(source), 5)
        candidate = replace_once(candidate, "    *(u32*)lbl_802510F4 = 0;", "    memset(lbl_802510F4, 0, sizeof(f32));")
        diff = list(difflib.unified_diff(source.splitlines(True), candidate.splitlines(True)))
        print("*** Begin Patch\n*** Update File: " + str(REPO / edge["src"]).replace("\\", "/"))
        for line in diff[2:]:
            print("@@" if line.startswith("@@") else line.rstrip("\n"))
        print("*** End Patch")
        return
    baseline = compile_baseline(edge, source_path, out / "baseline.o", expected_path, out)
    print("FIDELITY: whole raw object BYTE-IDENTICAL", flush=True)
    base_funcs = disassemble_words(str(baseline))
    target = disassemble_words(str(target_object(UNIT)))
    stages = {"multiply": 0, "angle": 0, "reaction": 0, "pads": 1, "simple": 1,
              "stack2": 2, "constant_locals": 2, "stack3": 3,
              "sqrt_order": 3, "stack4": 3, "no_propagation": 3, "joint5": 3,
              "entry11": 5, "entry12": 5, "product13": 5, "stun14": 5, "pool15": 5, "baseline": 5}
    seed_source = source if args.axis.startswith("current") else fixes(seed(source), stages.get(args.axis, 4))
    for name, candidate in variants(seed_source, args.axis):
        if args.only and args.only not in name:
            continue
        folder = out / name
        folder.mkdir(exist_ok=True)
        path = folder / "enemy.c"
        path.write_text(candidate, encoding="utf-8")
        obj, error = compile_logged(edge, path, folder)
        row = dict(name=name, error=error) if error else dict(name=name, **summary(obj, base_funcs, target))
        (folder / "result.json").write_text(json.dumps(row, indent=2) + "\n", encoding="utf-8")
        if args.verify and not error:
            proof = verify_pins(obj)
            (folder / "proof.json").write_text(json.dumps(proof, indent=2) + "\n", encoding="utf-8")
            print(json.dumps(proof), flush=True)
        compact = {k: v for k, v in row.items() if k != "changes"}
        if "changes" in row:
            compact["changes_words_stack"] = {r["function"]: [r["words"], r["stack_words"]] if r["kind"] == "WORDS" else [r["before"], r["after"], "COUNT"] for r in row["changes"]}
        print(json.dumps(compact), flush=True)


if __name__ == "__main__":
    main()
