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

This queue reports measured code scores only. It makes no claims about prior
attempts, parked work, or ownership; coordinate scope before editing.

IMPORTABLE CORE: residual_columns, format_residual, format_row,
summary_line, pool_offset_rows,
pool_offset_lines — pure over parsed line lists,
no build and no printing at import (run-43 item 10; the
convention is documented in AGENTS.md).

`pool=N` IS THE PART OF `real` THAT IS NOT CODEGEN (run-50 item 5), and the
queue RANKS on `real - pool`. A same-opcode row whose immediate differs by a
constant that RECURS, and whose BASE REGISTER the two streams relocate
against DIFFERENT symbols, is a DATA-POSITION artifact: the same data in the
same order at a different base, unreachable from inside the function.
Measured on game/game/player::write_health_and_items
(attempt.NC_write-health-and-items-real-is-dominated-by-an-840-byte-rodata-
pool-offset.20260903.v1): three `addi r7,r29,N` rows, every one off by
EXACTLY 840, base `lbl_80113AE0` in the target against `@125` in ours.

TWO-SIDED, over the 195 functions in the >=90% band at run-50 HEAD:
  RECURRENCE ALONE fires on 79 (40%) and its heaviest rows are `-8` x49 and
  `-4` x33 -- struct-field and frame displacements, not data position. That
  draft was DISCARDED.
  THE SHIPPED RULE (recurrence + a differing relocated base) fires on 6 and
  is silent on 189, and all six were already named in the corpus:
  write_health_and_items, do_players, setup_player_display,
  create_player_blits (one player.c .rodata gap),
  combat::screen_limitation and audio::AudioStreamPlay (the section-alias
  base class of attempt.CV_critternewinst-error-string-carried-a-newline-
  retail-does-not.20260903.v1 and attempt.CB_screen-limitation-string-size-
  audit-and-reloc-arbiter-bounds.20260901.v1).

AND THE HEADLINE IT REFUTES: `real` is NOT dominated by these rows. They are
1.2%-16.3% of it on the six that carry any (write_health_and_items 6 of 68 =
8.8%). Ranking on the remainder moves 26 of 195 positions, but only TWO rows
move for a reason of their own -- AudioStreamPlay 54 -> 48 and
write_health_and_items 74 -> 63 -- and the other 24 are neighbours they
displace. The column earns its place by naming rows a lane CANNOT close from
inside the function, not by re-ordering the queue.

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
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

from fndiff import (classify_function, count_real, immediate_deltas,
                    immediates, normalized_reloc_lines, parse)

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parent.parent.parent
REPORT = REPO / "build" / VERSION / "report.json"


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


_BASE_RE = re.compile(r"^\s*\S+\s+r\d+,(?:r(\d+),|(\d+)\(r(\d+)\))")
_DEST_RE = re.compile(r"^\s*\S+\s+r(\d+),")


def _indexed(lines):
    """(instruction lines, {instruction index: its relocation symbol})."""
    instructions, relocs, index = [], {}, -1
    for line in lines:
        if not line:
            continue
        if line.startswith("    "):
            parts = line.strip().split(maxsplit=1)
            if len(parts) > 1 and index >= 0:
                relocs.setdefault(index, parts[1].strip())
        else:
            instructions.append(line)
            index += 1
    return instructions, relocs


def _base_register(line):
    match = _BASE_RE.match(line)
    return (match.group(1) or match.group(3)) if match else None


def _base_symbol(instructions, relocs, index, register):
    """Relocation symbol of the nearest EARLIER writer of `register`."""
    for j in range(index - 1, -1, -1):
        match = _DEST_RE.match(instructions[j])
        if match and match.group(1) == register:
            return relocs.get(j)
    return None


def _single_delta(target_line, base_line):
    """The one differing immediate's delta, or None if not exactly one."""
    t_imms, b_imms = immediates(target_line), immediates(base_line)
    if len(t_imms) != len(b_imms):
        return None
    deltas = []
    for a, b in zip(t_imms, b_imms):
        try:
            av, bv = int(a, 0), int(b, 0)
        except ValueError:
            return None
        if av != bv:
            deltas.append(av - bv)
    return deltas[0] if len(deltas) == 1 else None


