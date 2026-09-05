"""Reproduce R64's finite source-only register-home experiments.

The actual Ninja edge must reproduce the entire raw object before any probe.
Only generated build/r64_init_next_level files are written. This is a bounded
source-form comparison, not a proof that any compiler output is unreachable.
Raw text equality does not imply relocation, datum, or complete-object equality.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

from cv_probe import REPO, read_edges, compile_with
from pj_body_ab import disassemble_words

UNIT = "game/world/gauntworld"
FUNCTION = "init_next_level_8005638C"
OLD_DECLS = """    u8* tbl = (u8*)lbl_80257680;
    u8* q;
    s32 i;
    s32 result;
    s32 flag;
    s32 off;
    u8* w;
    s32 id;
    s32 t;"""
BEST_DECLS = """    s32 off;
    s32 flag;
    u8* w;
    s32 id;
    s32 t;
    u8* tbl = (u8*)lbl_80257680;
    u8* q;
    s32 i;
    s32 result;"""
OLD_STORES = "    lbl_80344854 = mlmMemUsed;\n    lbl_80343C30 = 0;"
BEST_STORES = "    lbl_80343C30 = 0;\n    lbl_80344854 = mlmMemUsed;"


def once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"historical source guard failed: {old!r}")
    return text.replace(old, new, 1)


def candidates(body):
    if BEST_DECLS in body:
        body = once(body, BEST_DECLS, OLD_DECLS)
        body = once(body, BEST_STORES, OLD_STORES)
    if OLD_DECLS not in body or OLD_STORES not in body:
        raise ValueError("expected the R64 baseline or retained source form")
    yield "historical", body
    yield "cse-on", once(body, "#pragma opt_common_subs off", "#pragma opt_common_subs on")
    lines = OLD_DECLS.splitlines()
    yield "rotation6", once(body, OLD_DECLS, "\n".join(lines[6:] + lines[:6]))
    best = once(once(body, OLD_DECLS, BEST_DECLS), OLD_STORES, BEST_STORES)
    yield "best", best
    start = best.index("        switch (t) {")
    end = best.index("        if (flag != 0)", start)
    switch = re.sub(r"\b(?:id|t)\b", "type", best[start:end])
    helper = (
        "static inline s32 loadEnemyTypeWad(s32 type, u8* tbl, s32 off, u8* w)\n"
        "{\n    s32 flag = 1;\n" + switch + "    return flag;\n}\n\n"
    )
    caller = best[:start] + "        flag = loadEnemyTypeWad(t, tbl, off, w);\n" + best[end:]
    caller = once(once(caller, "        id = t;\n        flag = 1;\n", ""), "    s32 id;\n", "")
    yield "inline-dispatch", "#pragma opt_common_subs off\n" + helper + caller
    yield "inline-dispatch-limit", (
        "#pragma inline_depth(2)\n#pragma inline_max_size(10000)\n"
        "#pragma opt_common_subs off\n" + helper + caller +
        "\n#pragma inline_depth reset\n#pragma inline_max_size reset\n"
    )
    start = best.index("    for (i = 0, off = 0;")
    end = best.index("    CritterLoadAllTypes(0);", start)
    loop = best[start:end]
    declarations = (
        "    s32 off;\n", "    s32 flag;\n", "    u8* w;\n", "    s32 id;\n",
        "    s32 t;\n", "    u8* q;\n", "    s32 i;\n",
    )
    helper = "static inline void loadLevelEnemyTypes(u8* tbl)\n{\n" + "".join(declarations) + loop + "}\n\n"
    caller = best[:start] + "    loadLevelEnemyTypes(tbl);\n\n" + best[end:]
    for declaration in declarations:
        caller = once(caller, declaration, "")
    yield "inline-loop", "#pragma opt_common_subs off\n" + helper + caller
    lines = BEST_DECLS.splitlines()
    for index in range(len(lines) - 1):
        order = lines.copy()
        order[index], order[index + 1] = order[index + 1], order[index]
        yield f"neighbor-{index}", once(best, BEST_DECLS, "\n".join(order))


def mnemonic(row):
    return row[2].split("\t")[-1].split()[0]


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", action="append", help="named case; repeat; omission runs all fifteen")
    parser.add_argument("--out", default="build/r64_init_next_level/r64_results.json")
    args = parser.parse_args()
    edge = read_edges()[UNIT]
    scratch = REPO / "build/r64_init_next_level"
    scratch.mkdir(parents=True, exist_ok=True)
    baseline, error = compile_with(edge, edge["mw"], edge["cflags"], scratch / "r64_fidelity.o", scratch)
    if error:
        raise SystemExit(error)
    production = REPO / edge["body_o"]
    fidelity = baseline.read_bytes() == production.read_bytes()
    print("WHOLE-RAW-OBJECT-FIDELITY", fidelity, "size", baseline.stat().st_size,
          "compiled", digest(baseline), "production", digest(production), flush=True)
    if not fidelity:
        raise SystemExit("fidelity refused: rebuild the actual Ninja raw-object edge first")
    source = (REPO / edge["src"]).read_text(encoding="utf-8")
    start = source.index("#pragma opt_common_subs off\ns32 " + FUNCTION)
    end = source.index("#pragma opt_common_subs on", start) + len("#pragma opt_common_subs on")
    variants = dict(candidates(source[start:end]))
    requested = args.case or list(variants)
    unknown = set(requested) - variants.keys()
    if unknown:
        raise SystemExit(f"unknown cases: {sorted(unknown)}")
    target_path = REPO / ("build/GUNE5D/obj/" + UNIT + ".o")
    target = disassemble_words(str(target_path))
    target_names = [name for name in target if name.startswith("init_next_level")]
    if len(target_names) != 1:
        raise SystemExit(f"ambiguous target alias: {target_names}")
    target_rows = target[target_names[0]]
    baseline_functions = disassemble_words(str(baseline))
    results = []
    for name in requested:
        candidate_source = scratch / ("r64_" + name + ".c")
        candidate_source.write_text(source[:start] + variants[name] + source[end:], encoding="utf-8")
        modified_edge = dict(edge, src=str(candidate_source))
        obj, error = compile_with(modified_edge, edge["mw"], edge["cflags"], scratch / ("r64_" + name + ".o"), scratch)
        if error:
            raise SystemExit(f"{name}: {error}; no negative result may be inferred from a failed compile")
        functions = disassemble_words(str(obj))
        ours = functions[FUNCTION]
        differences = None
        if len(target_rows) == len(ours):
            differences = [{"offset": hex(t[0]), "target": t[2], "ours": o[2]}
                           for t, o in zip(target_rows, ours) if t[1] != o[1]]
        tc, oc = Counter(map(mnemonic, target_rows)), Counter(map(mnemonic, ours))
        delta = {m: oc[m] - tc[m] for m in sorted(tc.keys() | oc.keys()) if tc[m] != oc[m]}
        siblings = [n for n in sorted(functions.keys() & baseline_functions.keys()) if n != FUNCTION
                    and [x[1] for x in functions[n]] != [x[1] for x in baseline_functions[n]]]
        row = {"case": name, "count": [len(target_rows), len(ours)],
               "differing_words": len(differences) if differences is not None else None,
               "opcode_delta_ours_minus_target": delta, "differences": differences,
               "sibling_bodies_changed": siblings,
               "new_emitted_functions": sorted(functions.keys() - baseline_functions.keys()),
               "source_sha256": digest(candidate_source), "object_sha256": digest(obj)}
        results.append(row)
        print(name, row["count"], "words", row["differing_words"], "opcode delta", delta,
              "changed siblings", siblings, "emitted helpers", row["new_emitted_functions"], flush=True)
    output = REPO / args.out
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({"unit": UNIT, "compiler": edge["mw"], "cflags": edge["cflags"],
                                 "raw_fidelity": fidelity, "source_sha256": digest(REPO / edge["src"]),
                                 "target_sha256": digest(target_path), "results": results}, indent=2) + "\n", encoding="utf-8")
    print("wrote", output.relative_to(REPO))


if __name__ == "__main__":
    main()
