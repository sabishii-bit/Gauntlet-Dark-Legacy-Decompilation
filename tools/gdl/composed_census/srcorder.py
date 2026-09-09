#!/usr/bin/env python3
"""Bound a PDB-ABSENT function to a GC SOURCE SLOT by re-reading the Xbox order.

    python tools/gdl/composed_census/srcorder.py game/game/player
    python tools/gdl/composed_census/srcorder.py game/mb/mb_tree --quiet
    python tools/gdl/composed_census/srcorder.py <unit> --module PLAYER.OBJ
    python tools/gdl/composed_census/srcorder.py <unit> --out build/x.json

WHAT IT ANSWERS. A discarded-helper hunt (the StartCompass /
any_player_walking device) needs to know WHERE IN THE SOURCE the missing
function sat, because a pool slot, a `.bss` seat and an EH record are all
ordered by source position. `research/xbox_symbols/functions_by_module.txt`
names ~47 PLAYER.OBJ functions the GameCube target does not contain, but a
name alone is not a slot.

THE OBSERVATION (lane P7, `build/p7_lane/p7_order.py`, promoted here). The GC
emission order is NOT the Xbox order and it is not a permutation without
structure: it is the Xbox order re-read in DESCENDING RUNS. Printing each GC
function's Xbox index in GC emission order on `game/game/player` gives

    gc#7..gc#20 = xbox 108,107,[124],106,104,103,[123],100,96,95,94,93,91,90

— a long descent with two out-of-run interlopers. 67 of the 89 paired
adjacent steps on that unit are descents, 22 are ascents (this tool's STEP
CENSUS line). Once the local orientation is known, an ABSENT Xbox index is
bounded: its two nearest PRESENT Xbox neighbours occupy two GC slots, and
the absent name must emit between them.

THE DISCRIMINANT IS LOCAL, NOT GLOBAL. Orientation is decided per absent
name from its own two neighbours (`gc[hi] < gc[lo]` = descending there),
never from a whole-file verdict, because the interlopers prove the file is
not uniformly descending. A bound is reported with its WIDTH — the number of
GC slots it spans — and a width-1 bound ("strictly between two ADJACENT GC
functions") is the only kind that pins a slot on its own. Wider bounds are
printed with the GC functions they still contain, so a second, independent
constraint (`poolslot.py`'s creation-order bracket) can intersect them.

WHAT IT IS NOT. Positional Xbox correspondence is not proof of identity;
AGENTS.md says so for renames and it is just as true here. This tool reports
where a name COULD sit given two orders. It cannot prove the GC file had that
function at all, and a GC-only name (`create_player_blits`,
`do_got_it_8007FC80` on player) proves the two files are not the same source.

LIVE, measured in W:/Repositories/GDL-Claude-P5 at 9c4412023 with

    python tools/gdl/composed_census/srcorder.py game/game/player --quiet

    92 GC .text functions, 137 module code rows, 90 paired by name
    STEP CENSUS over 89 consecutive paired steps: 67 down, 22 up, 0 flat
    ...
    101  power_bar_state    0x66  G  descending  after gc#12 draw_power_meter,
                            before gc#14 get_display_mode  [width 2]
                            inside: setup_player_display
    102  hide_power_meter   0x28  L  descending  (the same bound)
    -- GC-ONLY names --
    gc#62   create_player_blits
    gc#82   do_got_it_8007FC80

24 of that unit's 47 absent names come back PINNED (width 1), which is the
yield that makes the screen worth running.

Note what the bound is and is NOT. `poolslot.py` brackets the same pair
between setup_player_display and AddExp from the `.sdata2` creation order;
that is a DIFFERENT constraint, and the two intersect on the one slot
strictly between setup_player_display (gc#13) and get_display_mode (gc#14).
Quoting either bound as if it were the other overstates one and understates
the other.

CONTROL, an EXACT unit: `game/mb/mb_tree` (Matching, so its emission order is
already proven right by the link) reads 25 GC functions, 31 module code rows,
25 paired, 19 down / 5 up, no GC-only name, and bounds its 6 absent module
names the same way — `MBNodeSetEmpty` xb#6 after gc#16 MBNodeSetParent and
before gc#19 MBNewNode. The control's value is that the structure is not an
artefact of a broken unit: an exact TU shows the same descending runs.

REFUSALS (exit 2), because a wrong module silently produces a whole table of
confident nonsense: no module whose basename matches the unit; more than one
such module (name the one you mean with `--module`); a module that shares NO
function name with the GC object; a missing target object.

IMPORTABLE CORE: parse_modules, module_index, choose_module, gc_order,
align_orders, monotone_runs, step_census, bound_absent, gc_only, survey --
pure over paths already on disk, no build, no printing.
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))

import cliscreen                                          # noqa: E402
import fndiff                                             # noqa: E402
import textorder                                          # noqa: E402

OK, REFUSED = 0, 2
VERSION = "GUNE5D"
MODULES_TXT = os.path.join(ROOT, "research", "xbox_symbols",
                           "functions_by_module.txt")

_HEADER = re.compile(r"^==\s+(.*?)(?:\s+\(.*\))?\s*$")
_ROW = re.compile(r"^\[(\d{4}):([0-9A-Fa-f]{8})\]\s+([0-9A-Fa-f]+)\s+([GLD])\s+(.*)$")


class Refused(Exception):
    """A stated precondition of the measurement is not met."""


# ---------------------------------------------------------------- IMPORTABLE


def parse_modules(path=MODULES_TXT):
    """{module: [(segment, offset, size, kind, name)]} in FILE order.

    Kind is `G`/`L` for code and `D` for data; both are kept because a
    caller screening a module for ownership wants the data rows too. The
    file order is preserved rather than sorted: `module_index` sorts, and
    reports whether sorting moved anything.
    """
    if not os.path.exists(path):
        raise Refused("no %s to read the Xbox module rosters from"
                      % os.path.relpath(path, ROOT).replace("\\", "/"))
    modules, current = {}, None
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if line.startswith("== "):
                match = _HEADER.match(line.rstrip("\n"))
                current = match.group(1).strip() if match else None
                if current is not None:
                    modules.setdefault(current, [])
                continue
            if current is None:
                continue
            match = _ROW.match(line.strip())
            if match:
                modules[current].append((
                    int(match.group(1)), int(match.group(2), 16),
                    int(match.group(3), 16), match.group(4),
                    match.group(5).strip()))
    return modules


def module_index(rows):
    """(code_rows, name -> index, resorted) for one module's rows.

    CODE ONLY, sorted by (segment, offset) — the module's link order, which
    for the Xbox compiler is its source order. `resorted` is True when that
    sort moved a row relative to the file, which is the honest signal that
    the file was not already in that order and the indices below are the
    tool's ordering, not the file's.
    """
    code = [row for row in rows if row[3] in ("G", "L")]
    ordered = sorted(code, key=lambda row: (row[0], row[1]))
    by_name = {}
    for index, row in enumerate(ordered):
        by_name.setdefault(row[4], index)
    return ordered, by_name, ordered != code


def _sample(names, cap=12):
    """A refusal names the choices, but a 434-name refusal is unreadable."""
    ordered = sorted(names)
    if len(ordered) <= cap:
        return ", ".join(ordered)
    return "%s ... and %d more" % (", ".join(ordered[:cap]),
                                   len(ordered) - cap)


def choose_module(modules, unit, explicit=None):
    """(module_name, why). Raises Refused when the choice is not forced.

    Without `--module` the only accepted evidence is the unit BASENAME
    against the module file's basename, case-insensitively and without its
    extension. Two modules with that basename is a refusal, not a coin
    flip: `.OBJ` and `.obj` rosters coexist in this file and choosing the
    wrong one produces a full table of confident nonsense.
    """
    def basename(name):
        return re.split(r"[\\/]", name)[-1].rsplit(".", 1)[0].lower()

    if explicit:
        exact = [name for name in modules if name == explicit]
        if exact:
            return exact[0], "named exactly on the command line"
        wanted = explicit.lower()
        hits = [name for name in modules
                if basename(name) == basename(explicit)
                or wanted in name.lower()]
        if len(hits) == 1:
            return hits[0], "named on the command line as %r" % explicit
        if not hits:
            raise Refused("no module matches --module %r" % explicit)
        raise Refused("--module %r matches %d modules: %s"
                      % (explicit, len(hits), _sample(hits)))
    want = unit.split("/")[-1].lower()
    hits = [name for name in modules if basename(name) == want]
    if not hits:
        raise Refused(
            "no Xbox module is named %r; this unit has no PDB counterpart to"
            " order against, or it is spelled differently there — pass"
            " --module <name> if you know it" % want)
    if len(hits) > 1:
        raise Refused("%d modules are named %r: %s — pass --module <name>"
                      % (len(hits), want, _sample(hits)))
    return hits[0], "the only module whose basename is %r" % want


def gc_order(objfile):
    """[name] in GC .text emission order, from textorder's symbol reader."""
    return [name for _offset, _size, name in textorder.text_symbols(objfile)]


