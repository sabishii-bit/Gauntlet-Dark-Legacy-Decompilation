#!/usr/bin/env python3
"""Recheck metrics quoted in an explicit JSON file against existing objects.

Usage: python tools/gdl/composed_census/t25_record_recheck.py <path-to.json>
       [--gate] [--unit U --function F]

Reads quoted commands to identify the function; does not guess ownership or
build objects. Reports HELD / MOVED / NOT-MEASURED per metric. --gate makes
a moved measurement exit 1. No repository-wide record or identifier lookup.
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

# Recognize both direct measurements and before -> after transitions.
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
    """Load an explicitly supplied JSON file; never resolve an identifier."""
    path = Path(reference)
    if not path.is_file():
        raise SystemExit(
            f"JSON file not found: {reference!r}; pass an explicit file path")
    return json.loads(path.read_text(encoding="utf-8")), path


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("record", help="explicit path to a JSON file containing quoted metrics")
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

    # Multiple functions make an unlabelled metric ambiguous.
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
              " to re-run. Pass --unit/--function to check it"
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
              " committing, or identify the source revision for each number.")
    else:
        print("  VERDICT: HELD — every quoted metric reproduces at this"
              " tree.")
    return 1 if (moved and args.gate) else 0


if __name__ == "__main__":
    raise SystemExit(main())
