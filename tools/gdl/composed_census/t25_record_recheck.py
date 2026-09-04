#!/usr/bin/env python3
"""Re-run the measurements a record QUOTES and say whether they still hold.

WHY THIS EXISTS (run-55 item 4, reported by CR against run 54): "I quoted a
record number one commit stale ... a multi-probe pass invalidates its own
earlier measurements, and nothing in the loop re-checks them at commit
time."  A pass takes a baseline, probes three forms, keeps two, commits
twice — and the numbers written into the record were read at different tree
states along the way.  Every gate in the loop describes HEAD; none of them
looks at what the RECORD says.

    python tools/gdl/composed_census/t25_record_recheck.py <record-id>
    python tools/gdl/composed_census/t25_record_recheck.py <path-to.json>
    python tools/gdl/composed_census/t25_record_recheck.py <id> --gate
    python tools/gdl/composed_census/t25_record_recheck.py <id> --unit U --function F

It reads the record (accepted or in the inbox), finds the metric assertions
it makes, re-runs the read-only tools that produce them AT THE CURRENT TREE,
and prints HELD / MOVED per metric.  No build is started: the objects must
already exist, which after `ninja` they do.

EXIT CODE follows wf_word_diff's rule — 0 whenever the re-check RAN, so a
MOVED verdict does not look like a crashed tool.  `--gate` is the opt-in
that makes MOVED exit 1, for use in a commit-time chain.  A record that
quotes no re-checkable metric exits 0 and says so; that is an answer.

SCOPE, measured at 030385209 over all 2,240 records in records/ + inbox/:
  * 1,568 quote at least one metric this screen understands
    (`DIFFERING WORDS = N`, `MNEMONIC DIVERGENCE = N`, `RELOC-SYMBOL
    MISMATCH = N`, `real N`, `insns TN/ON`);
  *   489 quote a `<tool> <unit> <function>` command;
  *   434 carry BOTH and are re-checkable by this tool today;
  * 1,134 quote a number with NO command that would reproduce it — the
    negative side, and the reason this tool REPORTS that state instead of
    treating it as a pass.  It is the same defect AGENTS.md names for work
    orders ("a present-tense number in an order needs a record id"), on the
    record side.

TWO-SIDED CALIBRATION, run in-process over the whole corpus at 030385209:
355 records were attributable and measurable, and **347 (97.7%) read HELD**
— the negative side, and the number that says this is not a screen that
flags everything. 8 read STALE, and every one is a real historical
staleness: fn_800D8BCC's residual is quoted as 122 and 95 differing words in
three records and measures 66 today, dbgtext::fn_800C03E0's 216 is 202,
MBWorldToScreen3D's 55/7 is 32/5. 18 more were SKIPPED as multi-function
(see the AMBIGUOUS branch) and 162 quote a unit/function that no longer
measures. Nothing here is retro-actively wrong — a record describes its own
moment — but a lane QUOTING one of those numbers today is quoting a number
that is gone, which is the run-54 report.

WHAT IT DELIBERATELY DOES NOT DO: guess. If the unit cannot be read out of a
quoted command it says so and stops rather than searching every object for
the function name — an inferred unit would make a HELD verdict meaningless.
Pass `--unit`/`--function` to re-check by hand.

IMPORTABLE CORE: parse_assertions, parse_commands, live_metrics, compare.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE
while not (ROOT / "config" / "GUNE5D").is_dir():
    if ROOT.parent == ROOT:
        raise SystemExit(f"repo root not found above {HERE}")
    ROOT = ROOT.parent

# Metric name -> the pattern that reads it out of prose OR tool output. The
# spellings are the tools' own, which is why a record that pastes tool
# output (AGENTS.md discipline 8) is re-checkable and a paraphrase is not.
#
# EACH METRIC TAKES A LIST OF SPELLINGS, and the second entry of `real` and
# `insns` is why. A record writes an improvement as a TRANSITION —
# `IMPROVED real 68 -> 66` — so a pattern reading only `real N` collects the
# BEFORE value and calls the record stale against its own kept result. Same
# for the counts: probe prints `insns T148/O148` and `fndiff --count` prints
# `insns 148/148`, so a screen that knows one spelling reports the other as
# NOT-MEASURED. Both were live false verdicts on
# attempt.CR_crittercollideitems-...20260904.v1 before these were added.
METRIC_PATTERNS = {
    "differing_words": [re.compile(r"DIFFERING WORDS = (\d+)")],
    "mnemonic_divergence": [re.compile(r"MNEMONIC DIVERGENCE = (\d+)")],
    "reloc_symbol_mismatch": [re.compile(r"RELOC-SYMBOL MISMATCH = (\d+)")],
    "real": [re.compile(r"\breal (\d+)\b"),
             re.compile(r"\breal \d+ -+> (\d+)\b")],
    "insns": [re.compile(r"insns T(\d+)/O(\d+)"),
              re.compile(r"insns (\d+)/(\d+)")],
}
# The tools a record quotes as `<tool>.py <unit> <function>`. Only the
# read-only ones are ever RUN; probe.py is recognised so the unit can be
# read out of it, never executed (it builds, and it banks state).
COMMAND_RE = re.compile(
    r"(?:python\s+)?(?:tools/gdl/(?:composed_census/)?)?"
    r"(wf_word_diff|fndiff|savedregs|regnorm|probe)\.py\s+"
    r"([A-Za-z0-9_./\\-]+/[A-Za-z0-9_.-]+)\s+([A-Za-z0-9_$.]+)")
RUNNABLE = {"wf_word_diff", "fndiff", "savedregs", "regnorm"}


def parse_assertions(text: str) -> dict[str, list]:
    """{metric: [values]} for every metric the text asserts."""
    found: dict[str, list] = {}
    for name, patterns in METRIC_PATTERNS.items():
        for pattern in patterns:
            for match in pattern.finditer(text):
                groups = [int(g) for g in match.groups() if g is not None]
                value = tuple(groups) if len(groups) > 1 else groups[0]
                found.setdefault(name, [])
                if value not in found[name]:
                    found[name].append(value)
    return found


def parse_commands(text: str) -> list[tuple[str, str, str]]:
    """[(tool, unit, function)] for every quoted per-function command."""
    seen = []
    for tool, unit, function in COMMAND_RE.findall(text):
        row = (tool, unit, function)
        if row not in seen:
            seen.append(row)
    return seen


def _run(args: list[str]) -> str:
    result = subprocess.run([sys.executable, *args], cwd=str(ROOT),
                            capture_output=True, text=True)
    return (result.stdout or "") + (result.stderr or "")


def live_metrics(unit: str, function: str) -> tuple[dict, list[str]]:
    """Re-measure the same metrics at the CURRENT tree. ({metric: value},
    [command lines run])."""
    commands = [
        ["tools/gdl/composed_census/wf_word_diff.py", unit, function],
        ["tools/gdl/fndiff.py", unit, function, "--count"],
    ]
    text = ""
    ran = []
    for command in commands:
        ran.append("python " + " ".join(command))
        text += _run(command) + "\n"
    live = {}
    for name, values in parse_assertions(text).items():
        live[name] = values[0]
    return live, ran


def compare(quoted: dict, live: dict) -> list[tuple[str, object, object, str]]:
    """[(metric, quoted, live, verdict)] — HELD / MOVED / NOT-MEASURED."""
    rows = []
    for name in METRIC_PATTERNS:
        if name not in quoted:
            continue
        if name not in live:
            rows.append((name, quoted[name], None, "NOT-MEASURED"))
            continue
        verdict = "HELD" if live[name] in quoted[name] else "MOVED"
        rows.append((name, quoted[name], live[name], verdict))
    return rows


def load_record(reference: str):
    """(record_dict, path) for a record id or a path."""
    path = Path(reference)
    if path.is_file():
        return json.loads(path.read_text(encoding="utf-8")), path
    for base in (ROOT / "memory_graph" / "records",
                 ROOT / "memory_graph" / "inbox"):
        for candidate in base.rglob("*.json"):
            try:
                record = json.loads(candidate.read_text(encoding="utf-8"))
            except Exception:
                continue
            if record.get("id") == reference:
                return record, candidate
    raise SystemExit(
        f"no record and no file named {reference!r} — pass a record id"
        " exactly as `gdlmem.py record` prints it, or a path to the draft"
        " JSON you are about to propose")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("record", help="record id, or path to a draft JSON")
    ap.add_argument("--unit", help="override the unit (when the record"
                                   " quotes no command)")
    ap.add_argument("--function", help="override the function")
    ap.add_argument("--gate", action="store_true",
                    help="exit 1 when a quoted number MOVED (default exits 0"
                         " whenever the re-check ran, so a stale record does"
                         " not read as a crashed tool)")
    args = ap.parse_args()

    record, path = load_record(args.record)
    text = json.dumps(record)
    quoted = parse_assertions(text)
    commands = parse_commands(text)
    print(f"RECORD {record.get('id') or path.name}")
    try:
        print(f"  source: {path.relative_to(ROOT)}")
    except ValueError:
        print(f"  source: {path}")
    if not quoted:
        print("  NO RE-CHECKABLE METRIC: this record quotes none of"
              f" {', '.join(METRIC_PATTERNS)}. That is an answer, not a"
              " pass — there is nothing here a later lane could falsify"
              " without repeating the whole pass.")
        return 0
    print("  QUOTED: " + "; ".join(
        f"{name}={values}" for name, values in quoted.items()))

    # A RECORD THAT SPANS SEVERAL FUNCTIONS CANNOT BE ATTRIBUTED (found by
    # reading the calibration's positives, not by counting them). A sweep
    # record quotes one function's number and another's command, and every
    # metric would be charged to whichever command came first:
    # claim.CX_expiry-check-sweep-six-denials-are-unrunnable-not-one-and-two-
    # premises-are-expired.20260903.v1 quotes [0, 66, 95, 122] differing
    # words across four functions and read STALE against do_enemies' 172.
    functions = {row[2] for row in commands}
    if len(functions) > 1 and not args.function:
        print(f"  AMBIGUOUS: this record quotes commands for"
              f" {len(functions)} functions ({', '.join(sorted(functions))}),"
              " so a quoted number cannot be attributed to one of them."
              " Re-run with --function <name> to check one.")
        return 0

    unit, function = args.unit, args.function
    if not (unit and function):
        for tool, cmd_unit, cmd_function in commands:
            if args.function and cmd_function != args.function:
                continue
            unit, function = unit or cmd_unit, function or cmd_function
            print(f"  unit/function read from the record's own quoted"
                  f" `{tool}.py {cmd_unit} {cmd_function}`")
            break
    if not (unit and function):
        print("  UNANCHORED: the record quotes numbers but no"
              " `<tool>.py <unit> <function>` command, so there is nothing"
              " to re-run. 1,134 of 2,240 records are in this state"
              " (measured at 030385209). Pass --unit/--function to check it"
              " by hand, and quote the command in the record next time.")
        return 0

    live, ran = live_metrics(unit, function)
    for line in ran:
        print(f"  RE-RAN: {line}")
    rows = compare(quoted, live)
    moved = [row for row in rows if row[3] == "MOVED"]
    for name, was, now, verdict in rows:
        print(f"    {verdict:<12} {name}: record {was} -> live {now}")
    if moved:
        print(f"  VERDICT: STALE — {len(moved)} quoted metric(s) no longer"
              " reproduce at this tree. Re-measure and rewrite them before"
              " committing (AGENTS.md discipline 8: write records FROM tool"
              " output), or say which commit each number belongs to, the way"
              " attempt.CR_crittercollideitems-the-pointer-local-closes-both-"
              "load-order-sites-and-the-residual-becomes-a-pure-recolor"
              ".20260904.v1 anchors each of its numbers to 47ae4d37c /"
              " 0a577abd3 / dc2c70e2f.")
    else:
        print("  VERDICT: HELD — every quoted metric reproduces at this"
              " tree.")
    return 1 if (moved and args.gate) else 0


if __name__ == "__main__":
    raise SystemExit(main())
