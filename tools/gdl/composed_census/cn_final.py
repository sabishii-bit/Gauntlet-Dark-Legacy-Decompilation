"""CN lane: finalize the four new closures.

ProcessCritter's searched order was the IDENTITY, i.e. no reorder is needed at
all; shipping an identity instruction_permutation would pin a region hash for
no mechanism and misdescribe the residual, so it is re-derived here as a pure
copy-form + recolor rule.  next_rune_hint is retried over its siblings' window
shape, since next_boss_hint and next_legend_hint closed on an identical one.
"""
import copy
import hashlib
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools", "gdl"))
import webfrank as wf  # noqa: E402
sys.path.insert(0, os.path.dirname(__file__))
from cn_analyze import our_object, target_object, load, decode  # noqa: E402
from cn_search import try_candidate  # noqa: E402


def sha(b):
    return hashlib.sha256(b).hexdigest()


def prove(unit, fn, rule):
    op, _ = our_object(unit)
    tp = target_object(unit)
    _od, _os, _osym, ours, _orel, _oj = load(op, fn)
    _td, _ts, _tsym, tgt, _trel, _tj = load(tp, fn)
    probe = copy.deepcopy(rule)
    data = bytearray(open(op, "rb").read())
    _b, _a, changed = wf.apply_patch(data, probe, open(tp, "rb").read())
    sec = wf._sections(data)
    sym = wf._find_symbol(data, sec, fn)
    t = sec[sym.section_index]
    got = bytes(data[t.offset + sym.value:t.offset + sym.value + sym.size])
    resid = sum(1 for o in range(0, len(got), 4)
                if wf._u32(got, o) != wf._u32(tgt, o))
    print(f"  apply_patch OK, {changed} atoms/fields, residual {resid}")
    for f in probe.get("equivalent_copy_form", []):
        if "_proved_at" in f:
            print(f"    form {f['at']} proof={f['proof']} discharged at "
                  f"+0x{f['_proved_at']:x} ({decode(wf._u32(ours, f['_proved_at']))})")
    return resid == 0


print("== ProcessCritter re-derived WITHOUT the identity permutation")
unit, fn = "game/enemy/critter", "ProcessCritter"
op, _ = our_object(unit)
_od, _os, _osym, ours, _orel, _oj = load(op, fn)
_td, _ts, _tsym, tgt, _trel, _tj = load(target_object(unit), fn)
diffs = [o for o in range(0, len(ours), 4)
         if wf._u32(ours, o) != wf._u32(tgt, o)]
for o in diffs:
    ow, tw = wf._u32(ours, o), wf._u32(tgt, o)
    try:
        pure = not ((ow ^ tw) & ~wf.register_slot_mask(ow))
    except ValueError:
        pure = False
    print(f"  +0x{o:x}  {'RECOLOR ' if pure else 'FORM    '} "
          f"ours {decode(ow):<24} tgt {decode(tw)}")
pc = {
    "function": fn,
    "before_sha256": sha(ours),
    "after_sha256": sha(tgt),
    "equivalent_copy_form": [{"at": "0xb4", "proof": "dominating_def"}],
    "copy_register_fields": True,
}
ok = prove(unit, fn, pc)
print(f"  ProcessCritter closes without a permutation stage: {ok}\n")

print("== next_rune_hint retried over wider windows")
for lo, hi in ((0x10c, 0x128), (0xfc, 0x118), (0x100, 0x11c), (0xf8, 0x118),
               (0x100, 0x120)):
    print(f"  window [0x{lo:x},0x{hi:x}):")
    try_candidate("game/ui/options", "next_rune_hint", lo, hi)

found = json.load(open(os.path.join(os.path.dirname(__file__), "cn_found.json")))
found["game/enemy/critter::ProcessCritter"] = pc
json.dump(found, open(os.path.join(os.path.dirname(__file__), "cn_found.json"),
                      "w"), indent=2)
print("\nfinal rule set:")
for k, v in found.items():
    print(f"  {k}: stages="
          f"{[s for s in ('instruction_permutation', 'equivalent_copy_form', 'copy_register_fields') if v.get(s)]}")
    print(json.dumps(v, indent=2))
