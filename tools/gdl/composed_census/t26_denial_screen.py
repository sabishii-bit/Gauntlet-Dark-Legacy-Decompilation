"""Screen a shipped census roster against the ACCEPTED denials it contradicts.

RUN-56 ITEM 4, reproduced verbatim before anything was designed:

    $ python tools/gdl/composed_census/al_addrlo_positive.py
    $ python tools/gdl/composed_census/al_dest_split.py
    --- V: class (b) VOLATILE dest -- DECL-ORDER LEVER LIVE  (4 function(s))
      game/enemy/critter   ProcessCritterList  B 0->0  V 0->1  T49/O50

and, in the graph, on that same function:

    attempt.CR_processcritterlist-the-declaration-order-lever-is-inert-here-
    so-the-addr16lo-reachable-remainder-is-zero.20260904.v1  (accepted)
      denial.scope: "TWO axes on ProcessCritterList ... (a) DECLARATION ORDER
      of the function's four locals, measured across three distinct orders."

A census computed from BYTES cannot see the graph, so it advertises a lever
as LIVE on a function whose accepted denial says that exact axis is dead.
Nothing is wrong with either artifact; what was missing is the join. This
tool is that join, and it is deliberately NOT wired into any one census:
`al_dest_split.py` belongs to the ADDR16_LO lane's workflow this run, and a
screen that lives inside one census helps only that census. It reads any
census JSON that carries function names, so the next roster gets it free.

WHAT A CONTRADICTION IS HERE. Both halves must hold:
  1. the function carries an ACCEPTED attempt record with a typed `denial`
     block that is not superseded by a newer accepted record; and
  2. an axis phrase from the controlled vocabulary below appears BOTH in the
     census row's own advertised lever text AND in the DENYING half of that
     denial's scope.

THE DENYING HALF IS NOT THE WHOLE SCOPE, and that is the whole difficulty.
Scopes are written to bound themselves — "It does NOT deny statement-shape
levers", "It denies nothing about the indexing form" — so a naive substring
match fires on the EXEMPTION and reports a contradiction that the record
explicitly disclaims. This is the same failure the `owned_units` prose screen
has (AGENTS.md: it "cannot read a negation, so a lane that names another
lane's TUs in order to exclude them is reported as their co-owner"), and it
is measured here rather than assumed: see the calibration below.

TWO-SIDED CALIBRATION at c8e3b3479 (`--calibrate` reprints the corpus half,
so it is re-derivable rather than remembered):

  corpus     188 accepted records carry a typed denial; 151 of those are
             live (not superseded) and anchored to a function, over 104
             functions. Across their scopes the vocabulary matches 68 axis
             mentions, of which 48 sit in a DENYING clause and 20 (29%) are
             exemptions the cut discards. Per axis the exemption share
             varies enormously -- loop-form 2 of 2, pragma 8 of 16,
             declaration-order 0 of 9 -- so the cut is neither uniform nor
             optional.
  POSITIVE   1 of `al_dest_split.json`'s 41 rows, and it is exactly the
             reported one (ProcessCritterList / declaration-order).
  NEGATIVE   the other 40 rows, plus 3 of the 4 census JSONs present in
             `build/GUNE5D/` screening to zero.
  EXCLUDED   `volatile` -- the census writes "VOLATILE dest" for a REGISTER
             CLASS while a denial writes it for the C qualifier, and 7 of
             its 9 corpus mentions are exemption clauses anyway; and
             `position`, which appears in 55 denial blocks and names no axis
             by itself. Each adds 0 flags on the four censuses available
             here, so they are excluded on the corpus evidence, not on a
             measured false-positive count -- if a later roster makes either
             pay, add it and reprice.

IMPORTABLE CORE: denying_half, axis_terms, contradictions -- pure over
strings and parsed records, no build, no subprocess.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent.parent
RECORDS = REPO / "memory_graph" / "records"

# The axis vocabulary: CANONICAL AXIS -> the spellings that name it. Taken
# from the denial corpus and from the census rosters, not invented.
#
# DESIGN REVERSAL, and the reason this is a mapping rather than a flat tuple.
# The first cut of this file matched flat PHRASES, calibrated cleanly on the
# exemption cut, and then found NO CONTRADICTION on the very census that
# motivated it: al_dest_split.py writes `DECL-ORDER LEVER LIVE` while
# ProcessCritterList's denial writes `DECLARATION ORDER`. Two spellings of
# one axis, zero intersection, and a screen that reports "all clear" on its
# own seed case. A phrase screen answers "do these two texts share words";
# the question is "do they name the same axis".
AXIS_SPELLINGS = {
    "declaration-order": ("declaration order", "decl order", "decl-order",
                          "declaration ORDER"),
    "declaration-width": ("declaration width", "declaration WIDTH"),
    "statement-order": ("statement order", "statement-order",
                        "statement ORDER"),
    "extern-type": ("extern type", "extern TYPE"),
    "loop-form": ("loop form", "loop-form"),
    "pragma": ("pragma",),
    "inline": ("inline",),
    "cast": ("cast",),
}

# Where a scope stops denying and starts exempting. Each of these opens a
# clause that LIMITS the denial, so everything after it is out of scope for a
# contradiction. Ordered longest-first so the more specific phrase wins.
_EXEMPTION_OPENERS = (
    "it does not deny",
    "it denies nothing about",
    "does not deny",
    "denies nothing about",
    "it does not speak to",
    "does not speak to",
)


def denying_half(scope: str) -> str:
    """The part of a denial scope that actually DENIES something.

    Everything from the first exemption opener onward is dropped. Measured
    over the corpus by `--calibrate`: without this cut the screen fires on
    clauses whose whole purpose is to say the axis is NOT denied.
    """
    text = str(scope or "")
    low = text.lower()
    cut = len(text)
    for opener in _EXEMPTION_OPENERS:
        index = low.find(opener)
        if index != -1:
            cut = min(cut, index)
    return text[:cut]


def axis_terms(text: str) -> set[str]:
    """Which CANONICAL axes this text names, under any known spelling."""
    low = str(text or "").lower()
    return {axis for axis, spellings in AXIS_SPELLINGS.items()
            if any(spelling.lower() in low for spelling in spellings)}


def _load_records() -> list[dict]:
    out = []
    for path in RECORDS.rglob("*.json"):
        try:
            record = json.loads(path.read_text(encoding="utf-8-sig"))
        except (OSError, ValueError):
            continue
        if isinstance(record, dict):
            out.append(record)
    return out


def _denial_of(record: dict) -> dict | None:
    denial = record.get("denial")
    if not isinstance(denial, dict):
        denial = (record.get("attributes") or {}).get("denial")
    return denial if isinstance(denial, dict) else None


def live_denials(records: list[dict] | None = None) -> dict[str, list[dict]]:
    """function name -> its accepted, NOT-superseded typed denials.

    A superseded record's denial is not a veto (AGENTS.md: check
    `superseded_by` before applying an old law), so a screen that ignored
    supersession would resurrect retired vetoes.
    """
    records = _load_records() if records is None else records
    superseded = set()
    for record in records:
        value = record.get("supersedes")
        for cited in (value if isinstance(value, list) else [value]):
            if cited:
                superseded.add(cited)
    out: dict[str, list[dict]] = {}
    for record in records:
        denial = _denial_of(record)
        if not denial or record.get("id") in superseded:
            continue
        subject = str(record.get("function") or "")
        if not subject.startswith("function:"):
            continue
        name = subject.split(":", 1)[1]
        out.setdefault(name, []).append(
            {"record_id": record.get("id"), "denial": denial})
    return out


def _rows_from_census(payload) -> list[dict]:
    """Harvest {fn, text} rows out of ANY census JSON shape."""
    rows: list[dict] = []

    def walk(node):
        if isinstance(node, dict):
            name = None
            for key in ("fn", "function", "name", "symbol"):
                value = node.get(key)
                if isinstance(value, str) and re.fullmatch(
                        r"[A-Za-z_][A-Za-z0-9_]*", value):
                    name = value
                    break
            if name:
                text = " ".join(str(v) for v in node.values()
                                if isinstance(v, str))
                rows.append({"fn": name, "text": text,
                             "unit": node.get("unit")})
            for value in node.values():
                walk(value)
        elif isinstance(node, list):
            for value in node:
                walk(value)

    walk(payload)
    return rows


def contradictions(rows: list[dict], denials: dict[str, list[dict]],
                   axis_override: str | None = None) -> list[dict]:
    out = []
    for row in rows:
        row_terms = (axis_terms(axis_override) if axis_override
                     else axis_terms(row.get("text", "")))
        if not row_terms:
            continue
        for entry in denials.get(row["fn"], []):
            scope = denying_half(entry["denial"].get("scope"))
            shared = row_terms & axis_terms(scope)
            if shared:
                out.append({
                    "fn": row["fn"],
                    "unit": row.get("unit"),
                    "axes": sorted(shared),
                    "record_id": entry["record_id"],
                    "expiry_check": entry["denial"].get("expiry_check"),
                    "row_text": row.get("text", "")[:160],
                })
    return out


def calibrate() -> int:
    records = _load_records()
    denials = live_denials(records)
    total_blocks = sum(1 for r in records if _denial_of(r))
    print(f"accepted records carrying a typed denial: {total_blocks}")
    print(f"live (not superseded) function-anchored denials: "
          f"{sum(len(v) for v in denials.values())} on {len(denials)} "
          f"function(s)")
    print()
    print("canonical axis occurrence in live denial scopes "
          "(whole scope vs DENYING half):")
    whole = dict.fromkeys(AXIS_SPELLINGS, 0)
    cut = dict.fromkeys(AXIS_SPELLINGS, 0)
    for entries in denials.values():
        for entry in entries:
            scope = str(entry["denial"].get("scope") or "")
            for term in axis_terms(scope):
                whole[term] += 1
            for term in axis_terms(denying_half(scope)):
                cut[term] += 1
    for term in AXIS_SPELLINGS:
        dropped = whole[term] - cut[term]
        note = f"   <- {dropped} dropped as EXEMPTION clauses" if dropped else ""
        print(f"  {term:<20} whole {whole[term]:<4} denying {cut[term]:<4}{note}")
    print(f"\ntotals: {sum(whole.values())} axis mentions, "
          f"{sum(cut.values())} in a denying clause, "
          f"{sum(whole.values()) - sum(cut.values())} dropped as exemptions")
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--census", help="census JSON to screen (any shape "
                                         "carrying function names)")
    parser.add_argument("--function", action="append", default=[],
                        help="screen one function by name (repeatable)")
    parser.add_argument("--axis", help="axis phrase to screen for; required "
                                       "with --function, optional override "
                                       "with --census")
    parser.add_argument("--calibrate", action="store_true",
                        help="print the corpus numbers this screen rests on")
    parser.add_argument("--quiet-ok", action="store_true",
                        help="exit 0 even when contradictions are found")
    args = parser.parse_args(argv)

    if args.calibrate:
        return calibrate()

    denials = live_denials()
    if args.census:
        path = Path(args.census)
        if not path.is_absolute():
            path = REPO / path
        if not path.exists():
            print(f"missing census {path}")
            return 2
        rows = _rows_from_census(json.loads(path.read_text(encoding="utf-8")))
        source = str(path)
    elif args.function:
        if not args.axis:
            print("--function needs --axis")
            return 2
        rows = [{"fn": name, "text": args.axis} for name in args.function]
        source = "--function"
    else:
        parser.print_help()
        return 2

    found = contradictions(rows, denials, axis_override=args.axis)
    print(f"screened {len(rows)} row(s) from {source} against "
          f"{sum(len(v) for v in denials.values())} live denial(s) on "
          f"{len(denials)} function(s)")
    if not found:
        # A verdict, never silence (run-50 item 3, run-56 item 3).
        print("NO CONTRADICTION: no screened row advertises an axis that a "
              "live typed denial on the same function denies.")
        return 0
    print(f"\nCONTRADICTIONS: {len(found)}")
    for row in found:
        print(f"  {str(row['unit'] or ''):<28} {row['fn']}")
        print(f"      row advertises: {row['row_text']}")
        print(f"      denied axis:    {', '.join(row['axes'])}")
        print(f"      by:             {row['record_id']}")
        print(f"      expiry_check:   {str(row['expiry_check'])[:200]}")
    print("\nA contradiction is not proof the census is wrong: a denial can "
          "have expired. RUN each expiry_check above, then either re-measure "
          "the row or supersede the denial -- do not silently prefer one.")
    return 0 if args.quiet_ok else 1


if __name__ == "__main__":
    sys.exit(main())
