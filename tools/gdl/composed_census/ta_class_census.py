#!/usr/bin/env python3
"""Live calibration of retire_audit's five instruction classes, image-wide.

    python tools/gdl/composed_census/ta_class_census.py
    python tools/gdl/composed_census/ta_class_census.py --out build/ta_classes.json

Classifies every differing word of every function present in BOTH our raw
pre-postprocess object and the dtk-extracted target, and prints the class
census with example rows so each label can be read against real encodings
rather than trusted.

PROMOTED ON PURPOSE. This started as a scratch file under ignored `build/`,
and tools/gdl/tests/test_retire_audit.py quotes its numbers -- which is
exactly the trap wf_word_diff.decode_word_class records in its own docstring:
a run-47 denial named `WR_scratch/wr_wordscreen.py` as its expiry check and
that file was a lane scratch, so nobody could re-run the measurement that
decided it. A calibration another file cites has to be runnable by its
reader.

Measured at 50e8c254e over 1461 functions (85 count-asymmetric, so 1376
comparable):

    register-assignment  6875 words in 222 functions
    scheduling           4683 words in 141 functions
    memory-access         389 words in  55 functions
    immediate             399 words in  87 functions
    relocation             14 words in   9 functions
    unmodelled relocation types: NONE

No class is empty, so none of the five is a label with no live population,
and RELOC_TYPE_NAMES covers every relocation type the image actually uses.
Two units are SKIPPED and named: movieplayer and NMWException carry extab
payload relocations exception_metadata refuses to read from raw bytes.

Requires a completed `ninja`; stale objects silently misreport. Exit 0 means
the census completed, NOT that any function matches.
"""
import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl import retire_audit as ra  # noqa: E402

VERSION = "GUNE5D"


def census():
    totals = {name: 0 for name in ra.CLASSES}
    functions = {name: set() for name in ra.CLASSES}
    examples = {name: [] for name in ra.CLASSES}
    scanned = asymmetric = missing = 0
    unmodelled = set()
    skipped = []

    src = ROOT / "build" / VERSION / "src"
    if not src.exists():
        raise SystemExit(f"missing {src}; run `ninja` first")
    for raw in sorted(src.rglob(".postprocess/body/*.o")):
        unit = raw.relative_to(src).as_posix().replace(".postprocess/body/", "")[:-2]
        target = ROOT / "build" / VERSION / "obj" / (unit + ".o")
        if not target.exists():
            missing += 1
            continue
        try:
            ours = ra.object_image(raw)
            theirs = ra.object_image(target)
        except (ra.Refused, OSError, ValueError, KeyError) as error:
            # A refusal to measure is named, never counted as zero differences.
            skipped.append({"unit": unit, "reason": str(error)})
            continue
        for name, row in ours["functions"].items():
            other = theirs["functions"].get(name)
            if other is None:
                continue
            scanned += 1
            a = bytes.fromhex(row["body"])
            b = bytes.fromhex(other["body"])
            if len(a) != len(b):
                asymmetric += 1
                continue
            for _off, kind, _name, _add in list(row["relocations"]) + list(other["relocations"]):
                if kind not in ra.RELOC_TYPE_NAMES:
                    unmodelled.add(kind)
            types = ra._reloc_types_by_index(row["relocations"], other["relocations"])
            for entry in ra.classify_stream(a, b, types)["rows"]:
                klass = entry["class"]
                totals[klass] += 1
                functions[klass].add(unit + "::" + name)
                if len(examples[klass]) < 4:
                    examples[klass].append(
                        f"{unit}::{name} +0x{entry['offset']:04x}"
                        f" ours {entry['ours']} target {entry['target']}"
                        f" decode={entry['decode']}"
                        f" relocs={entry['relocation_types']}")
    return {"schema_version": 1, "functions_scanned": scanned,
            "count_asymmetric_functions": asymmetric,
            "units_without_target": missing, "skipped_units": skipped,
            "unmodelled_relocation_types": sorted(unmodelled),
            "differing_words_by_class": totals,
            "functions_touched_by_class": {k: len(v) for k, v in functions.items()},
            "examples": examples,
            "scope": "Class census over existing objects; not a matching claim."}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, help="write the JSON census here")
    args = parser.parse_args(argv)
    if args.out is not None and not args.out.resolve().is_relative_to((ROOT / "build").resolve()):
        parser.error("--out must be under this checkout's build/")
    result = census()
    if args.out is not None:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    for row in result["skipped_units"]:
        print("SKIP", row["unit"], row["reason"])
    print(json.dumps({k: v for k, v in result.items()
                      if k not in ("examples", "skipped_units", "scope")}, indent=2))
    for name in ra.CLASSES:
        print("\n== " + name + " ==")
        for row in result["examples"][name]:
            print("   " + row)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
