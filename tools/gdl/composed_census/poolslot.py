#!/usr/bin/env python3
"""Name the SOURCE SLOT of the function that created a pool gap's datums.

    python tools/gdl/composed_census/poolslot.py game/game/player .sdata2
    python tools/gdl/composed_census/poolslot.py <unit> .rodata 0x80113AE0
    python tools/gdl/composed_census/poolslot.py <unit> .data --module X.OBJ
    python tools/gdl/composed_census/poolslot.py <unit> .sdata2 --out build/x.json

WHAT IT ANSWERS. `secbind.py` reports that our object binds the DOL with a
0x10-byte DOL-SIDE GAP: the target has two datums we do not emit. That says
WHAT is missing, never WHO made it. MWCC creates a pool entry at the datum's
FIRST USE in source order, so a gap is a slot: whatever created those datums
sat, in the source, between the creators of the datums on either side of it.
This screen turns a gap into that bracket, then into a CANDIDATE LIST from
`srcorder.py`'s PDB-absent names, then classifies which discarded-helper
device the gap belongs to.

THE THREE MEASUREMENTS, and why each is separate.

1. THE BRACKET. Every target datum in the run is attributed to the FIRST
   target function (in GC emission order) whose relocations name it. The
   datum immediately below the gap and the one immediately above it give two
   GC functions; the creator emitted between them. The bracket is only sound
   where the surviving pool order is MONOTONE across the gap, so the screen
   checks that and says so — a datum whose own first surviving referrer is
   far out of order is itself evidence of another discarded creator, not a
   bracket you may quote.

2. THE REFERRER TEST, which is the device discriminant:

     a surviving body INSIDE the bracket references the gap's datums
         -> the creator was INLINED into it (the StartCompass class): there
            is a live shape to read, and the datums have a reader.
     referrers exist but every one is OUTSIDE the bracket
         -> the creator was compiled, OUTLINED and DEAD-STRIPPED (the
            any_player_walking class). The datums survive because later
            functions share them; there is no inlined shape to read.
     no referrer at all
         -> dead datums of a discarded, never-called function.

   The middle case is the one a summary loses. On player's `.sdata2` gap the
   two datums have TEN referrers between them; what makes it the
   any_player_walking class is that NONE of the ten is inside the bracket.
   "No referrer" and "no referrer in the bracket" are different facts and
   only the second one is true here.

3. THE CANDIDATES. `srcorder.py` bounds each PDB-absent module name to a
   range of GC slots. A name is a candidate when that range INTERSECTS the
   bracket. The intersection is reported per name; a one-sided (unbounded)
   absent name cannot be screened and is counted, never silently dropped.

LIVE, measured in W:/Repositories/GDL-Claude-P5 at a386f2d48 with

    python tools/gdl/composed_census/poolslot.py game/game/player .sdata2

    target run 0x80347608..0x80347C6C: 234 symbols.txt datums, 129
    attributed to a first-referencing GC function

    GAP 0x80347828..0x80347838 (0x10 B) after our +0x0220
      lbl_80347828  f64 0.001 / "?PbM"  referrers: PlayerUpdateAtts (gc#53),
                                        PlayerProcessPowerups (gc#67)
      lbl_80347830  f64 500             referrers: do_heal_players (gc#29),
                                        heal_player, player_max_health,
                                        inactivate_player + 4 more
      BRACKET  after gc#13 setup_player_display (lbl_80347820),
               before gc#15 AddExp (lbl_80347838)
      CLASS    discarded creator, datums SHARED by later bodies — the
               any_player_walking class; 10 referrers, 0 inside the bracket
      CANDIDATES  IncLevel xb#97, SetPlayerLevel xb#98, IncAtt xb#99
                  (bound gc#14..gc#15), power_bar_state xb#101,
                  hide_power_meter xb#102 (bound gc#12..gc#14),
                  load_player_atts xb#115 (bound gc#0..gc#50)

The other two gaps on that run come back UNBRACKETED and say so: the 0.707
at 0x80347880 sits between two datums start_magic (gc#20) also first-uses,
so the surviving order cannot separate them, and the 0x228 trailing gap at
0x80347938 has no attributed datum above it.

CALIBRATION NOTE, and it widens a narrower claim. Lane P7 recorded this
gap's creator as "power_bar_state / hide_power_meter". Those two ARE
candidates, but so are IncLevel, SetPlayerLevel and IncAtt: get_display_mode
(gc#14) first-uses NO `.sdata2` datum, so the pool cannot separate the slot
before it from the slot after it, and five absent names have bounds that
intersect (six with load_player_atts, whose 50-slot bound intersects almost
anything — which is why the list is sorted tightest-first). The gap's values
are evidence a reader should weigh separately: `500.0` is read by
player_max_health, PlayersRestoreHealth and six more health/attribute
bodies. This tool reports the candidates, not a favourite.

WHAT IT IS NOT. A bracket is not an identity, a candidate is not a
recovered function, and none of this proves the GC source contained the
Xbox name at all. It also cannot see a creator with no PDB counterpart:
player's `.bss` seater has none (see `bssorder.py`).

EXIT 0 when the gaps were measured, 2 when the binding could not be made.

IMPORTABLE CORE: reloc_targets, datum_first_reference, run_datums,
bracket_gap, classify_gap, candidates_for, survey -- pure over objects and
the DOL, no build, no printing.
"""
import argparse
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, HERE)

