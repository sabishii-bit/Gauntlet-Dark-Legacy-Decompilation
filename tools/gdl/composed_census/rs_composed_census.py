#!/usr/bin/env python3
"""RS composed census: the score-INVISIBLE reloc-identity population.

Enumerates every function that objdiff scores fuzzy == 100 (i.e. the
matched headline CONTAINS it) while fndiff still measures a residual.
Three independent residual views are taken per function, because each
one is blind to a different defect:

  real   -- fndiff --count's `real` (raw diff rows minus every reloc row)
  clean  -- fndiff --clean's status over reloc-NORMALIZED text
  relocs -- fndiff --relocs' address-resolved relocation SET delta

A function with fuzzy 100 and any of the three non-clean is a member of
the population. Rows are classified:

  RELOC_SET_DELTA        target/ours relocation sets differ by ADDRESS
                         -> a wrong callee/datum: a real defect
  NAMING_ONLY            instruction words + reloc types agree; the only
                         residual is anonymous-local vs named spelling
  TEXT_RESIDUAL          real > 0 with instruction words differing
  SPELLING_DRIFT         real 0 and reloc sets equal, but the raw symbol
                         TEXT differs (two names for one address) -- the
                         score-invisible source-correctness class
                         (ControllerMessageBox: gControllerButtons+0x4
                         where retail names sFlags)

Usage:
  python RS_scratch/rs_composed_census.py [--out build/GUNE5D/rs_census.json]
                                          [--unit-filter game/]
Run from the repository root.
"""
import argparse
import difflib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path("tools/gdl").resolve()))
import fndiff  # noqa: E402

VERSION = "GUNE5D"


def webfrank_pins():
    """{(unit, function)} for every WebFrank-pinned function.

    A pinned function's source is FROZEN (the postprocessor hash-asserts
    its body and the build ABORTS on drift), and fndiff scores the
    POSTPROCESSED object — so a pinned row in this census is a finding
    about the postprocessed bytes that no lane can act on without
    re-deriving the pin. Two of this run's rows were only discovered to
    be pinned by a failed ninja; the census says so up front.
    """
    path = Path(f"config/{VERSION}/webfrank.json")
    if not path.exists():
        return set()
    data = json.loads(path.read_text(encoding="utf-8"))
    return {(unit, rule.get("function"))
            for unit, rules in (data.get("units") or {}).items()
            for rule in rules}


def report_fuzzy(report_path):
    """{(unit, fn): fuzzy_percent} for every code function in the report."""
    data = json.loads(Path(report_path).read_text(encoding="utf-8"))
    out = {}
    for unit in data.get("units", []):
        name = unit.get("name") or ""
        if not name.startswith("main/"):
            continue
        unit_path = name[len("main/"):]
        # objdiff emits the function list at UNIT level; the .text section
        # entry carries only an aggregate. A first cut read
        # sections[].functions and silently scanned ZERO functions while
        # printing a confident "population 0".
        for fn in unit.get("functions") or []:
            m = fn.get("measures") or fn
            out[(unit_path, fn.get("name"))] = m.get(
                "fuzzy_match_percent", 0.0)
    return out


_LBL_RE = None


def rs_resolve(symbol):
    """Address for a relocation symbol, INCLUDING named `lbl_ADDR` data.

    fndiff.resolve_reloc_symbol refuses every `lbl_`/`jumptable_` spelling
    as an anonymous compiler local with no stable cross-object address.
    That is right for OUR object's pool entries and WRONG for the target's:
    dtk's `lbl_803445D4` is a real, addressed .sbss object that
    config/GUNE5D/symbols.txt carries with a size — so the address the
    target reads is knowable and the row can be decided. Without this,
    every such row keys to `<local>`, and a genuinely wrong datum
    (combat::MoveCam_walk reading 0x803445D4 where the target reads
    0x803444F0) is indistinguishable from a benign rename.
    """
    addr = fndiff.resolve_reloc_symbol(symbol)
    if addr is not None:
        return addr
    global _LBL_RE
    if _LBL_RE is None:
        import re
        _LBL_RE = re.compile(
            r"((?:lbl|jumptable|jtbl)_[0-9A-Fa-f]{6,8})"
            r"([+-]0x[0-9A-Fa-f]+|[+-]\d+)?$")
    m = _LBL_RE.match((symbol or "").strip())
    if not m:
        return None
    base = fndiff.symbol_addresses().get(m.group(1))
    if base is None:
        return None
    try:
        return base + int(m.group(2) or "0", 0)
    except ValueError:
        return None


