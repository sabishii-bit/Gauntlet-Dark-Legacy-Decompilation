#!/usr/bin/env python3
"""EV lane (run 32): the mandatory canary for the law evidence-scoring layer.

WHY THIS EXISTS. A ranking layer over the law corpus is only worth shipping
if it agrees with what the project already knows. A layer that floats a
known-REFUTED law above a known-WINNER is worse than no layer at all: it
launders a measured-false claim into an authoritative-looking recommendation,
and every lane downstream pays for it.

So this tool hand-scores fifteen laws spanning the outcome space — laws whose
status is established by the corpus's own refutation records and landing
history, not by the scorer — and asserts that the SHIPPED layer reproduces
them. It checks three independent things:

  1. TIER agreement: the shipped status for each canary law equals the hand
     status.
  2. INDEPENDENT RECOMPUTATION: a second implementation that reads the raw
     JSON records directly (no SQL, no core.py scoring code) reproduces the
     shipped successes/failures counts exactly. A scorer agreeing with itself
     proves nothing; this is the part that catches an import-side bug.
  3. THE ORDERING INVARIANT: every known-winner ranks strictly above every
     known-refuted and known-contested law in the deterministic view. This is
     the assertion the whole layer exists to satisfy.

Run from the repository root:
    python tools/gdl/composed_census/ev_law_score_canary.py

Exit 0 = canary green. Exit 1 = the layer disagrees with the corpus; do not
ship it. `--table` prints the full comparison table for a report.
"""

from __future__ import annotations

import argparse
import json
import sqlite3
import sys
from collections import Counter
from contextlib import closing
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO_ROOT))

from memory_graph.core import (  # noqa: E402
    LAW_STATUS_ORDER,
    LAW_SUCCESS_OUTCOMES,
    _is_law_record,
    _law_id_list,
    _refuted_ids,
    ensure_database,
    law_corpus,
    law_evidence_score,
    open_database,
)

# ---------------------------------------------------------------------------
# THE HAND SCORES.
#
# Each row is (law-id, hand-status, role, why-this-status-was-assigned-by-hand).
# The status was assigned by reading each law's own evidence trail in
# memory_graph/records — which records refute it, and which citing attempts
# actually landed — BEFORE consulting the shipped layer's output. The `why`
# text is the audit trail: a future lane can re-derive any row from the named
# records without trusting this file.
#
# ROLES: "refuted" and "contested" are the known-refuted set the run-32 work
# order names (the SI epilogue-target law and the run-30 refutations);
# "winner" is the known-winner set (merged-disjunction, region-arithmetic, and
# the corpus's heaviest-landing laws); "provisional" is the unverified control.
# ---------------------------------------------------------------------------
CANARY: tuple[tuple[str, str, str, str], ...] = (
    # --- known-refuted: measured false, and carrying no landing to offset it
    (
        "claim.law.SI_forward-branch-pair-needs-an-epilogue-target-not-a-shared"
        "-interior-block.20260901.v1",
        "refuted", "refuted",
        "the work order's named example. Target of a refutes edge from"
        " attempt.SI_registeritemwobj-save-set-delta-reclass-cap; no citing"
        " attempt landed exact/improved.",
    ),
    (
        "claim.law.frame-bottom-region-is-outgoing-args-not-a-dead-local"
        ".20260831.v1",
        "refuted", "refuted",
        "refuted by a later frame-region record; zero landings. A frame law"
        " that is wrong about what the bottom region IS mis-steers every"
        " frame-delta probe that cites it.",
    ),
    (
        "claim.law.addr16-lo-home-copy-census-no-source-discriminant"
        ".20260831.v1",
        "refuted", "refuted",
        "refuted census premise, zero landings — the 'no source discriminant'"
        " conclusion is exactly the kind of negative that must not keep"
        " screening work out after it was overturned.",
    ),
    (
        "claim.law.callee-saved-home-copy-coalescing-is-source-unreachable"
        ".20260831.v1",
        "contested", "refuted",
        "a source-unreachability law that was later refuted, but which has one"
        " landing behind it. Contested, not refuted: the refutation may be"
        " scope-limited, and the reader must go look.",
    ),
    (
        "claim.law.per-lib-pragma-and-flag-map.20260829.v1",
        "contested", "refuted",
        "the per-lib pragma map was refuted in the run-30 pragma work (the"
        " pragma premise was measured load-bearing TU-wide, not per-lib), and"
        " it still carries a landing.",
    ),
    (
        "claim.law.dont-inline-pragma-is-function-granular-and-unoverridable"
        ".20260831.v1",
        "contested", "refuted",
        "same pragma family, same shape: one landing, one standing"
        " refutation.",
    ),
    (
        "claim.law.narrow-type-buys-rlwimi-insert-and-pays-a-masked-shift"
        ".20260901.v1",
        "contested", "refuted",
        "refuted by the fn800d87fc u16-range-mask work; one landing survives.",
    ),
    (
        "claim.law.offsetof-rename-isolated-site-outlier.20260830.v1",
        "contested", "refuted",
        "THE HARD ROW. Eleven landings and one refutation — its raw Wilson"
        " score (0.65) beats every low-n winner in this table. If tier did not"
        " outrank score, this refuted law would sit above"
        " merged-disjunction, which is the precise failure the canary"
        " forbids.",
    ),
    # --- known-winners
    (
        "claim.law.DE_merged-disjunction-unfuses-a-forward-guard-pair-only-into"
        "-the-epilogue.20260901.v1",
        "established", "winner",
        "the work order's named winner. Cited by"
        " attempt.DE_fn-8004db3c-combined-disjunction-exact, which went EXACT."
        " Low n by design: a recent law with one clean landing.",
    ),
    (
        "claim.law.PE_localise-a-frame-delta-by-region-arithmetic-not-by-the"
        "-exclusive-slot-list.20260901.v1",
        "established", "winner",
        "the work order's second named winner (region-arithmetic). One"
        " landing, nothing against it.",
    ),
    (
        "claim.law.offsetof-rename-preserves-protected-web.20260830.v1",
        "established", "winner",
        "the corpus's heaviest earner: the de-fakematch campaign's workhorse,"
        " with landings in the dozens and no refutation. Expected rank 1.",
    ),
    (
        "claim.law.C1_permute-recolor-composition-needs-a-permutation-legal-in"
        "-our-colouring.20260901.v1",
        "established", "winner",
        "the composed-permutation law that governs the WebFrank path; heavy"
        " landing history, unrefuted.",
    ),
    (
        "claim.law.multifield-alias-defeats-indexed-addressing.20260830.v2",
        "established", "winner",
        "a v2 addressing law with a long unrefuted landing history.",
    ),
    (
        "claim.law.identical-multiset-is-blind-to-displacements.20260831.v1",
        "established", "winner",
        "THE DENOMINATOR ROW. The single most-cited law in the corpus, but"
        " most of those citations are PARKS it correctly predicted. It must"
        " score on its landings only, not on its citation count — if n here"
        " equals cited_total, the layer is counting parks as successes.",
    ),
    # --- provisional control
    (
        "claim.law.GS_a-record-mining-gate-refuses-the-record-that-documents-it"
        ".20260901.v1",
        "provisional", "provisional",
        "the run-29 graph lane's own law. True, well-evidenced in prose, and"
        " cited by NO attempt — so the layer must call it provisional and"
        " hide it by default. Being right is not the same as being verified"
        " by this corpus's own success signal.",
    ),
)