import claimable_sections as cs                           # noqa: E402
import cliscreen                                          # noqa: E402
import fndiff                                             # noqa: E402
import textorder                                          # noqa: E402

import secbind                                            # noqa: E402
import srcorder                                           # noqa: E402

OK, REFUSED = 0, 2
VERSION = "GUNE5D"
DATUM_ROWS = 12          # a 77-datum trailing gap is a page, not a reading


class Refused(Exception):
    """A stated precondition of the measurement is not met."""


# ---------------------------------------------------------------- IMPORTABLE


def reloc_targets(lines):
    """[(symbol, addend)] for the relocation rows of one parsed function.

    `fndiff.parse` indents relocation rows by four spaces and spells the
    payload `R_PPC_<type>\\t<symbol>[+0xNN]`; `fndiff.split_addend` owns the
    addend half so a `sym+0x4` row resolves to the address it names rather
    than to the symbol's start.
    """
    rows = []
    for _kind, payload in fndiff.reloc_rows_from_lines(lines):
        symbol = payload.split()[0] if payload else ""
        if symbol:
            rows.append(fndiff.split_addend(symbol))
    return rows


def datum_first_reference(objfile, low, high):
    """{address: (gc_index, function)} over the target's own emission order.

    Only addresses inside [low, high) are kept. A symbol the table does not
    resolve is skipped and counted by the caller, never guessed at.
    """
    order = [name for _offset, _size, name in textorder.text_symbols(objfile)]
    table = fndiff.parse(objfile)
    symbols = fndiff.symbol_table()
    first, unresolved = {}, set()
    for index, function in enumerate(order):
        for symbol, addend in reloc_targets(table.get(function, [])):
            entry = symbols.get(symbol)
            if entry is None:
                address = fndiff.placeholder_address(symbol)
                if address is None:
                    unresolved.add(symbol)
                    continue
            else:
                address = entry[1]
            address += addend
            if low <= address < high and address not in first:
                first[address] = (index, function)
    return first, unresolved


def run_datums(section, low, high):
    """[(address, name, size)] for symbols.txt datums in [low, high)."""
    rows = []
    for name, (where, address, size) in fndiff.symbol_table().items():
        if where == section and low <= address < high:
            rows.append((address, name, size))
    return sorted(rows)


def datum_note(address, size, cap=32):
    """What the datum's DOL bytes ARE, for the eye.

    `claimable_sections.describe_bytes` is the project's renderer and does
    the string/float choice, but it PREFERS a printable run of four bytes,
    and a float can be one: the gap datum at 0x80347828 is `0.001`, whose
    big-endian bytes begin `3f 50 62 4d` = "?PbM", and describe_bytes calls
    it a string. So a 4- or 8-byte datum is reported numerically here, with
    the printable reading kept ALONGSIDE it when there is one — both facts,
    neither hidden behind the other. Everything else is describe_bytes'.
    """
    if not size:
        return ""
    raw = fndiff.dol_read(address, min(size, cap))
    if not raw:
        return ""
    described = cs.describe_bytes(raw)
    if size in (4, 8) and len(raw) == size:
        value = struct.unpack(">f" if size == 4 else ">d", raw)[0]
        numeric = "f%d %g" % (size * 8, value)
        return ("%s / %s" % (numeric, described)
                if described.startswith('"') else numeric)
    return described


