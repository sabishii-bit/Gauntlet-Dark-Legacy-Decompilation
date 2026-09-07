"""Rank shipped WebFrank rules by raw differing words and function size.

Reads webfrank.json and existing raw/target objects; never builds or modifies
them. Missing or unequal-size objects have no measured word count.
Declared atom counts describe explicit rule operations, not blanket copies.
Proof modes distinguish strict, value-equality and unproven rule declarations.

Usage: python tools/gdl/composed_census/t15_promotion_queue.py
       [--top N] [--proof strict|value-eq|UNPROVEN] [--unit U] [--out PATH]
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                ".."))
import webfrank as wf  # noqa: E402

SRC = "build/GUNE5D/src"
OBJ = "build/GUNE5D/obj"
WEBFRANK_JSON = "config/GUNE5D/webfrank.json"
STAGE_KEYS = ("function", "mechanism", "before_sha256", "after_sha256",
              "audit")


def _load(path):
    data = bytearray(open(path, "rb").read())
    return data, wf._sections(data)


def _functions(data, sections):
    out = {}
    for sym in wf._symbols(data, sections):
        if sym.size and 0 <= sym.section_index < len(sections):
            if sections[sym.section_index].name == ".text":
                out[sym.name] = sym
    return out


def _raw_object(unit_obj):
    head, tail = os.path.split(unit_obj)
    body = os.path.join(head, ".postprocess", "body", tail)
    return body if os.path.exists(body) else unit_obj


def rule_size(rule):
    """Declared atoms: what the rule explicitly names, not what it copies."""
    atoms = 0
    for key, value in rule.items():
        if key in STAGE_KEYS:
            continue
        if key == "instruction_permutation":
            windows = value if isinstance(value, list) else [value]
            atoms += sum(len(window.get("order", ())) for window in windows)
        elif isinstance(value, list):
            atoms += len(value)
        elif isinstance(value, dict):
            atoms += len(value.get("locations", ())) or 1
    return atoms


def proof_mode(stages):
    if "unproven_recolor_audit" in stages:
        return "UNPROVEN"
    if "value_equality_recolor" in stages:
        return "value-eq"
    return "strict"


def _word_counts(unit, wanted):
    """{function: (differing words, insns)} for one unit, raw body vs target."""
    target_path = os.path.join(OBJ, unit + ".o")
    our_path = _raw_object(os.path.join(SRC, unit + ".o"))
    if not (os.path.exists(target_path) and os.path.exists(our_path)):
        return {}
    try:
        our_data, our_sections = _load(our_path)
        target_data, target_sections = _load(target_path)
        ours = _functions(our_data, our_sections)
        targets = _functions(target_data, target_sections)
    except Exception:  # noqa: BLE001
        return {}
    out = {}
    for name in wanted:
        our_sym, target_sym = ours.get(name), targets.get(name)
        if our_sym is None or target_sym is None:
            continue
        our_text = our_sections[our_sym.section_index]
        target_text = target_sections[target_sym.section_index]
        our_blob = bytes(our_data[our_text.offset + our_sym.value:]
                         [:our_sym.size])
        target_blob = bytes(target_data[target_text.offset + target_sym.value:]
                            [:target_sym.size])
        insns = len(our_blob) // 4
        if len(our_blob) != len(target_blob):
            out[name] = (None, insns)
            continue
        out[name] = (sum(1 for off in range(0, len(our_blob), 4)
                         if wf._u32(our_blob, off) != wf._u32(target_blob,
                                                              off)), insns)
    return out


def rule_rows(unit_filter=None):
    """One row per shipped rule, with sizes and declared proof mode."""
    config = json.load(open(WEBFRANK_JSON))
    rows = []
    for unit, rules in sorted(config["units"].items()):
        unit = unit.replace("\\", "/")
        if unit_filter and unit_filter not in unit:
            continue
        measured = _word_counts(unit, [rule["function"] for rule in rules])
        for rule in rules:
            name = rule["function"]
            stages = [key for key in rule if key not in STAGE_KEYS]
            words, insns = measured.get(name, (None, None))
            audit = rule.get("audit") or {}
            rows.append({
                "unit": unit,
                "function": name,
                "words": words,
                "insns": insns if insns is not None
                else audit.get("instructions"),
                "atoms": rule_size(rule),
                "stages": stages,
                "stage_count": len(stages),
                "proof": proof_mode(stages),
                "classification": audit.get("classification"),
            })
    return rows


# The JSON export declares its row grain and field types.
OUT_SCHEMA = {
    "rows_key": "rules",
    "row_is": "ONE WEBFRANK RULE (a pinned function)",
    "join_key": ["unit", "function"],
    "fields": {
        "unit": "str — repo-relative unit path, no src/ and no extension",
        "function": "str — the pinned symbol, as webfrank.json spells it",
        "words": "int|null — raw differing words (wf_word_diff)",
        "insns": "int|null — instruction count of the function",
        "atoms": "int — size of the rule body",
        "stages": "list[str] — the rule's postprocessor stage keys",
        "stage_count": "int — len(stages)",
        "proof": "str — strict | value-eq | UNPROVEN",
        "classification": "str|null — webfrank_audit's residual class",
    },
}


def _sort_key(row):
    return (row["words"] if row["words"] is not None else 10 ** 6,
            row["insns"] or 10 ** 6, row["unit"], row["function"])


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--unit")
    parser.add_argument("--top", type=int, default=40)
    parser.add_argument("--proof", choices=("strict", "value-eq", "UNPROVEN"))
    parser.add_argument("--out")
    args = parser.parse_args(argv)

    rows = rule_rows(args.unit)
    shown = [row for row in rows
             if not args.proof or row["proof"] == args.proof]
    shown.sort(key=_sort_key)

    print("SHIPPED RULES: %d over %d units (%d shown)"
          % (len(rows), len({row["unit"] for row in rows}), len(shown)))
    seen, twice = set(), []
    for row in rows:
        key = (row["unit"], row["function"])
        (twice.append(key) if key in seen else seen.add(key))
    print("  distinct FUNCTIONS: %d%s -- EQUIVALENT credit is counted per"
          " function, not per rule" % (
              len(seen),
              (" (%s carry two rules each)"
               % ", ".join(name for _unit, name in twice)) if twice else ""))
    by_proof = {}
    for row in rows:
        by_proof[row["proof"]] = by_proof.get(row["proof"], 0) + 1
    print("  proof modes: %s" % ", ".join(
        "%s %d" % item for item in sorted(by_proof.items())))
    total_words = [row["words"] for row in rows if row["words"] is not None]
    if total_words:
        print("  raw differing words: total %d, median %d, max %d"
              % (sum(total_words), sorted(total_words)[len(total_words) // 2],
                 max(total_words)))
    else:
        print("  raw differing words: unavailable (no comparable objects)")
    print()
    print("%-26s %-30s %5s %5s %5s %2s %-9s"
          % ("UNIT", "FUNCTION", "WORDS", "INSNS", "ATOMS", "ST", "PROOF"))
    for row in shown[:args.top]:
        print("%-26s %-30s %5s %5s %5d %2d %-9s"
              % (row["unit"], row["function"],
                 "-" if row["words"] is None else row["words"],
                 row["insns"] if row["insns"] is not None else "-",
                 row["atoms"], row["stage_count"], row["proof"]))
    if len(shown) > args.top:
        print("  ... %d more (--top)" % (len(shown) - args.top))
    if args.out:
        os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
        with open(args.out, "w") as handle:
            json.dump({"schema": OUT_SCHEMA, "rules": rows}, handle,
                      indent=2, sort_keys=True)
        print("wrote %s" % args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
