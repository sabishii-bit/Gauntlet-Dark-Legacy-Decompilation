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

That vote was WRONG for four sections three run-62 lanes checked by hand
(player `.rodata` 0x80113E54 for 0x80113E28, options `.rodata` 0x80113A40
for 0x80113A0C, options `.sdata2` 0x80347598 over a base that misaligns
every f64, controls `.rodata` 0x80111FB8 for 0x80111F70), so four things now
run before it:

  1. `object_symbols` can see MWCC's section-base local (`...rodata.0`). Its
     `objdump -t` row carries a `l      ` flag field with no `O`, which the
     old whitespace-split pattern dropped -- and that symbol is the anchor
     the whole pool-base rule reads.
  2. `pool_base_sites` DERIVES the base rather than voting; see its
     docstring for the anchor and datum forms and why the minimum is the
     binding.
  3. `proven_alignment` and `claim_containing` REMOVE candidates that cannot
     be a base: one that breaks the alignment this section's own padding
     proves, and one inside another unit's claimed run. Both are reported.
  4. With no anchor, `gap_inventory` decides: the right base explains every
     one of our words once DOL-side insertions are allowed, and a merely
     popular one does not.

SHORT AT THE FRONT, IN THE MIDDLE, OR BOTH
------------------------------------------
Our object can emit FEWER bytes than the target section, missing datums at
the FRONT (an `extern` placeholder still standing in for a const the source
has not recovered) and INSIDE the run. Which of the two happened is
measured, not assumed: the front deficit from the pool-base rule (or, with
no anchor, the symbols.txt boundary) and the interior gaps from the resync.
Run 62 reported controls `.rodata` "short at the FRONT" when its first 0x7C
bytes were byte-identical and its whole deficit was one 0x48 hole at +0x7C.

A raw DOL percentage is NOT a base score for such a section: at the right
base every datum after the first omission is compared against the wrong
target datum. Player `.rodata` measures 14.4% at the true base and 53.8%
0x2C above it, and resyncs 131/131 at the true base and 114/131 at the other
one. Every row therefore prints its gap inventory with each gap's address,
size and DOL content.

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