def align_orders(gc_names, by_name):
    """[(gc_index, name, xbox_index|None)] in GC emission order."""
    return [(index, name, by_name.get(name))
            for index, name in enumerate(gc_names)]


def monotone_runs(rows, minimum=2):
    """Maximal strictly-monotone runs over the PAIRED rows, in GC order.

    A run is [(gc_index, name, xbox_index)] whose Xbox indices strictly
    decrease (or strictly increase) at every consecutive PAIRED step.
    Unpaired GC functions are skipped, not broken on: a name the module
    does not carry says nothing about the order of the ones it does.
    """
    paired = [row for row in rows if row[2] is not None]
    runs, current, direction = [], [], 0
    for row in paired:
        if not current:
            current, direction = [row], 0
            continue
        step = row[2] - current[-1][2]
        this = 1 if step > 0 else -1 if step < 0 else 0
        if this and (direction == 0 or direction == this):
            current.append(row)
            direction = this
            continue
        if len(current) >= minimum:
            runs.append((direction, current))
        current, direction = [row], 0
    if len(current) >= minimum:
        runs.append((direction, current))
    return runs


def step_census(rows):
    """{'down','up','flat','pairs'} over consecutive PAIRED Xbox indices."""
    paired = [row[2] for row in rows if row[2] is not None]
    census = {"pairs": max(0, len(paired) - 1), "down": 0, "up": 0, "flat": 0}
    for before, after in zip(paired, paired[1:]):
        if after < before:
            census["down"] += 1
        elif after > before:
            census["up"] += 1
        else:
            census["flat"] += 1
    return census


