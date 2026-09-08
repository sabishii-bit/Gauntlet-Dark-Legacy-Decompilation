#!/usr/bin/env python3
"""Read-only ledger over objdiff's DATA credit: who is paid, and what withholds it.

    python tools/gdl/data_credit.py                       # every unit with data
    python tools/gdl/data_credit.py game/game/player      # one unit, per section
    python tools/gdl/data_credit.py --verify              # reproduce matched_data
    python tools/gdl/data_credit.py --rank                # the uncredited frontier
    python tools/gdl/data_credit.py --rank --real-only --band 90 100
    python tools/gdl/data_credit.py --boundaries --band 90 100 --out build/x.json

This tool never builds, never edits config and never decides that a symbol
name is right. It reads `build/GUNE5D/report.json` and, only when asked to
classify a gap, the two objects objdiff already compared.

THE CREDIT LAW THIS EXISTS TO MAKE VISIBLE. A unit's `matched_data` is NOT
the sum of its matched data symbols. It is

    sum(section.size for section in unit if section is data
        and section.fuzzy_match_percent == 100.0)

-- all-or-nothing PER SECTION. A section at 99.99% is paid nothing. "Data"
means every section that is not CODE, and the code sections are `.text` and
`.init`. `.init` is the reason this is stated rather than assumed:
`TRK_MINNOW_DOLPHIN/Os/dolphin/dolphin_trk` carries `.init` (32 B) and its
`total_code` is 532 = `.text` 500 + `.init` 32, so a naive "data is
everything but .text" model over-credits it by exactly 32 bytes. `--verify`
therefore reconciles BOTH sums -- code and data -- against the unit's own
`total_code`/`total_data`, and prints any unit where either disagrees
instead of averaging it away.

WHY THAT MATTERS AND WHY IT IS EASY TO GET WRONG. `objdiff-cli diff` will
happily report a data symbol at 100.0% inside a section the report pays
zero for. Measured at 9b5bd089a on `game/enemy/enemy`: `gEnemies` (22900 B)
scored 100.0% in the one-shot diff while the unit's `matched_data` was 2880,
because one 84-byte symbol-boundary disagreement elsewhere held `.bss` at
98.536%. A lane reading the one-shot diff concludes the bytes are banked;
they are not. Splitting that one symbol later moved Game-Code Data from
53.89% to 71.84% in a single commit -- 26508 bytes that were always "100%
matched" at symbol level and never paid.

CALIBRATION (two-sided, over the live 368-unit report). POSITIVE: with the
partition above, **all 299 units carrying data reconcile exactly** -- both
their `matched_data` and their `total_data`. NEGATIVE, and the reason the
double reconciliation is not decoration: the first draft of this tool used
"data is everything but .text" and reported four units it could not explain
(`Runtime.PPCEABI.H/global_destructor_chain`,
`Runtime.PPCEABI.H/__init_cpp_exceptions`, `MSL/sincos`, and
`TRK_MINNOW_DOLPHIN/Os/dolphin/dolphin_trk`, whose modelled 92 exceeded its
reported total of 60). Every one of those was the same misclassification of
`.init`, not four report defects. A model that only checked `matched_data`
would have called three of them "unexplained" and silently mis-credited the
fourth. `--verify` prints any unit where either sum disagrees; if this list
is ever non-empty again, the partition -- not the report -- is the suspect.

CLASSIFYING A GAP. `--boundaries` answers the only question that decides
whether a section is cheap: is the shortfall a symbol BOUNDARY disagreement
(the two objects carve the same bytes into differently named or differently
sized objects -- a symbols.txt question) or a BYTE difference (a source
question)? It compares the {name: size} map per section between the target
object and ours and reports the symmetric difference. Verdicts:

  SOURCELESS  no source object exists (an `auto_*` unit); naming can never
              pay it, and it is excluded by `--real-only`.
  BOUNDARY    the name/size maps differ over named symbols; the offending
              names are printed. This is a CANDIDATE, not a proof: equal
              names and sizes do not prove equal intent, and a real fix
              still needs its own witnesses.
  BYTE        the maps agree yet the section is under 100%. The bytes
              differ; symbols.txt cannot help.
  ANONYMOUS   every differing name is compiler-generated (`@NN`, `@etb_`,
              `jumptable_`, `gap_`). MWCC renumbers `@NN` on any pool
              change, so these can never be paired by renaming either.

IMPORTABLE CORE: is_data_section, section_rows, model_matched, verify_rows,
rank_rows, base_of, is_real_tu, strip_visibility, symbol_map, classify_gap,
format_units,
format_rank, format_verify -- pure over dicts already read, except
symbol_map (and classify_gap when it is not given maps), which shell out to
objdump through datadiff.
"""
import argparse
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
VERSION = "GUNE5D"
REPORT = REPO / "build" / VERSION / "report.json"
TARGET_OBJ_DIR = REPO / "build" / VERSION / "obj"

# The report counts these toward total_code; everything else is data.
CODE_SECTIONS = frozenset({".text", ".init"})

