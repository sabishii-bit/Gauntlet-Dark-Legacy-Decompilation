#!/usr/bin/env python3
"""Image-wide census of UNCLAIMED object data sections in NonMatching TUs.

`datadiff.py --sections` already names the gap, one unit at a time:

    [game/game/controls] .data: SIZE target 0x0 vs ours 0xCB4  <- FLIP
    BLOCKER - UNCLAIMED SECTION - the target side is 0x0 because the dtk
    split assigned no bytes of this section to this TU.

That is the whole data campaign's queue, and nothing enumerated it or
ranked it. This does: for every unit, every data-class section our object
emits that `config/GUNE5D/splits.txt` does not claim, it derives the
candidate target extent BY RELOCATION RESOLUTION, compares our bytes to the
DOL bytes there, and ranks the units by how many bytes are actually
claimable today.

WHAT DECIDES A BASE
-------------------
Never equal-value matching, and never section size. Our object's .text
relocations and the dtk-extracted target object's .text relocations are
paired by (function, function-relative offset); every pair whose OURS side
names a symbol defined in the section under test yields

    base = target_address + target_addend - our_addend - our_symbol_offset

i.e. the address our section offset 0 binds to. Pairing by offset inside a
NonMatching TU is NOT sound on its own -- the two instruction streams differ
-- so a base is only ever a CANDIDATE here, and the DOL bytes at it are the
discriminant that promotes or kills it. Measured at 20c0d7ea1,
`af_data_base_census.py` reports THREE bases for game/game/controls .sdata2
(0x803463E8, 0x803463F8, 0x80346408) for a section whose shipped claim is
0x803463F8; only that one is byte-equal in the DOL, and the other two are
single-row anonymous-pool artefacts of exactly this mispairing.

SHORT AT THE FRONT
------------------
Our object can emit FEWER bytes than the target section, missing datums at
the FRONT (an `extern` placeholder still standing in for a const the source
has not recovered). Then `base` is where OUR bytes land, which is above the
target section's true start, and any derivation that works back from the
section SIZE is wrong by that gap. This anchors on the resolved relocation
instead, then asks symbols.txt whether `base` is itself a symbol start:

  - base on a symbol boundary          -> the extent starts there
  - base strictly inside a symbol run  -> SHORT AT FRONT; the section is
    reported `blocked-short-at-front` with the byte gap, and the orphan
    extern list below usually names the missing datum

A section is only called `claimable` when every compared byte is equal, so
a section whose bytes differ at its candidate base can never be listed as
claimable -- that is the negative half of the calibration.

ORPHAN EXTERNS
--------------
Undefined symbols our object references that NO built object under
build/<version>/src defines: they resolve today only out of the extracted
target image. Those are the datums a TU still has to recover into its own
source before its section can be complete, and they are the usual cause of
a short-at-front section.

Usage (from the repository root):
  python tools/gdl/claimable_sections.py                     # NonMatching
  python tools/gdl/claimable_sections.py game/game/controls
  python tools/gdl/claimable_sections.py --all --top 20
  python tools/gdl/claimable_sections.py --out build/GUNE5D/claimable.json
  python tools/gdl/claimable_sections.py --splits <file> <unit>   # calibrate

Exit 0 when every selected unit was measured, 2 when any unit could not be
(missing object, unreadable DOL range). Finding nothing claimable is a
measured result and exits 0; it is never silence.

VERIFY BEFORE CLAIMING. This proves where our bytes bind and that they
equal the DOL there. It does not prove ownership of the whole target
extent, link placement, or that the surplus is not dead-stripped. Write the
splits line, rebuild, and require `datadiff.py --sections <unit>`.

IMPORTABLE CORE: candidate_bases, score_base, boundary_of, section_result,
rank_units -- pure over rows/bytes already read, no subprocess, no build.
"""

import argparse
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

import fndiff  # noqa: E402

REPO = HERE.parent.parent
VERSION = "GUNE5D"

# Sections a splits.txt block can claim for a C unit, in report order.
BYTE_SECTIONS = (".rodata", ".data", ".sdata", ".sdata2")
BSS_SECTIONS = (".bss", ".sbss", ".sbss2")
DATA_SECTIONS = BYTE_SECTIONS + BSS_SECTIONS


class Unmeasurable(RuntimeError):
    """A missing measurement, never an empty successful census."""


# --------------------------------------------------------------------------
# splits.txt / configure.py inventory