def bound_absent(code_rows, by_name, gc_names):
    """[record] for every module function ABSENT from the GC object.

    Each record carries the two nearest PRESENT Xbox neighbours, the GC
    slots they occupy, the LOCAL orientation those two slots imply, the
    bounding pair in GC order, the bound's WIDTH in GC slots and the GC
    functions strictly inside it. A one-sided bound (the absent name is
    past the last present neighbour on one side) is reported as such and
    never widened into a two-sided claim.
    """
    present = set(gc_names)
    slot = {}
    for index, name in enumerate(gc_names):
        xbox = by_name.get(name)
        if xbox is not None:
            slot.setdefault(xbox, index)
    records = []
    for index, row in enumerate(code_rows):
        _segment, _offset, size, kind, name = row
        if name in present:
            continue
        low = high = None
        for step in range(index - 1, -1, -1):
            if step in slot:
                low = step
                break
        for step in range(index + 1, len(code_rows)):
            if step in slot:
                high = step
                break
        record = {"xbox_index": index, "name": name, "size": size,
                  "binding": kind,
                  "lower_neighbour": None if low is None else
                  {"xbox_index": low, "name": code_rows[low][4],
                   "gc_index": slot[low]},
                  "higher_neighbour": None if high is None else
                  {"xbox_index": high, "name": code_rows[high][4],
                   "gc_index": slot[high]}}
        if low is None or high is None:
            record.update(orientation="one-sided", bound=None,
                          width=None, inside=[])
            records.append(record)
            continue
        low_slot, high_slot = slot[low], slot[high]
        descending = high_slot < low_slot
        first, last = ((high_slot, low_slot) if descending
                       else (low_slot, high_slot))
        record.update(
            orientation="descending" if descending else "ascending",
            bound={"after_gc_index": first, "after": gc_names[first],
                   "before_gc_index": last, "before": gc_names[last]},
            width=last - first,
            inside=[gc_names[step] for step in range(first + 1, last)])
        records.append(record)
    return records