ANON_RE = re.compile(r"^(@\d+$|@etb_|@eti_|jumptable_|gap_|\.\.\.)")

# objdump -t puts a visibility keyword in the NAME column, so the symbol
# reaches us as ".hidden @etb_80005CE0". Every EH record is hidden, so a
# classifier that does not strip this reads the entire extab/extabindex
# population as named symbols and reports BOUNDARY for all of it.
VISIBILITY_RE = re.compile(r"^\.(hidden|protected|internal)\s+")


class CreditUnavailable(RuntimeError):
    """A missing or failed measurement, never an empty successful comparison."""


def is_data_section(name):
    """The report's partition: data is every section that is not code."""
    return name not in CODE_SECTIONS


def _int(value):
    return int(value or 0)


def section_rows(unit):
    """[{name, size, fuzzy, credited}] for one report unit's DATA sections."""
    rows = []
    for section in unit.get("sections") or []:
        name = section.get("name")
        if not is_data_section(name):
            continue
        fuzzy = section.get("fuzzy_match_percent", 0.0)
        rows.append({"name": name,
                     "size": _int(section.get("size")),
                     "fuzzy": fuzzy,
                     "credited": fuzzy == 100.0})
    return rows


def model_matched(unit):
    """matched_data as the credit law predicts it."""
    return sum(row["size"] for row in section_rows(unit) if row["credited"])


def model_total_data(unit):
    """The unit's data bytes as the partition predicts them."""
    return sum(row["size"] for row in section_rows(unit))


def verify_rows(report):
    """One row per unit with data.

    (unit, total_data, reported_matched, modelled_matched, modelled_total)
    -- a unit is reconciled only when the reported matched bytes AND the
    reported total agree with the partition, so a misclassified section
    cannot hide inside a coincidentally equal matched figure.
    """
    rows = []
    for unit in report.get("units") or []:
        measures = unit.get("measures") or {}
        total = _int(measures.get("total_data"))
        if not total:
            continue
        rows.append((unit.get("name"), total,
                     _int(measures.get("matched_data")), model_matched(unit),
                     model_total_data(unit)))
    return rows


def base_of(unit_name):
    """`main/game/game/player` -> `game/game/player`."""
    text = str(unit_name)
    return text[len("main/"):] if text.startswith("main/") else text


def is_real_tu(unit_name):
    """An `auto_*` unit is a splitter artefact with no source; a TU is not."""
    return not base_of(unit_name).startswith("auto_")


def rank_rows(report, real_only=False, band=None):
    """Uncredited data sections, largest first: (size, unit, section, fuzzy)."""
    rows = []
    for unit in report.get("units") or []:
        name = unit.get("name")
        if real_only and not is_real_tu(name):
            continue
        for row in section_rows(unit):
            if row["credited"]:
                continue
            if band and not (band[0] <= row["fuzzy"] <= band[1]):
                continue
            rows.append((row["size"], name, row["name"], row["fuzzy"]))
    rows.sort(key=lambda r: (-r[0], r[1], r[2]))
    return rows


def _datadiff():
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import datadiff
    return datadiff


def strip_visibility(name):
    """`.hidden @etb_80005CE0` -> `@etb_80005CE0` (see VISIBILITY_RE)."""
    return VISIBILITY_RE.sub("", str(name))


def symbol_map(obj):
    """{section: {name: size}} over the OBJECT (data) symbols of one object file."""
    out = {}
    for sym in _datadiff().obj_symbols(obj):
        if not sym["object"] or sym["section_sym"]:
            continue
        out.setdefault(sym["section"], {})[strip_visibility(sym["name"])] = sym["size"]
    return out


def classify_gap(unit_name, section, target_map=None, ours_map=None):
    """Why a section is under 100%: SOURCELESS / BOUNDARY / ANONYMOUS / BYTE.

    Returns {verdict, only_target, only_ours, resized}. Callers may pass
    pre-read maps; that is what makes this testable without object files.
    """
    if target_map is None or ours_map is None:
        base = base_of(unit_name)
        target = TARGET_OBJ_DIR / (base + ".o")
        if not target.is_file():
            raise CreditUnavailable("missing target object: %s" % target)
        try:
            ours = _datadiff().ours_object(base)
        except Exception as exc:                      # noqa: BLE001 - reported
            return {"verdict": "SOURCELESS", "only_target": [],
                    "only_ours": [], "resized": [], "note": str(exc)}
        target_map = symbol_map(target)
        ours_map = symbol_map(getattr(ours, "path", ours))

    tgt = target_map.get(section, {})
    our = ours_map.get(section, {})
    only_target = sorted(set(tgt) - set(our))
    only_ours = sorted(set(our) - set(tgt))
    resized = sorted(n for n in set(tgt) & set(our) if tgt[n] != our[n])
    differing = [strip_visibility(n) for n in only_target + only_ours + resized]
    if not differing:
        verdict = "BYTE"
    elif all(ANON_RE.match(n) for n in differing):
        verdict = "ANONYMOUS"
    else:
        verdict = "BOUNDARY"
    return {"verdict": verdict, "only_target": only_target,
            "only_ours": only_ours, "resized": resized}


