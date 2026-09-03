#!/usr/bin/env python3
"""WF lane (run 48): the ORDERED-datum screen, the third datum comparison.

T17's next-lane hypothesis, executed (attempt.T17_tool-queue-17-ten-items-
one-refuted-premise-and-a-flip-precondition.20260903.v1).  The datum
MULTISET is blind to a transposition by construction, and
es_named_reloc_census's row-set LABEL PERMUTATION test only looks at its own
differing rows, so one unrelated row destroys the permutation verdict for a
pair that IS transposed.  This screen compares each function's whole ORDERED
sequence of datum keys, target against ours, and then decides each ordered
mismatch on the REGISTER FIELD — which is what separates a transposition
from a schedule reorder.

    python tools/gdl/composed_census/wf_ordered_datum_screen.py
        [--census build/GUNE5D/es_named_census.json] [--disposition CANDIDATE]
    python tools/gdl/composed_census/wf_ordered_datum_screen.py --sweep
    python tools/gdl/composed_census/wf_ordered_datum_screen.py \
        --unit game/enemy/enemy --function move_logic00

Run from the repository root, after a completed `ninja`.

THE DECIDING COMPARISON.  For an ordered mismatch at index i whose two datum
keys also appear MIRRORED at some index j:

  REORDER     the register at i in the target reappears at j in ours and
              vice versa -- each datum still reaches the SAME register and
              the two words were merely emitted in the other order.  A
              schedule fact, not a datum fact.
  DATUM-SWAP  the SAME register, at the SAME position, under the SAME
              mnemonic, receives a DIFFERENT datum -- two registers hold
              each other's data.  THE defect.

Three further exclusions, each measured (the count each removes is in this
lane's record):

  CONVERSION-MAGIC  a mirror involving 0x4330000000000000 /
                    0x4330000080000000, MWCC's own int<->float conversion
                    constants, whose position is a codegen fact.
  SHIFT             our key at i is the target's at i-d for one small d --
                    the signature of a displaced relocation earlier in the
                    stream, i.e. this screen's own pairing artifact.
  SPLIT-FORM        mirrored under two DIFFERENT mnemonics (an @ha/@lo
                    materialisation interleave): unpaired evidence.

Per function it prints one of:

  BENIGN        no ordered mismatch survives the exclusions.
  DEFECT        at least one DATUM-SWAP row.
  UNDECIDABLE   the multisets differ (read fndiff.datum_multiset_screen
                first), the relocation counts differ, or unpaired /
                split-form / address-only mismatches remain.

TWO-SIDED CALIBRATION at 5f9ef72ba, against es_named_reloc_census's own
verdicts (its 8 TRANSPOSED are the positive control, its 7 WRONG-DATUM the
negative one), and image-wide for the base rate:

  positive  8 TRANSPOSED   -> 6 DEFECT, 2 UNDECIDABLE, 0 BENIGN
  negative  7 WRONG-DATUM  -> 0 DEFECT, 7 UNDECIDABLE, 0 BENIGN
  the 11 CANDIDATE rows    -> 1 DEFECT, 3 BENIGN, 7 UNDECIDABLE
  base rate 3032 paired    -> 12 DEFECT (0.40%), 2845 BENIGN, 175 UNDECIDABLE

The 12 include game/enemy/enemy::move_logic00, the swapped pi/2pi that
fndiff's own datum_screen_from_lines docstring names as the transposition
its multiset cannot see -- an independent positive control this screen was
not fitted to, and one es_named_reloc_census never nominated.
"""
import argparse
import json
import os
import sys
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))

import fndiff  # noqa: E402


# MWCC emits these two doubles itself for every int<->float conversion
# (0x43300000_00000000 unsigned, 0x43300000_80000000 signed).  WHERE they
# land in the relocation stream is a codegen fact — the position of the
# conversion — never a source datum fact, so an "exchange" that involves
# one is not evidence of a transposed operand.  Excluding them is the
# measured difference between an EXCHANGE class of 44 functions and one of
# 33 (see the sweep numbers in this lane's record).
CONVERSION_MAGIC = {
    "B:4330000000000000",
    "B:4330000080000000",
}


