"""al_control.py -- matched controls + per-instance D15 for the P roster."""
import os
import sys
import collections

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


def load():
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
    tgt, ours, exact = {}, {}, set()
    for unit, tp, op in units:
        tp_, op_ = fndiff.parse(Path(tp)), fndiff.parse(Path(op))
        for name in op_:
            if name in tp_ and tp_[name] == op_[name]:
                exact.add((unit, name))
        for name, ins in mod.fns_of(tp):
            tgt[(unit, name)] = (mod.analyse(ins), ins)
        for name, ins in mod.fns_of(op):
            ours[(unit, name)] = (mod.analyse(ins), ins)
    return tgt, ours, exact


def main():
    tgt, ours, exact = load()
    common = sorted(k for k in ours if k in tgt)

    print("=" * 76)
    print("D15 CONDITIONED ON SHADOW -- gap only for sites whose @ha lis is")
    print("AFTER the frame setup (removes the split-across-prologue confound)")
    for label, keys in (("ALL", common), ("BYTE-EXACT", sorted(exact & set(common)))):
        for side, store in (("TARGET", tgt), ("OURS", ours)):
            for shape in ("A", "B"):
                g = [s["idx"] - s["lis"] for k in keys
                     for s in store[k][0]["sites"]
                     if s["shape"] == shape and s["framed"]
                     and s["lis"] is not None and not s["lis_shadow"]]
                if not g:
                    continue
                adj = sum(1 for x in g if x == 1)
                print(f"  {label:11} {side:6} shape {shape}  n={len(g):4} "
                      f"adjacent={adj:4} ({100.0*adj/len(g):5.1f}%) "
                      f"mean={sum(g)/len(g):5.2f}")

    # P roster, per instance: pair the target's A site with our B site
    print()
    print("=" * 76)
    print("PER-INSTANCE D15 ON THE P ROSTER: target shape-A gap vs ours shape-B")
    def bcount(a, sh=("B", "V")):
        return sum(1 for s in a["sites"] if s["shape"] in sh)
    P = [k for k in common if bcount(ours[k][0]) > bcount(tgt[k][0])]
    win = lose = tie = nopair = 0
    rows = []
    for k in P:
        osites = [s for s in ours[k][0]["sites"] if s["shape"] in ("B", "V")]
        for os_ in osites:
            ts = [s for s in tgt[k][0]["sites"]
                  if s["sym"] == os_["sym"] and s["shape"] == "A"]
            if not ts:
                # symbol names can differ between splits; fall back on home reg
                ts = [s for s in tgt[k][0]["sites"]
                      if s["home"] == os_["home"] and s["shape"] == "A"]
            if not ts:
                nopair += 1
                rows.append((k, os_, None))
                continue
            t = ts[0]
            rows.append((k, os_, t))
            tg = t["idx"] - t["lis"]
            og = os_["idx"] - os_["lis"]
            if tg > og:
                win += 1
            elif tg < og:
                lose += 1
            else:
                tie += 1
    print(f"  paired instances: {win+lose+tie}   unpaired: {nopair}")
    print(f"  target gap  >  ours gap : {win}   (D15 predicts the divergence)")
    print(f"  target gap  <  ours gap : {lose}")
    print(f"  target gap  == ours gap : {tie}")
    print()
    print(f"  {'unit::fn':52} {'sym':22} {'Tgap':>5} {'Ogap':>5} {'Thome':>6} "
          f"{'Ohome':>6} {'Tlis_sh':>8} {'Olis_sh':>8}")
    for k, o, t in rows:
        nm = f"{k[0]}::{k[1]}"
        if t is None:
            print(f"  {nm[:52]:52} {str(o['sym'])[:22]:22} {'-':>5} "
                  f"{o['idx']-o['lis']:>5} {'-':>6} {str(o['home']):>6}")
            continue
        print(f"  {nm[:52]:52} {str(o['sym'])[:22]:22} "
              f"{t['idx']-t['lis']:>5} {o['idx']-o['lis']:>5} "
              f"{str(t['home']):>6} {str(o['home']):>6} "
              f"{str(t['lis_shadow']):>8} {str(o['lis_shadow']):>8}")

    # matched controls: same TU, same symbol, ours shape A in a BYTE-EXACT fn
    print()
    print("=" * 76)
    print("MATCHED CONTROLS -- for each P instance, a BYTE-EXACT function in")
    print("the SAME TU where OUR compiler homes the SAME symbol with shape A")
    found = 0
    for k, o, _t in rows:
        unit = k[0]
        ctl = []
        for k2 in common:
            if k2[0] != unit or k2 == k or k2 not in exact:
                continue
            for s2 in ours[k2][0]["sites"]:
                if s2["sym"] == o["sym"] and s2["shape"] == "A":
                    ctl.append((k2[1], s2))
        if ctl:
            found += 1
            print(f"  {unit}::{k[1]}  sym={o['sym']}  (ours B, home={o['home']},"
                  f" gap={o['idx']-o['lis']})")
            for nm, s2 in ctl[:4]:
                print(f"      CONTROL EXACT {nm:34} shape A home={s2['dst']:4} "
                      f"gap={s2['idx']-s2['lis']} lis_sh={s2['lis_shadow']}")
    print(f"\n  P instances with a same-TU same-symbol EXACT shape-A control: "
          f"{found} of {len(rows)}")

    # cross-TU controls on the same symbol
    print()
    print("=" * 76)
    print("CROSS-TU CONTROLS -- byte-exact shape-A homes of the SAME symbols")
    syms = sorted({o["sym"] for _k, o, _t in rows})
    for sym in syms:
        hits = [(k2[0], k2[1], s2) for k2 in common if k2 in exact
                for s2 in ours[k2][0]["sites"]
                if s2["sym"] == sym and s2["shape"] == "A"]
        print(f"  {sym[:28]:28} exact shape-A homes: {len(hits):3}"
              + ("   e.g. " + ", ".join(f"{h[1]}" for h in hits[:3]) if hits else ""))


main()
