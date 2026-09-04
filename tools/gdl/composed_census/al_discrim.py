"""al_discrim.py -- discriminant battery over the ADDR16_LO home class.

Imports al_addrlo_positive's parser and runs the candidate discriminants that
claim.law.addr16-lo-home-copy-census-no-source-discriminant's twelve did NOT
cover, each reported BOTH WAYS (target vs ours, and member vs non-member).
"""
import os
import sys
import json
import collections

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.getcwd(), "tools", "gdl"))

import importlib.util
spec = importlib.util.spec_from_file_location(
    "alpos", os.path.join(HERE, "al_addrlo_positive.py"))
# the module runs main() at import, so re-implement the loader here instead
import re
import subprocess
from pathlib import Path
import fndiff

src = open(os.path.join(HERE, "al_addrlo_positive.py")).read()
src = src.replace("\nmain()\n", "\n")
mod = type(sys)("alpos")
mod.__dict__["__file__"] = os.path.join(HERE, "al_addrlo_positive.py")
exec(compile(src, "al_addrlo_positive.py", "exec"), mod.__dict__)

TGT = mod.TGT
OURS = mod.OURS
CS = mod.CS


def load():
    units = []
    for root, _d, names in os.walk(OURS):
        if ".postprocess" in root.replace("\\", "/"):
            continue
        for n in sorted(names):
            if not n.endswith(".o"):
                continue
            op = os.path.join(root, n)
            rel = os.path.relpath(op, OURS)
            tp = os.path.join(TGT, rel)
            if os.path.exists(tp):
                units.append((rel.replace("\\", "/")[:-2], tp, op))
    tgt, ours, exact = {}, {}, set()
    for unit, tp, op in units:
        tparse = fndiff.parse(Path(tp))
        oparse = fndiff.parse(Path(op))
        for name in oparse:
            if name in tparse and tparse[name] == oparse[name]:
                exact.add((unit, name))
        for name, ins in mod.fns_of(tp):
            tgt[(unit, name)] = (mod.analyse(ins), ins)
        for name, ins in mod.fns_of(op):
            ours[(unit, name)] = (mod.analyse(ins), ins)
    return tgt, ours, exact


