"""al_shadow.py -- D21: prologue-shadow OCCUPANCY as the discriminant."""
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

FOCUS = [
    ("game/world/items", "LoadItems"), ("game/sound/sounds", "MapMusicStart"),
    ("game/mb/mb_model", "MBOX_BGLoadModelStart"),
    ("game/world/world", "StartWorldLoad"),
    ("game/ui/auxscreen", "init_mapscreen"),
    ("game/audio/dcs", "dcsReadCalls"),
    ("game/enemy/critter", "CritterInitGeo"),
    ("game/enemy/enemy", "fn_80046680"),
]


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
    ex = set(exact) & set(common)

    print("=" * 76)
    print("D21 -- SHADOW OCCUPANCY. For every site whose @ha lis is in the")
    print("prologue shadow, how big is that shadow and where does the addi go?")
    for label, keys in (("ALL", common), ("BYTE-EXACT", sorted(ex))):
        for side, store in (("TARGET", tgt), ("OURS", ours)):
            for shape in ("A", "B"):
                rows = [(store[k][0]["stwu"], s) for k in keys
                        for s in store[k][0]["sites"]
                        if s["shape"] == shape and s["lis_shadow"]]
                if not rows:
                    continue
                sizes = collections.Counter(sz for sz, _s in rows)
                inshadow = sum(1 for _sz, s in rows if s["addi_shadow"])
                print(f"  {label:11} {side:6} shape {shape}  n={len(rows):4} "
                      f"addi-also-in-shadow={inshadow:4}  shadow sizes="
                      f"{dict(sorted(sizes.items()))}")

    print()
    print("=" * 76)
    print("THE 174 POSITIVE CONTROLS: byte-exact OURS shape-A with the lis in")
    print("the shadow and the addi AFTER the frame -- the TARGET's exact form,")
    print("emitted by OUR compiler from PROVEN-CORRECT source.")
    pos = [(k, s) for k in sorted(ex) for s in ours[k][0]["sites"]
           if s["shape"] == "A" and s["lis_shadow"] and not s["addi_shadow"]]
    print(f"  count = {len(pos)}")
    szc = collections.Counter(ours[k][0]["stwu"] for k, _s in pos)
    print(f"  shadow sizes of those functions: {dict(sorted(szc.items()))}")
    for k, s in pos[:14]:
        print(f"    {k[0][:26]:26} {k[1][:28]:28} sym={str(s['sym'])[:20]:20} "
              f"shadow={ours[k][0]['stwu']} lis@{s['lis']} addi@{s['idx']} "
              f"home={s['dst']}")

    print()
    print("=" * 76)
    print("SIDE-BY-SIDE PROLOGUES for the P-shadow subclass")
    for k in FOCUS:
        if k not in tgt or k not in ours:
            print(f"  {k}: NOT PAIRED")
            continue
        print()
        print(f"--- {k[0]}::{k[1]}   T shadow={tgt[k][0]['stwu']} "
              f"O shadow={ours[k][0]['stwu']} ---")
        n = max(tgt[k][0]["stwu"] or 0, ours[k][0]["stwu"] or 0) + 12
        ti = [t for t, _a, _b in tgt[k][1][:n]]
        oi = [t for t, _a, _b in ours[k][1][:n]]
        tr = [rs for _t, _rt, rs in tgt[k][1][:n]]
        orr = [rs for _t, _rt, rs in ours[k][1][:n]]
        for i in range(n):
            a = f"{ti[i]} {'<'+tr[i]+'>' if i < len(tr) and tr[i] else ''}" \
                if i < len(ti) else ""
            b = f"{oi[i]} {'<'+orr[i]+'>' if i < len(orr) and orr[i] else ''}" \
                if i < len(oi) else ""
            mark = "  " if a == b else "**"
            print(f"  {i:3} {mark} {a[:52]:52} | {b[:52]}")


main()
