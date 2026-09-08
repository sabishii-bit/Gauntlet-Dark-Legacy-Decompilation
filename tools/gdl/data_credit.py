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
  RELOC       the maps agree AND every byte is equal, yet the section is
              under 100% because relocations in it point at datums NO unit
              claims, so objdiff has nothing to pair them with. This is
              `.rodata`-CLAIM work, not source work. Run 62 measured
              `game/game/controls .data` at 99.0715% while `objcopy -O
              binary --only-section .data` on both objects gave 3252 == 3252
              with zero differing bytes and `datadiff --sections` reported
              `100.0% bytes equal`; the shortfall is 11 `R_PPC_ADDR32` rows
              at +0xC48..+0xC68 and +0xC80..+0xC88 whose target side names
              `lbl_80111F70..lbl_801120F8` in an UNCLAIMED `.rodata` while
              ours names `...rodata.0+off`. The mechanism is proved by the
              two neighbours that DO pair, +0xC44 and +0xC50: they point
              into `.sdata2`, which IS claimed. Classing that as BYTE told a
              lane to go looking for a byte difference that does not exist,
              and contradicted `datadiff` on the same tree.
  BYTE        the maps agree, no unpairable relocation explains it, yet the
              section is under 100%. The bytes differ; symbols.txt cannot
              help.
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


#: `objdump -r`: "00000c4c R_PPC_ADDR32      ...rodata.0+0x0000000c"
RELOC_ROW = re.compile(r"^([0-9a-f]{8})\s+(R_PPC\S+)\s+(\S+)$")
#: A dtk-invented name for a datum with no symbol of its own.
LBL_RE = re.compile(r"^lbl_([0-9A-Fa-f]{8})$")


def relocation_rows(obj):
    """{section: {offset: (type, symbol, addend)}} for one object file."""
    out, section = {}, None
    for line in _datadiff().dump_object(obj, "-r").splitlines():
        head = re.match(r"^RELOCATION RECORDS FOR \[(\S+)\]", line)
        if head:
            section = head.group(1)
            out.setdefault(section, {})
            continue
        row = RELOC_ROW.match(line)
        if row and section:
            symbol, addend = row.group(3), 0
            for separator, sign in (("+0x", 1), ("-0x", -1)):
                if separator in symbol:
                    symbol, tail = symbol.split(separator, 1)
                    addend = sign * int(tail, 16)
                    break
            out[section][int(row.group(1), 16)] = (row.group(2), symbol,
                                                   addend)
    return out


def claimed_runs(unit_name, splits=None):
    """[(section, start, end)] this unit claims, from splits.txt."""
    splits = _datadiff().parse_splits() if splits is None else splits
    base = base_of(unit_name)
    for key, sections in splits.items():
        if re.sub(r"\.(c|cpp|s)$", "", key) == base:
            return [(name, lo, hi) for name, (lo, hi) in sections.items()]
    return []


def unpairable_relocations(section, target_relocs, our_relocs, runs):
    """Rows relocating the same word to symbols nothing can pair.

    A word both objects relocate, where the target names an address NO run
    of this unit claims, has no counterpart symbol on our side at all: our
    object reaches the same datum through its own section base
    (`...rodata.0 + off`) because the datum is not in the split. objdiff
    scores that word as unmatched however identical the bytes are, and no
    renaming or source edit can pay it -- only claiming the run can.

    A differing row whose target address IS inside one of this unit's runs
    is not a defect and not evidence either: the split has a symbol there,
    so objdiff pairs the two however differently they are spelled (controls
    +0xC44, ours `@1` against the target's `lbl_803463F8`). Those are
    returned as `pairable`. Only a row whose target address cannot be
    resolved at all is `unexplained`, and that stops the RELOC verdict.

    Returns {"rows", "pairable", "unexplained"}.
    """
    # EVERY run this unit claims, not just this section's: a `.data` pointer
    # into the unit's own claimed `.sdata2` pairs fine even though the two
    # objects spell the symbol differently (controls +0xC44, ours `@1` and
    # the target `lbl_803463F8`). What cannot pair is a pointer to an
    # address no run of this unit covers at all.
    claimed = [(lo, hi) for _name, lo, hi in runs]
    rows, pairable, unexplained = [], [], []
    theirs = target_relocs.get(section, {})
    mine = our_relocs.get(section, {})
    for offset in sorted(set(theirs) & set(mine)):
        their_kind, their_symbol, their_addend = theirs[offset]
        our_kind, our_symbol, our_addend = mine[offset]
        if (their_kind, their_symbol, their_addend) == \
                (our_kind, our_symbol, our_addend):
            continue
        literal = LBL_RE.match(their_symbol)
        address = int(literal.group(1), 16) + their_addend if literal else None
        inside = address is not None and any(lo <= address < hi
                                             for lo, hi in claimed)
        row = {"offset": offset, "target_symbol": their_symbol,
               "our_symbol": our_symbol, "our_addend": our_addend,
               "target_address": None if address is None
               else "0x%08X" % address}
        if address is None:
            unexplained.append(row)
        elif inside:
            pairable.append(row)
        else:
            rows.append(row)
    return {"rows": rows, "pairable": pairable, "unexplained": unexplained}