def mnemonic(text):
    return text.split()[0] if text.split() else ""


def register_field(text):
    """The instruction's first register operand.

    For a load or an `lis`/`addi` that is the DESTINATION; for a store it is
    the VALUE written.  Either way it is the slot the datum occupies, which
    is what decides a transposition from a reorder.
    """
    parts = text.split()
    if len(parts) < 2:
        return ""
    return parts[1].split(",")[0]


def ordered_symbols(lines, with_offsets=False):
    """The function's relocation symbols in body order (with addends)."""
    out, offsets, last = [], [], ""
    for line in lines:
        if line.startswith("    "):
            parts = line.strip().split(maxsplit=1)
            if len(parts) > 1:
                out.append(parts[1].strip())
                offsets.append(last.split(":")[0].strip())
        else:
            last = line
    return (out, offsets) if with_offsets else out


def classify(tkeys, okeys, tinsn, oinsn, tsyms=None, osyms=None,
             words_equal=False):
    """The verdict, over four parallel sequences and nothing else.

    IMPORTABLE CORE: classify, register_field, mnemonic, ordered_symbols —
    pure functions over parsed lists; no build, no printing, no object
    reads.  `screen` below is the object-reading wrapper.
    """
    tsyms = list(tsyms if tsyms is not None else tkeys)
    osyms = list(osyms if osyms is not None else okeys)
    multiset_equal = Counter(tkeys) == Counter(okeys)
    result = {"relocs": (len(tkeys), len(okeys)),
              "words_equal": words_equal,
              "multiset_equal": multiset_equal}
    if len(tkeys) != len(okeys):
        result.update(verdict="UNDECIDABLE",
                      why=f"relocation counts differ "
                          f"({len(tkeys)} target vs {len(okeys)} ours)")
        return result
    return _decide(result, tkeys, okeys, tinsn, oinsn, tsyms, osyms,
                   multiset_equal, words_equal)


def screen(unit, function):
    unit = fndiff.unit_key(unit)
    target_object = os.path.join(
        ROOT, "build", "GUNE5D", "obj", unit + ".o")
    ours_object = os.path.join(
        ROOT, "build", "GUNE5D", "src", unit + ".o")
    if not (os.path.exists(target_object) and os.path.exists(ours_object)):
        return {"verdict": "UNDECIDABLE", "why": "missing object"}
    tfns = fndiff.parse(target_object)
    ofns = fndiff.parse(ours_object)
    if function not in tfns or function not in ofns:
        return {"verdict": "UNDECIDABLE", "why": "function absent"}
    tlines, olines = tfns[function], ofns[function]

    tlocal = fndiff.object_datum_table(target_object)
    olocal = fndiff.object_datum_table(ours_object)
    tsyms, tinsn = ordered_symbols(tlines, with_offsets=True)
    osyms, oinsn = ordered_symbols(olines, with_offsets=True)
    tkeys = [fndiff.datum_key(s, tlocal)[0] for s in tsyms]
    okeys = [fndiff.datum_key(s, olocal)[0] for s in osyms]

    words_equal = (fndiff.instruction_lines(tlines)
                   == fndiff.instruction_lines(olines))
    multiset_equal = Counter(tkeys) == Counter(okeys)

    result = {
        "unit": unit, "function": function,
        "relocs": (len(tkeys), len(okeys)),
        "words_equal": words_equal,
        "multiset_equal": multiset_equal,
    }
    if len(tkeys) != len(okeys):
        result.update(verdict="UNDECIDABLE",
                      why=f"relocation counts differ "
                          f"({len(tkeys)} target vs {len(okeys)} ours)")
        return result
    return _decide(result, tkeys, okeys, tinsn, oinsn, tsyms, osyms,
                   multiset_equal, words_equal)