def classify(t, b):
    """(real, clean_status, target_only, ours_only, spelling_pairs)."""
    raw = [ln for ln in difflib.unified_diff(t, b, lineterm="", n=0)
           if ln[:1] in "+-" and ln[:3] not in ("+++", "---")]
    real = fndiff.count_real(raw)

    tn = fndiff.normalized_reloc_lines(t)
    bn = fndiff.normalized_reloc_lines(b)
    clean_real = sum(
        1 for ln in difflib.unified_diff(tn, bn, lineterm="", n=0)
        if ln[:1] in "+-" and ln[:3] not in ("+++", "---"))

    t_rows = fndiff.reloc_rows_from_lines(t)
    b_rows = fndiff.reloc_rows_from_lines(b)

    # POSITIONAL pass. The relocation lists are in instruction order and
    # the instruction words agree, so position i on one side denotes the
    # same instruction on the other. Two rows that both resolve decide
    # each other outright:
    #   same address, different text -> SPELLING_DRIFT (links identical,
    #                                   the SOURCE names the wrong datum)
    #   different address            -> WRONG_DATUM (a real bug)
    spelling, wrong = [], []
    b_rows_cancelled = list(b_rows)
    if len(t_rows) == len(b_rows):
        for i, ((tt, ts), (bt, bs)) in enumerate(zip(t_rows, b_rows)):
            if tt != bt or ts == bs:
                continue
            ta, ba = rs_resolve(ts), rs_resolve(bs)
            if ta is None or ba is None:
                continue
            if ta == ba:
                spelling.append((ts, bs, f"0x{ta:08X}"))
                # Proven same datum: cancel it out of the SET delta so the
                # set view reports only rows the positional pass could NOT
                # decide. Otherwise one spelling drift shows up twice.
                b_rows_cancelled[i] = (tt, ts)
            else:
                wrong.append((ts, bs, f"0x{ta:08X}", f"0x{ba:08X}"))

    # Set delta on fndiff's own resolver (lbl_ = anonymous local): our
    # object emits `@N` pool entries that have no address at all, so
    # resolving the target's lbl_ names HERE would strand 563 benign rows
    # as false deltas (measured this run).
    target_only, ours_only, _ = fndiff.reloc_set_delta(
        t_rows, b_rows_cancelled)

    if wrong:
        kind = "WRONG_DATUM"
    elif target_only or ours_only:
        kind = "RELOC_SET_DELTA"
    elif fndiff.instruction_lines(t) != fndiff.instruction_lines(b):
        kind = "TEXT_RESIDUAL"
    elif spelling:
        kind = "SPELLING_DRIFT"
    elif clean_real or real:
        kind = "NAMING_ONLY"
    else:
        kind = "CLEAN"
    return real, clean_real, kind, target_only, ours_only, spelling, wrong


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=f"build/{VERSION}/rs_census.json")
    ap.add_argument("--report", default=f"build/{VERSION}/report.json")
    ap.add_argument("--unit-filter", default="")
    args = ap.parse_args()

    fuzzy = report_fuzzy(args.report)
    pins = webfrank_pins()
    units = {}
    for (unit, fn), pct in fuzzy.items():
        if pct >= 100.0 and unit.startswith(args.unit_filter):
            units.setdefault(unit, []).append(fn)

    rows = []
    scanned_units = 0
    scanned_fns = 0
    missing = []
    for unit in sorted(units):
        target_o = Path(f"build/{VERSION}/obj/{unit}.o")
        base_o = Path(f"build/{VERSION}/src/{unit}.o")
        if not target_o.exists() or not base_o.exists():
            missing.append(unit)
            continue
        target = fndiff.parse(target_o)
        base = fndiff.parse(base_o)
        scanned_units += 1
        for fn in sorted(units[unit]):
            name = fn
            if name not in target and not name.startswith("fn_"):
                import re
                name = re.sub(r"_80[0-9A-Fa-f]{6}$", "", name)
            t, b = target.get(name), base.get(name)
            if t is None or b is None:
                continue
            scanned_fns += 1
            (real, clean_real, kind, t_only, o_only,
             spelling, wrong) = classify(t, b)
            if kind == "CLEAN":
                continue
            rows.append({
                "unit": unit, "function": fn, "fuzzy": fuzzy[(unit, fn)],
                "real": real, "clean_real": clean_real, "kind": kind,
                "target_only": t_only, "ours_only": o_only,
                "spelling": spelling, "wrong_datum": wrong,
                "insns": len(fndiff.instruction_lines(t)),
                "webfrank_pinned": (unit, name) in pins
                or (unit, fn) in pins,
            })
        print(f"  scanned {unit}: {len(units[unit])} fuzzy-100 fn(s)",
              file=sys.stderr)

    out = {
        "scanned_units": scanned_units,
        "scanned_functions": scanned_fns,
        "missing_objects": missing,
        "population": len(rows),
        "by_kind": {},
        "rows": rows,
    }
    for row in rows:
        out["by_kind"][row["kind"]] = out["by_kind"].get(row["kind"], 0) + 1
    out["pinned_rows"] = sum(1 for r in rows if r["webfrank_pinned"])

    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    Path(args.out).write_text(json.dumps(out, indent=1), encoding="utf-8")

    print(f"\nCOMPOSED CENSUS: {scanned_fns} functions at objdiff fuzzy 100"
          f" across {scanned_units} unit(s)")
    print(f"POPULATION (fuzzy 100 with a residual in any view):"
          f" {len(rows)}")
    for kind, n in sorted(out["by_kind"].items(), key=lambda kv: -kv[1]):
        print(f"  {kind:<18} {n}")
    print(f"  ({out['pinned_rows']} of these are WebFrank-PINNED: source"
          " frozen, and the bytes scored here are POSTPROCESSED)")
    print()
    for row in sorted(rows, key=lambda r: (r["kind"], r["real"], r["unit"])):
        pin = " [PINNED]" if row["webfrank_pinned"] else ""
        print(f"{row['kind']:<18} real {row['real']:>4}"
              f"  clean {row['clean_real']:>4}  insns {row['insns']:>4}"
              f"  {row['unit']}::{row['function']}{pin}")
        for ts, bs, ta, ba in row["wrong_datum"]:
            print(f"      WRONG     target={ts} @{ta}"
                  f"   ours={bs} @{ba}")
        for ts, bs, addr in row["spelling"]:
            print(f"      spelling  target={ts}  ours={bs}  @{addr}")
        for line in row["target_only"]:
            print(f"      target-only {line}")
        for line in row["ours_only"]:
            print(f"      ours-only   {line}")
    print(f"\nwrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
