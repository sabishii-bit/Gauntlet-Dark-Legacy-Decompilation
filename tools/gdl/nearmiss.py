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

IMPORTABLE CORE: residual_columns, format_residual, format_row,
summary_line, load_graph_facts, load_parked, pool_offset_rows,
pool_offset_lines — pure over parsed line lists (load_graph_facts opens the
graph database), no build and no printing at import (run-43 item 10; the
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


def load_graph_facts():
    """(parked names, {name: count}, {name: evidence tier}).

    Attempt history is immutable, so a re-triage or successful revisit records
    a new attempt that supersedes the old cap.  Only unsuperseded heads may
    suppress queue entries.

    The COUNT is the second half (run-43 item 3).  AGENTS.md's close-lane
    screen says to rank candidates by records-per-unmatched-function, and the
    run-42 close lane had to reconstruct that by hand, one `gdlmem context`
    per candidate: a zero-record row is genuinely unexplored, while a
    five-record row is where five lanes already spent their probes.

    THE TIER IS THE THIRD (run-45 item 9), because the count alone ranked a
    ZERO-PROBE park as the best-explored function in the image.  A record
    proves work was done only if it says WHAT was probed, so each function
    gets the strongest evidence any of its records carries:

      ``D``  a typed denial (`denial`: probed_form / falsifier /
             premise_measurement) -- the axis is machine-screenable;
      ``P``  a literal `probed_form` but no typed denial;
      ``-``  records exist and NONE of them says what was probed: prose.

    Measured over the live corpus at 56067bfae -- 466 functions carry attempt
    records, of which 52 reach D, 140 reach P, and 274 (59%) are prose only.
    Nine functions hold FIVE prose-only records each (AudioSetupBossStreams,
    GetAnimAngXYZVal, InitEffects, PlayerMotion, WPitchMat3,
    __dt__15MoviePlayerBaseFv, msgPost, pbWinSetup, sysResetService): on the
    count alone those are the most-explored rows in the queue, and not one of
    them records a probed form.
    """
    sys.path.insert(0, str(REPO))
    try:
        from memory_graph.core import ensure_database, open_database

        ensure_database(REPO)
        connection = open_database(REPO)
    except Exception as error:  # graph unavailable: honest empty cap set
        print(f"nearmiss: memory graph unavailable ({error}); no parked caps"
              " and no record counts", file=sys.stderr)
        return set(), {}, {}
    try:
        parked = {row[0] for row in connection.execute(
            "SELECT e.name FROM attempt a"
            " JOIN entity e ON e.id = a.function_entity_id"
            " WHERE a.outcome IN ('parked', 'capped')"
            " AND NOT EXISTS (SELECT 1 FROM record_ingest newer"
            " WHERE json_extract(newer.raw_json, '$.supersedes') = a.record_id"
            " AND newer.record_state = 'accepted')"
        ).fetchall()}
        rows = connection.execute(
            "SELECT e.name, COUNT(*),"
            " SUM(CASE WHEN json_extract(r.raw_json,'$.denial') IS NOT NULL"
            "          THEN 1 ELSE 0 END),"
            " SUM(CASE WHEN json_extract(r.raw_json,"
            "                            '$.attributes.probed_form')"
            "            IS NOT NULL"
            "        OR json_extract(r.raw_json,'$.probed_form') IS NOT NULL"
            "          THEN 1 ELSE 0 END)"
            " FROM attempt a"
            " JOIN entity e ON e.id = a.function_entity_id"
            " JOIN record_ingest r ON r.record_id = a.record_id"
            " GROUP BY e.name"
        ).fetchall()
        counts = {name: total for name, total, _d, _p in rows}
        tiers = {name: evidence_tier(denials, probed)
                 for name, _total, denials, probed in rows}
        return parked, counts, tiers
    finally:
        connection.close()


def evidence_tier(denials, probed):
    """The strongest evidence a function's attempt records carry."""
    if denials:
        return "D"
    if probed:
        return "P"
    return "-"


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


def format_row(pct, size, residual, records, name, unit, tag, tier=""):
    """One queue row. `rec=N` is the record COUNT and the letter after it is
    the strongest EVIDENCE those records carry (see `load_graph_facts`): a
    row reading `rec=5-` holds five records and not one probed form."""
    stamp = f"{records}{tier if records else ''}"
    return (f"{pct:6.2f}%  {size:5d}B{residual}  rec={stamp:<3}"
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
            f" | rec=N is that function's attempt-record count, and the"
            f" letter after it is the strongest evidence they carry:"
            f" D typed denial, P probed_form, - prose only (rec=5- is five"
            f" records that never say what was probed) ---")


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

    parked, record_counts, record_tiers = load_graph_facts()
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
    shown = hidden = 0
    for pct, size, name, unit, real, category, clean, pool in rows:
        tag = ""
        if name in parked:
            if args.parked == "skip":
                hidden += 1
                continue
            tag = "  [PARKED]"
        residual = format_residual(real, clean, category, args.residuals, pool)
        print(format_row(pct, size, residual, record_counts.get(name, 0),
                         name, unit, tag, record_tiers.get(name, "")))
        shown += 1
    print(summary_line(shown, hidden, len(parked), args.min))
    return 0


if __name__ == "__main__":
    sys.exit(main())