def parse_splits(path):
    units, current = {}, None
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        head = re.match(r"^(\S.+):$", line)
        if head:
            current = head.group(1)
            units[current] = {}
            continue
        row = re.match(r"^\t(\S+)\s+start:0x([0-9A-Fa-f]+)"
                       r"\s+end:0x([0-9A-Fa-f]+)", line)
        if row and current:
            units[current][row.group(1)] = (int(row.group(2), 16),
                                            int(row.group(3), 16))
    return units


def claimed_intervals(units):
    """section -> sorted [(start, end, unit)] over the whole image."""
    table = {}
    for unit, sections in units.items():
        for section, (lo, hi) in sections.items():
            table.setdefault(section, []).append((lo, hi, unit))
    for rows in table.values():
        rows.sort()
    return table


def configured_units(path, matching=None):
    """[unit_key] from configure.py; matching=False selects NonMatching."""
    text = Path(path).read_text(encoding="utf-8")
    out = []
    for state, name in re.findall(r'Object\((\w+),\s*"([^"]+)"', text):
        if matching is not None and (state == "Matching") != matching:
            continue
        if not name.endswith((".c", ".cpp")):
            continue
        out.append(fndiff.unit_key(name))
    return out


# --------------------------------------------------------------------------
# object reads


def _dump(path, *flags):
    return fndiff.objdump(Path(path), *flags)


def relocation_rows(path):
    """[(function, function-relative offset, type, symbol, addend)]."""
    rows, current, start = [], None, 0
    for line in _dump(path, "-dr").splitlines():
        head = re.match(r"^([0-9a-f]+) <(.+)>:$", line)
        if head:
            current, start = head.group(2), int(head.group(1), 16)
            continue
        row = re.match(r"^\s*([0-9a-f]+):\s+(R_PPC\S+)\s+(\S+)$", line)
        if row and current is not None:
            symbol, addend = row.group(3), 0
            for separator, sign in (("+0x", 1), ("-0x", -1)):
                if separator in symbol:
                    symbol, tail = symbol.split(separator, 1)
                    addend = sign * int(tail, 16)
                    break
            rows.append((current, int(row.group(1), 16) - start,
                         row.group(2), symbol, addend))
    return rows


def object_symbols(path):
    """name -> (section, value, size) for symbols defined in this object."""
    table = {}
    for line in _dump(path, "-t").splitlines():
        row = re.match(r"^([0-9a-f]{8})\s+\S+\s+\S+\s+(\S+)\s+"
                       r"([0-9a-f]{8})\s+(.*)$", line)
        if row:
            table[row.group(4).strip()] = (row.group(2), int(row.group(1), 16),
                                           int(row.group(3), 16))
    return table


def undefined_symbols(path):
    """Names this object references but does not define."""
    out = set()
    for line in _dump(path, "-t").splitlines():
        row = re.match(r"^([0-9a-f]{8})\s+(.{7})\s+(\S+)\s+"
                       r"([0-9a-f]{8})\s+(.*)$", line)
        if row and row.group(3) == "*UND*":
            out.add(row.group(5).strip())
    return out


def section_sizes(path):
    sizes = {}
    for line in _dump(path, "-h").splitlines():
        row = re.match(r"^\s*\d+\s+(\S+)\s+([0-9a-f]{8})", line)
        if row:
            sizes[row.group(1)] = int(row.group(2), 16)
    return sizes


def section_bytes(path, section):
    data = bytearray()
    for line in _dump(path, "-s", "-j", section).splitlines():
        row = re.match(r"^ [0-9a-f]+ ((?:[0-9a-f]{2,8} ?){1,4}) ", line)
        if row:
            data += bytes.fromhex(row.group(1).replace(" ", ""))
    return bytes(data)


def relocated_offsets(path):
    """section -> {word offsets carrying a relocation in our object}."""
    table, section = {}, None
    for line in _dump(path, "-r").splitlines():
        head = re.match(r"^RELOCATION RECORDS FOR \[(\S+)\]", line)
        if head:
            section = head.group(1)
            table[section] = set()
            continue
        row = re.match(r"^([0-9a-f]+)\s+R_PPC_", line)
        if row and section:
            table[section].add(int(row.group(1), 16) & ~3)
    return table


# --------------------------------------------------------------------------
# IMPORTABLE CORE


