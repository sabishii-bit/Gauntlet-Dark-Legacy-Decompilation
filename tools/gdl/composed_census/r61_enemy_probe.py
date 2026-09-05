"""R61 bounded enemy address/pool experiments; only build/ files change.

The real Ninja edge must reproduce the entire raw object before testing.
Raw word equality is not a relocation, data, or complete-TU proof.
Pool variants are EXPERIMENTAL and can drift frozen bodies. They never thaw
production source or refresh rules. The weak sqrt hypothesis is not proof
of original linkage; full-link/metadata validation remains mandatory.
"""
import argparse
import difflib
import json
import re
from pathlib import Path

from r60_enemy_source_probe import (
    REPO, UNIT, compile_baseline, compile_with, function_span, load,
    parse, read_edges, replace_once, target_object,
)
from pj_body_ab import compare_words, disassemble_words
from pj_pool_literalize import DECL, literal_for, read_datum
from cn_analyze import wf
from pooldump import our_pool, describe
from poolval import load_symbols

FUNCTION = "do_enemy_move"
POOL_START = 0x80346810
POOL_END = 0x80346AAC  # Last enemy datum BOSSGEN; next TU starts at 6AB0.
ENTRY = """    e = (Enemy*)((u8*)lbl_80250E00 + index * sizeof(Enemy));
    e = (Enemy*)((u8*)e + ENEMY_POOL_OFF);"""
GUARD = """    if (e->stun_timer > 0) {
        e->stun_timer -= gFrameTicks;"""


def timer_variants(body):
    # Capture the read AND use that captured value for the decrement. A
    # const read by itself can split CSE and introduce a second load.
    for typ in ("s32", "int"):
        for view in ("e", "((const Enemy*)e)"):
            for binding in ("statement", "condition"):
                label = typ + ("_const" if view != "e" else "") + "_" + binding
                candidate = replace_once(body, "    s32 alg;", "    s32 timer;\n    s32 alg;" if typ == "s32" else "    int timer;\n    s32 alg;")
                if binding == "condition":
                    guard = f"    if ((timer = {view}->stun_timer) > 0) {{"
                else:
                    guard = f"    timer = {view}->stun_timer;\n    if (timer > 0) {{"
                yield label, replace_once(candidate, GUARD, guard + "\n        e->stun_timer = timer - gFrameTicks;")
    # A short-lived pointer owns both sides of this read/modify/write.
    for view in ("Enemy*", "const Enemy*"):
        for capture in (False, True):
            candidate = replace_once(body, "    s32 alg;", "    " + view + " initial;\n    s32 timer;\n    s32 alg;" if capture else "    " + view + " initial;\n    s32 alg;")
            candidate = replace_once(candidate, "    alg = e->algorithm;", "    initial = e;\n    alg = e->algorithm;")
            if capture:
                guard = "    if ((timer = initial->stun_timer) > 0) {\n        e->stun_timer = timer - gFrameTicks;"
            elif view == "Enemy*":
                guard = GUARD.replace("e->stun_timer", "initial->stun_timer")
            else:
                continue
            yield "owner_" + ("const_" if "const" in view else "") + str(capture), replace_once(candidate, GUARD, guard)
    # Inline accessors supply a genuine read/modify/write operation, not
    # an identity wrapper. Both return-value and output-pointer shapes.
    for typ in ("Enemy*", "const Enemy*", "s32*"):
        expr = "*timer" if typ == "s32*" else "enemy->stun_timer"
        param = "timer" if typ == "s32*" else "enemy"
        arg = "&e->stun_timer" if typ == "s32*" else "e"
        for out in (False, True):
            helper = (f"static inline {'void' if out else 's32'} enemy_stun_value({typ} {param}" + (", s32* value" if out else "") + ")\n{\n    " + ("*value = " if out else "return ") + expr + ";\n}\n\n")
            candidate = replace_once(body, "    s32 alg;", "    s32 timer;\n    s32 alg;")
            binding = f"    enemy_stun_value({arg}, &timer);" if out else f"    timer = enemy_stun_value({arg});"
            guard = binding + "\n    if (timer > 0) {\n        e->stun_timer = timer - gFrameTicks;"
            yield "accessor_" + param + ("_const" if "const" in typ else "") + ("_out" if out else ""), helper + replace_once(candidate, GUARD, guard)