IMPORTABLE CORE: candidate_bases, pool_base_sites, proven_alignment,
claim_containing, gap_inventory, describe_bytes, elect_base, score_base,
boundary_of, section_result, rank_units -- pure over rows/bytes already
read, no subprocess, no build.
"""

import argparse
import json
import re
import struct
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


#: `objdump -t`: 8 hex value, space, a SEVEN-character flag field, space,
#: section, TAB, 8 hex size, space, name. The flag field is padded with
#: spaces, so splitting on whitespace loses the rows whose flags are only
#: `l` -- and those are exactly MWCC's zero-size section-base locals
#: (`...rodata.0`, `...data.0`), the anchor the pool-base rule reads.
SYMBOL_ROW = re.compile(r"^([0-9a-f]{8}) (.{7}) (\S+)\s+"
                        r"([0-9a-f]{8})\s+(.*)$")
#: MWCC's per-section base local: a zero-size symbol at section offset 0 that
#: every pooled address in that section is reached from.
SECTION_BASE = re.compile(r"^\.\.\.(\w+)\.0$")


def object_symbols(path):
    """name -> (section, value, size) for symbols defined in this object."""
    table = {}
    for line in _dump(path, "-t").splitlines():
        row = SYMBOL_ROW.match(line)
        if row and row.group(3) not in ("*UND*", "*ABS*"):
            table[row.group(5).strip()] = (row.group(3), int(row.group(1), 16),
                                           int(row.group(4), 16))
    return table


def section_base_name(section):
    """The local MWCC emits at offset 0 of `section`, e.g. `...rodata.0`."""
    return "...%s.0" % section.lstrip(".")


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


def data_relocation_rows(path):
    """{section: [(offset, type, symbol, addend)]} from `objdump -r`.

    `relocation_rows` reads `-dr`, which only covers code. A pointer table in
    `.data` that stores a literal's address carries its relocation here and
    nowhere else, and those rows are the cleanest pool-base evidence there
    is: data layout can be identical between the two objects even when the
    instruction streams are not.
    """
    table, section = {}, None
    for line in _dump(path, "-r").splitlines():
        head = re.match(r"^RELOCATION RECORDS FOR \[(\S+)\]", line)
        if head:
            section = head.group(1)
            table.setdefault(section, [])
            continue
        row = re.match(r"^([0-9a-f]{8})\s+(R_PPC\S+)\s+(\S+)$", line)
        if row and section:
            symbol, addend = row.group(3), 0
            for separator, sign in (("+0x", 1), ("-0x", -1)):
                if separator in symbol:
                    symbol, tail = symbol.split(separator, 1)
                    addend = sign * int(tail, 16)
                    break
            table[section].append((int(row.group(1), 16), row.group(2),
                                   symbol, addend))
    return table


INSTRUCTION_HEAD = re.compile(r"^([0-9a-f]+) <(.+)>:$")
INSTRUCTION_WORD = re.compile(r"^\s*([0-9a-f]+):\t((?:[0-9a-f]{2} ){4})\t")
INSTRUCTION_RELOC = re.compile(r"^\s*([0-9a-f]+): (R_PPC\S+)\s+(\S+)$")


def instruction_map(path):
    """{function: {function-relative offset: (word, relocation symbol)}}.

    The word is needed because the pool-base rule's discriminant is an
    IMMEDIATE, not a relocation: MWCC materialises the section base once and
    then reaches each datum with a plain `addi rD, rBase, <offset>` that
    carries no relocation at all.
    """
    out, function, start = {}, None, 0
    for line in _dump(path, "-dr").splitlines():
        head = INSTRUCTION_HEAD.match(line)
        if head:
            function, start = head.group(2), int(head.group(1), 16)
            out[function] = {}
            continue
        if function is None:
            continue
        word = INSTRUCTION_WORD.match(line)
        if word:
            out[function][int(word.group(1), 16) - start] = \
                [int(word.group(2).replace(" ", ""), 16), None]
            continue
        relocation = INSTRUCTION_RELOC.match(line)
        if relocation:
            at = (int(relocation.group(1), 16) - start) & ~3
            if at in out[function]:
                out[function][at][1] = relocation.group(3)
    return out


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

    Rows naming a SECTION-BASE local (`...rodata.0`) are excluded and counted
    separately: in `.text` that symbol is a CSE anchor for the whole section,
    so the pair resolves the TARGET section's offset 0 rather than where our
    bytes land, and feeding it to the vote as if the two were the same thing
    is how player `.rodata` came to report a base 0x348 away from the truth.
    `pool_base_sites` owns those rows.
    """
    target = {(fn, off): (kind, sym, add)
              for fn, off, kind, sym, add in target_rows}
    symbols = fndiff.symbol_table()
    bases, stats = {}, {"paired": 0, "unpaired": 0, "offset_shifted": 0,
                        "unresolved_target": 0, "section_base_rows": 0}
    for fn, off, kind, sym, add in ours_rows:
        section, value, _size = our_symbols.get(sym, (None, None, None))
        if section not in sections:
            continue
        if SECTION_BASE.match(sym):
            stats["section_base_rows"] += 1
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


def resolve_target_symbol(symbol, addend, symbols):
    """The address `symbol + addend` denotes, or None when it is unknown."""
    literal = re.match(r"^lbl_([0-9A-Fa-f]{8})$", symbol)
    if literal:
        return int(literal.group(1), 16) + addend
    entry = symbols.get(symbol)
    address = entry[1] if isinstance(entry, tuple) and len(entry) >= 2 \
        else entry
    return address + addend if isinstance(address, int) else None


def _addi(word):
    """(rD, rA, signed immediate) when `word` is an addi, else None."""
    if word is None or word >> 26 != 14:
        return None
    immediate = word & 0xFFFF
    return ((word >> 21) & 31, (word >> 16) & 31,
            immediate - 0x10000 if immediate & 0x8000 else immediate)


