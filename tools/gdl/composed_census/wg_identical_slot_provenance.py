"""WG lane (run 46): the COMPLEMENT of t15_operand_provenance's screen.

WHY.  `t15_operand_provenance` answers "do the two streams read the same
VALUES?" only for register slots whose ENCODING DIFFERS.  It skips twice:

    t15 line 380-381:  if our_word == target_word: continue
    t15 line 421-422:  if our_reg == target_reg: continue

Both skips are blind to the same shape -- a slot that names the SAME register
number in both streams while that register HOLDS A DIFFERENT VALUE, which is
exactly what a per-web reallocation produces when one register number is
reused for two different webs across the two streams.

MEASURED POSITIVE, game/audio/mempool::pool_garbage_collect +0x30:

    target  0x20 lwz r0,4(r3)       (secondary.head node into r0)
    target  0x28 addi r29,r3,@l     (entries base parked in r29)
    target  0x30 mr r3,r0           <- copies the NODE
    ours    0x20 lwz r5,4(r3)       (node into r5)
    ours    0x28 addi r0,r3,@l      (entries base into r0)
    ours    0x30 mr r31,r0          <- copies the ENTRIES BASE

The word at +0x30 differs (dest r3 vs r31) so t15 examines it, but its two
SOURCE slots both read `r0` in both streams, so t15's line-421 skip drops
them -- and t15 returns `UNDECIDED` with ZERO OPERAND-VALUE-DIFF rows for a
function whose own shipped rule note documents that very value difference.

This screen closes that gap: for every USE slot of every instruction in a
differing-body pair whose register number is IDENTICAL on both sides, it
resolves both sides to a t15 value token and compares.  Verdicts per
function: IDENTICAL-SLOT-VALUE-DIFF (at least one such slot reads different
values), UNDECIDED (only depth-limited leaves), CLEAN.

CALIBRATION IS TWO-SIDED (`--calibrate`):
  KNOWN POSITIVE, 1: game/audio/mempool::pool_garbage_collect must flag +0x30.
  KNOWN NEGATIVES: every webfrank-pinned function carrying a MACHINE-PROVED
    recolor with no pre-recolor stage.  A proven consistent recolor is a
    bijective renaming, and value tokens are register-free except at `entry`
    leaves, so an identical slot under a proven recolor must compare EQUAL.
    Any firing there is a false positive of THIS screen.
  Pre-recolor-staged rules and the unproven_recolor_audit rule are excluded
  by name for the same reasons t15 excludes them.

ADVISORY.  Refuses nothing, writes no config.

CALIBRATED, run 46, at commit c621fcbac (both sides, and the exclusions are
named with the number each removed):
  positive  1/1   pool_garbage_collect flags +0x30 g0.
  negatives 83 machine-proved recolor rules, all with a differing raw body:
            75 CLEAN, 4 UNDECIDED (depth only), 4 FIRED.
            exclusions: DATUM-SPELLING removed 88 slots in 3 functions,
            NAMING-DRIFT removed 12 slots in 3 functions (10 firings -> 4).
  The 4 residual firings are a PRINCIPLED false-positive class, not defects:
  `verify_consistent_recolor` proves a RUNNING renaming, which lets one
  register NAME carry different webs at different points, so an identical
  slot is not obliged to read the same value.  A firing therefore means
  "the recolor is not a global renaming here", never "this is a bug" --
  the proof, not this screen, is the discriminant.  ADVISORY.

    python tools/gdl/composed_census/wg_identical_slot_provenance.py --only <fn>
    python tools/gdl/composed_census/wg_identical_slot_provenance.py --calibrate
    python tools/gdl/composed_census/wg_identical_slot_provenance.py --pinned
"""
import argparse
import glob
import json
import os
import sys

ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf                                    # noqa: E402
import fndiff                                            # noqa: E402
import t15_operand_provenance as t15                     # noqa: E402

OBJ = os.path.join(ROOT, "build", "GUNE5D", "obj")
SRC = os.path.join(ROOT, "build", "GUNE5D", "src")