def optimizer_joint_variants(body):
    for setting in ("opt_propagation", "opt_common_subs", "opt_lifetimes", "scheduling", "peephole"):
        yield setting, f"#pragma {setting} off\n" + body + f"\n#pragma {setting} reset\n"
    yield "register_enemy", replace_once(body, "    Enemy* e;", "    register Enemy* e;")
    # Cross propagation with three coupled timer/owner source shapes.
    for name, candidate in timer_variants(body):
        if name not in ("s32_condition", "accessor_enemy_out", "owner_True"):
            continue
        yield "noprop_" + name, "#pragma opt_propagation off\n" + candidate + "\n#pragma opt_propagation reset\n"


def lookup_variants(body):
    # Output-parameter lookup differs from the previously tested by-value
    # pointer getter: the caller's actual Enemy* local is assigned by it.
    for mode, expr in (("array", "&gEnemies[index]"),
                       ("byte", "(Enemy*)((u8*)lbl_80250E00 + index * sizeof(Enemy) + ENEMY_POOL_OFF)"),
                       ("staged", "(Enemy*)((u8*)lbl_80250E00 + index * sizeof(Enemy))")):
        setup = "    *enemy = " + expr + ";\n"
        if mode == "staged":
            setup += "    *enemy = (Enemy*)((u8*)*enemy + ENEMY_POOL_OFF);\n"
        helper = "static inline void enemy_lookup(s32 index, Enemy** enemy)\n{\n" + setup + "}\n\n"
        candidate = replace_once(body, ENTRY, "    enemy_lookup(index, &e);")
        yield "lookup_out_" + mode, helper + candidate
        helper_timer = helper.replace("void enemy_lookup", "s32 enemy_lookup").replace("\n}\n", "\n    return (*enemy)->stun_timer;\n}\n")
        timer = replace_once(candidate, "    s32 alg;", "    s32 timer;\n    s32 alg;")
        timer = replace_once(timer, "    enemy_lookup(index, &e);", "    timer = enemy_lookup(index, &e);")
        timer = replace_once(timer, GUARD, "    if (timer > 0) {\n        e->stun_timer = timer - gFrameTicks;")
        yield "lookup_timer_" + mode, helper_timer + timer
    # A genuine small timer helper receiving the local by reference.
    helper = """static inline void enemy_stun_tick(Enemy** enemy)
{
    Enemy* e = *enemy;
    if (e->stun_timer > 0) {
        e->stun_timer -= gFrameTicks;
        e->trans[0] = 0.0f;
        e->trans[1] = 0.0f;
        e->trans[2] = 0.0f;
    }
}

"""
    full_guard = GUARD + "\n        e->trans[0] = 0.0f;\n        e->trans[1] = 0.0f;\n        e->trans[2] = 0.0f;\n    }"
    yield "stun_ref", helper + replace_once(body, full_guard, "    enemy_stun_tick(&e);")