def main():
    tgt, ours, exact = load()
    common = sorted(k for k in ours if k in tgt)

    print("=" * 74)
    print("FINDING A -- IS S STILL ZERO?  CS-only vs generalised home")
    for label, shapes in (("CS-only (the published census's detector)", ("B",)),
                          ("CS + VOLATILE home (generalised)", ("B", "V"))):
        P = S = Q = 0
        srows = []
        for k in common:
            tb = sum(1 for s in tgt[k][0]["sites"] if s["shape"] in shapes)
            ob = sum(1 for s in ours[k][0]["sites"] if s["shape"] in shapes)
            if ob > tb:
                P += 1
            elif ob < tb:
                S += 1
                srows.append((k, tb, ob))
            elif ob:
                Q += 1
        opp = sum(1 for k in common for s in tgt[k][0]["sites"]
                  if s["shape"] in ("A",) + shapes)
        print(f"  {label:44} P={P:3} Q={Q:3} S={S:3}   "
              f"opportunities={opp}")
        for (u, f), tb, ob in srows:
            print(f"      S: {u}::{f}  tB={tb} oB={ob}")

    print()
    print("=" * 74)
    print("FINDING B -- THE S INSTANCE, INSTANCE BY INSTANCE")
    for k in common:
        tb = sum(1 for s in tgt[k][0]["sites"] if s["shape"] in ("B", "V"))
        ob = sum(1 for s in ours[k][0]["sites"] if s["shape"] in ("B", "V"))
        if ob >= tb:
            continue
        print(f"  {k[0]}::{k[1]}")
        for side, store in (("T", tgt), ("O", ours)):
            for s in store[k][0]["sites"]:
                print(f"    {side} shape={s['shape']} sym={str(s['sym'])[:26]:26}"
                      f" dst={s['dst']:4} home={str(s['home']):5} idx={s['idx']:4}"
                      f" lis={str(s['lis']):5} lis_sh={s['lis_shadow']}")

    # ---- discriminant battery -------------------------------------------
    def sites(store, keys, shape):
        for k in keys:
            a, ins = store[k]
            for s in a["sites"]:
                if s["shape"] == shape and s["framed"]:
                    yield k, s, ins

    ex = sorted(exact & set(common))

    print()
    print("=" * 74)
    print("D14 -- @ha TEMP REGISTER IDENTITY (never measured; axis 7 was the")
    print("       HOME register, not the temp).  distribution of `src`.")
    for label, keys in (("ALL", common), ("BYTE-EXACT", ex)):
        for side, store in (("TARGET", tgt), ("OURS", ours)):
            for shape in ("A", "B"):
                c = collections.Counter(s["dst"] if shape == "B" else None
                                        for _k, s, _i in sites(store, keys, shape))
                c2 = collections.Counter()
                for _k, s, ins in sites(store, keys, shape):
                    m = mod.ADDI.match(ins[s["idx"]][0])
                    c2[m.group(2)] += 1
                tot = sum(c2.values())
                top = ", ".join(f"{r}:{n}" for r, n in c2.most_common(6))
                print(f"  {label:10} {side:6} shape {shape}  n={tot:4}  {top}")

    print()
    print("=" * 74)
    print("D15 -- lis->addi GAP (adjacency of the @ha/@lo pair).  axis 11 was")
    print("       the gap BETWEEN formations, not inside one.")
    for label, keys in (("ALL", common), ("BYTE-EXACT", ex)):
        for side, store in (("TARGET", tgt), ("OURS", ours)):
            for shape in ("A", "B"):
                gaps = [s["idx"] - s["lis"] for _k, s, _i in sites(store, keys, shape)
                        if s["lis"] is not None]
                miss = sum(1 for _k, s, _i in sites(store, keys, shape)
                           if s["lis"] is None)
                if not gaps:
                    continue
                adj = sum(1 for g in gaps if g == 1)
                print(f"  {label:10} {side:6} shape {shape}  n={len(gaps):4} "
                      f"adjacent(gap=1)={adj:4} ({100.0*adj/len(gaps):5.1f}%) "
                      f"mean={sum(gaps)/len(gaps):6.2f} max={max(gaps):3} "
                      f"no-lis={miss}")

    print()
    print("=" * 74)
    print("D17 -- @ha TEMP REUSE: how many ADDR16_LO sites in the function")
    print("       share the same @ha temp register?")
    for label, keys in (("ALL", common), ("BYTE-EXACT", ex)):
        for side, store in (("TARGET", tgt), ("OURS", ours)):
            for shape in ("A", "B"):
                vals = []
                for k, s, ins in sites(store, keys, shape):
                    a = store[k][0]
                    m = mod.ADDI.match(ins[s["idx"]][0])
                    temp = m.group(2)
                    n = 0
                    for s2 in a["sites"]:
                        m2 = mod.ADDI.match(store[k][1][s2["idx"]][0])
                        if m2 and m2.group(2) == temp:
                            n += 1
                    vals.append(n)
                if vals:
                    print(f"  {label:10} {side:6} shape {shape}  n={len(vals):4} "
                          f"mean temp-sharing={sum(vals)/len(vals):5.2f} "
                          f"solo={sum(1 for v in vals if v == 1)}")

    print()
    print("=" * 74)
    print("THE POSITIVE CORPUS -- every shape-B/V site our compiler emits")
    print("inside a BYTE-EXACT function (source proven correct).")
    for k in ex:
        for s in ours[k][0]["sites"]:
            if s["shape"] in ("B", "V") and s["framed"]:
                print(f"  {k[0][:28]:28} {k[1][:30]:30} shape={s['shape']} "
                      f"sym={str(s['sym'])[:24]:24} dst={s['dst']:4} "
                      f"home={s['home']:4} lis_sh={str(s['lis_shadow']):5} "
                      f"gap={s['idx']-s['lis'] if s['lis'] is not None else '?'}")


main()
