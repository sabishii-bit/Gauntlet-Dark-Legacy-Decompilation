#!/usr/bin/env python3
"""Immediate-delta families across a TU, bound to the DATA FACT that explains them.

    python tools/gdl/composed_census/dispdelta.py <unit>
    python tools/gdl/composed_census/dispdelta.py <unit> <fn> [<fn> ...]
    python tools/gdl/composed_census/dispdelta.py <unit> --min 3
    python tools/gdl/composed_census/dispdelta.py <unit> --no-facts
    python tools/gdl/composed_census/dispdelta.py <unit> --out build/x.json

WHAT THIS SEPARATES. A residual of paired same-opcode instructions differing
only in a DISPLACEMENT is not register allocation. When many such rows share
ONE signed delta, that delta is a fact about DATA — a missing datum before
our section, an object emitted at the wrong offset, a missing struct field —
and the whole family closes at once when the datum is recovered. Lane P1
turned game/game/player's "allocation residuals" into exactly two data
constants this way, and lanes then predicted (and lane P2 confirmed) which
functions would go exact the moment the `.bss` order was fixed.

THE FAMILIES IT RECOVERED on game/game/player at 77dd0fdef:

    +840  = 0x348   `.rodata` FRONT DEFICIT — claimable_sections reports
                    our section short 0x348 at the front, and
                    write_health_and_items reads `addi r7,r29,868/884/896`
                    where we read `28/44/56`.
     +12            SEVEN `.bss` objects (lbl_802757E0, rune13_blit,
                    pm_blit, key_blit, crystal_blit, rune_blit, frame_blit)
                    sit 12 bytes LATER in the target than in our object.
    -768            the same fact's other half: `gDefaultPlayerPosition`,
                    the one non-static in that run, sits 768 bytes EARLIER
                    in the target (target +0x934, ours +0xC34).
   +3136            `do_players` reaches the NEXT TU's object off player's
                    own `.bss` base where we materialise `gPlayers`.

FOUR THINGS IT REFUSES TO COUNT, each measured in the predecessor's output
(build/p1_lane/p1_delta.txt, lane P1, 2026-09-08) as a family that was not
one. This is the calibration, and it is why this is not that script:

  BRANCH DISPLACEMENTS. `beq <fn+0x1ec>` against `beq <fn+0x200>` produced
  the families `+199 x3` and `-199 x2` — from reading the LAST DIGIT RUN of
  a hex branch target ("1" and "200"). Branch rows are counted in their own
  bucket and never enter a delta family: under a count delta every branch
  in the function shifts, which is fallout, not a datum.

  RELOCATION ROWS. `R_PPC_ADDR16_HA jumptable_80120BA8` against `@3763`
  produced `delta -3755` — the digits of two SYMBOL NAMES subtracted. A
  relocation-annotation row has no immediate at all; `fndiff --relocs` is
  the view that decides those.

  HEX vs DECIMAL. `andi. r0,r0,0x7fff` and `addi r3,r3,32767` hold the same
  value and the predecessor read `7` from one and `32767` from the other.
  Immediates are parsed by OPERAND FORM here and converted with base 0.

  STACK SLOTS. `addi r4,r1,24` against `addi r4,r1,12` is a missing 12-byte
  LOCAL, and it landed in the same `+12` family as the `.bss` rows —
  which is how one number came to stand for two unrelated facts. Rows
  based on r1 are reported as a separate STACK family (`slotdiff.py` is
  the tool that decides those).

HOW IT PAIRS. `regnorm.analyze` — the project's aligned-row classifier,
imported, not re-run per function as a subprocess — and only its
STRUCTURAL rows, so RENAMING rows (a pure recolor) and its four artifact
annotations (`alignment`, `branch-disp`, `schedule-disp`, reloc naming)
are already off the table. A count-asymmetric function still reports, and
its rows are labelled: after an insertion, positional pairing near the gap
can lie, and AGENTS.md requires that be said rather than assumed away.

THE DATA FACTS come from measurements this tool does not make itself:
`claimable_sections.census` for per-section FRONT DEFICITS, and
`claimable_sections.object_symbols` for both objects' symbol tables, whose
same-name/same-section offset difference is the displacement of a real
object. `--no-facts` skips both (they cost a DOL read and four objdumps).
A delta with NO matching fact is reported as `unexplained` — that is the
honest answer and it is the queue this tool exists to shorten.

EXIT 0 when the measurement completed; a residual is data, not a failure.
EXIT 2 when it did not happen (missing object, unknown function).
"""
import argparse
import collections
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))

