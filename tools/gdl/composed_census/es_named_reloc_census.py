#!/usr/bin/env python3
"""Image-wide census of SCORE-INVISIBLE pool/label relocation defects.

At each relocation position (target vs ours, same index, same reloc type)
report the cases where BOTH sides name a splitter `lbl_*` symbol and the
names DIFFER.  Nothing else in the toolchain sees this row:

  * `fndiff --count` / probe.py `real` DROP every reloc line;
  * `fndiff --clean` NORMALIZES `lbl_*` and `@N` alike to `<local>`;
  * `reloc_set_delta` keys both spellings to `<local>` too, so a
    TRANSPOSITION cancels perfectly in the multiset;
  * objdiff fuzzy scored damage_enemy at 99.22% while it loaded 0.0f
    where retail loads 1.0f.

EVERY ROW IS POSITIONAL, so no row is a bug until something ALIGNMENT-FREE
agrees. Run-47 item 4 (the third costume of the run-42 finding that a
positional census ships "a list of candidates advertised as a list of bugs"):
the disposition is now decided by the WHOLE-FUNCTION DATUM MULTISET
(`fndiff.datum_multiset_screen`), which is alignment-free by construction --
it compares the multiset of datum VALUES the two functions read, so it cannot
be fooled by a pairing this census had to guess at.

  WRONG-DATUM   the multiset says VALUE-DELTA: our function reads a value
                retail's does not. The positional row is corroborated by a
                measurement that never looked at position. A real defect.
  TRANSPOSED    the multiset says VALUE-EQUAL and the row set's labels are a
                PERMUTATION of each other: every value is right, the ORDER is
                not -> a source operand/statement order defect (fix = swap the
                operands). The multiset is blind to a transposition BY
                CONSTRUCTION (fndiff documents this), so these two facts are
                independent, not circular.
  TRANSPOSED?   a permutation AND a VALUE-DELTA: the swap is real but the
                function reads a wrong value SOMEWHERE ELSE too. Read the
                multiset delta before touching the operand order.
  CANDIDATE     the multiset says VALUE-EQUAL and the labels are not a
                permutation: our function reads exactly the values retail
                reads, and the mismatch exists only in this census's own
                positional pairing. NOT a bug on this evidence.

Calibrated at 998144326 over the census's own 28 functions: 7 WRONG-DATUM,
8 TRANSPOSED (2 of which the old 2-cycle-only test called WRONG -- a 4-cycle
rotation is a transposition too), 2 TRANSPOSED?, and 11 CANDIDATE. That last
number is the finding: 11 of the 20 rows previously shipped as "WRONG"
(55%) read every datum value retail reads.

Run from the repository root.
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))
import difflib  # noqa: E402

from fndiff import datum_multiset_screen, erase_registers, parse  # noqa: E402


def disposition(unit, fn, rows):
    """(verdict, datum_verdict, is_permutation) for one function's rows.

    The positional evidence (`rows`) decides NOTHING on its own; the
    whole-function datum multiset does. `is_permutation` compares the two
    LABEL MULTISETS of the row set, not 2-cycles: the old test asked whether
    every (a, b) had a matching (b, a), which is true only of a swap, so
    set_hidden_player's 4-label rotation and InitNameAudio's were both filed
    as WRONG-CONSTANT candidates.
    """
    permutation = (sorted(r["target"] for r in rows)
                   == sorted(r["ours"] for r in rows))
    screen = datum_multiset_screen(unit, fn)
    datum_verdict = screen["verdict"] if screen else "NO-SCREEN"
    if datum_verdict == "VALUE-DELTA":
        return ("TRANSPOSED?" if permutation else "WRONG-DATUM",
                datum_verdict, permutation)
    if datum_verdict == "VALUE-EQUAL":
        return ("TRANSPOSED" if permutation else "CANDIDATE",
                datum_verdict, permutation)
    # No screen (an object or function the multiset reader could not pair):
    # UNSCREENED is not clean and it is not a bug either.
    return "UNSCREENED", datum_verdict, permutation


def instr_relocs(lines):
    """[(normalized_instruction_text, [(reloc_type, symbol), ...]), ...]."""
    out = []
    for line in lines:
        if line.startswith("    "):
            if out:
                parts = line.strip().split(maxsplit=1)
                out[-1][1].append((parts[0], parts[1] if len(parts) > 1 else ""))
        else:
            out.append((erase_registers(line.strip()), []))
    return out


def aligned_reloc_pairs(t_lines, b_lines):
    """Reloc pairs that provably address the same instruction.

    Equal instruction COUNT is NOT alignment: MBOX_AllocModelMem is 102/102
    with a 2-instruction schedule drift and fabricated a wrong-symbol row
    that way. Align the two instruction streams with difflib and pair
    relocations only inside the matcher's EQUAL runs — the same discipline
    fndiff --ops applies to its IMMEDIATE rows.
    """
    t, b = instr_relocs(t_lines), instr_relocs(b_lines)
    sm = difflib.SequenceMatcher(None, [x[0] for x in t], [x[0] for x in b],
                                 autojunk=False)
    pairs = []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag != "equal":
            continue
        for k in range(i2 - i1):
            trel, brel = t[i1 + k][1], b[j1 + k][1]
            if len(trel) != len(brel):
                continue
            for (tt, ts), (bt, bs) in zip(trel, brel):
                if tt == bt:
                    pairs.append((i1 + k, tt, ts.strip(), bs.strip()))
    return pairs


VERSION = "GUNE5D"
LBL_RE = re.compile(r"^lbl_[0-9A-Fa-f]{6,8}$")


def poolvals(labels):
    if not labels:
        return {}
    r = subprocess.run([sys.executable, str(REPO / "tools" / "gdl" / "poolval.py"),
                        "--json", *sorted(labels)],
                       cwd=str(REPO), capture_output=True, text=True)
    try:
        data = json.loads(r.stdout)
    except json.JSONDecodeError:
        return {}
    return {row["name"]: row for row in data.get("labels", [])}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(REPO / "build" / VERSION / "es_named_census.json"))
    args = ap.parse_args()

    report = json.loads((REPO / "build" / VERSION / "report.json").read_text())
    findings = []
    labels_seen = set()
    for u in report.get("units", []):
        unit = u.get("name", "").removeprefix("main/")
        if u.get("metadata", {}).get("complete"):
            continue
        tobj = REPO / "build" / VERSION / "obj" / f"{unit}.o"
        bobj = REPO / "build" / VERSION / "src" / f"{unit}.o"
        if not (tobj.exists() and bobj.exists()):
            continue
        tfns, bfns = parse(tobj), parse(bobj)
        for fn, tlines in tfns.items():
            blines = bfns.get(fn)
            if blines is None:
                continue
            rows = []
            for i, tt, ts, bs in aligned_reloc_pairs(tlines, blines):
                if ts == bs:
                    continue
                if LBL_RE.match(ts) and LBL_RE.match(bs):
                    rows.append({"idx": i, "type": tt, "target": ts, "ours": bs})
            if not rows:
                continue
            for r in rows:
                labels_seen.add(r["target"])
                labels_seen.add(r["ours"])
            verdict, datum_verdict, permutation = disposition(unit, fn, rows)
            findings.append({"unit": unit, "function": fn, "rows": rows,
                             "disposition": verdict,
                             "datum_multiset": datum_verdict,
                             "row_labels_are_a_permutation": permutation})

    vals = poolvals(labels_seen)
    for f in findings:
        for r in f["rows"]:
            for side in ("target", "ours"):
                v = vals.get(r[side], {})
                r[side + "_val"] = v.get("f64", v.get("f32"))
                r[side + "_sec"] = v.get("section")

    order = {"WRONG-DATUM": 0, "TRANSPOSED": 1, "TRANSPOSED?": 2,
             "UNSCREENED": 3, "CANDIDATE": 4}
    findings.sort(key=lambda f: (order.get(f["disposition"], 9), f["unit"],
                                 f["function"]))
    Path(args.out).write_text(json.dumps(findings, indent=1))
    tally = {}
    for f in findings:
        tally[f["disposition"]] = tally.get(f["disposition"], 0) + 1
    print(f"{len(findings)} function(s) with a named-vs-named pool relocation "
          f"mismatch (positional, counts held)")
    print("  disposition: " + ", ".join(
        f"{name} {tally[name]}" for name in sorted(tally, key=lambda n:
                                                   order.get(n, 9))))
    print("  EVERY row below is positional. The disposition is decided by the"
          " ALIGNMENT-FREE whole-function datum multiset; a CANDIDATE reads"
          " every value retail reads and is not a bug on this evidence.")
    for f in findings:
        print(f"\n{f['disposition']:<12} {f['unit']}::{f['function']}  "
              f"({len(f['rows'])} row(s), datum multiset"
              f" {f['datum_multiset']},"
              f" labels {'ARE' if f['row_labels_are_a_permutation'] else 'are NOT'}"
              " a permutation)")
        for r in f["rows"]:
            print(f"    [{r['idx']:>3}] {r['type']:<18} "
                  f"T {r['target']} = {r.get('target_val')!r} ({r.get('target_sec')})"
                  f"   O {r['ours']} = {r.get('ours_val')!r} ({r.get('ours_sec')})")
    print(f"\nwrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
