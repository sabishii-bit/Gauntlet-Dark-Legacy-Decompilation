#!/usr/bin/env python3
"""Which law FALSIFIERS name a function a NEWER record has already worked?

WHY THIS EXISTS (run-55 item 9, reported by CR against run 53/54): "the
run-53 CR record's law_screen states the decl-swap cure is already
satisfied; the AL law's falsifier calls the same function the one untried
row - both accepted the same day, three hours apart, and disagree".

Reproduced at 215bd2193, and both records are accepted:

  claim.law.AL_the-addr16lo-surplus-copy-roster-is-two-classes-split-by-
  destination-register-and-only-the-volatile-tenth-is-reachable.20260904.v1
  falsifier: "... ProcessCritterList is the one untried row carrying the
  responsive polarity ..."

  attempt.CR_processcritterlist-the-declaration-order-lever-is-inert-here-
  so-the-addr16lo-reachable-remainder-is-zero.20260904.v1
  head: "Discharge the falsifier that claim.law.AL_...20260904.v1 names ON
  THIS FUNCTION BY NAME", outcome `negative` — the lever is inert.

A falsifier is what lets a later lane screen a law OUT (AGENTS.md proposal
gate 1), so a falsifier naming an already-probed row sends that lane to
re-run a discharged probe, and nothing in the corpus notices because both
records are individually correct.

    python tools/gdl/composed_census/t25_falsifier_freshness.py
    python tools/gdl/composed_census/t25_falsifier_freshness.py --law <id>
    python tools/gdl/composed_census/t25_falsifier_freshness.py --tier2

TWO TIERS, never totalled:
  CITED    a newer accepted record on the named function that CITES the law
           by id. This is the high-confidence tier: the later lane already
           went to the falsifier and answered it.
  NEWER    a newer accepted record on the named function that does not cite
           the law (`--tier2`). Advisory — it may be about another axis
           entirely.

Function names are resolved against config/GUNE5D/symbols.txt, so a token
is only treated as a function if the image actually has one by that name.
That is what keeps the extraction honest: ordinary prose words cannot
manufacture rows.

TWO-SIDED CALIBRATION at 215bd2193 over 779 law records and 1,408 attempt
records, resolving names against 2,985 function symbols:
  POSITIVE  35 CITED rows — 14 CROSS-LANE and 21 same-lane. The reported
            pair is the FIRST cross-lane row.
  NEGATIVE  125 NEWER rows are held back behind `--tier2` rather than
            printed as findings, and the 21 same-lane rows are printed
            separately because reading them showed the class: a law's OWN
            deriving record cites it, names its function and is minutes
            newer (claim.law.CR_a-repeatedly-read-struct-array-member-...
            .20260904.v1 against attempt.CR_crittercollideitems-...
            .20260904.v1 is one pass writing both halves, not a
            disagreement). That class is invisible in the COUNT and obvious
            in the LIST, which is why the split exists.
The symbol resolution is the other half of the negatives: prose words
cannot manufacture rows, because a token is only a function if the image
has one by that name.

Exit 0 always: this reports a reading obligation, never a verdict.

IMPORTABLE CORE: function_symbols, named_functions, rows.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE
while not (ROOT / "config" / "GUNE5D").is_dir():
    if ROOT.parent == ROOT:
        raise SystemExit(f"repo root not found above {HERE}")
    ROOT = ROOT.parent

SYMBOLS = ROOT / "config" / "GUNE5D" / "symbols.txt"
RECORDS = ROOT / "memory_graph" / "records"
TOKEN = re.compile(r"[A-Za-z_][A-Za-z0-9_]{3,}")


def function_symbols(path=None):
    """Every function name in symbols.txt, as a set."""
    path = Path(path) if path else SYMBOLS
    names = set()
    if not path.exists():
        return names
    for line in path.read_text(encoding="utf-8",
                               errors="replace").splitlines():
        head, _, rest = line.partition("=")
        if "type:function" in rest:
            names.add(head.strip())
    return names


def named_functions(text, symbols):
    """The function symbols a falsifier's prose actually names."""
    return sorted({t for t in TOKEN.findall(text or "") if t in symbols})