def first_use_variants(body):
    # int and signed long are separate source types in this ABI, although
    # both are 32 bits. These are diagnostics, not approved type-pun keeps.
    for typ in ("int", "const int", "s32", "const s32"):
        read = f"(*({typ}*)&e->stun_timer)"
        yield "read_" + typ.replace(" ", "_"), replace_once(body, "if (e->stun_timer > 0)", f"if ({read} > 0)")
        capture = replace_once(body, "    s32 alg;", "    s32 timer;\n    s32 alg;")
        yield "captured_" + typ.replace(" ", "_"), replace_once(capture, GUARD, f"    if ((timer = {read}) > 0) {{\n        e->stun_timer = timer - gFrameTicks;")
    # Binding the completed address at the actual lvalue use tests the
    # assignment-expression tree, rather than another equivalent cast.
    short = replace_once(body, "    e = (Enemy*)((u8*)e + ENEMY_POOL_OFF);\n", "")
    short = replace_once(short, "    alg = e->algorithm;", "    alg = (e = (Enemy*)((u8*)e + ENEMY_POOL_OFF))->algorithm;")
    yield "advance_algorithm", short
    # Keep initialization order, but select e through its first member
    # reference; initial field reads are delayed to the following statement.
    short = replace_once(body, "    e = (Enemy*)((u8*)e + ENEMY_POOL_OFF);\n", "")
    inits = "    alg = e->algorithm;\n    rad = e->rad;\n    hht = e->hht;\n    blocked = 0;\n"
    short = replace_once(short, inits, "    s32 timer;\n")
    # Declaration must precede ENTRY under C89.
    short = replace_once(short, "    s32 timer;\n", "")
    short = replace_once(short, "    s32 alg;", "    s32 timer;\n    s32 alg;")
    short = replace_once(short, GUARD, "    timer = (e = (Enemy*)((u8*)e + ENEMY_POOL_OFF))->stun_timer;\n" + inits + "    if (timer > 0) {\n        e->stun_timer = timer - gFrameTicks;")
    yield "advance_timer", short


def carrier_variants(body):
    # Keep one type-uniform cursor through the whole function, retaining
    # named Enemy members (no numeric field-offset reconstruction).
    for typ, advance in (("u8", "ENEMY_POOL_OFF"), ("u32", "ENEMY_POOL_OFF / sizeof(u32)")):
        for assignment in ("compound", "ordinary"):
            converted = re.sub(r"\be\b", "((Enemy*)cursor)", body)
            converted = replace_once(converted, "    Enemy* ((Enemy*)cursor);", f"    {typ}* cursor;")
            setup_old = re.sub(r"\be\b", "((Enemy*)cursor)", ENTRY)
            setup = f"    cursor = ({typ}*)((u8*)lbl_80250E00 + index * sizeof(Enemy));\n"
            setup += f"    cursor += {advance};" if assignment == "compound" else f"    cursor = cursor + {advance};"
            converted = replace_once(converted, setup_old, setup)
            yield typ + "_" + assignment, converted
    # A block-scoped typed current enemy and an outer traversal owner have
    # the same values but different lexical lifetimes.
    for reuse in ("other", "new_owner"):
        base = body
        if reuse == "new_owner":
            base = replace_once(base, "    Enemy* e;", "    Enemy* base;\n    Enemy* e;")
            owner = "base"
        else:
            owner = "other"
        entry = ENTRY.replace("    e =", "    " + owner + " =", 1)
        entry = entry.replace("(u8*)e + ENEMY_POOL_OFF", "(u8*)" + owner + " + ENEMY_POOL_OFF")
        yield "owner_" + reuse, replace_once(base, ENTRY, entry)


def allocation_variants(body):
    # Declaration rank is a distinct axis on the new staged-address shape.
    without = replace_once(body, "    Enemy* e;\n", "")
    for name, marker in (("after_other", "    Enemy* other;\n"),
                         ("after_mat", "    f32 mat[16];\n"),
                         ("after_vectors", "    u8 unused4[12];\n")):
        yield name, replace_once(without, marker, marker + "    Enemy* e;\n")
    for qualifier in ("register", "const"):
        for address, expr in (("direct", "&gEnemies[index]"),
                              ("bytes", "(Enemy*)((u8*)lbl_80250E00 + index * sizeof(Enemy) + ENEMY_POOL_OFF)")):
            decl = f"    {'register ' if qualifier == 'register' else ''}Enemy* {'const ' if qualifier == 'const' else ''}e = {expr};"
            yield qualifier + "_" + address, replace_once(replace_once(body, "    Enemy* e;", decl), ENTRY + "\n", "")
    # Produce the index multiply from distinct ABI-compatible scalar types.
    for typ in ("int", "unsigned int", "s16", "u16", "u32"):
        yield "index_" + typ.replace(" ", "_"), replace_once(body, "index * sizeof(Enemy)", f"({typ})index * sizeof(Enemy)")


