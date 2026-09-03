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

  python tools/gdl/progress.py --split         # the STRICT/EQUIVALENT split

Notes:
  * matched_code_percent is BYTE-weighted: a TU with one giant unmatched
    function reads low even if most functions match. Watch the fn column too.
  * report.json is NOT rebuilt by a normal `ninja`; use --refresh (or run
    `ninja build/GUNE5D/report.json`) to get fresh numbers.

IMPORTABLE CORE: postprocessor_split — pure over report.json and
config/GUNE5D/webfrank.json; no build, no printing, and importing this module
has no side effects.
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parent.parent.parent
REPORT = REPO / "build" / VERSION / "report.json"
RULES = REPO / "config" / VERSION / "webfrank.json"


def postprocessor_split(report=None, rules=None):
    """The STRICT / EQUIVALENT split, as a dict, from report.json + the pins.

    Run-47 item 6. Mandatory-policy: "Progress reporting always publishes the
    STRICT/EQUIVALENT split; never quote the combined matched% alone." The
    computation existed only INLINE inside `configure.py progress`, so nothing
    could store it, and a lane reporting its own STRICT delta had to ATTRIBUTE
    one — read two printed percentages from two different moments and assert a
    difference. `defake_gate.py baseline` now stamps this into its meta, which
    makes the delta a measurement between two stamped numbers instead.

    STRICT = byte-exact functions that carry NO webfrank rule (the compiler's
    own output is byte-identical). EQUIVALENT = byte-exact functions that DO
    (proven equivalent modulo regalloc/schedule). Both are byte-weighted over
    every function in the report, which is what makes them comparable to the
    headline matched%.

    The pin table lives under webfrank.json's top-level `units` key; a parser
    that iterates the ROOT finds two keys and zero pins, which reads exactly
    like "no pins exist" and would report the whole matched set as STRICT.
    """
    report_path = Path(report) if report else REPORT
    rules_path = Path(rules) if rules else RULES
    data = json.loads(report_path.read_text(encoding="utf-8"))
    pinned = {}
    if rules_path.exists():
        for unit, entries in (json.loads(
                rules_path.read_text(encoding="utf-8")).get("units", {})
        ).items():
            pinned.setdefault(unit, set()).update(
                entry["function"] for entry in entries)
    total = assisted_bytes = matched_bytes = 0
    assisted_count = matched_count = 0
    for unit in data.get("units", []):
        key = next((k for k in pinned
                    if unit.get("name", "").endswith(k)), None)
        for function in unit.get("functions", []):
            size = int(function.get("size", 0))
            total += size
            if float(function.get("fuzzy_match_percent", 0)) < 100.0:
                continue
            matched_bytes += size
            matched_count += 1
            if key and function["name"] in pinned[key]:
                assisted_bytes += size
                assisted_count += 1
    strict_bytes = matched_bytes - assisted_bytes
    return {
        "strict_percent": 100.0 * strict_bytes / total if total else 0.0,
        "equivalent_percent": 100.0 * assisted_bytes / total if total else 0.0,
        "strict_functions": matched_count - assisted_count,
        "equivalent_functions": assisted_count,
        "strict_bytes": strict_bytes,
        "equivalent_bytes": assisted_bytes,
        "total_bytes": total,
    }


def format_split(split):
    """The one line a record or a report quotes."""
    return (f"STRICT matched {split['strict_percent']:.2f}%"
            f" ({split['strict_functions']} fns) + EQUIVALENT"
            f" {split['equivalent_percent']:.2f}%"
            f" ({split['equivalent_functions']} fns)")


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
    ap.add_argument("--split", action="store_true",
                    help="print only the STRICT/EQUIVALENT postprocessor split")
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

    if args.split:
        print(format_split(postprocessor_split()))
        return 0

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
