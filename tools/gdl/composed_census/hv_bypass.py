"""Is offset TO reachable from entry along a path that AVOIDS offset AVOID?

If yes, a recolor binding established at AVOID is legitimately intersected away
at TO and the refusal is a fact about the FUNCTION.  If no, the binding was
lost for some other reason and the refusal is a fact about the CHECKER.
"""
import os
import sys
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
import webfrank as wf                                   # noqa: E402
from cn_analyze import our_object, target_object, load, decode  # noqa: E402

unit, fn, to, avoid = sys.argv[1], sys.argv[2], int(sys.argv[3], 0), int(sys.argv[4], 0)
op, _k = our_object(unit)
_a, _b, _c, ours, orel, ojt = load(op, fn)
cur = [wf._u32(ours, o) for o in range(0, len(ours), 4)]
succ, _calls = wf._successors(cur, {o // 4 for o in orel}, {o // 4 for o in ojt})
skip, goal = avoid // 4, to // 4
seen, stack, parent = {0}, [0], {0: None}
hit = None
while stack:
    i = stack.pop()
    if i == goal:
        hit = i
        break
    for s in succ[i]:
        if 0 <= s < len(cur) and s not in seen and s != skip:
            seen.add(s)
            parent[s] = i
            stack.append(s)
if hit is None:
    print(f"NO path to +0x{to:x} avoids +0x{avoid:x} "
          f"(reached {len(seen)} of {len(cur)} words)")
else:
    path, i = [], hit
    while i is not None:
        path.append(i)
        i = parent[i]
    path.reverse()
    print(f"PATH to +0x{to:x} avoiding +0x{avoid:x} ({len(path)} words):")
    for i in path:
        print(f"    +0x{4*i:03x}  {decode(cur[i])}")