def address_tree_variants(body):
    for name, expr in (("commuted", "index * sizeof(Enemy) + (u8*)lbl_80250E00"),
                       ("product_commuted", "sizeof(Enemy) * index + (u8*)lbl_80250E00"),
                       ("array_index", "&((u8*)lbl_80250E00)[index * sizeof(Enemy)]"),
                       ("indexed_array", "&((u8 (*)[sizeof(Enemy)])lbl_80250E00)[index]")):
        candidate = replace_once(body, "(u8*)lbl_80250E00 + index * sizeof(Enemy)", expr)
        yield name, candidate
    yield "commuted_advance", replace_once(body, "(u8*)e + ENEMY_POOL_OFF", "ENEMY_POOL_OFF + (u8*)e")
    # A pointer-sized integer intermediate is a diagnostic of the lowering
    # tree, not a proposed portable reconstruction endpoint.
    yield "integer_address_diagnostic", replace_once(body, ENTRY, "    e = (Enemy*)((u32)lbl_80250E00 + index * sizeof(Enemy));\n    e = (Enemy*)((u32)e + ENEMY_POOL_OFF);")


def literal_pool(source):
    declared = {m[2]: (m[1], m[3]) for m in DECL.finditer(source)
                if POOL_START <= int(m[2][4:], 16) < POOL_END}
    result = source
    for symbol, (kind, dimension) in declared.items():
        literal = literal_for(symbol, kind, dimension)
        if literal.startswith("-"):
            literal = "(" + literal + ")"
        result = re.sub(r"\*\(volatile\s+(?:f32|f64)\s*\*\)\s*&\s*" + symbol + r"\b", symbol, result)
        result = DECL.sub(lambda m: "" if m[2] == symbol else m[0], result)
        result = re.sub(r"\b" + symbol + r"\b", lambda m: literal, result)
    return result