def bracket_gap(gap, datums, first):
    """The creation-order bracket around one gap, and its soundness.

    Returns a dict with the datum immediately BELOW the gap and the one
    immediately AT OR ABOVE its end, each with the GC function that first
    references it, plus `monotone`: False when the two are already out of
    order, which means the surviving pool order cannot support a bracket
    here at all.
    """
    start = gap["address"]
    end = start + gap["size"]
    below = above = None
    for address, name, size in datums:
        if address < start and address in first:
            below = (address, name, first[address])
        if address >= end and address in first and above is None:
            above = (address, name, first[address])
    record = {
        "below": None if below is None else
        {"address": below[0], "name": below[1],
         "gc_index": below[2][0], "function": below[2][1]},
        "after": None if above is None else
        {"address": above[0], "name": above[1],
         "gc_index": above[2][0], "function": above[2][1]},
    }
    if below is None or above is None:
        record["monotone"] = None
        record["interval"] = None
        return record
    low, high = below[2][0], above[2][0]
    record["monotone"] = low < high
    record["interval"] = (low, high) if low < high else None
    return record


def classify_gap(gap, datums, referrers, bracket):
    """The device class for one gap, from its referrers and its bracket."""
    inside_names = [name for address, name, _size in datums
                    if gap["address"] <= address < gap["address"] + gap["size"]]
    hits, per_datum = [], {}
    for name in inside_names:
        rows = referrers.get(name, [])
        per_datum[name] = rows
        hits.extend(rows)
    interval = bracket.get("interval")
    within = [] if interval is None else [
        row for row in hits if interval[0] < row[0] < interval[1]]
    if interval is None:
        verdict = ("UNBRACKETED — the surviving pool order does not bracket"
                   " this gap, so no slot can be inferred from it")
    elif within:
        verdict = ("INLINED into a surviving body — the StartCompass class;"
                   " there is a live shape to read")
    elif hits:
        verdict = ("discarded creator, datums SHARED by later bodies — the"
                   " any_player_walking class (compiled, outlined,"
                   " dead-stripped); no inlined shape to read")
    else:
        verdict = ("NO REFERRER anywhere in the target text — dead datums of"
                   " a discarded, never-called function")
    return {"datums": inside_names, "referrers": hits,
            "per_datum": per_datum, "referrers_in_bracket": within,
            "verdict": verdict}


def candidates_for(absent, interval):
    """[record] for absent names whose bound intersects `interval`.

    A bound and a bracket are both OPEN intervals of GC slots: "strictly
    after slot a and strictly before slot b". They intersect when
    max(a, c) < min(b, d). A one-sided absent name has no interval and is
    reported as UNSCREENED rather than admitted or dropped.
    """
    admitted, unscreened = [], []
    if interval is None:
        return admitted, [row["name"] for row in absent]
    low, high = interval
    for row in absent:
        bound = row.get("bound")
        if bound is None:
            unscreened.append(row["name"])
            continue
        first = max(bound["after_gc_index"], low)
        last = min(bound["before_gc_index"], high)
        if first < last:
            admitted.append({
                "name": row["name"], "xbox_index": row["xbox_index"],
                "size": row["size"], "binding": row["binding"],
                "bound": [bound["after_gc_index"], bound["before_gc_index"]],
                "intersection": [first, last]})
    # TIGHTEST FIRST. A name bounded across 50 GC slots intersects almost any
    # bracket and is a true but nearly contentless candidate; sorting by the
    # intersection's width and then the bound's keeps it in the list without
    # letting it head it.
    admitted.sort(key=lambda row: (row["intersection"][1]
                                   - row["intersection"][0],
                                   row["bound"][1] - row["bound"][0],
                                   row["xbox_index"]))
    return admitted, unscreened


