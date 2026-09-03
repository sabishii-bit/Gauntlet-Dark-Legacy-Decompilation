"""Dump a raw word window of ours vs target, with copy-form decode."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# Rule-17 promotion damage; see the note in wf_detail.py (run-43 item 9).
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.dirname(HERE))       # tools/gdl
import webfrank as wf  # noqa: E402
from wf_detail import load  # noqa: E402


def main():
    if len(sys.argv) < 5:
        raise SystemExit(
            "usage: wf_dump.py <unit> <function> <lo> <hi>   "
            "(e.g. game/enemy/enemy move_logic00 0 0x40; offsets are"
            " function-relative and take 0x forms)")
    unit, name = sys.argv[1], sys.argv[2]
    lo = int(sys.argv[3], 0)
    hi = int(sys.argv[4], 0)
    ours, tgt, _ = load(unit, name)
    print(f"{unit}::{name}  [0x{lo:x},0x{hi:x})")
    for off in range(lo, hi, 4):
        ow, tw = wf._u32(ours, off), wf._u32(tgt, off)
        flag = "  " if ow == tw else "!!"
        try:
            pure = not ((ow ^ tw) & ~wf.register_slot_mask(ow))
        except ValueError:
            pure = False
        print(f" {flag} +0x{off:<5x} ours 0x{ow:08x} {str(wf.decode_copy_form(ow)):22}"
              f" tgt 0x{tw:08x} {str(wf.decode_copy_form(tw)):22}"
              f" {'regfield' if (pure and ow != tw) else ''}"
              f" {'CTRL' if wf._is_control_instruction(ow) else ''}")


if __name__ == "__main__":
    main()
