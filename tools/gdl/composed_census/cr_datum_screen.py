#!/usr/bin/env python3
"""CR lane: decide a flagged reloc row by the DATA BEHIND both symbols.

`cr_reloc_setdelta.py` compares symbol NAMES restricted to the census rows.
That is enough to expose most order artifacts, but it has one measured blind
spot: when retail's compensating half is a dtk `lbl_ADDR` and ours is an
ANONYMOUS `@N` pool entry, the name-multiset shows the target reading a datum
"one more time" while our object holds exactly the same bytes under a
different name.  game/shop/shop::show_gold is the worked case -- retail loads
`lbl_80348370` (0x4330000080000000, the s32->f64 bias) into f30 and
`lbl_803483B0` (0.00390625) into f31; we load them into the opposite
registers and spell the bias `@193`, whose .sdata2 bytes are
`43300000 80000000`.  Same data, swapped registers, zero defects.

So compare BYTES, not names, exactly as
claim.law.T11_a-pool-rows-verdict-is-the-value-behind-the-two-symbols-not-the-
kind-of-their-names requires, and honour its granularity trap by comparing on
the PREFIX of the shorter entry (dtk names a whole contiguous .rodata run with
one symbol while MWCC emits one @N per literal).

  VALUE-EQUAL   every datum retail reads, we read -- the row is an emission
                order / register-assignment artifact.  No correctness content.
  VALUE-DELTA   a datum appears on one side only.  This is the row worth a
                source edit, and the delta names it.

Run from the repository root:
    python tools/gdl/composed_census/cr_datum_screen.py <unit> <fn>
    python tools/gdl/composed_census/cr_datum_screen.py --image \
        --out build/GUNE5D/cr_image_datum.txt
    python tools/gdl/composed_census/cr_datum_screen.py --raw <unit> <fn>

MERGED, run 43 item 2: the screen itself now lives in `fndiff` as
`datum_screen_from_lines` / `datum_multiset_screen`, shared with
`cq_raw_pool_screen.py`, whose contribution was the RAW-object path (`--raw`
here, `fndiff --datum --raw`).  This file is the CR-shaped CLI over it and
keeps its output verbatim: the merged core was calibrated by re-running
`--image` over all 1,702 screened functions and diffing against the
pre-merge report.

Calibrated 2026-09-03 against four live controls before shipping:
  player::PlayerRestoreState  VALUE-DELTA (100.0/500.0 vs 0.5/30.0)  [true +]
  player::start_magic         VALUE-DELTA (1.5 vs -1.5)              [true +]
  atree::AtreeNodeInit        VALUE-DELTA (the two error strings)    [true +]
  enemy::init_enemy           VALUE-EQUAL (byte-exact, kind-differing
                              pool row)                              [true -]
  shop::show_gold             VALUE-EQUAL (bias/1-256 swapped between
                              f30 and f31, ours anonymous)           [true -]
NOTE (run 43): the three true positives above now read VALUE-EQUAL — every
one of those defects was FIXED in run 42, so the shipped control set no
longer discriminates.  The live discriminating set is the 49 VALUE-DELTA
functions in `--image`; re-derive controls from a fresh `--image` run rather
than trusting the list above.
KNOWN LIMIT, by construction: a TRANSPOSITION preserves the multiset, so this
screen cannot see one (enemy::move_logic00's swapped pi/2pi reads VALUE-EQUAL
here).  It is the complement of es_named_reloc_census.py, not a replacement:
that tool finds wrong-OPERAND rows, this one finds wrong-DATUM rows.
"""

import argparse
import json
import sys
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))
import fndiff  # noqa: E402
from fndiff import parse  # noqa: E402

VERSION = fndiff.VERSION


def screen(unit, fn, raw=False, resolve_lbl=False):
    """(target_only, ours_only, labels), or None when the function is absent.

    Thin wrapper: `fndiff.datum_multiset_screen` is the implementation.
    """
    result = fndiff.datum_multiset_screen(unit, fn, raw=raw,
                                          placeholder_addresses=resolve_lbl)
    if result is None:
        return None
    return result["target_only"], result["ours_only"], result["labels"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("unit", nargs="?")
    ap.add_argument("fn", nargs="?")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--image", action="store_true",
                    help="every function of every NonMatching unit pair")
    ap.add_argument("--raw", action="store_true",
                    help="read the pre-webfrank compiler object, so a RULE"
                         " artifact cannot read as a source-value defect")
    ap.add_argument("--resolve-lbl", action="store_true",
                    help="take a dtk placeholder name's address FROM the name"
                         " (our `extern u8 lbl_8023D000[]` vs the splitter's"
                         " `atree_scroll` are one datum). OFF by default so a"
                         " lane keeps the verdicts it started with")
    ap.add_argument("--census",
                    default=str(REPO / "build" / VERSION / "cr_es_census.json"))
    ap.add_argument("--out", default=None,
                    help="write the report here (default: stdout; generated "
                         "artifacts belong under build/, never beside the script)")
    args = ap.parse_args()

    out = open(args.out, "w", encoding="utf-8") if args.out else sys.stdout

    def emit(*a):
        print(*a, file=out)

    if args.image:
        todo = []
        report = json.loads((REPO / "build" / VERSION / "report.json").read_text())
        for u in report.get("units", []):
            unit = u.get("name", "").removeprefix("main/")
            if u.get("metadata", {}).get("complete"):
                continue
            tobj = REPO / "build" / VERSION / "obj" / f"{unit}.o"
            bobj = REPO / "build" / VERSION / "src" / f"{unit}.o"
            if not (tobj.exists() and bobj.exists()):
                continue
            for fn in parse(tobj):
                todo.append((unit, fn))
    elif args.unit and args.fn:
        todo = [(args.unit.removeprefix("src/"), args.fn)]
    else:
        todo = [(f["unit"], f["function"]) for f in
                json.loads(Path(args.census).read_text())]
    counts = Counter()
    for unit, fn in todo:
        res = screen(unit, fn, raw=args.raw, resolve_lbl=args.resolve_lbl)
        if res is None:
            if not args.image:
                emit(f"MISSING     {unit}::{fn}")
            continue
        only_t, only_b, label = res
        verdict = "VALUE-EQUAL" if not only_t and not only_b else "VALUE-DELTA"
        counts[verdict] += 1
        if args.image and verdict == "VALUE-EQUAL":
            continue
        emit(f"{verdict:<12} {unit}::{fn}")
        for k, n in sorted(only_t.items()):
            emit(f"    TARGET-ONLY x{n}  {k}   {label.get(k)}")
        for k, n in sorted(only_b.items()):
            emit(f"    OURS-ONLY   x{n}  {k}   {label.get(k)}")
    summary = "  ".join(f"{k}={v}" for k, v in sorted(counts.items()))
    emit("\n" + summary)
    if out is not sys.stdout:
        out.close()
        print(f"wrote {args.out}\n  {summary}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
