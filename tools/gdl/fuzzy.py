#!/usr/bin/env python3
"""Fast per-function fuzzy readout from the existing objdiff report.

Usage:
  python tools/gdl/fuzzy.py game/sfx/sfx ProcessEffects
  python tools/gdl/fuzzy.py game/sfx/sfx            # whole-unit table

Reads build/GUNE5D/report.json AS-IS (no regeneration): the number is only
as fresh as the last full ninja. For live iteration scoring use fndiff
--count (real diff lines) against the just-built object; fuzzy from here is
the before/after bookend, not the per-edit gate.
"""

import json
import re
import sys
from pathlib import Path

REPORT = Path("build/GUNE5D/report.json")


def load_unit(unit):
    unit = unit.replace("\\", "/").strip("/")
    if unit.startswith("src/"):
        unit = unit[len("src/"):]
    unit = re.sub(r"\.(c|cpp)$", "", unit)
    report = json.loads(REPORT.read_text(encoding="utf-8"))
    for entry in report.get("units", []):
        if entry.get("name", "").endswith(unit):
            return entry
    return None


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("--help", "-h"):
        print(__doc__)
        return 1
    if not REPORT.exists():
        print(f"missing {REPORT} — run a full ninja first")
        return 1
    entry = load_unit(sys.argv[1])
    if entry is None:
        print(f"no report unit matches {sys.argv[1]!r}")
        return 1
    wanted = sys.argv[2] if len(sys.argv) > 2 else None
    shown = 0
    for function in entry.get("functions", []):
        if wanted and function["name"] != wanted:
            continue
        fuzzy = float(function.get("fuzzy_match_percent", 0.0))
        size = function.get("size", "?")
        print(f"{fuzzy:8.4f}%  {function['name']}  (size {size})")
        shown += 1
    if wanted and not shown:
        print(f"function {wanted!r} not in unit {entry['name']}")
        return 1
    print(f"[unit {entry['name']}; report mtime is the last full ninja]")
    return 0


if __name__ == "__main__":
    sys.exit(main())
