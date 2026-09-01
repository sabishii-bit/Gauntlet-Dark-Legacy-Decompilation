"""HV run-30 discipline-14 instrumentation of the eight Tier-A REFUSALS.

A guard's refusal is a measurement of the guard, not only of the function.
For each refusal this prints WHAT check fired, on WHICH word, and the decoded
instruction pair -- so the question "would a sound-but-finer check pass?" is
answered from the words rather than from the message text.
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# Repo root by landmark, so this runs unchanged from lane scratch AND from
# tools/gdl/composed_census after promotion (discipline 17).
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    _parent = os.path.dirname(ROOT)
    if _parent == ROOT:
        raise SystemExit("repo root not found above " + HERE)
    ROOT = _parent
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf                                        # noqa: E402
import ha_close as ha                                        # noqa: E402
from cn_analyze import our_object, target_object, load       # noqa: E402

AT = re.compile(r"\(\+0x([0-9a-f]+): ([^)]*(?:\([^)]*\))?[^)]*)\)")
PRIMARY = {32: "lwz", 33: "lwzu", 34: "lbz", 35: "lbzu", 36: "stw",
           37: "stwu", 38: "stb", 39: "stbu", 40: "lhz", 41: "lhzu",
           44: "sth", 45: "sthu", 48: "lfs", 49: "lfsu", 50: "lfd",
           52: "stfs", 53: "stfsu", 54: "stfd", 14: "addi", 15: "addis",
           7: "mulli", 24: "ori", 31: "X-form", 59: "fsingle", 63: "fdouble"}


def decode(w):
    op = w >> 26
    name = PRIMARY.get(op, f"op{op}")
    d, a = (w >> 21) & 31, (w >> 16) & 31
    imm = w & 0xFFFF
    if imm >= 0x8000:
        imm -= 0x10000
    return f"{name:7} d={d:<2} a={a:<2} imm={imm}"


def main():
    proved = json.load(open(os.path.join(ROOT, "build",
                                         "HV_regfield_proved.json")))
    buckets = {}
    for row in proved:
        if row["class"] != "REFUSED":
            continue
        unit, fn = row["unit"], row["fn"]
        m = AT.search(row["note"])
        if not m:
            print(f"{unit}::{fn}: unparsed note")
            continue
        off, check = int(m.group(1), 16), m.group(2)
        kind = ("BASE-PRESENCE" if check.startswith("base register")
                else "RENAMING-CONFLICT")
        op, _k = our_object(unit)
        tp = target_object(unit)
        _a, _b, _c, ours, _r, _j = load(op, fn)
        _d, _e, _f, tgt, _r2, _j2 = load(tp, fn)
        ow, tw = wf._u32(ours, off), wf._u32(tgt, off)
        ob, tb = (ow >> 16) & 31, (tw >> 16) & 31
        r0 = "YES  <- r0 in base slot = LITERAL ZERO, not a register" \
            if 0 in (ob, tb) else "no"
        buckets.setdefault(kind, []).append(fn)
        print(f"--- {unit}::{fn}  +0x{off:x}  [{kind}]")
        print(f"    check : {check}")
        print(f"    ours  : {ow:08x}  {decode(ow)}")
        print(f"    target: {tw:08x}  {decode(tw)}")
        print(f"    regfield-only(lexical): {ha.regfield_only(ow, tw)}"
              f"   base slot a: {ob} vs {tb}   involves r0: {r0}")
    print()
    for k, v in buckets.items():
        print(f"{k}: {len(v)}  {v}")


if __name__ == "__main__":
    main()
