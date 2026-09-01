"""CN lane: dump the full mechanism of each found rule so its prose can be written
from measurement rather than from the search's say-so, and re-prove it."""
import copy
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))  # tools/gdl (fixed after promotion out of CN_scratch)
import webfrank as wf  # noqa: E402
sys.path.insert(0, os.path.dirname(__file__))
from cn_analyze import our_object, target_object, load, decode  # noqa: E402

found = json.load(open(os.path.join(os.path.dirname(__file__), "cn_found.json")))

for key, rule in found.items():
    unit, fn = key.split("::")
    op, _ = our_object(unit)
    tp = target_object(unit)
    _od, _os, _osym, ours, orel, _oj = load(op, fn)
    _td, _ts, _tsym, tgt, trel, _tj = load(tp, fn)
    perm = rule.get("instruction_permutation")
    print(f"##### {unit}::{fn}   ({len(ours)//4} insns)")
    diffs = [o for o in range(0, len(ours), 4)
             if wf._u32(ours, o) != wf._u32(tgt, o)]
    print(f"  differing words: {len(diffs)}  {[hex(d) for d in diffs]}")
    if perm:
        lo, hi = int(perm["start"], 16), int(perm["end"], 16)
        order = perm["order"]
        print(f"  permutation [0x{lo:x},0x{hi:x}) order={order}"
              f"{'   <-- IDENTITY (no reorder needed)' if order == sorted(order) else ''}")
        for i in range(lo, hi, 4):
            print(f"    atom {(i-lo)//4}  +0x{i:x}  {decode(wf._u32(ours, i))}")
        print("    target slots:")
        for i in range(lo, hi, 4):
            print(f"      +0x{i:x}  {decode(wf._u32(tgt, i))}")
        rr = {o: v for o, v in orel.items() if lo <= o < hi}
        tr = {o: v for o, v in trel.items() if lo <= o < hi}
        print(f"    region relocations: ours {rr} target {tr}")
    for f in rule.get("equivalent_copy_form", []):
        o = int(f["at"], 16)
        print(f"  form +0x{o:x}: proof={f['proof']}")
    print(f"  register stage: {'copy_register_fields' if rule.get('copy_register_fields') else 'NONE'}")

    probe = copy.deepcopy(rule)
    data = bytearray(open(op, "rb").read())
    _b, _a, changed = wf.apply_patch(data, probe, open(tp, "rb").read())
    for f in probe.get("equivalent_copy_form", []):
        if "_proved_at" in f:
            print(f"    form {f['at']} discharged by the def at +0x{f['_proved_at']:x}"
                  f"  ({decode(wf._u32(ours, f['_proved_at']))} pre-permute)")
    print(f"  apply_patch OK, {changed} atoms/fields\n")