def candidate_bases(ours_rows, target_rows, our_symbols,
                    sections=DATA_SECTIONS):
    """{section: {base: {"symbols": [...], "targets": [...]}}} plus stats.

    Returns (bases, stats). A base is where OUR section offset 0 binds.
    Pairing tolerates the +/-2 offset objdump records EMB_SDA21 relocations
    at on one side of some object pairs; the count is reported, never hidden.
    """
    target = {(fn, off): (kind, sym, add)
              for fn, off, kind, sym, add in target_rows}
    symbols = fndiff.symbol_table()
    bases, stats = {}, {"paired": 0, "unpaired": 0, "offset_shifted": 0,
                        "unresolved_target": 0}
    for fn, off, kind, sym, add in ours_rows:
        section, value, _size = our_symbols.get(sym, (None, None, None))
        if section not in sections:
            continue
        hit = None
        for delta in (0, -2, 2):
            found = target.get((fn, off + delta))
            if found and found[0] == kind:
                hit = found
                stats["offset_shifted"] += 1 if delta else 0
                break
        if hit is None:
            stats["unpaired"] += 1
            continue
        _kind, target_symbol, target_addend = hit
        literal = re.match(r"^lbl_([0-9A-Fa-f]{8})$", target_symbol)
        if literal:
            address = int(literal.group(1), 16)
        else:
            entry = symbols.get(target_symbol)
            address = entry[1] if isinstance(entry, tuple) and len(entry) >= 2 \
                else entry
        if not isinstance(address, int):
            stats["unresolved_target"] += 1
            continue
        stats["paired"] += 1
        base = address + target_addend - add - value
        row = bases.setdefault(section, {}).setdefault(
            base, {"symbols": set(), "targets": set()})
        row["symbols"].add(sym)
        row["targets"].add(target_symbol)
    for section in bases:
        bases[section] = {base: {"symbols": sorted(row["symbols"]),
                                 "targets": sorted(row["targets"])}
                          for base, row in bases[section].items()}
    return bases, stats


def score_base(our_bytes, relocated, dol_bytes):
    """Word-by-word DOL agreement for one candidate base.

    Words carrying a relocation in our object are NOT compared: their final
    value exists only after the link. `compared` is therefore the real
    denominator, and a section with nothing comparable scores None, never
    100% -- an unmeasured section must not read as a perfect one.
    """
    if dol_bytes is None or len(dol_bytes) < len(our_bytes):
        return {"compared": 0, "equal": 0, "percent": None, "skipped": 0,
                "first_difference": None,
                "note": "the candidate range is not inside the DOL"}
    compared = equal = skipped = 0
    first = None
    for offset in range(0, len(our_bytes), 4):
        if offset in relocated:
            skipped += 1
            continue
        mine = bytes(our_bytes[offset:offset + 4])
        theirs = bytes(dol_bytes[offset:offset + len(mine)])
        compared += 1
        if mine == theirs:
            equal += 1
        elif first is None:
            first = {"offset": offset, "ours": mine.hex(),
                     "dol": theirs.hex()}
    return {"compared": compared, "equal": equal, "skipped": skipped,
            "percent": (100.0 * equal / compared) if compared else None,
            "first_difference": first,
            "note": "" if compared else
            "every word is relocated; no byte comparison was possible"}


def boundary_of(section, address, symbols=None):
    """Where `address` sits relative to symbols.txt: exact / inside / gap."""
    symbols = fndiff.symbol_table() if symbols is None else symbols
    for name, entry in symbols.items():
        if not (isinstance(entry, tuple) and len(entry) >= 3):
            continue
        where, start, size = entry[0], entry[1], entry[2]
        if where != section:
            continue
        if start == address:
            return {"kind": "exact", "symbol": name, "start": start,
                    "end": start + size}
        if size and start < address < start + size:
            return {"kind": "inside", "symbol": name, "start": start,
                    "end": start + size, "front_gap": address - start}
    return {"kind": "gap", "symbol": None}


def straddled_end(section, end, symbols=None):
    """The symbol an END address falls strictly inside, or None.

    dtk refuses a split ending inside a symbol, so the claim must round up
    to that symbol's end and carry the slack (claim.law.AF_dtk-rejects-an-
    unaligned-auto-split-start-so-some-claim-slack-is-structural).
    """
    symbols = fndiff.symbol_table() if symbols is None else symbols
    for name, entry in symbols.items():
        if not (isinstance(entry, tuple) and len(entry) >= 3):
            continue
        where, start, size = entry[0], entry[1], entry[2]
        if where == section and size and start < end < start + size:
            return name, start, start + size
    return None


