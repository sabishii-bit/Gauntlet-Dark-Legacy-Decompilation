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
    python tools/gdl/composed_census/wf_word_diff.py <unit> <fn> --range 0x1c4:0x250 --list

EXIT CODE (run 42): 0 whenever the MEASUREMENT succeeded, whether or not
words differ.  It used to return 1 on any residual — which is what a normal
call looks like — so every `&&` chain and CI step around it stopped at the
first function that had one.  Nonzero now means the measurement did not
happen (missing object, count-asymmetric function).

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

  DECODE (run 48, T18) — the PER-WORD class, promoted from WR's
  `WR_scratch/wr_wordscreen.py` prototype, which was a lane scratch file and
  is gone.  The record that depends on it (attempt.WR_limitcamval2-is-not-
  postprocessor-eligible-a-branch-displacement-decides-it.20260903.v1) names
  `python WR_scratch/wr_wordscreen.py game/boss/bosscam LimitCamVal2` as its
  denial's EXPIRY CHECK — a command nobody but its author could run.  The
  whole-function CLASS line reads MNEMONIC divergence alone and cannot
  express what decides candidacy: every differing word is REGFIELD-ONLY,
  IMMEDIATE, BRANCH, OPCODE or RELOCATED, and only REGFIELD-ONLY is inside a
  shipped rule class.  The DECODE summary prints unconditionally; `--decode`
  (and `--list`) adds the per-word column.
    Reproduces the record's numbers exactly at cdfff02e2: LimitCamVal2, 88
    insns, DIFFERING WORDS = 29, MNEMONIC DIVERGENCE = 5, RELOC-SYMBOL
    MISMATCH = 0 -> `REGFIELD-ONLY 23, IMMEDIATE 0, BRANCH 1, OPCODE 5`, and
    +0x13c reads BRANCH on `O 4081000c / T 40810008`, which is the word the
    denial turns on.
    CALIBRATED TWO-SIDED over all 2,883 comparable function pairs (115
    count-asymmetric, not scored):
      POSITIVES  16 functions whose CLASS line said RECOLOR — "cure is a
                 register-assignment question" — while carrying words no
                 register assignment can reach (sDvdReadSync 5 IMMEDIATE of
                 5 words; btricol::LineLineDist 4; pbInitTlutRegions 4 of 4).
                 Those now read RECOLOR-SHAPED BUT NOT RECOLOURABLE.
      NEGATIVES  99 functions stay RECOLOR (genuinely recolourable) and 162
                 stay SCHEDULE-REORDER with the verdict unchanged; 2,606
                 have no residual and print nothing.
    The RELOCATED class is what makes those numbers honest: 18 differing
    words image-wide differ ONLY in bits a relocation patches, and without
    the relocation table four functions (gutil::gstrcmp, gutil::gstrcpy,
    pbutils::stricmp, OSTime::OSGetTime) read `BRANCH 1` — "no shipped rule
    reaches it" — on a `R_PPC_REL14` displacement dtk emits with a zero
    payload.  Those bits are the LINKER's; the positives fell 20 -> 16.

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


DECODE_CLASSES = ("REGFIELD-ONLY", "IMMEDIATE", "BRANCH", "OPCODE",
                  "RELOCATED")

# Every class a shipped postprocessor rule can reach lives inside the four
# five-bit register slots. `register_slot_mask` returns 0 for a branch and for
# lmw/stmw, so those words are unreachable by construction, and a word whose
# difference falls outside the mask is a literal the linker does not own.
REACHABLE_DECODE_CLASSES = ("REGFIELD-ONLY",)

# RELOCATED is neither: the differing bits belong to the LINKER, so the word
# is not evidence of a codegen difference at all. It is counted and named
# separately rather than folded into either side.
LINKER_OWNED_DECODE_CLASSES = ("RELOCATED",)

