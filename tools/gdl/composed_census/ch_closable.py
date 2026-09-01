"""CH lane run-26 HALF 1, final step: turn the roster from "carries combined
sites" into "CLOSES WHEN THE COMBINED STAGE LANDS".

Carrying a combined-stage site is necessary, not sufficient.  The run-26
harvest measured why: of 10 otherwise-clean candidates, 9 died at the RECOLOR
stage (4 inconsistent renaming, 5 base-register presence) and 1 at a branch
target.  A roster that does not run the recolor check hands WF a list of
functions that will refuse.

So simulate the stage WF is building, in apply_patch's own order:
  permutation (where the roster derived a legal one)
    -> rewrite every form-class site, EITHER arrow, EITHER destination
       (this is exactly what the combined stage does)
    -> verify_consistent_recolor on the result, with the same jumptable /
       relocation / call-target context apply_patch constructs.

A member that reaches a clean verify is a genuine next-run exact; a member that
refuses is reported with the bar NAMED, so the stage's author is not asked to
close something that a different mechanism blocks.
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

FORMS = {"fwd", "inv", "fwd_rc", "inv_rc"}


def simulate(unit, fn, order=None, window=None):
    op, _k = an.our_object(unit)
    tp = an.target_object(unit)
    _d, _s, _y, ours, orel, ojt = an.load(op, fn)
    _d2, _s2, _y2, tgt, _tr, _tj = an.load(tp, fn)
    if len(ours) != len(tgt):
        return "size mismatch", None
    cur = bytearray(ours)
    reloc_off = dict(orel)
    if order and window:
        lo, hi = window
        region = bytes(cur[lo:hi])
        cur[lo:hi] = b"".join(region[s * 4:s * 4 + 4] for s in order)
        # Relocations ride their atom.  Passing the UN-permuted offsets to
        # verify_consistent_recolor makes a Tier B verdict unsound, so map
        # every in-window relocation through the permutation, keyed by the
        # owning instruction (the SDA21 encoding law).
        dest_by_src = {s: d for d, s in enumerate(order)}
        moved = {}
        for off, ident in orel.items():
            insn = off & ~3
            if lo <= insn < hi:
                src = (insn - lo) // 4
                new_insn = lo + dest_by_src[src] * 4
                moved[new_insn + (off - insn)] = ident
            else:
                moved[off] = ident
        reloc_off = moved
    # the combined stage: rewrite every form-class site to the target word
    nform = 0
    for off in range(0, len(cur), 4):
        a, b = wf._u32(cur, off), wf._u32(tgt, off)
        if a == b:
            continue
        if c26.classify(a, b) in FORMS:
            cur[off:off + 4] = tgt[off:off + 4]
            nform += 1
    try:
        wf.verify_consistent_recolor(
            bytes(cur), tgt,
            jumptable_targets=ojt,
            relocated_offsets=set(reloc_off),
            call_targets={o: n for o, (t, n) in reloc_off.items() if t == 10},
        )
    except ValueError as e:
        return f"RECOLOR REFUSES: {e}", nform
    return "CLEAN", nform


def main():
    roster = json.load(open(os.path.join(HERE, "ch_roster.json")))
    out = []
    for r in roster:
        order = r["legal_order"]
        win = ([int(r["window"][0], 16), int(r["window"][1], 16)]
               if r["window"] else None)
        try:
            verdict, nform = simulate(r["unit"], r["function"], order, win)
        except Exception as e:
            verdict, nform = f"ERROR {type(e).__name__}: {e}", None
        r["recolor"] = verdict
        r["form_sites_rewritten"] = nform
        out.append(r)

    clean = [r for r in out if r["recolor"] == "CLEAN"]
    print("CLOSABILITY ROSTER -- does the function close once the combined "
          "form+recolor stage exists?\n")
    print(f"{'tier':6} {'unit':28} {'function':30} {'ins':>5} {'perm':>5} verdict")
    for r in out:
        p = ("yes" if r["legal_order"] else ("n/a" if r["tier"] == "A" else "NO"))
        v = r["recolor"] if len(r["recolor"]) < 68 else r["recolor"][:65] + "..."
        print(f"{r['tier']:6} {r['unit']:28} {r['function']:30} "
              f"{r['insns']:5} {p:>5} {v}")
    print(f"\nCLEAN (closes when the stage lands): {len(clean)}")
    for r in clean:
        c = r["counts"]
        print(f"  {r['unit']}::{r['function']}  "
              f"fwd_rc={c.get('fwd_rc',0)} inv_rc={c.get('inv_rc',0)} "
              f"regfield={c.get('regfield',0)} perm={r['legal_order']}")
    json.dump(out, open(os.path.join(HERE, "ch_closable.json"), "w"), indent=1)
    print(f"\nwrote {os.path.join(HERE, 'ch_closable.json')}")


if __name__ == "__main__":
    main()