def next_symbol_start(section, address, symbols=None):
    """The lowest symbols.txt symbol start strictly above `address`."""
    symbols = fndiff.symbol_table() if symbols is None else symbols
    best = None
    for entry in symbols.values():
        if not (isinstance(entry, tuple) and len(entry) >= 2):
            continue
        if entry[0] == section and entry[1] > address:
            if best is None or entry[1] < best:
                best = entry[1]
    return best


def resolve_end(section, end, symbols=None, starts=(), claim_starts=()):
    """Where a claim may actually END, and why. (end, note-or-None).

    dtk auto-generates a split beginning where a claim stops, and rejects it
    unless that address is 4-byte aligned or another unit already claims the
    range (claim.law.AF_dtk-rejects-an-unaligned-auto-split-start-so-some-
    claim-slack-is-structural.20260903.v1). An end that is a symbol start is
    fine; one that is not, and is not aligned, must round up to the next
    symbol start and carry the slack. A PASTE line that ignores this is a
    build failure handed to the next lane.
    """
    if end in starts or end % 4 == 0 or end in claim_starts:
        return end, None
    rounded = next_symbol_start(section, end, symbols)
    if rounded is None:
        return end, ("end 0x%08X is neither 4-byte aligned nor a symbol"
                     " start, and no later symbol was found: dtk will reject"
                     " the auto-split it generates here" % end)
    return rounded, ("end 0x%08X is neither 4-byte aligned nor a symbol"
                     " start, so dtk would reject the auto-split it"
                     " generates; rounded up to the next symbol start"
                     " 0x%08X and the 0x%X byte(s) of slack are structural"
                     % (end, rounded, rounded - end))


def overlapping_claim(section, lo, hi, intervals, exclude=None):
    """The first existing claim [lo, hi) would collide with, or None."""
    for start, end, unit in intervals.get(section, ()):
        if unit == exclude:
            continue
        if start < hi and lo < end:
            return {"unit": unit, "start": start, "end": end}
    return None


