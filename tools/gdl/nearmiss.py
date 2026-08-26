#!/usr/bin/env python3
"""Repo-wide near-miss work queue from objdiff's report.json.

Lists every function whose fuzzy match is >= threshold but < 100%, sorted
closest-first: these are the "one pad / one decl-order away" wins that the
per-TU views never surface.

Usage (from repo root):
  python tools/gdl/nearmiss.py                # >= 90%, closest first
  python tools/gdl/nearmiss.py --min 95       # tighter queue
  python tools/gdl/nearmiss.py --refresh      # regenerate report.json first
  python tools/gdl/nearmiss.py --grep sfx     # one TU family
  python tools/gdl/nearmiss.py --parked skip  # hide fns listed in PARKED.txt

.claude/memory/PARKED.txt (optional): one function name per line (comments with
'#'), the residuals already diagnosed as allocator-quirk walls. Default is to
mark them [PARKED] rather than hide, so the queue stays honest.
"""

import argparse
import difflib
import json
import subprocess
import sys
from pathlib import Path

from fndiff import classify_function, normalized_reloc_lines, parse

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parent.parent.parent
REPORT = REPO / "build" / VERSION / "report.json"
PARKED_CANDIDATES = (
    REPO / ".claude" / "memory" / "PARKED.txt",
    REPO / "research" / "PARKED.txt",  # legacy fallback
)


def load_parked():
    parked_path = next((path for path in PARKED_CANDIDATES if path.exists()), None)
    if parked_path is None:
        return set()
    names = set()
    for line in parked_path.read_text().splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            names.add(line.split()[0])
    return names


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--min", type=float, default=90.0, metavar="PCT",
                    help="lower fuzzy bound (default 90)")
    ap.add_argument("--refresh", action="store_true",
                    help="regenerate report.json (ninja) before reading")
    ap.add_argument("--grep", metavar="STR", help="only TUs whose name contains STR")
    ap.add_argument("--parked", choices=["mark", "skip"], default="mark",
                    help="PARKED.txt handling (default: mark)")
    ap.add_argument("--residuals", action="store_true",
                    help="measure real object-diff lines and sort cheapest first")
    args = ap.parse_args()

    if args.refresh:
        r = subprocess.run(["ninja", f"build/{VERSION}/report.json"], cwd=str(REPO))
        if r.returncode:
            print("ninja report.json FAILED -- fix the build before trusting this queue",
                  file=sys.stderr)
            return 1

    if not REPORT.exists():
        print(f"no {REPORT} -- run with --refresh", file=sys.stderr)
        return 1

    parked = load_parked()
    rows = []
    for u in json.loads(REPORT.read_text()).get("units", []):
        unit = u.get("name", "").removeprefix("main/")
        if args.grep and args.grep not in unit:
            continue
        # Matching (linked) TUs are byte-proven by the link itself: any <100%
        # fuzzy inside them is reloc-name scoring noise, NOT a near-miss.
        # Editing their source based on fuzzy% BREAKS REAL DOL BYTES.
        if u.get("metadata", {}).get("complete"):
            continue
        target_fns = base_fns = None
        if args.residuals:
            target_obj = REPO / "build" / VERSION / "obj" / f"{unit}.o"
            base_obj = REPO / "build" / VERSION / "src" / f"{unit}.o"
            if target_obj.exists() and base_obj.exists():
                target_fns = parse(target_obj)
                base_fns = parse(base_obj)
        for f in u.get("functions", []):
            pct = f.get("fuzzy_match_percent", 0.0)
            if pct >= args.min and pct < 100.0:
                name = f.get("name", "?")
                size = int(f.get("size", 0) or 0)
                real = None
                category = None
                if target_fns is not None:
                    target = target_fns.get(name)
                    base = base_fns.get(name)
                    if target is not None and base is not None:
                        diff = difflib.unified_diff(
                            normalized_reloc_lines(target),
                            normalized_reloc_lines(base), lineterm="", n=0)
                        real = sum(1 for line in diff
                                   if line[:1] in "+-" and
                                   line[:3] not in ("+++", "---"))
                        category = classify_function(target, base)
                rows.append((pct, size, name, unit, real, category))

    if args.residuals:
        rows.sort(key=lambda r: (r[4] is None, r[4] or 0, -r[1], -r[0]))
    else:
        rows.sort(key=lambda r: (-r[0], r[1]))
    shown = 0
    for pct, size, name, unit, real, category in rows:
        tag = ""
        if name in parked:
            if args.parked == "skip":
                continue
            tag = "  [PARKED]"
        residual = (f"  d={real:3d} {category:<18}" if real is not None
                    else "" if not args.residuals else "  d=???")
        print(f"{pct:6.2f}%  {size:5d}B{residual}  {name:<40} {unit}{tag}")
        shown += 1
    print(f"--- {shown} near-miss fns (>= {args.min}%, < 100%)"
          f"{'' if not parked else f' | {len(parked)} names in PARKED.txt'} ---")
    return 0


if __name__ == "__main__":
    sys.exit(main())
