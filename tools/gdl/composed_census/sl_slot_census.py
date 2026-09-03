#!/usr/bin/env python3
"""Image-wide SLOT column: defake_gate roster's slot verdict for every unit.

`defake_gate.py roster <unit>` prints a SLOT column per TU (run-42 item 7),
but the class it names — save-set / frame-size / exclusive-slot deltas — is a
CROSS-TU family, and a lane assigned to it had no way to see the whole family
without one roster call per unit. This runs the same two `fndiff.parse` calls
per unit over every unit with both objects on disk, and prints the open rows
grouped the way the class is worked: FRAME rows ranked by |frame delta|
ascending (the cheapest frame closures first), then save-set rows, then
exclusive-only rows.

`real` is reported but is NOT the ranking key for frame rows: it actively
fights slot work (claim.law.real-can-underweight-a-large-alignment-gain-so-
arbitrate-conflicts-on-fuzzy.20260831.v1). Arbitrate a row on
`tools/gdl/slotdiff.py <unit> <fn>`.

THE `REAL` COLUMN IS THE ARBITER'S REAL since run 45 (item 5). It used to be
the `fndiff --clean` flavour (diff rows over reloc-normalized text) under a
header that a lane naturally read as the number probe.py and `fndiff --count`
report -- which is the OTHER computation, raw diff rows minus reloc lines
(fndiff.real_reconciliation names the confusion). The column now calls
`fndiff.count_real`, the same function `--count` calls, and prints the clean
flavour beside it as CLEAN only where the two disagree. Measured at 3d9896c18:
43 of 53 flagged rows disagree (41 where the arbiter is SMALLER, 2 where it is
LARGER -- the gap goes both ways), the flagged population is unchanged at 53
rows with the same keys, and the EXCLUSIVE-ONLY ranking changes from rank 9
(was game/game/gamemain::world_update, now game/enemy/enemy::do_enemy_move)
while the SAVE-SET ranking is unchanged. Verified against the tool itself:
`fndiff.py game/boss/boss HealthMeterStart --count` prints `lines 28 real 24`
and this census now prints REAL 24 / CLEAN 28; `fndiff.py game/ps2/fakelib
sDvdReadSync --count` prints `lines 18 real 10` against REAL 10 / CLEAN 18.

WebFrank-pinned functions are screened out of the open set (a pinned function
reads real 0 by construction and is not open work) but still counted.

Run from the repository root:
    python tools/gdl/composed_census/sl_slot_census.py
    python tools/gdl/composed_census/sl_slot_census.py --out build/GUNE5D/x.json

Calibration 2026-09-03 (run 43, SL lane), 257 units / 3032 functions
compared: 58 open rows flagged, 0 pinned rows flagged — reproducing
attempt.T12_tool-queue-12-nine-items-one-refuted-cause-one-latent-hazard
.20260903.v1's 58-row figure exactly, split 17 save-set / 15 frame-only /
26 exclusive-only (11 rows carry BOTH a save-set and a frame delta).
REMEASURED at 3d9896c18 (run 45): 257 units / 3032 functions, 53 open rows,
0 pinned, split 16 save-set / 21 frame / 26 exclusive-only. The population
moves as its own rows are worked — quote a live run, never this line.
"""
import argparse
import difflib
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import fndiff      # noqa: E402
import slotdiff    # noqa: E402

VERSION = "GUNE5D"


def pins(root):
    path = root / "config" / VERSION / "webfrank.json"
    out = {}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return out
    for unit, rules in (data.get("units") or {}).items():
        out[unit] = {r.get("function") for r in rules if r.get("function")}
    return out


def _rows(a, b):
    return [l for l in difflib.unified_diff(a, b, "target", "base",
                                            lineterm="", n=0)
            if l[:1] in "+-" and l[:3] not in ("+++", "---")]


def real_counts(t, b):
    """``(arbiter_real, clean_real)`` -- the TWO numbers called `real`.

    fndiff.real_reconciliation names the confusion and this census used to
    live inside it: it computed the `--clean` flavour (diff rows over
    reloc-NORMALIZED text) while its column header said `REAL`, and a lane
    reading the column compared it against what probe.py and `fndiff --count`
    report, which is the OTHER computation (raw diff rows minus reloc lines).
    They differ in either direction, so the column mis-ranked rows against
    the arbiter a lane actually uses (measured: 150 in this census against
    132 from the arbiter).

    `arbiter_real` is `fndiff.count_real` -- the same function `--count` and
    probe.py call, so the two can no longer drift -- and it is what this
    census ranks and prints. `clean_real` is kept and printed beside it
    whenever they disagree, because the gap is itself the finding.
    """
    arbiter = fndiff.count_real(_rows(t, b))
    clean = len(_rows(fndiff.normalized_reloc_lines(t),
                      fndiff.normalized_reloc_lines(b)))
    return arbiter, clean


