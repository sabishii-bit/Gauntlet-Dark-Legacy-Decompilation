"""WR lane (run 42): derive a value_equality_recolor rule by iterated refusal.

Runs the REAL webfrank.apply_patch with copy_register_fields +
value_equality_recolor and grows the declaration lists from the guard's own
"undeclared ..." refusals until the patch reaches the target byte-for-byte or
the guard refuses for a reason declarations cannot fix.

    python tools/gdl/composed_census/wr_ve_derive.py <unit> <function> [--out PATH]

Read-only: it writes nothing but --out (default under build/GUNE5D/).
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf                                      # noqa: E402
from cn_analyze import our_object, target_object            # noqa: E402

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
    return odata, tdata, osym, ostart, body, target, kind


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("unit")
    parser.add_argument("function")
    parser.add_argument("--out", default=None)
    parser.add_argument("--extra", default=None,
                        help="JSON file of extra rule keys merged in "
                             "(instruction_permutation, "
                             "equivalent_copy_form, memory_disambiguation)")
    arguments = parser.parse_args()
    extra = {}
    if arguments.extra:
        with open(arguments.extra, encoding="utf-8") as handle:
            extra = json.load(handle)

    odata, tdata, osym, ostart, body, target, kind = bodies(
        arguments.unit, arguments.function)
    print(f"source: {kind}; {len(body)//4} insns ours, "
          f"{len(target)//4} target")
    if len(body) != len(target):
        raise SystemExit("COUNT-ASYMMETRIC: outside every postprocessor class")

    subs = []
    exchanges = []
    seen = set()
    last = ""
    for round_index in range(40):
        rule = {
            "function": arguments.function,
            "before_sha256": wf._sha256(body),
            "after_sha256": wf._sha256(target),
            "copy_register_fields": True,
            "value_equality_recolor": {
                "audit": "WR run 42 derivation",
                "substitutions": list(subs),
                "compare_exchanges": list(exchanges),
            },
        }
        rule.update(extra)
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
            ROOT, "build", "GUNE5D",
            f"wr_rule_{arguments.function}.json")
        with open(out, "w", encoding="utf-8") as handle:
            json.dump(rule, handle, indent=2)
        print(f"  wrote {out}")
        return 0 if verdict == "BYTE-EQUAL" else 1
    print(f"did not converge; last refusal: {last}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
