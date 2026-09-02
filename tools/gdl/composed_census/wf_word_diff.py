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

TWO SWEEP SCREENS TRAVEL WITH THE COUNT (run 40, T10, from the run-39 RC
lane).  The word count alone was measured to mislead a sweep in two
independent ways, and both fixes are free here because this tool already
holds the two word streams and the repo root:

  MNEMONIC DIVERGENCE — claim.law.RC_identical-multiset-with-zero-clusters-
  merges-schedule-class-and-recolor-class.20260902.v1.  A large word count
  is satisfied by TWO disjoint classes with disjoint cures: RECOLOR (streams
  index-aligned, only register fields differ) and SCHEDULE-REORDER (same
  opcode bag, different emission order).  The run-39 census measured the
  split at 8 vs 31 functions, and the top three rows of a word-count
  ranking were schedule-class functions before the first recolor.  The
  discriminator is an index-aligned mnemonic comparison; this tool computes
  it from the opcode fields of the words it already has, with no
  disassembler.  Zero divergences at equal counts = recolor class.
    VERIFIED against the numbers the RC law itself records, which were
    taken with `rc_recolor_class.py` over objdump MNEMONIC STRINGS:
      drawMemCardMessage  204 insns,  61 words,   0 diffs — RECOLOR   ✓exact
      DrawPsysSub         290 insns,  91 words,   7 diffs — SCHEDULE  ✓exact
      AdsPutBuffer        254 insns, 220 words, 185 diffs — SCHEDULE  (law
                                                            says 189)
    The 4-count gap on AdsPutBuffer is SIMPLIFIED MNEMONICS: `mr` is
    `or rA,rS,rS`, so objdump prints two different mnemonics for identical
    opcode fields while this comparison — correctly, for the recolor
    question — treats them as the same instruction with different register
    fields.  The ZERO/NONZERO verdict, which is what names the class, is
    unaffected, and both zero cases agree exactly.

  PINNED — claim.law.RC_wf-word-diff-reads-the-raw-pre-postprocess-body-so-
  pinned-functions-rank-first.20260902.v1.  Because this tool reads the RAW
  body, a webfrank-pinned function reports the residual its pin ALREADY
  CLOSES, so a queue ranked by differing words puts FINISHED functions at
  the top: 4 of the 8 functions in the run-39 recolor class were pinned and
  closed, occupying first, third, fifth and eighth place.  That is the
  mirror image of AGENTS.md discipline 10 (pinned functions read `real` 0),
  and the screen is exactly as mandatory.  Note the rules live under the
  top-level `units` key — a parser that iterates the root finds 0 pins and
  reads like "no pins exist".
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
from unabsorbed import opcode_key, rule_served_functions  # noqa: E402


def mnemonic_divergence(ours, tgt):
    """Index-aligned mnemonic disagreements between two word streams.

    Zero at equal instruction counts means the streams are index-aligned
    and only REGISTER FIELDS differ — the recolor class.  Any disagreement
    means instructions moved, and the count measures how far the schedule
    travelled.

    `opcode_key` is IMPORTED from tools/gdl/unabsorbed.py rather than
    rewritten here.  The first draft of this function carried its own copy
    that masked only the 10-bit XO primaries; unabsorbed's has been
    handling the A-form 5-bit XO primaries (fmadd and friends) since it
    shipped, and it is covered by test_unabsorbed.py.  A second, worse copy
    of a discriminator is how two lanes end up quoting different numbers
    for the same question.
    """
    return sum(
        1 for o in range(0, len(ours), 4)
        if opcode_key(wf._u32(ours, o)) != opcode_key(wf._u32(tgt, o))
    )


def word_diff(unit, fn):
    """Return (kind, insns, rows, mnemonic_diffs)."""
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
    return kind, len(ours) // 4, rows, mnemonic_divergence(ours, tgt)


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
    kind, insns, rows, mnem = word_diff(unit, args.function)
    # rule_served_functions reads the top-level `units` key: a parser that
    # iterates the ROOT finds 2 keys and 0 pins, which reads exactly like
    # "no pins exist" and is how the run-39 sweep ranked four closed
    # functions first.
    pinned = args.function in rule_served_functions(unit, ROOT)
    print(f"{unit}::{args.function} ({kind}): {insns} insns, "
          f"DIFFERING WORDS = {len(rows)}, "
          f"MNEMONIC DIVERGENCE = {mnem}, "
          f"PINNED = {'YES' if pinned else 'no'}")
    if rows:
        print("  CLASS: " + (
            "RECOLOR — streams index-aligned, only register fields differ."
            " Cure is a register-assignment question (declaration order,"
            " width, type)."
            if mnem == 0 else
            f"SCHEDULE-REORDER — {mnem} instruction(s) differ in MNEMONIC at"
            " the same index, so the streams are not aligned. Cure is an"
            " emission-order question (statement order, the permutation"
            " rule class), NOT a recolor. The word count alone cannot tell"
            " these two apart."))
    if pinned:
        print("  PIN SCREEN: this function carries a webfrank rule, and this"
              " tool reads the RAW pre-postprocess body — the count above is"
              " the residual the pin ALREADY CLOSES, not an open one. DROP"
              " it before ranking a sweep by differing words (4 of 8"
              " functions in the run-39 recolor class were pinned and closed,"
              " and they took first, third, fifth and eighth place).")
    if args.list:
        for off, ow, tw in rows:
            print(f"  +{off:#06x}  O {ow:08x}  T {tw:08x}")
    return 0 if not rows else 1


if __name__ == "__main__":
    raise SystemExit(main())
