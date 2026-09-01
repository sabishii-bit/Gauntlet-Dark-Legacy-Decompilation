"""Name the mechanism behind a '0 slot-compatible candidates' refusal.

For each destination slot in the window, report whether ANY of our source
atoms can legally serve it, and when none can, say WHY: no opcode/payload
match at all, an inverse-direction copy-form pair (ours is the copy, the
target is not -- unserved per the DC directionality law, a referral to lane
WF rather than a park), or a relocation-identity mismatch that survives the
instruction-boundary normalisation.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools", "gdl"))
import webfrank as wf  # noqa: E402
sys.path.insert(0, os.path.dirname(__file__))
from cn_analyze import our_object, target_object, load, decode  # noqa: E402
from ch_derive import word_compatible, norm_relocs  # noqa: E402


def main(unit, fn, lo, hi):
    op, _k = our_object(unit)
    _od, _os, _oy, ours, orel, _oj = load(op, fn)
    _td, _ts, _ty, tgt, trel, _tj = load(target_object(unit), fn)
    lo, hi = max(0, lo), min(len(ours), hi)
    n = (hi - lo) // 4
    norel, ntrel = norm_relocs(orel), norm_relocs(trel)
    unservable = []
    for d in range(n):
        tw = wf._u32(tgt, lo + d * 4)
        trl = ntrel.get(lo + d * 4)
        servers, inverse, relblock = [], 0, 0
        for s in range(n):
            ow = wf._u32(ours, lo + s * 4)
            ok, _kind = word_compatible(ow, tw)
            if ok:
                if norel.get(lo + s * 4) == trl:
                    servers.append(s)
                else:
                    relblock += 1
                continue
            o_cf, t_cf = wf.decode_copy_form(ow), wf.decode_copy_form(tw)
            if o_cf is not None and (t_cf is None or t_cf[0] != "copy"):
                inverse += 1
        if not servers:
            unservable.append((d, tw, inverse, relblock))
    if not unservable:
        print("  every destination slot has at least one server "
              "(refusal is elsewhere: step-0 or apply_patch)")
        return
    print(f"  {len(unservable)} UNSERVABLE destination slot(s):")
    for d, tw, inv, rb in unservable:
        off = lo + d * 4
        note = []
        if inv:
            note.append(f"{inv} inverse-direction copy-form near-server(s) "
                        f"-> UNSERVED CLASS, refer to lane WF")
        if rb:
            note.append(f"{rb} server(s) blocked on relocation identity")
        if not note:
            note.append("no opcode/payload match among any of our atoms")
        print(f"    +0x{off:03x} target {tw:08x} {decode(tw):<28} :: "
              + "; ".join(note))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], int(sys.argv[3], 0), int(sys.argv[4], 0))
