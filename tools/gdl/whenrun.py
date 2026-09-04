#!/usr/bin/env python3
"""Resolve a commit citation to a DATE, an AGE and a RUN NUMBER.

RUN-56 ITEM 5b, from CU's run-55 report: `"measured at c0f978273"` is
unreadable as an age, and nothing in the project resolved a commit to a run.
Records are anchored to commits on purpose (AGENTS.md: "Anchor records to
code paths, target addresses/hashes, reproducible commands, or immutable
commits — never to Markdown"), and that is the right anchor; it just could
not be READ. Freshness is how this project ranks two disagreeing records, so
an unreadable anchor is a record whose age cannot enter the ranking.

    $ python tools/gdl/whenrun.py c0f978273
    c0f978273  2026-09-03  age 1d  run 52  (reachable)
        Stage run-52 work claims (6 lanes; provenance repair mandate ...)

THE RUN NUMBER COMES FROM HISTORY, not from a table anyone maintains: the
integrator's dispatch commit is `Stage run-N work claims`, and a commit
belongs to the most recent such marker at or before it. Measured at
2a90f8403: 48 markers covering runs 5..56, with NO marker for runs 6, 8, 29
and 32 — so the answer is reported as `run N (floor: nearest staged marker)`
and never as an exact run when the span is ambiguous. A run with no marker
cannot be invented, and pretending otherwise would put a wrong number where
there is currently an honest blank.

TWO-SIDED CALIBRATION at 2a90f8403 over `memory_graph/records`:
  POSITIVE  1,144 records cite at least one hash-shaped token; 1,194
            distinct tokens; 956 of them (80%) are commits in this repo and
            resolve.
  NEGATIVE  238 (20%) are NOT commits — source sha1s from defake_gate
            baselines, object hashes, pin body digests, and commits that
            only ever existed on a worker branch. They must be reported as
            `not a commit here`, never guessed at: `--scan-records` prints
            both populations and the tool never treats a non-commit as one.

Usage (from the repo root):
  python tools/gdl/whenrun.py <hash> [<hash> ...]
  python tools/gdl/whenrun.py --scan-records [--limit N]
  python tools/gdl/whenrun.py --runs            # the marker table itself

IMPORTABLE CORE: run_markers, run_for_commit, resolve — pure over `git`
output, no build; importing this module has no side effects.
"""
from __future__ import annotations

import argparse
import datetime as _dt
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
RECORDS = REPO / "memory_graph" / "records"
HASH_TOKEN = re.compile(r"\b[0-9a-f]{9,40}\b")
MARKER = re.compile(r"Stage run-(\d+) work claims")


def _git(*args):
    proc = subprocess.run(["git", *args], cwd=REPO,
                          capture_output=True, text=True)
    return proc.returncode, proc.stdout.strip(), proc.stderr.strip()


def history(order="--topo-order"):
    """sha -> {index, date, subject} over HEAD's history, newest first.

    ONE git call for the whole tool. `--topo-order` is load-bearing: run
    ordering is an ANCESTRY question and the default reverse-chronological
    log can interleave branches, which is how the first cut of this file
    reported `c0f978273` — the run-52 staging commit ITSELF — as run 54.
    Runs 52, 53 and 54 all staged on 2026-09-03, so a date comparison picked
    the newest same-day marker and was wrong by two runs on the one input
    the item quotes.
    """
    code, out, _ = _git("log", order, "--format=%H%x09%ad%x09%s",
                        "--date=short")
    if code:
        return {}
    rows = {}
    for index, line in enumerate(out.splitlines()):
        parts = line.split("\t", 2)
        if len(parts) != 3:
            continue
        sha, date, subject = parts
        rows[sha] = {"index": index, "date": date, "subject": subject}
    return rows


def run_markers(hist=None):
    """[(run_number, sha, date, index)] newest first, from history alone."""
    hist = history() if hist is None else hist
    rows = []
    for sha, meta in hist.items():
        found = MARKER.search(meta["subject"])
        if found:
            rows.append((int(found.group(1)), sha, meta["date"],
                         meta["index"]))
    rows.sort(key=lambda row: row[3])
    return rows


def run_for_commit(index, markers):
    """The run of the nearest marker AT OR BEFORE this commit in history.

    A FLOOR, not an exact answer: four runs in the covered span staged no
    marker, so a commit after marker N may belong to run N or to a
    marker-less run after it. Saying so is the point.
    """
    if index is None:
        return None
    candidates = [row for row in markers if row[3] >= index]
    return candidates[0][0] if candidates else None