def pool_variants(source):
    whole = literal_pool(source)
    yield "literal_all", whole
    # The unreferenced order helper owns no target function. Preventing its
    # early float literal from starting the pool is diagnostic only.
    yield "literal_no_helper_float", replace_once(whole, "    lbl_802510F4[0] = 0.0f;", "    (void)lbl_802510F4;")
    yield "literal_helper_integer_zero", replace_once(whole, "    lbl_802510F4[0] = 0.0f;", "    *(u32*)lbl_802510F4 = 0;")
    owned = replace_once(whole, "    lbl_802510F4[0] = 0.0f;", "    *(u32*)lbl_802510F4 = 0;")
    owned = replace_once(owned, "(f64)enemy->damage > (f64)10.0f", "enemy->damage > 10.0f")
    yield "literal_float_comparison", owned
    # Diagnostic for a plausible header-emitted sqrt body whose out-of-line
    # weak copy was coalesced by the linker. This is NOT proof of ownership.
    controls = (REPO / "src/game/game/controls.c").read_text(encoding="utf-8")
    start, end = function_span(controls, "fn_80034C88")
    sqrt_body = "".join(controls.splitlines(keepends=True)[start:end])
    for linkage in ("ordinary", "weak"):
        helper = sqrt_body if linkage == "ordinary" else sqrt_body.replace("f32 fn_80034C88", "__declspec(weak) f32 fn_80034C88", 1)
        helper = "#pragma dont_inline on\n" + helper + "\n#pragma dont_inline reset\n\n"
        candidate = replace_once(owned, "static inline void enemy_nearest_live_player(u8* e, f32 best1, u8* p, s32* nearest)\n{", helper + "static inline void enemy_nearest_live_player(u8* e, f32 best1, u8* p, s32* nearest)\n{")
        yield "literal_sqrt_" + linkage, candidate
        if linkage != "weak":
            continue
        swapped = replace_once(candidate, "f32 fight, f32 health, f32 upper,\n                                     f32 lower, s32 type", "f32 fight, f32 health, f32 lower,\n                                     f32 upper, s32 type")
        swapped = replace_once(swapped, "(f32)(0.667 * threshold), (f32)(0.333 * threshold),", "(f32)(0.333 * threshold), (f32)(0.667 * threshold),")
        yield "literal_sqrt_damage_argument_order", swapped
        for order in ("lower_first", "upper_first"):
            lower = "        f32 lower = (f32)(0.333 * threshold);\n"
            upper = "        f32 upper = (f32)(0.667 * threshold);\n"
            locals_form = replace_once(candidate, "        f32 threshold = gCurLevel->ene_health * lbl_8011BA10[e->type];\n", "        f32 threshold = gCurLevel->ene_health * lbl_8011BA10[e->type];\n" + (lower + upper if order == "lower_first" else upper + lower))
            locals_form = replace_once(locals_form, "(f32)(0.667 * threshold), (f32)(0.333 * threshold),", "upper, lower,")
            yield "literal_sqrt_damage_" + order, locals_form
        old = "    if (health > lower) {\n        return (f32)(0.667 * fight);\n    }\n    return (f32)(0.333 * fight);"
        inverted = "    if (!(health > lower)) {\n        return (f32)(0.333 * fight);\n    }\n    return (f32)(0.667 * fight);"
        yield "literal_sqrt_damage_invert", replace_once(candidate, old, inverted)
        for qual in ("", "const "):
            declaration = "    " + qual + "f64 lowScale = 0.333;\n    " + qual + "f64 highScale = 0.667;\n"
            start, end = function_span(candidate, "scale_fight_damage")
            lines = candidate.splitlines(keepends=True)
            body = "".join(lines[start:end])
            body = body.replace("0.333 * fight", "lowScale * fight").replace("0.667 * fight", "highScale * fight")
            body = body.replace("{\n", "{\n" + declaration, 1)
            yield "literal_sqrt_damage_" + ("const" if qual else "locals"), "".join(lines[:start]) + body + "".join(lines[end:])


def pool_report(obj):
    print("OUR .sdata2 (offsets relative to proposed start 80346810)")
    for name, section, offset, size, blob in our_pool(obj):
        if section == ".sdata2":
            same = blob == read_datum(POOL_START + offset, size)
            print(f"+{offset:03x} {name:8} {size:2} {'=' if same else '!'} {describe(blob)}")
    print("RETAIL .sdata2")
    for name, info in load_symbols().items():
        if POOL_START <= info["addr"] < POOL_END:
            blob = read_datum(info["addr"], info["size"] or 4)
            print(f"+{info['addr']-POOL_START:03x} {name} {info['size']:2} {describe(blob)}")