def _decide(result, tkeys, okeys, tinsn, oinsn, tsyms, osyms,
            multiset_equal, words_equal):
    toffs, ooffs = tinsn, oinsn
    positions = [i for i, (a, b) in enumerate(zip(tkeys, okeys)) if a != b]
    result["ordered_mismatches"] = len(positions)
    if not positions:
        result.update(verdict="BENIGN",
                      why="ordered datum sequences are IDENTICAL — no "
                          "transposition exists anywhere in the function")
        return result

    # The whole-sequence index compare is only sound when the two
    # instruction streams agree.  When they do not, decide each mismatch
    # INDIVIDUALLY with two alignment-free tests, because the sequence-level
    # verdict is dominated by schedule displacement:
    #
    #   EXCHANGE  some other index j carries the MIRROR pair (target j holds
    #             our key at i, and ours at j holds target's key at i).  A
    #             genuine transposition, spread across two sites; this is
    #             exactly the shape T17 says the row-set permutation test
    #             misses when an unrelated row is present.
    #   SHIFT     our key at i equals target's key at i-d for one small,
    #             locally consistent d (and the reverse for target).  The
    #             signature of one extra/absent relocation earlier in the
    #             stream — a positional-pairing artifact, not a datum fact.
    rows = []
    verdicts = Counter()
    for i in positions:
        ka, kb = tkeys[i], okeys[i]
        mirror = [j for j in range(len(tkeys))
                  if j != i and tkeys[j] == kb and okeys[j] == ka]
        shift = None
        for d in (1, -1, 2, -2, 3, -3):
            if 0 <= i - d < len(tkeys) and tkeys[i - d] == kb:
                shift = -d
                break
        # A mirror pair (i, j) whose two INSTRUCTIONS also swap carries the
        # same datum into the same register on both sides — the two words
        # were merely EMITTED in the other order.  That is a schedule fact,
        # not a wrong operand, and it is the dominant population: measured
        # on game/game/gamemain::fn_80057024, target `lfs f1,255.0f` then
        # `lfs f4,0.0f` against ours `lfs f4,0.0f` then `lfs f1,255.0f` —
        # f1 holds 255.0 and f4 holds 0.0 in BOTH streams.
        # THE DECIDING COMPARISON for a mirror pair (i, j): the register
        # field, read at the SAME POSITION on both sides.
        #
        #   crossed   -- the register at i in the target reappears at j in
        #                ours and vice versa: each datum still reaches the
        #                same register, the two words were merely EMITTED in
        #                the other order.  A schedule fact.  (Measured on
        #                gamemain::fn_80057024: target `lfs f1,255.0f` then
        #                `lfs f4,0.0f` against ours `lfs f4,0.0f` then
        #                `lfs f1,255.0f`.)
        #   same-slot -- the SAME register at the SAME position receives a
        #                DIFFERENT datum.  That is the transposition: two
        #                registers hold each other's data.  (Measured on
        #                enemy::generate_enemy: both streams write r27 and
        #                r28 with an `addi` off the same @ha, and the two
        #                addresses are swapped between them.)
        crossed = [j for j in mirror
                   if register_field(tinsn[i]) == register_field(oinsn[j])
                   and register_field(tinsn[j]) == register_field(oinsn[i])]
        same_slot = [j for j in mirror
                     if register_field(tinsn[i]) == register_field(oinsn[i])
                     and mnemonic(tinsn[i]) == mnemonic(oinsn[i])]
        if mirror and (ka in CONVERSION_MAGIC or kb in CONVERSION_MAGIC):
            kind = "CONVERSION-MAGIC"
        elif crossed and not same_slot:
            kind = "REORDER"
        elif same_slot:
            kind = "DATUM-SWAP"
        elif mirror:
            kind = "SPLIT-FORM"
        elif shift is not None:
            kind = "SHIFT"
        elif ka.startswith("A:") and kb.startswith("A:"):
            kind = "ADDRESS-ONLY"
        else:
            kind = "UNPAIRED"
        verdicts[kind] += 1
        rows.append({"index": i, "kind": kind, "target": tsyms[i],
                     "ours": osyms[i], "target_key": ka[:26],
                     "ours_key": kb[:26], "mirror_at": mirror[:2],
                     "shift": shift, "target_at": toffs[i],
                     "ours_at": ooffs[i]})
    result["mismatch_kinds"] = dict(verdicts)
    result["mismatch_rows"] = rows[:12]
    if verdicts["DATUM-SWAP"]:
        result.update(
            verdict="DEFECT",
            why=f"{verdicts['DATUM-SWAP']} DATUM-SWAP row(s): the same "
                f"register, at the same position and under the same "
                f"mnemonic, receives a DIFFERENT datum, and the two datums "
                f"appear in each other's slot — a transposition the datum "
                f"multiset cannot see and the row-set permutation test "
                f"missed")
        return result
    if not multiset_equal:
        # A value delta exists somewhere.  This screen answers the ORDER
        # question only and must never issue a clean bill over one.
        result.update(
            verdict="UNDECIDABLE",
            why="the datum MULTISETS differ, so a value delta exists that "
                "this ordering screen does not adjudicate; read "
                "fndiff.datum_multiset_screen first")
        return result
    if verdicts["SPLIT-FORM"] or verdicts["UNPAIRED"] or \
            verdicts["ADDRESS-ONLY"]:
        result.update(
            verdict="UNDECIDABLE",
            why=f"{verdicts['UNPAIRED']} unpaired, "
                f"{verdicts['SPLIT-FORM']} split-form (mirrored under two "
                f"DIFFERENT mnemonics — an @ha/@lo materialisation "
                f"interleave) and {verdicts['ADDRESS-ONLY']} address-only "
                f"mismatch(es) remain after the reorder, magic and shift "
                f"exclusions"
                + ("" if words_equal else
                   "; the instruction streams also differ, so relocation i "
                   "need not name the same instruction on both sides"))
        return result
    result.update(
        verdict="BENIGN",
        why=f"no mismatch survives the three exclusions: "
            f"{verdicts['REORDER']} crossed-register REORDER(s) (each datum "
            f"reaches the SAME register on both sides), "
            f"{verdicts['CONVERSION-MAGIC']} int<->float conversion magic, "
            f"{verdicts['SHIFT']} local shift(s)")
    return result


