#!/usr/bin/env python3
"""Re-audit every pragma scaffold: is its original premise still live?

`AGENTS.md` § "Direct investigation and handoff" requires a historical stop
claim to carry a current measured premise, and `probe.py` prints a standing
reminder ("N pragma/volatile scaffold row(s) in this TU -- re-audit each") that
nothing automated. This does the audit.

For every `#pragma X off|on ... #pragma X reset` region in a NonMatching TU it
records `real` for EVERY function the region spans, deletes the pragma pair,
rebuilds the TU object, re-measures, and restores with git. Verdicts:

  HARMFUL       every spanned function's real is <= baseline and at least one
                improves -- the scaffold is costing match quality
  LOAD-BEARING  some spanned function's real rises without it -- keep it
  DEAD          the object is byte-identical -- a proved class boundary, and
                the pragma can be deleted for clarity with no codegen effect

MEASURING ONLY THE NON-EXACT FUNCTION IS A TRAP, and it cost a lane one bad
keep (measured at e28d467f). A region routinely spans several functions; taking
`real` for the one non-exact function found four "HARMFUL" regions, and a
TU-wide objdiff fuzzy then showed THREE of them regressing the TU
(screensaver -0.1846, mb_particle -0.0144, pb_diag -0.0032) because removal
helped the measured function and hurt its neighbours. Spanning every function
catches that without a full report build.

`real` remains a SCREEN, not the arbiter. A HARMFUL verdict must still be
confirmed with a fresh objdiff fuzzy and a TU-wide gate before anything is kept
-- `real` and fuzzy read register-colour cascades differently and the project's
rule is that a fresh fuzzy decides.

Usage (from the repository root):
  python tools/gdl/composed_census/scaffold_audit.py --list
  python tools/gdl/composed_census/scaffold_audit.py --tu game/boss/bosscam
  python tools/gdl/composed_census/scaffold_audit.py --out build/GUNE5D/scaffold_audit.json
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent.parent
REPORT = REPO / "build" / "GUNE5D" / "report.json"
PRAGMA = re.compile(r"^\s*#pragma\s+(\S+)\s+(off|on|reset|\d+)\s*$")
FNDEF = re.compile(r"^[A-Za-z_][A-Za-z0-9_ \*]*\b(\w+)\s*\([^;]*$")
REAL_RE = re.compile(r"real (\d+)")


def nonmatching_tus():
    cfg = (REPO / "configure.py").read_text(errors="replace")
    return {re.sub(r"\.(c|cpp)$", "", m)
            for m in re.findall(r'Object\(NonMatching,\s*"([^"]+)"', cfg)}


def fuzzy_index():
    if not REPORT.exists():
        return {}
    out = {}
    for u in json.loads(REPORT.read_text())["units"]:
        key = u["name"].split("/", 1)[1] if "/" in u["name"] else u["name"]
        for f in u.get("functions", []):
            out[(key, f["name"])] = float(f.get("fuzzy_match_percent", 0))
    return out


def source_of(tu):
    for ext in (".c", ".cpp"):
        p = REPO / "src" / (tu + ext)
        if p.exists():
            return p
    return None


def regions(tu, scores):
    """[(pragma, opener_line, reset_line, [functions spanned])] for one TU."""
    path = source_of(tu)
    if path is None:
        return []
    lines = path.read_text(errors="replace").split("\n")
    stack, out = [], []
    for i, line in enumerate(lines, 1):
        m = PRAGMA.match(line)
        if not m:
            continue
        name, state = m.group(1), m.group(2)
        if state == "reset":
            for k in range(len(stack) - 1, -1, -1):
                if stack[k][0] == name:
                    pname, pstate, start = stack.pop(k)
                    fns = []
                    for j in range(start, i):
                        fm = FNDEF.match(lines[j])
                        if fm and "{" in "\n".join(lines[j:j + 3]):
                            fn = fm.group(1)
                            if (tu, fn) in scores:
                                fns.append(fn)
                    out.append((f"{pname} {pstate}", start, i, fns))
                    break
        else:
            stack.append((name, state, i))
    return out


def real_of(tu, fn):
    """`real` for one function, with 0 for an already-exact one.

    fndiff prints `DIFF <fn> ... real N` only for a function that differs; an
    exact one is `OK <fn>` and a pool-name-only one is `POOL <fn>`, NEITHER of
    which carries a `real` field. Reading only the `real` token returned None
    for all of them, and they were reported UNMEASURED -- 55 of 139 regions at
    a9c09f62, and exactly the ones that matter, because an EXACT function going
    non-zero is the regression signal this audit exists to catch.
    """
    r = subprocess.run([sys.executable, "tools/gdl/fndiff.py", f"{tu}.c", fn,
                        "--count"], capture_output=True, text=True, cwd=REPO,
                       timeout=600)
    for line in reversed((r.stdout + r.stderr).splitlines()):
        m = REAL_RE.search(line)
        if m:
            return int(m.group(1))
        if re.match(rf"^(OK|POOL)\s+{re.escape(fn)}\b", line.strip()):
            return 0
    return None


def build_tu(tu):
    r = subprocess.run(["ninja", f"build/GUNE5D/src/{tu}.o"],
                       capture_output=True, text=True, cwd=REPO, timeout=900)
    return r.returncode == 0


def verdict_for_deltas(deltas):
    """(verdict, worsened, improved) from {fn: (before, after)} real counts.

    ANY function worsening makes the region LOAD-BEARING even when another
    improves. Ranking a mixed region HARMFUL on the improvement alone is the
    trap this tool exists to close: three regions read HARMFUL that way and a
    TU-wide fuzzy then showed all three regressing the TU.
    """
    worse = sorted(f for f, d in deltas.items() if d[1] > d[0])
    better = sorted(f for f, d in deltas.items() if d[1] < d[0])
    if worse:
        return "LOAD-BEARING", worse, better
    if better:
        return "HARMFUL", worse, better
    return "DEAD", worse, better


def audit_region(tu, pragma, a, b, fns):
    path = source_of(tu)
    base = {f: real_of(tu, f) for f in fns}
    lines = path.read_text(errors="replace").split("\n")
    if not lines[a - 1].strip().startswith("#pragma"):
        return {"verdict": "SKIPPED", "why": f"line {a} is not a pragma"}
    original = "\n".join(lines)
    del lines[b - 1]
    del lines[a - 1]
    path.write_text("\n".join(lines))
    try:
        if not build_tu(tu):
            return {"verdict": "BUILD-FAILED"}
        after = {f: real_of(tu, f) for f in fns}
    finally:
        path.write_text(original)
        build_tu(tu)
    deltas = {f: (base[f], after[f]) for f in fns
              if base.get(f) is not None and after.get(f) is not None}
    if not deltas:
        return {"verdict": "UNMEASURED"}
    verdict, worse, better = verdict_for_deltas(deltas)
    return {"verdict": verdict, "deltas": deltas,
            "worse": worse, "better": better}


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tu", help="only this TU")
    ap.add_argument("--list", action="store_true",
                    help="enumerate regions without building anything")
    ap.add_argument("--out", help="write results as JSON here")
    args = ap.parse_args()

    scores = fuzzy_index()
    if not scores:
        print("scaffold_audit: build/GUNE5D/report.json missing — run ninja")
        return 2
    tus = sorted(nonmatching_tus())
    if args.tu:
        tus = [t for t in tus if args.tu in t]

    plan = []
    for tu in tus:
        for pragma, a, b, fns in regions(tu, scores):
            if fns:
                plan.append((tu, pragma, a, b, fns))

    if args.list:
        print(f"{len(plan)} pragma region(s) spanning at least one measured "
              f"function, across {len(tus)} NonMatching TU(s)")
        for tu, pragma, a, b, fns in plan:
            nonexact = [f for f in fns if scores.get((tu, f), 100) < 100]
            print(f"  {tu:26} {pragma:22} {a}-{b}  spans {len(fns)} fn(s)"
                  f"{', non-exact: ' + ', '.join(nonexact) if nonexact else ''}")
        return 0

    results = []
    print(f"{'TU':24} {'pragma':22} {'spans':>5}  verdict")
    print("-" * 92)
    for tu, pragma, a, b, fns in plan:
        r = audit_region(tu, pragma, a, b, fns)
        r.update({"tu": tu, "pragma": pragma, "lines": [a, b], "functions": fns})
        results.append(r)
        extra = ""
        if r["verdict"] == "HARMFUL":
            extra = "  improves: " + ", ".join(
                f"{f} {r['deltas'][f][0]}->{r['deltas'][f][1]}"
                for f in r["better"])
        elif r["verdict"] == "LOAD-BEARING":
            extra = "  worsens: " + ", ".join(
                f"{f} {r['deltas'][f][0]}->{r['deltas'][f][1]}"
                for f in r["worse"][:2])
        print(f"{tu:24} {pragma:22} {len(fns):5}  {r['verdict']}{extra[:44]}")
    tally = {}
    for r in results:
        tally[r["verdict"]] = tally.get(r["verdict"], 0) + 1
    print("\n" + "  ".join(f"{k} {v}" for k, v in sorted(tally.items())))
    print("\n`real` is a SCREEN. Confirm any HARMFUL verdict with a fresh "
          "objdiff fuzzy and a TU-wide gate before keeping it.")
    if args.out:
        Path(args.out).write_text(json.dumps(results, indent=1))
        print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
