#!/usr/bin/env python3
"""The .text FUNCTION-ORDER screen: the one flip precondition no score sees.

A TU can read 100% of its functions at `real 0`, carry byte-verified data in
every section, and still be UNFLIPPABLE because its .text emits those
functions in the WRONG ORDER. fndiff, probe, defake_gate, objdiff fuzzy and
the progress report all pair functions BY NAME and never compare position, so
a permutation of function groups is invisible to every number the project
computes -- while the linked DOL depends on it completely
(claim.law.MF_every-function-at-real-0-does-not-mean-the-text-order-is-right-
and-no-project-score-sees-it.20260903.v1). game/anim/atree.c carried exactly
this for at least four runs at 36/36 real 0.

Usage (from repo root):
  python tools/gdl/textorder.py game/anim/atree        # one unit
  python tools/gdl/textorder.py game/mb/mb_blit game/ui/select
  python tools/gdl/textorder.py --all                  # sweep every unit pair
  python tools/gdl/textorder.py --all --state NonMatching
  python tools/gdl/textorder.py --all --quiet          # verdict lines only

Exit codes: 0 = every checked unit ORDER-OK; 1 = at least one MISORDERED;
2 = a named unit has no object pair to compare (build it first).

THE DISCRIMINANT, and why it is an INTERSECTION. Compare the sequence of
.text symbol names, ordered by offset, in the dtk split object
(build/GUNE5D/obj/<unit>.o -- the target) against our compiled object
(build/GUNE5D/src/<unit>.o), restricted to the names BOTH objects define.
The restriction is the whole calibration: our objects routinely define static
helpers the linker dead-strips (`ours-only`), and dtk sometimes names a
function we spell differently (`target-only`). Neither occupies a position in
the linked image, so neither can misorder it. Measured at 4726b33ca over the
252 unit pairs in this tree:

    naive full-sequence compare   53 of 200 Matching units flagged  (all FP)
    intersection compare           0 of 200 Matching units flagged
    intersection compare          23 of  52 NonMatching units flagged

The 200 Matching units are the negative set by construction -- they link
byte-identically today, so any hit among them is a false positive. The
unpaired names are printed, never scored, so a reader can see exactly what
the intersection set aside (PPCArch alone has 22 ours-only inline helpers).

IMPORTABLE CORE: text_symbols, common_sequences, misordered_groups,
check_unit -- pure over object paths; no build, no printing, and importing
this module has no side effects.
"""

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import fndiff  # noqa: E402  (unit_key + the memoized objdump reader)

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parent.parent.parent


def text_symbols(objfile):
    """[(offset, size, name)] for the .text function symbols, offset-ordered.

    Section symbols and dtk's `.text`-prefixed local labels are dropped: they
    are not functions and both objects spell them differently.
    """
    rows = []
    for line in fndiff.objdump(objfile, "-t").splitlines():
        parts = line.split()
        if len(parts) < 5:
            continue
        try:
            offset = int(parts[0], 16)
        except ValueError:
            continue
        name, section, size = parts[-1], parts[-3], parts[-2]
        if section != ".text" or name.startswith("."):
            continue
        try:
            size = int(size, 16)
        except ValueError:
            size = 0
        rows.append((offset, size, name))
    rows.sort()
    return rows


def common_sequences(target_rows, ours_rows):
    """(target_seq, ours_seq, target_only, ours_only) over the shared names."""
    target_names = [name for _, _, name in target_rows]
    ours_names = [name for _, _, name in ours_rows]
    shared = set(target_names) & set(ours_names)
    target_only = [n for n in target_names if n not in shared]
    ours_only = [n for n in ours_names if n not in shared]
    return ([n for n in target_names if n in shared],
            [n for n in ours_names if n in shared],
            target_only, ours_only)


def misordered_groups(target_seq, ours_seq):
    """The contiguous runs where the two orders diverge, resyncing at equality.

    Each group is {at, target: [...], ours: [...]} -- exactly the shape the
    law record names for atree (a 2-function transposition, then a 4-function
    rotation, both re-synchronising at the next function).

    NOT difflib. The two sequences are permutations of one another by
    construction (`common_sequences` intersects them), so an insert/delete
    matcher reports each transposition TWICE, as an unpaired insert and an
    unpaired delete with the other side blank -- atree's two groups printed
    as four, half of them empty. A span is closed here at the first index
    where the two slices hold the same MULTISET, which is precisely "the
    order re-synchronises".
    """
    from collections import Counter

    groups = []
    length = min(len(target_seq), len(ours_seq))
    index = 0
    while index < length:
        if target_seq[index] == ours_seq[index]:
            index += 1
            continue
        target_seen, ours_seen = Counter(), Counter()
        end = index
        while end < length:
            target_seen[target_seq[end]] += 1
            ours_seen[ours_seq[end]] += 1
            end += 1
            if target_seen == ours_seen:
                break
        groups.append({"at": index,
                       "target": target_seq[index:end],
                       "ours": ours_seq[index:end]})
        index = end
    return groups


