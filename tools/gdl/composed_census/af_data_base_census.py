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

THE DOL NOW DECIDES BETWEEN DISAGREEING BASES (run 61 item 8).  Measured at
20c0d7ea1, this tool printed THREE bases for game/game/controls `.sdata2` --
0x803463E8 (one row, @1531), 0x803463F8 (six rows) and 0x80346408 (one row,
@1528) -- for a section whose shipped, verified claim is 0x803463F8 and
whose bytes are 100% equal there.  Two of the three were artefacts of
pairing .text relocations BY OFFSET across two instruction streams that do
not agree, which is exactly what a NonMatching TU has.  Row counts cannot
settle that (the same run printed six mutually disagreeing `.sbss` bases),
so every candidate is now scored against the DOL bytes at it, skipping the
words our object relocates, and a base is only ever promoted to a PASTE
line when it is the single fully byte-equal one.  The measured scores for
those three: 13.3%, 100.0%, 16.7%.

Everything else is demoted to CANDIDATE and carries the DOL comparison it
scored in the same line, so no lane has to guess which number to believe.
BSS-class sections occupy no DOL bytes and can never be scored this way;
they stay on relocation consensus and say so.

Verify every base with the DOL bytes before committing a claim -- this tool
proves WHERE a section goes, never that its CONTENT is right.  Follow it
with `datadiff.py <unit>` once the claim is written, and read
claim.law.AF_dtk-rejects-an-unaligned-auto-split-start-so-some-claim-slack-
is-structural.20260903.v1 before choosing the claim's END address.

For the image-wide version of this question -- which units have unclaimed
sections, how many bytes are claimable today, and what blocks the rest --
use `tools/gdl/claimable_sections.py`, whose scoring core this shares.
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

import claimable_sections  # noqa: E402
import fndiff  # noqa: E402

DATA_SECTIONS = (".rodata", ".data", ".bss", ".sbss", ".sdata", ".sdata2")
BSS_SECTIONS = (".bss", ".sbss", ".sbss2")


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


def straddled_symbol(section, end):
    """The target symbol an END address falls STRICTLY INSIDE, or None.

    dtk refuses a split that ends inside a symbol. Measured verbatim on the
    line this tool used to print for game/enemy/enemy:

        Split game/enemy/enemy.c .bss (0x80250E00..0x8025758C) ends within
        symbol 'gEnemies' (0x80251C18..0x80257590)

    Our object's `.bss` is 0x678C, so base + size lands at 0x8025758C, four
    bytes inside gEnemies — MWCC and the target disagree about the trailing
    pad of the last object, and the correct end (0x80257590) was sitting in
    dtk's own refusal. `claim.law.AF_dtk-rejects-an-unaligned-auto-split-
    start-so-some-claim-slack-is-structural.20260903.v1` records the slack as
    structural; this makes the PASTE line carry it instead of making the next
    lane read it out of a build failure.
    """
    for name, entry in fndiff.symbol_table().items():
        if not (isinstance(entry, tuple) and len(entry) >= 3):
            continue
        symbol_section, address, size = entry[0], entry[1], entry[2]
        if symbol_section != section or not size:
            continue
        if address < end < address + size:
            return name, address, address + size
    return None


