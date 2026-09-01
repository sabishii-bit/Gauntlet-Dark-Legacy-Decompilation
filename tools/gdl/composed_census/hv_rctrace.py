"""Trace verify_consistent_recolor's own fixpoint for ONE register key.

Discipline 14: instrument the refusal -- WHAT check fired, on WHICH word, and
would a sound-but-finer check pass?  This replays the SHIPPED transfer
functions (_recolor_transfer / _successors / _helper_call / _map_define) with
the identical merge rule, and reports which predecessor edge carried the
binding that got intersected away.
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

unit, fn = sys.argv[1], sys.argv[2]
key = ("g", int(sys.argv[3]))
at = int(sys.argv[4], 0)
perm = []
for spec in sys.argv[5:]:
    lo, hi, order = spec.split(":")
    perm.append((int(lo, 0), int(hi, 0), [int(x) for x in order.split(",")]))

op, _k = our_object(unit)
tp = target_object(unit)
_a, _b, _c, ours, orel, ojt = load(op, fn)
_d, _e, _f, tgt, trel, _g = load(tp, fn)
ours = bytearray(ours)
for lo, hi, order in perm:
    region = bytes(ours[lo:hi])
    ours[lo:hi] = b"".join(region[s * 4:s * 4 + 4] for s in order)

cur = [wf._u32(ours, o) for o in range(0, len(ours), 4)]
tw = [wf._u32(tgt, o) for o in range(0, len(tgt), 4)]
relocated = {o // 4 for o in orel}
jt = {o // 4 for o in ojt}
succ, calls = wf._successors(cur, relocated, jt)
ctargets = {o: n for o, (t, n) in orel.items() if t == 10}

identity = {("g", n): n for n in range(32)}
identity.update({("f", n): n for n in range(32)})
in_maps = {0: identity}
pending = [0]
out_maps = {}
while pending:
    i = pending.pop()
    state = dict(in_maps[i])
    try:
        state = wf._recolor_transfer(i, cur[i], tw[i], state)
    except ValueError as exc:
        print(f"  REFUSAL at +0x{4*i:03x}: {exc}")
        print(f"    in_map at that word: "
              + ", ".join(f"{k[0]}{k[1]}->{v}" for k, v in sorted(
                  state.items(), key=lambda kv: (kv[0][0], kv[0][1]))
                  if k[1] != v))
        out_maps[i] = state
        continue
    if calls[i]:
        helper = wf._helper_call(ctargets.get(i * 4))
        if helper is None:
            for k in wf._CALL_VOLATILE:
                state.pop(k, None)
            for k in wf._CALL_RETURNS:
                wf._map_define(state, k, k[1])
        elif helper[0] == "rest":
            _, bank, first = helper
            for n in range(first, 32):
                wf._map_define(state, (bank, n), n)
    out_maps[i] = state
    for s in succ[i]:
        known = in_maps.get(s)
        merged = dict(state) if known is None else {
            k: v for k, v in known.items() if state.get(k) == v}
        if known is None or merged != known:
            in_maps[s] = merged
            pending.append(s)

idx = at // 4
print(f"{unit}::{fn}  key={key}  at +0x{at:x}")
print(f"  ours {decode(cur[idx])} | tgt {decode(tw[idx])}")
print(f"  in_map[{key}] at +0x{at:x} = {in_maps.get(idx, {}).get(key, 'ABSENT')}")
preds = [i for i in range(len(cur)) if idx in succ[i]]
for p in preds:
    print(f"  pred +0x{4*p:03x}: out_map[{key}] = "
          f"{out_maps.get(p, {}).get(key, 'ABSENT')}   | {decode(cur[p])}")
# who last defined it on each path
print("  --- every position whose out_map binds this key differently ---")
prev = None
for i in range(len(cur)):
    v = out_maps.get(i, {}).get(key, "ABSENT")
    if v != prev:
        print(f"    +0x{4*i:03x} -> {v}    | {decode(cur[i])} | tgt {decode(tw[i])}")
        prev = v
