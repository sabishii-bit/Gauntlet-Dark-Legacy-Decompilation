#!/usr/bin/env python3
"""CV lane: the SCALAR-DATUM VALUE-SET SWEEP, image-wide.

Promoted to the tool corpus by the run-50 TOOL lane from the algorithm
recorded verbatim in
attempt.CV_critternewinst-error-string-carried-a-newline-retail-does-not
.20260903.v1 (`hypothesis.statement`), whose working copy was
CV_scratch/cv_scalar_datum_sweep.py in a retired lane worktree.  CV could
not promote it because tools/gdl was another lane's claimed scope that run.

    python tools/gdl/composed_census/cv_scalar_datum_sweep.py
    python tools/gdl/composed_census/cv_scalar_datum_sweep.py --cap 16
    python tools/gdl/composed_census/cv_scalar_datum_sweep.py \
        --out build/GUNE5D/cv_scalar_datum_sweep.json

Run from the repository root after a completed `ninja`.

WHY IT EXISTS.  It is the only screen in the project that sees a WRONG
CONSTANT inside a BYTE-IDENTICAL function: `real` is 0, fuzzy is 100, every
other arbiter is silent, and the function still loads a value retail never
loads (claim.law.SL_pool-constant-errors-are-score-invisible.20260901.v1).
Both of run 49's real defects -- CritterNewInst's error string carrying a
`\\n` retail does not, and fn_8005A868's placeholder name entry -- were
found here and were invisible everywhere else.

THE SHAPE THAT MEASURED CLEAN, and every clause of it is load-bearing:

  SCALARS ONLY     `fndiff.datum_key` B: keys whose datum is at most --cap
                   bytes (default 8).  Lifting the cap took the image-wide
                   finding set from 7 functions to 21 and introduced the
                   SECTION-SYMBOL BASE false-positive class -- where the
                   target relocates against a named string symbol plus a
                   displacement and we relocate against the .rodata section
                   alias plus a different displacement, both landing on the
                   same storage (measured: do_got_it_8007FC80, a constant
                   delta of 1328 at all five sites).  Long datums are where
                   that class lives, so the cap excludes it structurally
                   rather than by a filter.
  SETS, NOT        a COUNT difference is a CSE fact, not a value fact:
  MULTISETS        camera_mode_follow loads 0.6 three times in the target
                   and once in ours -- same value, same uses.
  PREFIX FILTER    differences are cancelled through
                   `fndiff._datum_prefix_equal`: dtk names a whole
                   contiguous .rodata run with one symbol while our compiler
                   emits each literal separately, so the shorter entry being
                   a PREFIX of the longer is one datum at two granularities.
  RANK BY `real`   ASCENDING.  A row at real 0 is a defect inside a
                   byte-identical function -- the whole point.  A row at
                   high `real` is usually an ordinary reconstruction gap.

A row is a CANDIDATE, never a verdict: verify each against the retail split
listing (build/GUNE5D/asm/...) or the aligned `fnasm --diff` view before
editing.  Run 49's seven capped rows were 2 real defects, 2 base-symbol
granularity proved by arithmetic, 1 phantom FP argument (a matching lever,
not a bug) and 2 ordinary reconstruction gaps.
"""
import argparse
import difflib
import glob
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

DEFAULT_CAP = 8


def scalar_keys(lines, local, cap):
    """The SET of B: datum keys in one function whose datum is <= cap bytes.

    Multiplicity is deliberately dropped (see the module docstring): a count
    difference is a CSE fact.  The value alongside each key is the decoded
    bytes, kept for the prefix filter and for rendering.

    THE CAP IS APPLIED AFTER KEYING, NEVER AS `datum_key`'s OWN CAP.  That
    parameter TRUNCATES the datum to its first `cap` bytes, so passing 8
    turns every string into an 8-byte "scalar": `'trbo_ful'` becomes
    `0x7472626F5F66756C` and reads as an f64.  Calibrated against
    attempt.CV_critternewinst-error-string-carried-a-newline-retail-does-not
    .20260903.v1, which reports 7 findings from 56 set differences at
    c8a28c3bb: the truncating form measured 25 findings from 68 differences
    at run-50 HEAD and every extra row was a string -- do_got_it,
    create_player_blits, setup_player_display, screen_limitation and
    write_health_and_items, i.e. exactly the section-alias class the scalar
    cap exists to exclude.  Key at the full DATUM_PREFIX_BYTES, then filter
    on the resulting datum's real length.
    """
    keys = {}
    for symbol in fndiff.datum_relocs(lines):
        key, _size = fndiff.datum_key(symbol, local, fndiff.DATUM_PREFIX_BYTES)
        if not key.startswith("B:"):
            continue
        blob = bytes.fromhex(key[2:])
        if len(blob) > cap:
            continue
        keys[key] = blob
    return keys