def object_paths(unit):
    unit = fndiff.unit_key(unit)
    return (REPO / "build" / VERSION / "obj" / f"{unit}.o",
            REPO / "build" / VERSION / "src" / f"{unit}.o")


def check_unit(unit):
    """Verdict dict for one unit. `verdict` is ORDER-OK / MISORDERED / NO-PAIR."""
    unit = fndiff.unit_key(unit)
    target_object, ours_object = object_paths(unit)
    if not target_object.is_file() or not ours_object.is_file():
        missing = [str(p.relative_to(REPO)) for p in (target_object, ours_object)
                   if not p.is_file()]
        return {"unit": unit, "verdict": "NO-PAIR", "missing": missing,
                "groups": [], "target_only": [], "ours_only": [], "compared": 0}
    target_rows = text_symbols(target_object)
    ours_rows = text_symbols(ours_object)
    target_seq, ours_seq, target_only, ours_only = common_sequences(
        target_rows, ours_rows)
    if not target_seq:
        return {"unit": unit, "verdict": "NO-PAIR",
                "missing": ["no shared .text symbols"], "groups": [],
                "target_only": target_only, "ours_only": ours_only,
                "compared": 0}
    groups = misordered_groups(target_seq, ours_seq)
    return {"unit": unit,
            "verdict": "MISORDERED" if groups else "ORDER-OK",
            "groups": groups, "target_only": target_only,
            "ours_only": ours_only, "compared": len(target_seq),
            "missing": []}


def print_result(result, quiet=False):
    unit = result["unit"]
    verdict = result["verdict"]
    if verdict == "NO-PAIR":
        print(f"NO-PAIR      {unit}  (missing: {', '.join(result['missing'])})")
        return
    print(f"{verdict:<12} {unit}  ({result['compared']} shared .text functions"
          f", {len(result['target_only'])} target-only"
          f", {len(result['ours_only'])} ours-only)")
    if quiet:
        return
    for index, group in enumerate(result["groups"], 1):
        print(f"   group {index} @ position {group['at']}:"
              f"  target order  {' '.join(group['target'])}")
        print(f"   {' ' * len(str(index))}"
              f"                ours order    {' '.join(group['ours'])}")
    if result["groups"]:
        print("   the fix is SOURCE-DEFINITION ORDER in the .c (and any"
              " dead-stripped helper that moves with it); no score will"
              " confirm it -- re-run this screen.")
    if result["target_only"] or result["ours_only"]:
        print(f"   not compared: target-only {result['target_only'] or '[]'}"
              f" / ours-only {result['ours_only'] or '[]'}")


def configure_states():
    """unit -> Object() state word from configure.py, for --state filtering."""
    text = (REPO / "configure.py").read_text(encoding="utf-8")
    return {fndiff.unit_key(m.group(2)): m.group(1) for m in re.finditer(
        r'Object\(\s*(\w+)\s*,\s*"([^"]+)"', text)}


def all_units():
    objdir = REPO / "build" / VERSION / "obj"
    units = []
    for path in sorted(objdir.rglob("*.o")):
        unit = str(path.relative_to(objdir)).replace("\\", "/")[:-len(".o")]
        if (REPO / "build" / VERSION / "src" / f"{unit}.o").is_file():
            units.append(unit)
    return units


def main():
    parser = argparse.ArgumentParser(
        description=".text function-order screen (flip precondition)")
    parser.add_argument("units", nargs="*")
    parser.add_argument("--all", action="store_true",
                        help="sweep every unit with both objects built")
    parser.add_argument("--state", help="with --all: only this Object() state")
    parser.add_argument("--quiet", action="store_true",
                        help="verdict lines only, no group detail")
    args = parser.parse_args()

    if args.all:
        units = all_units()
        if args.state:
            states = configure_states()
            units = [u for u in units if states.get(u) == args.state]
    elif args.units:
        units = args.units
    else:
        print(__doc__)
        return 1

    results = [check_unit(u) for u in units]
    bad = [r for r in results if r["verdict"] == "MISORDERED"]
    nopair = [r for r in results if r["verdict"] == "NO-PAIR"]

    if args.all:
        for result in results:
            if result["verdict"] != "ORDER-OK":
                print_result(result, args.quiet)
        print(f"\n{len(results)} units compared: "
              f"{len(results) - len(bad) - len(nopair)} ORDER-OK, "
              f"{len(bad)} MISORDERED, {len(nopair)} NO-PAIR")
    else:
        for result in results:
            print_result(result, args.quiet)

    if bad:
        return 1
    if nopair and not args.all:
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
