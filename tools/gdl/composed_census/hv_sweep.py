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
import re
import subprocess
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

# TU-shaped path fragments, for reading ownership out of claim prose.
TU_RE = re.compile(r"\b((?:game|dolphin|MSL|zlib|libc|runtime)/[\w/]+)")
# The FALLBACK only: the value hardcoded at run 31, kept solely so the
# sweep still gates sensibly when the graph cannot be queried.
_FALLBACK_OWNED = ("game/sys/memcard", "game/movie/movieplayer",
                   "game/world/gauntworld", "game/game/combat",
                   "game/game/player")


def units_from_claims(payload, me=None):
    """TU fragments in ACTIVE claims not owned by ``me``. Pure; testable."""
    units = set()
    for claim in payload.get("claims", []):
        if claim.get("state") != "active" or claim.get("owner") == me:
            continue
        blob = f"{claim.get('scope', '')} {claim.get('function', '')}"
        units.update(TU_RE.findall(blob))
    return tuple(sorted(units))


def owned_units(me=None):
    """TUs claimed by OTHER lanes, read LIVE from `gdlmem claims`.

    A webfrank rule FREEZES its function's source, so shipping into a TU
    another lane is editing aborts THEIR build at the WEBFRANK step -- this
    is a courtesy gate, not an optimisation. It was a hardcoded tuple that
    had already drifted a full run out of date once (still harvest-3's
    list during harvest-4), and a stale courtesy gate is worse than none:
    it silently sweeps the TUs it was meant to protect while skipping ones
    nobody owns any more.

    Claim scopes are prose, so TU-shaped fragments are extracted from the
    scope and function fields. Over-inclusion costs only a skipped
    candidate; this sweep never edits source.
    """
    try:
        out = subprocess.run(
            [sys.executable, os.path.join(ROOT, "memory_graph", "gdlmem.py"),
             "claims"], capture_output=True, text=True, cwd=ROOT, timeout=300)
        payload = json.loads(out.stdout)
    except Exception as exc:                                # noqa: BLE001
        print(f"!! could not read `gdlmem claims` ({type(exc).__name__}:"
              f" {exc}); falling back to the run-31 hardcoded list, which"
              " MAY BE STALE — verify ownership by hand")
        return tuple(_FALLBACK_OWNED)
    units = tuple(units_from_claims(payload, me))
    if units:
        print(f"claims: {len(units)} TU(s) owned by other lanes will be"
              f" skipped: {', '.join(sorted(units))}")
    else:
        print("claims: no other lane holds a TU-scoped claim — sweeping all")
    return tuple(sorted(units))


# Rows the sweep could not EVALUATE, kept apart from rows it evaluated and
# found wanting. Conflating the two is what turned the un-migrated
# _relocation_sha256 call sites into a page of false "does not close" rows:
# a deriver that CRASHES has measured nothing, and reporting that as a
# refusal is a false negative dressed as a result.
TOOL_ERRORS: list[tuple[str, str, str, str]] = []


def report_tool_errors():
    """Print the unevaluated rows and fail loudly if there are any."""
    if not TOOL_ERRORS:
        return 0
    print(f"\n!! {len(TOOL_ERRORS)} row(s) could NOT BE EVALUATED — these are"
          " TOOL BREAKAGE, not refusals, and must not be read as"
          " 'does not close':")
    seen = {}
    for unit, fn, stage, message in TOOL_ERRORS:
        seen.setdefault(message.split(":")[0] + ": " + message[:110], []) \
            .append(f"{unit}::{fn} [{stage}]")
    for message, rows in seen.items():
        print(f"   {message}")
        print(f"     {len(rows)} row(s), e.g. {rows[0]}")
    return 1


def shipped():
    path = os.path.join(ROOT, "config", "GUNE5D", "webfrank.json")
    with open(path) as fh:
        cfg = json.load(fh)
    out = set()
    for unit, rules in cfg.get("units", {}).items():
        for rule in rules:
            out.add((unit, rule["function"]))
    return out