def section_result(section, our_size, bases, best, score, boundary,
                   straddle, collision, bss, prior=None, align=None):
    """The verdict for one claimable section. Pure; every input measured.

    Verdicts, in refusal order:
      unresolved-no-base       nothing in .text binds into this section
      unresolved-disagreement  no candidate wins on measured DOL bytes
      conflict-existing-claim  the extent collides with another unit
      blocked-short-at-front   the base sits inside a target symbol run
      blocked-bytes-differ     the winning base is not byte-equal
      claimable-bss            no DOL bytes exist; consensus only
      claimable                every compared byte equals the DOL

    `prior` is this unit's own existing, TOO SHORT claim for the section, if
    any: our object emits more than splits.txt gives it, so the same
    derivation extends the claim instead of creating one. This is the case
    66ccb3fc6 landed by hand for game/world/btricol (.sdata2 end 0x80345D70
    -> 0x80345DB8 for a 0x74-byte pool). `claim_bytes` then counts the bytes
    GAINED, not the whole extent.
    """
    row = {"section": section, "ours_size": our_size,
           "candidate_count": len(bases),
           "candidates": ["0x%08X" % base for base in sorted(bases)]}
    if prior:
        row["existing_claim"] = "0x%08X..0x%08X" % prior
        row["kind"] = "extend"
    else:
        row["kind"] = "new"
    if not bases:
        row["verdict"] = "unresolved-no-base"
        row["reason"] = ("no .text relocation binds into this section: it is"
                         " reached through a section symbol, is dead data,"
                         " or the target object pairs nothing here")
        return row
    if best is None:
        row["verdict"] = "unresolved-disagreement"
        row["reason"] = ("candidate bases disagree and no single one is"
                         " byte-equal in the DOL; compare the bytes before"
                         " believing any of them")
        return row
    if prior and best != prior[0]:
        row["verdict"] = "unresolved-base-vs-claim"
        row["base"] = "0x%08X" % best
        row["reason"] = ("the resolved base 0x%08X disagrees with this"
                         " unit's existing claim start 0x%08X; the claim,"
                         " the derivation, or the object is wrong and none"
                         " of the three is settled here"
                         % (best, prior[0]))
        return row
    end = best + our_size
    row.update(base="0x%08X" % best, claim_start="0x%08X" % best,
               claim_end="0x%08X" % end, support=score.get("support"),
               dissent=score.get("dissent"), dol=score.get("dol"))
    if straddle:
        name, _start, symbol_end = straddle
        row["claim_end"] = "0x%08X" % symbol_end
        row["end_note"] = (
            "object end 0x%08X falls inside target symbol %s (ends 0x%08X);"
            " dtk refuses a split that ends within a symbol, so the claim"
            " ends at the symbol boundary and the slack is structural"
            % (end, name, symbol_end))
        end = symbol_end
    if align is not None:
        aligned, note = align(end)
        if note:
            row["end_alignment_note"] = note
            end = aligned
            row["claim_end"] = "0x%08X" % end
    row["claim_bytes"] = (end - best) if not prior else max(end - prior[1], 0)
    hit = collision(best, end) if callable(collision) else collision
    if hit:
        row["verdict"] = "conflict-existing-claim"
        row["reason"] = ("0x%08X..0x%08X overlaps %s's claim 0x%08X..0x%08X"
                         % (best, end, hit["unit"], hit["start"], hit["end"]))
        return row
    row["base_boundary"] = boundary["kind"]
    if boundary["kind"] == "inside":
        row["verdict"] = "blocked-short-at-front"
        row["front_gap"] = boundary["front_gap"]
        row["reason"] = (
            "the resolved base 0x%08X is 0x%X byte(s) INSIDE target symbol %s"
            " (0x%08X..0x%08X): our object is short at the FRONT of this"
            " section, so the target extent starts below where our bytes"
            " bind. Recover the missing datum(s) into the source first --"
            " see this unit's orphan externs -- and re-run."
            % (best, boundary["front_gap"], boundary["symbol"],
               boundary["start"], boundary["end"]))
        return row
    if bss:
        row["verdict"] = "claimable-bss"
        row["reason"] = ("bss occupies no DOL bytes, so nothing can be"
                         " byte-compared: this rests on relocation consensus"
                         " and the symbol boundary alone")
        return row
    dol = score.get("dol") or {}
    if not dol.get("compared"):
        # Zero equal of zero compared is not a difference, and must never
        # print as one: nothing was measured here.
        row["verdict"] = "unresolved-not-comparable"
        row["reason"] = (dol.get("note")
                         or "no word of this section could be compared")
        return row
    if dol.get("percent") == 100.0:
        row["verdict"] = "claimable"
        row["reason"] = ("all %d compared word(s) equal the DOL at this base"
                         " (%d relocated word(s) not comparable before the"
                         " link)" % (dol["compared"], dol.get("skipped", 0)))
        return row
    row["verdict"] = "blocked-bytes-differ"
    row["reason"] = ("only %s of %s compared word(s) equal the DOL at the"
                     " best candidate base%s"
                     % (dol.get("equal"), dol.get("compared"),
                        "" if not dol.get("first_difference") else
                        "; first at +0x%X (ours %s vs dol %s)"
                        % (dol["first_difference"]["offset"],
                           dol["first_difference"]["ours"],
                           dol["first_difference"]["dol"])))
    return row


def rank_units(results):
    """[(unit, claimable_bytes, bss_bytes, blocked_bytes)] worth first."""
    rows = []
    for result in results:
        claimable = sum(row.get("claim_bytes", 0) for row in result["sections"]
                        if row["verdict"] == "claimable")
        bss = sum(row.get("claim_bytes", 0) for row in result["sections"]
                  if row["verdict"] == "claimable-bss")
        blocked = sum(row.get("ours_size", 0) for row in result["sections"]
                      if row["verdict"].startswith(("blocked", "unresolved",
                                                    "conflict")))
        if claimable or bss or blocked:
            rows.append((result["unit"], claimable, bss, blocked))
    rows.sort(key=lambda row: (-(row[1] + row[2]), -row[3], row[0]))
    return rows


# --------------------------------------------------------------------------
# defined-symbol index for the orphan-extern screen


def _object_inventory(root, version):
    return sorted(Path(root, "build", version, "src").rglob("*.o"))


