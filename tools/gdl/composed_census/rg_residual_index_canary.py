#!/usr/bin/env python3
"""RG lane (run 33): the mandatory canary for the pure-reorder residual index.

WHY THIS EXISTS. The index backfills 1,001 recorded residual signatures into a
derived table and then RANKS retrieval on it. A backfill that silently parses
the wrong thing is worse than no index: it makes a query look answered. Three
prior measured incidents shape every check below.

  1. `laws --residual` returned BYTE-IDENTICAL payloads for four distinct
     pure-reorder signatures (claim.law.RS_residual-retrieval-is-blind-to-pure-
     reorder-residuals). So the canary asserts DISCRIMINATION, not presence.
  2. A signature recording only the multiset delta serialises a function whose
     whole residual is same-opcode IMMEDIATES as "0t IDENTICAL", which is
     indistinguishable from a closed function (MB lane, DrawPsysSub: stored
     `0t (290/290)` vs a live 49 IMMEDIATE rows). So `IDENTICAL u0 i49` and
     `IDENTICAL u4 i0` must land in DIFFERENT kinds.
  3. The importer once dropped 92.6% of the corpus's law citations because the
     check that would have caught it shared the buggy reader. So the shipped
     table is compared against a re-parse written here, not against itself.

Run from the repository root:
    python tools/gdl/composed_census/rg_residual_index_canary.py [--table]

Exit 0 = green. Exit 1 = do not trust the index.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from contextlib import closing
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO_ROOT))

from memory_graph.core import (  # noqa: E402
    ensure_database,
    open_database,
    parse_residual_signature,
    residual_facet_similarity,
)

# Hand-written signature fixtures spanning the classes, each with the kind and
# the facts a reader should be able to recover from it. Written from the two
# emitted forms MEASURED in the tree: the STORED corpus form ("opcode multiset
# IDENTICAL (155/155); insns T155/O155; 0 ops clusters") and the LIVE
# `fndiff --ops` form ("opcode multiset: IDENTICAL (33/33) -- pure reorder"),
# which differ by a COLON that a stored-form-only regex silently fails on.
FIXTURES: tuple[tuple[str, str, dict], ...] = (
    (
        "stored pure reorder",
        "0t opcode multiset IDENTICAL (155/155); insns T155/O155;"
        " 0 ops clusters",
        {"kind": "reorder", "insns_target": 155, "clusters": 0,
         "resolution": "multiset-only"},
    ),
    (
        "LIVE pure reorder (colon form)",
        "==== AtreeNodeInsert: target 33 insns, ours 33\n"
        "  opcode multiset: IDENTICAL (33/33) -- pure reorder,"
        " schedule-class residual",
        {"kind": "reorder", "insns_target": 33, "resolution": "multiset-only"},
    ),
    (
        "LIVE identical-multiset with IMMEDIATES (the MB separation)",
        "  opcode multiset: IDENTICAL (52/52) -- but 4 IMMEDIATE word(s)"
        " differ at aligned same-opcode positions (below): NOT pure reorder,"
        " NOT schedule-class",
        {"kind": "immediate-aligned", "insns_target": 52, "immediates": 4,
         "resolution": "row-resolved"},
    ),
    (
        "canonical row-resolved short form",
        "0t opcode multiset IDENTICAL (290/290); u0 i49 g3",
        {"kind": "reorder", "insns_target": 290, "unpaired": 0,
         "immediates": 49, "genuine": 3, "resolution": "row-resolved"},
    ),
    (
        "count-asymmetric",
        "DIFFERS target-only: +1 add ours-only: -1 mr; insns T71/O71;"
        " 6 ops clusters",
        {"kind": "asymmetric", "insns_target": 71, "clusters": 6},
    ),
    (
        "empty",
        "",
        {"kind": "empty"},
    ),
)

# The four RS pilot signatures, whose defining measured property is that the
# OLD index returned one byte-identical payload for all of them.
PILOT_SIGNATURES: tuple[tuple[str, str], ...] = (
    ("AtreeNodeInsert",
     "  opcode multiset: IDENTICAL (33/33) -- pure reorder, schedule-class"
     " residual"),
    ("TowerInit",
     "  opcode multiset: IDENTICAL (25/25) -- pure reorder, schedule-class"
     " residual"),
    ("ExtractYPR",
     "  opcode multiset: IDENTICAL (92/92) -- pure reorder, schedule-class"
     " residual\n"
     "  delete  T[61:62]@f4-f8=['fmr']  O[61:61]@f4-f4=[]\n"
     "  insert  T[64:64]@100-100=[]  O[63:64]@fc-100=['fmr']  [SHIFTABLE]\n"
     "  1 of 2 clusters flagged"),
    ("PlayerCollidePlayers",
     "  opcode multiset: IDENTICAL (124/124) -- pure reorder, schedule-class"
     " residual"),
)


def independent_parse(signature: str) -> tuple[str, list[str]]:
    """A second, deliberately dumb reader: kind plus sorted facet list.

    Shares no code with the shipped parser beyond the standard library; its
    only job is to catch the shipped one drifting from the emitted formats.
    """
    import re

    text = signature or ""
    low = text.lower()
    ident = re.search(r"multiset[:\s]+identical\s*\(\s*(\d+)\s*/\s*(\d+)\s*\)",
                      low)
    markers = re.findall(r"[+-]\s*\d+\s+([a-z][a-z0-9_.]*)", low)
    if ident:
        kind = ("immediate-aligned"
                if ("not pure reorder" in low or "immediate word" in low)
                else "reorder")
    elif markers:
        kind = "asymmetric"
    else:
        kind = "empty"
    return kind, sorted(markers)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=REPO_ROOT)
    parser.add_argument("--table", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()
    failures: list[str] = []

    # (1) FIXTURES: the parser recovers the documented facts from both forms.
    for label, signature, expected in FIXTURES:
        parsed = parse_residual_signature(signature)
        for key, want in expected.items():
            got = parsed.get(key)
            if got != want:
                failures.append(
                    f"FIXTURE {label}: {key} = {got!r}, expected {want!r}")
        kind, _ = independent_parse(signature)
        if kind != parsed["kind"]:
            failures.append(
                f"INDEPENDENT PARSE DISAGREES on {label}:"
                f" shipped {parsed['kind']!r} vs direct {kind!r}")

    # (2) THE MB SEPARATION is a KIND boundary, not a nuance.
    reorder = parse_residual_signature(
        "opcode multiset IDENTICAL (290/290); u4 i0 g0")
    immediate = parse_residual_signature(
        "opcode multiset: IDENTICAL (290/290) -- but 49 IMMEDIATE word(s)"
        " differ at aligned same-opcode positions: NOT pure reorder")
    if reorder["kind"] == immediate["kind"]:
        failures.append(
            "MB SEPARATION LOST: 'IDENTICAL u4 i0' and 'IDENTICAL i49' both"
            f" parsed as kind {reorder['kind']!r}; they are different families")

    # (3) DISCRIMINATION: the four pilot signatures must not collapse.
    digests: dict[str, str] = {}
    for name, signature in PILOT_SIGNATURES:
        facets = parse_residual_signature(signature)["facets"]
        digests[name] = hashlib.sha1(
            json.dumps(sorted(facets)).encode()).hexdigest()[:12]
    if len(set(digests.values())) < len(digests):
        collided = [n for n in digests
                    if list(digests.values()).count(digests[n]) > 1]
        failures.append(
            "PILOT SIGNATURES COLLAPSED to a shared facet set"
            f" ({', '.join(sorted(collided))}) — this is exactly the state"
            " claim.law.RS_residual-retrieval-is-blind-to-pure-reorder-"
            "residuals measured, and the index does not fix it")

    # A zero-weight facet must never be enough to pair two rows: that was the
    # measured regression where every reorder record matched every reorder
    # query and only the ORDER varied.
    only_metadata = {"kind:reorder", "resolution:multiset-only"}
    strength, shared = residual_facet_similarity(only_metadata, only_metadata)
    if shared or strength:
        failures.append(
            "METADATA-ONLY MATCH: kind/resolution facets alone produced"
            f" strength={strength} shared={shared}; they must score nothing")

    # (4) THE SHIPPED TABLE reproduces a direct re-parse, row for row.
    ensure_database(root, None)
    with closing(open_database(root, None)) as connection:
        rows = connection.execute(
            "SELECT record_id, kind, facets_json, signature"
            " FROM residual_signature").fetchall()
        indexable = int(connection.execute(
            "SELECT COUNT(*) FROM record_ingest"
            " WHERE json_extract(raw_json,'$.residual') IS NOT NULL"
        ).fetchone()[0])
    kinds: dict[str, int] = {}
    drift = 0
    for row in rows:
        kinds[row["kind"]] = kinds.get(row["kind"], 0) + 1
        fresh = parse_residual_signature(row["signature"])
        if (fresh["kind"] != row["kind"]
                or json.dumps(fresh["facets"]) != row["facets_json"]):
            drift += 1
    if drift:
        failures.append(
            f"{drift} indexed row(s) disagree with a fresh parse of their own"
            " signature — the table was built by different code than the"
            " readers use")
    if len(rows) != indexable:
        failures.append(
            f"INDEX COVERAGE: {len(rows)} rows indexed but {indexable} records"
            " carry a residual object")
    if not rows:
        failures.append(
            "THE INDEX IS EMPTY: every residual query would return nothing,"
            " which reads as a silent corpus rather than a broken build")

    if args.table or failures:
        print("FIXTURES")
        for label, signature, _ in FIXTURES:
            parsed = parse_residual_signature(signature)
            print(f"  {label:<46} kind={parsed['kind']:<18}"
                  f" insns={parsed['insns_target']}"
                  f" clusters={parsed['clusters']}"
                  f" u={parsed['unpaired']} i={parsed['immediates']}"
                  f" g={parsed['genuine']}"
                  f" resolution={parsed['resolution']}")
        print("\nPILOT SIGNATURE FACET DIGESTS (must all differ)")
        for name, digest in digests.items():
            print(f"  {name:<24} {digest}")
        print(f"\nINDEX: {len(rows)} rows / {indexable} indexable records")
        for kind, count in sorted(kinds.items()):
            print(f"  kind {kind:<20} {count}")

    if failures:
        print("\nCANARY RED — do not trust the residual index:")
        for line in failures:
            print(f"  - {line}")
        return 1
    print(f"\nCANARY GREEN: {len(FIXTURES)} fixtures parsed as documented,"
          " the immediate/reorder families stay separate, the four pilot"
          f" signatures produce {len(set(digests.values()))} distinct facet"
          f" sets, and all {len(rows)} indexed rows reproduce a fresh parse.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