import claimable_sections as cs                           # noqa: E402
import cliscreen                                          # noqa: E402
import fndiff                                             # noqa: E402
import regnorm                                            # noqa: E402

OK, REFUSED = 0, 2

#: `<mnem> rD,<disp>(rA)` — every d-form load/store. The displacement is
#: the immediate and rA is the base the fact is about.
D_FORM = re.compile(
    r"^(\S+)\s+[^,]+,\s*(-?(?:0[xX][0-9a-fA-F]+|\d+))\((r\d+)\)$")

#: Instructions whose last operand is an immediate AND whose previous
#: operand is the register the immediate is measured FROM. A whitelist, not
#: "the last number on the line": `rlwinm r3,r3,2,0,29` ends in a mask
#: field and `srawi rA,rS,SH` in a shift count, and subtracting two of
#: those is not a displacement family.
BASE_OPS = frozenset("""
    addi addis addic addic. subi subis subic subic. subfic
    ori oris xori xoris andi. andis. mulli
""".split())

#: The same shape with NO base: the one register operand is a destination
#: or a compared value, so calling it a base would name the wrong fact.
NO_BASE_OPS = frozenset("li lis cmpwi cmplwi cmpdi cmpldi cmpi cmpli".split())

IMMEDIATE_OPS = BASE_OPS | NO_BASE_OPS

LAST_IMMEDIATE = re.compile(r"^(\S+)\s+(.*?),\s*"
                            r"(-?(?:0[xX][0-9a-fA-F]+|\d+))$")
REGISTER = re.compile(r"^r\d+$")
BRANCH = re.compile(r"^b[a-z+.-]*$")


def mnemonic(line):
    return line.split()[0] if line and not line.startswith("    ") else None


def immediate_of(line):
    """(value, base_register) for an instruction with one immediate.

    base_register is 'r1' for a frame reference, another `rN` for a d-form
    or `addi`-style base, and None when the form has no base operand
    (`li`, `lis`, `cmpwi`). Returns None when the line has no immediate in
    a form this tool will subtract.
    """
    text = line.strip()
    match = D_FORM.match(text)
    if match:
        return int(match.group(2), 0), match.group(3)
    match = LAST_IMMEDIATE.match(text)
    if not match:
        return None
    mnem, head, value = match.group(1), match.group(2), match.group(3)
    if mnem not in IMMEDIATE_OPS:
        return None
    operands = [token.strip() for token in head.split(",")]
    base = None
    if mnem in BASE_OPS and len(operands) >= 2 \
            and REGISTER.match(operands[-1]):
        base = operands[-1]
    return int(value, 0), base


CLASS_MNEMONIC = "MNEMONIC-DIFFERS"
CLASS_BRANCH = "BRANCH-DISPLACEMENT"
CLASS_RELOC = "RELOCATION-ROW"
CLASS_NONE = "NO-IMMEDIATE"
CLASS_EQUAL = "IMMEDIATES-EQUAL"


DeltaRow = collections.namedtuple(
    "DeltaRow", "kind offset delta base target ours same_base")


