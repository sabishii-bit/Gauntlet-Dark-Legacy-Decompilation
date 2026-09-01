"""CN lane: characterize camera_mode_level's blocking region [0x8b0,0x8d0) for the
LZ v2 combined form+renaming handoff, and test whether the whole function would
close given (i) the cluster-A permute and (ii) a blanket recolor.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools", "gdl"))
import webfrank as wf  # noqa: E402
sys.path.insert(0, os.path.dirname(__file__))
from cn_analyze import load, our_object, target_object, decode  # noqa: E402

UNIT, FN = "game/world/camera", "camera_mode_level"
_od, _os, _osym, ours, orel, ojt = load(our_object(UNIT)[0], FN)
_td, _ts, _tsym, tgt, trel, _tj = load(target_object(UNIT), FN)

print("--- blocking region ---")
for off in range(0x8a4, 0x8d4, 4):
    ow, tw = wf._u32(ours, off), wf._u32(tgt, off)
    m = "  " if ow == tw else "<>"
    extra = ""
    if ow != tw:
        try:
            extra = ("  REGFIELD-ONLY" if not ((ow ^ tw) & ~wf.register_slot_mask(ow))
                     else "  NON-REGISTER")
        except ValueError as e:
            extra = f"  UNDECODABLE({e})"
    print(f"  {m} +0x{off:x}  ours {decode(ow):<30} tgt {decode(tw):<30}{extra}")

print("\n--- decode_copy_form at +0x8b0 ---")
print(f"  ours {wf.decode_copy_form(wf._u32(ours, 0x8b0))}")
print(f"  tgt  {wf.decode_copy_form(wf._u32(tgt, 0x8b0))}")

print("\n--- STEP 0 on the +0x8c0 pair (C2 says it must refuse) ---")
region = ours[0x8c0:0x8c8]
try:
    wf.check_permutation_dependences(region, [1, 0], None)
    print("  PASS  <-- contradicts the C2 record")
except ValueError as e:
    print(f"  REFUSED (correctly): {e}")

print("\n--- would permute(A) + blanket recolor close the function? ---")
patched = bytearray(ours)
patched[0x740:0x748] = ours[0x744:0x748] + ours[0x740:0x744]
try:
    out, n = wf.copy_register_fields(bytes(patched), tgt)
    print(f"  copy_register_fields ACCEPTS, {n} fields -> function closes")
except ValueError as e:
    print(f"  copy_register_fields REFUSES: {e}")

print("\n--- and with the +0x8b0 form word additionally set to target? ---")
p2 = bytearray(patched)
p2[0x8b0:0x8b4] = tgt[0x8b0:0x8b4]
try:
    out, n = wf.copy_register_fields(bytes(p2), tgt)
    print(f"  copy_register_fields ACCEPTS, {n} fields -> function CLOSES")
    print("  (so +0x8b0 is the ONLY blocker beyond cluster A; the rest is recolor)")
except ValueError as e:
    print(f"  copy_register_fields REFUSES: {e}")