def resolve(token, markers=None, hist=None, today=None):
    hist = history() if hist is None else hist
    markers = run_markers(hist) if markers is None else markers
    code, full, _ = _git("rev-parse", "--verify", token)
    if code or full not in hist:
        code, kind, _ = _git("cat-file", "-t", token)
        if code or kind != "commit":
            return {"token": token, "is_commit": False,
                    "note": "not a commit object in this repository"}
        return {"token": token, "is_commit": True, "sha": full or token,
                "date": "", "age_days": None, "subject": "",
                "reachable_from_head": False, "run_floor": None,
                "note": "a commit, but not in HEAD's history"}
    meta = hist[full]
    day = today or _dt.date.today()
    try:
        age = (day - _dt.date.fromisoformat(meta["date"])).days
    except ValueError:
        age = None
    return {"token": token, "is_commit": True, "sha": full,
            "date": meta["date"], "age_days": age,
            "subject": meta["subject"], "reachable_from_head": True,
            "run_floor": run_for_commit(meta["index"], markers)}


def _format(row):
    if not row["is_commit"]:
        return f"{row['token']:<12} {row['note']}"
    age = "?" if row["age_days"] is None else f"{row['age_days']}d"
    run = ("run ?" if row["run_floor"] is None
           else f"run {row['run_floor']}")
    reach = "reachable" if row["reachable_from_head"] else "NOT on HEAD"
    return (f"{row['token']:<12} {row['date']}  age {age:<5} {run:<8}"
            f" ({reach})\n    {row['subject'][:100]}")


def scan_records(limit=None):
    hist = history()
    markers = run_markers(hist)
    seen = {}
    for path in sorted(RECORDS.rglob("*.json")):
        try:
            blob = path.read_text(encoding="utf-8-sig")
        except OSError:
            continue
        for token in set(HASH_TOKEN.findall(blob)):
            seen.setdefault(token, []).append(path.name)
    commits, non_commits = [], []
    for token in sorted(seen):
        row = resolve(token, markers=markers, hist=hist)
        row["cited_by"] = len(seen[token])
        (commits if row["is_commit"] else non_commits).append(row)
    print(f"hash-shaped tokens in memory_graph/records: {len(seen)}")
    print(f"  commits in this repository: {len(commits)}")
    print(f"  NOT commits here:           {len(non_commits)}"
          "   (source sha1s, object/body digests, worker-branch commits --"
          " reported, never guessed)")
    by_run = {}
    for row in commits:
        by_run.setdefault(row["run_floor"], 0)
        by_run[row["run_floor"]] += 1
    print("\ncited commits per run (floor):")
    for run in sorted(by_run, key=lambda r: (r is None, r)):
        print(f"  run {str(run):<5} {by_run[run]} commit(s)")
    if limit:
        print(f"\nfirst {limit} resolved citations:")
        for row in commits[:limit]:
            print("  " + _format(row).replace("\n", "\n  "))
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("tokens", nargs="*", help="commit hashes to resolve")
    parser.add_argument("--scan-records", action="store_true",
                        help="resolve every commit citation in the corpus")
    parser.add_argument("--runs", action="store_true",
                        help="print the run-marker table")
    parser.add_argument("--limit", type=int, default=0,
                        help="with --scan-records, also list N citations")
    args = parser.parse_args(argv)

    if args.runs:
        markers = run_markers()
        numbers = sorted({row[0] for row in markers})
        print(f"{len(markers)} 'Stage run-N work claims' marker(s), "
              f"runs {numbers[0]}..{numbers[-1]}")
        gaps = [n for n in range(numbers[0], numbers[-1] + 1)
                if n not in numbers]
        print(f"runs with NO marker (why a run number is a floor): {gaps}")
        for number, sha, date, _index in markers:
            print(f"  run {number:<4} {date}  {sha[:9]}")
        return 0

    if args.scan_records:
        return scan_records(limit=args.limit or None)

    if not args.tokens:
        parser.print_help()
        return 2
    hist = history()
    markers = run_markers(hist)
    unresolved = 0
    for token in args.tokens:
        row = resolve(token, markers=markers, hist=hist)
        unresolved += not row["is_commit"]
        print(_format(row))
    return 1 if unresolved else 0


if __name__ == "__main__":
    sys.exit(main())
