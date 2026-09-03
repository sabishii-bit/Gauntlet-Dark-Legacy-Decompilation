"""WC lane (run 44): retirement sweep over every unproven_recolor_audit rule.

For each rule that still carries the audit escape, this replays the REAL
apply_patch, captures the exact images the register stage produces (by
wrapping verify_consistent_recolor in this script -- webfrank is not
modified), and then attempts retirement over those images with the full
current machinery, in four configurations:

    A  value-equality, narrow relation
    B  value-equality + the constant-equality closure
    C  A, with every equivalent_copy_form edit the shipped guard accepts
       offered first (form sites detected by DECODED COPY FORM)
    D  C + the closure

Each configuration grows its declaration lists from the guard's own
"undeclared ..." refusals, exactly as a rule author would, and stops at
PROVED or at a refusal declarations cannot fix.  The verbatim refusal is
printed for every configuration that stands, which is the `verifiers_run` /
`windows_tried` evidence a refusal record owes.

    python tools/gdl/composed_census/wc_unproven_sweep.py [--only FUNCTION]
"""
import argparse
import json
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf                                          # noqa: E402
from cn_analyze import our_object, target_object                # noqa: E402
from ce_const_closure_census import form_candidates             # noqa: E402

SUB = re.compile(r"\+0x([0-9a-f]+) ([gf])(\d+)->[gf](\d+)")
EXC = re.compile(
    r"\+0x([0-9a-f]+) \(([gf])(\d+),[gf](\d+)\)<->\([gf](\d+),[gf](\d+)\)")

REAL_STRICT = wf.verify_consistent_recolor
CAPTURED = []


def strict_recorder(current, target, **kwargs):
    CAPTURED.append((current, target, dict(kwargs)))
    return REAL_STRICT(current, target, **kwargs)


def derive_forms(pre, target, common):
    accepted = []
    for offset in range(0, len(pre), 4):
        if wf._u32(pre, offset) == wf._u32(target, offset):
            continue
        for edit in form_candidates(pre, target, offset):
            try:
                wf.equivalent_copy_form(pre, target, [edit], **common)
            except ValueError:
                continue
            accepted.append(edit)
            break
    return accepted


def attempt(pre, post, kwargs, closure):
    """Grow declarations from the guard's refusals; return (verdict, detail)."""
    subs, exchanges, seen, last = [], [], set(), ""
    for _round in range(60):
        arguments = dict(kwargs)
        arguments["substitutions"] = list(subs)
        arguments["compare_exchanges"] = list(exchanges)
        arguments["constant_equality"] = closure
        try:
            wf.verify_value_equality_recolor(pre, post, **arguments)
        except ValueError as failure:
            last = str(failure)
            grew = False
            for at, bank, ours, tgt in SUB.findall(last):
                key = ("s", at, bank, ours, tgt)
                if key in seen:
                    continue
                seen.add(key)
                subs.append({"at": "0x" + at, "bank": bank,
                             "ours": int(ours), "target": int(tgt)})
                grew = True
            for at, bank, o1, o2, t1, t2 in EXC.findall(last):
                key = ("e", at, bank, o1, o2, t1, t2)
                if key in seen:
                    continue
                seen.add(key)
                exchanges.append({"at": "0x" + at, "bank": bank,
                                  "ours": [int(o1), int(o2)],
                                  "target": [int(t1), int(t2)]})
                grew = True
            if not grew:
                return "REFUSED", last
            continue
        return "PROVED", {"substitutions": subs, "compare_exchanges": exchanges}
    return "REFUSED", f"did not converge; last: {last}"


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--only", default=None)
    parser.add_argument("--out", default=os.path.join(
        ROOT, "build", "GUNE5D", "wc_unproven_sweep.json"))
    arguments = parser.parse_args()

    config = json.load(open(os.path.join(ROOT, "config", "GUNE5D",
                                         "webfrank.json"), encoding="utf-8"))
    units = config.get("units", config)
    wf.verify_consistent_recolor = strict_recorder
    report = []
    for unit, rules in units.items():
        for rule in rules:
            if not rule.get("unproven_recolor_audit"):
                continue
            name = rule["function"]
            if arguments.only and arguments.only != name:
                continue
            ours_path, kind = our_object(unit)
            odata = bytearray(open(ours_path, "rb").read())
            tdata = bytes(open(target_object(unit), "rb").read())
            CAPTURED.clear()
            wf.apply_patch(odata, json.loads(json.dumps(rule)), tdata)
            if len(CAPTURED) != 1:
                print(f"!! {unit}::{name}: captured {len(CAPTURED)} strict "
                      f"proofs, expected 1")
                continue
            pre, post, kwargs = CAPTURED[0]
            stages = [k for k in ("instruction_permutation",
                                  "equivalent_copy_form",
                                  "post_recolor_permutation")
                      if rule.get(k)]
            print(f"=== {unit}::{name}  ({kind}, {len(pre)//4} insns, "
                  f"stages: {'+'.join(stages) or 'recolor only'})")
            try:
                REAL_STRICT(pre, post, **kwargs)
                print("  strict recolor now PROVES -- the audit escape is "
                      "already unnecessary")
                report.append({"unit": unit, "function": name,
                               "verdict": "STRICT-PROVES"})
                continue
            except ValueError as failure:
                print(f"  strict refusal (the escape's reason): {failure}")

            common = dict(
                relocated_offsets=kwargs.get("relocated_offsets", set()),
                target_relocated_offsets=set(),
                jumptable_offsets=kwargs.get("jumptable_targets", ()),
                call_targets=kwargs.get("call_targets"),
                relocation_types={})
            try:
                forms = derive_forms(pre, post, common)
            except Exception as failure:            # noqa: BLE001
                forms = []
                print(f"  form derivation unavailable: {failure}")
            form_pre = pre
            if forms:
                form_pre, _n = wf.equivalent_copy_form(pre, post, forms,
                                                       **common)

            ve_kwargs = {
                "jumptable_targets": kwargs.get("jumptable_targets", ()),
                "relocated_offsets": kwargs.get("relocated_offsets", ()),
                "target_relocated_offsets": (),
                "call_targets": kwargs.get("call_targets"),
            }
            row = {"unit": unit, "function": name, "insns": len(pre) // 4,
                   "stages": stages, "form_sites": len(forms)}
            for label, image, closure in (
                    ("A narrow           ", pre, False),
                    ("B closure          ", pre, True),
                    (f"C narrow +{len(forms)} form(s)", form_pre, False),
                    (f"D closure+{len(forms)} form(s)", form_pre, True)):
                verdict, detail = attempt(image, post, ve_kwargs, closure)
                key = label.strip().split()[0]
                if verdict == "PROVED":
                    print(f"  {label}: PROVED with "
                          f"{len(detail['substitutions'])} substitution(s), "
                          f"{len(detail['compare_exchanges'])} exchange(s)")
                    row[key] = {"verdict": verdict, "declarations": detail}
                else:
                    print(f"  {label}: REFUSED -- {detail}")
                    row[key] = {"verdict": verdict, "why": detail}
            report.append(row)
    os.makedirs(os.path.dirname(arguments.out), exist_ok=True)
    with open(arguments.out, "w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=2)
    print(f"\nwrote {arguments.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
