#!/usr/bin/env python3
"""Mechanical per-function gate for de-fakematch and matching passes.

Snapshots every function's fndiff state for a TU, then verifies later edits
against it: a matched function must stay byte-identical and a fuzzy function
must measure equal-or-better on real diff lines, or the check fails. This
turns the AGENTS.md gate rule into tooling instead of discipline — always
scored from the real ninja-built object via fndiff (matchtool presets can
diverge from a TU's actual cflags).

Usage:
  python tools/gdl/defake_gate.py baseline game/enemy/enemy.c
  python tools/gdl/defake_gate.py check game/enemy/enemy.c --rebuild
  python tools/gdl/defake_gate.py check game/enemy/enemy.c --update-improved

--rebuild runs the unit's ninja object target first, so rebuild+gate is one
call and a stale object can never be gated. On any REGRESSION the check
automatically prints each regressing function's fndiff --ops summary so the
diagnosis doesn't need a separate command.

`baseline` writes build/GUNE5D/gate/<unit>.json. `check` exits 1 on any
regression and lists it; improvements are reported (and, with
--update-improved, become the new baseline so later edits are gated against
the better score).
"""

import json
import re
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
FNDIFF = Path(__file__).resolve().parent / "fndiff.py"

COUNT_RE = re.compile(
    r"^DIFF\s+(\S+)\s+insns\s+(\d+)/(\d+)\s+lines\s+(\d+)\s+real\s+(\d+)\s*$"
)
CLASSIFY_RE = re.compile(
    r"^(EXACT|RELOCATION_ONLY|REGISTER_ONLY|SCHEDULE_CANDIDATE|OPERAND_DIFF"
    r"|STRUCTURAL|BASE_ONLY|TARGET_ONLY)\s+(\S+)(?:\s+insns\s+(\d+)/(\d+))?\s*$"
)


def parse_classify(text):
    """Roster of every function with its fndiff --classify category."""
    roster = {}
    for line in text.splitlines():
        match = CLASSIFY_RE.match(line.strip())
        if match:
            category, name, ti, bi = match.groups()
            roster[name] = {
                "status": category,
                "ti": int(ti) if ti else None,
                "bi": int(bi) if bi else None,
            }
    return roster


def parse_count(text):
    """Per-function real-diff counts from fndiff --count (mismatchers only)."""
    counts = {}
    for line in text.splitlines():
        match = COUNT_RE.match(line.strip())
        if match:
            name, ti, bi, lines, real = match.groups()
            counts[name] = {
                "ti": int(ti), "bi": int(bi),
                "lines": int(lines), "real": int(real),
            }
    return counts


def snapshot(classify_text, count_text):
    """Merge the two fndiff views into {fn: {status, ti, bi, real}}."""
    roster = parse_classify(classify_text)
    counts = parse_count(count_text)
    merged = {}
    for name, entry in roster.items():
        row = dict(entry)
        if entry["status"] in ("BASE_ONLY", "TARGET_ONLY"):
            row["real"] = None
        elif name in counts:
            row.update(counts[name])
        else:
            row["real"] = 0  # EXACT (or noise-only): no DIFF line emitted
        merged[name] = row
    return merged


def compare(baseline, current):
    """Verdicts per function; regression = matched fell or real grew."""
    verdicts = []
    for name, base in sorted(baseline.items()):
        cur = current.get(name)
        if cur is None:
            verdicts.append((name, "REGRESSION", "function vanished from object"))
            continue
        # A byte-exact function must STAY byte-exact: real normalizes
        # relocation payloads, so EXACT -> RELOCATION_ONLY demotions kept
        # real at 0 and slipped through this gate (4 shipped before the
        # census caught them — claim.law.never-literalize-inside-a-real-
        # zero-function). Status is checked unconditionally now.
        if base["status"] == "EXACT" and cur["status"] != "EXACT":
            verdicts.append(
                (name, "REGRESSION",
                 f"status EXACT -> {cur['status']} (byte-exact demoted;"
                 " real can stay 0 for relocation-payload changes)")
            )
            continue
        base_real, cur_real = base.get("real"), cur.get("real")
        if base_real is None or cur_real is None:
            if base["status"] != cur["status"]:
                verdicts.append(
                    (name, "REGRESSION",
                     f"status {base['status']} -> {cur['status']}")
                )
            continue
        if base_real == 0 and cur_real > 0:
            verdicts.append(
                (name, "REGRESSION", f"was byte-identical, now real {cur_real}")
            )
        elif cur_real > base_real:
            verdicts.append(
                (name, "REGRESSION", f"real {base_real} -> {cur_real}")
            )
        elif cur_real < base_real:
            verdicts.append(
                (name, "IMPROVED", f"real {base_real} -> {cur_real}")
            )
    for name in sorted(set(current) - set(baseline)):
        verdicts.append((name, "NEW", "function absent from baseline"))
    return verdicts


