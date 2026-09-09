#!/usr/bin/env python3
"""Bind ONE of our object's sections against the DOL at ONE stated address.

    python tools/gdl/composed_census/secbind.py <unit> <section>
    python tools/gdl/composed_census/secbind.py <unit> .rodata 0x80113AE0
    python tools/gdl/composed_census/secbind.py <unit> .data --object <path>
    python tools/gdl/composed_census/secbind.py <unit> .rodata --out build/x.json

WHAT IT ANSWERS, and what claimable_sections answers instead. That tool
DERIVES a base by voting relocation sites and scoring DOL bytes, and it will
tell you the base it elected and refuse the ones it did not. This one takes
the address YOU name — a boundary from splits.txt, a neighbour TU's first
symbol, a hypothesis from a PDB roster — and reports what our bytes look
like there:

  * the byte-exact EQUAL PREFIX and the first differing word, both sides
    printed as hex and as text;
  * every DOL-side insertion with its address, size and decoded content;
  * the SECTION-END verdict: where our last byte lands, whether that is a
    symbol start, the inside of a symbol (dtk refuses a split that ends
    there) or a gap, and what the next symbol start above it is.

It is for testing a stated hypothesis, which is exactly the question
claimable_sections' election cannot be asked. With no address it falls back
to that election, so the two never disagree by accident.

NOT A SECOND GAP FINDER. The alignment, the insertion search and the
inventory rendering are `claimable_sections.gap_inventory`,
`describe_bytes` and `render_inventory`, imported and called. A second copy
of a resync heuristic is how two lanes come to quote different gap counts
for one section; the ONLY thing computed here that that tool does not
already expose is the raw equal PREFIX (its inventory resynchronises, which
is the right thing for a base score and hides where the first divergence
actually is) and the end verdict.

WHY THE PREFIX AND THE RESYNC ARE DIFFERENT NUMBERS. A resynchronised
inventory can read `131/131 word(s) equal` for a section whose FIRST word
already differs from the DOL at that address, because the aligner is
allowed to insert. The prefix is the honest "how far do the bytes agree
literally", and a lane that reads only one of the two will mis-size the
recovery.

LIVE, measured at 6da714b06 in W:/Repositories/GDL-Claude-P5.
`game/game/player` `.rodata` is 0x20E bytes. Bound at the target's own
front-block address 0x80113AE0:

    EQUAL PREFIX 0x0 byte(s) of 0x20E; 0 of 131 words equal
    first differing word at +0x0 (0x80113AE0)
        ours  31365f2573434f494e000000  '16_%sCOIN...'
        dol   7472626f5f66756c6c5f6e6577  'trbo_full_new'

Bound instead at the base claimable_sections elects, 0x80113E28:

    EQUAL PREFIX 0x44 byte(s); 19 of 131 words equal position-for-position
    DOL INVENTORY: 131/131 word(s) equal after resync, 0 mismatched,
                   3 DOL-side gap(s) totalling 0x1E8 byte(s)
      GAP after our +0x0044: 0x80113E6C..0x80113E98 (0x2C B) "NO FLOOR" ...
    SECTION END 0x80114036 falls INSIDE lbl_80113FA0

Every one of those numbers is true and they answer different questions.
0x80113AE0 is the front of a block our source does not have (the 0x348
FRONT DEFICIT), so nothing binds there; 0x80113E28 is where what we DO
have starts, and there every word is right once three missing interior
datums are allowed for. Reading only the raw percentage at either address
would size the recovery wrongly in opposite directions.

Lane P3 measured that front block at 0x32C byte-exact of 0x348 with its
`i_tables.c` `.data` recovery APPLIED — a modified tree, not this one, and
its 0x1C remainder was "MIKEYPUP" plus a 0x10 const. Quote that number only
against the tree that produced it; this tool prints what the object in
front of it binds, which is the point of having it.

EXIT 0 when the binding was measured, 2 when it could not be (no such
section, address outside the DOL, missing object).
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))

import claimable_sections as cs                           # noqa: E402
import cliscreen                                          # noqa: E402
import fndiff                                             # noqa: E402

OK, REFUSED = 0, 2


def equal_prefix(ours, theirs):
    """(bytes agreeing from offset 0, word-aligned).

    Word-aligned because everything else in this comparison is: a
    byte-granular prefix would report 0x32E for a run that agrees for
    0x32C and then differs inside the next word.
    """
    limit = min(len(ours), len(theirs)) // 4 * 4
    for offset in range(0, limit, 4):
        if ours[offset:offset + 4] != theirs[offset:offset + 4]:
            return offset
    return limit


def text_of(data):
    """`data` as printable text with non-printables dotted, for the eye."""
    return "".join(chr(byte) if 0x20 <= byte < 0x7F else "."
                   for byte in data)


def derive_base(unit, section):
    """(address, why) from claimable_sections' election, or (None, why)."""
    splits_path = os.path.join(ROOT, "config", "GUNE5D", "splits.txt")
    if not os.path.exists(splits_path):
        return None, "no config/GUNE5D/splits.txt to derive a base from"
    splits = cs.parse_splits(splits_path)
    row = cs.census(unit, splits, cs.claimed_intervals(splits), defined=None)
    for found in row.get("sections", []):
        if found["section"] != section:
            continue
        if not found.get("base"):
            return None, ("claimable_sections could not elect a base: %s"
                          % found.get("verdict"))
        source = (found.get("base_decision") or {}).get("source", "vote")
        return int(found["base"], 16), (
            "derived by claimable_sections (%s rule, verdict %s)"
            % (source, found["verdict"]))
    return None, ("claimable_sections does not report %s for %s (it censuses"
                  " UNCLAIMED sections and this one may already be claimed)"
                  % (section, unit))


