#!/usr/bin/env python3
"""Repo-wide near-miss work queue from objdiff's report.json.

Lists every function whose fuzzy match is >= threshold but < 100%, sorted
closest-first: these are the "one pad / one decl-order away" wins that the
per-TU views never surface.

Usage (from repo root):
  python tools/gdl/nearmiss.py                # >= 90%, closest first
  python tools/gdl/nearmiss.py --min 95       # tighter queue
  python tools/gdl/nearmiss.py --refresh      # regenerate report.json first
  python tools/gdl/nearmiss.py --grep sfx     # one TU family
  python tools/gdl/nearmiss.py --parked skip  # hide graph-parked functions

Parked caps come from the project memory graph: attempt records whose outcome
is 'parked' or 'capped' (residuals already diagnosed as allocator-quirk
walls). Default is to mark them [PARKED] rather than hide, so the queue stays
honest.

`--parked skip` HIDES rows, and it hides a lot of them: 142 of the 221 rows
in the >= 90 band, including several above 99% (measured run 43). A row that
vanishes for this reason used to leave no trace, which reads exactly like a
tool dropping rows; the footer now prints the in-band total, the hidden
count, and what "parked" was read from.

Every row carries `rec=N`, the number of attempt records the memory graph
holds for that function. AGENTS.md's close-lane screen ranks candidates by
records-per-unmatched-function — rec=0 is genuinely unexplored, rec=5 is
where five lanes already spent their probes — and the run-42 close lane had
to reconstruct that column by hand.

--residuals prints `real=N`, which is `fndiff --count`'s real (raw diff rows
with every relocation line dropped) — the same number probe.py prints and the
one work orders and attempt records quote — and RANKS on it. It used to print
and rank on `fndiff --clean`'s differently-computed real under the
unexplained label `d=`: measured over the live 219-row queue the two
disagree on 140 rows and 177 of the 219 positions move when ranked on the
arbiter every other tool quotes. `clean=N` is printed beside it only when the
two disagree, so a record quoting either number still resolves to this row.
"""

import argparse
import difflib
import json
import subprocess
import sys
from pathlib import Path

from fndiff import (classify_function, count_real, normalized_reloc_lines,
                    parse)

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parent.parent.parent
REPORT = REPO / "build" / VERSION / "report.json"
def load_graph_facts():
    """(parked names, {name: attempt-record count}) from the memory graph.

    Attempt history is immutable, so a re-triage or successful revisit records
    a new attempt that supersedes the old cap.  Only unsuperseded heads may
    suppress queue entries.

    The COUNT is the second half (run-43 item 3).  AGENTS.md's close-lane
    screen says to rank candidates by records-per-unmatched-function, and the
    run-42 close lane had to reconstruct that by hand, one `gdlmem context`
    per candidate: a zero-record row is genuinely unexplored, while a
    five-record row is where five lanes already spent their probes.
    """
    sys.path.insert(0, str(REPO))
    try:
        from memory_graph.core import ensure_database, open_database

        ensure_database(REPO)
        connection = open_database(REPO)
    except Exception as error:  # graph unavailable: honest empty cap set
        print(f"nearmiss: memory graph unavailable ({error}); no parked caps"
              " and no record counts", file=sys.stderr)
        return set(), {}
    try:
        parked = {row[0] for row in connection.execute(
            "SELECT e.name FROM attempt a"
            " JOIN entity e ON e.id = a.function_entity_id"
            " WHERE a.outcome IN ('parked', 'capped')"
            " AND NOT EXISTS (SELECT 1 FROM record_ingest newer"
            " WHERE json_extract(newer.raw_json, '$.supersedes') = a.record_id"
            " AND newer.record_state = 'accepted')"
        ).fetchall()}
        counts = {row[0]: row[1] for row in connection.execute(
            "SELECT e.name, COUNT(*) FROM attempt a"
            " JOIN entity e ON e.id = a.function_entity_id"
            " GROUP BY e.name"
        ).fetchall()}
        return parked, counts
    finally:
        connection.close()


def load_parked():
    """Just the parked names — `lowmatch.py` and its tests import this."""
    return load_graph_facts()[0]


def residual_columns(target, base):
    """(real, clean, category) for one function's two parsed line lists.

    THE COLUMN IS probe.py's `real` (run-41 item 6). Two different
    computations are both called `real` in this project: raw diff rows minus
    every relocation line (what `fndiff --count` and probe.py report, and
    what work orders and attempt records quote), and rows over
    reloc-NORMALIZED text (what `fndiff --clean` reports). This queue used to
    print and RANK on the second under the unexplained label `d=`. Measured
    over the live 219-row queue: the two disagree on 140 rows and 177 of the
    219 positions change when ranked on the arbiter every other tool quotes
    (AudioSetupBossStreams 1523 vs 1297; PlayerMotion 4168 vs 3982).
    """
    clean_rows = [line for line in difflib.unified_diff(
        normalized_reloc_lines(target), normalized_reloc_lines(base),
        lineterm="", n=0)
        if line[:1] in "+-" and line[:3] not in ("+++", "---")]
    raw_rows = [line for line in difflib.unified_diff(
        target, base, lineterm="", n=0)
        if line[:1] in "+-" and line[:3] not in ("+++", "---")]
    return (count_real(raw_rows), len(clean_rows),
            classify_function(target, base))


