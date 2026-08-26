#!/usr/bin/env python3
"""Repo-wide lowest-match function queue from objdiff's report.json.

Unlike nearmiss.py, this tool finds functions that still need semantic or
structural reconstruction. It excludes complete/linked translation units,
sorts lowest percentages first by default, and reports an estimated unmatched
byte gap so a campaign can also prioritize high-impact bodies.

Usage (from repo root):
  python tools/gdl/lowmatch.py
  python tools/gdl/lowmatch.py --max 25 --min-size 200 --limit 30
  python tools/gdl/lowmatch.py --sort impact --parked skip
  python tools/gdl/lowmatch.py --refresh --residuals
"""

import argparse
import difflib
import json
import subprocess
import sys

from fndiff import classify_function, normalized_reloc_lines, parse
from nearmiss import REPORT, REPO, VERSION, load_parked


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--min", type=float, default=0.0, metavar="PCT",
                    help="lower fuzzy bound, inclusive (default 0)")
    ap.add_argument("--max", type=float, default=50.0, metavar="PCT",
                    help="upper fuzzy bound, inclusive (default 50)")
    ap.add_argument("--min-size", type=int, default=0, metavar="BYTES",
                    help="minimum target function size (default 0)")
    ap.add_argument("--limit", type=int, default=50, metavar="N",
                    help="maximum rows to print; 0 prints all (default 50)")
    ap.add_argument("--sort", choices=["lowest", "impact", "size"],
                    default="lowest",
                    help="queue order (default: lowest percentage first)")
    ap.add_argument("--refresh", action="store_true",
                    help="regenerate report.json before reading")
    ap.add_argument("--grep", metavar="STR",
                    help="only TUs whose name contains STR")
    ap.add_argument("--parked", choices=["mark", "skip"], default="mark",
                    help="PARKED.txt handling (default: mark)")
    ap.add_argument("--residuals", action="store_true",
                    help="measure normalized real diff lines (slower)")
    ap.add_argument("--include-unscored", action="store_true",
                    help="also show unpaired functions with no fuzzy score")
    args = ap.parse_args()

    if args.min < 0.0 or args.max > 100.0 or args.min > args.max:
        ap.error("require 0 <= --min <= --max <= 100")
    if args.min_size < 0 or args.limit < 0:
        ap.error("--min-size and --limit must be non-negative")

    if args.refresh:
        result = subprocess.run(
            ["ninja", f"build/{VERSION}/report.json"], cwd=str(REPO))
        if result.returncode:
            print("ninja report.json FAILED -- fix the build before trusting "
                  "this queue", file=sys.stderr)
            return 1

    if not REPORT.exists():
        print(f"no {REPORT} -- run with --refresh", file=sys.stderr)
        return 1

    parked = load_parked()
    rows = []
    for unit_info in json.loads(REPORT.read_text()).get("units", []):
        unit = unit_info.get("name", "").removeprefix("main/")
        if args.grep and args.grep not in unit:
            continue
        if unit_info.get("metadata", {}).get("complete"):
            continue

        target_fns = base_fns = None
        if args.residuals:
            target_obj = REPO / "build" / VERSION / "obj" / f"{unit}.o"
            base_obj = REPO / "build" / VERSION / "src" / f"{unit}.o"
            if target_obj.exists() and base_obj.exists():
                target_fns = parse(target_obj)
                base_fns = parse(base_obj)

        for function in unit_info.get("functions", []):
            name = function.get("name", "?")
            raw_pct = function.get("fuzzy_match_percent")
            if raw_pct is None:
                if args.include_unscored:
                    print(f"UNSCORED  {int(function.get('size', 0) or 0):5d}B  "
                          f"{name:<40} {unit}")
                continue
            pct = float(raw_pct)
            size = int(function.get("size", 0) or 0)
            if name == "?" or size < args.min_size:
                continue
            if not (args.min <= pct <= args.max) or pct >= 100.0:
                continue
            if name in parked and args.parked == "skip":
                continue

            real = category = None
            if target_fns is not None:
                target = target_fns.get(name)
                base = base_fns.get(name)
                if target is not None and base is not None:
                    diff = difflib.unified_diff(
                        normalized_reloc_lines(target),
                        normalized_reloc_lines(base), lineterm="", n=0)
                    real = sum(
                        1 for line in diff
                        if line[:1] in "+-" and line[:3] not in ("+++", "---"))
                    category = classify_function(target, base)

            gap = size * (100.0 - pct) / 100.0
            rows.append((pct, size, gap, name, unit, real, category))

    sort_keys = {
        "lowest": lambda row: (row[0], -row[1], row[4], row[3]),
        "impact": lambda row: (-row[2], row[0], -row[1], row[4], row[3]),
        "size": lambda row: (-row[1], row[0], row[4], row[3]),
    }
    rows.sort(key=sort_keys[args.sort])
    if args.limit:
        rows = rows[:args.limit]

    for pct, size, gap, name, unit, real, category in rows:
        parked_tag = "  [PARKED]" if name in parked else ""
        residual = ""
        if args.residuals:
            residual = (f"  d={real:4d} {category:<18}" if real is not None
                        else "  d=????")
        print(f"{pct:6.2f}%  {size:5d}B  gap~{gap:6.0f}B{residual}  "
              f"{name:<40} {unit}{parked_tag}")

    gap_total = sum(row[2] for row in rows)
    print(f"--- {len(rows)} low-match fns ({args.min:g}%..{args.max:g}%, "
          f"size >= {args.min_size}B) | shown gap~{gap_total:.0f}B | "
          f"{len(parked)} names in PARKED.txt ---")
    return 0


if __name__ == "__main__":
    sys.exit(main())
