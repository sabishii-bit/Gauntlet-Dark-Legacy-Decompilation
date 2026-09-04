"""al_twin.py -- find BYTE-EXACT functions with StartWorldLoad's exact shape:
shadow == 3, the @ha lis hoisted into the shadow, TWO OR MORE file-scope
addresses homed to callee-saved registers, all shape A.  Those are the
positive instances of the form the P-shadow subclass fails to reach."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.getcwd(), "tools", "gdl"))
from pathlib import Path
import fndiff

src = open(os.path.join(HERE, "al_addrlo_positive.py")).read().replace(
    "\nmain()\n", "\n")
mod = type(sys)("alpos")
mod.__dict__["__file__"] = os.path.join(HERE, "al_addrlo_positive.py")
exec(compile(src, "al_addrlo_positive.py", "exec"), mod.__dict__)
TGT, OURS = mod.TGT, mod.OURS


def main():
    units = []
    for root, _d, names in os.walk(OURS):
        if ".postprocess" in root.replace("\\", "/"):
            continue
        for n in sorted(names):
            if n.endswith(".o"):
                op = os.path.join(root, n)
                rel = os.path.relpath(op, OURS)
                tp = os.path.join(TGT, rel)
                if os.path.exists(tp):
                    units.append((rel.replace("\\", "/")[:-2], tp, op))
    hits = []
    for unit, tp, op in units:
        tp_, op_ = fndiff.parse(Path(tp)), fndiff.parse(Path(op))
        for name, ins in mod.fns_of(op):
            if name not in tp_ or tp_[name] != op_.get(name):
                continue
            a = mod.analyse(ins)
            sites = [s for s in a["sites"] if s["shape"] == "A"]
            if a["stwu"] != 3 or len(sites) < 2:
                continue
            if not any(s["lis_shadow"] for s in sites):
                continue
            hits.append((unit, name, a, ins))
    print(f"BYTE-EXACT twins of StartWorldLoad's shape: {len(hits)}")
    for unit, name, a, ins in hits:
        print()
        print(f"--- {unit}::{name}  shadow=3  sites="
              f"{[(s['sym'], s['dst'], s['lis'], s['idx']) for s in a['sites'] if s['shape']=='A']}")
        for i in range(min(12, len(ins))):
            t, _rt, rs = ins[i]
            print(f"    {i:3}  {t:34} {'<'+rs+'>' if rs else ''}")


main()