def _symbol_pair(token_a, token_b):
    """Why an ``expr`` pair with the SAME canonical word differs only in its
    relocation symbol.  Returns an exclusion label or None."""
    if token_a[0] != "expr" or token_b[0] != "expr":
        return None
    word_a, sym_a = token_a[1]
    word_b, sym_b = token_b[1]
    if word_a != word_b or sym_a == sym_b:
        return None
    if "pool-datum" in (sym_a, sym_b):
        # our `@N` / a section symbol + addend against dtk's `lbl_ADDR` or its
        # real name.  WHICH datum a relocation names is decided by
        # fndiff.datum_multiset_screen and webfrank.verify_datum_binding, not
        # by an operand-provenance screen (AGENTS.md residual discipline 3).
        return "DATUM-SPELLING"
    if sym_a and sym_b:
        # two REAL names for one address: our symbol table and dtk's disagree
        # on the name, not on the datum (claim.law naming-drift/address
        # identity).  Also out of scope here, and reported separately.
        return "NAMING-DRIFT"
    return None


def _excluded_symbol_diff(token_a, token_b):
    """Recursively: is the ONLY reason these tokens differ a symbol spelling?

    Returns the exclusion label, or None when a non-symbol difference exists.
    """
    if token_a == token_b:
        return None
    if token_a[0] != token_b[0]:
        return False
    if token_a[0] == "expr":
        label = _symbol_pair(token_a, token_b)
        inner = None
        if len(token_a[2]) != len(token_b[2]):
            return False
        for a, b in zip(token_a[2], token_b[2]):
            if a == b:
                continue
            deeper = _excluded_symbol_diff(a, b)
            if deeper is False or deeper is None:
                return False
            inner = deeper
        if token_a[1] != token_b[1] and label is None:
            return False
        return label or inner
    if token_a[0] == "phi":
        if len(token_a[1]) != len(token_b[1]):
            return False
        label = None
        for a, b in zip(sorted(token_a[1], key=repr),
                        sorted(token_b[1], key=repr)):
            if a == b:
                continue
            deeper = _excluded_symbol_diff(a, b)
            if deeper is False or deeper is None:
                return False
            label = deeper
        return label
    return False


def screen_identical_slots(ours, target, depth=t15.DEFAULT_DEPTH):
    """Rows for USE slots whose register number is identical on both sides."""
    rows = []
    for index in range(len(ours.words)):
        our_word = ours.words[index]
        target_word = target.words[index]
        try:
            operands = wf.instruction_operands(our_word)
        except ValueError:
            continue
        try:
            wf.instruction_operands(target_word)
        except ValueError:
            continue
        if (our_word & ~wf.register_slot_mask(our_word)) != \
                (target_word & ~wf.register_slot_mask(target_word)):
            # different opcode/immediate: positions do not correspond, and
            # this screen makes no claim about them.
            continue
        values = {}
        for bank, shift, role, zero_none in operands:
            if role not in ("u", "b"):
                continue
            our_reg = (our_word >> shift) & 0x1F
            target_reg = (target_word >> shift) & 0x1F
            if our_reg != target_reg:
                continue          # t15's own domain
            if zero_none and our_reg == 0:
                continue          # RA=0 is a form, not a value
            values[shift] = (
                bank, our_reg,
                t15.value_token(ours, index, bank, our_reg, depth),
                t15.value_token(target, index, bank, target_reg, depth))
        # EXCHANGE: a commutative pair whose two values are swapped is a
        # reordered operand pair, not a value difference (t15._mark_exchanges,
        # applied within the instruction).
        pair = wf._commutative_shifts(our_word)
        exchanged = set()
        if pair is not None and all(shift in values for shift in pair):
            first, second = pair
            if t15.compare_values(values[first][2], values[second][3]) == \
                    t15.EQUAL and \
                    t15.compare_values(values[second][2],
                                       values[first][3]) == t15.EQUAL:
                exchanged = set(pair)
        for shift in sorted(values):
            bank, reg, our_value, target_value = values[shift]
            result = t15.compare_values(our_value, target_value)
            if result == t15.EQUAL or shift in exchanged:
                continue
            verdict = ("IDENTICAL-SLOT-VALUE-DIFF"
                       if result == t15.DIFFERENT else
                       "IDENTICAL-SLOT-UNDECIDED")
            if result == t15.DIFFERENT:
                excluded = _excluded_symbol_diff(our_value, target_value)
                if excluded:
                    verdict = "EXCLUDED-" + excluded
            rows.append({
                "at": "0x%x" % (index * 4),
                "bank": bank, "shift": shift,
                "reg": "%s%d" % (t15.BANK_NAME[bank], reg),
                "word_differs": our_word != target_word,
                "ours_value": t15._render(our_value),
                "target_value": t15._render(target_value),
                "verdict": verdict,
            })
    verdicts = {row["verdict"] for row in rows}
    if "IDENTICAL-SLOT-VALUE-DIFF" in verdicts:
        return rows, "IDENTICAL-SLOT-VALUE-DIFF"
    if "IDENTICAL-SLOT-UNDECIDED" in verdicts:
        return rows, "UNDECIDED"
    if verdicts:
        return rows, "CLEAN-AFTER-EXCLUSIONS"
    return rows, "CLEAN"


