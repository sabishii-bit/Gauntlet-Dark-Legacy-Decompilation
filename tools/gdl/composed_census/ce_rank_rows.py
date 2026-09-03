"""Rank the image-wide datum rows: real wrong-DATUM rows vs keying artifacts.

The screen's datum_key falls back to an ADDRESS key (`A:`) for a symbol with
no bytes (BSS/SBSS) and to a NAME key (`N:`) for one it cannot resolve through
retail's symbols.txt at all.  Our source's own global names are not in
symbols.txt, so a BSS global that retail's splitter left as `lbl_ADDR` and our
source named is keyed `A:` on one side and `N:` on the other and can NEVER
pair.  Those rows are guaranteed false positives.  A `P:` key is a pointer
table keyed by SIZE, so two tables differing in length read as different data.
Only a `B:` key on BOTH sides is a comparison of actual datum BYTES, and only
those rows can carry a wrong constant.

    python tools/gdl/composed_census/ce_eq_datum_audit.py --image \
        --out build/GUNE5D/ce_image_datum.json
    python tools/gdl/composed_census/ce_rank_rows.py \
        [--in build/GUNE5D/ce_image_datum.json]

claim.law.CE_a-bss-datum-has-no-bytes-so-the-datum-screen-keys-it-by-address-
on-one-side-and-by-name-on-the-other.20260903.v1 is the law this partitions on;
it measured the artifact class at 17 of 49 rows image-wide at 9eb99ec5c.
"""
import argparse
import json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
_parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
_parser.add_argument("--in", dest="source", default=str(
    ROOT / "build" / "GUNE5D" / "ce_image_datum.json"))
_arguments = _parser.parse_args()
data = json.load(open(_arguments.source, encoding="utf-8"))


def kinds(entries):
    out = set()
    for entry in entries:
        datum = entry["datum"]
        if datum.startswith("A:"):
            out.add("A")
        elif datum.startswith("N:"):
            out.add("N")
        elif datum.startswith("P:"):
            out.add("P")
        else:
            out.add("B")
    return out


real, artifact = [], []
for row in data["rows"]:
    seen = kinds(row["target_only"]) | kinds(row["ours_only"])
    (real if "B" in seen else artifact).append((row, sorted(seen)))

print(f"IMAGE-WIDE RAW-OBJECT DATUM SCREEN: {len(data['rows'])} VALUE-DELTA "
      f"rows of {data['tally']['screened']} NonMatching functions")
print(f"  rows carrying a real BYTE datum difference: {len(real)}")
print(f"  rows that are key-class artifacts only:     {len(artifact)}")
print(f"    artifact key classes: "
      f"{Counter(tuple(k) for _r, k in artifact).most_common()}")
print()
print("REAL WRONG-DATUM ROWS (the hand-off table):")
for row, seen in sorted(real, key=lambda rk: rk[0]["unit"]):
    print(f"\n  {row['unit']}::{row['function']}   key classes {seen}")
    for entry in row["target_only"]:
        if entry["datum"].startswith("B:") or not entry["datum"][1:2] == ":":
            print(f"      TARGET-ONLY x{entry['n']}  {entry['datum'][:120]}")
    for entry in row["ours_only"]:
        if entry["datum"].startswith("B:") or not entry["datum"][1:2] == ":":
            print(f"      OURS-ONLY   x{entry['n']}  {entry['datum'][:120]}")
print("\nKEY-CLASS ARTIFACT ROWS (no source edit implied):")
for row, seen in sorted(artifact, key=lambda rk: rk[0]["unit"]):
    print(f"  {row['unit']}::{row['function']}   {seen}")