def _load(kind):
    out = []
    for path in (RECORDS / kind).rglob("*.json"):
        try:
            out.append(json.loads(path.read_text(encoding="utf-8")))
        except Exception:
            continue
    return out


def _stamp(record):
    return str(record.get("recorded_at") or record.get("valid_from") or "")


def _anchor(record):
    ref = str(record.get("function") or "")
    return ref.split(":", 1)[1] if ref.startswith("function:") else ""


def lane_prefix(record_id):
    """The lane tag in a record id (`claim.law.AL_x` -> 'AL'), or ''.

    The SAME-LANE / CROSS-LANE split is what makes the CITED tier readable
    (found by reading the tier, not by counting it). A law's own DERIVING
    record cites it, names its function and is minutes newer — that is not
    a disagreement, it is one pass writing both halves, and it is always
    same-lane. The reported case is cross-lane: an AL law's falsifier
    against a CR record.
    """
    for part in (record_id or "").split("."):
        head = part.split("_", 1)[0]
        if head.isupper() and 1 < len(head) <= 3 and head != "LAW":
            return head
    return ""


def rows(claims, attempts, symbols):
    """[(law_id, function, tier, record_id, outcome, stamp)] pairs."""
    by_function = {}
    for attempt in attempts:
        name = _anchor(attempt)
        if name:
            by_function.setdefault(name, []).append(attempt)
    found = []
    for law in claims:
        falsifier = law.get("falsifier") or (
            law.get("attributes") or {}).get("falsifier")
        if not falsifier:
            continue
        law_id, law_stamp = law.get("id") or "", _stamp(law)
        for name in named_functions(str(falsifier), symbols):
            for attempt in by_function.get(name, []):
                if _stamp(attempt) <= law_stamp:
                    continue
                cites = law_id and law_id in json.dumps(attempt)
                found.append((law_id, name, "CITED" if cites else "NEWER",
                              attempt.get("id"), attempt.get("outcome"),
                              _stamp(attempt)))
    return found


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--law", help="only this law id")
    ap.add_argument("--tier2", action="store_true",
                    help="also print the advisory NEWER tier")
    args = ap.parse_args()

    symbols = function_symbols()
    claims = [c for c in _load("claims")
              if not args.law or c.get("id") == args.law]
    attempts = _load("attempts")
    found = rows(claims, attempts, symbols)
    cited = [r for r in found if r[2] == "CITED"]
    newer = [r for r in found if r[2] == "NEWER"]
    cross = [r for r in cited if lane_prefix(r[0]) != lane_prefix(r[3])]
    same = [r for r in cited if lane_prefix(r[0]) == lane_prefix(r[3])]
    print(f"function symbols known        : {len(symbols)}")
    print(f"law records scanned           : {len(claims)}")
    print(f"attempt records scanned       : {len(attempts)}")
    print(f"CITED rows (high confidence)  : {len(cited)}"
          f"  = {len(cross)} CROSS-LANE + {len(same)} same-lane")
    print(f"NEWER rows (advisory)         : {len(newer)}"
          + ("" if args.tier2 else "  [--tier2 to print them]"))
    for law_id, name, _tier, rec, outcome, stamp in cross:
        print(f"  CROSS-LANE  {law_id}")
        print(f"     falsifier names {name}; {rec} ({outcome}, {stamp})"
              " cites this law, is newer, and was written by a DIFFERENT"
              " lane — read it before treating the falsifier's row as"
              " untried.")
    for law_id, name, _tier, rec, outcome, stamp in same:
        print(f"  same-lane   {law_id}")
        print(f"     falsifier names {name}; {rec} ({outcome}, {stamp})"
              " cites this law and is newer. Usually the law's OWN deriving"
              " record rather than a disagreement — check the order.")
    if args.tier2:
        for law_id, name, _tier, rec, outcome, stamp in newer:
            print(f"  NEWER  {law_id}")
            print(f"     falsifier names {name}; {rec} ({outcome}, {stamp})"
                  " is newer but does not cite the law — may be another"
                  " axis entirely.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
