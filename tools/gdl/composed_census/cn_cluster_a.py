"""CN lane: verify camera_mode_level cluster A readiness. VERIFY ONLY -- per the
work claim's mid-text correction (LZ discipline: a pin parks live source, and
clusters B/C stay open), this cluster must NOT be shipped as a rule.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))  # tools/gdl (fixed after promotion out of CN_scratch)
import webfrank as wf  # noqa: E402
sys.path.insert(0, os.path.dirname(__file__))
from cn_analyze import load, our_object, target_object, decode  # noqa: E402

UNIT, FN = "game/world/camera", "camera_mode_level"
_od, _os, osym, ours, orel, _oj = load(our_object(UNIT)[0], FN)
_td, _ts, tsym, tgt, trel, _tj = load(target_object(UNIT), FN)
print(f"sizes: ours {len(ours)//4} target {len(tgt)//4}")
diffs = [o for o in range(0, len(ours), 4) if wf._u32(ours, o) != wf._u32(tgt, o)]
print(f"differing words: {len(diffs)}")
print(f"first/last: 0x{diffs[0]:x} .. 0x{diffs[-1]:x}")

# cluster the differing words
clusters, cur = [], [diffs[0]]
for d in diffs[1:]:
    if d - cur[-1] <= 8:
        cur.append(d)
    else:
        clusters.append(cur)
        cur = [d]
clusters.append(cur)
print(f"clusters: {len(clusters)}")
for c in clusters:
    non_reg = 0
    for off in c:
        ow, tw = wf._u32(ours, off), wf._u32(tgt, off)
        try:
            if (ow ^ tw) & ~wf.register_slot_mask(ow):
                non_reg += 1
        except ValueError:
            non_reg += 1
    print(f"  [0x{c[0]:x}..0x{c[-1]:x}] {len(c)} words, {non_reg} non-register")

print("\n--- CLUSTER A candidate window ---")
for off in range(0x738, 0x750, 4):
    ow, tw = wf._u32(ours, off), wf._u32(tgt, off)
    m = "  " if ow == tw else "<>"
    print(f"  {m} +0x{off:x}  ours {decode(ow):<30} tgt {decode(tw)}")

for lo, hi, order in ((0x740, 0x748, [1, 0]),):
    region = ours[lo:hi]
    print(f"\nSTEP 0 on [0x{lo:x},0x{hi:x}) order={order}:")
    try:
        wf.check_permutation_dependences(region, order, None)
        print("  PASS (strict, exit_dead=None)")
    except ValueError as e:
        print(f"  REFUSED: {e}")
    permuted = b"".join(region[s * 4:s * 4 + 4] for s in order)
    reach = permuted == tgt[lo:hi]
    print(f"  swap alone reproduces target bytes for the window: {reach}")
    rr = {o: v for o, v in orel.items() if lo <= o < hi}
    tr = {o: v for o, v in trel.items() if lo <= o < hi}
    print(f"  region relocations: ours {rr} target {tr}")