def pool_offset_rows(target, base, min_recurrence=2):
    """[(t_index, b_index, delta, target_base, ours_base)] — DATA-POSITION
    rows: same opcode, immediate off by a RECURRING constant, and a base
    register the two streams relocate against DIFFERENT symbols.

    Both conditions are load-bearing; see the module docstring for the
    two-sided census (recurrence alone: 79 of 195; with the base condition:
    6 of 195, all six independently named in the corpus).
    """
    t_ins, t_relocs = _indexed(target)
    b_ins, b_relocs = _indexed(base)
    candidates = []
    for ti, bi, kind, t_line, b_line in immediate_deltas(target, base):
        if kind != "immediate":
            continue
        delta = _single_delta(t_line, b_line)
        if not delta:
            continue
        t_reg, b_reg = _base_register(t_line), _base_register(b_line)
        if t_reg is None or b_reg is None:
            continue
        t_sym = _base_symbol(t_ins, t_relocs, ti, t_reg)
        b_sym = _base_symbol(b_ins, b_relocs, bi, b_reg)
        if t_sym is None or b_sym is None or t_sym == b_sym:
            continue
        candidates.append((ti, bi, delta, t_sym, b_sym))
    counts = Counter(row[2] for row in candidates)
    return [row for row in candidates if counts[row[2]] >= min_recurrence]


def pool_offset_lines(target, base):
    """`real` LINES the data-position rows account for.

    A same-opcode/different-immediate pair is one `-` and one `+` line in
    the unified diff `real` counts, so the conversion is x2. Stated as a
    function rather than inlined because the units are exactly what a
    lane must not guess at.
    """
    return 2 * len(pool_offset_rows(target, base))


def format_residual(real, clean, category, residuals, pool=0):
    """The residual columns of one queue row."""
    if real is None:
        return "  real=???" if residuals else ""
    text = f"  real={real:4d}"
    text += f" clean={clean:<4d}" if clean != real else " " * 11
    text += f" pool={pool:<4d}" if pool else " " * 10
    return text + f" {category:<18}"


def format_row(pct, size, residual, name, unit):
    """One measured queue row, without inferred work-history metadata."""
    return f"{pct:6.2f}%  {size:5d}B{residual}  {name:<40} {unit}"


def summary_line(shown, minimum):
    """All matching rows in the selected score/TU band are shown."""
    return (f"--- {shown} near-miss fns (>= {minimum}%, < 100%)"
            f" | {shown} in band | prior attempts and ownership not assessed ---")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--min", type=float, default=90.0, metavar="PCT",
                    help="lower fuzzy bound (default 90)")
    ap.add_argument("--refresh", action="store_true",
                    help="regenerate report.json (ninja) before reading")
    ap.add_argument("--grep", metavar="STR", help="only TUs whose name contains STR")
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
                pool = 0
                if target_fns is not None:
                    target = target_fns.get(name)
                    base = base_fns.get(name)
                    if target is not None and base is not None:
                        real, clean, category = residual_columns(target, base)
                        pool = pool_offset_lines(target, base)
                rows.append((pct, size, name, unit, real, category, clean,
                             pool))

    if args.residuals:
        # RANK ON THE CODEGEN REMAINDER (run-50 item 5): `real` minus the
        # data-position lines no edit inside the function can reach.
        rows.sort(key=lambda r: (r[4] is None, (r[4] or 0) - r[7],
                                 -r[1], -r[0]))
    else:
        rows.sort(key=lambda r: (-r[0], r[1]))
    if args.residuals:
        print("legend: real=N is `fndiff --count`'s real — raw diff rows with"
              " every relocation line dropped — which is the number probe.py"
              " prints and the one every work order quotes. clean=N appears"
              " only when `fndiff --clean`'s differently-computed real"
              " disagrees, so a record quoting either can be matched to this"
              " row. pool=N is the part of real that is DATA POSITION, not"
              " codegen — same-opcode rows whose immediate is off by a"
              " RECURRING constant over a base the two streams relocate"
              " against different symbols — and the queue is ranked on"
              " real-pool, the codegen remainder. Those rows cannot be closed"
              " from inside the function.")
    shown = 0
    for pct, size, name, unit, real, category, clean, pool in rows:
        residual = format_residual(real, clean, category, args.residuals, pool)
        print(format_row(pct, size, residual, name, unit))
        shown += 1
    print(summary_line(shown, args.min))
    return 0


if __name__ == "__main__":
    sys.exit(main())
