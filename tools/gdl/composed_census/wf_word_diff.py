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
    python tools/gdl/composed_census/wf_word_diff.py game/movie/movieplayer fn_800D8BCC --by-region

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

  RELOC-SYMBOL MISMATCH (run 41) — claim.law.SA_a-wrong-global-that-shares-
  an-instruction-word-is-invisible-to-every-score-including-the-word-count
  .20260902.v1.  When two globals are both reached through EMB_SDA21 the
  instruction word is `lwz rD,0(rB)` in BOTH streams and only the
  relocation names the symbol, so fndiff `real`, the multiset, objdiff
  fuzzy AND this tool's word count are all blind to reading the wrong one.
  PlayerProcessPowerups did exactly that, and the count read 80 before the
  fix and 80 after — a gameplay defect (powerup timers frozen by the wrong
  condition).  This column compares the two relocation tables at every
  offset whose WORD is identical, resolving symbols to addresses so a
  rename cancels.  CALIBRATED over the whole tree: 2,826 comparable
  functions, 15 flagged (7 with an index-aligned row), 47 not comparable
  and reported as such — an unscreened function must never read as a clean
  one.  A flagged row in a stream with mnemonic divergence nearby is marked
  PAIRING UNRELIABLE: an unlinked `bl` is one word whatever it calls.

  --by-region (run 41) — attempt.MV_fn800d8bcc-duplicated-branch-locals-
  belong-to-the-common-block.20260903.v1.  A residual DECOMPOSITION written
  in prose is what steers the next lane, and prose rots: fn_800D8BCC's
  predecessor record asserted the else-branch was not part of the residual
  while 27 of its 95 words (28%) were there, and three lanes worked from
  it.  This buckets the differing words into the --ops cluster partition
  SPLIT FURTHER at basic-block leaders — clusters alone put 88% of
  fn_800D8BCC's words in one 141-instruction equal run, which is true and
  useless, because a recolor lives entirely inside equal runs.

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
import difflib
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

import fndiff                                            # noqa: E402
import webfrank as wf                                    # noqa: E402
from cn_analyze import our_object, target_object, load    # noqa: E402
from unabsorbed import opcode_key, rule_served_functions  # noqa: E402


def ops_clusters(ours, tgt):
    """The --ops cluster partition of the function, as target-index blocks.

    Computed from the OPCODE-KEY streams this tool already holds, not from
    a second objdump: `fndiff --ops` builds its clusters the same way (a
    sequence match over mnemonics), and a partition derived from a
    different object than the words would let the two disagree.

    Returns difflib opcodes: (tag, t_lo, t_hi, o_lo, o_hi) over instruction
    indices, covering the whole function with no gaps.
    """
    keys_t = [opcode_key(wf._u32(tgt, o)) for o in range(0, len(tgt), 4)]
    keys_o = [opcode_key(wf._u32(ours, o)) for o in range(0, len(ours), 4)]
    return difflib.SequenceMatcher(
        None, keys_t, keys_o, autojunk=False).get_opcodes()


