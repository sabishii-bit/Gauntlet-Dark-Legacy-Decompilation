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

Two dispositions:
  TRANSPOSED  the same two labels appear on both sides, swapped -> a source
              operand/statement ORDER defect (fix = swap the operands).
  WRONG       a label ours reads is not the one target reads at that slot and
              is not explained by a swap -> candidate WRONG-CONSTANT BUG.

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

from fndiff import erase_registers, parse  # noqa: E402


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
            pairs = {(r["target"], r["ours"]) for r in rows}
            transposed = all((b, a) in pairs for a, b in pairs)
            for r in rows:
                labels_seen.add(r["target"])
                labels_seen.add(r["ours"])
            findings.append({"unit": unit, "function": fn, "rows": rows,
                             "disposition": "TRANSPOSED" if transposed else "WRONG"})

    vals = poolvals(labels_seen)
    for f in findings:
        for r in f["rows"]:
            for side in ("target", "ours"):
                v = vals.get(r[side], {})
                r[side + "_val"] = v.get("f64", v.get("f32"))
                r[side + "_sec"] = v.get("section")

    findings.sort(key=lambda f: (f["disposition"], f["unit"], f["function"]))
    Path(args.out).write_text(json.dumps(findings, indent=1))
    print(f"{len(findings)} function(s) with a named-vs-named pool relocation "
          f"mismatch (positional, counts held)")
    for f in findings:
        print(f"\n{f['disposition']:<10} {f['unit']}::{f['function']}  "
              f"({len(f['rows'])} row(s))")
        for r in f["rows"]:
            print(f"    [{r['idx']:>3}] {r['type']:<18} "
                  f"T {r['target']} = {r.get('target_val')!r} ({r.get('target_sec')})"
                  f"   O {r['ours']} = {r.get('ours_val')!r} ({r.get('ours_sec')})")
    print(f"\nwrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