# The bits each relocation type patches. A differing bit inside one of these
# is the linker's, not the compiler's — the same exclusion `fndiff --ops`
# applies when it refuses to call a relocated field an IMMEDIATE row.
# Measured need: game/sys/gutil::gstrcmp differs in exactly one word, +0x44
# `ours 4082ffbc / target 40820000`, and the TARGET side carries
# `R_PPC_REL14 gstrcmp` there while ours carries none — dtk emits an
# intra-function branch as a relocation with a zero displacement. Without
# this table that word reads BRANCH, i.e. "no shipped rule reaches it", when
# the two streams may link identically.
_RELOC_FIELD_MASKS = {
    "R_PPC_REL24": 0x03FFFFFC,
    "R_PPC_ADDR24": 0x03FFFFFC,
    "R_PPC_REL14": 0x0000FFFC,
    "R_PPC_REL14_BRTAKEN": 0x0000FFFC,
    "R_PPC_REL14_BRNTAKEN": 0x0000FFFC,
    "R_PPC_ADDR14": 0x0000FFFC,
    "R_PPC_ADDR14_BRTAKEN": 0x0000FFFC,
    "R_PPC_ADDR14_BRNTAKEN": 0x0000FFFC,
    "R_PPC_ADDR16": 0x0000FFFF,
    "R_PPC_ADDR16_LO": 0x0000FFFF,
    "R_PPC_ADDR16_HI": 0x0000FFFF,
    "R_PPC_ADDR16_HA": 0x0000FFFF,
    "R_PPC_ADDR16_SDA": 0x0000FFFF,
    "R_PPC_SDAREL16": 0x0000FFFF,
    # EMB_SDA21 patches the base register field AND the displacement.
    "R_PPC_EMB_SDA21": 0x001FFFFF,
}
# An unknown relocation type owns UNKNOWN bits, so the whole word is treated
# as the linker's rather than attributed to codegen.
_UNKNOWN_RELOC_MASK = 0xFFFFFFFF


def relocated_field_mask(reloc_types):
    """Bits owned by the relocations present at one instruction."""
    mask = 0
    for name in reloc_types or ():
        if not name:
            continue
        mask |= _RELOC_FIELD_MASKS.get(name, _UNKNOWN_RELOC_MASK)
    return mask


def decode_word_class(ours_word, target_word, reloc_types=()):
    """Which CLASS of difference one differing word carries (run-48 item 4).

    Promoted from WR's `WR_scratch/wr_wordscreen.py` prototype, which was a
    lane scratch file and is therefore gone: the run-47 record that depends on
    it (attempt.WR_limitcamval2-is-not-postprocessor-eligible-a-branch-
    displacement-decides-it.20260903.v1) names
    `python WR_scratch/wr_wordscreen.py game/boss/bosscam LimitCamVal2` as its
    denial's EXPIRY CHECK, and that command cannot be run by anyone but its
    author. The classification it computed is what decides postprocessor
    candidacy, and the whole-function CLASS line below cannot express it:
    LimitCamVal2 is `MNEMONIC DIVERGENCE = 5` so it reads SCHEDULE-REORDER,
    while the word that actually decides it is a BRANCH DISPLACEMENT no
    permutation may touch.

      OPCODE         the two words are not the same instruction at this index
                     (the opcode key differs) — outside every class
      RELOCATED      same instruction, and every differing bit falls inside a
                     field a RELOCATION patches. Those bits are the LINKER's,
                     so the raw difference is not evidence of a codegen
                     difference either way — read it with `fndiff --relocs`
      BRANCH         same instruction, and it is a control op (primary 16-19).
                     `register_slot_mask` is 0 here, so no register-field rule
                     can reach it, and `permute_instruction_atoms` refuses any
                     region containing one
      IMMEDIATE      same instruction, and the differing bits fall OUTSIDE
                     every register slot — a literal or displacement, which
                     no shipped class reaches either
      REGFIELD-ONLY  same instruction, and every differing bit lies inside
                     one of PowerPC's four five-bit register slots

    The mask is INTERSECTED across the two words rather than read off ours
    alone: two words with one opcode key can still take different operand
    forms (`li rD,x` is `addi rD,0,x`), and crediting a bit as a register
    field on the strength of one side's form would call a real difference
    recolourable.
    """
    if opcode_key(ours_word) != opcode_key(target_word):
        return "OPCODE"
    diff = ours_word ^ target_word
    if not diff & ~relocated_field_mask(reloc_types):
        return "RELOCATED"
    if wf._is_control_instruction(ours_word):
        return "BRANCH"
    mask = (wf.register_slot_mask(ours_word)
            & wf.register_slot_mask(target_word))
    if diff & ~mask:
        return "IMMEDIATE"
    return "REGFIELD-ONLY"