def classify_gap(unit_name, section, target_map=None, ours_map=None,
                 target_relocs=None, our_relocs=None, bytes_equal=None,
                 runs=None):
    """Why a section is under 100%.

    SOURCELESS / BOUNDARY / ANONYMOUS / RELOC / BYTE, in that refusal order.
    Returns {verdict, only_target, only_ours, resized, unpairable_relocs}.
    Callers may pass pre-read maps, relocations, byte equality and claimed
    runs; that is what makes this testable without object files. RELOC is
    only ever reached with a measured `bytes_equal` -- an unmeasured byte
    comparison must never turn a byte difference into a claim question.
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
                    "only_ours": [], "resized": [], "note": str(exc),
                    "unpairable_relocs": []}
        ours_path = getattr(ours, "path", ours)
        target_map = symbol_map(target)
        ours_map = symbol_map(ours_path)
        if target_relocs is None or our_relocs is None:
            target_relocs = relocation_rows(target)
            our_relocs = relocation_rows(ours_path)
        if bytes_equal is None:
            datadiff = _datadiff()
            try:
                bytes_equal = (datadiff.section_bytes(target, section)
                               == datadiff.section_bytes(ours_path, section))
            except Exception:             # noqa: BLE001 - an absent section
                # One object does not carry the section at all (a BSS-class
                # run, or a `.ctors` only one side emits). Unmeasured byte
                # equality must never promote a row to RELOC.
                bytes_equal = None
        if runs is None:
            runs = claimed_runs(unit_name)

    tgt = target_map.get(section, {})
    our = ours_map.get(section, {})
    only_target = sorted(set(tgt) - set(our))
    only_ours = sorted(set(our) - set(tgt))
    resized = sorted(n for n in set(tgt) & set(our) if tgt[n] != our[n])
    differing = [strip_visibility(n) for n in only_target + only_ours + resized]
    row = {"only_target": only_target, "only_ours": only_ours,
           "resized": resized, "unpairable_relocs": []}
    if differing:
        row["verdict"] = ("ANONYMOUS" if all(ANON_RE.match(n)
                                             for n in differing)
                          else "BOUNDARY")
        return row
    if bytes_equal and target_relocs is not None and our_relocs is not None:
        found = unpairable_relocations(section, target_relocs, our_relocs,
                                       runs or [])
        row["unpairable_relocs"] = found["rows"]
        row["pairable_relocs"] = found["pairable"]
        row["relocs_not_explained"] = found["unexplained"]
        if found["rows"] and not found["unexplained"]:
            row["verdict"] = "RELOC"
            return row
    row["verdict"] = "BYTE"
    return row


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
            if verdict["verdict"] == "RELOC":
                rows = verdict["unpairable_relocs"]
                lines.append("        every byte of this section is equal;"
                             " %d relocation(s) point at datums no run of"
                             " this unit claims, so objdiff has nothing to"
                             " pair them with. CLAIM work, not source work"
                             " -- run tools/gdl/claimable_sections.py %s"
                             % (len(rows), unit))
                for row in rows[:6]:
                    lines.append("        +0x%04X %s -> target %s (%s)"
                                 % (row["offset"], row["our_symbol"],
                                    row["target_symbol"],
                                    row["target_address"]))
                if len(rows) > 6:
                    lines.append("        ... %d more" % (len(rows) - 6))
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