def defined_symbols(root=REPO, version=VERSION, cache=True):
    """Every symbol name defined by a built object under build/<v>/src.

    Cached under build/<v>/t4_defined_symbols.json against the (path, mtime,
    size) inventory of those objects, so a rebuilt object invalidates it.
    """
    objects = _object_inventory(root, version)
    stamp = [[path.relative_to(root).as_posix(), path.stat().st_mtime_ns,
              path.stat().st_size] for path in objects]
    store = Path(root, "build", version, "t4_defined_symbols.json")
    if cache and store.is_file():
        try:
            saved = json.loads(store.read_text(encoding="utf-8"))
            if saved.get("inventory") == stamp:
                return set(saved["symbols"])
        except (OSError, ValueError, KeyError):
            pass
    names = set()
    for path in objects:
        for line in _dump(path, "-t").splitlines():
            row = re.match(r"^([0-9a-f]{8})\s+(.{7})\s+(\S+)\s+"
                           r"([0-9a-f]{8})\s+(.*)$", line)
            if row and row.group(3) not in ("*UND*", "*ABS*"):
                names.add(row.group(5).strip())
    if cache:
        store.parent.mkdir(parents=True, exist_ok=True)
        store.write_text(json.dumps({"inventory": stamp,
                                     "symbols": sorted(names)}), "utf-8")
    return names


def orphan_externs(our_object, defined, symbols=None):
    """Undefined DATA symbols no built src object defines.

    These resolve today only out of the extracted target object, so they are
    exactly the datums a TU still owes its own source. Function symbols are
    excluded by their symbols.txt section, and a name symbols.txt does not
    know at all is reported with section `unknown` rather than dropped.
    """
    symbols = fndiff.symbol_table() if symbols is None else symbols
    out = []
    for name in sorted(undefined_symbols(our_object) - set(defined)):
        entry = symbols.get(name)
        if isinstance(entry, tuple) and len(entry) >= 3:
            section, address, size = entry[0], entry[1], entry[2]
            if section == ".text":
                continue
            out.append({"name": name, "section": section,
                        "address": "0x%08X" % address, "size": size})
        else:
            out.append({"name": name, "section": "unknown", "address": None,
                        "size": None})
    return out


# --------------------------------------------------------------------------
# per-unit census


def our_object(unit, root=REPO, version=VERSION):
    from raw_object import RawObjectError, resolve_object
    try:
        return resolve_object(unit, root=root, version=version).path
    except RawObjectError as error:
        raise Unmeasurable(str(error)) from error


def census(unit, splits, intervals, defined=None, root=REPO, version=VERSION):
    unit = fndiff.unit_key(unit)
    try:
        ours = our_object(unit, root=root, version=version)
    except Unmeasurable as error:
        return {"unit": unit, "status": "UNRESOLVED", "error": str(error),
                "sections": [], "orphan_externs": []}
    target = Path(root, "build", version, "obj", *unit.split("/")[:-1],
                  unit.split("/")[-1] + ".o")
    if not target.is_file():
        return {"unit": unit, "status": "UNRESOLVED", "sections": [],
                "orphan_externs": [],
                "error": "missing target object: %s" % target}

    claims = splits.get(unit + ".c") or splits.get(unit + ".cpp") \
        or splits.get(unit) or {}
    sizes = section_sizes(ours)
    # Unclaimed sections, plus this unit's own claims that are SHORTER than
    # the bytes our object emits: both need the same derivation, and the
    # second is the shape 66ccb3fc6 fixed for btricol by hand.
    unclaimed = []
    for section in DATA_SECTIONS:
        size = sizes.get(section)
        if not size:
            continue
        prior = claims.get(section)
        if prior is None:
            unclaimed.append((section, None))
        elif prior[1] - prior[0] < size:
            unclaimed.append((section, prior))
    result = {"unit": unit, "status": "PASS", "sections": [],
              "claimed_sections": sorted(claims),
              "orphan_externs": []}
    if not unclaimed:
        result["note"] = ("every data-class section this object emits is"
                          " already claimed at or beyond its emitted size;"
                          " nothing to derive")
        return result

    bases, stats = candidate_bases(relocation_rows(ours),
                                   relocation_rows(target),
                                   object_symbols(ours))
    result["pairing"] = stats
    relocated = relocated_offsets(ours)
    symbols = fndiff.symbol_table()
    for section, prior in unclaimed:
        size = sizes[section]
        bss = section in BSS_SECTIONS
        found = bases.get(section, {})
        payload = b"" if bss else section_bytes(ours, section)
        scored = {}
        for base, evidence in found.items():
            dol = None if bss else score_base(
                payload, relocated.get(section, set()),
                fndiff.dol_read(base, size))
            scored[base] = {"support": len(evidence["symbols"]),
                            "dissent": len(found) - 1, "dol": dol,
                            "symbols": evidence["symbols"][:8],
                            "targets": evidence["targets"][:8]}
        best = _elect(scored, bss)
        boundary = boundary_of(section, best, symbols) if best is not None \
            else None
        straddle = straddled_end(section, best + size, symbols) \
            if best is not None else None
        symbol_starts = {entry[1] for entry in symbols.values()
                         if isinstance(entry, tuple) and entry[0] == section}
        claim_starts = {start for start, _end, owner
                        in intervals.get(section, ()) if owner != unit + ".c"}
        row = section_result(
            section, size, found, best, scored.get(best, {}), boundary,
            straddle,
            lambda lo, hi: overlapping_claim(section, lo, hi, intervals,
                                             exclude=unit + ".c"),
            bss, prior,
            align=lambda end: resolve_end(section, end, symbols,
                                          symbol_starts, claim_starts))
        row["evidence"] = {"0x%08X" % base: {
            "support": entry["support"], "symbols": entry["symbols"],
            "dol_percent": (entry["dol"] or {}).get("percent"),
            "dol_compared": (entry["dol"] or {}).get("compared"),
            "dol_note": ("bss: no DOL bytes exist to compare" if bss
                         else (entry["dol"] or {}).get("note"))}
            for base, entry in scored.items()}
        result["sections"].append(row)
    # A section whose evidence does not decide is a MEASURED verdict, not an
    # unmeasured unit: it is counted and named, but it does not make the
    # census itself unresolved. Only a unit that could not be read at all
    # (missing object, unreadable range) does that, above.
    result["undecided_sections"] = sum(
        1 for row in result["sections"] if row["verdict"].startswith("unresolved"))
    if defined is not None:
        result["orphan_externs"] = orphan_externs(ours, defined, symbols)
    return result