def pool_base_sites(section, ours_data, target_data, ours_text, target_text,
                    symbols=None):
    """The POOL-BASE rule: where our section binds, derived exactly.

    MWCC gives every section a zero-size local at offset 0 (`...rodata.0`)
    and reaches the section's contents from it. Two forms carry that anchor
    into the relocations, and they mean DIFFERENT things:

    ANCHOR form (`.text`). `lis/addi rBase, ...rodata.0` against the
    target's `lis/addi rBase, <its own base symbol>`, then each datum is
    reached by a plain `addi rD, rBase, <offset in section>`. The paired
    relocation gives the TARGET SECTION's offset 0 -- not where OUR bytes
    land, because a short-at-front section's offset 0 is a different datum.
    The consuming immediates give that: for a datum our object places at
    `O` and the target at `T`, our section offset 0 binds at
    `section_base + T - O`.

    DATUM form (any section). A stored pointer relocates against
    `...rodata.0 + A`, i.e. our section offset `A` directly, and the target
    row at the same (section, offset) names the datum's real address. Then
    our offset 0 binds at `target_address - A`.

    Both yield one candidate per site. Our section is an ordered SUBSEQUENCE
    of the target's -- omissions only, never reordering -- so a site with
    `k` DOL-side bytes inserted before it derives a base `k` too high, and
    the MINIMUM over the sites is the binding, exact as soon as one site
    precedes every insertion. The spread between sites is the interior
    deficit and is reported, never averaged away.

    Inputs are the prepared reads, so this stays pure and one objdump pass
    per object serves every section: `ours_data`/`target_data` from
    `data_relocation_rows`, `ours_text`/`target_text` from `instruction_map`.

    Returns {"bind_base", "section_base", "sites", "candidate_bases",
    "skipped", "note"} with `bind_base` None when nothing binds.
    """
    symbols = fndiff.symbol_table() if symbols is None else symbols
    anchor = section_base_name(section)
    sites, section_bases, skipped = [], set(), []

    for where, rows in ours_data.items():
        if where == ".text":
            continue
        paired = {offset: (kind, symbol, addend)
                  for offset, kind, symbol, addend
                  in target_data.get(where, ())}
        for offset, kind, symbol, addend in rows:
            if symbol != anchor:
                continue
            hit = paired.get(offset)
            if hit is None or hit[0] != kind:
                skipped.append("%s+0x%X: no paired target relocation"
                               % (where, offset))
                continue
            address = resolve_target_symbol(hit[1], hit[2], symbols)
            if address is None:
                skipped.append("%s+0x%X: target symbol %s unresolved"
                               % (where, offset, hit[1]))
                continue
            sites.append({"form": "datum", "our_offset": addend,
                          "target_address": address,
                          "base": address - addend,
                          "where": "%s+0x%X" % (where, offset)})

    for function, words in sorted(ours_text.items()):
        theirs = target_text.get(function)
        if theirs is None:
            continue
        for at in sorted(words):
            word, symbol = words[at]
            if symbol != anchor:
                continue
            target_word, target_symbol = theirs.get(at, (None, None))
            if target_symbol is None:
                skipped.append("%s+0x%X: no paired target base relocation"
                               % (function, at))
                continue
            ours_form, target_form = _addi(word), _addi(target_word)
            if not ours_form or not target_form:
                continue                # the HA half of the pair, not the addi
            address = resolve_target_symbol(target_symbol, 0, symbols)
            if address is None:
                skipped.append("%s+0x%X: target base symbol %s unresolved"
                               % (function, at, target_symbol))
                continue
            section_bases.add(address)
            if ours_form[0] != target_form[0]:
                skipped.append(
                    "%s+0x%X: base register differs (ours r%d, target r%d);"
                    " the consuming immediates cannot be paired"
                    % (function, at, ours_form[0], target_form[0]))
                continue
            register = ours_form[0]
            for later in sorted(words):
                if later <= at:
                    continue
                our_later, our_relocation = words[later]
                their_later, their_relocation = theirs.get(later, (None, None))
                if our_relocation or their_relocation or their_later is None:
                    continue
                mine, theirs_form = _addi(our_later), _addi(their_later)
                if not mine or not theirs_form:
                    continue
                if mine[1] != register or theirs_form[1] != register:
                    continue
                if mine[0] != theirs_form[0]:
                    continue
                sites.append({"form": "anchor", "our_offset": mine[2],
                              "target_address": address + theirs_form[2],
                              "base": address + theirs_form[2] - mine[2],
                              "where": "%s+0x%X" % (function, later)})

    bases = sorted({site["base"] for site in sites})
    result = {"bind_base": bases[0] if bases else None,
              "section_base": min(section_bases) if section_bases else None,
              "sites": sorted(sites, key=lambda site: (site["our_offset"],
                                                       site["where"])),
              "candidate_bases": bases, "skipped": skipped}
    if not bases:
        result["note"] = ("no relocation in this object reaches %s through"
                          " %s, so the pool-base rule says nothing here"
                          % (section, anchor))
    else:
        result["note"] = (
            "%d site(s) through %s derive %d distinct base(s) %s; the lowest"
            " is the binding and the spread is the DOL-side insertion(s)"
            " between them"
            % (len(sites), anchor, len(bases),
               ", ".join("0x%08X" % base for base in bases)))
    return result