def rows_for_function(target_lines, ours_lines, resolver=None):
    """[DeltaRow] for one function's aligned STRUCTURAL rows.

    `kind` is 'IMMEDIATE' for a countable row and one of the CLASS_*
    strings for a row deliberately excluded from every family.
    `same_base` says whether both sides read the SAME base register: a
    displacement family with a low same-base share is not necessarily one
    fact, because a differing base can mean a DIFFERENT OBJECT rather than
    a different offset into one (`addi r18,r31,3136` against
    `addi r19,r18,0` subtracts to 3136 and is a fold, not an offset).
    """
    result = regnorm.analyze(target_lines, ours_lines, resolver)
    out = []
    for row in result.rows:
        if row.kind != "STRUCTURAL":
            continue
        if row.target is None or row.ours is None:
            continue
        if row.artifact:                 # regnorm already called it noise
            continue
        target_text, ours_text = row.target, row.ours

        def excluded(kind):
            return DeltaRow(kind, row.offset, None, None,
                            target_text, ours_text, None)

        if target_text.startswith("    ") or ours_text.startswith("    "):
            out.append(excluded(CLASS_RELOC))
            continue
        t_mnem, o_mnem = mnemonic(target_text), mnemonic(ours_text)
        if t_mnem != o_mnem:
            out.append(excluded(CLASS_MNEMONIC))
            continue
        if BRANCH.match(t_mnem or "") and t_mnem != "bl":
            out.append(excluded(CLASS_BRANCH))
            continue
        t_imm = immediate_of(target_text)
        o_imm = immediate_of(ours_text)
        if t_imm is None or o_imm is None:
            out.append(excluded(CLASS_NONE))
            continue
        delta = t_imm[0] - o_imm[0]
        out.append(DeltaRow("IMMEDIATE" if delta else CLASS_EQUAL,
                            row.offset, delta, t_imm[1] or o_imm[1],
                            target_text, ours_text, t_imm[1] == o_imm[1]))
    return out


def symbol_displacements(target_object, ours_object):
    """{delta: [(name, section, ours_offset, target_offset, size)]}.

    A name defined in BOTH objects in the SAME section, at different
    section offsets, IS the data fact: the object moved. `delta` is
    target_offset - ours_offset, the same direction as an immediate delta,
    so the two match without a sign convention to remember.
    """
    ours = cs.object_symbols(ours_object)
    target = cs.object_symbols(target_object)
    out = collections.defaultdict(list)
    for name, (section, value, size) in ours.items():
        found = target.get(name)
        if not found or found[0] != section:
            continue
        delta = found[1] - value
        if delta:
            out[delta].append((name, section, value, found[1], size))
    return dict(out)


def object_sizes(ours_object):
    """{size: [names]} for every sized datum our object defines."""
    out = collections.defaultdict(list)
    for name, (section, _value, size) in cs.object_symbols(ours_object).items():
        if size and section not in (".text", "*ABS*"):
            out[size].append(name)
    return dict(out)


def data_section_sizes(ours_object):
    """{size: [section]} for our object's DATA sections.

    A displacement equal to a whole section's size reaches the byte just
    PAST that section's end from its base — i.e. the target folds the next
    object in the link onto this section's base. That is the +3136 family
    in game/game/player: our `.bss` is 0xC40 = 3136 bytes, and `do_players`
    reads `addi rD,r31,3136` off player's own `.bss` base where we
    materialise `gPlayers` separately.
    """
    out = collections.defaultdict(list)
    for section, size in cs.section_sizes(ours_object).items():
        if size and section in cs.DATA_SECTIONS:
            out[size].append(section)
    return dict(out)


def front_deficits(unit):
    """{section: bytes missing before our section's first datum}.

    From `claimable_sections.census`, the tool that already derives it
    against the DOL. `defined=None` skips its orphan-extern screen, which
    is the expensive half and is not what this reads.
    """
    splits_path = os.path.join(ROOT, "config", "GUNE5D", "splits.txt")
    if not os.path.exists(splits_path):
        return {}
    splits = cs.parse_splits(splits_path)
    row = cs.census(unit, splits, cs.claimed_intervals(splits), defined=None)
    out = {}
    for section in row.get("sections", []):
        if section.get("front_deficit"):
            out[section["section"]] = section["front_deficit"]
    return out


