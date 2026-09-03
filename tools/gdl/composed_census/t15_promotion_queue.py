"""THE PROMOTION QUEUE: every WebFrank rule as promotion debt, smallest first.

IMPORTABLE CORE: rule_rows, rule_size, proof_mode, attempt_index — pure over
config/GUNE5D/webfrank.json, the built objects and the record corpus; no build
and no side effects.

USER DIRECTIVE 2026-09-03 (promotion backlog): every rule-served function is
promotion debt.  A close that lands source-exact DELETES its rule in the same
commit, and an EQUIVALENT -> STRICT promotion is a pure honesty gain.  The
directive had no instrument: `config/GUNE5D/webfrank.json` is 295 KB of
mechanism prose in which the RULES are not enumerable by eye, nothing printed
how big each rule is, and the one number that decides how far a function is
from source-exact -- its RAW differing-word count against the target -- was
per-function only (`wf_word_diff <unit> <fn>`).  This is that instrument.

WHAT EACH COLUMN MEANS AND WHERE IT COMES FROM

  words     RAW differing words between our PRE-WEBFRANK body
            (build/GUNE5D/src/<unit>/.postprocess/body/<unit>.o) and the
            target, counted the way AGENTS.md's dispatch screen requires:
            the raw count decides candidacy, not the `--ops` cluster count.
            `-` when the objects are missing or unequal in size.
  insns     instruction count of our body.
  atoms     declared work the rule does: permuted atoms + form sites +
            declared substitutions/exchanges + explicit register_fields
            edits.  `copy_register_fields: true` declares no atoms (it is a
            blanket field copy), so a big blanket recolor shows atoms=0 and
            is ranked by `words` -- which is the honest ordering, since what
            a source lane must close is words, not declarations.
  stages    how many stages the rule composes, then their names.
  proof     strict     = verify_consistent_recolor alone,
            value-eq   = declares value_equality_recolor,
            UNPROVEN   = rests on the unproven_recolor_audit human escape.
  prov      the source-exhaustion provenance the Mandatory result policy
            requires: `rec` when the rule's mechanism cites a record id,
            `NONE` when it cites none (the "wanton use" failure mode).
  attempts  attempt records anchored to the function, and how many carry a
            literal `probed_form` -- i.e. whether a source lane has actually
            probed it.  `0/0` means nobody has tried the source route.

RANKING is smallest-first on (words, insns): the cheapest rule to retire is
the one whose source residual is smallest.  `--proof UNPROVEN` ranks the
soundness backlog instead, and `--never-probed` ranks the rules no source
lane has ever touched, which is where the directive's cheapest honesty gains
are.

Usage, from the repository root:

    python tools/gdl/composed_census/t15_promotion_queue.py [--top N]
        [--proof strict|value-eq|UNPROVEN] [--never-probed] [--unit U]
        [--out build/GUNE5D/t15_promotion_queue.json]
"""
import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                ".."))
import webfrank as wf  # noqa: E402

SRC = "build/GUNE5D/src"
OBJ = "build/GUNE5D/obj"
WEBFRANK_JSON = "config/GUNE5D/webfrank.json"
RECORDS = "memory_graph/records"
STAGE_KEYS = ("function", "mechanism", "before_sha256", "after_sha256",
              "audit")
RECORD_ID_RE = re.compile(r"\b(?:attempt|claim)\.[A-Za-z0-9_.\-]+v\d+\b")


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


def attempt_index():
    """{function name: (records, records carrying a literal probed_form)}."""
    index = {}
    for root, _dirs, files in os.walk(RECORDS):
        for name in files:
            if not name.endswith(".json"):
                continue
            try:
                with open(os.path.join(root, name)) as handle:
                    record = json.load(handle)
            except Exception:  # noqa: BLE001
                continue
            subject = record.get("function") or record.get("subject") or ""
            if not isinstance(subject, str) or \
                    not subject.startswith("function:"):
                continue
            function = subject.split(":", 1)[1]
            total, probed = index.get(function, (0, 0))
            attributes = record.get("attributes") or {}
            has_form = bool(record.get("probed_form")
                            or attributes.get("probed_form"))
            index[function] = (total + 1, probed + (1 if has_form else 0))
    return index


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
    """One row per shipped rule, with sizes, proof mode and provenance."""
    config = json.load(open(WEBFRANK_JSON))
    attempts = attempt_index()
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
            citations = RECORD_ID_RE.findall(rule.get("mechanism") or "")
            total, probed = attempts.get(name, (0, 0))
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
                "provenance": citations[0] if citations else None,
                "attempt_records": total,
                "probed_form_records": probed,
            })
    return rows


def _sort_key(row):
    return (row["words"] if row["words"] is not None else 10 ** 6,
            row["insns"] or 10 ** 6, row["unit"], row["function"])


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--unit")
    parser.add_argument("--top", type=int, default=40)
    parser.add_argument("--proof", choices=("strict", "value-eq", "UNPROVEN"))
    parser.add_argument("--never-probed", action="store_true",
                        help="only rules whose function has no probed_form "
                             "attempt record")
    parser.add_argument("--out")
    args = parser.parse_args(argv)

    rows = rule_rows(args.unit)
    shown = [row for row in rows
             if (not args.proof or row["proof"] == args.proof)
             and (not args.never_probed or row["probed_form_records"] == 0)]
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
    print("  no provenance citation: %d     no probed_form record: %d"
          "     no attempt record AT ALL: %d"
          % (sum(1 for row in rows if not row["provenance"]),
             sum(1 for row in rows if row["probed_form_records"] == 0),
             sum(1 for row in rows if row["attempt_records"] == 0)))
    total_words = [row["words"] for row in rows if row["words"] is not None]
    print("  raw differing words: total %d, median %d, max %d"
          % (sum(total_words), sorted(total_words)[len(total_words) // 2],
             max(total_words)))
    print()
    print("%-26s %-30s %5s %5s %5s %2s %-9s %-8s %s"
          % ("UNIT", "FUNCTION", "WORDS", "INSNS", "ATOMS", "ST", "PROOF",
             "PROV", "ATTEMPTS(probed)"))
    for row in shown[:args.top]:
        print("%-26s %-30s %5s %5s %5d %2d %-9s %-8s %d(%d)"
              % (row["unit"], row["function"],
                 "-" if row["words"] is None else row["words"],
                 row["insns"] if row["insns"] is not None else "-",
                 row["atoms"], row["stage_count"], row["proof"],
                 "rec" if row["provenance"] else "NONE",
                 row["attempt_records"], row["probed_form_records"]))
    if len(shown) > args.top:
        print("  ... %d more (--top)" % (len(shown) - args.top))
    if args.out:
        os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
        with open(args.out, "w") as handle:
            json.dump({"rules": rows}, handle, indent=2, sort_keys=True)
        print("wrote %s" % args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