def _elect(scored, bss):
    """The single winning base, or None when the evidence does not decide.

    For a byte-bearing section the DOL decides: exactly one candidate must
    be fully byte-equal. When none is, the best-scoring one is returned so
    the row can report HOW WRONG it is -- but `section_result` will still
    refuse to call it claimable. Only a tie between equally-supported
    non-equal candidates is a true `unresolved-disagreement`.

    For bss there are no bytes at all, so relocation consensus is the only
    evidence: one candidate must be supported by at least two distinct
    symbols and by strictly more of them than every other candidate
    combined.
    """
    if not scored:
        return None
    if bss:
        ranked = sorted(scored.items(), key=lambda row: -row[1]["support"])
        top, support = ranked[0][0], ranked[0][1]["support"]
        others = sum(entry["support"] for base, entry in ranked[1:])
        return top if support >= 2 and support > others else None
    exact = [base for base, entry in scored.items()
             if (entry["dol"] or {}).get("percent") == 100.0
             and (entry["dol"] or {}).get("compared")]
    if len(exact) == 1:
        return exact[0]
    if len(exact) > 1:
        return None
    ranked = sorted(scored.items(),
                    key=lambda row: (-((row[1]["dol"] or {}).get("percent")
                                       or -1.0), -row[1]["support"]))
    if len(ranked) > 1:
        first, second = ranked[0][1], ranked[1][1]
        if ((first["dol"] or {}).get("percent")
                == (second["dol"] or {}).get("percent")
                and first["support"] == second["support"]):
            return None
    return ranked[0][0]


# --------------------------------------------------------------------------
# CLI


