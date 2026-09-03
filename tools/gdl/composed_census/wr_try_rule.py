"""WR lane (run 42): run one hand-written rule body through real apply_patch.

    python tools/gdl/composed_census/wr_try_rule.py <unit> <function> <rule-fragment.json>

The fragment supplies everything except function/before_sha256/after_sha256,
which are filled from the live objects.  Prints BYTE-EQUAL / APPLIED-NOT-EQUAL
/ the guard's verbatim refusal, and on success writes the completed rule.
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf                                      # noqa: E402
from cn_analyze import our_object, target_object            # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("unit")
    parser.add_argument("function")
    parser.add_argument("fragment")
    parser.add_argument("--out", default=None)
    arguments = parser.parse_args()

    ours_path, kind = our_object(arguments.unit)
    odata = bytearray(open(ours_path, "rb").read())
    tdata = bytearray(open(target_object(arguments.unit), "rb").read())
    osec, tsec = wf._sections(odata), wf._sections(tdata)
    osym = wf._find_symbol(odata, osec, arguments.function)
    tsym = wf._find_symbol(tdata, tsec, arguments.function)
    ostart = osec[osym.section_index].offset + osym.value
    tstart = tsec[tsym.section_index].offset + tsym.value
    body = bytes(odata[ostart:ostart + osym.size])
    target = bytes(tdata[tstart:tstart + tsym.size])

    with open(arguments.fragment, encoding="utf-8") as handle:
        fragment = json.load(handle)
    rule = {"function": arguments.function,
            "before_sha256": wf._sha256(body),
            "after_sha256": wf._sha256(target)}
    rule.update(fragment)

    probe = bytearray(odata)
    try:
        before, after, changed = wf.apply_patch(
            probe, json.loads(json.dumps(rule)), bytes(tdata),
            wf.load_symbol_addresses(
                os.path.join(ROOT, "config", "GUNE5D", "symbols.txt")))
    except ValueError as failure:
        print(f"REFUSED ({kind}): {failure}")
        return 1
    final = bytes(probe[ostart:ostart + osym.size])
    verdict = "BYTE-EQUAL" if final == target else "APPLIED-NOT-EQUAL"
    print(f"{verdict} ({kind}); {changed} word(s) changed")
    out = arguments.out or os.path.join(
        ROOT, "build", "GUNE5D", f"wr_rule_{arguments.function}.json")
    with open(out, "w", encoding="utf-8") as handle:
        json.dump(rule, handle, indent=2)
    print(f"  wrote {out}")
    return 0 if verdict == "BYTE-EQUAL" else 1


if __name__ == "__main__":
    raise SystemExit(main())