def decode_rows(rows, reloc_types_by_index=None):
    """[(offset, ours, target, class)] for the differing words.

    `reloc_types_by_index` is {instruction index: (type, type, ...)} —
    whatever relocations either object carries at that instruction. None
    means the tables could not be paired, and the classification then runs
    without them exactly as before.
    """
    types = reloc_types_by_index or {}
    return [(offset, ours, target,
             decode_word_class(ours, target, types.get(offset // 4, ())))
            for offset, ours, target in rows]


def decode_counts(rows, reloc_types_by_index=None):
    """{class: count} over the differing words, every class present."""
    counts = {name: 0 for name in DECODE_CLASSES}
    for _offset, _o, _t, name in decode_rows(rows, reloc_types_by_index):
        counts[name] += 1
    return counts


def reloc_types_by_index(unit, fn, insn_count):
    """{index: (type, ...)} over BOTH objects, or {} when unpairable."""
    out = {}
    for objpath in (target_object(unit), our_object(unit)[0]):
        table = _reloc_map(objpath, fn, insn_count)
        if table is None:
            continue
        for index, (rtype, _sym) in table.items():
            out.setdefault(index, set()).add(rtype)
    return {index: tuple(sorted(types)) for index, types in out.items()}


def decode_summary(counts, total):
    """The one-line DECODE verdict, or "" when there is nothing to decode."""
    if not total:
        return ""
    unreachable = sum(
        n for name, n in counts.items()
        if name not in REACHABLE_DECODE_CLASSES
        and name not in LINKER_OWNED_DECODE_CLASSES)
    line = ("  DECODE: "
            + ", ".join(f"{name} {counts[name]}" for name in DECODE_CLASSES))
    if unreachable:
        line += (f" — {unreachable} of {total} word(s) lie OUTSIDE every"
                 " register-field class, so NO copy_register_fields rule can"
                 " close this residual however small the count is. A BRANCH"
                 " word is refused twice over: register_slot_mask returns 0"
                 " for it and permute_instruction_atoms refuses any region"
                 " containing a control op.")
    elif counts.get("RELOCATED"):
        line += (f" — no word here is outside the register-field class, but"
                 f" {counts['RELOCATED']} differ only in bits a RELOCATION"
                 " patches; those are the LINKER's and are not evidence of a"
                 " codegen difference. Read them with `fndiff --relocs`"
                 " before sizing anything.")
    else:
        line += (f" — all {total} differing word(s) are register fields, the"
                 " only class a shipped rule reaches. Count parity and an"
                 " identical multiset are NOT sufficient for candidacy; this"
                 " line is.")
    return line


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


def parse_range(text):
    """`0xA:0xB` -> (lo, hi) TARGET byte offsets, or None.

    Run-42 item 2. A hand grep is what this replaces, and the measured
    failure mode of the hand grep was UNDER-MATCHING: it counted the rows
    whose printed `+0x....` happened to match a pattern rather than the
    rows whose offset falls in the window, which is how a windowed count
    nearly reached a record (AGENTS.md discipline 8 — write records FROM
    tool output). `hi` is EXCLUSIVE, matching the `0xA:0xB` spelling every
    other tool here uses.
    """
    if not text:
        return None
    parts = text.replace(" ", "").split(":")
    if len(parts) != 2:
        raise SystemExit(f"--range wants LO:HI, got {text!r}")
    try:
        lo, hi = (int(part, 0) for part in parts)
    except ValueError:
        raise SystemExit(f"--range wants two integers, got {text!r}")
    if hi <= lo:
        raise SystemExit(f"--range HI must exceed LO, got {text!r}")
    return lo, hi


def rows_in_range(rows, window):
    """The differing-word rows whose TARGET offset falls in the window."""
    if window is None:
        return rows
    lo, hi = window
    return [row for row in rows if lo <= row[0] < hi]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("unit")
    ap.add_argument("function")
    ap.add_argument("--list", action="store_true",
                    help="print every differing word, not just the count")
    ap.add_argument("--decode", action="store_true",
                    help="print the per-word CLASS column (REGFIELD-ONLY /"
                         " IMMEDIATE / BRANCH / OPCODE) beside every"
                         " differing word. The DECODE summary line prints"
                         " unconditionally; this adds the rows")
    ap.add_argument("--by-region", action="store_true",
                    help="bucket the differing words into the --ops cluster"
                         " partition, so a residual DECOMPOSITION claim is"
                         " falsifiable in one command")
    ap.add_argument("--range", dest="window", metavar="0xA:0xB",
                    help="restrict the listing and the SECOND count to this"
                         " TARGET byte-offset window (the offsets --ops and"
                         " fnasm print). The whole-function count is always"
                         " printed too and is the only one that decides"
                         " postprocessor candidacy")
    args = ap.parse_args()
    window = parse_range(args.window)
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
    windowed = rows_in_range(rows, window)
    if window is not None:
        print(f"  IN RANGE +{window[0]:#07x}-{window[1]:#07x}:"
              f" {len(windowed)} of {len(rows)} differing word(s)"
              f" ({100.0 * len(windowed) / len(rows):.1f}%)"
              if rows else
              f"  IN RANGE +{window[0]:#07x}-{window[1]:#07x}: 0 differing"
              " words (the whole function has none)")
        print("  A WINDOWED COUNT IS NOT THE FUNCTION'S RESIDUAL: the RAW"
              f" whole-function count is {len(rows)}, and that is the number"
              " that decides postprocessor candidacy (AGENTS.md: any brief"
              " inheriting a residual signature quotes the raw differing-word"
              " count).")
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
    reloc_index = reloc_types_by_index(unit, args.function, insns)
    counts = decode_counts(rows, reloc_index)
    if rows:
        # THE CLASS LINE IS QUALIFIED BY THE DECODE (run-48 item 4). It reads
        # the MNEMONIC divergence alone, so a function with zero divergence is
        # called RECOLOR — "cure is a register-assignment question" — even
        # when some of its words are branch displacements or literals that no
        # register-assignment change can reach.
        recolourable = all(
            counts[name] == 0 for name in DECODE_CLASSES
            if name not in REACHABLE_DECODE_CLASSES
            and name not in LINKER_OWNED_DECODE_CLASSES)
        if mnem == 0 and recolourable:
            print("  CLASS: RECOLOR — streams index-aligned, only register"
                  " fields differ. Cure is a register-assignment question"
                  " (declaration order, width, type).")
        elif mnem == 0:
            print("  CLASS: RECOLOR-SHAPED BUT NOT RECOLOURABLE — the streams"
                  " are index-aligned (0 mnemonic divergence), which is what"
                  " the old CLASS line read, but"
                  f" {counts['BRANCH']} BRANCH and {counts['IMMEDIATE']}"
                  " IMMEDIATE word(s) sit outside every register slot. Those"
                  " are not a register-assignment question and no shipped"
                  " rule reaches them — read the DECODE line below before"
                  " sizing any recolor work.")
        else:
            print(f"  CLASS: SCHEDULE-REORDER — {mnem} instruction(s) differ"
                  " in MNEMONIC at the same index, so the streams are not"
                  " aligned. Cure is an emission-order question (statement"
                  " order, the permutation rule class), NOT a recolor. The"
                  " word count alone cannot tell these two apart.")
        print(decode_summary(counts, len(rows)))
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
    if args.list or args.decode:
        # The per-word CLASS column, beside the words themselves. `--list`
        # carries it too: a lane that asked for every differing word wants
        # the one fact that decides what can close them.
        for off, ow, tw, klass in decode_rows(windowed, reloc_index):
            print(f"  +{off:#06x}  O {ow:08x}  T {tw:08x}  {klass}")
    # EXIT 0 ON A SUCCESSFUL MEASUREMENT (run-42 item 2). This used to
    # return 1 whenever the function had any differing word, which is what
    # a normal call to a residual-measuring tool looks like: every ordinary
    # invocation read as a failure, so a `&&` chain or a CI step around it
    # stopped after the first function that had a residual — the thing the
    # tool exists to find. Failure means the measurement did not happen,
    # and word_streams already exits nonzero with a message for a missing
    # object or a count-asymmetric function.
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
