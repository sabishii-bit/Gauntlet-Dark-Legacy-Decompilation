#!/usr/bin/env python3
"""CV stage 4: image-wide distributional census of the two dominant residual
families, measured on the RAW compiler output (.postprocess/body/*.o) so that
webfrank-pinned functions are counted honestly instead of reading real 0.

Families counted, per function, over equal-length target/ours pairs:

  LIVEZERO  a differing word where one side is `li rD,0` and the other is a
            register move (`mr rD,rS` / `addi rD,rS,0` / `or rD,rS,rS`).
            Direction recorded (target-copies vs target-remats).
  RECOLOR   every differing word has the same OPCODE and the same operand
            shape on both sides, differing only in register numbers
            (a necessary condition for a pure recoloring).
  PERMUTE   instruction MULTISETS are equal but the order differs.
  OTHER     anything else.

Output: per-family function counts, TU spread, and the pure-family population
(functions whose ENTIRE residual is that one family).
"""
import collections
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]  # repo root (fixed after promotion out of CV_scratch)
VERSION = "GUNE5D"
sys.path.insert(0, str(REPO / "tools" / "gdl"))
import matchtool  # noqa: E402

INSN = re.compile(r"^\s*[0-9a-fA-F]+:\s+(?:[0-9a-fA-F]{2} ){4}\s*(\S+)\s*(.*)$")
LI0 = re.compile(r"^li\s+r(\d+),\s*0$")
MOVE = re.compile(r"^(?:mr\s+r(\d+),\s*r(\d+)|addi\s+r(\d+),\s*r(\d+),\s*0"
                  r"|or\s+r(\d+),\s*r(\d+),\s*r\6)$")
# BOTH register files: an earlier revision of this script normalized only
# `r\d+`, which silently classified every FPR-only renaming (e.g.
# mb_particle::getSinCos) as OTHER and undercounted the recolor family.
REGNUM = re.compile(r"\b[rf]\d+\b")


def body(lines):
    """[(opcode, operands, rawtext)] for instruction lines only."""
    out = []
    for ln in lines:
        if ln.startswith("    R_PPC"):
            continue
        m = INSN.match(ln)
        if m:
            out.append((m.group(1), m.group(2).strip()))
        else:
            # fndiff's normalizer may already give 'opcode operands' form
            s = ln.strip()
            if s and not s.startswith("<") and not s.endswith(">:"):
                p = s.split(None, 1)
                out.append((p[0], p[1] if len(p) > 1 else ""))
    return out


def istext(op, ops):
    return f"{op} {ops}".strip()


def classify(t, o):
    """Return (family, detail) for a target/ours function pair."""
    if t == o:
        return "MATCH", 0
    if len(t) != len(o):
        return "LENGTH", abs(len(t) - len(o))
    if collections.Counter(t) == collections.Counter(o):
        return "PERMUTE", sum(1 for a, b in zip(t, o) if a != b)
    diffs = [(i, t[i], o[i]) for i in range(len(t)) if t[i] != o[i]]
    fams = set()
    for _, a, b in diffs:
        ta, tb = istext(*a), istext(*b)
        if (LI0.match(ta) and MOVE.match(tb)) or (LI0.match(tb) and MOVE.match(ta)):
            fams.add("LIVEZERO")
        elif a[0] == b[0] and REGNUM.sub("r#", a[1]) == REGNUM.sub("r#", b[1]):
            fams.add("RECOLOR")
        else:
            fams.add("OTHER")
    if fams == {"LIVEZERO"}:
        return "LIVEZERO", len(diffs)
    if fams == {"RECOLOR"}:
        return "RECOLOR", len(diffs)
    if fams == {"LIVEZERO", "RECOLOR"}:
        return "LIVEZERO+RECOLOR", len(diffs)
    if "LIVEZERO" in fams:
        return "MIXED-LIVEZERO", len(diffs)
    return "OTHER", len(diffs)