def sweep(out_path=None):
    """BASE RATE: run the screen over every paired function in the image.

    A trigger measured only on the rows it should catch is half a
    measurement (AGENTS.md, run-44 two-sided calibration rule).  This is
    the other half: how many functions the EXCHANGE test fires on when
    nothing selected them.
    """
    import glob
    tally = Counter()
    hits = []
    pattern = os.path.join(ROOT, "build", "GUNE5D", "obj", "**", "*.o")
    for target_object in glob.glob(pattern, recursive=True):
        rel = os.path.relpath(
            target_object, os.path.join(ROOT, "build", "GUNE5D", "obj"))
        unit = rel[:-2].replace(os.sep, "/")
        ours_object = os.path.join(
            ROOT, "build", "GUNE5D", "src", unit + ".o")
        if not os.path.exists(ours_object):
            tally["no-ours-object"] += 1
            continue
        try:
            tfns = fndiff.parse(target_object)
            ofns = fndiff.parse(ours_object)
        except Exception:                                  # noqa: BLE001
            tally["parse-failed"] += 1
            continue
        for function in tfns:
            if function not in ofns:
                continue
            result = screen(unit, function)
            tally[result["verdict"]] += 1
            kinds = result.get("mismatch_kinds") or {}
            for kind, count in kinds.items():
                tally["kind:" + kind] += count
            if kinds.get("DATUM-SWAP"):
                tally["has-DATUM-SWAP"] += 1
                hits.append({"unit": unit, "function": function,
                             "exchanges": kinds["DATUM-SWAP"],
                             "verdict": result["verdict"],
                             "words_equal": result.get("words_equal"),
                             "multiset_equal": result.get("multiset_equal")})
    print("SWEEP over every paired function:")
    for key, value in sorted(tally.items()):
        print(f"  {key:<18} {value}")
    print(f"\nfunctions carrying at least one DATUM-SWAP row: {len(hits)}")
    for hit in sorted(hits, key=lambda h: -h["exchanges"])[:40]:
        print(f"  {hit['exchanges']:>3}x  {hit['unit']}::{hit['function']}"
              f"  ({hit['verdict']})")
    if out_path:
        if not os.path.isabs(out_path):
            out_path = os.path.join(ROOT, out_path)
        with open(out_path, "w", encoding="utf-8") as handle:
            json.dump({"tally": dict(tally), "hits": hits}, handle, indent=1)
        print(f"wrote {out_path}")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--sweep", action="store_true",
                        help="image-wide base rate instead of a census read")
    parser.add_argument("--census",
                        default="build/GUNE5D/es_named_census.json",
                        help="es_named_reloc_census.py --out (default: its "
                             "own default path)")
    parser.add_argument("--disposition", default="CANDIDATE",
                        help="census disposition to re-screen, or '*'")
    parser.add_argument("--unit", default=None,
                        help="screen ONE function instead of a census")
    parser.add_argument("--function", default=None)
    parser.add_argument("--out", default=None)
    arguments = parser.parse_args()
    if arguments.sweep:
        return sweep(arguments.out)
    if arguments.unit and arguments.function:
        result = screen(arguments.unit, arguments.function)
        print(f"{result['verdict']:<12} {arguments.unit}::"
              f"{arguments.function}")
        print(f"    {result['why']}")
        for row in result.get("mismatch_rows", []):
            print(f"      [{row['index']:>4}] {row['kind']:<16} "
                  f"T@{row['target_at']} {row['target']} "
                  f"({row['target_key']})  O@{row['ours_at']} {row['ours']} "
                  f"({row['ours_key']})")
        return 0

    path = arguments.census
    if not os.path.isabs(path):
        path = os.path.join(ROOT, path)
    with open(path, encoding="utf-8") as handle:
        census = json.load(handle)
    rows = [entry for entry in census
            if arguments.disposition in ("*", entry["disposition"])]
    print(f"{len(rows)} row(s) with disposition "
          f"{arguments.disposition} in {arguments.census}\n")
    results = []
    tally = Counter()
    for entry in rows:
        result = screen(entry["unit"], entry["function"])
        result["census_rows"] = len(entry.get("rows", []))
        result["census_disposition"] = entry["disposition"]
        results.append(result)
        tally[result["verdict"]] += 1
        print(f"{result['verdict']:<12} {entry['unit']}::{entry['function']}"
              f"  ({result['census_rows']} census row(s); relocs "
              f"{result.get('relocs')}; words_equal="
              f"{result.get('words_equal')}; multiset_equal="
              f"{result.get('multiset_equal')})")
        print(f"    {result['why']}")
        for row in result.get("mismatch_rows", []):
            extra = (f" mirror@{row['mirror_at']}" if row["mirror_at"]
                     else (f" shift{row['shift']:+d}"
                           if row["shift"] is not None else ""))
            print(f"      [{row['index']:>4}] {row['kind']:<12} "
                  f"T@{row['target_at']} {row['target']} "
                  f"({row['target_key']})  "
                  f"O@{row['ours_at']} {row['ours']} "
                  f"({row['ours_key']}){extra}")
        print()
    print("TALLY: " + ", ".join(f"{k} {v}" for k, v in sorted(tally.items())))
    if arguments.out:
        out = arguments.out
        if not os.path.isabs(out):
            out = os.path.join(ROOT, out)
        with open(out, "w", encoding="utf-8") as handle:
            json.dump(results, handle, indent=1)
        print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