def gc_only(gc_names, by_name):
    """[(gc_index, name)] for GC functions the module does not carry."""
    return [(index, name) for index, name in enumerate(gc_names)
            if name not in by_name]


def survey(unit, module=None, modules_path=MODULES_TXT, objfile=None):
    """The whole record for one unit. Pure of printing; raises Refused."""
    objfile = objfile or os.path.join(ROOT, "build", VERSION, "obj",
                                      unit + ".o")
    if not os.path.exists(objfile):
        raise Refused("no target object at %s — this unit is not split, or"
                      " the tree is not built"
                      % os.path.relpath(objfile, ROOT).replace("\\", "/"))
    modules = parse_modules(modules_path)
    name, why = choose_module(modules, unit, module)
    code_rows, by_name, resorted = module_index(modules[name])
    gc_names = gc_order(objfile)
    if not gc_names:
        raise Refused("%s defines no .text function symbols"
                      % os.path.relpath(objfile, ROOT).replace("\\", "/"))
    rows = align_orders(gc_names, by_name)
    paired = [row for row in rows if row[2] is not None]
    if not paired:
        raise Refused(
            "module %r shares NO function name with %s (%d GC names, %d"
            " module names) — that is not this unit's module"
            % (name, os.path.relpath(objfile, ROOT).replace("\\", "/"),
               len(gc_names), len(code_rows)))
    return {
        "unit": unit, "object": objfile, "module": name, "module_why": why,
        "module_resorted": resorted,
        "module_functions": len(code_rows), "gc_functions": len(gc_names),
        "paired": len(paired), "rows": rows,
        "runs": [{"direction": "descending" if direction < 0 else "ascending",
                  "length": len(run),
                  "gc_first": run[0][0], "gc_last": run[-1][0],
                  "xbox_first": run[0][2], "xbox_last": run[-1][2],
                  "names": [row[1] for row in run]}
                 for direction, run in monotone_runs(rows)],
        "steps": step_census(rows),
        "absent": bound_absent(code_rows, by_name, gc_names),
        "gc_only": gc_only(gc_names, by_name),
    }


# ------------------------------------------------------------------- RENDER