def collect(root):
    obj = root / "build" / VERSION / "obj"
    src = root / "build" / VERSION / "src"
    pin_map = pins(root)
    units = sorted(p.relative_to(obj).with_suffix("").as_posix()
                   for p in obj.rglob("*.o")
                   if (src / f"{p.relative_to(obj).with_suffix('').as_posix()}"
                       ".o").exists())
    rows, n_fn, dropped = [], 0, []
    for unit in units:
        try:
            target = fndiff.parse(obj / f"{unit}.o")
            ours = fndiff.parse(src / f"{unit}.o")
        except (OSError, ValueError):
            continue
        upins = pin_map.get(unit, set())
        for name, our_lines in ours.items():
            tl = target.get(name)
            if tl is None:
                continue
            n_fn += 1
            t_slots = slotdiff.slot_map(tl)
            o_slots = slotdiff.slot_map(our_lines)
            t_save, o_save = slotdiff.save_set(tl), slotdiff.save_set(our_lines)
            t_frame = fndiff.frame_size(tl)
            o_frame = fndiff.frame_size(our_lines)
            parts, kinds = [], []
            if t_save != o_save:
                parts.append(f"save {t_save}/{o_save}")
                kinds.append("save")
            if t_frame != o_frame:
                parts.append(f"frame {t_frame}/{o_frame}")
                kinds.append("frame")
            ex_t = len(set(t_slots) - set(o_slots))
            ex_o = len(set(o_slots) - set(t_slots))
            if ex_t or ex_o:
                parts.append(f"{ex_t}T/{ex_o}O")
                kinds.append("excl")
            if not parts:
                continue
            real, clean = real_counts(tl, our_lines)
            if real <= 0:
                # The row is CLOSED as the arbiter sees it.  Rows where only
                # the clean flavour is non-zero are counted, not silently
                # dropped: that count is the size of the disagreement.
                if clean > 0:
                    dropped.append((unit, name, clean))
                continue
            rows.append({
                "unit": unit, "fn": name, "slot": ",".join(parts),
                "kinds": kinds, "real": real, "clean_real": clean,
                "pinned": name in upins,
                "frame_delta": (None if t_frame is None or o_frame is None
                                else t_frame - o_frame),
                "insns": f"{len(fndiff.instruction_lines(tl))}/"
                         f"{len(fndiff.instruction_lines(our_lines))}",
            })
    return units, n_fn, rows, dropped


def show(title, rows, key):
    print(f"\n-- {title} --")
    print(f"  {'UNIT':<28} {'FUNCTION':<34} {'INSNS':>10} {'REAL':>6}"
          f" {'CLEAN':>6} SLOT")
    for r in sorted(rows, key=key):
        clean = r.get("clean_real")
        clean_col = "-" if clean == r["real"] else str(clean)
        print(f"  {r['unit']:<28.28} {r['fn']:<34.34} {r['insns']:>10}"
              f" {r['real']:>6} {clean_col:>6} {r['slot']}")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=".")
    ap.add_argument("--out", default=f"build/{VERSION}/sl_slot_census.json")
    args = ap.parse_args()
    root = Path(args.root).resolve()
    units, n_fn, rows, dropped = collect(root)
    open_rows = [r for r in rows if not r["pinned"]]
    print(f"units={len(units)} functions_compared={n_fn} "
          f"flagged={len(rows)} open_flagged={len(open_rows)} "
          f"pinned_flagged={len(rows) - len(open_rows)}")
    disagree = [r for r in rows if r["clean_real"] != r["real"]]
    print(f"  REAL is `fndiff.count_real` -- what --count and probe.py report"
          f" and what this census now RANKS on; CLEAN is the --clean flavour,"
          f" printed only where it disagrees ({len(disagree)} of {len(rows)}"
          f" rows).")
    if dropped:
        print(f"  {len(dropped)} row(s) are CLOSED to the arbiter but non-zero"
              " to the clean flavour, so they are not listed: "
              + ", ".join(f"{u}::{f} (clean {c})" for u, f, c in dropped[:6]))
    print(f"  by class: save={sum(1 for r in open_rows if 'save' in r['kinds'])}"
          f" frame={sum(1 for r in open_rows if 'frame' in r['kinds'])}"
          f" excl-only={sum(1 for r in open_rows if r['kinds'] == ['excl'])}")
    show("FRAME rows, |delta| ascending (arbitrate on slotdiff, not real)",
         [r for r in open_rows if r["frame_delta"] not in (None, 0)],
         lambda r: (abs(r["frame_delta"]), r["real"]))
    show("SAVE-SET rows (no frame delta), real ascending",
         [r for r in open_rows
          if "save" in r["kinds"] and r["frame_delta"] in (None, 0)],
         lambda r: r["real"])
    show("EXCLUSIVE-ONLY rows, real ascending",
         [r for r in open_rows if r["kinds"] == ["excl"]],
         lambda r: r["real"])
    out = Path(args.out)
    if not out.is_absolute():
        out = root / out
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(rows, indent=1), encoding="utf-8")
    print(f"\nwrote {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
