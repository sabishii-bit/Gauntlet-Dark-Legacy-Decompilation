"""CN lane: is camera_mode_level's +0x8c0 pair reachable if BOTH the form word and
the swap are granted? Tests whether the residual is then a pure recolor, and
whether that recolor is a bijection (a renaming) or a coalescing.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))  # tools/gdl (fixed after promotion out of CN_scratch)
import webfrank as wf  # noqa: E402
sys.path.insert(0, os.path.dirname(__file__))
from cn_analyze import load, our_object, target_object  # noqa: E402

UNIT, FN = "game/world/camera", "camera_mode_level"
_od, _os, _osym, ours, orel, ojt = load(our_object(UNIT)[0], FN)
_td, _ts, _tsym, tgt, trel, _tj = load(target_object(UNIT), FN)

p = bytearray(ours)
p[0x740:0x748] = ours[0x744:0x748] + ours[0x740:0x744]   # cluster A permute
p[0x8b0:0x8b4] = tgt[0x8b0:0x8b4]                        # grant the form word
p[0x8c0:0x8c8] = ours[0x8c4:0x8c8] + ours[0x8c0:0x8c4]   # grant the swap
try:
    out, n = wf.copy_register_fields(bytes(p), tgt)
    print(f"copy_register_fields ACCEPTS with {n} fields -> residual is PURE RECOLOR")
except ValueError as e:
    print(f"copy_register_fields REFUSES: {e}")

# Is that recolor a bijection?  Collect the implied per-word field mapping.
pairs = set()
for off in range(0, len(p), 4):
    ow, tw = wf._u32(p, off), wf._u32(tgt, off)
    if ow == tw:
        continue
    try:
        mask = wf.register_slot_mask(ow)
    except ValueError:
        continue
    if (ow ^ tw) & ~mask:
        continue
    for shift in (21, 16, 11, 6):
        if not (mask >> shift) & 31:
            continue
        a, b = (ow >> shift) & 31, (tw >> shift) & 31
        if a != b:
            pairs.add((a, b))
src = {}
for a, b in sorted(pairs):
    src.setdefault(a, set()).add(b)
dst = {}
for a, b in sorted(pairs):
    dst.setdefault(b, set()).add(a)
print(f"\nimplied register pairs (ours -> target): {sorted(pairs)}")
print(f"non-functional (one of ours -> many target): "
      f"{ {k: v for k, v in src.items() if len(v) > 1} }")
print(f"NON-INJECTIVE (many of ours -> one target): "
      f"{ {k: v for k, v in dst.items() if len(v) > 1} }")

print("\nbisimulation on the granted state:")
try:
    wf.verify_consistent_recolor(bytes(p), tgt, jumptable_targets=ojt,
                                 relocated_offsets=set(orel),
                                 call_targets={o: n for o, (t, n) in orel.items()
                                               if t == 10})
    print("  PASS")
except ValueError as e:
    print(f"  REFUSED: {e}")