def explain(delta, displacements, deficits, sizes, section_sizes=None):
    """[fact strings] for one delta, most decisive first, or []."""
    facts = []
    moved = displacements.get(delta) or []
    if moved:
        names = ", ".join(name for name, *_rest in moved[:8])
        more = "" if len(moved) <= 8 else " (+%d more)" % (len(moved) - 8)
        section = moved[0][1]
        facts.append(
            "%s: %d named object(s) sit %d byte(s) %s in the TARGET than in"
            " our object — %s%s. Only names DEFINED IN BOTH objects can be"
            " compared, and the extracted object carries dtk's split names,"
            " so an unnamed run of ours is invisible to this check"
            % (section, len(moved), abs(delta),
               "LATER" if delta > 0 else "EARLIER", names, more))
    for section, deficit in sorted(deficits.items()):
        if deficit == delta:
            facts.append(
                "%s FRONT DEFICIT 0x%X (%d B): claimable_sections reports our"
                " section short by exactly this much before its first datum,"
                " so every displacement into it is low by the same amount"
                % (section, deficit, deficit))
    for section in sorted((section_sizes or {}).get(abs(delta)) or []):
        facts.append(
            "equals the WHOLE SIZE of our %s (0x%X = %d B): a displacement"
            " this large off that section's base reaches the byte just PAST"
            " its end, i.e. the NEXT object in the link — check whether the"
            " target folds a neighbour's datum onto this base"
            % (section, abs(delta), abs(delta)))
    named = sizes.get(abs(delta)) or []
    if named and not moved and not facts:
        facts.append(
            "equals the SIZE of %d datum(s) our object defines (%s) — a"
            " candidate missing object or field, not yet a proof"
            % (len(named), ", ".join(sorted(named)[:6])))
    return facts


def measure(unit, functions=None, raw=False, facts=True):
    """The whole record for one unit. Pure of printing."""
    target_table, ours_table, resolver = regnorm.load_tables(unit, raw)
    names = []
    if functions:
        for wanted in functions:
            resolved = fndiff.resolve_function_name(
                dict(target_table, **ours_table), wanted)
            if resolved is None or resolved not in target_table \
                    or resolved not in ours_table:
                raise SystemExit("dispdelta: %s has no function %r in both"
                                 " objects" % (unit, wanted))
            names.append(resolved)
    else:
        names = [name for name in target_table if name in ours_table]

    families = collections.Counter()
    same_base = collections.Counter()
    buckets = collections.Counter()
    per_function = collections.defaultdict(collections.Counter)
    examples = collections.defaultdict(list)
    stack = collections.Counter()
    asymmetric = []
    for name in names:
        target_lines, ours_lines = target_table[name], ours_table[name]
        if len(fndiff.instruction_lines(target_lines)) \
                != len(fndiff.instruction_lines(ours_lines)):
            asymmetric.append(name)
        for row in rows_for_function(target_lines, ours_lines, resolver):
            if row.kind != "IMMEDIATE":
                buckets[row.kind] += 1
                continue
            key = ("stack %+d" if row.base == "r1" else "%+d") % row.delta
            if row.base == "r1":
                stack[row.delta] += 1
            else:
                families[row.delta] += 1
                same_base[row.delta] += 1 if row.same_base else 0
            per_function[name][key] += 1
            examples[key].append("%s @0x%04x  T %s | O %s"
                                 % (name, row.offset, row.target, row.ours))

    displacements, deficits, sizes, section_sizes = {}, {}, {}, {}
    if facts:
        ours_object, _kind = cs_paths(unit, raw)
        target_object = os.path.join(ROOT, "build", "GUNE5D", "obj",
                                     unit + ".o")
        displacements = symbol_displacements(target_object, ours_object)
        sizes = object_sizes(ours_object)
        section_sizes = data_section_sizes(ours_object)
        deficits = front_deficits(unit)

    return {
        "unit": unit, "functions": len(names),
        "count_asymmetric": sorted(asymmetric),
        "families": [
            {"delta": delta, "rows": rows, "same_base": same_base[delta],
             "functions": sorted(fn for fn in per_function
                                 if per_function[fn]["%+d" % delta]),
             "facts": explain(delta, displacements, deficits, sizes,
                              section_sizes),
             "examples": examples["%+d" % delta][:6]}
            for delta, rows in families.most_common()],
        "stack_families": [
            {"delta": delta, "rows": rows,
             "examples": examples["stack %+d" % delta][:4]}
            for delta, rows in stack.most_common()],
        "excluded": dict(buckets),
        "section_front_deficits": deficits,
        "symbol_displacement_deltas":
            {str(delta): [name for name, *_rest in rows]
             for delta, rows in sorted(displacements.items())},
    }


