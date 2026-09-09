#!/usr/bin/env python3
"""Why the target seats a BSS object where our compiler does not.

    python tools/gdl/composed_census/bssorder.py game/game/player
    python tools/gdl/composed_census/bssorder.py <unit> --section .sbss \\
        --run 0x80344AFC..0x80344B18
    python tools/gdl/composed_census/bssorder.py <unit> --out build/x.json

WHAT IT ANSWERS. A `.bss` run carries no bytes, so no diff sees it and no
score moves when it is wrong; the only evidence is ORDER. The emission law
lane P3 measured two-sided on player.c is:

    MWCC emits .bss FILE STATICS in DECLARATION order, then .bss EXTERNAL
    tentative definitions in ascending order of FIRST CODE REFERENCE.
    Declaration order among globals has no effect; an initialiser moves the
    object to .data; linkage does not affect small-data placement.

So a target layout that puts an EXTERNAL in the middle of the statics is not
a layout our source can reach by reordering declarations. It is a SEAT, and
a seat means a function that referenced that object first and is no longer in
the text. This screen prints the four orders that decide it and says which
law the target's order actually follows.

THE FOUR BLOCKS.

  OURS      our object's section symbols with their STATIC/EXTERNAL binding,
            in address order — which for the statics IS declaration order.
  TARGET    the target object's own section symbols and every symbols.txt
            datum inside the run: the independently known offsets.
  SEATED    the hypothesis: our statics in declaration order with each
            external moved to its known target offset. It is VALIDATED
            against every target symbol it did not seat by construction; an
            unvalidated seating is printed as a hypothesis and never used.
  FIRST-REF the target-side first-reference order, one row per object.

THE TRACKER, and why it is not lane P7's. `p7_bssref2.py` counted any
`addi rD,rS,IMM` or `op rT,IMM(rS)` whose IMM equalled a known object offset,
with rS merely not r1/r2/r13, and flagged three small offsets as possible
false positives. That rule is too loose in both directions. MEASURED on
player `.bss` at 1bf4aabbf, the two trackers differ on five of sixteen rows:

  * PlayerUnsetParent/UnsetGrabbed/SetParent/SetGrabbed do not touch the run
    at all. Their `stfs f0,52(r3)` is a store through a POINTER FIELD loaded
    by `lwz r3,116(r3)`; 52 = 0x34 collides with hud_pad_034's offset.
  * gDefaultPlayerPosition's first reference is kill_player (gc#34), not
    do_players (gc#21). do_players builds `addi r21,r31,0xc40` — one byte
    PAST the end of the 0xC40-byte run — and its `lwz r0,0x934(r21)` reads
    0x80276414, a different object entirely.

This one tracks PROVENANCE: a register holds a run address only when it was
built from an `@ha`/`@l` pair naming a symbol in the run, or copied or
displaced from such a register, and a reference is recorded only when the
resulting address lands inside the run. Loose hits are still collected and
reported separately as UNCONFIRMED so nothing is silently dropped.

LIVE, measured in W:/Repositories/GDL-Claude-P5 at 1bf4aabbf:

  .bss (run 0x80274EA0..0x80275AE0, from this unit's splits.txt claim)
    OURS      15 statics then 1 external — gDefaultPlayerPosition LAST, at
              +0xC34, exactly as the law says
    TARGET    gDefaultPlayerPosition at +0x934, between got_it and
              lbl_802757E0
    SEATED    validates: all 5 independently known target symbols land where
              the seating puts them (lbl_80274EA0 +0x0, player_multiple_models
              +0x514, got_it +0x694, lbl_802757E0 +0x940, frame_blit +0xBE0)
    VERDICT   DECLARATION ORDER over the statics is preserved intact; the
              single break is the seated external, and FIRST-REFERENCE order
              does not explain it either — the eight objects from +0x934 on
              have surviving first references gc# 34, 62, 5, 9, 5, 5, 5, 5,
              which is not ascending. A discarded function that referenced
              those eight in ascending address order before gc#5
              del_player_blits is what the law requires.

  .sbss --run 0x80344AFC..0x80344B18  (extent measured by lane P3; our
    object defines no .sbss, so nothing here is derivable from it)
    alpha is SECOND by address and FIRST of its pair by first reference:
    both it and key_blit_idx are first read in write_health_and_items
    (gc#9), alpha earlier in the body. Address order and first-reference
    order disagree by that one swap.

CONTROLS, both EXACT units whose `.bss` the link already proves right:

  game/mb/mb_poly   3 objects, ZERO discordant pairs under BOTH laws — the
                    two orders coincide, and nothing is implied.
  game/sys/recorder 0 discordant pairs under DECLARATION order and 4 under
                    FIRST-REFERENCE order. This is the calibration that
                    matters: a Matching unit whose first-reference order is
                    plainly wrong proves the screen is not merely reporting
                    "whatever order the text happens to show", and that a
                    first-reference disagreement alone is NOT a defect.

WHAT IT IS NOT. A seat is not a name: player's `.bss` seater has no PDB
counterpart at its slot, so `srcorder.py` cannot bound it and this screen
reports the requirement, not a candidate. An unreferenced object (player's
hud_pad_034) is not evidence of anything — it is reached only through the
per-player index arithmetic already attributed to the object that covers it.

EXIT 0 when the orders were measured, 2 when the run could not be
established (say so with `--run LOW..HIGH` rather than guessing).

IMPORTABLE CORE: object_symbols, partition, seat_layout, validate_seating,
track_references, loose_references, order_verdict, survey -- pure over
objects and symbols.txt, no build, no printing.
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, HERE)

import claimable_sections as cs                           # noqa: E402
import cliscreen                                          # noqa: E402
import fndiff                                             # noqa: E402
import textorder                                          # noqa: E402

OK, REFUSED = 0, 2
VERSION = "GUNE5D"
BSS_SECTIONS = (".bss", ".sbss", ".sbss2")

_MEM = re.compile(r"^[a-z][a-z0-9._]*\s+[rf]\d+,(-?\d+)\(r(\d+)\)$")
_ADDI = re.compile(r"^addi\s+r(\d+),r(\d+),(-?\d+)$")
_LIS = re.compile(r"^lis\s+r(\d+),")
_ADD = re.compile(r"^add\s+r(\d+),r(\d+),r(\d+)$")
_MR = re.compile(r"^mr\s+r(\d+),r(\d+)$")
_XFORM = re.compile(r"^[a-z][a-z0-9._]*x\s+[rf]\d+,r(\d+),r(\d+)$")
_DEST = re.compile(r"^[a-z][a-z0-9._]*\s+r(\d+),")
_LOOSE_BAD_BASE = {1, 2, 13}


class Refused(Exception):
    """A stated precondition of the measurement is not met."""


# ---------------------------------------------------------------- IMPORTABLE


def object_symbols(objfile, section):
    """[(offset, size, name, binding)] for one section, in address order.

    `binding` is "static" for a local symbol and "external" for a global —
    the partition the emission law turns on. dtk's extracted objects mark
    everything global, so this distinction is only meaningful on OUR object.
    """
    rows = []
    for line in fndiff.objdump(objfile, "-t").splitlines():
        if "\t" not in line:
            continue
        left, right = line.split("\t", 1)
        fields, tail = left.split(), right.split()
        if len(fields) < 2 or len(tail) < 2 or "O" not in fields[1:]:
            continue
        if fields[-1] != section:
            continue
        try:
            offset, size = int(fields[0], 16), int(tail[0], 16)
        except ValueError:
            continue
        if not size:
            continue
        rows.append((offset, size, tail[-1],
                     "external" if "g" in fields[1:] else "static"))
    return sorted(rows)


def partition(rows):
    """(statics, externals, law_shape). `law_shape` is True when every
    static precedes every external, which is the shape the emission law
    predicts for OUR object and the shape a violated target breaks."""
    statics = [row for row in rows if row[3] == "static"]
    externals = [row for row in rows if row[3] == "external"]
    if not statics or not externals:
        return statics, externals, True
    return statics, externals, statics[-1][0] < externals[0][0]


def seat_layout(statics, externals, target_offsets, align=4):
    """[(offset, size, name, binding)] — statics in declaration order with
    each external moved to its known target offset.

    A static is placed at the running cursor; whenever the cursor reaches a
    seated external's offset that external is emitted first and the cursor
    resumes past it. An external whose target offset the caller does not
    know is appended, which is where our own compiler puts it. Returns
    (layout, conflicts): a conflict is a seat the cursor had already passed,
    and it means the seating hypothesis is wrong, not that the seat is.
    """
    seats = sorted((target_offsets[name], size, name)
                   for _offset, size, name, _binding in externals
                   if name in target_offsets)
    trailing = [row for row in externals if row[2] not in target_offsets]
    layout, conflicts, cursor, index = [], [], 0, 0
    for _offset, size, name, _binding in statics:
        while index < len(seats) and seats[index][0] <= cursor:
            seat = seats[index]
            if seat[0] < cursor:
                conflicts.append({"name": seat[2], "target_offset": seat[0],
                                  "cursor": cursor})
            layout.append((seat[0], seat[1], seat[2], "external"))
            cursor = seat[0] + seat[1]
            index += 1
        if cursor % align:
            cursor += align - (cursor % align)
        layout.append((cursor, size, name, "static"))
        cursor += size
    for seat in seats[index:]:
        layout.append((seat[0], seat[1], seat[2], "external"))
        cursor = max(cursor, seat[0] + seat[1])
    for _offset, size, name, _binding in trailing:
        if cursor % align:
            cursor += align - (cursor % align)
        layout.append((cursor, size, name, "external"))
        cursor += size
    return layout, conflicts


def validate_seating(layout, target_offsets, seeded):
    """{'agree','disagree','rows'} over target symbols NOT seeded into the
    seating. Seeding a symbol and then checking it proves nothing, so those
    names are excluded from the score and listed separately."""
    placed = {name: offset for offset, _size, name, _binding in layout}
    rows, agree, disagree = [], 0, 0
    for name, offset in sorted(target_offsets.items(), key=lambda kv: kv[1]):
        if name not in placed:
            continue
        ok = placed[name] == offset
        if name in seeded:
            rows.append((name, offset, placed[name], "seeded"))
            continue
        rows.append((name, offset, placed[name], "agree" if ok else "DIFFERS"))
        agree += 1 if ok else 0
        disagree += 0 if ok else 1
    return {"agree": agree, "disagree": disagree, "rows": rows}


def _resolve(symbol, addend, symbols):
    entry = symbols.get(symbol)
    if entry is not None:
        return entry[1] + addend
    address = fndiff.placeholder_address(symbol)
    return None if address is None else address + addend


def track_references(objfile, low, high):
    """{address: (gc_index, position, function, form)} — first reference.

    `position` is the instruction index inside the function, and it is part
    of the key because two objects first read in the SAME body have to be
    ordered somehow: player's `alpha` and `key_blit_idx` are both first read
    in write_health_and_items (gc#9), and which of them the pool law puts
    first is exactly the question the run is being asked.

    A register is a run-address holder only when it was built from an
    `@ha`/`@l` pair naming a symbol whose address lands in [low, high), or
    copied/displaced from such a register; a `R_PPC_EMB_SDA21` row naming
    such a symbol is a direct reference. Every other write to a register
    clears it. Returns (first, sequences) where `sequences[fn]` is that
    function's reference addresses in instruction order.
    """
    order = [name for _offset, _size, name in textorder.text_symbols(objfile)]
    table = fndiff.parse(objfile)
    symbols = fndiff.symbol_table()
    first, sequences = {}, {}

    def note(address, index, function, form, seq, position):
        if not (low <= address < high):
            return
        seq.append(address)
        if address not in first:
            first[address] = (index, position, function, form)

    for index, function in enumerate(order):
        lines = table.get(function, [])
        held, pending, seq = {}, {}, []
        for position, line in enumerate(lines):
            if line.startswith("    "):
                continue
            text = " ".join(line.split())
            following = lines[position + 1] if position + 1 < len(lines) else ""
            reloc = (following.strip().split("\t")[-1]
                     if following.startswith("    ") else "")
            symbol, addend = fndiff.split_addend(reloc) if reloc else ("", 0)
            address = _resolve(symbol, addend, symbols) if symbol else None

            if "EMB_SDA21" in following and address is not None:
                note(address, index, function, "sda21", seq, position)
                continue
            match = _LIS.match(text)
            if match:
                register = int(match.group(1))
                held.pop(register, None)
                pending.pop(register, None)
                if "ADDR16_HA" in following and address is not None \
                        and low <= address < high:
                    pending[register] = address
                continue
            match = _ADDI.match(text)
            if match:
                dest = int(match.group(1))
                source = int(match.group(2))
                immediate = int(match.group(3))
                if "ADDR16_LO" in following and source in pending \
                        and address is not None:
                    held[dest] = address
                    note(address, index, function, "sym", seq, position)
                elif source in held:
                    held[dest] = held[source] + immediate
                    note(held[dest], index, function, "addi", seq, position)
                else:
                    held.pop(dest, None)
                pending.pop(dest, None)
                continue
            match = _MR.match(text)
            if match:
                dest, source = int(match.group(1)), int(match.group(2))
                held.pop(dest, None)
                pending.pop(dest, None)
                if source in held:
                    held[dest] = held[source]
                continue
            match = _ADD.match(text)
            if match:
                dest = int(match.group(1))
                left, right = int(match.group(2)), int(match.group(3))
                pending.pop(dest, None)
                # An unknown index register only makes the address unknown
                # WITHIN the object; the object itself is still the base's.
                if left in held:
                    held[dest] = held[left]
                elif right in held:
                    held[dest] = held[right]
                else:
                    held.pop(dest, None)
                continue
            match = _XFORM.match(text)
            if match:
                for register in (int(match.group(1)), int(match.group(2))):
                    if register in held:
                        note(held[register], index, function, "indexed", seq, position)
                continue
            match = _MEM.match(text)
            if match:
                immediate, source = int(match.group(1)), int(match.group(2))
                if source in held:
                    note(held[source] + immediate, index, function, "mem", seq, position)
                continue
            match = _DEST.match(text)
            if match:
                held.pop(int(match.group(1)), None)
                pending.pop(int(match.group(1)), None)
        if seq:
            sequences[function] = seq
    return first, sequences


def loose_references(objfile, offsets, low):
    """{offset: (gc_index, function)} under lane P7's displacement rule.

    Kept so a row the provenance tracker does not see is REPORTED rather
    than dropped; every row here that the tracker did not confirm is an
    UNCONFIRMED candidate, and on player `.bss` all five such rows are the
    false positives the module docstring names.
    """
    order = [name for _offset, _size, name in textorder.text_symbols(objfile)]
    table = fndiff.parse(objfile)
    wanted = set(offsets)
    found = {}
    for index, function in enumerate(order):
        for line in table.get(function, []):
            if line.startswith("    "):
                continue
            text = " ".join(line.split())
            match = _ADDI.match(text)
            if match:
                base, immediate = int(match.group(2)), int(match.group(3))
            else:
                match = _MEM.match(text)
                if not match:
                    continue
                immediate, base = int(match.group(1)), int(match.group(2))
            if base in _LOOSE_BAD_BASE or immediate not in wanted:
                continue
            found.setdefault(immediate, (index, function))
    return found


def order_verdict(address_order, declaration_order, first_reference):
    """How the TARGET's address order relates to the two candidate laws.

    `address_order` and `declaration_order` are name lists;
    `first_reference` is {name: gc_index}. Returns the discordant adjacent
    pairs under each law, and the SUFFIX from the first name that breaks
    declaration order — the objects a seater would have to touch.
    """
    rank = {name: index for index, name in enumerate(declaration_order)}
    declaration_breaks = [
        (before, after) for before, after in zip(address_order,
                                                 address_order[1:])
        if before in rank and after in rank and rank[before] > rank[after]]
    firstref_breaks = [
        (before, after) for before, after in zip(address_order,
                                                 address_order[1:])
        if before in first_reference and after in first_reference
        and first_reference[before] > first_reference[after]]
    suffix = []
    for index, name in enumerate(address_order):
        if any(name == before for before, _after in declaration_breaks):
            suffix = address_order[index:]
            break
    return {"declaration_breaks": declaration_breaks,
            "firstref_breaks": firstref_breaks,
            "suffix": suffix,
            "suffix_first_reference": [first_reference.get(name)
                                       for name in suffix]}


def derive_run(unit, section, ours_object, explicit=None):
    """(low, high, why). Raises Refused when nothing establishes the run."""
    if explicit:
        return explicit[0], explicit[1], "given on the command line"
    splits_path = os.path.join(ROOT, "config", VERSION, "splits.txt")
    if os.path.exists(splits_path):
        splits = cs.parse_splits(splits_path)
        claims = (splits.get(unit + ".c") or splits.get(unit + ".cpp")
                  or splits.get(unit) or {})
        claim = claims.get(section)
        if claim:
            return claim[0], claim[1], "this unit's splits.txt claim"
    raise Refused(
        "no splits.txt %s claim for %s and none given — a BSS run carries no"
        " bytes, so nothing in the object establishes where it starts; pass"
        " --run 0xLOW..0xHIGH with the extent you measured" % (section, unit))


def survey(unit, section=".bss", run=None, object_path=None):
    """The whole record for one (unit, section). Pure of printing."""
    if section not in BSS_SECTIONS:
        raise Refused("%s is not a BSS section; this screen is about objects"
                      " that carry no bytes (%s)"
                      % (section, ", ".join(BSS_SECTIONS)))
    ours_object = object_path or os.path.join(
        ROOT, "build", VERSION, "src", unit + ".o")
    target_object = os.path.join(ROOT, "build", VERSION, "obj", unit + ".o")
    if not os.path.exists(target_object):
        raise Refused("no target object at %s"
                      % os.path.relpath(target_object, ROOT).replace("\\", "/"))
    low, high, why = derive_run(unit, section, ours_object, run)

    ours = (object_symbols(ours_object, section)
            if os.path.exists(ours_object) else [])
    statics, externals, law_shape = partition(ours)
    target_rows = object_symbols(target_object, section)
    target_offsets = {name: offset for offset, _size, name, _binding
                      in target_rows}
    table_symbols = [(address - low, size, name, "target")
                     for name, (where, address, size)
                     in fndiff.symbol_table().items()
                     if where == section and low <= address < high]

    seeded = {name for _offset, _size, name, _binding in externals
              if name in target_offsets}
    layout, conflicts, validation = [], [], None
    if ours and seeded:
        layout, conflicts = seat_layout(statics, externals, target_offsets)
        validation = validate_seating(layout, target_offsets, seeded)

    objects = layout or sorted(table_symbols) or [
        (offset, size, name, "target") for offset, size, name, _b in target_rows]
    if not objects:
        raise Refused("neither our object, symbols.txt nor the target object"
                      " names an object in %s 0x%08X..0x%08X"
                      % (section, low, high))

    first, _sequences = track_references(target_object, low, high)
    loose = loose_references(target_object, [row[0] for row in objects], low)

    def owner(address):
        for offset, size, name, _binding in objects:
            if offset <= address - low < offset + size:
                return name
        return None

    confirmed = {}
    for address, (index, position, function, form) in sorted(first.items()):
        name = owner(address)
        if name is None:
            continue
        key = (index, position)
        if name not in confirmed or key < confirmed[name][:2]:
            confirmed[name] = (index, position, function, form)
    unconfirmed = {}
    for offset, (index, function) in loose.items():
        name = owner(low + offset)
        if name is not None and name not in confirmed:
            unconfirmed[name] = (index, function)

    address_order = [name for _offset, _size, name, _b in objects]
    declaration_order = ([name for _offset, _size, name, _b in statics]
                         + [name for _offset, _size, name, _b in externals])
    # NO DECLARATION ORDER WITHOUT AN OBJECT. Falling back to the address
    # order would make the comparison compare a list with itself and print
    # "declaration order explains the target layout" for a unit whose source
    # defines nothing in the section at all.
    verdict = order_verdict(
        address_order, declaration_order,
        {name: row[:2] for name, row in confirmed.items()})
    verdict["declaration_known"] = bool(declaration_order)
    return {
        "unit": unit, "section": section, "run": [low, high], "run_why": why,
        "ours": ours, "statics": len(statics), "externals": len(externals),
        "law_shape": law_shape, "target_rows": target_rows,
        "symbols_txt": sorted(table_symbols),
        "layout": layout, "seat_conflicts": conflicts,
        "validation": validation,
        "objects": objects, "confirmed": confirmed, "unconfirmed": unconfirmed,
        "unreferenced": [name for name in address_order
                         if name not in confirmed and name not in unconfirmed],
        "verdict": verdict,
    }


# ------------------------------------------------------------------- RENDER


def render(record):
    low, high = record["run"]
    print("== %s %s: run 0x%08X..0x%08X (0x%X B) — %s"
          % (record["unit"], record["section"], low, high, high - low,
             record["run_why"]))
    print()
    print("   -- OURS: %d static(s), %d external(s) --"
          % (record["statics"], record["externals"]))
    if not record["ours"]:
        print("      our object defines nothing in this section; the blocks"
              " below are target-side only")
    for offset, size, name, binding in record["ours"]:
        print("      +0x%-5X 0x%-5X %-9s %s" % (offset, size, binding, name))
    if record["ours"]:
        print("      LAW SHAPE (all statics before all externals): %s"
              % ("holds" if record["law_shape"] else "BROKEN in our object"))
    print()
    print("   -- TARGET: %d object symbol(s), %d symbols.txt datum(s) in run"
          % (len(record["target_rows"]), len(record["symbols_txt"])))
    for offset, size, name, _binding in record["target_rows"]:
        print("      +0x%-5X 0x%-5X %s" % (offset, size, name))
    if record["validation"] is not None:
        print()
        print("   -- SEATED hypothesis: our statics in declaration order,"
              " externals at their target offsets --")
        for offset, size, name, binding in record["layout"]:
            print("      +0x%-5X 0x%-5X %-9s %s"
                  % (offset, size, binding, name))
        validation = record["validation"]
        print("      VALIDATION against target symbols NOT seeded into it:"
              " %d agree, %d differ" % (validation["agree"],
                                        validation["disagree"]))
        for name, target, placed, state in validation["rows"]:
            if state != "agree":
                print("         %-26s target +0x%-5X seated +0x%-5X  %s"
                      % (name, target, placed, state))
        if record["seat_conflicts"]:
            print("      SEAT CONFLICT — the seating is REFUTED, not the"
                  " seat: %s" % record["seat_conflicts"])
    print()
    print("   -- TARGET-SIDE FIRST REFERENCE (provenance-tracked) --")
    rows = sorted(record["confirmed"].items(), key=lambda kv: kv[1][:2])
    for name, (index, position, function, form) in rows:
        print("      %-28s gc#%-4d +%-4d %-38s %s"
              % (name, index, position, function, form))
    for name, (index, function) in sorted(record["unconfirmed"].items(),
                                          key=lambda kv: kv[1][0]):
        print("      %-28s gc#%-4d %-5s %-38s UNCONFIRMED (displacement only)"
              % (name, index, "-", function))
    if record["unreferenced"]:
        print("      NOT REFERENCED: %s" % ", ".join(record["unreferenced"]))
    print()
    verdict = record["verdict"]
    print("   -- VERDICT --")
    if not verdict["declaration_known"]:
        print("      target address order vs OUR DECLARATION order:"
              " UNAVAILABLE — our object defines nothing in this section, so"
              " there is no declaration order to compare against")
    else:
        print("      target address order vs OUR DECLARATION order: %d"
              " discordant adjacent pair(s)"
              % len(verdict["declaration_breaks"]))
        for before, after in verdict["declaration_breaks"]:
            print("         %s then %s" % (before, after))
    print("      target address order vs FIRST-REFERENCE order: %d"
          " discordant adjacent pair(s)" % len(verdict["firstref_breaks"]))
    for before, after in verdict["firstref_breaks"][:8]:
        print("         %s then %s" % (before, after))
    if verdict["suffix"]:
        print("      A SEATER IS REQUIRED. From %s onward the target holds"
              " %d object(s) whose surviving first references are %s — not"
              " ascending, so the law's external ordering cannot produce"
              " this layout from the text that survives. A discarded"
              " function referencing them in ascending address order before"
              " gc#%s is what it needs."
              % (verdict["suffix"][0], len(verdict["suffix"]),
                 ", ".join("gc#%s" % (value[0] if value else "?")
                           for value in verdict["suffix_first_reference"]),
                 min((value[0] for value in verdict["suffix_first_reference"]
                      if value is not None), default="?")))
    elif verdict["declaration_known"] and not verdict["declaration_breaks"]:
        print("      DECLARATION ORDER explains the target layout with no"
              " break; no seat is implied.")
    print()
    print("   ORDER IS THE ONLY EVIDENCE HERE. A BSS run carries no bytes, so"
          " none of this is checked by a byte diff; a seat implies a missing"
          " referencer, never a name.")


def parse_run(text):
    if not text:
        return None
    match = re.match(r"^\s*(0x[0-9A-Fa-f]+|\d+)\s*\.\.\s*"
                     r"(0x[0-9A-Fa-f]+|\d+)\s*$", text)
    if not match:
        raise ValueError(text)
    return int(match.group(1), 0), int(match.group(2), 0)


def main(argv=None):
    cliscreen.help_only(__doc__)
    parser = argparse.ArgumentParser(
        prog="bssorder.py",
        description="Compare our BSS layout, the target's and first-reference"
                    " order.")
    parser.add_argument("unit")
    parser.add_argument("--section", default=".bss",
                        help="%s (default .bss)" % ", ".join(BSS_SECTIONS))
    parser.add_argument("--run", help="0xLOW..0xHIGH when splits.txt has no"
                                      " claim for this section")
    parser.add_argument("--object", help="read this object instead of"
                                         " build/GUNE5D/src/<unit>.o")
    parser.add_argument("--out", help="write the record as JSON (under build/)")
    args = parser.parse_args(argv)
    unit = fndiff.unit_key(args.unit)
    try:
        run = parse_run(args.run)
    except ValueError:
        print("bssorder: %r is not a 0xLOW..0xHIGH range" % args.run)
        return REFUSED
    try:
        record = survey(unit, args.section, run, args.object)
    except Refused as refusal:
        print("bssorder REFUSED: %s" % refusal)
        return REFUSED
    except (OSError, fndiff.ObjdumpFailed, cs.Unmeasurable) as error:
        print("bssorder REFUSED: %s" % error)
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