def render(result):
    print("=== %s ===" % result["unit"])
    if result.get("error"):
        print("  UNRESOLVED: %s" % result["error"])
        return
    if result.get("note"):
        print("  %s" % result["note"])
    pairing = result.get("pairing")
    if pairing:
        print("  relocation pairing: %d paired, %d unpaired, %d offset-shifted,"
              " %d target symbol(s) unresolved"
              % (pairing["paired"], pairing["unpaired"],
                 pairing["offset_shifted"], pairing["unresolved_target"]))
    for row in result["sections"]:
        print("  %-8s ours 0x%X  %s" % (row["section"], row["ours_size"],
                                        row["verdict"].upper()))
        print("      %s" % row["reason"])
        for base, entry in sorted(row.get("evidence", {}).items()):
            percent = entry["dol_percent"]
            print("      candidate %s  support %d  DOL %s  from %s"
                  % (base, entry["support"],
                     "%.1f%% of %d compared word(s)"
                     % (percent, entry["dol_compared"]) if percent is not None
                     else "n/a - %s" % (entry.get("dol_note")
                                        or "not measured"),
                     ", ".join(entry["symbols"][:5]) or "-"))
        if row.get("end_note"):
            print("      NOTE: %s" % row["end_note"])
        if row["verdict"] in ("claimable", "claimable-bss"):
            print("      PASTE:\t%-11s start:%s end:%s"
                  % (row["section"], row["claim_start"], row["claim_end"]))
            print("      THEN VERIFY: python tools/gdl/datadiff.py --sections"
                  " %s   (a base is not ownership)" % result["unit"])
    for entry in result.get("orphan_externs", []):
        print("  orphan extern %-28s %s %s size 0x%X"
              % (entry["name"], entry["section"], entry["address"] or "-",
                 entry["size"] or 0))


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__.split("\n")[0],
        formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    parser.add_argument("units", nargs="*", help="unit keys; default is every"
                        " NonMatching unit in configure.py")
    parser.add_argument("--all", action="store_true",
                        help="census Matching units too")
    parser.add_argument("--splits", type=Path,
                        default=REPO / "config" / VERSION / "splits.txt",
                        help="splits.txt to read claims from (calibration)")
    parser.add_argument("--configure", type=Path, default=REPO / "configure.py")
    parser.add_argument("--top", type=int, default=10,
                        help="how many units to list in the ranking")
    parser.add_argument("--no-externs", action="store_true",
                        help="skip the orphan-extern screen (much faster)")
    parser.add_argument("--out", type=Path, help="write the JSON census here")
    options = parser.parse_args(argv)

    splits = parse_splits(options.splits)
    intervals = claimed_intervals(splits)
    if options.units:
        units = [fndiff.unit_key(unit) for unit in options.units]
    else:
        units = configured_units(options.configure,
                                 matching=None if options.all else False)
        known = {key.rsplit(".", 1)[0] for key in splits}
        units = [unit for unit in units if unit in known]
    if not units:
        print("no unit selected: configure.py lists none of the requested"
              " class with a splits.txt block (this is a verdict, not"
              " silence)")
        return 2
    defined = None if options.no_externs else defined_symbols()

    results = [census(unit, splits, intervals, defined) for unit in units]
    for result in results:
        render(result)

    ranking = rank_units(results)
    print("\nCLAIMABLE-BYTE RANKING (%d unit(s) with anything to claim, of %d"
          " censused)" % (len(ranking), len(results)))
    print("  %-34s %10s %10s %10s" % ("unit", "claimable", "bss", "blocked"))
    for unit, claimable, bss, blocked in ranking[:max(options.top, 0)]:
        print("  %-34s %10d %10d %10d" % (unit, claimable, bss, blocked))
    verdicts = {}
    for result in results:
        for row in result["sections"]:
            verdicts[row["verdict"]] = verdicts.get(row["verdict"], 0) + 1
    print("  section verdicts: %s" % (", ".join(
        "%s=%d" % pair for pair in sorted(verdicts.items())) or "none"))
    undecided = [r["unit"] for r in results if r.get("undecided_sections")]
    if undecided:
        print("  units with at least one UNDECIDED section: %d - %s"
              % (len(undecided), ", ".join(undecided[:12])))
    unresolved = [r["unit"] for r in results if r["status"] == "UNRESOLVED"]
    if unresolved:
        print("  UNMEASURED unit(s): %d - %s  (NO census was made for them;"
              " this is a refusal, not a clean result)"
              % (len(unresolved), ", ".join(unresolved[:12])))

    if options.out:
        path = options.out if options.out.is_absolute() else REPO / options.out
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(
            {"schema_version": 1, "scope": "unclaimed object data sections;"
             " candidate extents by relocation resolution and DOL byte"
             " comparison, NOT proof of target ownership or link placement",
             "splits": str(options.splits), "units": results,
             "ranking": [{"unit": u, "claimable_bytes": c, "bss_bytes": b,
                          "blocked_bytes": k} for u, c, b, k in ranking]},
            indent=1) + "\n", encoding="utf-8")
        print("wrote %s" % path)
    return 2 if unresolved else 0


if __name__ == "__main__":
    sys.exit(main())
