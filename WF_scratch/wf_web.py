"""Trace every def/use of one GPR in ours vs target, to expose a web split."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
import webfrank as wf  # noqa: E402
from wf_detail import load  # noqa: E402


def main():
    unit, name, reg = sys.argv[1], sys.argv[2], int(sys.argv[3])
    lo = int(sys.argv[4], 0) if len(sys.argv) > 4 else 0
    hi = int(sys.argv[5], 0) if len(sys.argv) > 5 else None
    ours, tgt, _ = load(unit, name)
    hi = hi if hi is not None else len(ours)
    words = [wf._u32(ours, o) for o in range(0, len(ours), 4)]
    succ, _c = wf._successors(words, set(), set())
    preds = {}
    for i, ts in enumerate(succ):
        for t in ts:
            preds.setdefault(t, []).append(i)
    print(f"{name}: def/use of g{reg} in [0x{lo:x},0x{hi:x})")
    for off in range(lo, hi, 4):
        i = off // 4
        ow, tw = wf._u32(ours, off), wf._u32(tgt, off)
        try:
            r, w = wf._word_effects(ow)
        except Exception:
            continue
        tags = []
        if ("g", reg) in w:
            tags.append("DEF")
        if ("g", reg) in r:
            tags.append("use")
        pp = preds.get(i, [])
        join = f"  <-preds {[hex(p*4) for p in pp]}" if len(pp) != 1 or pp != [i - 1] else ""
        if tags or join:
            mark = "!!" if ow != tw else "  "
            print(f" {mark} +0x{off:<5x} {','.join(tags):8} "
                  f"ours 0x{ow:08x}  tgt 0x{tw:08x}{join}")


if __name__ == "__main__":
    main()