def render(record, quiet=False):
    print("== %s vs Xbox module %s" % (record["unit"], record["module"]))
    print("   chosen: %s" % record["module_why"])
    print("   %d GC .text functions, %d module code rows, %d paired by name"
          % (record["gc_functions"], record["module_functions"],
             record["paired"]))
    if record["module_resorted"]:
        print("   NOTE the module rows were NOT in (segment, offset) order in"
              " the file; the indices below are this tool's sort")
    steps = record["steps"]
    print("   STEP CENSUS over %d consecutive paired steps: %d down, %d up,"
          " %d flat" % (steps["pairs"], steps["down"], steps["up"],
                        steps["flat"]))
    if not quiet:
        print()
        print("   -- GC emission order vs Xbox index --")
        print("   %-4s %-46s %-6s %s" % ("gc#", "function", "xbox#", "step"))
        previous = None
        for index, name, xbox in record["rows"]:
            if xbox is None:
                note = "no module counterpart"
            elif previous is None:
                note = ""
            else:
                note = "%+d" % (xbox - previous)
            print("   %-4d %-46s %-6s %s"
                  % (index, name, "-" if xbox is None else xbox, note))
            if xbox is not None:
                previous = xbox
        print()
        print("   -- monotone runs over the paired rows (length >= 2) --")
        for run in record["runs"]:
            print("   %-11s gc#%-4d..gc#%-4d  xbox %d..%d  (%d functions)"
                  % (run["direction"], run["gc_first"], run["gc_last"],
                     run["xbox_first"], run["xbox_last"], run["length"]))
    print()
    print("   -- module names ABSENT from the GC object, with their bounded"
          " GC slot --")
    print("   %-4s %-34s %-7s %-4s %-11s %s"
          % ("xb#", "name", "size", "bind", "orientation", "bounded GC slot"))
    for row in record["absent"]:
        if row["bound"] is None:
            low, high = row["lower_neighbour"], row["higher_neighbour"]
            edge = ("no present module neighbour below"
                    if low is None else
                    "no present module neighbour above")
            known = high or low
            detail = "%s (only side: %s at gc#%d)" % (
                edge, known["name"], known["gc_index"]) if known else edge
        else:
            bound = row["bound"]
            detail = ("after gc#%d %s, before gc#%d %s  [width %d]"
                      % (bound["after_gc_index"], bound["after"],
                         bound["before_gc_index"], bound["before"],
                         row["width"]))
            if row["width"] == 1:
                detail += "  PINNED (adjacent GC slots)"
            elif row["inside"]:
                detail += "  inside: " + ", ".join(row["inside"])
        print("   %-4d %-34s 0x%-5X %-4s %-11s %s"
              % (row["xbox_index"], row["name"], row["size"], row["binding"],
                 row["orientation"], detail))
    if not record["absent"]:
        print("   (none — every module function is present in the GC object)")
    print()
    only = record["gc_only"]
    print("   -- GC-ONLY names (the module does not carry them) --")
    if only:
        for index, name in only:
            print("   gc#%-4d %s" % (index, name))
    else:
        print("   (none)")
    print()
    print("   A BOUND IS NOT AN IDENTITY. This says where a name COULD emit"
          " given two orders; it does not prove the GC source contained that"
          " function, and every GC-only name above is direct evidence that"
          " the two files are not the same source.")


def main(argv=None):
    cliscreen.help_only(__doc__)
    parser = argparse.ArgumentParser(
        prog="srcorder.py",
        description="Bound PDB-absent functions to GC source slots.")
    parser.add_argument("unit")
    parser.add_argument("--module", help="Xbox module name (or a unique"
                                         " substring of one)")
    parser.add_argument("--object", help="read this target object instead of"
                                         " build/GUNE5D/obj/<unit>.o")
    parser.add_argument("--quiet", action="store_true",
                        help="skip the per-function order table and the runs")
    parser.add_argument("--out", help="write the record as JSON (under build/)")
    args = parser.parse_args(argv)
    unit = fndiff.unit_key(args.unit)
    try:
        record = survey(unit, args.module, objfile=args.object)
    except Refused as refusal:
        print("srcorder REFUSED: %s" % refusal)
        return REFUSED
    except (OSError, fndiff.ObjdumpFailed) as error:
        print("srcorder REFUSED: %s" % error)
        return REFUSED
    render(record, quiet=args.quiet)
    if args.out:
        out = os.path.join(ROOT, args.out)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w", encoding="utf-8") as handle:
            json.dump(record, handle, indent=2, sort_keys=True)
        print("\nwrote %s" % args.out)
    return OK


if __name__ == "__main__":
    sys.exit(main())