def independent_tally(root: Path) -> dict[str, dict[str, int]]:
    """Recompute successes/failures straight from the JSON. No SQL, no scorer.

    Deliberately a separate implementation from core._derive_law_evidence: the
    point is to catch an IMPORT-side defect, which a check sharing the import
    path could not see. (It found one: the importer accepted only the list
    spelling of laws_applied and was dropping 92.6% of the corpus's
    citations.)
    """
    records: dict[str, dict] = {}
    for base in ("records", "inbox"):
        directory = root / "memory_graph" / base
        if not directory.exists():
            continue
        for path in directory.rglob("*.json"):
            try:
                record = json.loads(path.read_text(encoding="utf-8-sig"))
            except (OSError, json.JSONDecodeError):
                continue
            if isinstance(record, dict) and isinstance(record.get("id"), str):
                records[record["id"]] = record

    successes: Counter[str] = Counter()
    failures: dict[str, set[str]] = {}
    neutral: Counter[str] = Counter()
    cited: Counter[str] = Counter()
    for rid, record in records.items():
        if record.get("kind") == "attempt":
            landed = str(record.get("outcome", "")).lower() in LAW_SUCCESS_OUTCOMES
            for law_id in _law_id_list(record, "laws_applied"):
                cited[law_id] += 1
                if landed:
                    successes[law_id] += 1
                else:
                    neutral[law_id] += 1
            for law_id in _law_id_list(record, "laws_failed"):
                failures.setdefault(law_id, set()).add(rid)
        for refuted in _refuted_ids(record):
            failures.setdefault(refuted, set()).add(rid)

    keys = set(successes) | set(failures) | set(neutral)
    return {
        key: {
            "successes": successes.get(key, 0),
            "failures": len(failures.get(key, set())),
            "neutral": neutral.get(key, 0),
            "cited": cited.get(key, 0),
        }
        for key in keys
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=REPO_ROOT)
    parser.add_argument("--table", action="store_true",
                        help="print the full comparison table")
    args = parser.parse_args()
    root = args.root.resolve()

    ensure_database(root, None)
    with closing(open_database(root, None)) as connection:
        shipped = {
            row["law_record_id"]: dict(row)
            for row in connection.execute(
                "SELECT law_record_id, successes, failures, neutral_citations,"
                " cited_total FROM law_evidence").fetchall()
        }
    hand_tally = independent_tally(root)

    # The deterministic view is what a lane actually reads.
    ranked = law_corpus(root=root, limit=500, include_provisional=1)["laws"]
    rank_of = {row["id"]: index + 1 for index, row in enumerate(ranked)}
    default_view = {
        row["id"] for row in law_corpus(root=root, limit=500)["laws"]
    }

    failures: list[str] = []
    rows: list[dict] = []
    for law_id, hand_status, role, why in CANARY:
        db_row = shipped.get(law_id, {})
        s = int(db_row.get("successes", 0))
        f = int(db_row.get("failures", 0))
        derived = law_evidence_score(s, f)
        independent = hand_tally.get(
            law_id, {"successes": 0, "failures": 0, "neutral": 0, "cited": 0})
        rows.append({
            "id": law_id, "role": role, "hand_status": hand_status,
            "derived_status": derived["status"], "s": s, "f": f,
            "n": derived["n"], "wilson": derived["wilson"],
            "beta": derived["beta_mean"],
            "cited_total": int(db_row.get("cited_total", 0)),
            "neutral": int(db_row.get("neutral_citations", 0)),
            "rank": rank_of.get(law_id),
            "in_default_view": law_id in default_view,
            "independent": independent, "why": why,
        })

        # (0) the law must exist at all — a typoed canary id passes vacuously.
        if law_id not in rank_of:
            failures.append(f"canary law not present in the corpus: {law_id}")
            continue
        # (1) tier agreement
        if derived["status"] != hand_status:
            failures.append(
                f"TIER MISMATCH {law_id}: hand={hand_status}"
                f" derived={derived['status']} (s={s} f={f})")
        # (2) independent recomputation
        if (independent["successes"], independent["failures"]) != (s, f):
            failures.append(
                f"INDEPENDENT RECOUNT MISMATCH {law_id}: shipped s={s} f={f}"
                f" vs json-direct s={independent['successes']}"
                f" f={independent['failures']}")
        # (3) provisional laws must be hidden from the default view, and
        #     everything else must be visible in it.
        hidden = law_id not in default_view
        if (derived["status"] == "provisional") != hidden:
            failures.append(
                f"VISIBILITY MISMATCH {law_id}: status={derived['status']}"
                f" but in_default_view={not hidden}")

    # (4) THE ORDERING INVARIANT.
    winners = [row for row in rows if row["role"] == "winner" and row["rank"]]
    losers = [row for row in rows
              if row["role"] == "refuted" and row["rank"]]
    if winners and losers:
        worst_winner = max(row["rank"] for row in winners)
        best_loser = min(row["rank"] for row in losers)
        if best_loser <= worst_winner:
            offender = min(losers, key=lambda row: row["rank"])
            victim = max(winners, key=lambda row: row["rank"])
            failures.append(
                "ORDERING VIOLATION: known-refuted"
                f" {offender['id']} ranks {offender['rank']}, above"
                f" known-winner {victim['id']} at {victim['rank']}")
    else:
        failures.append("ORDERING INVARIANT UNCHECKABLE: canary lost a role")

    # (5) the denominator row: n must be landings, never citation count.
    denominator = next(
        (row for row in rows
         if row["id"].startswith("claim.law.identical-multiset")), None)
    if denominator is None:
        failures.append("denominator canary row missing")
    elif denominator["n"] >= denominator["cited_total"]:
        failures.append(
            "DENOMINATOR VIOLATION: parks are being counted as successes"
            f" (n={denominator['n']} cited_total={denominator['cited_total']})")

    if args.table or failures:
        width = max(len(row["id"]) for row in rows)
        print(f"{'ROLE':<12} {'HAND':<12} {'DERIVED':<12} {'S':>4} {'F':>3}"
              f" {'N':>4} {'WILSON':>7} {'BETA':>6} {'CITED':>6} {'RANK':>5}"
              f"  LAW")
        for row in sorted(rows, key=lambda r: (r["rank"] or 9999)):
            mark = "  " if row["hand_status"] == row["derived_status"] else "!!"
            print(f"{mark}{row['role']:<10} {row['hand_status']:<12}"
                  f" {row['derived_status']:<12} {row['s']:>4} {row['f']:>3}"
                  f" {row['n']:>4} {row['wilson']:>7.4f} {row['beta']:>6.3f}"
                  f" {row['cited_total']:>6} {str(row['rank']):>5}"
                  f"  {row['id'][:width]}")
        print()
        print(f"corpus: {len(shipped)} scored records;"
              f" tiers = {' < '.join(LAW_STATUS_ORDER)}")
        print(f"deterministic view holds {len(default_view)} laws;"
              f" {len(ranked) - len(default_view)} provisional are hidden")

    if failures:
        print("\nCANARY RED — do not ship the scoring layer:")
        for line in failures:
            print(f"  - {line}")
        return 1
    print(f"\nCANARY GREEN: {len(rows)}/{len(rows)} hand scores reproduced;"
          " every known-winner outranks every known-refuted law.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
