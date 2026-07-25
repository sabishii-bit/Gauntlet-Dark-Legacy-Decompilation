#!/usr/bin/env python3
"""Region survey: one call replaces a fnasm-per-function mapping session.

For every function symbol inside a VA range, print its size, callees
(REL24 targets), and data references (SDA21/ADDR16 targets), plus the
first N instructions with --heads. Built for shape-mapping campaigns
(AUDIO region: match GCN fns to Xbox PDB names by callee/data pattern).

Usage (from repo root):
  python tools/gdl/fnsurvey.py 0x800D1E04 0x800D5260
  python tools/gdl/fnsurvey.py 0x800D1E04 0x800D5260 --heads 8
  python tools/gdl/fnsurvey.py 0x800D1E04 0x800D5260 --calls AudioQueUpdate
  python tools/gdl/fnsurvey.py --callers fn_800D2568   # whole-DOL reverse index
"""

import re
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parent.parent.parent
OBJDUMP = REPO / "build" / "binutils" / "powerpc-eabi-objdump.exe"
SYMBOLS = REPO / "config" / VERSION / "symbols.txt"
OBJDIR = REPO / "build" / VERSION / "obj"


def range_fns(lo, hi):
    fns = []
    for m in re.finditer(
            r"^(\S+) = \.text:0x([0-9A-Fa-f]+); // type:function size:0x([0-9A-Fa-f]+)",
            SYMBOLS.read_text(encoding="utf-8"), re.M):
        addr = int(m.group(2), 16)
        if lo <= addr < hi:
            fns.append((addr, m.group(1), int(m.group(3), 16)))
    return sorted(fns)


def build_symbol_index():
    """symbol name -> object path, from every auto/unit object's symtab."""
    idx = {}
    for obj in OBJDIR.rglob("*.o"):
        out = subprocess.run([str(OBJDUMP), "-t", str(obj)],
                             capture_output=True, text=True).stdout
        for line in out.splitlines():
            m = re.match(r"^[0-9a-f]+ .{7}\s*(?:F|O)?\s*\.text\s+[0-9a-f]+\s+(\S+)$", line)
            if m:
                idx.setdefault(m.group(1), obj)
    return idx


DISASM_CACHE = {}


def disasm(obj):
    if obj not in DISASM_CACHE:
        DISASM_CACHE[obj] = subprocess.run(
            [str(OBJDUMP), "-dr", str(obj)], capture_output=True, text=True).stdout
    return DISASM_CACHE[obj]


def fn_block(obj, name):
    text = disasm(obj)
    m = re.search(rf"^[0-9a-f]+ <{re.escape(name)}>:\n(.*?)(?=\n[0-9a-f]+ <|\Z)",
                  text, re.M | re.S)
    return m.group(1) if m else None


def main():
    args = sys.argv[1:]
    heads = 0
    calls_filter = None
    if "--heads" in args:
        i = args.index("--heads")
        heads = int(args[i + 1])
        del args[i:i + 2]
    if "--calls" in args:
        i = args.index("--calls")
        calls_filter = args[i + 1]
        del args[i:i + 2]
    if args and args[0] == "--callers":
        target = args[1]
        pat = re.compile(r"R_PPC_REL24\s+" + re.escape(target) + r"(\+|$)")
        for obj in sorted(OBJDIR.rglob("*.o")):
            out = subprocess.run([str(OBJDUMP), "-r", str(obj)],
                                 capture_output=True, text=True).stdout
            n = sum(1 for line in out.splitlines() if pat.search(line.rstrip()))
            if n:
                print(f"{obj.relative_to(OBJDIR)}  x{n}")
        return 0
    if len(args) < 2:
        print(__doc__)
        return 1
    lo, hi = int(args[0], 16), int(args[1], 16)

    fns = range_fns(lo, hi)
    if not fns:
        print("no function symbols in range")
        return 1
    idx = build_symbol_index()

    for addr, name, size in fns:
        obj = idx.get(name)
        if obj is None:
            print(f"0x{addr:08X} {name} (0x{size:X}) -- NO OBJECT (re-split?)")
            continue
        block = fn_block(obj, name)
        if block is None:
            print(f"0x{addr:08X} {name} (0x{size:X}) -- not in {obj.name}")
            continue
        callees, drefs = [], []
        for m in re.finditer(r"R_PPC_REL24\s+(\S+)", block):
            t = m.group(1).split("+")[0]
            if t != name and (not callees or callees[-1] != t):
                callees.append(t)
        for m in re.finditer(r"R_PPC_(?:EMB_SDA21|ADDR16_HA)\s+(\S+)", block):
            t = m.group(1).split("+")[0]
            if not drefs or drefs[-1] != t:
                drefs.append(t)
        if calls_filter and calls_filter not in callees:
            continue

        def uniq(seq):
            seen, out = set(), []
            for x in seq:
                if x not in seen:
                    seen.add(x)
                    out.append(x)
            return out

        print(f"0x{addr:08X} {name} (0x{size:X})")
        if callees:
            print(f"    calls: {', '.join(uniq(callees))}")
        if drefs:
            print(f"    data:  {', '.join(uniq(drefs))}")
        if heads:
            lines = [l for l in block.splitlines() if re.match(r"^\s*[0-9a-f]+:", l)]
            for l in lines[:heads]:
                ins = re.sub(r"^\s*[0-9a-f]+:\s*(?:[0-9a-f]{2} ){4}\s*", "  ", l)
                print(f"   {ins}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