def triage(units=None, owned=None):
    """Roster every unshipped equal-size differing function.

    ``units`` restricts the sweep to the named TUs (the --unit argument):
    this was image-or-nothing, so a lane wanting one TU's roster paid a
    full-image sweep for it.
    """
    have = shipped()
    owned = tuple(owned if owned is not None else owned_units())
    wanted = tuple(units) if units else None
    rows = []
    for unit in census.units():
        if owned and unit.startswith(owned):
            continue
        if wanted and not unit.startswith(wanted):
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
            except Exception as exc:                       # noqa: BLE001
                # NOT a refusal: the classifier failing is TOOL BREAKAGE.
                # Silently `continue`-ing here dropped rows from the
                # roster entirely, which reads as "nothing to do".
                TOOL_ERRORS.append((unit, tsym.name, "unabsorbed",
                                    f"{type(exc).__name__}: {exc}"))
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
            # A crash is not a measurement. Keep it out of the refusal
            # population entirely and surface it at the summary.
            rule, note = None, f"{type(exc).__name__}: {exc}"
            traceback.print_exc(limit=1)
            TOOL_ERRORS.append((unit, fn, f"tier {row['tier']}", note))
            rec = dict(row)
            rec["verdict"] = "ERROR"
            rec["note"] = note
            out.append(rec)
            print(f"[{i+1}/{len(todo[:limit])}] {unit}::{fn}: "
                  f"ERROR (NOT a refusal) -- {note[:150]}", flush=True)
            continue
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


USAGE = """hv_sweep — union re-sweep of the postprocessor-closability roster.

  python tools/gdl/composed_census/hv_sweep.py triage [--unit U[,U...]]
  python tools/gdl/composed_census/hv_sweep.py prove [A|B] [limit]

  --unit U[,U...]   restrict the sweep to these TUs (repeatable). Without
                    it the sweep is image-wide, which was the only mode.
  --out DIR         where hv_roster.json / hv_proved_*.json go. Default is
                    build/GUNE5D/hv/ — BUILD OUTPUT. They used to be written
                    beside this script, i.e. untracked JSON dropped into a
                    tracked directory of the repo.
  --me OWNER        this lane's work_claim owner, so its OWN claim does not
                    exclude the TUs it is sweeping.
"""

if __name__ == "__main__":
    argv = sys.argv[1:]
    if argv and argv[0] in ("--help", "-h", "help"):
        print(USAGE)
        raise SystemExit(0)
    flags = {a.split("=", 1)[0]: a.split("=", 1)[1] if "=" in a else ""
             for a in argv if a.startswith("--")}
    positional = [a for a in argv if not a.startswith("--")]
    mode = positional[0] if positional else "triage"
    only_units = [u for u in flags.get("--unit", "").split(",") if u] or None
    outdir = flags.get("--out") or os.path.join(
        ROOT, "build", "GUNE5D", "hv")
    os.makedirs(outdir, exist_ok=True)
    rpath = os.path.join(outdir, "hv_roster.json")
    if mode == "triage":
        rows = triage(only_units, owned_units(flags.get("--me") or None))
        with open(rpath, "w") as fh:
            json.dump(rows, fh, indent=1)
        ta = [r for r in rows if r["tier"] == "A"]
        tb = [r for r in rows if r["tier"] == "B"]
        scope = f" over {', '.join(only_units)}" if only_units else ""
        print(f"{len(rows)} equal-size differing unshipped functions{scope}: "
              f"tier A {len(ta)}, tier B {len(tb)}")
        from collections import Counter
        print("tier-B cluster histogram:",
              dict(sorted(Counter(r["clusters"] for r in tb).items())))
        print(f"wrote {rpath}")
        raise SystemExit(report_tool_errors())
    else:
        with open(rpath) as fh:
            rows = json.load(fh)
        only = positional[1] if len(positional) > 1 else None
        limit = int(positional[2]) if len(positional) > 2 else None
        res = prove(rows, only, limit)
        opath = os.path.join(outdir, f"hv_proved_{only or 'all'}.json")
        with open(opath, "w") as fh:
            json.dump(res, fh, indent=1)
        closes = [r for r in res if r["verdict"] == "CLOSES"]
        errors = [r for r in res if r["verdict"] == "ERROR"]
        evaluated = len(res) - len(errors)
        # Denominator is rows actually EVALUATED. Counting crashed rows as
        # part of "N of M" understates the close rate and hides breakage.
        print(f"\n{len(closes)} of {evaluated} evaluated CLOSE"
              + (f" ({len(errors)} row(s) NOT evaluated — see below)"
                 if errors else ""))
        for r in closes:
            print(f"  {r['unit']}::{r['fn']}  {r['note']}")
        raise SystemExit(report_tool_errors())