def cs_paths(unit, raw):
    """(our object path, description) — the SAME resolver every tool uses."""
    if raw:
        import fnasm
        return os.path.join(ROOT, fnasm.raw_obj_path(unit)), "raw"
    return os.path.join(ROOT, "build", "GUNE5D", "src", unit + ".o"), "src"


def render(record, minimum=1, examples=True):
    print("== %s: %d function pair(s) measured" % (record["unit"],
                                                   record["functions"]))
    if record["count_asymmetric"]:
        print("   %d COUNT-ASYMMETRIC function(s): positional pairing near"
              " the insertion can pair unrelated rows, so read their"
              " examples before believing a family — %s"
              % (len(record["count_asymmetric"]),
                 ", ".join(record["count_asymmetric"][:8])))
    shown = [row for row in record["families"] if row["rows"] >= minimum]
    print("\nDATA-BASE IMMEDIATE FAMILIES (target immediate - ours),"
          " %d of %d family(ies) with >= %d row(s)"
          % (len(shown), len(record["families"]), minimum))
    print("  %6s  %-10s %-11s %s"
          % ("rows", "delta", "same base", "data fact"))
    for row in shown:
        facts = row["facts"] or ["unexplained — no section deficit, object"
                                 " displacement, section size or datum size"
                                 " matches it"]
        print("  %6d  %-10s %-11s %s"
              % (row["rows"], "%+d" % row["delta"],
                 "%d/%d" % (row["same_base"], row["rows"]), facts[0]))
        for extra in facts[1:]:
            print("  %6s  %-10s %-11s %s" % ("", "", "", extra))
    if record["stack_families"]:
        print("\nSTACK FAMILIES (base r1 — a FRAME question, not a datum;"
              " decide these with slotdiff.py)")
        for row in record["stack_families"]:
            print("  %6d  %+d" % (row["rows"], row["delta"]))
    print("\nROWS EXCLUDED FROM EVERY FAMILY, by why")
    for kind, count in sorted(record["excluded"].items()):
        print("  %6d  %s" % (count, kind))
    if not record["excluded"]:
        print("  %6d  (none)" % 0)
    if examples:
        print("\nEXAMPLES")
        for row in shown:
            print("  -- delta %+d (%d rows, %d function(s)) --"
                  % (row["delta"], row["rows"], len(row["functions"])))
            for line in row["examples"]:
                print("      " + line)


def main(argv=None):
    cliscreen.help_only(__doc__)
    parser = argparse.ArgumentParser(
        prog="dispdelta.py",
        description="Immediate-delta families across a TU, bound to the data"
                    " fact that explains them.")
    parser.add_argument("unit")
    parser.add_argument("functions", nargs="*")
    parser.add_argument("--min", type=int, default=1,
                        help="only show families with at least N rows")
    parser.add_argument("--raw", action="store_true",
                        help="score the raw compiler object")
    parser.add_argument("--no-facts", action="store_true",
                        help="skip the data-fact binding (no DOL read)")
    parser.add_argument("--no-examples", action="store_true")
    parser.add_argument("--out", help="write the record as JSON (under build/)")
    args = parser.parse_args(argv)
    unit = fndiff.unit_key(args.unit)
    try:
        record = measure(unit, args.functions or None, raw=args.raw,
                         facts=not args.no_facts)
    except SystemExit as refusal:
        if isinstance(refusal.code, int):
            raise
        print(str(refusal))
        return REFUSED
    except (OSError, cs.Unmeasurable) as error:
        print("dispdelta REFUSED: %s" % error)
        return REFUSED
    render(record, minimum=args.min, examples=not args.no_examples)
    if args.out:
        out = os.path.join(ROOT, args.out)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w", encoding="utf-8") as handle:
            json.dump(record, handle, indent=2, sort_keys=True)
        print("\nwrote %s" % args.out)
    return OK


if __name__ == "__main__":
    sys.exit(main())
