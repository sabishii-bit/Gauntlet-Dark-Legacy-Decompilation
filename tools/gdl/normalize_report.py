#!/usr/bin/env python3
"""Normalize an objdiff progress report in place.

objdiff-cli serializes its report through protobuf-style default-skipping:
a function whose fuzzy_match_percent is exactly 0.0 gets NO
"fuzzy_match_percent" key at all (proven by game/movie/movieplayer's
fn_800DBA80, whose written-but-heavily-divergent body scores 0). Every
downstream consumer that reads the field naively — decomp.dev ingest,
dashboards, ad-hoc scripts — then treats the function as unreported
instead of as scored-zero, and it silently drops out of statistics.

This script rewrites the report so every function row carries an explicit
fuzzy_match_percent (defaulting the omitted value to 0.0). It changes no
other content and is idempotent.

Usage (wired into the ninja `report` rule after `objdiff report generate`):
  python tools/gdl/normalize_report.py build/GUNE5D/report.json
"""

from __future__ import annotations

import json
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: normalize_report.py <report.json>", file=sys.stderr)
        return 1
    path = Path(sys.argv[1])
    report = json.loads(path.read_text(encoding="utf-8"))
    filled = 0
    for unit in report.get("units", []):
        for function in unit.get("functions", []):
            if "fuzzy_match_percent" not in function:
                function["fuzzy_match_percent"] = 0.0
                filled += 1
    path.write_text(json.dumps(report, separators=(",", ":")),
                    encoding="utf-8")
    print(f"normalize_report: {filled} zero-score row(s) made explicit")
    return 0


if __name__ == "__main__":
    sys.exit(main())