def cancel_prefixes(only_target, only_ours):
    """Drop every pair where one datum is a PREFIX of the other."""
    kept_target, kept_ours = dict(only_target), dict(only_ours)
    for key, mine in list(kept_ours.items()):
        for other, theirs in list(kept_target.items()):
            if fndiff._datum_prefix_equal(theirs, mine):
                del kept_target[other]
                del kept_ours[key]
                break
    return kept_target, kept_ours


def function_real(target_lines, ours_lines):
    """`fndiff --count`'s `real`, computed exactly as its main() does."""
    diff = [line for line in difflib.unified_diff(
        target_lines, ours_lines, lineterm="", n=0)
        if line[:1] in "+-" and line[:3] not in ("+++", "---")]
    return fndiff.count_real(diff)


def paired_units():
    """[(unit, target_object, ours_object)] for every unit built both ways."""
    pattern = os.path.join(ROOT, "build", "GUNE5D", "obj", "**", "*.o")
    out = []
    for target_object in sorted(glob.glob(pattern, recursive=True)):
        rel = os.path.relpath(
            target_object, os.path.join(ROOT, "build", "GUNE5D", "obj"))
        unit = rel[:-2].replace(os.sep, "/")
        ours_object = os.path.join(ROOT, "build", "GUNE5D", "src", unit + ".o")
        if os.path.exists(ours_object):
            out.append((unit, target_object, ours_object))
    return out


def sweep(cap=DEFAULT_CAP, raw=False):
    tally = Counter()
    rows = []
    for unit, target_object, ours_object in paired_units():
        if raw:
            body = os.path.join(os.path.dirname(ours_object), ".postprocess",
                                "body", os.path.basename(ours_object))
            if os.path.isfile(body):
                ours_object = body
                tally["raw-object-used"] += 1
        try:
            tfns = fndiff.parse(target_object)
            ofns = fndiff.parse(ours_object)
            target_local = fndiff.object_datum_table(target_object)
            ours_local = fndiff.object_datum_table(ours_object)
        except Exception:                                      # noqa: BLE001
            tally["parse-failed"] += 1
            continue
        tally["units"] += 1
        for function, target_lines in tfns.items():
            ours_lines = ofns.get(function)
            if ours_lines is None:
                continue
            tally["functions"] += 1
            tkeys = scalar_keys(target_lines, target_local, cap)
            okeys = scalar_keys(ours_lines, ours_local, cap)
            only_target = {k: v for k, v in tkeys.items() if k not in okeys}
            only_ours = {k: v for k, v in okeys.items() if k not in tkeys}
            if not only_target and not only_ours:
                continue
            tally["raw-rows"] += 1
            only_target, only_ours = cancel_prefixes(only_target, only_ours)
            if not only_target and not only_ours:
                tally["prefix-cancelled"] += 1
                continue
            tally["findings"] += 1
            rows.append({
                "unit": unit,
                "function": function,
                "real": function_real(target_lines, ours_lines),
                "target_only": [fndiff._render_value(v)
                                for v in only_target.values()],
                "ours_only": [fndiff._render_value(v)
                              for v in only_ours.values()],
            })
    rows.sort(key=lambda row: (row["real"], row["unit"], row["function"]))
    return {"cap": cap, "raw": raw, "tally": dict(tally), "rows": rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--cap", type=int, default=DEFAULT_CAP,
                        help="max datum size in bytes (default 8; lifting it "
                             "admits the section-alias false-positive class)")
    parser.add_argument("--raw", action="store_true",
                        help="read the pre-postprocess body object, so a "
                             "webfrank rule's relocations cannot be mistaken "
                             "for a source-value defect")
    parser.add_argument("--limit", type=int, default=40)
    parser.add_argument("--out", default=None)
    arguments = parser.parse_args()

    result = sweep(arguments.cap, arguments.raw)
    tally = result["tally"]
    print(f"SCALAR-DATUM VALUE-SET SWEEP (cap {result['cap']} bytes"
          + (", RAW body objects" if result["raw"] else "") + ")")
    print(f"  units {tally.get('units', 0)}"
          f"  paired functions {tally.get('functions', 0)}")
    print(f"  set differences {tally.get('raw-rows', 0)}"
          f"  -> prefix-cancelled {tally.get('prefix-cancelled', 0)}"
          f"  -> findings {tally.get('findings', 0)}")
    if tally.get("parse-failed"):
        print(f"  parse-failed units {tally['parse-failed']}")
    print("\nranked by `real` ASCENDING (real 0 = a wrong constant inside a "
          "byte-identical function):")
    for row in result["rows"][:arguments.limit]:
        print(f"  real {row['real']:>5}  {row['unit']}::{row['function']}")
        for value in row["target_only"]:
            print(f"      target-only  {value}")
        for value in row["ours_only"]:
            print(f"      ours-only    {value}")
    if len(result["rows"]) > arguments.limit:
        print(f"  ... {len(result['rows']) - arguments.limit} more "
              f"(raise --limit or use --out)")
    if not result["rows"]:
        print("  (none)")
    if arguments.out:
        path = arguments.out
        if not os.path.isabs(path):
            path = os.path.join(ROOT, path)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(result, handle, indent=1)
        print(f"wrote {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
