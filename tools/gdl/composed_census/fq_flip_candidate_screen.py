#!/usr/bin/env python3
"""FQ lane: the IMAGE-WIDE FLIP-CANDIDATE SCREEN.

Promoted to the tool corpus by the run-50 TOOL lane from the algorithm
recorded verbatim in
claim.FQ_image-wide-flip-candidate-screen-at-c8a28c3bb-has-exactly-one-
candidate-and-it-is-walled.20260903.v1 and
attempt.FQ_atree-flip-is-blocked-by-a-linkage-vs-layout-contradiction-mwcc-
cannot-satisfy.20260903.v1 (its `verification` field).  It lived only in a
retired lane worktree and was the only tool of its kind: nothing else in
tools/gdl answers "which NonMatching TU is ready to flip".

    python tools/gdl/composed_census/fq_flip_candidate_screen.py
    python tools/gdl/composed_census/fq_flip_candidate_screen.py --json
    python tools/gdl/composed_census/fq_flip_candidate_screen.py \
        --out build/GUNE5D/fq_flip_screen.json

Run from the repository root after a completed `ninja` (it reads
build/GUNE5D/report.json, which the report build writes).

THE JOIN IS THE WHOLE TRAP.  report.json unit names carry a `main/` prefix
and NO file extension (`main/game/anim/atree`), while configure.py's
Object() rows carry the extension and no prefix (`game/anim/atree.c`), so a
naive join on the Object() path matches ZERO rows and reports a false
all-clear -- an empty candidate list that reads exactly like "nothing to
flip".  This tool strips the prefix, strips the extension, and REFUSES
(exit 2) when the join rate falls below --min-join (default 0.5), because a
broken join and a genuinely empty queue are the same output otherwise.

THREE QUESTIONS, not one.  A flip lane needs the queue behind the candidate
as much as the candidate:

  READY        matched_code == total_code AND
               matched_functions == total_functions, on a NonMatching unit
               -- the flip candidate proper.
  NEAR         matched_code_percent in [--near-lo, 100) -- the band a close
               lane's next landing moves into READY.
  FNS-DONE     every function matched but code bytes still short -- the
               shape where a data/pool claim, not a function, is the wall.

The screen decides READINESS, never FLIPPABILITY: the run-49 candidate
(atree.c, 36/36 fns, 11632/11632 bytes) passed this screen and then failed
three independent audit blockers.  Per AGENTS.md's dispatch screens, run it
as step 0 of a CLOSE lane on the TU that lane is about to finish, not as a
lane of its own.
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))

REPORT = os.path.join("build", "GUNE5D", "report.json")

# Object(Matching, "path") / Object(NonMatching, "path", cflags=...)
_OBJECT_RE = re.compile(
    r"""Object\(\s*(Matching|NonMatching|Equivalent)\s*,\s*["']([^"']+)["']""")


def object_states(configure_py=None):
    """{extension-stripped unit stem: state} from configure.py's Object rows."""
    path = configure_py or os.path.join(ROOT, "configure.py")
    with open(path, encoding="utf-8") as handle:
        text = handle.read()
    states = {}
    for state, unit in _OBJECT_RE.findall(text):
        states[os.path.splitext(unit)[0].replace("\\", "/")] = state
    return states


def report_units(report_path=None):
    """[(stem, measures)] for every non-auto-generated report unit."""
    path = report_path or os.path.join(ROOT, REPORT)
    with open(path, encoding="utf-8") as handle:
        report = json.load(handle)
    rows = []
    for unit in report.get("units", []):
        metadata = unit.get("metadata") or {}
        if metadata.get("auto_generated"):
            continue
        name = unit.get("name", "")
        if "/" in name:
            name = name.split("/", 1)[1]
        rows.append((os.path.splitext(name)[0], unit.get("measures") or {}))
    return rows


def _int(measures, key):
    value = measures.get(key)
    try:
        return int(value)
    except (TypeError, ValueError):
        return 0


def screen(report_path=None, configure_py=None, near_lo=98.5):
    states = object_states(configure_py)
    rows = report_units(report_path)
    census = {"Matching": 0, "NonMatching": 0, "Equivalent": 0, "unjoined": 0}
    ready, near, fns_done, joined = [], [], [], 0
    for stem, measures in rows:
        state = states.get(stem)
        if state is None:
            census["unjoined"] += 1
            continue
        joined += 1
        census[state] = census.get(state, 0) + 1
        if state == "Matching":
            continue
        matched_code = _int(measures, "matched_code")
        total_code = _int(measures, "total_code")
        matched_fns = measures.get("matched_functions") or 0
        total_fns = measures.get("total_functions") or 0
        code_pct = measures.get("matched_code_percent") or 0.0
        row = {"unit": stem, "state": state,
               "matched_code": matched_code, "total_code": total_code,
               "matched_functions": matched_fns, "total_functions": total_fns,
               "matched_code_percent": code_pct,
               "fuzzy": measures.get("fuzzy_match_percent"),
               "matched_data": _int(measures, "matched_data"),
               "total_data": _int(measures, "total_data")}
        code_done = total_code and matched_code == total_code
        fn_done = total_fns and matched_fns == total_fns
        if code_done and fn_done:
            ready.append(row)
        elif fn_done:
            fns_done.append(row)
        elif near_lo <= code_pct < 100.0:
            near.append(row)
    return {"census": census, "joined": joined, "rows": len(rows),
            "ready": ready, "near": near, "fns_done": fns_done,
            "near_lo": near_lo}


def _print_rows(title, rows):
    print(f"\n{title}: {len(rows)}")
    for row in sorted(rows, key=lambda r: -(r["matched_code_percent"] or 0)):
        print(f"  {row['unit']:<44} code {row['matched_code']}/"
              f"{row['total_code']} ({row['matched_code_percent']:.4f}%)"
              f"  fns {row['matched_functions']}/{row['total_functions']}"
              f"  data {row['matched_data']}/{row['total_data']}"
              f"  fuzzy {row['fuzzy']}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--report", default=None,
                        help=f"report.json path (default {REPORT})")
    parser.add_argument("--configure", default=None)
    parser.add_argument("--near-lo", type=float, default=98.5,
                        help="low edge of the NEAR band (default 98.5)")
    parser.add_argument("--min-join", type=float, default=0.5,
                        help="refuse below this joined/rows ratio (0.5)")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--out", default=None)
    arguments = parser.parse_args()

    result = screen(arguments.report, arguments.configure, arguments.near_lo)
    rate = result["joined"] / result["rows"] if result["rows"] else 0.0
    result["join_rate"] = rate
    if arguments.out:
        path = arguments.out
        if not os.path.isabs(path):
            path = os.path.join(ROOT, path)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(result, handle, indent=1)
        print(f"wrote {path}")
    if arguments.json:
        print(json.dumps(result, indent=1))
        return 0

    census = result["census"]
    print(f"FLIP-CANDIDATE SCREEN over {result['rows']} non-auto-generated "
          f"report unit(s)")
    print(f"  joined to configure.py Object(): {result['joined']}"
          f" ({rate:.1%}); unjoined {census['unjoined']}")
    print(f"  Matching {census['Matching']}"
          f"  NonMatching {census['NonMatching']}"
          f"  Equivalent {census.get('Equivalent', 0)}")
    if rate < arguments.min_join:
        print(f"\nREFUSED: join rate {rate:.1%} < --min-join "
              f"{arguments.min_join:.0%}. report.json unit names and "
              f"configure.py Object() paths are not joining, so an empty "
              f"candidate list here means NOTHING. Fix the join first.")
        return 2
    _print_rows("READY (matched_code == total_code AND all functions matched)",
                result["ready"])
    _print_rows(f"NEAR ({result['near_lo']}% <= matched code < 100%)",
                result["near"])
    _print_rows("FNS-DONE (all functions matched, code bytes short)",
                result["fns_done"])
    if not result["ready"]:
        print("\nNo flip candidate. A FLIP lane has nothing to harvest until "
              "a NonMatching TU's last open function closes.")
    else:
        print("\nREADINESS only -- every candidate still owes the full audit "
              "(fndiff --clean, textorder.py, claimcheck.py, datadiff.py) "
              "before finish_tu.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
