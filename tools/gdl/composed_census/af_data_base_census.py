#!/usr/bin/env python3
"""Recover a unit's TARGET data-section base addresses from its relocations.

The flip blocker `claimcheck` reports as

    object emits .rodata (0x17b) but splits.txt claims nothing

names the section but not the ADDRESS, and splits.txt cannot be written
without it.  This recovers the address mechanically: our object's .text
relocations and the dtk-extracted target object's .text relocations are
paired by (function, function-relative offset) -- sound whenever the two
instruction streams agree word for word -- and every pair whose OURS side
names a local data symbol yields `target address - our section offset` =
the section's base.  Bases that every symbol in a section agrees on are
printed as a ready-to-paste splits.txt line.

Pairing note: objdump records EMB_SDA21 relocations at a 2-byte offset on
one side of some object pairs, so offsets are matched with a +/-2 window
before being called unpaired; the tool prints how many rows that absorbed.

Usage (from the repository root):
  python tools/gdl/composed_census/af_data_base_census.py game/anim/atree
  python tools/gdl/composed_census/af_data_base_census.py game/mb/mb_camera \
      --out build/GUNE5D/af_data_base_census.json

Verify every base with the DOL bytes before committing a claim -- this tool
proves WHERE a section goes, never that its CONTENT is right.  Follow it
with `datadiff.py <unit>` once the claim is written, and read
claim.law.AF_dtk-rejects-an-unaligned-auto-split-start-so-some-claim-slack-
is-structural.20260903.v1 before choosing the claim's END address.
"""

import argparse
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))

import fndiff  # noqa: E402

DATA_SECTIONS = (".rodata", ".data", ".bss", ".sbss", ".sdata", ".sdata2")


def _dump(path, args):
    return subprocess.run([str(fndiff.OBJDUMP)] + args + [str(path)],
                          capture_output=True, text=True).stdout


def relocations(path):
    """[(fn, function-relative offset, type, symbol, addend)] in order."""
    rows, cur, start = [], None, 0
    for line in _dump(path, ["-dr"]).splitlines():
        m = re.match(r"^([0-9a-f]+) <(.+)>:$", line)
        if m:
            cur, start = m.group(2), int(m.group(1), 16)
            continue
        m = re.match(r"^\s*([0-9a-f]+):\s+(R_PPC\S+)\s+(\S+)$", line)
        if m and cur is not None:
            sym, add = m.group(3), 0
            for sep, sign in (("+0x", 1), ("-0x", -1)):
                if sep in sym:
                    sym, tail = sym.split(sep, 1)
                    add = sign * int(tail, 16)
                    break
            rows.append((cur, int(m.group(1), 16) - start,
                         m.group(2), sym, add))
    return rows


def symbol_table(path):
    """name -> (section, value, size) for defined data symbols."""
    tab = {}
    for line in _dump(path, ["-t"]).splitlines():
        m = re.match(r"^([0-9a-f]{8})\s+\S+\s+\S+\s+(\S+)\s+"
                     r"([0-9a-f]{8})\s+(.*)$", line)
        if m:
            tab[m.group(4).strip()] = (m.group(2), int(m.group(1), 16),
                                       int(m.group(3), 16))
    return tab


def section_sizes(path):
    sizes = {}
    for line in _dump(path, ["-h"]).splitlines():
        m = re.match(r"^\s*\d+\s+(\S+)\s+([0-9a-f]{8})", line)
        if m:
            sizes[m.group(1)] = int(m.group(2), 16)
    return sizes


def census(unit):
    unit = fndiff.unit_key(unit).rsplit(".", 1)[0]
    target = os.path.join(ROOT, "build", "GUNE5D", "obj", *unit.split("/"))
    ours = os.path.join(ROOT, "build", "GUNE5D", "src", *unit.split("/"))
    target, ours = target + ".o", ours + ".o"
    for p in (target, ours):
        if not os.path.exists(p):
            return {"unit": unit, "error": "missing object: %s" % p}

    tmap = {}
    for fn, off, kind, sym, add in relocations(target):
        tmap[(fn, off)] = (kind, sym, add)
    osyms = symbol_table(ours)
    sizes = section_sizes(ours)

    bases, unpaired, shifted = {}, 0, 0
    for fn, off, kind, sym, add in relocations(ours):
        sec, val, _size = osyms.get(sym, (None, None, None))
        if sec not in DATA_SECTIONS:
            continue
        hit = None
        for delta in (0, -2, 2):
            cand = tmap.get((fn, off + delta))
            if cand and cand[0] == kind:
                hit = cand
                shifted += 1 if delta else 0
                break
        if hit is None:
            unpaired += 1
            continue
        _k, tsym, tadd = hit
        m = re.match(r"^lbl_([0-9A-Fa-f]{8})$", tsym)
        taddr = int(m.group(1), 16) if m else fndiff.symbol_table().get(tsym)
        if isinstance(taddr, tuple):
            taddr = taddr[0]
        if not isinstance(taddr, int):
            continue
        bases.setdefault(sec, {}).setdefault(taddr + tadd - add - val,
                                             []).append(sym)

    out = {"unit": unit, "unpaired_relocations": unpaired,
           "offset_shifted_pairs": shifted, "sections": {}}
    for sec in DATA_SECTIONS:
        if sec not in sizes and sec not in bases:
            continue
        cands = bases.get(sec, {})
        row = {"object_size": sizes.get(sec),
               "candidate_bases": {"0x%08X" % b: sorted(set(v))
                                   for b, v in cands.items()}}
        if len(cands) == 1 and sizes.get(sec):
            base = next(iter(cands))
            row["splits_line"] = ("\t%-11s start:0x%08X end:0x%08X"
                                  % (sec, base, base + sizes[sec]))
        out["sections"][sec] = row
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("units", nargs="+")
    ap.add_argument("--out", default=None,
                    help="write the JSON result here"
                         " (default: print only)")
    args = ap.parse_args()
    results = [census(u) for u in args.units]
    for r in results:
        print("=== %s ===" % r["unit"])
        if "error" in r:
            print("  %s" % r["error"])
            continue
        print("  relocation pairs: %d offset-shifted, %d unpaired"
              % (r["offset_shifted_pairs"], r["unpaired_relocations"]))
        for sec, row in r["sections"].items():
            print("  %-9s object 0x%X" % (sec, row["object_size"] or 0))
            for b, syms in sorted(row["candidate_bases"].items()):
                print("      base %s  from %s" % (b, ", ".join(syms[:6])))
            if "splits_line" in row:
                print("      PASTE:%s" % row["splits_line"])
            elif not row["candidate_bases"]:
                print("      no target address resolvable from .text"
                      " relocations: the section is reached through a"
                      " SECTION symbol (.bss.0/.data+addend), holds only"
                      " dead data, or is bss-only -- fall back to"
                      " config/GUNE5D/symbols.txt for the first symbol"
                      " this section defines")
    if args.out:
        path = (args.out if os.path.isabs(args.out)
                else os.path.join(ROOT, args.out))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(results, fh, indent=1)
        print("wrote %s" % path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
