#!/usr/bin/env python3
"""Normalize an objdiff progress report in place.

objdiff-cli serializes its report through protobuf-style default-skipping:
a field whose value is the type default (0, 0.0, "0") gets NO key at all.
Every downstream consumer that reads such a field naively — decomp.dev
ingest, dashboards, ad-hoc scripts, a worker eyeballing the JSON — then
treats the row as UNREPORTED instead of as scored-zero, and it silently
drops out of statistics or reads as a hole in the data.

WHAT THE CENSUS SHOWS (run 36, measured over the live 327-scope report:
one top-level `measures` plus 326 units). NOT ONE of the sixteen measure
keys appears with an explicit zero ANYWHERE — every zero in the file is an
absence. Presence counts ranged from 134/327 (`complete_data`) to 327/327,
and seven different unit-measure key-shapes exist, purely as a function of
which values happened to be zero. A unit with no code omits `total_code`,
`matched_code`, `total_functions` and `matched_functions` together; a unit
with no data omits the whole `*_data` family; an unfinished unit omits
`complete_units` and `complete_code`.

This script rewrites the report so every scope carries every key
explicitly, defaulting each omitted value to its typed zero:
  * `measures` at the top level and on every unit — 16 keys;
  * every function row — `fuzzy_match_percent`, `size`, `address`.
Byte counts are int64 and serialize as STRINGS, so their zero is "0", not
0; percents are floats and counts are ints. Filling a key with the value
protobuf omitted it FOR cannot change any correct consumer's answer — it
only removes the KeyError, and removes the ambiguity between "zero" and
"not measured". It changes no other content and is idempotent.

The original narrow version of this script filled only the function-level
`fuzzy_match_percent` (proven by game/movie/movieplayer's fn_800DBA80,
whose written-but-heavily-divergent body scores 0). That one field is
still handled; the other fifteen were left to the same defect.

Usage (wired into the ninja `report` rule after `objdiff report generate`):
  python tools/gdl/normalize_report.py build/GUNE5D/report.json
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

# key -> typed zero. The TYPE matters: objdiff serializes int64 byte counts
# as JSON strings, so filling `total_code` with 0 rather than "0" would make
# the filled rows the only ones a consumer has to special-case — exactly the
# inconsistency this script exists to remove.
MEASURE_DEFAULTS = {
    "complete_code": "0",
    "complete_code_percent": 0.0,
    "complete_data": "0",
    "complete_data_percent": 0.0,
    "complete_units": 0,
    "fuzzy_match_percent": 0.0,
    "matched_code": "0",
    "matched_code_percent": 0.0,
    "matched_data": "0",
    "matched_data_percent": 0.0,
    "matched_functions": 0,
    "matched_functions_percent": 0.0,
    "total_code": "0",
    "total_data": "0",
    "total_functions": 0,
    "total_units": 0,
}

FUNCTION_DEFAULTS = {
    "fuzzy_match_percent": 0.0,
    "size": "0",
    "address": "0",
}


def fill(scope, defaults):
    """Add every missing key with its typed zero; return how many."""
    if not isinstance(scope, dict):
        return 0
    filled = 0
    for key, default in defaults.items():
        if key not in scope:
            scope[key] = default
            filled += 1
    return filled


def normalize(report):
    """Fill every defaulted key in place; return a per-scope count."""
    counts = {"measures": 0, "functions": 0}
    counts["measures"] += fill(report.get("measures"), MEASURE_DEFAULTS)
    for unit in report.get("units", []):
        counts["measures"] += fill(unit.get("measures"), MEASURE_DEFAULTS)
        for function in unit.get("functions", []):
            counts["functions"] += fill(function, FUNCTION_DEFAULTS)
    return counts


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: normalize_report.py <report.json>", file=sys.stderr)
        return 1
    path = Path(sys.argv[1])
    report = json.loads(path.read_text(encoding="utf-8"))
    counts = normalize(report)
    path.write_text(json.dumps(report, separators=(",", ":")),
                    encoding="utf-8")
    print(f"normalize_report: {counts['measures']} measure key(s) and"
          f" {counts['functions']} function key(s) made explicit"
          " (protobuf omits every zero; absence is now unambiguous)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