def run_fndiff(unit, flag):
    result = subprocess.run(
        [sys.executable, str(FNDIFF), unit, flag],
        capture_output=True, text=True,
    )
    if result.returncode != 0 and "missing:" in (result.stdout + result.stderr):
        raise SystemExit(f"fndiff failed for {unit}:\n{result.stdout}{result.stderr}")
    return result.stdout


def gate_path(unit):
    slug = re.sub(r"[^A-Za-z0-9_.-]+", "_", re.sub(r"\.(c|cpp)$", "", unit))
    return Path(f"build/{VERSION}/gate/{slug}.json")


def normalize_unit(unit):
    """Accept src/-prefixed, backslashed, or extensioned unit spellings."""
    unit = unit.replace("\\", "/").strip("/")
    if unit.startswith("src/"):
        unit = unit[len("src/"):]
    return unit


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    update_improved = "--update-improved" in sys.argv
    rebuild = "--rebuild" in sys.argv or "--build" in sys.argv
    if len(args) != 2 or args[0] not in ("baseline", "check"):
        print(__doc__)
        return 2
    mode, unit = args
    unit = normalize_unit(unit)
    if rebuild:
        obj = re.sub(r"\.(c|cpp)$", "", unit)
        build = subprocess.run(
            ["ninja", f"build/{VERSION}/src/{obj}.o"],
            capture_output=True, text=True,
        )
        if build.returncode != 0:
            print("BUILD FAILED (gate not run):")
            print((build.stdout + build.stderr).strip()[-1500:])
            return 1
    snap = snapshot(run_fndiff(unit, "--classify"), run_fndiff(unit, "--count"))
    path = gate_path(unit)
    if mode == "baseline":
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(snap, indent=2, sort_keys=True), encoding="utf-8")
        exact = sum(1 for row in snap.values() if row.get("real") == 0)
        print(f"baseline: {len(snap)} functions ({exact} at real 0) -> {path}")
        return 0
    if not path.exists():
        print(f"no baseline at {path}; run `defake_gate.py baseline {unit}` first")
        return 2
    baseline = json.loads(path.read_text(encoding="utf-8"))
    verdicts = compare(baseline, snap)
    regressions = [v for v in verdicts if v[1] == "REGRESSION"]
    for name, verdict, detail in verdicts:
        print(f"{verdict:10} {name}  {detail}")
    if regressions:
        print(f"GATE FAILED: {len(regressions)} regression(s) — revert or fix"
              " before committing")
        bare_unit = re.sub(r"\.(c|cpp)$", "", unit)
        for name, _verdict, detail in regressions[:4]:
            if "vanished" in detail:
                continue
            print(f"---- fndiff --ops {name} ----")
            ops = subprocess.run(
                [sys.executable, str(FNDIFF), bare_unit, name,
                 "--ops", "--no-build"],
                capture_output=True, text=True,
            ).stdout
            print("\n".join(ops.strip().splitlines()[:14]))
        return 1
    improved = [v for v in verdicts if v[1] == "IMPROVED"]
    if improved and update_improved:
        # Archive the outgoing baseline so the session-start census stays
        # reconstructable (a worker had to rebuild it from transcripts).
        archive = path.with_suffix(".prev.json")
        archive.write_text(path.read_text(encoding="utf-8"),
                           encoding="utf-8")
        path.write_text(json.dumps(snap, indent=2, sort_keys=True), encoding="utf-8")
        print(f"baseline updated with {len(improved)} improvement(s)"
              f" (previous archived at {archive.name})")
    print("GATE OK" + (f" ({len(improved)} improved)" if improved else ""))
    if improved and not update_improved:
        print("(flags combine: re-run with --rebuild --update-improved to"
              " bank these in ONE call — no separate second check needed)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