def format_units(report, wanted=None):
    lines = []
    for unit in report.get("units") or []:
        name = unit.get("name")
        if wanted and base_of(name) not in wanted:
            continue
        rows = section_rows(unit)
        if not rows:
            continue
        measures = unit.get("measures") or {}
        lines.append("%s  data %d/%d bytes credited"
                     % (name, _int(measures.get("matched_data")),
                        _int(measures.get("total_data"))))
        for row in rows:
            tail = ("CREDITED" if row["credited"]
                    else "withheld   gap to 100%%: %d byte(s)" % row["size"])
            lines.append("   %-12s size %-8d fuzzy %8.4f%%  %s"
                         % (row["name"], row["size"], row["fuzzy"], tail))
    return lines


def format_rank(rows, top=40):
    lines = ["%9s  %-42s %-12s %s" % ("bytes", "unit", "section", "fuzzy%")]
    for size, unit, section, fuzzy in rows[:top]:
        lines.append("%9d  %-42s %-12s %.4f" % (size, unit, section, fuzzy))
    lines.append("")
    lines.append("%d section(s), %d uncredited byte(s)"
                 % (len(rows), sum(r[0] for r in rows)))
    return lines


def format_verify(rows):
    bad = [r for r in rows if r[2] != r[3] or r[1] != r[4]]
    lines = ["credit law reproduces matched_data for %d of %d unit(s) with data"
             % (len(rows) - len(bad), len(rows))]
    for name, total, got, model, model_total in bad:
        lines.append("   UNEXPLAINED %-46s total=%d/%d reported=%d modelled=%d"
                     % (name, got and total or total, model_total, got, model))
    if not bad:
        lines.append("   no unexplained unit")
    return lines


def load_report(path):
    path = Path(path)
    if not path.is_file():
        raise CreditUnavailable("missing report: %s (run ninja first)" % path)
    return json.loads(path.read_text(encoding="utf-8"))


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Read-only ledger over objdiff's per-section data credit.")
    parser.add_argument("units", nargs="*", help="unit keys; default every unit")
    parser.add_argument("--report", default=str(REPORT))
    parser.add_argument("--verify", action="store_true",
                        help="reproduce matched_data and print unexplained units")
    parser.add_argument("--rank", action="store_true",
                        help="rank uncredited data sections by size")
    parser.add_argument("--boundaries", action="store_true",
                        help="classify each ranked section's gap (reads objects)")
    parser.add_argument("--real-only", action="store_true",
                        help="skip auto_* units, which have no source object")
    parser.add_argument("--band", nargs=2, type=float, metavar=("LO", "HI"),
                        help="only sections whose fuzzy%% is in [LO, HI]")
    parser.add_argument("--top", type=int, default=40)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args(argv)

    try:
        report = load_report(args.report)
    except CreditUnavailable as exc:
        print(exc, file=sys.stderr)
        return 2

    wanted = {re.sub(r"\.(c|cpp)$", "", u.replace("\\", "/").strip("/"))
              for u in args.units} or None
    band = tuple(args.band) if args.band else None
    payload, lines = {}, []

    if args.verify:
        rows = verify_rows(report)
        payload["verify"] = [{"unit": r[0], "total_data": r[1],
                              "reported": r[2], "modelled": r[3],
                              "modelled_total": r[4]} for r in rows]
        lines += format_verify(rows)
    if args.rank or args.boundaries:
        ranked = rank_rows(report, real_only=args.real_only, band=band)
        lines += format_rank(ranked, top=args.top)
        payload["rank"] = [{"bytes": r[0], "unit": r[1], "section": r[2],
                            "fuzzy": r[3]} for r in ranked]
    if args.boundaries:
        ranked = rank_rows(report, real_only=args.real_only, band=band)
        lines.append("")
        lines.append("gap classification (BOUNDARY rows are candidates, not proofs):")
        classified = []
        for size, unit, section, _fuzzy in ranked[:args.top]:
            try:
                verdict = classify_gap(unit, section)
            except CreditUnavailable as exc:
                lines.append("   %-42s %-12s UNAVAILABLE %s" % (unit, section, exc))
                continue
            classified.append({"unit": unit, "section": section,
                               "bytes": size, **verdict})
            names = (verdict["only_target"] + verdict["only_ours"]
                     + verdict["resized"])[:6]
            lines.append("   %-42s %-12s %-10s %s"
                         % (unit, section, verdict["verdict"], ", ".join(names)))
        payload["boundaries"] = classified
    if not (args.verify or args.rank or args.boundaries):
        lines += format_units(report, wanted)

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(payload, indent=1) + "\n", encoding="utf-8")
        print("wrote %s" % args.out)
    if args.json:
        print(json.dumps(payload, indent=1))
    else:
        for line in lines:
            print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