def bss_symbols(raw):
    sections = wf._sections(raw)
    return {symbol.name: (symbol.value, symbol.size)
            for symbol in wf._symbols(raw, sections)
            if symbol.size and 0 < symbol.section_index < len(sections)
            and sections[symbol.section_index].name == ".bss"}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("axis", choices=["timer", "optimizer_joint", "lookup", "first_use", "carrier", "allocation", "address_tree", "pool"])
    ap.add_argument("--show")
    ap.add_argument("--only")
    ap.add_argument("--entry", action="store_true", help="With --show, print only the first 26 instructions")
    ap.add_argument("--pool-report", action="store_true")
    ap.add_argument("--function", default=FUNCTION, help="Function for --show")
    args = ap.parse_args()
    edge = read_edges()[UNIT]
    out = REPO / "build/r61_enemy_probes" / args.axis
    out.mkdir(parents=True, exist_ok=True)
    if args.show:
        if not re.fullmatch(r"[A-Za-z0-9_]+", args.show):
            raise ValueError("invalid variant name")
        if args.pool_report:
            pool_report(out / args.show / "enemy.o")
        elif args.entry:
            rows = disassemble_words(str(out / args.show / "enemy.o"))[args.function]
            print("\n".join(f"+{r[0]:04x} {r[2]}" for r in rows[:26]))
        else:
            print("\n".join(difflib.unified_diff(parse(target_object(UNIT))[args.function], parse(out / args.show / "enemy.o")[args.function], fromfile="TARGET", tofile=args.show)))
        return
    src = REPO / edge["src"]
    source = src.read_text(encoding="utf-8")
    first, last = function_span(source, FUNCTION)
    lines = source.splitlines(keepends=True)
    pre, body, post = "".join(lines[:first]), "".join(lines[first:last]), "".join(lines[last:])
    baseline = compile_baseline(edge, src, out / "baseline.o", REPO / edge["body_o"], out)
    print("FIDELITY: whole raw object BYTE-IDENTICAL", flush=True)
    base_functions = disassemble_words(str(baseline))
    target = load(str(target_object(UNIT)), FUNCTION)[3]
    rows = []
    generator = {"timer": timer_variants, "optimizer_joint": optimizer_joint_variants, "lookup": lookup_variants, "first_use": first_use_variants, "carrier": carrier_variants, "allocation": allocation_variants, "address_tree": address_tree_variants, "pool": pool_variants}[args.axis]
    for name, candidate in generator(source if args.axis == "pool" else body):
        if args.only and args.only not in name:
            continue
        folder = out / name
        folder.mkdir(exist_ok=True)
        generated = folder / "enemy.c"
        generated.write_text(candidate if args.axis == "pool" else pre + candidate + post, encoding="utf-8")
        obj, error = compile_with(dict(edge, src=str(generated)), edge["mw"], edge["cflags"], folder / "enemy.o", folder)
        if error:
            row = dict(name=name, error=error)
        else:
            actual = load(str(obj), FUNCTION)[3]
            functions = disassemble_words(str(obj))
            changed = [r[0] for r in compare_words(base_functions, functions) if r[0] != FUNCTION]
            row = dict(name=name, target_insns=len(target)//4, ours_insns=len(actual)//4,
                       raw_exact=actual == target, changed_siblings=changed,
                       removed=sorted(set(base_functions)-set(functions)),
                       added=sorted(set(functions)-set(base_functions)))
            if len(actual) == len(target):
                row["differing_words"] = sum(actual[i:i+4] != target[i:i+4] for i in range(0, len(target), 4))
            if args.axis == "pool":
                raw = obj.read_bytes()
                section = next(s for s in wf._sections(raw) if s.name == ".sdata2")
                data = raw[section.offset:section.offset+section.size]
                retail = read_datum(POOL_START, POOL_END - POOL_START)
                row["sdata2_bytes"] = len(data)
                row["prefix_equal"] = next((i for i, (a, b) in enumerate(zip(data, retail)) if a != b), len(data))
                row["full_pool_bytes_equal"] = data == retail
                expected_bss = bss_symbols(Path(target_object(UNIT)).read_bytes())
                ours_bss = bss_symbols(raw)
                row["bss_symbol_differences"] = {name: {"target": pair, "ours": ours_bss.get(name)}
                                                for name, pair in expected_bss.items() if ours_bss.get(name) != pair}
                row["changes"] = [{"function": n, "kind": k, "before": d[0], "after": d[1], "words": len(d[2])} for n, k, d in compare_words(base_functions, functions)]
        rows.append(row)
        print(json.dumps(row), flush=True)
        # Preserve a complete archive even when a later variant fails.
        (folder / "result.json").write_text(json.dumps(row, indent=2) + "\n", encoding="utf-8")
        (out / "results.json").write_text(json.dumps(rows, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
