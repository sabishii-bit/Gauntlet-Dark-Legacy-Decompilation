"""HV lane run-28: the UNION RE-SWEEP.

Joint coverage of the blind spots of BOTH prior censuses, with the run-27
corrections applied and a THIRD class none of them modelled.

  * cn/CH/HB screen 2 kept at most ONE impure cluster.  The multi-window
    permutation schema shipped in run 27 (claim.law.WF_multi-window-...v1)
    makes several clusters ordinary, so this sweep allows up to three.
  * cn screen 3 widened each cluster by a slot BEFORE testing for control
    ops.  hv_perm tests the UNWIDENED cluster first and widens only on
    failure (claim.law.HB_two-censuses-...v1).
  * ch_census26 dropped the copy->copy arrow, which mis-TIERS rather than
    merely miscounting.  ha_close.classify carries it and is reused, not
    re-derived.
  * NEW CLASS -- TIER A-REFUSED.  HB measured 93 Tier-A functions of which 70
    verify and are all shipped, and stopped there.  The other 23 refuse
    verify_consistent_recolor and were never worked: a Tier-A refusal is
    read as "the recolor is not a renaming", but for a whole population it is
    really "the recolor is not a renaming AT THIS SCHEDULE".  hv_repair
    searches for the register-field-only transposition that repairs it.
    No census can see that window, because it contributes zero unabsorbed
    words.
"""
import json
import os
import sys
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
# Repo root, located by landmark so this file runs unchanged from the
# lane scratch directory AND from tools/gdl/composed_census after
# promotion (discipline 17: a promoted script must actually run).
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
import hv_perm as hv           # noqa: E402
import hv_repair as hr         # noqa: E402
import cn_census as census     # noqa: E402

# TUs owned by other lanes this run (work_claim.harvest-3.20260901.v1).
OWNED = ("game/game/combat", "game/sys/memcard", "game/mb/mb_particle",
         "game/world/gauntworld", "game/game/player", "game/sfx/sfx")


def shipped():
    path = os.path.join(ROOT, "config", "GUNE5D", "webfrank.json")
    with open(path) as fh:
        cfg = json.load(fh)
    out = set()
    for unit, rules in cfg.get("units", {}).items():
        for rule in rules:
            out.add((unit, rule["function"]))
    return out


def triage():
    have = shipped()
    rows = []
    for unit in census.units():
        if unit.startswith(OWNED):
            continue
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
        except Exception:                                  # noqa: BLE001
            continue
        for tsym in tsyms:
            osym = osyms.get(tsym.name)
            if osym is None or osym.size != tsym.size or not tsym.size:
                continue
            if (unit, tsym.name) in have:
                continue
            ot = osec[osym.section_index]
            tt = tsec[tsym.section_index]
            ours = bytes(odata[ot.offset + osym.value:
                               ot.offset + osym.value + osym.size])
            tgt = bytes(tdata[tt.offset + tsym.value:
                              tt.offset + tsym.value + tsym.size])
            if ours == tgt:
                continue
            try:
                rest = hv.unabsorbed(ours, tgt)
            except Exception:                              # noqa: BLE001
                continue
            cl = hv.clusters(rest)
            rows.append({"unit": unit, "fn": tsym.name,
                         "insns": osym.size // 4,
                         "unabsorbed": len(rest),
                         "clusters": len(cl),
                         "tier": "A" if not rest else "B"})
    return rows


def prove(rows, only=None, limit=None):
    out = []
    todo = [r for r in rows if only is None or r["tier"] == only]
    todo.sort(key=lambda r: (r["clusters"], r["unabsorbed"], r["insns"]))
    for i, row in enumerate(todo[:limit]):
        unit, fn = row["unit"], row["fn"]
        try:
            if row["tier"] == "A":
                rule, _res, note = ha.build_rule(unit, fn)
                if not rule:
                    rule, note = hr.repair(unit, fn, verbose=False)
                    note = "REPAIR " + str(note)
            else:
                rule, note = hv.search(unit, fn)
        except Exception as exc:                           # noqa: BLE001
            rule, note = None, f"{type(exc).__name__}: {exc}"
            traceback.print_exc(limit=1)
        verdict = "CLOSES" if rule else "refuses"
        print(f"[{i+1}/{len(todo[:limit])}] {unit}::{fn} "
              f"({row['insns']}i, tier {row['tier']}, {row['unabsorbed']}u/"
              f"{row['clusters']}c): {verdict} -- {str(note)[:150]}",
              flush=True)
        rec = dict(row)
        rec["verdict"] = verdict
        rec["note"] = str(note)
        if rule:
            rec["rule"] = rule
        out.append(rec)
    return out


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "triage"
    rpath = os.path.join(HERE, "hv_roster.json")
    if mode == "triage":
        rows = triage()
        with open(rpath, "w") as fh:
            json.dump(rows, fh, indent=1)
        ta = [r for r in rows if r["tier"] == "A"]
        tb = [r for r in rows if r["tier"] == "B"]
        print(f"{len(rows)} equal-size differing unshipped functions: "
              f"tier A {len(ta)}, tier B {len(tb)}")
        from collections import Counter
        print("tier-B cluster histogram:",
              dict(sorted(Counter(r["clusters"] for r in tb).items())))
    else:
        with open(rpath) as fh:
            rows = json.load(fh)
        only = sys.argv[2] if len(sys.argv) > 2 else None
        limit = int(sys.argv[3]) if len(sys.argv) > 3 else None
        res = prove(rows, only, limit)
        opath = os.path.join(HERE, f"hv_proved_{only or 'all'}.json")
        with open(opath, "w") as fh:
            json.dump(res, fh, indent=1)
        closes = [r for r in res if r["verdict"] == "CLOSES"]
        print(f"\n{len(closes)} of {len(res)} CLOSE")
        for r in closes:
            print(f"  {r['unit']}::{r['fn']}  {r['note']}")
