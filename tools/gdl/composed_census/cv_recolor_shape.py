#!/usr/bin/env python3
"""CV stage 4b: fingerprint the pure-RECOLOR population.

For every function whose whole raw residual is a register renaming, derive the
induced map target-reg -> our-reg over the differing words and report:
  * is it a well-defined injective map (a permutation of a register subset)?
  * its cycle structure (transposition / 3-cycle / longer)
  * whether the registers involved are ADJACENT numbers
  * whether it is a GPR or FPR renaming

An allocator-revision difference produces small permutations of adjacent
allocation-order slots; source-reachable differences do not have that shape.
"""
import collections, json, re, sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]  # repo root (fixed after promotion out of CV_scratch)
sys.path.insert(0, str(REPO / "tools" / "gdl"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import matchtool  # noqa: E402
from cv_census import body  # noqa: E402

REG = re.compile(r"\b([rf])(\d+)\b")
rows = json.load(open(REPO / "build" / "CV_census.json", encoding="utf-8"))
pure = [r for r in rows if r["family"] == "RECOLOR"]

cyc_hist = collections.Counter()
kind_hist = collections.Counter()
adj_hist = collections.Counter()
welldef = 0
illdef = []
detail = []

cache = {}
for r in pure:
    unit = r["unit"]
    if unit not in cache:
        src = REPO / "build" / "GUNE5D" / "src"
        d, n = Path(unit).parent, Path(unit).name
        raw = src / d / ".postprocess" / "body" / f"{n}.o"
        cand = raw if raw.exists() else src / f"{unit}.o"
        cache[unit] = (matchtool.parse(REPO / "build" / "GUNE5D" / "obj" / f"{unit}.o"),
                       matchtool.parse(cand))
    tf, of = cache[unit]
    t, o = body(tf[r["fn"]]), body(of[r["fn"]])
    m = {}
    ok = True
    for a, b in zip(t, o):
        if a == b:
            continue
        ta = REG.findall(f"{a[0]} {a[1]}")
        tb = REG.findall(f"{b[0]} {b[1]}")
        if len(ta) != len(tb):
            ok = False
            break
        for (k1, n1), (k2, n2) in zip(ta, tb):
            if k1 != k2:
                ok = False
                break
            if n1 == n2:
                continue
            key = (k1, int(n1))
            if key in m and m[key] != int(n2):
                ok = False
            m[key] = int(n2)
    if not ok or not m:
        illdef.append((unit, r["fn"]))
        continue
    welldef += 1
    kinds = {k for k, _ in m}
    kind_hist["+".join(sorted(kinds))] += 1
    # cycle structure within each register file
    for kind in kinds:
        sub = {n: v for (k, n), v in m.items() if k == kind}
        seen, cycles = set(), []
        for s in sub:
            if s in seen:
                continue
            cur, cyc = s, []
            while cur in sub and cur not in seen:
                seen.add(cur)
                cyc.append(cur)
                cur = sub[cur]
            cycles.append(len(cyc))
        cyc_hist[tuple(sorted(cycles, reverse=True))] += 1
        nums = sorted(set(sub) | set(sub.values()))
        span = nums[-1] - nums[0] + 1
        adj_hist["contiguous" if span == len(nums) else f"span{span}/n{len(nums)}"] += 1
    detail.append((unit, r["fn"], r["diffs"],
                   {f"{k}{n}": v for (k, n), v in sorted(m.items())}))

print(f"pure-RECOLOR functions: {len(pure)}")
print(f"  induced map is well-defined and injective-per-file: {welldef}")
print(f"  not a clean renaming (mixed/conflicting):           {len(illdef)}")
for u, f in illdef:
    print(f"      {u}::{f}")
print("\nregister file involved:")
for k, v in kind_hist.most_common():
    print(f"   {k:10s} {v}")
print("\ncycle structure of the induced permutation (per register file):")
for k, v in cyc_hist.most_common(12):
    print(f"   {str(k):24s} {v}")
print("\nregister-number span:")
for k, v in adj_hist.most_common(12):
    print(f"   {k:24s} {v}")
print("\nsample maps (smallest first):")
for u, f, d, m in sorted(detail, key=lambda x: x[2])[:18]:
    print(f"   d{d:<4d} {u:<26s} {f:<32s} {m}")
