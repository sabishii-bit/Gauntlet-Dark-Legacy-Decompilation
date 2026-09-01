"""Per-function differing-word classification detail."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
import webfrank as wf  # noqa: E402
from wf_census import our_path, functions, OBJ, classify  # noqa: E402


def load(unit, name):
    op, is_raw = our_path(unit)
    odata = bytearray(open(op, "rb").read())
    tdata = bytearray(open(os.path.join(OBJ, unit + ".o"), "rb").read())
    osec, tsec = wf._sections(odata), wf._sections(tdata)
    s = {x.name: x for x in functions(odata, osec)}[name]
    t = {x.name: x for x in functions(tdata, tsec)}[name]
    ot, tt = osec[s.section_index], tsec[t.section_index]
    ours = bytes(odata[ot.offset + s.value:ot.offset + s.value + s.size])
    tgt = bytes(tdata[tt.offset + t.value:tt.offset + t.value + t.size])
    return ours, tgt, is_raw


def main():
    unit, name = sys.argv[1], sys.argv[2]
    ours, tgt, is_raw = load(unit, name)
    print(f"{unit}::{name}  insns={len(ours)//4}  raw_body={is_raw}")
    counts = {}
    for off in range(0, len(ours), 4):
        ow, tw = wf._u32(ours, off), wf._u32(tgt, off)
        if ow == tw:
            continue
        k = classify(ow, tw)
        counts[k] = counts.get(k, 0) + 1
        if k != "regfield":
            print(f"  {k:8} +0x{off:<5x} ours 0x{ow:08x} {wf.decode_copy_form(ow)}"
                  f"   target 0x{tw:08x} {wf.decode_copy_form(tw)}")
    print("  counts:", counts)


if __name__ == "__main__":
    main()
