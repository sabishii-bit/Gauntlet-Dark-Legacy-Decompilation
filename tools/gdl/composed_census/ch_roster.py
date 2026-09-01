"""CH lane run-26 HALF 1: the CLOSABILITY-ranked roster of the combined
form+recolor population.

The question the roster answers is not "which functions contain a combined
site" (that is the census, 25 functions) but "which functions' ENTIRE residual
is combined-stage sites + recolor + already-served classes" -- i.e. which ones
WF's new stage actually turns into exacts.

METHOD.  Re-apply HV's proven screens 2-3 with ONE change: relax what counts as
already-handled.  cn_census marks a word impure unless it is a pure
register-field difference; here a word is impure only if it is `other`, so
served form sites (fwd/inv) and combined-stage sites (fwd_rc/inv_rc) count as
handled.  Then:

  TIER A  0 impure clusters -> no permutation needed at all.  The combined
          stage closes the function on its own.  Highest closability.
  TIER B  1 impure cluster, control-free -> single-window permute + combined
          stage.  Derivable with the fixed bipartite deriver.
  TIER C  >1 impure cluster -> multi-region; needs a multi-window composition
          that no stage currently offers.  Lowest closability.

Tier B members are then actually DERIVED, with compatibility relaxed to admit
_rc pairs, to separate "a legal order exists" from "no order exists".
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "tools", "gdl"))
sys.path.insert(0, os.path.join(HERE, "..", "tools", "gdl", "composed_census"))
sys.path.insert(0, HERE)
import webfrank as wf  # noqa: E402
import cn_analyze as an  # noqa: E402
import ch_census26 as c26  # noqa: E402

ABSORBABLE = {"regfield", "fwd", "inv", "fwd_rc", "inv_rc"}


def relaxed_compatible(ow, tw):
    """Compatibility for the roster deriver: identical, recolor material, OR a
    form pair in EITHER arrow with EITHER destination agreement.  The last case
    is precisely what the combined stage is being built to serve."""
    if ow == tw:
        return True
    return c26.classify(ow, tw) in ABSORBABLE


def tier(ours, tgt):
    """Return (tier, impure_clusters, window or None, counts)."""
    counts, impure_offs, diffs = {}, [], []
    for off in range(0, len(ours), 4):
        a, b = wf._u32(ours, off), wf._u32(tgt, off)
        if a == b:
            continue
        diffs.append(off)
        k = c26.classify(a, b)
        counts[k] = counts.get(k, 0) + 1
        if k == "other":
            impure_offs.append(off)
    if not diffs:
        return None, 0, None, counts
    if not impure_offs:
        return "A", 0, None, counts
    # cluster the impure words with HV's adjacency rule
    clusters, cur = [], [impure_offs[0]]
    for d in impure_offs[1:]:
        if d - cur[-1] <= 8:
            cur.append(d)
        else:
            clusters.append(cur)
            cur = [d]
    clusters.append(cur)
    if len(clusters) != 1:
        return "C", len(clusters), None, counts
    c = clusters[0]
    lo, hi = max(0, c[0] - 4), min(len(ours), c[-1] + 8)
    for off in range(lo, hi, 4):
        if (wf._is_control_instruction(wf._u32(ours, off))
                or wf._is_control_instruction(wf._u32(tgt, off))):
            return "C-ctrl", 1, (lo, hi), counts
    return "B", 1, (lo, hi), counts


def derive_relaxed(ours, tgt, orel, trel, lo, hi, limit=200000):
    """Sparse bipartite derivation with relaxed (combined-stage) compatibility."""
    n = (hi - lo) // 4
    ow = [wf._u32(ours, lo + i * 4) for i in range(n)]
    tw = [wf._u32(tgt, lo + i * 4) for i in range(n)]
    norel = {(o - 0) & ~3: v for o, v in orel.items()}
    ntrel = {(o - 0) & ~3: v for o, v in trel.items()}
    orl = [norel.get(lo + i * 4) for i in range(n)]
    trl = [ntrel.get(lo + i * 4) for i in range(n)]
    cand = []
    for d in range(n):
        row = [s for s in range(n)
               if relaxed_compatible(ow[s], tw[d]) and orl[s] == trl[d]]
        if not row:
            return []
        cand.append(row)
    slots = sorted(range(n), key=lambda d: len(cand[d]))
    order, used, out = [None] * n, [False] * n, []

    def rec(i):
        if len(out) >= 50:
            return
        if i == len(slots):
            out.append(list(order))
            return
        d = slots[i]
        for s in cand[d]:
            if used[s]:
                continue
            used[s], order[d] = True, s
            rec(i + 1)
            used[s], order[d] = False, None
    rec(0)
    return out


def main():
    census = json.load(open(os.path.join(HERE, "ch_census26.json")))
    shipped = set(json.load(open(os.path.join(HERE, "ch_shipped.json"))))
    carriers = [r for r in census["rows"]
                if {"fwd_rc", "inv_rc"} & set(r["counts"])]
    print(f"COMBINED-SITE CARRIERS: {len(carriers)}\n")
    rows = []
    for r in sorted(carriers, key=lambda r: r["insns"]):
        unit, fn = r["unit"], r["function"]
        try:
            op, _k = an.our_object(unit)
            tp = an.target_object(unit)
            _d, _s, _y, ours, orel, _j = an.load(op, fn)
            _d2, _s2, _y2, tgt, trel, _j2 = an.load(tp, fn)
        except Exception as e:
            print(f"  {unit}::{fn}: load error {e}")
            continue
        t, ncl, win, counts = tier(ours, tgt)
        orders = []
        step0 = None
        if t == "B":
            lo, hi = win
            if (hi - lo) // 4 <= 24:
                for o in derive_relaxed(ours, tgt, orel, trel, lo, hi):
                    try:
                        wf.check_permutation_dependences(ours[lo:hi], o, None)
                        orders.append(o)
                        break
                    except ValueError as e:
                        if step0 is None:
                            step0 = str(e)
            else:
                step0 = f"window {(hi-lo)//4} atoms: not derived this pass"
        rows.append({"unit": unit, "function": fn, "insns": r["insns"],
                     "tier": t, "impure_clusters": ncl,
                     "window": [hex(win[0]), hex(win[1])] if win else None,
                     "counts": counts, "shipped": fn in shipped,
                     "legal_order": orders[0] if orders else None,
                     "step0": step0})
    order_key = {"A": 0, "B": 1, "C-ctrl": 2, "C": 3}
    rows.sort(key=lambda r: (order_key.get(r["tier"], 9),
                             0 if r["legal_order"] else 1,
                             r["counts"].get("other", 0), r["insns"]))
    print(f"{'tier':5} {'unit':30} {'function':32} {'ins':>5} {'oth':>4} "
          f"{'reg':>4} {'f_rc':>4} {'i_rc':>4} {'cl':>3} order?")
    for r in rows:
        c = r["counts"]
        o = ("yes" if r["legal_order"] else
             ("-" if r["tier"] != "B" else "NO"))
        print(f"{r['tier']:5} {r['unit']:30} {r['function']:32} {r['insns']:5} "
              f"{c.get('other',0):4} {c.get('regfield',0):4} "
              f"{c.get('fwd_rc',0):4} {c.get('inv_rc',0):4} "
              f"{r['impure_clusters']:3} {o}")
    for t in ("A", "B", "C-ctrl", "C"):
        n = len([r for r in rows if r["tier"] == t])
        print(f"  TIER {t}: {n}")
    json.dump(rows, open(os.path.join(HERE, "ch_roster.json"), "w"), indent=1)
    print(f"\nwrote {os.path.join(HERE, 'ch_roster.json')}")


if __name__ == "__main__":
    main()
