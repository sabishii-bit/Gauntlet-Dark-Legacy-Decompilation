"""al_dest_split.py -- split the ADDR16_LO surplus-copy roster by the copy's
DESTINATION REGISTER CLASS, which is the discriminant
claim.law.pointer-decl-order-flips-addrlo-inplace-vs-fresh-destination.20260901.v1
names and which every prior roster of this class has silently combined.

That law's disposition is OPPOSITE for the two halves:
  (a) destination CALLEE-SAVED  -> source-unreachable, S=0 image-wide, DO NOT PROBE
  (b) destination VOLATILE      -> declaration ORDER is a live lever

al_addrlo_positive.py's P roster counts shapes B and V TOGETHER ("paid-copy
count, CS *and* volatile homes"), so it reports one class where there are two.
Measured in run 53 inside a single TU: game/enemy/enemy::fn_80046680 is shape B
(`addi r0,r3,gPlayers@lo ; mr r26,r0`, r26 callee-saved) and
game/enemy/enemy::move_logic22 is shape V (`addi r3,r3,sMilestones@lo ;
mr r4,r3`, r4 volatile). The decl-order lever took move_logic22 from T227/O228
to T227/O227 with an IDENTICAL multiset; the same class of edit is vetoed on
fn_80046680 by the law.

IMPORTABLE CORE: split_roster

Usage: python tools/gdl/composed_census/al_dest_split.py [--census PATH]
       [--out PATH]   (from the repo root; both default under build/GUNE5D/.
       Run al_addrlo_positive.py first -- it writes the census this reads.)
"""
import os
import sys
import json

sys.path.insert(0, os.path.join(os.getcwd(), "tools", "gdl"))

HERE = os.path.dirname(os.path.abspath(__file__))


def _load_parser():
    src = open(os.path.join(HERE, "al_addrlo_positive.py"),
               encoding="utf-8").read().replace("\nmain()\n", "\n")
    mod = type(sys)("alpos")
    mod.__dict__["__file__"] = os.path.join(HERE, "al_addrlo_positive.py")
    exec(compile(src, "al_addrlo_positive.py", "exec"), mod.__dict__)
    return mod


def _inplace(mod, insns, site):
    """Did this @lo materialisation land in the SAME register that held the
    @ha (`addi rT,rT,SYM@lo`, IN PLACE) or in a fresh one (`addi rD,rT,...`)?
    Returns None when the site's instruction cannot be read as an addi."""
    m = mod.ADDI.match(insns[site["idx"]][0])
    if not m:
        return None
    return m.group(1) == m.group(2)


def split_roster(mod, roster):
    """-> rows with the per-function shape-B / shape-V surplus decomposition."""
    rows = []
    for entry in roster:
        unit, fn = entry["unit"], entry["fn"]
        tp = os.path.join(mod.TGT, unit + ".o")
        op = os.path.join(mod.OURS, unit + ".o")
        if not (os.path.exists(tp) and os.path.exists(op)):
            continue
        t = {n: i for n, i in mod.fns_of(tp)}
        o = {n: i for n, i in mod.fns_of(op)}
        if fn not in t or fn not in o:
            continue
        ta = mod.analyse(t[fn])["sites"]
        oa = mod.analyse(o[fn])["sites"]
        tb = sum(1 for r in ta if r["shape"] == "B")
        ob = sum(1 for r in oa if r["shape"] == "B")
        tv = sum(1 for r in ta if r["shape"] == "V")
        ov = sum(1 for r in oa if r["shape"] == "V")
        surplus_b, surplus_v = ob - tb, ov - tv
        # POLARITY (run 53): among shape-V rows, which SIDE lowers the @lo
        # in place (dst == the @ha temp) decides whether the decl-order lever
        # applies at all. The lever's law describes ours-IN-PLACE /
        # target-FRESH; the mirror does not respond. See the module docstring.
        o_inplace = any(_inplace(mod, o[fn], r) for r in oa
                        if r["shape"] == "V")
        t_fresh = any(_inplace(mod, t[fn], r) is False for r in ta)
        polarity = ("ours-in-place/target-fresh" if (o_inplace and t_fresh)
                    else "mirror-or-other")
        if surplus_v > 0 and surplus_b <= 0:
            cls, disp = "V", "class (b) VOLATILE dest -- DECL-ORDER LEVER LIVE"
        elif surplus_b > 0 and surplus_v <= 0:
            cls, disp = "B", "class (a) callee-saved dest -- source-unreachable"
        elif surplus_b > 0 and surplus_v > 0:
            cls, disp = "MIXED", "both shapes surplus -- split per instance"
        else:
            cls, disp = "OTHER", "surplus not in either shape (re-derive)"
        rows.append(dict(unit=unit, fn=fn, cls=cls, disposition=disp,
                         t_B=tb, o_B=ob, t_V=tv, o_V=ov,
                         surplus_B=surplus_b, surplus_V=surplus_v,
                         polarity=polarity,
                         tn=entry.get("tn"), on=entry.get("on")))
    return rows


def main():
    out = os.path.join("build", "GUNE5D", "al_dest_split.json")
    if "--out" in sys.argv:
        out = sys.argv[sys.argv.index("--out") + 1]
    cpath = os.path.join("build", "GUNE5D", "al_addrlo_positive.json")
    if "--census" in sys.argv:
        cpath = sys.argv[sys.argv.index("--census") + 1]
    if not os.path.exists(cpath):
        sys.exit(f"missing census {cpath} -- run "
                 f"tools/gdl/composed_census/al_addrlo_positive.py first")
    mod = _load_parser()
    census = json.load(open(cpath, encoding="utf-8"))
    rows = split_roster(mod, census["P"])
    order = {"V": 0, "MIXED": 1, "B": 2, "OTHER": 3}
    rows.sort(key=lambda r: (order[r["cls"]], r["unit"], r["fn"]))
    counts = {}
    for r in rows:
        counts[r["cls"]] = counts.get(r["cls"], 0) + 1

    print("=== P roster (ours pays a surplus ADDR16_LO copy) SPLIT BY "
          "DESTINATION REGISTER CLASS")
    print("    the two halves have OPPOSITE dispositions under "
          "claim.law.pointer-decl-order-flips-addrlo-inplace-vs-fresh-destination\n")
    last = None
    for r in rows:
        if r["cls"] != last:
            print(f"--- {r['cls']}: {r['disposition']}  "
                  f"({counts[r['cls']]} function(s))")
            last = r["cls"]
        print(f"  {r['unit']:<28} {r['fn']:<34} "
              f"B {r['t_B']}->{r['o_B']}  V {r['t_V']}->{r['o_V']}  "
              f"T{r['tn']}/O{r['on']}"
              + (f"  [{r['polarity']}]" if r["cls"] == "V" else ""))
    print("\n=== TOTALS")
    for k in ("V", "MIXED", "B", "OTHER"):
        if k in counts:
            print(f"  {k:<6} {counts[k]}")
    print(f"  TOTAL  {len(rows)} of {len(census['P'])} P-roster rows resolved")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    json.dump({"counts": counts, "rows": rows}, open(out, "w",
              encoding="utf-8"), indent=1)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