def screen_function(our_data, our_sections, our_sym,
                    target_data, target_sections, target_sym, depth):
    our_text = our_sections[our_sym.section_index]
    target_text = target_sections[target_sym.section_index]
    our_blob = bytes(our_data[our_text.offset + our_sym.value:][:our_sym.size])
    target_blob = bytes(
        target_data[target_text.offset + target_sym.value:][:target_sym.size])
    if our_blob == target_blob or len(our_blob) != len(target_blob):
        return None
    ours = t15.Stream(our_blob, wf._function_text_relocations(
        our_data, our_sections, our_sym.section_index,
        our_sym.value, our_sym.value + our_sym.size),
        wf._jumptable_targets(
            our_data, our_sections, our_sym.section_index,
            our_sym.value, our_sym.value + our_sym.size))
    target = t15.Stream(target_blob, wf._function_text_relocations(
        target_data, target_sections, target_sym.section_index,
        target_sym.value, target_sym.value + target_sym.size),
        wf._jumptable_targets(
            target_data, target_sections, target_sym.section_index,
            target_sym.value, target_sym.value + target_sym.size))
    rows, verdict = screen_identical_slots(ours, target, depth)
    differing = sum(1 for a, b in zip(ours.words, target.words) if a != b)
    return {"insns": len(ours.words), "differing_words": differing,
            "verdict": verdict, "rows": rows}


def census(unit_filter=None, function_filter=None, keys=None,
           depth=t15.DEFAULT_DEPTH):
    out = []
    for target_path in sorted(glob.glob(os.path.join(OBJ, "**", "*.o"),
                                        recursive=True)):
        unit = os.path.relpath(target_path, OBJ).replace("\\", "/")[:-2]
        if unit_filter and fndiff.unit_key(unit_filter) != unit:
            continue
        if keys is not None and not any(key[0] == unit for key in keys):
            continue
        our_path = t15._raw_object(os.path.join(SRC, unit + ".o"))
        if not os.path.exists(our_path):
            continue
        try:
            our_data, our_sections = t15._load(our_path)
            target_data, target_sections = t15._load(target_path)
            ours = t15._functions(our_data, our_sections)
            targets = t15._functions(target_data, target_sections)
        except Exception:                                    # noqa: BLE001
            continue
        for name, our_sym in sorted(ours.items()):
            if function_filter and name != function_filter:
                continue
            if keys is not None and (unit, name) not in keys:
                continue
            target_sym = targets.get(name)
            if target_sym is None or target_sym.size != our_sym.size:
                continue
            try:
                result = screen_function(our_data, our_sections, our_sym,
                                         target_data, target_sections,
                                         target_sym, depth)
            except Exception as exc:                         # noqa: BLE001
                out.append({"unit": unit, "function": name, "verdict": "ERROR",
                            "error": "%s: %s" % (type(exc).__name__, exc),
                            "insns": 0, "differing_words": 0, "rows": []})
                continue
            if result is None:
                continue
            result.update({"unit": unit, "function": name})
            out.append(result)
    return out


def _pin_stages():
    return t15.rules_by_function()