def basic_block_leaders(stream):
    """Instruction indices that START a basic block in one word stream.

    The --ops partition ALONE is not a decomposition for the residual class
    that most needs one: a recolor lives entirely INSIDE the matcher's equal
    runs, so bucketing fn_800D8BCC's 66 words by clusters put 58 of them
    (88%) in a single 141-instruction "equal" region — true, and useless.
    Splitting that run at control-flow boundaries is what makes the buckets
    correspond to the source blocks a record talks about (the if-branch, the
    else-branch, the case-2 init block).

    Leaders are index 0, every static branch target inside the function, and
    every instruction after a branch. `bl`/`bcl` are calls, not terminators.
    """
    count = len(stream) // 4
    leaders = {0}
    for index in range(count):
        word = wf._u32(stream, index * 4)
        primary, link, absolute = word >> 26, word & 1, (word >> 1) & 1
        if link:
            continue
        if primary in (16, 18):
            width = 26 if primary == 18 else 16
            mask = 0x03FFFFFC if primary == 18 else 0xFFFC
            if not absolute:
                target = index * 4 + wf._sign_extend(word & mask, width)
                if 0 <= target < count * 4:
                    leaders.add(target // 4)
            leaders.add(index + 1)
        elif primary == 19 and ((word >> 1) & 0x3FF) in (16, 528):
            leaders.add(index + 1)
    leaders.discard(count)
    return sorted(index for index in leaders if 0 <= index < count)


def ops_regions(ours, tgt):
    """The reported partition: --ops cluster boundaries PLUS block leaders.

    Every boundary of the cluster partition survives (so an --ops cluster is
    never split across two rows silently), and each region additionally
    breaks at the target stream's basic-block leaders.
    """
    clusters = ops_clusters(ours, tgt)
    leaders = set(basic_block_leaders(tgt))
    regions = []
    for tag, t_lo, t_hi, o_lo, o_hi in clusters:
        cuts = sorted({t_lo, t_hi} | {i for i in leaders if t_lo < i < t_hi})
        for lo, hi in zip(cuts, cuts[1:]):
            regions.append((tag, lo, hi, o_lo, o_hi))
    return regions


def region_word_counts(rows, regions):
    """[(tag, t_lo, t_hi, differing_words)] — the residual DECOMPOSITION.

    Run-41 item 4. A residual decomposition asserted in prose is the thing
    that steers the next lane, and prose is where it rots: the predecessor
    record on fn_800D8BCC asserted "the else-branch y-loop is NOT part of
    the residual" and "all ~87 words are anchored on two rows in the
    if-branch alone", while a per-region count of the same 95-word state
    read 8 words in the case-2 init block, 58 across the dir/row block and
    the if-branch, and 27 in the ELSE branch — 28% of the residual in the
    region the record excluded, and three lanes worked from it
    (attempt.MV_fn800d8bcc-duplicated-branch-locals-belong-to-the-common-
    block.20260903.v1). Bucketing the differing words into the --ops
    partition makes such a claim falsifiable in one command.

    Words are bucketed by their own offset against the TARGET index range,
    which is exact inside insert/delete/replace clusters and inside every
    equal run of a count-symmetric function.
    """
    counts = []
    for tag, t_lo, t_hi, _o_lo, _o_hi in regions:
        words = sum(1 for off, _o, _t in rows if t_lo <= off // 4 < t_hi)
        counts.append((tag, t_lo, t_hi, words))
    return counts


def _reloc_map(objpath, fn, insn_count):
    """{instruction index: (reloc_type, symbol_text)} for one function.

    Built from fndiff's parser rather than a second relocation reader:
    fndiff.parse already emits each relocation line directly after the
    instruction it patches, keeps the addend, and is covered by
    tools/gdl/tests. Returns None when the function is absent or its
    instruction count disagrees, so the caller reports "not comparable"
    instead of a silent empty result.
    """
    lines = fndiff.parse(objpath).get(fn)
    if lines is None:
        return None
    out = {}
    index = -1
    for line in lines:
        if line.startswith("    "):
            if index >= 0:
                parts = line.strip().split(maxsplit=1)
                out[index] = (parts[0],
                              parts[1] if len(parts) > 1 else "")
        else:
            index += 1
    if index + 1 != insn_count:
        return None
    return out


RELOC_ALIGNMENT_WINDOW = 4


def _locally_aligned(ours, tgt, index, window=RELOC_ALIGNMENT_WINDOW):
    """Do the two streams agree on MNEMONICS around this instruction?

    The pairing below is by function-relative OFFSET, which is exact when
    the streams are index-aligned and a guess when they are not: an
    unlinked `bl` is the same word 0x48000001 whatever it calls, so two
    DIFFERENT calls sitting at one offset in two differently-scheduled
    streams look like one call with two callees. camera_mode_follow has 360
    mnemonic divergences, and its single flagged row (get_cam_wpos vs
    calc_cam_pyr) is exactly that shape. Rows are still reported — a
    schedule difference at a call site is worth reading — but a row whose
    neighbourhood does not align is MARKED, the way fndiff marks its
    IMMEDIATE rows whose pairing the matcher had to choose.
    """
    count = len(tgt) // 4
    lo, hi = max(0, index - window), min(count, index + window + 1)
    return all(opcode_key(wf._u32(ours, i * 4))
               == opcode_key(wf._u32(tgt, i * 4)) for i in range(lo, hi))


def reloc_symbol_mismatches(unit, fn, ours, tgt):
    """[(offset, target_symbol, ours_symbol, locally_aligned)] rows.

    Run-41 item 4, second half. claim.law.SA_a-wrong-global-that-shares-an-
    instruction-word-is-invisible-to-every-score-including-the-word-count
    .20260902.v1: PlayerProcessPowerups read `gGameplayPauseTimer`
    (.sbss 0x80344770) where the target reads lbl_803447B8 (.sbss
    0x803447B8). Both are EMB_SDA21-reachable externs, so the instruction
    word is `lwz rD,0(rB)` in both streams and the RELOCATION is the only
    place the defect exists: `real`, the opcode multiset, objdiff fuzzy and
    THIS TOOL'S word count all read 80 before the fix and 80 after, while
    the aligned fnasm row went from `~` to `=`. It was a gameplay defect
    (powerup timers frozen by the wrong condition).

    Reported only where the two instruction WORDS at that offset are
    IDENTICAL and the relocation TYPES agree: then the two streams differ
    at that offset in the symbol alone, which no numeric arbiter here can
    see. Symbols are compared by resolved ADDRESS, so two spellings of one
    datum (`sFlags` vs `gControllerButtons+0x4`) cancel.
    """
    count = len(tgt) // 4
    t_map = _reloc_map(target_object(unit), fn, count)
    o_map = _reloc_map(our_object(unit)[0], fn, count)
    if t_map is None or o_map is None:
        return None
    rows = []
    for index in sorted(set(t_map) & set(o_map)):
        offset = index * 4
        if wf._u32(ours, offset) != wf._u32(tgt, offset):
            continue
        (t_type, t_sym), (o_type, o_sym) = t_map[index], o_map[index]
        if t_type != o_type or t_sym == o_sym:
            continue
        t_at = fndiff.resolve_reloc_symbol_positional(t_sym)
        o_at = fndiff.resolve_reloc_symbol_positional(o_sym)
        if t_at is not None and o_at is not None and t_at == o_at:
            continue  # one datum, two spellings
        if t_at is None or o_at is None:
            continue  # anonymous pool entry: fndiff --clean owns that row
        rows.append((offset, t_sym, o_sym,
                     _locally_aligned(ours, tgt, index)))
    return rows


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


def word_streams(unit, fn):
    """(kind, ours_bytes, target_bytes) for one function, count-checked."""
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
    return kind, ours, tgt


def word_diff(unit, fn):
    """Return (kind, insns, rows, mnemonic_diffs)."""
    kind, ours, tgt = word_streams(unit, fn)
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
    ap.add_argument("--by-region", action="store_true",
                    help="bucket the differing words into the --ops cluster"
                         " partition, so a residual DECOMPOSITION claim is"
                         " falsifiable in one command")
    args = ap.parse_args()
    unit = args.unit
    if unit.startswith("src/"):
        unit = unit[4:]
    for suffix in (".cpp", ".c"):
        if unit.endswith(suffix):
            unit = unit[:-len(suffix)]
    kind, ours, tgt = word_streams(unit, args.function)
    rows = [(o, wf._u32(ours, o), wf._u32(tgt, o))
            for o in range(0, len(ours), 4)
            if wf._u32(ours, o) != wf._u32(tgt, o)]
    insns = len(ours) // 4
    mnem = mnemonic_divergence(ours, tgt)
    mismatches = reloc_symbol_mismatches(unit, args.function, ours, tgt)
    # rule_served_functions reads the top-level `units` key: a parser that
    # iterates the ROOT finds 2 keys and 0 pins, which reads exactly like
    # "no pins exist" and is how the run-39 sweep ranked four closed
    # functions first.
    pinned = args.function in rule_served_functions(unit, ROOT)
    if mismatches is None:
        reloc_column = "not comparable"
    else:
        firm = sum(1 for row in mismatches if row[3])
        reloc_column = str(len(mismatches)) + (
            f" ({firm} index-aligned)" if len(mismatches) != firm else "")
    print(f"{unit}::{args.function} ({kind}): {insns} insns, "
          f"DIFFERING WORDS = {len(rows)}, "
          f"MNEMONIC DIVERGENCE = {mnem}, "
          f"RELOC-SYMBOL MISMATCH = {reloc_column}, "
          f"PINNED = {'YES' if pinned else 'no'}")
    if mismatches:
        print(f"  RELOC-SYMBOL MISMATCH: {len(mismatches)} instruction(s)"
              " whose WORD is identical in both streams relocate a"
              " DIFFERENT symbol. No number above moves when one of these"
              " is fixed — PlayerProcessPowerups read the wrong SDA21"
              " global and this count stayed at exactly 80 before and"
              " after (claim.law.SA_a-wrong-global-that-shares-an-"
              "instruction-word-is-invisible-to-every-score-including-the-"
              "word-count.20260902.v1). Read each row before believing a"
              " zero residual.")
        for offset, t_sym, o_sym, aligned in mismatches:
            print(f"    +{offset:#06x}  target {t_sym}"
                  f"   ours {o_sym}"
                  + ("" if aligned else
                     "   [PAIRING UNRELIABLE: the mnemonics disagree within"
                     f" {RELOC_ALIGNMENT_WINDOW} instruction(s) of here, so"
                     " these two offsets need not be the same instruction —"
                     " an unlinked `bl` is one word whatever it calls."
                     " Confirm against `fnasm <unit> <fn> 0xA:0xB --diff`]"))
    elif mismatches is None:
        print("  RELOC-SYMBOL MISMATCH: not comparable (a relocation table"
              " could not be paired against these words) — the wrong-symbol"
              " class is UNSCREENED here, not absent; use"
              " `fnasm <unit> <fn> --diff` and read the symbol column.")
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
    if args.by_region and rows:
        counts = region_word_counts(rows, ops_regions(ours, tgt))
        total = len(rows)
        clean = sum(1 for row in counts if not row[3])
        print(f"  BY REGION ({len(counts)} region(s): --ops cluster"
              " boundaries plus basic-block leaders; target byte offsets."
              f" {clean} clean region(s) omitted):")
        for tag, t_lo, t_hi, words in counts:
            if not words:
                continue
            share = 100.0 * words / total
            print(f"    +{t_lo * 4:#07x}-{t_hi * 4:#07x}  {tag:<7}"
                  f"  {t_hi - t_lo:>4} insns  {words:>4} words"
                  f"  {share:5.1f}%")
        ranked = sorted(counts, key=lambda row: -row[3])
        top = ranked[0]
        print(f"    DECOMPOSITION: the largest region"
              f" +{top[1] * 4:#07x}-{top[2] * 4:#07x} holds {top[3]} of"
              f" {total} words ({100.0 * top[3] / total:.1f}%);"
              f" {sum(1 for row in counts if row[3])} of {len(counts)}"
              " regions carry any. A prose claim that the residual lives in"
              " one place is checkable against exactly these numbers — a"
              " record that excluded a region holding 28% of the words"
              " steered three lanes"
              " (attempt.MV_fn800d8bcc-duplicated-branch-locals-belong-to-"
              "the-common-block.20260903.v1).")
    if args.list:
        for off, ow, tw in rows:
            print(f"  +{off:#06x}  O {ow:08x}  T {tw:08x}")
    return 0 if not rows else 1


if __name__ == "__main__":
    raise SystemExit(main())
