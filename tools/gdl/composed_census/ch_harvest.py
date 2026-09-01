"""CH lane run-26 HALF 2: harvest the population every prior census pass
structurally excluded.

THE BLIND SPOT.  cn_census.scan_function implements HV screen 2 as
`if len(impure) != 1: return None` -- it keeps functions with EXACTLY ONE
impure cluster.  A function whose every differing word is a pure register-field
difference has ZERO impure clusters and is therefore REJECTED, not accepted.
Every census pass in this family (HV, CN, CH run-25) inherited that filter, so
the pure-recolor and pure-form populations were never screened by any of them.
The run-26 classification finds 92 functions with `other == 0`; 108 rules are
shipped; the difference is unscreened.

These need NO new mechanism: copy_register_fields and equivalent_copy_form are
both shipped and sanctioned.  This script proposes the minimal rule for each
candidate and PROVES it through webfrank.apply_patch to residual 0 -- nothing
is authored on the strength of the classification alone.
"""
import copy
import hashlib
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "tools", "gdl"))
sys.path.insert(0, os.path.join(HERE, "..", "tools", "gdl", "composed_census"))
import webfrank as wf  # noqa: E402
import cn_analyze as an  # noqa: E402

# Proof modes to try for a form site, cheapest/strictest first.
FWD_PROOFS = ["dominating_def", "dominating_def_across_calls"]
INV_PROOFS = ["dominating_def_inverse", "dominating_def_inverse_across_calls"]


def sha(b):
    return hashlib.sha256(b).hexdigest()


def form_sites(ours, tgt):
    """Return (sites, needs_recolor, ok).  sites = [(off, arrow)]."""
    sites, recolor = [], False
    for off in range(0, len(ours), 4):
        a, b = wf._u32(ours, off), wf._u32(tgt, off)
        if a == b:
            continue
        try:
            if not ((a ^ b) & ~wf.register_slot_mask(a)):
                recolor = True
                continue
        except ValueError:
            return None, False, False
        oc, tc = wf.decode_copy_form(a), wf.decode_copy_form(b)
        if oc is None or tc is None:
            return None, False, False
        if oc[0] == "li" and tc[0] == "copy" and tc[2] != 0 and oc[1] == tc[1]:
            sites.append((off, "fwd"))
        elif oc[0] == "copy" and tc[0] == "li" and oc[2] != 0 and oc[1] == tc[1]:
            sites.append((off, "inv"))
        else:
            return None, False, False        # _rc or unserved: not this pass
    return sites, recolor, True


def try_rule(unit, fn, rule):
    """Drive apply_patch on the real object; return residual word count."""
    op, _ = an.our_object(unit)
    tp = an.target_object(unit)
    _d, _s, _y, tgt, _r, _j = an.load(tp, fn)
    data = bytearray(open(op, "rb").read())
    wf.apply_patch(data, copy.deepcopy(rule), open(tp, "rb").read())
    sec = wf._sections(data)
    sym = wf._find_symbol(data, sec, fn)
    text = sec[sym.section_index]
    got = bytes(data[text.offset + sym.value:text.offset + sym.value + sym.size])
    return sum(1 for o in range(0, len(got), 4)
               if wf._u32(got, o) != wf._u32(tgt, o))


def attempt(unit, fn):
    op, kind = an.our_object(unit)
    tp = an.target_object(unit)
    if not (os.path.exists(op) and os.path.exists(tp)):
        return None, "no object"
    _d, _s, _y, ours, _r, _j = an.load(op, fn)
    _d2, _s2, _y2, tgt, _r2, _j2 = an.load(tp, fn)
    if len(ours) != len(tgt):
        return None, "size mismatch"
    sites, recolor, ok = form_sites(ours, tgt)
    if not ok:
        return None, "residual is not pure form+recolor"

    base = {"function": fn, "before_sha256": sha(ours), "after_sha256": sha(tgt)}
    # Try each combination of proof modes; strictest first.
    choices = [FWD_PROOFS if a == "fwd" else INV_PROOFS for _o, a in sites]
    tries = [[]] if not sites else None
    if sites:
        tries = []
        # cartesian, strictest-first ordering
        def rec(i, acc):
            if i == len(choices):
                tries.append(list(acc))
                return
            for p in choices[i]:
                acc.append(p)
                rec(i + 1, acc)
                acc.pop()
        rec(0, [])
    last = None
    for combo in tries:
        rule = dict(base)
        if sites:
            rule["equivalent_copy_form"] = [
                {"at": hex(o), "proof": p} for (o, _a), p in zip(sites, combo)]
        if recolor:
            rule["copy_register_fields"] = True
        try:
            resid = try_rule(unit, fn, rule)
        except ValueError as e:
            last = str(e)
            continue
        if resid == 0:
            return rule, f"CLOSES ({kind}; {len(sites)} form site(s), "
        last = f"residual {resid}"
    return None, last or "no rule"


def main():
    census = json.load(open(os.path.join(HERE, "ch_census26.json")))
    shipped = set(json.load(open(os.path.join(HERE, "ch_shipped.json"))))
    cands = [r for r in census["rows"]
             if r["counts"].get("other", 0) == 0 and r["function"] not in shipped]
    print(f"other==0 functions: "
          f"{len([r for r in census['rows'] if r['counts'].get('other',0)==0])}")
    print(f"already shipped   : "
          f"{len([r for r in census['rows'] if r['counts'].get('other',0)==0 and r['function'] in shipped])}")
    print(f"UNSCREENED CANDIDATES: {len(cands)}\n")
    closed, refused = [], []
    for r in sorted(cands, key=lambda r: r["insns"]):
        unit, fn = r["unit"], r["function"]
        try:
            rule, why = attempt(unit, fn)
        except Exception as e:
            rule, why = None, f"ERROR {type(e).__name__}: {e}"
        tag = "CLOSES " if rule else "refused"
        print(f"  {tag} {unit:30} {fn:34} ins={r['insns']:5} {r['counts']}"
              + ("" if rule else f"  <- {why}"))
        (closed if rule else refused).append((unit, fn, rule, why))
    print(f"\nCLOSED WITH EXISTING CLASSES: {len(closed)}")
    for u, f, _r, _w in closed:
        print(f"  {u}::{f}")
    out = os.path.join(HERE, "ch_harvest.json")
    json.dump({"closed": [{"unit": u, "function": f, "rule": r}
                          for u, f, r, _w in closed],
               "refused": [{"unit": u, "function": f, "why": w}
                           for u, f, _r, w in refused]},
              open(out, "w"), indent=1)
    print(f"\nwrote {out}")


if __name__ == "__main__":
    main()
