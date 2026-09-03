"""WC lane (run 44): derive a value_equality_recolor rule, closure optional.

Same iterated-refusal shape as tools/gdl/composed_census/wr_ve_derive.py, but
it drives the SHIPPED apply_patch with the constant-equality closure opt-in
(`value_equality_recolor.constant_equality`) and can auto-derive the
equivalent_copy_form edits the composition needs, by asking the shipped guard
itself which proof mode it accepts at each copy-versus-constant site
(claim.law.CE_a-copy-versus-constant-site-encodes-as-a-pure-register-field-
difference.20260903.v1: detect form sites by DECODED COPY FORM, never by the
encoding-level regfield label).

    python tools/gdl/composed_census/wc_ve_derive.py <unit> <function> [--closure]
        [--auto-forms] [--extra FRAGMENT.json] [--out PATH]

Read-only: it writes nothing but --out.
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf                                          # noqa: E402
from cn_analyze import our_object, target_object                # noqa: E402
from ce_const_closure_census import form_candidates             # noqa: E402

SUB = re.compile(r"\+0x([0-9a-f]+) ([gf])(\d+)->[gf](\d+)")
EXC = re.compile(
    r"\+0x([0-9a-f]+) \(([gf])(\d+),[gf](\d+)\)<->\([gf](\d+),[gf](\d+)\)")


def bodies(unit, name):
    ours_path, kind = our_object(unit)
    target_path = target_object(unit)
    odata = bytearray(open(ours_path, "rb").read())
    tdata = bytearray(open(target_path, "rb").read())
    osec = wf._sections(odata)
    tsec = wf._sections(tdata)
    osym = wf._find_symbol(odata, osec, name)
    tsym = wf._find_symbol(tdata, tsec, name)
    ostart = osec[osym.section_index].offset + osym.value
    tstart = tsec[tsym.section_index].offset + tsym.value
    body = bytes(odata[ostart:ostart + osym.size])
    target = bytes(tdata[tstart:tstart + tsym.size])
    return (odata, tdata, osec, tsec, osym, tsym, ostart, body, target, kind)


def auto_forms(odata, tdata, osec, tsec, osym, tsym, body, target):
    relocations = wf._function_text_relocations(
        odata, osec, osym.section_index, osym.value, osym.value + osym.size)
    target_relocations = wf._function_text_relocations(
        tdata, tsec, tsym.section_index, tsym.value, tsym.value + tsym.size)
    jumptable = wf._jumptable_targets(
        odata, osec, osym.section_index, osym.value, osym.value + osym.size)
    common = dict(
        relocated_offsets=set(relocations),
        target_relocated_offsets=set(target_relocations),
        jumptable_offsets=jumptable,
        call_targets={o: n for o, (k, n) in relocations.items() if k == 10},
        relocation_types={o // 4: k for o, (k, _n) in relocations.items()})
    accepted = []
    for offset in range(0, len(body), 4):
        if wf._u32(body, offset) == wf._u32(target, offset):
            continue
        for edit in form_candidates(body, target, offset):
            try:
                wf.equivalent_copy_form(body, target, [edit], **common)
            except ValueError:
                continue
            accepted.append(edit)
            break
    return accepted


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("unit")
    parser.add_argument("function")
    parser.add_argument("--closure", action="store_true")
    parser.add_argument("--auto-forms", action="store_true")
    parser.add_argument("--extra", default=None)
    parser.add_argument("--audit", default="WC run 44 derivation")
    parser.add_argument("--out", default=None)
    arguments = parser.parse_args()

    extra = {}
    if arguments.extra:
        with open(arguments.extra, encoding="utf-8") as handle:
            extra = json.load(handle)

    (odata, tdata, osec, tsec, osym, tsym, ostart, body, target,
     kind) = bodies(arguments.unit, arguments.function)
    print(f"{arguments.unit}::{arguments.function}  source: {kind}; "
          f"{len(body)//4} insns ours, {len(target)//4} target")
    if len(body) != len(target):
        raise SystemExit("COUNT-ASYMMETRIC: outside every postprocessor class")

    if arguments.auto_forms and "equivalent_copy_form" not in extra:
        forms = auto_forms(odata, tdata, osec, tsec, osym, tsym, body, target)
        if forms:
            extra["equivalent_copy_form"] = forms
            print(f"  auto-derived {len(forms)} equivalent_copy_form edit(s): "
                  + ", ".join(f"+0x{e['at']:x}:{e['proof']}" for e in forms))

    subs, exchanges, seen, last = [], [], set(), ""
    for round_index in range(60):
        value_equality = {
            "audit": arguments.audit,
            "substitutions": list(subs),
            "compare_exchanges": list(exchanges),
        }
        if arguments.closure:
            value_equality["constant_equality"] = True
        rule = {
            "function": arguments.function,
            "before_sha256": wf._sha256(body),
            "after_sha256": wf._sha256(target),
            "copy_register_fields": True,
            "value_equality_recolor": value_equality,
        }
        rule.update(extra)
        rule["value_equality_recolor"] = value_equality
        probe = bytearray(odata)
        try:
            wf.apply_patch(probe, json.loads(json.dumps(rule)), bytes(tdata))
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
                print(f"REFUSED (round {round_index}): {last}")
                print(f"  declarations so far: {len(subs)} substitution(s), "
                      f"{len(exchanges)} exchange(s)")
                return 1
            continue
        final = bytes(probe[ostart:ostart + osym.size])
        verdict = "BYTE-EQUAL" if final == target else "APPLIED-NOT-EQUAL"
        print(f"{verdict} after {round_index} refusal round(s)")
        print(f"  substitutions: {len(subs)}  exchanges: {len(exchanges)}")
        out = arguments.out or os.path.join(
            ROOT, "build", "GUNE5D", f"wc_rule_{arguments.function}.json")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w", encoding="utf-8") as handle:
            json.dump(rule, handle, indent=2)
        print(f"  wrote {out}")
        return 0 if verdict == "BYTE-EQUAL" else 1
    print(f"did not converge; last refusal: {last}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