def proven_alignment(our_symbols, section):
    """The largest power of two a datum in `section` is PROVEN to require.

    NOT the section header's alignment: MWCC marks `.rodata` 2**3 whether or
    not anything in it needs eight, so believing the header would reject
    correct bases. This reads PADDING instead -- a datum placed further out
    than the next smaller alignment would have put it proves the larger
    requirement. options.c `.sdata2` proves 8 from its `4330000080000000`
    magic at +0x68 behind a symbol ending at +0x63; its `.rodata`, whose
    every datum is a string, proves only 2.

    A base that does not satisfy this cannot be where our bytes land: the
    same datum would be misaligned in the DOL.
    """
    rows = sorted((value, size) for _name, (where, value, size)
                  in our_symbols.items() if where == section)
    best, previous_end, seen = 1, 0, False
    for value, size in rows:
        # The FIRST datum proves nothing: whatever sits before it is not a
        # symbol we can see, so its offset is not evidence of an alignment
        # requirement. Only a gap between two known datums is.
        for align in (16, 8, 4, 2) if seen else ():
            if align <= best:
                break
            half = align // 2
            if value % align == 0 and \
                    ((previous_end + half - 1) & ~(half - 1)) < value:
                best = align
                break
        previous_end = max(previous_end, value + size)
        seen = True
    return best


def claim_containing(section, address, intervals, exclude=None):
    """The existing claim `address` falls inside, or None.

    A base inside another unit's claimed run cannot be where our section
    starts, whatever the relocation vote says. `overlapping_claim` already
    refuses such an EXTENT, but only after the election, so a candidate that
    is impossible on its face could still win and be reported with a
    confident percentage.
    """
    for start, end, unit in intervals.get(section, ()):
        if unit == exclude:
            continue
        if start <= address < end:
            return {"unit": unit, "start": start, "end": end}
    return None


def describe_bytes(data, limit=48):
    """DOL bytes as the strings/floats they are, for a gap inventory row.

    Printable runs are found without requiring a NUL inside the window: a
    gap that ends mid-string (controls' `print_edges` format) is exactly the
    case a reader most needs spelled out, and hex is useless there.
    """
    if not data:
        return ""
    text, run = [], bytearray()
    for byte in data + b"\x00":
        if 0x20 <= byte < 0x7F:
            run.append(byte)
            continue
        if len(run) >= 4:
            text.append('"%s"' % run.decode("latin1"))
        run = bytearray()
    if text:
        return " ".join(text)[:limit * 3]
    if len(data) == 4:
        return "f32 %g / 0x%s" % (struct.unpack(">f", data)[0], data.hex())
    if len(data) == 8:
        return "f64 %g / 0x%s" % (struct.unpack(">d", data)[0], data.hex())
    return "0x" + data[:limit].hex() + ("..." if len(data) > limit else "")


