#!/usr/bin/env python3
"""Pre-link data-section byte comparator: our compiled object vs the DOL.

For each data-class section (.rodata .data .sdata .sdata2) claimed by a unit
in splits.txt, compare our object's emitted bytes against the original DOL
bytes at the claimed addresses. Words that carry a relocation in our object
are skipped (their final value exists only after the link) but their count
and placement are reported, which still exposes jumptable ORDER mismatches.

This catches what fndiff structurally normalizes away:
  - wrong constant VALUES (mathfunc's 1e-5 vs 1e-14 epsilon, atan tables)
  - wrong pool/table EMISSION ORDER (sincos duplicate 0.5f pairs)
  - initializer typos that only sha1 would catch after a full link

Usage (from repo root):
  python tools/gdl/datadiff.py game/mathfunc
  python tools/gdl/datadiff.py MSL/sincos MSL/trigf_data
  python tools/gdl/datadiff.py --matching        # all Matching units

Exit 1 if any byte mismatch found.
"""

import re
import struct
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parent.parent.parent
OBJDUMP = REPO / "build" / "binutils" / "powerpc-eabi-objdump.exe"
SPLITS = REPO / "config" / VERSION / "splits.txt"
DOL = REPO / "orig" / VERSION / "sys" / "main.dol"

DATA_SECTIONS = (".rodata", ".data", ".sdata", ".sdata2")


def dol_read(va, size):
    data = DOL.read_bytes()
    text_off = struct.unpack(">7I", data[0x00:0x1C])
    data_off = struct.unpack(">11I", data[0x1C:0x48])
    text_addr = struct.unpack(">7I", data[0x48:0x64])
    data_addr = struct.unpack(">11I", data[0x64:0x90])
    text_size = struct.unpack(">7I", data[0x90:0xAC])
    data_size = struct.unpack(">11I", data[0xAC:0xD8])
    for off, addr, sz in list(zip(text_off, text_addr, text_size)) + \
                         list(zip(data_off, data_addr, data_size)):
        if addr and addr <= va and va + size <= addr + sz:
            fo = off + (va - addr)
            return data[fo:fo + size]
    return None


def parse_splits():
    units = {}
    cur = None
    for line in SPLITS.read_text(encoding="utf-8").splitlines():
        m = re.match(r"^(\S.+):$", line)
        if m:
            cur = m.group(1)
            units[cur] = {}
            continue
        m = re.match(r"^\t(\S+)\s+start:0x([0-9A-Fa-f]+) end:0x([0-9A-Fa-f]+)", line)
        if m and cur:
            units[cur][m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))
    return units


def obj_sections(obj):
    """name -> bytes, from objdump -s."""
    out = subprocess.run([str(OBJDUMP), "-s", str(obj)], capture_output=True,
                         text=True).stdout
    secs = {}
    name = None
    for line in out.splitlines():
        m = re.match(r"^Contents of section (\S+):", line)
        if m:
            name = m.group(1)
            secs[name] = bytearray()
            continue
        if name:
            m = re.match(r"^ ([0-9a-f]+) ((?:[0-9a-f ]{8,9}){1,4})", line)
            if m:
                hexpart = m.group(2).replace(" ", "")
                secs[name] += bytes.fromhex(hexpart)
    return secs


def obj_relocs(obj):
    """section -> set of relocated word offsets."""
    out = subprocess.run([str(OBJDUMP), "-r", str(obj)], capture_output=True,
                         text=True).stdout
    relocs = {}
    sec = None
    for line in out.splitlines():
        m = re.match(r"^RELOCATION RECORDS FOR \[(\S+)\]", line)
        if m:
            sec = m.group(1)
            relocs[sec] = set()
            continue
        m = re.match(r"^([0-9a-f]+)\s+R_PPC_", line)
        if m and sec:
            relocs[sec].add(int(m.group(1), 16) & ~3)
    return relocs


def check_unit(unit, claims):
    obj = REPO / "build" / VERSION / "src" / f"{unit.rsplit('.', 1)[0]}.o"
    if not obj.exists():
        print(f"[{unit}] SKIP: object not built ({obj})")
        return 0
    secs = obj_sections(obj)
    relocs = obj_relocs(obj)
    bad = 0
    for sec in DATA_SECTIONS:
        ours = secs.get(sec)
        claim = claims.get(sec)
        if ours is None and claim is None:
            continue
        if ours is None:
            print(f"[{unit}] {sec}: claimed 0x{claim[1]-claim[0]:X} but object emits nothing")
            continue
        if claim is None:
            print(f"[{unit}] {sec}: object emits 0x{len(ours):X} but nothing claimed")
            bad += 1
            continue
        lo, hi = claim
        orig = dol_read(lo, hi - lo)
        if orig is None:
            print(f"[{unit}] {sec}: claim 0x{lo:08X} not inside DOL")
            bad += 1
            continue
        if len(ours) > hi - lo:
            print(f"[{unit}] {sec}: object 0x{len(ours):X} bytes > claim 0x{hi-lo:X}")
            bad += 1
        n = min(len(ours), len(orig))
        rel = relocs.get(sec, set())
        skipped = 0
        shown = 0
        for i in range(0, n, 4):
            if i in rel:
                skipped += 1
                continue
            a = bytes(ours[i:i + 4])
            b = orig[i:i + 4]
            if a != b:
                bad += 1
                if shown < 8:
                    va = lo + i
                    fa = struct.unpack(">f", a.ljust(4, b"\0"))[0] if len(a) == 4 else 0
                    fb = struct.unpack(">f", b.ljust(4, b"\0"))[0] if len(b) == 4 else 0
                    print(f"[{unit}] {sec}+0x{i:X} (VA 0x{va:08X}): "
                          f"ours {a.hex()} ({fa!r}) != dol {b.hex()} ({fb!r})")
                    shown += 1
        if bad and shown >= 8:
            print(f"[{unit}] {sec}: ... more mismatches suppressed")
        tail = orig[n:]
        tailpad = all(c == 0 for c in tail)
        status = "OK" if not bad else "MISMATCH"
        extra = f", {skipped} reloc words skipped" if skipped else ""
        slack = f", 0x{len(orig)-n:X} claim slack{'(zero)' if tailpad else '(NONZERO!)'}" if len(orig) > n else ""
        if not tailpad:
            bad += 1
        print(f"[{unit}] {sec}: {status} 0x{n:X} bytes compared{extra}{slack}")
    return bad


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 1
    units = parse_splits()
    targets = []
    if args[0] == "--matching":
        cfg = (REPO / "configure.py").read_text(encoding="utf-8")
        targets = [m.group(1).rsplit(".", 1)[0] for m in
                   re.finditer(r'Object\(Matching, "([^"]+)"', cfg)]
    else:
        targets = [a.replace("\\", "/") for a in args]
    bad = 0
    for t in targets:
        key = next((k for k in units if k.rsplit(".", 1)[0] == t or k == t), None)
        if key is None:
            print(f"[{t}] no splits entry")
            continue
        bad += check_unit(key, units[key])
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