def survey(unit, section, base=None, object_path=None, module=None):
    """The whole record for one (unit, section). Pure of printing."""
    try:
        binding = secbind.bind(unit, section, base, object_path)
    except SystemExit as refusal:
        if isinstance(refusal.code, int):
            raise Refused("secbind refused with exit code %r" % refusal.code)
        raise Refused(str(refusal))
    except (OSError, fndiff.ObjdumpFailed, cs.Unmeasurable) as error:
        raise Refused(str(error))

    objfile = os.path.join(ROOT, "build", VERSION, "obj", unit + ".o")
    if not os.path.exists(objfile):
        raise Refused("no target object at %s"
                      % os.path.relpath(objfile, ROOT).replace("\\", "/"))

    low = binding["base"]
    high = max(binding["base"] + binding["size"],
               binding["inventory"].get("tail_address", 0))
    for gap in binding["inventory"]["gaps"]:
        high = max(high, gap["address"] + gap["size"])
    first, unresolved = datum_first_reference(objfile, low, high)
    datums = run_datums(section, low, high)

    order = [name for _offset, _size, name in textorder.text_symbols(objfile)]
    table = fndiff.parse(objfile)
    symbols = fndiff.symbol_table()
    referrers = {}
    for index, function in enumerate(order):
        named = set()
        for symbol, _addend in reloc_targets(table.get(function, [])):
            entry = symbols.get(symbol)
            if entry and entry[0] == section and low <= entry[1] < high:
                named.add(symbol)
        for symbol in named:
            referrers.setdefault(symbol, []).append((index, function))

    absent, module_note = [], None
    try:
        order_record = srcorder.survey(unit, module)
        absent = order_record["absent"]
        module_note = order_record["module"]
    except srcorder.Refused as refusal:
        module_note = "UNAVAILABLE: %s" % refusal

    gaps = []
    for gap in binding["inventory"]["gaps"]:
        bracket = bracket_gap(gap, datums, first)
        device = classify_gap(gap, datums, referrers, bracket)
        admitted, unscreened = candidates_for(absent, bracket.get("interval"))
        gaps.append({
            "address": gap["address"], "size": gap["size"],
            "our_offset": gap["our_offset"], "content": gap["content"],
            "values": [(name, datum_note(address, size))
                       for address, name, size in datums
                       if gap["address"] <= address
                       < gap["address"] + gap["size"]],
            "bracket": bracket, "device": device,
            "candidates": admitted, "unscreened": unscreened})
    return {"unit": unit, "section": section, "binding": binding,
            "module": module_note, "run": [low, high],
            "datums": len(datums), "attributed": len(first),
            "unresolved_symbols": sorted(unresolved), "gaps": gaps}


# ------------------------------------------------------------------- RENDER


