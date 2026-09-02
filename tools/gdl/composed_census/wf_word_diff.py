"""Count the TRUE differing words between our compiled body and the target.

WHY THIS EXISTS.  `fndiff --ops` clusters only where the OPCODE stream
diverges, so it is structurally blind to pure register-field differences.
A function can print "4 tokens in 5 clusters" while more than half its
words differ, and a lane that reads "only block X differs" off --ops will
size a postprocessor rule against a residual that is not the real one.
That is exactly how game/movie/movieplayer::fn_800D8BCC came to be recorded
as a 4-word live-zero park when it actually differs in 122 of 215 words
(claim.law.identical-multiset-is-blind-to-displacements.20260831.v1 is the
general form; this tool is the cheap measurement that catches it).

The differing-WORD count is the obligation any recolor stage must
discharge, so it -- not the --ops token delta -- is the number that decides
WebFrank candidacy.

    python tools/gdl/composed_census/wf_word_diff.py <unit> <function>
    python tools/gdl/composed_census/wf_word_diff.py game/movie/movieplayer fn_800D8BCC --list

Reads the RAW `.postprocess/body` where present, so it scores COMPILER
output rather than postprocessed output and already-shipped rules stay
visible.  Requires a completed `ninja`; stale objects silently misreport.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    parent = os.path.dirname(ROOT)
    if parent == ROOT:
        raise SystemExit("repo root not found above " + HERE)
    ROOT = parent
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, HERE)

import webfrank as wf                                    # noqa: E402
from cn_analyze import our_object, target_object, load    # noqa: E402


def word_diff(unit, fn):
    """Return (kind, insns, [(offset, our_word, target_word), ...])."""
    op, kind = our_object(unit)
    tp = target_object(unit)
    if not (os.path.exists(op) and os.path.exists(tp)):
        raise SystemExit(f"missing object for {unit} — run ninja first")
    _a, _b, _c, ours, _orel, _ojt = load(op, fn)
    _d, _e, _f, tgt, _trel, _tj = load(tp, fn)
    if len(ours) != len(tgt):
        raise SystemExit(
            f"{fn}: count-asymmetric ({len(ours)//4} vs {len(tgt)//4} insns) "
            f"— outside every postprocessor class by construction")
    rows = [(o, wf._u32(ours, o), wf._u32(tgt, o))
            for o in range(0, len(ours), 4)
            if wf._u32(ours, o) != wf._u32(tgt, o)]
    return kind, len(ours) // 4, rows


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("unit")
    ap.add_argument("function")
    ap.add_argument("--list", action="store_true",
                    help="print every differing word, not just the count")
    args = ap.parse_args()
    unit = args.unit
    if unit.startswith("src/"):
        unit = unit[4:]
    for suffix in (".cpp", ".c"):
        if unit.endswith(suffix):
            unit = unit[:-len(suffix)]
    kind, insns, rows = word_diff(unit, args.function)
    print(f"{unit}::{args.function} ({kind}): {insns} insns, "
          f"DIFFERING WORDS = {len(rows)}")
    if args.list:
        for off, ow, tw in rows:
            print(f"  +{off:#06x}  O {ow:08x}  T {tw:08x}")
    return 0 if not rows else 1


if __name__ == "__main__":
    raise SystemExit(main())
