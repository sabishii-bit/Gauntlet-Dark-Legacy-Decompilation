"""Epilogue RESTORE-ORDER census, both streams, zero builds (run 51, lane CR).

IMPORTABLE CORE: population, classify_tail — pure over parsed object data;
no build, no printing, and importing this module has no side effects.

WHY THIS EXISTS. CritterDamagePlayer's entire residual is the order of its
epilogue restores: retail emits

    lwz r0,68(r1) / lmw r28,40(r1) / lfd f31,56(r1) / mtlr r0 / addi r1,r1,64

and our build emits

    lwz r0,68(r1) / lfd f31,56(r1) / lmw r28,40(r1) / addi r1,r1,64 / mtlr r0

over a BYTE-IDENTICAL prologue, frame and save set. That shape invites the
conclusion "different compiler build / different flags", which is the
expensive wrong turn. This census refutes it in seconds: both orders occur in
BOTH streams, and our compiler reproduces retail's choice on 107 of the 109
functions where the question arises.

It also reports the NEGATIVE half (AGENTS: calibration counts positives AND
negatives): cross-tabulating the population by saved-GPR count, inline-FPR
count, whether the FPR slot sits above the GPR block, and the FPR number
separates NOTHING — so the determinant is real, reproduced, and unidentified.

Backs claim.law.CR_mwcc-epilogue-restore-order-is-contextual-and-our-build-
reproduces-the-target-on-107-of-109-image-wide.20260904.v1 and is that law's
expiry check.

    python tools/gdl/composed_census/cr_epilogue_census.py [--examples]

Functions restoring FPRs through the _restfpr_N/_savefpr_N MSL helpers are
excluded by construction: that is a different emission path with no inline
`lfd` to order.
"""

import re
import sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve()
REPO = HERE.parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import fndiff  # noqa: E402

OURS_ROOT = REPO / "build" / "GUNE5D" / "src"
TARGET_ROOT = REPO / "build" / "GUNE5D" / "obj"


def _insns(lines):
    out = []
    for ln in lines:
        m = re.match(r"\s*([a-z][a-z0-9_.]*)\s*(.*)$", ln.strip())
        if m:
            out.append((m.group(1), m.group(2).strip()))
    return out


def classify_tail(ins):
    """Return the epilogue shape + candidate discriminants, or None.

    None means "this function does not restore both an inline lfd and an lmw",
    which is the whole exclusion rule.
    """
    mns = [m for m, _ in ins]
    if not mns or "blr" not in mns[-6:]:
        return None
    tail = ins[-9:]
    tm = [m for m, _ in tail]
    if "lmw" not in tm or "lfd" not in tm:
        return None
    i_lmw = max(i for i, m in enumerate(tm) if m == "lmw")
    i_lfd = max(i for i, m in enumerate(tm) if m == "lfd")
    restore = "LMW_FIRST" if i_lmw < i_lfd else "LFD_FIRST"
    frame_order = "no-pair"
    if "mtlr" in tm and "addi" in tm:
        i_mtlr = max(i for i, m in enumerate(tm) if m == "mtlr")
        i_addi = max(i for i, m in enumerate(tm) if m == "addi")
        frame_order = "MTLR_FIRST" if i_mtlr < i_addi else "ADDI_FIRST"

    frame = None
    for m, ops in ins:
        if m == "stwu":
            g = re.search(r"r1,-(\d+)\(r1\)", ops)
            if g:
                frame = int(g.group(1))
            break
    g = re.match(r"r(\d+),(\d+)\(r1\)", tail[i_lmw][1])
    lmw_reg, lmw_off = (int(g.group(1)), int(g.group(2))) if g else (None, None)
    g2 = re.match(r"f(\d+),(\d+)\(r1\)", tail[i_lfd][1])
    lfd_reg, lfd_off = (int(g2.group(1)), int(g2.group(2))) if g2 else (None, None)
    return {
        "restore": restore,
        "frame_order": frame_order,
        "frame": frame,
        "n_gpr": (32 - lmw_reg) if lmw_reg is not None else None,
        "n_fpr": sum(1 for m, _ in tail if m == "lfd"),
        "lfd_reg": lfd_reg,
        "fpr_above": (lfd_off > lmw_off) if (lmw_off is not None and lfd_off is not None) else None,
    }


def population(root):
    """Every function under `root` that restores both an inline lfd and an lmw.

    The `.postprocess/` mirror objects are skipped so each function counts once.
    """
    rows, parsed = [], 0
    for p in sorted(Path(root).rglob("*.o")):
        if ".postprocess" in str(p):
            continue
        try:
            fns = fndiff.parse(p)
        except Exception:
            continue
        for name, lines in fns.items():
            parsed += 1
            r = classify_tail(_insns(lines))
            if r:
                r["fn"] = name
                r["obj"] = str(p.relative_to(REPO))
                rows.append(r)
    return rows, parsed


def _tab(rows, key):
    d = defaultdict(lambda: defaultdict(int))
    for r in rows:
        d[r[key]][r["restore"]] += 1
    print("    by %s:" % key)
    for k in sorted(d, key=lambda x: (x is None, str(x))):
        print("      %-10s LMW_FIRST=%-4d LFD_FIRST=%-4d"
              % (str(k), d[k].get("LMW_FIRST", 0), d[k].get("LFD_FIRST", 0)))


def main(argv):
    show = "--examples" in argv
    ours, n_ours = population(OURS_ROOT)
    targ, n_targ = population(TARGET_ROOT)
    if not ours or not targ:
        print("REFUSED: no population parsed — are build/GUNE5D/src and "
              "build/GUNE5D/obj present? (obj/** is the dtk split; recover with "
              "provision_worktree.py --resplit)")
        return 2

    for label, rows, n in (("OURS (compiled)", ours, n_ours),
                           ("TARGET (dtk split)", targ, n_targ)):
        shapes = defaultdict(int)
        for r in rows:
            shapes[(r["restore"], r["frame_order"])] += 1
        print("==== %s — %d functions parsed, %d restore both lfd and lmw"
              % (label, n, len(rows)))
        for k in sorted(shapes, key=lambda k: -shapes[k]):
            print("    %-26s %5d" % ("%s/%s" % k, shapes[k]))
        for key in ("n_gpr", "n_fpr", "fpr_above", "lfd_reg"):
            _tab(rows, key)
        print()

    ours_lfd = {r["fn"] for r in ours if r["restore"] == "LFD_FIRST"}
    targ_lfd = {r["fn"] for r in targ if r["restore"] == "LFD_FIRST"}
    print("LFD_FIRST set — ours %d, target %d" % (len(ours_lfd), len(targ_lfd)))
    print("  in BOTH          :", ", ".join(sorted(ours_lfd & targ_lfd)) or "(none)")
    print("  OURS ONLY (the anomalies):", ", ".join(sorted(ours_lfd - targ_lfd)) or "(none)")
    print("  TARGET ONLY      :", ", ".join(sorted(targ_lfd - ours_lfd)) or "(none)")
    agree = len(ours) - len(ours_lfd ^ targ_lfd)
    print("AGREEMENT: our build reproduces the target's restore order on "
          "%d of %d functions in this population." % (agree, len(ours)))
    if show:
        for r in sorted(ours, key=lambda r: r["fn"]):
            if r["restore"] == "LFD_FIRST":
                print("    %-28s frame=%-5s n_gpr=%-4s n_fpr=%-3s lfd=f%-3s  %s"
                      % (r["fn"], r["frame"], r["n_gpr"], r["n_fpr"], r["lfd_reg"], r["obj"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