def render(record):
    binding = record["binding"]
    print("== %s %s: 0x%X byte(s) bound at 0x%08X — %s"
          % (record["unit"], record["section"], binding["size"],
             binding["base"], binding["base_source"]))
    print("   target run 0x%08X..0x%08X: %d symbols.txt datums, %d attributed"
          " to a first-referencing GC function"
          % (record["run"][0], record["run"][1], record["datums"],
             record["attributed"]))
    print("   Xbox module for the candidate screen: %s" % record["module"])
    if not record["gaps"]:
        print("   NO DOL-SIDE GAP — our bytes cover this run; there is no"
              " missing creator for this screen to slot.")
        return
    for gap in record["gaps"]:
        print()
        print("   GAP 0x%08X..0x%08X (0x%X B) after our +0x%04X   %s"
              % (gap["address"], gap["address"] + gap["size"], gap["size"],
                 gap["our_offset"], gap["content"]))
        for name, note in gap["values"][:DATUM_ROWS]:
            rows = gap["device"]["per_datum"].get(name, [])
            if rows:
                shown = ", ".join("%s (gc#%d)" % (function, index)
                                  for index, function in rows[:4])
                if len(rows) > 4:
                    shown += " + %d more" % (len(rows) - 4)
                detail = "referrers: " + shown
            else:
                detail = "NO REFERRER"
            print("      %-20s %-30s %s" % (name, note[:30], detail))
        if len(gap["values"]) > DATUM_ROWS:
            print("      ... and %d more datum(s) in this gap"
                  % (len(gap["values"]) - DATUM_ROWS))
        bracket = gap["bracket"]
        below, after = bracket["below"], bracket["after"]
        if bracket["interval"] is None:
            print("      BRACKET  NONE — %s"
                  % ("no attributed datum below the gap" if below is None
                     else "no attributed datum above the gap" if after is None
                     else "the surviving pool order is NOT monotone here"
                          " (%s gc#%d then %s gc#%d)"
                          % (below["name"], below["gc_index"],
                             after["name"], after["gc_index"])))
        else:
            print("      BRACKET  after gc#%d %s (%s), before gc#%d %s (%s)"
                  % (below["gc_index"], below["function"], below["name"],
                     after["gc_index"], after["function"], after["name"]))
        device = gap["device"]
        print("      CLASS    %s" % device["verdict"])
        print("               %d referrer(s) over %d datum(s), %d inside the"
              " bracket" % (len(device["referrers"]), len(device["datums"]),
                            len(device["referrers_in_bracket"])))
        for index, function in device["referrers_in_bracket"]:
            print("               INSIDE: %s (gc#%d)" % (function, index))
        if gap["candidates"]:
            print("      CANDIDATES whose srcorder bound intersects the"
                  " bracket:")
            for row in gap["candidates"]:
                print("         %-34s xb#%-4d 0x%-5X %-2s bound gc#%d..gc#%d"
                      "  intersection gc#%d..gc#%d"
                      % (row["name"], row["xbox_index"], row["size"],
                         row["binding"], row["bound"][0], row["bound"][1],
                         row["intersection"][0], row["intersection"][1]))
        elif gap["bracket"]["interval"] is not None:
            print("      CANDIDATES none — no PDB-absent name's bound"
                  " intersects this bracket, so the creator is GC-only or"
                  " the module is wrong")
        if gap["unscreened"]:
            print("      %d absent name(s) have a one-sided bound and could"
                  " not be screened" % len(gap["unscreened"]))
    if record["unresolved_symbols"]:
        print()
        print("   %d relocation symbol(s) did not resolve to an address and"
              " were skipped: %s"
              % (len(record["unresolved_symbols"]),
                 ", ".join(record["unresolved_symbols"][:8])))
    print()
    print("   A BRACKET IS NOT AN IDENTITY and a candidate is not a recovered"
          " function. This narrows where the creator sat; the datum values,"
          " their readers and the target text decide which name it was.")


def main(argv=None):
    cliscreen.help_only(__doc__)
    parser = argparse.ArgumentParser(
        prog="poolslot.py",
        description="Infer the source slot of a pool gap's creator.")
    parser.add_argument("unit")
    parser.add_argument("section")
    parser.add_argument("address", nargs="?",
                        help="hex base to bind at; omit for secbind's")
    parser.add_argument("--module", help="Xbox module for the candidate list")
    parser.add_argument("--object", help="bind this object instead of"
                                         " build/GUNE5D/src/<unit>.o")
    parser.add_argument("--out", help="write the record as JSON (under build/)")
    args = parser.parse_args(argv)
    unit = fndiff.unit_key(args.unit)
    try:
        base = int(args.address, 16) if args.address else None
    except ValueError:
        print("poolslot: %r is not a hex address" % args.address)
        return REFUSED
    try:
        record = survey(unit, args.section, base, args.object, args.module)
    except Refused as refusal:
        print("poolslot REFUSED: %s" % refusal)
        return REFUSED
    except (OSError, fndiff.ObjdumpFailed) as error:
        print("poolslot REFUSED: %s" % error)
        return REFUSED
    render(record)
    if args.out:
        out = os.path.join(ROOT, args.out)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w", encoding="utf-8") as handle:
            json.dump(record, handle, indent=2, sort_keys=True, default=str)
        print("\nwrote %s" % args.out)
    return OK


if __name__ == "__main__":
    sys.exit(main())