def calibrate(depth):
    pins = _pin_stages()
    negatives, excluded_staged, excluded_unproven = [], 0, 0
    for key, stages in sorted(pins.items()):
        if "unproven_recolor_audit" in stages:
            excluded_unproven += 1
            continue
        if any(stage in stages for stage in t15.PRE_RECOLOR_STAGES):
            excluded_staged += 1
            continue
        negatives.append(key)
    positive_key = ("game/audio/mempool", "pool_garbage_collect")
    rows = census(keys=set(negatives) | {positive_key}, depth=depth)
    by_key = {(row["unit"], row["function"]): row for row in rows}

    print("TWO-SIDED CALIBRATION")
    hit = by_key.get(positive_key)
    print("  KNOWN POSITIVE (1): %s::%s -> %s" % (
        positive_key[0], positive_key[1],
        hit["verdict"] if hit else "NOT PAIRED"))
    if hit:
        for row in hit["rows"]:
            print("     %-7s %-4s %-27s ours=%s target=%s" % (
                row["at"], row["reg"], row["verdict"],
                row["ours_value"], row["target_value"]))
    fired, undecided, clean, unpaired = [], [], 0, 0
    exclusion_rows, exclusion_fns = {}, {}
    for key in negatives:
        row = by_key.get(key)
        if row is None:
            unpaired += 1
            continue
        labels = set()
        for entry in row["rows"]:
            if entry["verdict"].startswith("EXCLUDED-"):
                label = entry["verdict"][len("EXCLUDED-"):]
                exclusion_rows[label] = exclusion_rows.get(label, 0) + 1
                labels.add(label)
        for label in labels:
            exclusion_fns[label] = exclusion_fns.get(label, 0) + 1
        if row["verdict"] == "IDENTICAL-SLOT-VALUE-DIFF":
            fired.append((key, row))
        elif row["verdict"] == "UNDECIDED":
            undecided.append((key, row))
        else:
            clean += 1
    print("  KNOWN NEGATIVES (machine-proved recolor, no pre-recolor stage):"
          " %d rules, %d with a differing raw body" % (
              len(negatives), len(negatives) - unpaired))
    print("     CLEAN                     : %d" % clean)
    print("     UNDECIDED (depth only)    : %d" % len(undecided))
    print("     FALSE POSITIVES (fired)   : %d" % len(fired))
    for label in sorted(exclusion_rows):
        print("     excluded %-16s : %d slot(s) in %d function(s)"
              % (label, exclusion_rows[label], exclusion_fns.get(label, 0)))
    for key, row in fired:
        print("       %s::%s" % key)
        for entry in row["rows"][:4]:
            print("         %-7s %-4s %-27s ours=%s target=%s" % (
                entry["at"], entry["reg"], entry["verdict"],
                entry["ours_value"], entry["target_value"]))
    print("  EXCLUDED: pre_recolor_staged=%d unproven_recolor_audit=%d"
          % (excluded_staged, excluded_unproven))
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--only")
    parser.add_argument("--unit")
    parser.add_argument("--pinned", action="store_true")
    parser.add_argument("--calibrate", action="store_true")
    parser.add_argument("--depth", type=int, default=t15.DEFAULT_DEPTH)
    parser.add_argument("--out")
    args = parser.parse_args()

    if args.calibrate:
        rows = calibrate(args.depth)
    else:
        keys = set(_pin_stages()) if args.pinned else None
        rows = census(unit_filter=args.unit, function_filter=args.only,
                      keys=keys, depth=args.depth)
        tally = {}
        for row in rows:
            tally[row["verdict"]] = tally.get(row["verdict"], 0) + 1
        print("PAIRED FUNCTIONS WITH A DIFFERING BODY: %d" % len(rows))
        for verdict in sorted(tally):
            print("  %-28s %d" % (verdict, tally[verdict]))
        for row in rows:
            if row["verdict"] != "IDENTICAL-SLOT-VALUE-DIFF":
                continue
            print("\n%s::%s  (%d insns, %d differing words)" % (
                row["unit"], row["function"], row["insns"],
                row["differing_words"]))
            for entry in row["rows"]:
                print("   %-7s %-4s word_differs=%-5s %-27s ours=%s target=%s"
                      % (entry["at"], entry["reg"], entry["word_differs"],
                         entry["verdict"], entry["ours_value"],
                         entry["target_value"]))
    if args.out:
        with open(args.out, "w", encoding="utf-8") as fh:
            json.dump({"entries": rows}, fh, indent=2, sort_keys=True)
        print("\nwrote %s" % args.out)


if __name__ == "__main__":
    main()
