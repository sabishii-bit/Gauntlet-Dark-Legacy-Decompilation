#!/usr/bin/env python3
"""Cross-join the webfrank rule taxonomy against the MEASURED residual shape of
the same function's raw compiler output."""
import collections, json, re
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]  # repo root (fixed after promotion)


def key(unit, fn):
    # matchtool strips dtk's _80XXXXXX suffix from local function names;
    # webfrank.json keeps it.  Normalize both sides or the join fabricates
    # "already exact" rows.
    if not fn.startswith("fn_"):
        fn = re.sub(r"_80[0-9A-Fa-f]{6}$", "", fn)
    return (unit, fn)


rules = json.load(open(REPO / "build" / "CV_rules.json", encoding="utf-8"))
cens = {key(r["unit"], r["fn"]): r for r in
        json.load(open(REPO / "build" / "CV_census.json", encoding="utf-8"))}

grid = collections.Counter()
missing = []
for r in rules:
    c = cens.get(key(r["unit"], r["fn"]))
    cls = " + ".join(r["stages"]) or "(none)"
    fam = c["family"] if c else "MATCH(raw already exact?)"
    grid[(cls, fam)] += 1
    if not c:
        missing.append(key(r["unit"], r["fn"]))

print(f"{'rule class':58s} {'measured raw residual':22s} n")
for (cls, fam), n in sorted(grid.items(), key=lambda kv: (-kv[1], kv[0])):
    print(f"{cls:58s} {fam:22s} {n}")
print(f"\nrules whose raw output already matches the target: {len(missing)}")
for k in missing:
    print("   ", k)

# Coverage: what fraction of the whole non-identical population is pinned?
allc = json.load(open(REPO / "build" / "CV_census.json", encoding="utf-8"))
pinned = {key(r["unit"], r["fn"]) for r in rules}
print("\n== residual family: pinned vs unpinned ==")
fam_split = collections.Counter()
for r in allc:
    fam_split[(r["family"], (r["unit"], r["fn"]) in pinned)] += 1
fams = sorted({f for f, _ in fam_split})
print(f"{'family':22s} {'pinned':>7s} {'unpinned':>9s}")
for f in fams:
    print(f"{f:22s} {fam_split[(f, True)]:>7d} {fam_split[(f, False)]:>9d}")