def gap_inventory(our_bytes, relocated, dol_bytes, base, run=3, max_gap=256):
    """Align our section to the DOL allowing DOL-SIDE INSERTIONS.

    Promoted from lane I's `build/i_lane/i_resync.py`. A short section's raw
    agreement percentage is not a base score: at the RIGHT base, every datum
    after the first omission is compared against the wrong target datum and
    the percentage collapses, while a wrong base that happens to line the
    bulk of the run up scores higher. Player `.rodata` measured 14.4% at the
    true base and 53.8% at one 0x2C above it. Resynchronising after each
    insertion is what makes the two readable side by side, so this reports
    `resynced_equal` next to `equal`, and every gap with its DOL content.

    Words carrying a relocation in our object have no value before the link;
    they are counted as `relocated` and never used to justify a resync.
    """
    words = len(our_bytes) // 4
    available = len(dol_bytes) // 4 if dol_bytes else 0
    state = {"base": base, "words": words, "dol_words": available,
             "equal": 0, "relocated": 0, "mismatched": 0, "gaps": [],
             "resynced_equal": 0, "truncated": available < words}

    def same(index, target_index):
        return our_bytes[index * 4:index * 4 + 4] == \
            dol_bytes[target_index * 4:target_index * 4 + 4]

    index = target = 0
    while index < words and target < available:
        if index * 4 in relocated:
            state["relocated"] += 1
            index += 1
            target += 1
            continue
        if same(index, target):
            state["equal"] += 1
            state["resynced_equal"] += 1
            index += 1
            target += 1
            continue
        found = None
        for gap in range(1, max_gap + 1):
            if target + gap + run > available or index + run > words:
                break
            if all((index + step) * 4 in relocated
                   for step in range(run)):
                continue
            if all(same(index + step, target + gap + step)
                   for step in range(run)):
                found = gap
                break
        if found is not None:
            state["gaps"].append({
                "our_offset": index * 4,
                "address": base + target * 4,
                "size": found * 4,
                "content": describe_bytes(
                    bytes(dol_bytes[target * 4:(target + found) * 4]))})
            target += found
            continue
        state["mismatched"] += 1
        index += 1
        target += 1
    state["gap_bytes"] = sum(gap["size"] for gap in state["gaps"])
    state["tail_address"] = base + target * 4
    # A gap BEFORE our first word is not a missing datum in the middle of the
    # run: it says the base itself is that many bytes too low. Reporting it
    # as an interior gap would hide a base correction inside an inventory.
    leading = state["gaps"][0] if state["gaps"] \
        and state["gaps"][0]["our_offset"] == 0 else None
    state["leading_gap"] = leading["size"] if leading else 0
    state["implied_base"] = base + (leading["size"] if leading else 0)
    return state


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
                   straddle, collision, bss, prior=None, align=None,
                   inventory=None, front_deficit=None):
    """The verdict for one claimable section. Pure; every input measured.

    Verdicts, in refusal order:
      unresolved-no-base       nothing in .text binds into this section
      unresolved-disagreement  no candidate wins on measured DOL bytes
      conflict-existing-claim  the extent collides with another unit
      blocked-short-at-front   bytes missing before our section's first datum
      blocked-short-in-middle  bytes missing only INSIDE the run
      blocked-short-front-and-middle   both, with each byte count measured
      blocked-bytes-differ     words still differ after resynchronising
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
    if front_deficit is not None:
        row["front_deficit"] = front_deficit
    if inventory is not None:
        row["inventory"] = inventory
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
    front = row.get("front_deficit")
    if front is None and boundary["kind"] == "inside":
        front = boundary["front_gap"]
        row["front_gap"] = boundary["front_gap"]
    if inventory and not inventory["mismatched"] \
            and (front or inventory["gaps"]):
        # WHERE a section is short is a measurement, not an assumption. Run
        # 62's controls `.rodata` was reported "short at the FRONT" when its
        # first 0x7C bytes were byte-identical and the whole deficit was one
        # 72-byte hole at +0x7C (lane H). The front deficit comes from the
        # pool-base rule or the symbol boundary; the interior gaps come from
        # the resync; the verdict names whichever actually happened.
        middle = bool(inventory["gaps"])
        row["verdict"] = ("blocked-short-front-and-middle" if front and middle
                          else "blocked-short-at-front" if front
                          else "blocked-short-in-middle")
        row["reason"] = (
            "every one of our %d word(s) equals the DOL once %d DOL-side"
            " insertion(s) are allowed, so the base is right and the source"
            " is incomplete: 0x%X byte(s) missing at the FRONT and 0x%X in"
            " %d interior gap(s). Recover the missing datum(s) -- see this"
            " unit's orphan externs and the inventory below -- and re-run."
            % (inventory["resynced_equal"], len(inventory["gaps"]),
               front or 0, inventory["gap_bytes"], len(inventory["gaps"])))
        return row
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
                     " best candidate base%s%s"
                     % (dol.get("equal"), dol.get("compared"),
                        "" if not dol.get("first_difference") else
                        "; first at +0x%X (ours %s vs dol %s)"
                        % (dol["first_difference"]["offset"],
                           dol["first_difference"]["ours"],
                           dol["first_difference"]["dol"]),
                        "" if not inventory else
                        "; %d word(s) still differ after allowing %d DOL-side"
                        " insertion(s), so this is not a short section at a"
                        " right base" % (inventory["mismatched"],
                                         len(inventory["gaps"]))))
    return row


#: A resync needs enough words behind it to mean anything: three equal words
#: out of five prove nothing about a base.
RESYNC_FLOOR = 8


def elect_base(scored, bss, pool=None, alignment=1, claim_probe=None):
    """(base, decision): the guards, then the POOL-BASE rule, then the vote.

    Three lanes measured the same defect in run 62 -- the support-ranked vote
    picked a base our own object cannot bind at, and then reported a
    confident DOL percentage for it (player `.rodata` 0x80113E54 for
    0x80113E28, options `.sdata2` 0x80347598 over a candidate that misaligns
    every f64, controls `.rodata` 0x80111FB8 for 0x80111F70). So:

      1. Candidates that CANNOT be a base are removed before any ranking:
         one that breaks the section's own proven datum alignment, and one
         that lies inside another unit's claimed run. Both are reported with
         the reason, never dropped silently.
      2. `pool_base_sites`' derivation wins over the vote when it has one:
         it is arithmetic on a paired relocation plus an immediate, not a
         popularity contest. When a guard refuses the pool base itself that
         is reported and the vote decides -- a disagreement of that shape is
         evidence about the model, not something to paper over.
      3. Otherwise the surviving candidates go to the existing vote.
    """
    rejected, survivors = {}, {}
    for base, entry in scored.items():
        if alignment > 1 and base % alignment:
            rejected[base] = (
                "0x%08X is not %d-byte aligned, and this section places a"
                " datum whose padding PROVES a %d-byte requirement: the same"
                " datum would be misaligned in the DOL"
                % (base, alignment, alignment))
            continue
        hit = claim_probe(base) if claim_probe else None
        if hit:
            rejected[base] = (
                "0x%08X lies inside %s's claimed run 0x%08X..0x%08X, so it"
                " cannot be where this unit's section starts"
                % (base, hit["unit"], hit["start"], hit["end"]))
            continue
        survivors[base] = entry
    vote = _elect(survivors, bss)
    decision = {"rejected": {"0x%08X" % base: reason
                             for base, reason in rejected.items()},
                "alignment": alignment, "vote_base": vote,
                "source": "relocation-vote"}
    pool_base = (pool or {}).get("bind_base")
    if pool_base is not None:
        if pool_base in rejected:
            decision["pool_base_rejected"] = rejected[pool_base]
            decision["pool_base"] = "0x%08X" % pool_base
        else:
            decision["source"] = "pool-base"
            decision["pool_base"] = "0x%08X" % pool_base
            return pool_base, decision
    # No pool anchor. A base is RIGHT when every one of our words is
    # explained by it once DOL-side insertions are allowed; a base that is
    # merely popular is not. options `.rodata`'s vote picked 0x80113A40 (10
    # words differ after resync) over 0x80113A0C (none), which is the address
    # lane J proved from the DOL bytes.
    clean = [base for base, entry in survivors.items()
             if (entry.get("inventory") or {}).get("mismatched") == 0
             and not (entry.get("inventory") or {}).get("leading_gap")
             and (entry.get("inventory") or {}).get("words", 0) >= RESYNC_FLOOR]
    decision["clean_resync"] = ["0x%08X" % base for base in sorted(clean)]
    if len(clean) == 1 and not bss:
        decision["source"] = "clean-resync"
        return clean[0], decision
    return vote, decision


def derive_base(section, ours_object, target_object, intervals=None,
                unit=None, symbols=None):
    """The full base derivation for ONE section, for a sibling tool.

    Does its own objdump reads, so it is convenient rather than cheap: use
    `census` for a sweep. It exists so `pool_owner.py` can quote the SAME
    answer this census prints instead of forming a second opinion -- a
    `--range` hypothesis and the census disagreeing silently is exactly the
    failure run 62 spent a lane on.

    Returns {"base", "decision", "pool", "alignment"}; `base` is None when
    nothing decides.
    """
    symbols = fndiff.symbol_table() if symbols is None else symbols
    intervals = intervals or {}
    our_symbols = object_symbols(ours_object)
    pool = pool_base_sites(section,
                           data_relocation_rows(ours_object),
                           data_relocation_rows(target_object),
                           instruction_map(ours_object),
                           instruction_map(target_object), symbols)
    bases, _stats = candidate_bases(relocation_rows(ours_object),
                                    relocation_rows(target_object),
                                    our_symbols)
    found = dict(bases.get(section, {}))
    if pool["bind_base"] is not None:
        found.setdefault(pool["bind_base"],
                         {"symbols": [section_base_name(section)],
                          "targets": []})
    payload = b"" if section in BSS_SECTIONS \
        else section_bytes(ours_object, section)
    relocated = relocated_offsets(ours_object).get(section, set())
    scored = {}
    for base in found:
        window = fndiff.dol_read(base, len(payload) + max(len(payload), 0x400))
        scored[base] = {
            "support": len(found[base]["symbols"]),
            "dissent": len(found) - 1,
            "dol": score_base(payload, relocated,
                              fndiff.dol_read(base, len(payload))),
            "inventory": None if window is None
            else gap_inventory(payload, relocated, window, base),
            "symbols": found[base]["symbols"][:8],
            "targets": found[base]["targets"][:8]}
    alignment = proven_alignment(our_symbols, section)
    best, decision = elect_base(
        scored, section in BSS_SECTIONS, pool, alignment,
        lambda base: claim_containing(section, base, intervals,
                                      exclude=(unit + ".c") if unit else None))
    return {"base": best, "decision": decision, "pool": pool,
            "alignment": alignment}


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
    our_symbols = object_symbols(ours)
    ours_data = data_relocation_rows(ours)
    target_data = data_relocation_rows(target)
    ours_text = instruction_map(ours)
    target_text = instruction_map(target)
    for section, prior in unclaimed:
        size = sizes[section]
        bss = section in BSS_SECTIONS
        found = dict(bases.get(section, {}))
        payload = b"" if bss else section_bytes(ours, section)
        pool = pool_base_sites(section, ours_data, target_data, ours_text,
                               target_text, symbols)
        if pool["bind_base"] is not None and pool["bind_base"] not in found:
            found[pool["bind_base"]] = {
                "symbols": [section_base_name(section)],
                "targets": sorted({"0x%08X" % site["target_address"]
                                   for site in pool["sites"]})[:8]}
        scored = {}
        for base, evidence in found.items():
            dol = None if bss else score_base(
                payload, relocated.get(section, set()),
                fndiff.dol_read(base, size))
            window = None if bss else fndiff.dol_read(
                base, size + max(size, 0x400))
            scored[base] = {"support": len(evidence["symbols"]),
                            "dissent": len(found) - 1, "dol": dol,
                            "inventory": None if window is None else
                            gap_inventory(payload,
                                          relocated.get(section, set()),
                                          window, base),
                            "symbols": evidence["symbols"][:8],
                            "targets": evidence["targets"][:8]}
        alignment = 1 if bss else proven_alignment(our_symbols, section)
        best, decision = elect_base(
            scored, bss, pool, alignment,
            lambda base: claim_containing(section, base, intervals,
                                          exclude=unit + ".c"))
        boundary = boundary_of(section, best, symbols) if best is not None \
            else None
        straddle = straddled_end(section, best + size, symbols) \
            if best is not None else None
        symbol_starts = {entry[1] for entry in symbols.values()
                         if isinstance(entry, tuple) and entry[0] == section}
        claim_starts = {start for start, _end, owner
                        in intervals.get(section, ()) if owner != unit + ".c"}
        front_deficit = None
        if pool["section_base"] is not None and best is not None:
            front_deficit = best - pool["section_base"]
        # The gap inventory is what makes two bases' percentages comparable,
        # and it is what decides FRONT versus MIDDLE below: see
        # `gap_inventory`.
        inventory = (scored.get(best) or {}).get("inventory")
        row = section_result(
            section, size, found, best, scored.get(best, {}), boundary,
            straddle,
            lambda lo, hi: overlapping_claim(section, lo, hi, intervals,
                                             exclude=unit + ".c"),
            bss, prior,
            align=lambda end: resolve_end(section, end, symbols,
                                          symbol_starts, claim_starts),
            inventory=inventory, front_deficit=front_deficit)
        row["evidence"] = {"0x%08X" % base: {
            "support": entry["support"], "symbols": entry["symbols"],
            "dol_percent": (entry["dol"] or {}).get("percent"),
            "dol_compared": (entry["dol"] or {}).get("compared"),
            "dol_note": ("bss: no DOL bytes exist to compare" if bss
                         else (entry["dol"] or {}).get("note")),
            "resync_mismatched": (entry["inventory"] or {}).get("mismatched"),
            "resync_gaps": len((entry["inventory"] or {}).get("gaps", [])),
            "resync_leading_gap": (entry["inventory"] or {}).get("leading_gap")}
            for base, entry in scored.items()}
        row["base_decision"] = decision
        row["pool_base"] = {
            "bind_base": None if pool["bind_base"] is None
            else "0x%08X" % pool["bind_base"],
            "section_base": None if pool["section_base"] is None
            else "0x%08X" % pool["section_base"],
            "sites": len(pool["sites"]),
            "derived_bases": ["0x%08X" % base
                              for base in pool["candidate_bases"]],
            "note": pool["note"], "skipped": pool["skipped"][:8]}
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


def render_base_decision(row):
    """How the base was chosen, and what the pool-base rule measured."""
    decision = row.get("base_decision") or {}
    pool = row.get("pool_base") or {}
    vote = decision.get("vote_base")
    if decision.get("source") == "pool-base":
        print("      base %s by the POOL-BASE rule; the relocation vote %s"
              % (decision.get("pool_base"),
                 "had none" if vote is None else "said 0x%08X" % vote))
    elif decision.get("source") == "clean-resync":
        print("      base %s: the only candidate every one of our words"
              " resyncs against; the relocation vote %s"
              % (row.get("base"),
                 "had none" if vote is None else "said 0x%08X" % vote))
    if decision.get("pool_base_rejected"):
        print("      POOL-BASE %s REFUSED: %s   (the vote decided instead;"
              " this disagreement is evidence, not noise)"
              % (decision.get("pool_base"), decision["pool_base_rejected"]))
    if pool.get("section_base"):
        print("      target section base %s (its own `%s` anchor); our"
              " object is short 0x%X at the FRONT"
              % (pool["section_base"], section_base_name(row["section"]),
                 row.get("front_deficit", 0)))
    if pool.get("sites"):
        print("      pool-base evidence: %s" % pool["note"])
    for text in pool.get("skipped", [])[:4]:
        print("      pool-base site skipped: %s" % text)
    if decision.get("alignment", 1) > 1:
        print("      this section's own padding proves a %d-byte datum"
              " alignment, so a base must be %d-aligned"
              % (decision["alignment"], decision["alignment"]))


def render_inventory(row):
    """The DOL-side gap inventory at the chosen base."""
    inventory = row.get("inventory")
    if not inventory:
        return
    print("      DOL INVENTORY at %s: %d/%d word(s) equal after resync,"
          " %d mismatched, %d relocated (not comparable), %d DOL-side gap(s)"
          " totalling 0x%X byte(s)"
          % (row.get("base", "?"), inventory["resynced_equal"],
             inventory["words"], inventory["mismatched"],
             inventory["relocated"], len(inventory["gaps"]),
             inventory["gap_bytes"]))
    if inventory["gaps"]:
        print("      a raw DOL percentage compares our datum N against the"
              " target's datum N; with these insertions present that number"
              " is NOT a base score, and a higher one at another base means"
              " the wrong datums happened to line up")
    for gap in inventory["gaps"][:24]:
        print("        GAP after our +0x%04X: 0x%08X..0x%08X (0x%X B) %s"
              % (gap["our_offset"], gap["address"],
                 gap["address"] + gap["size"], gap["size"], gap["content"]))
    if len(inventory["gaps"]) > 24:
        print("        ... %d more gap(s)" % (len(inventory["gaps"]) - 24))
    if inventory["truncated"]:
        print("        (the DOL window ended before our section did)")
    if inventory.get("leading_gap"):
        print("        LEADING GAP 0x%X before our first word: the base is"
              " that much too low; 0x%08X is what the bytes support"
              % (inventory["leading_gap"], inventory["implied_base"]))


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
        render_base_decision(row)
        for base, entry in sorted(row.get("evidence", {}).items()):
            percent = entry["dol_percent"]
            marks = []
            if base == (row.get("base_decision") or {}).get("pool_base"):
                marks.append("POOL-BASE")
            if base == row.get("base"):
                marks.append("CHOSEN")
            if base in (row.get("base_decision") or {}).get("rejected", {}):
                marks.append("REJECTED")
            resync = ""
            if entry.get("resync_mismatched") is not None:
                resync = ("  resync %d differ / %d gap(s)%s"
                          % (entry["resync_mismatched"], entry["resync_gaps"],
                             "" if not entry.get("resync_leading_gap") else
                             " + 0x%X LEADING (this base is that much low)"
                             % entry["resync_leading_gap"]))
            print("      candidate %s%s  support %d  DOL %s%s  from %s"
                  % (base, (" [" + "/".join(marks) + "]") if marks else "",
                     entry["support"],
                     "%.1f%% of %d compared word(s)"
                     % (percent, entry["dol_compared"]) if percent is not None
                     else "n/a - %s" % (entry.get("dol_note")
                                        or "not measured"),
                     resync, ", ".join(entry["symbols"][:5]) or "-"))
        for base, reason in sorted((row.get("base_decision")
                                    or {}).get("rejected", {}).items()):
            print("      REFUSED %s: %s" % (base, reason))
        render_inventory(row)
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