def size_sequence_candidates(osyms, section, section_size):
    """Bases suggested by matching our object's SIZE SEQUENCE to symbols.txt.

    The fallback a lane ran by hand when the relocation pairing resolved
    nothing (attempt.EO_enemy-c-data-claims-land-two-of-three-...20260904.v1):
    our object emits N objects at ascending offsets with sizes s0..sN, and
    symbols.txt lists a run of N CONSECUTIVE symbols in the same section with
    the same sizes in the same order; the run's first address is the base.

    THIS IS ADVISORY AND NEVER BECOMES A PASTE LINE, which is a deliberate
    narrowing of the item that asked for it. Two measurements decided that.
    (1) The case that motivated it no longer needs it: enemy.c's `.data` base
    0x8011C0EC now falls straight out of the relocation pairing once named
    target symbols resolve, agreeing with the landed claim exactly. (2) A
    paste-ready line built on thin evidence is not merely useless, it is
    WRONG and confident: measured at c7b741799, this tool's old `.sbss` line
    for game/boss/boss read `start:0x80344758` while the shipped, verified
    claim in splits.txt is `start:0x80344378` — off by 0x3E0, printed
    paste-ready, on the strength of a single relocation row. Size agreement
    is weaker evidence than that row was. EO's own record confirms its claim
    "by content, not only by size" (100.0% bytes equal on the first build),
    and that content check is what promotes a candidate here to a claim.
    """
    ours = sorted(
        ((value, size) for _name, (sec, value, size) in osyms.items()
         if sec == section and size),
        key=lambda row: row[0])
    if not ours or not section_size:
        return []
    wanted = [size for _value, size in ours]
    table = sorted(
        ((entry[1], entry[2], name)
         for name, entry in fndiff.symbol_table().items()
         if isinstance(entry, tuple) and len(entry) >= 3
         and entry[0] == section and entry[2]),
        key=lambda row: row[0])
    out = []
    for start in range(len(table) - len(wanted) + 1):
        run = table[start:start + len(wanted)]
        if [size for _a, size, _n in run] != wanted:
            continue
        # Consecutive means each symbol begins exactly where the last ended;
        # a gap is a different layout and the sequence match is a coincidence.
        if any(run[i][0] + run[i][1] != run[i + 1][0]
               for i in range(len(run) - 1)):
            continue
        base = run[0][0] - ours[0][0]
        out.append({"base": "0x%08X" % base,
                    "symbols": [name for _a, _s, name in run],
                    "sizes": ["0x%X" % size for size in wanted]})
    return out


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
        if m:
            taddr = int(m.group(1), 16)
        else:
            # `fndiff.symbol_table()` returns (SECTION, ADDRESS, SIZE). This
            # read `entry[0]` — the SECTION STRING — and then discarded the
            # row because it was not an int, so EVERY relocation naming a
            # real target symbol was silently dropped and only the
            # `lbl_XXXXXXXX` spelling ever resolved. Measured on
            # game/enemy/enemy at c7b741799: 274 data-section relocation
            # pairs, 220 lbl_ and **54 named (20%) discarded**, among them
            # the pair `ours @692 -> target jumptable_8011C0EC` that decides
            # the .data base. That is why this tool printed "no target
            # address resolvable from .text relocations" for enemy.c's .data
            # and a lane derived 0x8011C0EC by hand from a size sequence
            # instead: the evidence was in the relocations the whole time.
            entry = fndiff.symbol_table().get(tsym)
            taddr = entry[1] if isinstance(entry, tuple) and len(entry) >= 2 \
                else entry
        if not isinstance(taddr, int):
            continue
        bases.setdefault(sec, {}).setdefault(taddr + tadd - add - val,
                                             []).append(sym)

    relocated = claimable_sections.relocated_offsets(ours)
    out = {"unit": unit, "unpaired_relocations": unpaired,
           "offset_shifted_pairs": shifted, "sections": {}}
    for sec in DATA_SECTIONS:
        if sec not in sizes and sec not in bases:
            continue
        cands = bases.get(sec, {})
        bss = sec in BSS_SECTIONS
        size = sizes.get(sec)
        payload = claimable_sections.section_bytes(ours, sec) \
            if size and not bss else b""
        scores = {}
        for b in cands:
            if bss or not size:
                scores[b] = None
                continue
            scores[b] = claimable_sections.score_base(
                payload, relocated.get(sec, set()), fndiff.dol_read(b, size))
        row = {"object_size": size,
               "candidate_bases": {"0x%08X" % b: sorted(set(v))
                                   for b, v in cands.items()},
               "dol_agreement": {
                   "0x%08X" % b: (None if score is None else
                                  {"percent": score["percent"],
                                   "compared": score["compared"],
                                   "skipped": score["skipped"],
                                   "note": score["note"]})
                   for b, score in scores.items()}}
        # ONE candidate is not evidence, and the DOL is -- WHERE IT EXISTS.
        # Where a comparison is possible, only a single fully byte-equal base
        # earns a paste line. Where none is possible (bss holds no DOL bytes;
        # a pointer table relocates every word it has) the old
        # single-candidate rule stands, labelled for what it is. Remeasured
        # on the native-only tree at 7ab03f3c1 over the 312 (unit, section)
        # pairs that resolve any base (build/c62_af_rule_delta.py, 255
        # configured units, 6 census refusals -- the Runtime.PPCEABI.H
        # archive members whose extracted object this tool cannot address):
        # 7 paste lines GAINED where the DOL elected one of several
        # candidates (controls/movieplayer/enemy .sdata2, attract/auxscreen
        # .sdata, SIBios .data, tower .data), 5 WITHDRAWN where the single
        # candidate is measurably wrong (dcs .rodata 95.6%, bosscam .sdata2
        # 50.0%, gauntworld .rodata 11.9%, combat .rodata 10.7%, nubinit
        # .rodata 0.0%), 300 unchanged. Requiring byte equality even where
        # nothing is comparable would have withdrawn 107 MORE (81 of them
        # bss-class sections, which own no DOL bytes at all, and the rest
        # fully relocated tables such as zlib/inflate .data) -- which is why
        # "unmeasurable" is not treated as "failed".
        equal = [b for b, score in scores.items()
                 if score and score["compared"] and score["percent"] == 100.0]
        comparable = any(score and score["compared"]
                         for score in scores.values())
        decided = (len(equal) == 1) if comparable else (len(cands) == 1)
        row["dol_comparable"] = comparable
        if decided and size:
            base = equal[0] if comparable else next(iter(cands))
            row["base_decided_by"] = (
                "100%% of %d compared DOL word(s)" % scores[base]["compared"]
                if comparable else
                "relocation consensus ONLY -- %s, so these bytes were never"
                " compared; confirm the claim with datadiff.py after writing"
                " it" % ("bss holds no DOL bytes" if bss else
                         "every word of this section is relocated"))
            end = base + sizes[sec]
            straddled = straddled_symbol(sec, end)
            if straddled:
                name, _start, symbol_end = straddled
                row["end_rounded_up_to"] = name
                row["object_end"] = "0x%08X" % end
                row["end_note"] = (
                    "object end 0x%08X falls INSIDE target symbol %s"
                    " (ends 0x%08X); dtk refuses a split that ends within a"
                    " symbol, so the claim ends at the symbol boundary and"
                    " the slack is structural"
                    % (end, name, symbol_end))
                end = symbol_end
            row["splits_line"] = ("\t%-11s start:0x%08X end:0x%08X"
                                  % (sec, base, end))
        if not cands:
            row["size_sequence_candidates"] = size_sequence_candidates(
                osyms, sec, sizes.get(sec))
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
            agreement = row.get("dol_agreement") or {}
            for b, syms in sorted(row["candidate_bases"].items()):
                score = agreement.get(b)
                if score is None:
                    dol = "DOL n/a (bss/empty: no bytes to compare)"
                elif score["percent"] is None:
                    dol = "DOL n/a (%s)" % (score["note"] or "not measured")
                else:
                    dol = "DOL %.1f%% of %d compared word(s)" % (
                        score["percent"], score["compared"])
                print("      base %s  %s  from %s"
                      % (b, dol, ", ".join(syms[:6])))
            if "splits_line" in row:
                if "end_note" in row:
                    print("      NOTE: %s" % row["end_note"])
                print("      DECIDED BY: %s" % row["base_decided_by"])
                print("      PASTE:%s" % row["splits_line"])
            elif row["candidate_bases"]:
                print("      CANDIDATES ONLY, no paste line: %s, so which of"
                      " these is the section base is NOT settled. Compare"
                      " the bytes yourself before believing one --"
                      % ("no single base is fully byte-equal in the DOL above"
                         if row.get("dol_comparable") else
                         "these bases disagree and nothing here is"
                         " byte-comparable against the DOL"))
                print("        python tools/gdl/claimable_sections.py %s"
                      % r["unit"])
                print("      A base with the most relocation rows is not the"
                      " right one: the .text pairing this derives from is"
                      " unsound wherever the two instruction streams differ.")
            elif not row["candidate_bases"]:
                print("      no target address resolvable from .text"
                      " relocations: the section is reached through a"
                      " SECTION symbol (.bss.0/.data+addend), holds only"
                      " dead data, or is bss-only")
                for hit in row.get("size_sequence_candidates") or []:
                    print("      SIZE-SEQUENCE CANDIDATE (advisory, NOT a"
                          " paste line): base %s" % hit["base"])
                    print("        our object's %d object(s) size %s match"
                          " consecutive symbols %s"
                          % (len(hit["sizes"]), ", ".join(hit["sizes"]),
                             ", ".join(hit["symbols"][:6])))
                    print("        CONFIRM BY CONTENT before claiming it:"
                          " write the splits line, rebuild, and require"
                          " `datadiff.py --sections <unit>` to report ~100%"
                          " bytes equal. Size agreement alone is weaker than"
                          " the single relocation row that once made this"
                          " tool print a WRONG paste-ready .sbss base for"
                          " game/boss/boss (0x80344758 against the shipped"
                          " 0x80344378).")
                if not (row.get("size_sequence_candidates") or []):
                    print("      no size-sequence match either -- fall back"
                          " to config/GUNE5D/symbols.txt for the first"
                          " symbol this section defines")
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
