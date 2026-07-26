#!/usr/bin/env python3
"""Per-file/per-TU progress report from objdiff's report.json.

Prints each translation unit's matched-code % and matched/total function
count. Avoids the nested-quote escaping that breaks an inline `python -c`
one-liner under PowerShell.

Usage (from repo root):
  python tools/gdl/progress.py                 # all TUs, sorted by % ascending
  python tools/gdl/progress.py --refresh       # regenerate report.json first
  python tools/gdl/progress.py --incomplete    # only TUs with unmatched fns
  python tools/gdl/progress.py --min 75        # only TUs >= 75% matched code
  python tools/gdl/progress.py --grep world    # only TUs whose name contains 'world'
  python tools/gdl/progress.py --sort name     # sort by name (default: pct)

Notes:
  * matched_code_percent is BYTE-weighted: a TU with one giant unmatched
    function reads low even if most functions match. Watch the fn column too.
  * report.json is NOT rebuilt by a normal `ninja`; use --refresh (or run
    `ninja build/GUNE5D/report.json`) to get fresh numbers.
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parent.parent.parent
REPORT = REPO / "build" / VERSION / "report.json"


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--refresh", action="store_true",
                    help="regenerate report.json (ninja) before printing")
    ap.add_argument("--incomplete", action="store_true",
                    help="only TUs with matched_functions < total_functions")
    ap.add_argument("--min", type=float, default=None, metavar="PCT",
                    help="only TUs with matched-code %% >= PCT")
    ap.add_argument("--grep", metavar="STR", help="only TUs whose name contains STR")
    ap.add_argument("--sort", choices=["pct", "name", "fns"], default="pct",
                    help="sort key (default: pct ascending)")
    args = ap.parse_args()

    if args.refresh:
        r = subprocess.run(["ninja", f"build/{VERSION}/report.json"], cwd=str(REPO))
        if r.returncode:
            print("ninja report.json failed", file=sys.stderr)
            return 1

    if not REPORT.exists():
        print(f"no {REPORT} -- run with --refresh (or `ninja build/{VERSION}/report.json`)",
              file=sys.stderr)
        return 1

    units = json.loads(REPORT.read_text()).get("units", [])
    rows = []
    for u in units:
        name = u.get("name", "").removeprefix("main/")
        m = u.get("measures", {})
        pct = m.get("matched_code_percent", 0.0)
        tf = m.get("total_functions", 0)
        mf = m.get("matched_functions", 0)
        if tf == 0:
            continue
        if args.incomplete and mf >= tf:
            continue
        if args.min is not None and pct < args.min:
            continue
        if args.grep and args.grep not in name:
            continue
        rows.append((pct, mf, tf, name))

    keys = {"pct": lambda r: (r[0], r[3]),
            "name": lambda r: r[3],
            "fns": lambda r: (r[1] / r[2] if r[2] else 0, r[3])}
    rows.sort(key=keys[args.sort])

    def num(v):
        try:
            return int(v)
        except (TypeError, ValueError):
            return 0

    tot_c = tot_mc = 0
    for u in units:
        m = u.get("measures", {})
        tot_c += num(m.get("total_code", 0))
        tot_mc += num(m.get("matched_code", 0))
    for pct, mf, tf, name in rows:
        print(f"{pct:6.2f}%  {mf:>3}/{tf:<3}  {name}")
    print(f"--- {len(rows)} TUs shown | overall matched code: "
          f"{(100.0 * tot_mc / tot_c) if tot_c else 0:.2f}% "
          f"({tot_mc}/{tot_c} bytes) ---")
    return 0


if __name__ == "__main__":
    sys.exit(main())
