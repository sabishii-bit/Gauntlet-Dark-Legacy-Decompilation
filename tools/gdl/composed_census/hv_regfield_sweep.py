"""HV lane run-30: FULL-IMAGE sweep for the REGFIELD-ONLY PERMUTATION class.

THE POPULATION THIS EXISTS FOR (claim.law.HV_a-register-field-only-permutation-
repairs-a-non-bijective-recolor-and-no-census-can-see-it.20260901.v1).

A function whose differing words are ALL pure register-field differences is
"Tier A": copy_register_fields ALONE reproduces the target bytes, so no census
that derives its windows from UNABSORBED words can ever nominate a permutation
inside it -- there are no unabsorbed words to cluster.  Tier A splits in two:

  * TIER-A-VERIFIES: verify_consistent_recolor accepts the positional renaming.
    HB measured 93 image-wide, found 70 verifying, and observed all 70 already
    shipped -- so this half is a HARVESTED-ness check, not a payoff.
  * TIER-A-REFUSED: the recolor is not a renaming AT THIS SCHEDULE.  This is
    the blind population.  The repair is an upstream transposition of two
    interchangeable definitions whose atoms are themselves pure regfield diffs.

Run 28 worked this class over PART of the image (hv_sweep.py excluded six TUs
owned by other lanes that run).  This sweep drops every ownership filter and
covers all 326 units, because the class law's population claim ("93 Tier-A
image-wide") has never actually been remeasured against the whole image at the
current match level -- and discipline 8 says remeasure is the default.

Nothing here simulates a stage it can call: every verdict is
webfrank.apply_patch's own residual count against the extracted retail object.
"""
import json
import os
import sys
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
# Repo root by landmark, so this file runs unchanged from lane scratch AND
# from tools/gdl/composed_census after promotion (discipline 17).
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    parent = os.path.dirname(ROOT)
    if parent == ROOT:
        raise SystemExit("repo root not found above " + HERE)
    ROOT = parent
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))
sys.path.insert(0, HERE)

import webfrank as wf          # noqa: E402
import ha_close as ha          # noqa: E402
import hv_repair as hr         # noqa: E402
import cn_census as census     # noqa: E402

OUT = os.path.join(ROOT, "build", "HV_regfield_roster.json")


def shipped():
    with open(os.path.join(ROOT, "config", "GUNE5D", "webfrank.json")) as fh:
        cfg = json.load(fh)
    return {(u, r["function"]) for u, rules in cfg.get("units", {}).items()
            for r in rules}


def triage():
    """Every equal-size differing unshipped function, split Tier A / Tier B.

    Tier A == zero unabsorbed words == copy_register_fields alone reproduces
    the target bytes.  That is the entry condition for the class; whether the
    recolor VERIFIES is decided later, by the shipped guard, not here.
    """
    have = shipped()
    rows = []
    for unit in census.units():
        our_path, _isbody = census.our_path(unit)
        if not our_path:
            continue
        tgt_path = os.path.join(census.OBJ, unit + ".o")
        try:
            odata = bytearray(open(our_path, "rb").read())
            tdata = bytearray(open(tgt_path, "rb").read())
            osec, tsec = wf._sections(odata), wf._sections(tdata)
            osyms = {s.name: s for s in census.functions(odata, osec)}
            tsyms = census.functions(tdata, tsec)
        except Exception:                                   # noqa: BLE001
            continue
        for tsym in tsyms:
            osym = osyms.get(tsym.name)
            if osym is None or osym.size != tsym.size or not tsym.size:
                continue
            ot, tt = osec[osym.section_index], tsec[tsym.section_index]
            ours = bytes(odata[ot.offset + osym.value:
                               ot.offset + osym.value + osym.size])
            tgt = bytes(tdata[tt.offset + tsym.value:
                              tt.offset + tsym.value + tsym.size])
            if ours == tgt:
                continue
            diff = sum(1 for o in range(0, len(ours), 4)
                       if wf._u32(ours, o) != wf._u32(tgt, o))
            unabs = sum(1 for o in range(0, len(ours), 4)
                        if wf._u32(ours, o) != wf._u32(tgt, o)
                        and not ha.regfield_only(wf._u32(ours, o),
                                                 wf._u32(tgt, o)))
            rows.append({"unit": unit, "fn": tsym.name,
                         "insns": osym.size // 4, "diff": diff,
                         "unabsorbed": unabs,
                         "tier": "A" if not unabs else "B",
                         "shipped": (unit, tsym.name) in have})
    return rows


def prove(rows, limit=None):
    """Tier A only: derive, and on refusal run the regfield-only repair.

    The three outcomes are kept DISTINCT on purpose.  A refusal that no repair
    reaches is a measurement of the guard as much as of the function
    (discipline 14), so its reason string is retained verbatim rather than
    collapsed into a count.
    """
    todo = [r for r in rows if r["tier"] == "A" and not r["shipped"]]
    todo.sort(key=lambda r: (r["diff"], r["insns"]))
    out = []
    for i, row in enumerate(todo[:limit]):
        unit, fn = row["unit"], row["fn"]
        cls, rule, note = "?", None, ""
        try:
            rule, _res, note = ha.build_rule(unit, fn)
            if rule:
                cls = "TIER-A-VERIFIES"
            else:
                rule, note = hr.repair(unit, fn, verbose=False)
                cls = "REPAIRED" if rule else "REFUSED"
        except Exception as exc:                            # noqa: BLE001
            cls, note = "ERROR", f"{type(exc).__name__}: {exc}"
            traceback.print_exc(limit=1)
        print(f"[{i+1}/{len(todo[:limit])}] {cls:15} {unit}::{fn} "
              f"({row['insns']}i, {row['diff']}d) -- {str(note)[:130]}",
              flush=True)
        rec = dict(row)
        rec["class"], rec["note"] = cls, str(note)
        if rule:
            rec["rule"] = rule
        out.append(rec)
    return out


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "triage"
    if mode == "triage":
        rows = triage()
        with open(OUT, "w") as fh:
            json.dump(rows, fh, indent=1)
        ta = [r for r in rows if r["tier"] == "A"]
        print(f"{len(rows)} equal-size differing functions image-wide "
              f"({len(census.units())} units)")
        print(f"  tier A (regfield-only diffs): {len(ta)}  "
              f"[{sum(1 for r in ta if r['shipped'])} shipped, "
              f"{sum(1 for r in ta if not r['shipped'])} UNSHIPPED]")
        print(f"  tier B (has unabsorbed words): "
              f"{sum(1 for r in rows if r['tier'] == 'B')}")
        print(f"roster -> {OUT}")
    else:
        with open(OUT) as fh:
            rows = json.load(fh)
        limit = int(sys.argv[2]) if len(sys.argv) > 2 else None
        res = prove(rows, limit)
        rpath = os.path.join(ROOT, "build", "HV_regfield_proved.json")
        with open(rpath, "w") as fh:
            json.dump(res, fh, indent=1)
        from collections import Counter
        print("\n" + str(dict(Counter(r["class"] for r in res))))
        for r in res:
            if r["class"] in ("TIER-A-VERIFIES", "REPAIRED"):
                print(f"  {r['class']:15} {r['unit']}::{r['fn']}  {r['note']}")