def format_residual(real, clean, category, residuals):
    """The residual columns of one queue row."""
    if real is None:
        return "  real=???" if residuals else ""
    text = f"  real={real:4d}"
    text += f" clean={clean:<4d}" if clean != real else " " * 11
    return text + f" {category:<18}"


def format_row(pct, size, residual, records, name, unit, tag):
    return (f"{pct:6.2f}%  {size:5d}B{residual}  rec={records:<2d}"
            f"  {name:<40} {unit}{tag}")


def summary_line(shown, hidden, parked_total, minimum):
    """The footer. A hidden row must be counted where it was hidden.

    Before run 43 this printed only `shown` and then cited a `PARKED.txt`
    the tool had not read since the parks moved into the memory graph, so
    `--parked skip` dropping 142 of 221 rows looked exactly like a queue
    tool losing rows.
    """
    return (f"--- {shown} near-miss fns (>= {minimum}%, < 100%)"
            f" | {shown + hidden} in band"
            f" | {hidden} hidden by --parked skip"
            f" | {parked_total} functions carry a live parked/capped attempt"
            f" record in the memory graph"
            f" | rec=N is that function's attempt-record count ---")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--min", type=float, default=90.0, metavar="PCT",
                    help="lower fuzzy bound (default 90)")
    ap.add_argument("--refresh", action="store_true",
                    help="regenerate report.json (ninja) before reading")
    ap.add_argument("--grep", metavar="STR", help="only TUs whose name contains STR")
    ap.add_argument("--parked", choices=["mark", "skip"], default="mark",
                    help="parked-cap handling (default: mark)")
    ap.add_argument("--residuals", action="store_true",
                    help="measure real object-diff lines and sort cheapest first")
    args = ap.parse_args()

    if args.refresh:
        r = subprocess.run(["ninja", f"build/{VERSION}/report.json"], cwd=str(REPO))
        if r.returncode:
            print("ninja report.json FAILED -- fix the build before trusting this queue",
                  file=sys.stderr)
            return 1

    if not REPORT.exists():
        print(f"no {REPORT} -- run with --refresh", file=sys.stderr)
        return 1

    parked, record_counts = load_graph_facts()
    rows = []
    for u in json.loads(REPORT.read_text()).get("units", []):
        unit = u.get("name", "").removeprefix("main/")
        if args.grep and args.grep not in unit:
            continue
        # Matching (linked) TUs are byte-proven by the link itself: any <100%
        # fuzzy inside them is reloc-name scoring noise, NOT a near-miss.
        # Editing their source based on fuzzy% BREAKS REAL DOL BYTES.
        if u.get("metadata", {}).get("complete"):
            continue
        target_fns = base_fns = None
        if args.residuals:
            target_obj = REPO / "build" / VERSION / "obj" / f"{unit}.o"
            base_obj = REPO / "build" / VERSION / "src" / f"{unit}.o"
            if target_obj.exists() and base_obj.exists():
                target_fns = parse(target_obj)
                base_fns = parse(base_obj)
        for f in u.get("functions", []):
            pct = f.get("fuzzy_match_percent", 0.0)
            if pct >= args.min and pct < 100.0:
                name = f.get("name", "?")
                size = int(f.get("size", 0) or 0)
                real = None
                clean = None
                category = None
                if target_fns is not None:
                    target = target_fns.get(name)
                    base = base_fns.get(name)
                    if target is not None and base is not None:
                        real, clean, category = residual_columns(target, base)
                rows.append((pct, size, name, unit, real, category, clean))

    if args.residuals:
        rows.sort(key=lambda r: (r[4] is None, r[4] or 0, -r[1], -r[0]))
    else:
        rows.sort(key=lambda r: (-r[0], r[1]))
    if args.residuals:
        print("legend: real=N is `fndiff --count`'s real — raw diff rows with"
              " every relocation line dropped — which is the number probe.py"
              " prints and the one every work order quotes; the queue is"
              " ranked on it. clean=N appears only when `fndiff --clean`'s"
              " differently-computed real disagrees, so a record quoting"
              " either can be matched to this row.")
    shown = hidden = 0
    for pct, size, name, unit, real, category, clean in rows:
        tag = ""
        if name in parked:
            if args.parked == "skip":
                hidden += 1
                continue
            tag = "  [PARKED]"
        residual = format_residual(real, clean, category, args.residuals)
        print(format_row(pct, size, residual, record_counts.get(name, 0),
                         name, unit, tag))
        shown += 1
    print(summary_line(shown, hidden, len(parked), args.min))
    return 0


if __name__ == "__main__":
    sys.exit(main())