def end_verdict(section, end):
    """The section-end facts, from claimable_sections' own boundary readers."""
    symbols = fndiff.symbol_table()
    boundary = cs.boundary_of(section, end, symbols)
    straddle = cs.straddled_end(section, end, symbols)
    return {
        "end": end,
        "boundary": boundary["kind"],
        "boundary_symbol": boundary.get("symbol"),
        "straddled": None if straddle is None else
        {"symbol": straddle[0], "start": straddle[1], "end": straddle[2]},
        "next_symbol_start": cs.next_symbol_start(section, end, symbols),
    }


def bind(unit, section, base=None, object_path=None):
    """The whole record for one (unit, section, base). Pure of printing."""
    ours_object = object_path or os.path.join(
        ROOT, "build", "GUNE5D", "src", unit + ".o")
    if not os.path.exists(ours_object):
        raise SystemExit("secbind: no object at %s; run ninja first"
                         % ours_object)
    sizes = cs.section_sizes(ours_object)
    if not sizes.get(section):
        # objdump -j on an absent section EXITS 1, which fndiff turns into
        # ObjdumpFailed (fail-closed, correctly). Ask the section table
        # first so the answer is a message naming the sections that exist,
        # not a traceback out of a dump that never ran.
        raise SystemExit("secbind: %s defines no %s (it has: %s)"
                         % (os.path.relpath(ours_object, ROOT), section,
                            ", ".join(sorted(sizes)) or "no sections"))
    ours = cs.section_bytes(ours_object, section)
    why = "given on the command line"
    if base is None:
        base, why = derive_base(unit, section)
        if base is None:
            raise SystemExit("secbind: no base given and none derivable — "
                             + why)
    try:
        exact = fndiff.dol_read(base, len(ours))
        # THE INVENTORY NEEDS ROOM TO INSERT. `gap_inventory` finds a
        # DOL-side insertion by looking AHEAD in the target; given only
        # len(ours) bytes it runs out of target words and reports the tail
        # as mismatched instead. claimable_sections reads
        # `len + max(len, 0x400)` for exactly this reason, and reading less
        # here made the same section report 88/131 resynced-equal against
        # that tool's 131/131 — two numbers for one section, which is the
        # failure this module's whole design is trying to avoid.
        window = fndiff.dol_read(base, len(ours) + max(len(ours), 0x400))
    except (SystemExit, ValueError) as error:
        raise SystemExit("secbind: cannot read 0x%08X..0x%08X out of the DOL"
                         " (%s)" % (base, base + len(ours), error))
    if exact is None or len(exact) < len(ours):
        raise SystemExit("secbind: 0x%08X..0x%08X is not inside any DOL"
                         " section" % (base, base + len(ours)))
    theirs = exact

    prefix = equal_prefix(ours, theirs)
    relocated = cs.relocated_offsets(ours_object).get(section, set())
    inventory = cs.gap_inventory(ours, relocated, window or theirs, base)
    return {
        "unit": unit, "section": section, "object": ours_object,
        "base": base, "base_source": why, "size": len(ours),
        "equal_prefix": prefix,
        "first_difference": None if prefix >= len(ours) else {
            "our_offset": prefix, "address": base + prefix,
            "ours": ours[prefix:prefix + 16].hex(),
            "dol": theirs[prefix:prefix + 16].hex(),
            "ours_text": text_of(ours[prefix:prefix + 24]),
            "dol_text": text_of(theirs[prefix:prefix + 24]),
        },
        "words_equal_raw": sum(1 for offset in range(0, len(ours) // 4 * 4, 4)
                               if ours[offset:offset + 4]
                               == theirs[offset:offset + 4]),
        "words": len(ours) // 4,
        "relocated_words": len(relocated),
        "inventory": inventory,
        "end_verdict": end_verdict(section, base + len(ours)),
    }


def render(record):
    print("== %s %s: 0x%X byte(s) from %s"
          % (record["unit"], record["section"], record["size"],
             os.path.relpath(record["object"], ROOT).replace("\\", "/")))
    print("   bound at 0x%08X — %s" % (record["base"], record["base_source"]))
    print("   EQUAL PREFIX 0x%X byte(s) of 0x%X; %d of %d word(s) equal"
          " position-for-position (NO resync)"
          % (record["equal_prefix"], record["size"],
             record["words_equal_raw"], record["words"]))
    first = record["first_difference"]
    if first is None:
        print("   the whole section is byte-identical at this address")
    else:
        print("   first differing word at +0x%X (0x%08X)"
              % (first["our_offset"], first["address"]))
        print("       ours  %s  %r" % (first["ours"], first["ours_text"]))
        print("       dol   %s  %r" % (first["dol"], first["dol_text"]))
    # The gap inventory is claimable_sections', rendered by its own printer,
    # so a gap count here and a gap count there can never disagree.
    cs.render_inventory({"base": "0x%08X" % record["base"],
                         "inventory": record["inventory"]})
    end = record["end_verdict"]
    print("   SECTION END 0x%08X: symbols.txt says %s%s"
          % (end["end"], end["boundary"],
             " (%s)" % end["boundary_symbol"] if end["boundary_symbol"]
             else ""))
    if end["straddled"]:
        print("       it falls INSIDE %s (0x%08X..0x%08X); dtk refuses a"
              " split that ends within a symbol, so a claim here must round"
              " up to 0x%08X and carry the slack"
              % (end["straddled"]["symbol"], end["straddled"]["start"],
                 end["straddled"]["end"], end["straddled"]["end"]))
    if end["next_symbol_start"]:
        print("       next symbol start above it: 0x%08X"
              % end["next_symbol_start"])
    print("   THIS PROVES WHERE OUR BYTES BIND, not ownership of the target"
          " extent, not link placement, and not that the surplus is dead."
          " Write the splits line, rebuild, and require"
          " `datadiff.py --sections %s`." % record["unit"])


def main(argv=None):
    cliscreen.help_only(__doc__)
    parser = argparse.ArgumentParser(
        prog="secbind.py",
        description="Bind one object section against the DOL at one address.")
    parser.add_argument("unit")
    parser.add_argument("section")
    parser.add_argument("address", nargs="?",
                        help="hex address to bind at; omit to use"
                             " claimable_sections' elected base")
    parser.add_argument("--object", help="bind this object instead of"
                                         " build/GUNE5D/src/<unit>.o")
    parser.add_argument("--out", help="write the record as JSON (under build/)")
    args = parser.parse_args(argv)
    unit = fndiff.unit_key(args.unit)
    try:
        base = int(args.address, 16) if args.address else None
    except ValueError:
        print("secbind: %r is not a hex address" % args.address)
        return REFUSED
    try:
        record = bind(unit, args.section, base, args.object)
    except SystemExit as refusal:
        if isinstance(refusal.code, int):
            raise
        print(str(refusal))
        return REFUSED
    except (OSError, fndiff.ObjdumpFailed, cs.Unmeasurable) as error:
        print("secbind REFUSED: %s" % error)
        return REFUSED
    render(record)
    if args.out:
        out = os.path.join(ROOT, args.out)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w", encoding="utf-8") as handle:
            json.dump(record, handle, indent=2, sort_keys=True)
        print("\nwrote %s" % args.out)
    return OK


if __name__ == "__main__":
    sys.exit(main())
