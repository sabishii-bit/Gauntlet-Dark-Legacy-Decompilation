"""CH lane run-26: re-derive the combined form+recolor population.

claim.law.WF_inverse-copy-form-is-served-and-the-payoff-inversion-recurs-one-
level-down.20260901.v1 measured, positionally, over the equal-size paired
population: other 9057, regfield 7859, inv_rc 47, fwd_rc 44, fwd 5, inv 2, and
states the 47/44 are FLOORS because a site only becomes a copy/li pair AFTER
the words move.  This script re-derives those numbers rather than quoting them,
and keeps the per-function breakdown that the closability roster needs.

THE AXIS IS ARROW x DESTINATION-EQUALITY, JOINTLY:
  arrow  fwd : ours `li rD,K`  , target a register copy   -- served today
         inv : ours a copy     , target `li rD,K`         -- served today (WF run-25)
  dest   ==  : ours[1] == theirs[1]  -- the form stage alone reproduces the word
         !=  : destinations differ   -- needs form AND renaming together,
                                        i.e. the UNBUILT combined stage
so fwd/inv are the pure-form classes and fwd_rc/inv_rc are the combined-stage
population.  `regfield` is copy_register_fields's job (served).  `other` is
everything no existing or planned stage can touch.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "tools", "gdl"))
sys.path.insert(0, os.path.join(HERE, "..", "tools", "gdl", "composed_census"))
import webfrank as wf  # noqa: E402
import cn_census as census  # noqa: E402

# Served today by shipped webfrank stages.
SERVED = {"regfield", "fwd", "inv"}
# Served only by the combined form+recolor stage WF is building this run.
COMBINED = {"fwd_rc", "inv_rc"}


def classify(ow, tw):
    """Classify one differing word pair by arrow x destination-equality."""
    try:
        if not ((ow ^ tw) & ~wf.register_slot_mask(ow)):
            return "regfield"
    except ValueError:
        return "other"
    ours, theirs = wf.decode_copy_form(ow), wf.decode_copy_form(tw)
    if ours is None or theirs is None:
        return "other"
    okind, odst = ours[0], ours[1]
    tkind, tdst = theirs[0], theirs[1]
    if okind == "li" and tkind == "copy":
        # r0 source is refused outright: the encoding asymmetry the class rests on.
        if theirs[2] == 0:
            return "other"
        return "fwd" if odst == tdst else "fwd_rc"
    if okind == "copy" and tkind == "li":
        if ours[2] == 0:
            return "other"
        return "inv" if odst == tdst else "inv_rc"
    return "other"


def walk():
    """Yield (unit, name, insns, {class: [offsets]}) for every equal-size pair."""
    for unit in census.units():
        op, is_raw = census.our_path(unit)
        if not op:
            continue
        try:
            odata = bytearray(open(op, "rb").read())
            tdata = bytearray(open(os.path.join(census.OBJ, unit + ".o"), "rb").read())
            osec, tsec = wf._sections(odata), wf._sections(tdata)
        except Exception:
            continue
        tmap = {s.name: s for s in census.functions(tdata, tsec)}
        for s in census.functions(odata, osec):
            t = tmap.get(s.name)
            if t is None or t.size != s.size:      # screen 1: equal size only
                continue
            try:
                ot, tt = osec[s.section_index], tsec[t.section_index]
                ours = bytes(odata[ot.offset + s.value:ot.offset + s.value + s.size])
                tgt = bytes(tdata[tt.offset + t.value:tt.offset + t.value + t.size])
            except Exception:
                continue
            buckets = {}
            for off in range(0, len(ours), 4):
                a, b = wf._u32(ours, off), wf._u32(tgt, off)
                if a == b:
                    continue
                buckets.setdefault(classify(a, b), []).append(off)
            yield unit, s.name, s.size // 4, is_raw, buckets


def main():
    totals = {}
    paired = 0
    rows = []
    for unit, name, insns, is_raw, buckets in walk():
        paired += 1
        for k, v in buckets.items():
            totals[k] = totals.get(k, 0) + len(v)
        if not buckets:
            continue
        rows.append({
            "unit": unit, "function": name, "insns": insns, "raw": is_raw,
            "counts": {k: len(v) for k, v in buckets.items()},
            "offsets": {k: [hex(o) for o in v] for k, v in buckets.items()},
        })

    print(f"EQUAL-SIZE PAIRED FUNCTIONS SCANNED: {paired}")
    print(f"DIFFERING FUNCTIONS: {len(rows)}\n")
    print("IMAGE-WIDE WORD CLASSIFICATION (positional):")
    for k in ("other", "regfield", "inv_rc", "fwd_rc", "fwd", "inv"):
        print(f"  {k:9} {totals.get(k, 0)}")
    comb = totals.get("fwd_rc", 0) + totals.get("inv_rc", 0)
    pure = totals.get("fwd", 0) + totals.get("inv", 0)
    print(f"\n  COMBINED-STAGE POPULATION (fwd_rc + inv_rc) = {comb}")
    print(f"  PURE-FORM POPULATION     (fwd + inv)        = {pure}")
    if pure:
        print(f"  ratio = {comb / pure:.1f}x")

    # Functions carrying at least one combined-stage site.
    carriers = [r for r in rows if COMBINED & set(r["counts"])]
    print(f"\nFUNCTIONS CARRYING >=1 COMBINED-STAGE SITE: {len(carriers)}")
    print(f"{'unit':32} {'function':34} {'ins':>5} {'oth':>4} {'reg':>4} "
          f"{'f_rc':>4} {'i_rc':>4} {'fwd':>3} {'inv':>3}")
    for r in sorted(carriers, key=lambda r: (r["counts"].get("other", 0), r["insns"])):
        c = r["counts"]
        print(f"{r['unit']:32} {r['function']:34} {r['insns']:5} "
              f"{c.get('other',0):4} {c.get('regfield',0):4} "
              f"{c.get('fwd_rc',0):4} {c.get('inv_rc',0):4} "
              f"{c.get('fwd',0):3} {c.get('inv',0):3}")

    out = os.path.join(HERE, "ch_census26.json")
    with open(out, "w") as fh:
        json.dump({"paired": paired, "totals": totals, "rows": rows}, fh, indent=1)
    print(f"\nwrote {out}")


if __name__ == "__main__":
    main()