def main():
    # Every built object is RAW compiler output EXCEPT the webfrank-pinned
    # units, whose raw output lives under .postprocess/body/.  Prefer the body
    # object wherever one exists so pinned functions are counted honestly.
    src_root = REPO / "build" / VERSION / "src"
    bodies_by_unit = {}
    for b in sorted(src_root.rglob("*.o")):
        s = str(b).replace("\\", "/")
        if "/.postprocess/" in s and "/.postprocess/body/" not in s:
            continue
        unit = s.split(f"build/{VERSION}/src/", 1)[1]
        unit = unit.replace(".postprocess/body/", "").removesuffix(".o")
        raw = "/.postprocess/body/" in s
        if unit not in bodies_by_unit or raw:
            bodies_by_unit[unit] = b
    bodies = [bodies_by_unit[u] for u in sorted(bodies_by_unit)]
    npinned = sum(1 for b in bodies if ".postprocess" in str(b))
    print(f"objects scanned: {len(bodies)} ({npinned} raw pre-webfrank bodies, "
          f"{len(bodies) - npinned} unpinned)")
    fam_fns = collections.Counter()
    fam_tus = collections.defaultdict(set)
    livezero_dir = collections.Counter()
    livezero_sites = 0
    rows = []
    for b in bodies:
        unit = str(b).replace("\\", "/")
        unit = unit.split(f"build/{VERSION}/src/", 1)[1]
        unit = unit.replace(".postprocess/body/", "").removesuffix(".o")
        tgt = REPO / "build" / VERSION / "obj" / f"{unit}.o"
        if not tgt.exists():
            continue
        try:
            tf, of = matchtool.parse(tgt), matchtool.parse(b)
        except Exception as e:  # noqa: BLE001
            print(f"  skip {unit}: {e}")
            continue
        for name, tl in tf.items():
            ol = of.get(name)
            if ol is None:
                continue
            t, o = body(tl), body(ol)
            fam, n = classify(t, o)
            if fam == "MATCH":
                continue
            fam_fns[fam] += 1
            fam_tus[fam].add(unit)
            rows.append({"unit": unit, "fn": name, "family": fam,
                         "diffs": n, "insns": len(t)})
            if "LIVEZERO" in fam:
                for i in range(min(len(t), len(o))):
                    if t[i] != o[i]:
                        ta, tb = istext(*t[i]), istext(*o[i])
                        if LI0.match(ta) and MOVE.match(tb):
                            livezero_dir["target-remats / ours-copies"] += 1
                            livezero_sites += 1
                        elif LI0.match(tb) and MOVE.match(ta):
                            livezero_dir["target-copies / ours-remats"] += 1
                            livezero_sites += 1

    total_pairs = sum(fam_fns.values())
    print(f"\nnon-identical paired functions: {total_pairs}")
    print(f"{'family':22s} {'fns':>6s} {'TUs':>5s}")
    for k, v in fam_fns.most_common():
        print(f"{k:22s} {v:>6d} {len(fam_tus[k]):>5d}")
    print("\nlive-zero SITE directions:")
    for k, v in livezero_dir.most_common():
        print(f"  {k:34s} {v}")
    print(f"  total live-zero sites: {livezero_sites}")
    json.dump(rows, open(REPO / "build" / "CV_census.json", "w",
                         encoding="utf-8"), indent=1)
    print("\nwrote build/CV_census.json")

    # pure-family rosters (small ones printed)
    for fam in ("LIVEZERO", "RECOLOR", "PERMUTE", "LIVEZERO+RECOLOR"):
        sub = [r for r in rows if r["family"] == fam]
        sub.sort(key=lambda r: r["diffs"])
        print(f"\n== pure {fam}: {len(sub)} fns across "
              f"{len({r['unit'] for r in sub})} TUs ==")
        for r in sub[:40]:
            print(f"   d{r['diffs']:<4d} n{r['insns']:<5d} {r['unit']:<28s} {r['fn']}")
        if len(sub) > 40:
            print(f"   ... and {len(sub) - 40} more")


if __name__ == "__main__":
    main()
