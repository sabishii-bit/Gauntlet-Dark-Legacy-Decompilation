"""Partition schema-1 CE datum candidates without calling them source bugs.

    python tools/gdl/composed_census/ce_rank_rows.py --in build/r66_ce_image.json

Byte keys (B:) prioritize literal review; A:/N:/P: keys prioritize identity,
ownership and relocation review. Neither partition proves a defect or a
false positive. Equal multisets do not rule out transposed operands. This
consumer preserves the input audit's PASS/FAIL/UNRESOLVED status and exit
code; unsupported or malformed input is a FAIL, never an empty clean roster.
"""
import argparse
import json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def ranked_rows(data):
    if not isinstance(data, dict) or data.get("schema_version") != 1:
        raise ValueError("expected ce_eq_datum_audit schema_version=1")
    if data.get("status") not in ("PASS", "FAIL", "UNRESOLVED"):
        raise ValueError("missing or unknown audit status")
    if not isinstance(data.get("rows"), list) or not isinstance(data.get("tally"), dict):
        raise ValueError("audit has no rows/tally; it did not complete")
    byte_rows, identity_rows, unreadable = [], [], []
    for row in data["rows"]:
        if not isinstance(row, dict) or not all(isinstance(row.get(k), str) for k in ("unit", "function")):
            raise ValueError("invalid function row")
        raw = row.get("raw")
        if not isinstance(raw, dict) or raw.get("status") not in ("PASS", "FAIL", "UNRESOLVED"):
            raise ValueError("invalid raw-side result")
        if "verdict" not in raw:
            if raw["status"] == "PASS" or not raw.get("error"):
                raise ValueError("unreadable side requires non-PASS status and error")
            unreadable.append(row)
            continue
        if raw["verdict"] not in ("VALUE-EQUAL", "VALUE-DELTA"):
            raise ValueError("unknown multiset verdict")
        kinds = set()
        for side in ("target_only", "ours_only"):
            entries = raw.get(side)
            if not isinstance(entries, dict):
                raise ValueError("datum delta must be a key/count object")
            for key, count in entries.items():
                if not isinstance(key, str) or key[:2] not in ("A:", "N:", "P:", "B:"):
                    raise ValueError("unknown datum key kind")
                if isinstance(count, bool) or not isinstance(count, int) or count <= 0:
                    raise ValueError("datum counts must be positive integers")
                kinds.add(key[0])
        if raw["verdict"] == "VALUE-EQUAL":
            if kinds:
                raise ValueError("VALUE-EQUAL row carries a delta")
            continue
        if not kinds:
            raise ValueError("VALUE-DELTA row has no delta")
        (byte_rows if "B" in kinds else identity_rows).append((row, sorted(kinds)))
    if data["status"] == "PASS" and (byte_rows or identity_rows or unreadable):
        raise ValueError("PASS audit contains unresolved rows")
    return byte_rows, identity_rows, unreadable


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--in", dest="source", default=str(ROOT / "build/GUNE5D/ce_image_datum.json"))
    args = parser.parse_args(argv)
    try:
        data = json.loads(Path(args.source).read_text(encoding="utf-8"))
        byte_rows, identity_rows, unreadable = ranked_rows(data)
    except (OSError, ValueError, TypeError) as exc:
        print(f"DATUM RANK FAIL: {exc}")
        return 1
    print(f"DATUM RANK {data['status']} (input audit scope: {data.get('selection', {})})")
    print(f"  byte-bearing review candidates: {len(byte_rows)}")
    print(f"  identity/representation review candidates: {len(identity_rows)}")
    print(f"  unreadable raw functions: {len(unreadable)}")
    print(f"  identity key classes: {Counter(tuple(k) for _, k in identity_rows).most_common()}")
    for title, rows in (("BYTE-BEARING CANDIDATES (not proven source defects)", byte_rows),
                        ("IDENTITY/REPRESENTATION CANDIDATES (not proved false positives)", identity_rows)):
        print(title)
        for row, seen in rows:
            print(f"  {row['unit']}::{row['function']} key classes {seen}")
            for side in ("target_only", "ours_only"):
                for key, count in row["raw"][side].items():
                    print(f"    {side} x{count} {key[:160]}")
    for row in unreadable:
        print(f"UNREADABLE {row['unit']}::{row['function']}: {row['raw']['error']}")
    return {"PASS": 0, "FAIL": 1, "UNRESOLVED": 2}[data["status"]]


if __name__ == "__main__":
    raise SystemExit(main())
