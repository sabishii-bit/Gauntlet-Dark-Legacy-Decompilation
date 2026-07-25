#!/usr/bin/env python3
"""Byte-compare the built DOL against the original, with the context this
project actually needs:

- Header deltas are decoded per section ("section 12 grew by 0x20") instead of
  dumped as raw bytes -- a grown section almost always means the newest
  Matching unit emits something its splits don't claim (run claimcheck).
- Body diffs are mapped file offset -> VA -> owning symbol (symbols.txt) and
  owning unit (splits.txt).
- The expected clean_extab diff is suppressed: config.yml sets
  `clean_extab: true`, so build.sha1 targets an extab-CLEANED image and the
  word at VA 0x8000872C (fn_800E3E80's extab, uninitialized compiler garbage)
  ALWAYS differs from the raw original. That diff is not a failure.
  Pass --all to show it anyway.

Usage (from repo root):
  python tools/gdl/doldiff.py            # summary of real differences
  python tools/gdl/doldiff.py --all      # include the expected clean_extab diff

Exit code 0 if only expected diffs remain, 1 otherwise.
"""

import re
import struct
import sys
from bisect import bisect_right
from pathlib import Path

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parent.parent.parent
ORIG = REPO / "orig" / VERSION / "sys" / "main.dol"
BUILT = REPO / "build" / VERSION / "main.dol"
SYMBOLS = REPO / "config" / VERSION / "symbols.txt"
SPLITS = REPO / "config" / VERSION / "splits.txt"

# clean_extab: the one word dtk cannot round-trip (see game-code-frontier notes)
EXPECTED = [(0x8000872C, 0x80008730)]
MAX_RUNS = 12


def dol_header(data):
    offs = struct.unpack(">18I", data[0x00:0x48])
    addrs = struct.unpack(">18I", data[0x48:0x90])
    sizes = struct.unpack(">18I", data[0x90:0xD8])
    bss_addr, bss_size, entry = struct.unpack(">3I", data[0xD8:0xE4])
    return offs, addrs, sizes, bss_addr, bss_size, entry


def file_to_va(hdr, off):
    offs, addrs, sizes = hdr[0], hdr[1], hdr[2]
    for o, a, s in zip(offs, addrs, sizes):
        if s and o <= off < o + s:
            return a + off - o
    return None


def load_symbols():
    syms = []
    pat = re.compile(r"^(\S+) = [^:]+:0x([0-9A-Fa-f]{8});")
    for line in SYMBOLS.read_text(encoding="utf-8").splitlines():
        m = pat.match(line)
        if m:
            syms.append((int(m.group(2), 16), m.group(1)))
    syms.sort()
    return [a for a, _ in syms], [n for _, n in syms]


def load_units():
    units = []
    cur = None
    for line in SPLITS.read_text(encoding="utf-8").splitlines():
        m = re.match(r"^(\S.+):$", line)
        if m:
            cur = m.group(1)
            continue
        m = re.match(r"^\t\S+\s+start:0x([0-9A-Fa-f]+) end:0x([0-9A-Fa-f]+)", line)
        if m and cur:
            units.append((int(m.group(1), 16), int(m.group(2), 16), cur))
    units.sort()
    return units


def owner(va, sym_addrs, sym_names, units):
    i = bisect_right(sym_addrs, va) - 1
    sym = f"{sym_names[i]}+{va - sym_addrs[i]:#x}" if i >= 0 else "?"
    unit = next((u for s, e, u in units if s <= va < e), "(auto/unclaimed)")
    return sym, unit


def main():
    show_all = "--all" in sys.argv
    orig = ORIG.read_bytes()
    new = BUILT.read_bytes()

    if len(orig) != len(new):
        print(f"SIZE MISMATCH: orig {len(orig):#x} vs built {len(new):#x}")

    ho, hn = dol_header(orig), dol_header(new)
    header_bad = False
    for i, (fo, fn_, ao, an, so, sn) in enumerate(
        zip(ho[0], hn[0], ho[1], hn[1], ho[2], hn[2])
    ):
        if (fo, ao, so) != (fn_, an, sn):
            header_bad = True
            delta = sn - so
            print(f"HEADER: section {i}: off {fo:#x}->{fn_:#x}  addr {ao:#x}->{an:#x}  "
                  f"size {so:#x}->{sn:#x}" + (f"  (grew {delta:+#x})" if delta else ""))
    if ho[3:] != hn[3:]:
        header_bad = True
        print(f"HEADER: bss/entry {ho[3:]} -> {hn[3:]}")
    if header_bad:
        print("=> a section changed size: some unit emits data its splits don't claim."
              " Run: python tools/gdl/claimcheck.py --matching")

    sym_addrs, sym_names = load_symbols()
    units = load_units()

    runs = []
    i, n = 0, min(len(orig), len(new))
    while i < n:
        if orig[i] != new[i]:
            start = i
            while i < n and orig[i] != new[i]:
                i += 1
            runs.append((start, i))
        i += 1

    real = 0
    shown = 0
    listed = 0
    for start, end in runs:
        va = file_to_va(ho, start)
        expected = va is not None and any(s <= va < e for s, e in EXPECTED)
        if expected and not show_all:
            continue
        listed += 1
        real += not expected
        if shown < MAX_RUNS:
            shown += 1
            tag = " (expected clean_extab)" if expected else ""
            if va is None:
                loc = "header/pad"
            else:
                sym, unit = owner(va, sym_addrs, sym_names, units)
                loc = f"VA {va:#x} {sym} [{unit}]"
            print(f"diff file {start:#x}-{end:#x}  {loc}{tag}")
            print(f"  orig: {orig[start:min(end, start + 16)].hex(' ')}")
            print(f"  new : {new[start:min(end, start + 16)].hex(' ')}")
    if listed > shown:
        print(f"... {listed - shown} more diff runs")

    suppressed = sum(
        1 for s, e in runs
        if (v := file_to_va(ho, s)) is not None and any(a <= v < b for a, b in EXPECTED)
    )
    if not real and not header_bad:
        note = f" ({suppressed} expected clean_extab diff suppressed)" if suppressed else ""
        print(f"DOL matches{note}")
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
