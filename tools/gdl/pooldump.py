#!/usr/bin/env python3
"""Dump original-DOL data bytes for a VA range, interpreted as doubles/floats.

Usage:
  python tools/gdl/pooldump.py 0x80349470 0x803494D0        # hex + f64/f32
  python tools/gdl/pooldump.py 0x80349470 0x803494D0 --f32  # force f32 view
  python tools/gdl/pooldump.py --sym lbl_80349470           # one symbol

Reads orig/GUNE5D/sys/main.dol section headers directly, so it shows the
true constant VALUES behind relocs (fdlibm coefficients, epsilons, magic
doubles) that no reloc-normalized diff can see.
"""

import re
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
DOL = REPO / "orig/GUNE5D/sys/main.dol"
SYMBOLS = REPO / "config/GUNE5D/symbols.txt"


def dol_read(va, size):
    data = DOL.read_bytes()
    offs = struct.unpack(">7I", data[0x00:0x1C]) + struct.unpack(">11I", data[0x1C:0x48])
    # proper parse: 7 text + 11 data offsets, then addresses, then sizes
    text_off = struct.unpack(">7I", data[0x00:0x1C])
    data_off = struct.unpack(">11I", data[0x1C:0x48])
    text_addr = struct.unpack(">7I", data[0x48:0x64])
    data_addr = struct.unpack(">11I", data[0x64:0x90])
    text_size = struct.unpack(">7I", data[0x90:0xAC])
    data_size = struct.unpack(">11I", data[0xAC:0xD8])
    for off, addr, sz in list(zip(text_off, text_addr, text_size)) + \
                         list(zip(data_off, data_addr, data_size)):
        if addr <= va and va + size <= addr + sz:
            fo = off + (va - addr)
            return data[fo:fo + size]
    return None


def main():
    args = sys.argv[1:]
    f32 = "--f32" in args
    args = [a for a in args if not a.startswith("--f32")]
    if args and args[0] == "--sym":
        name = args[1]
        m = re.search(rf"^{re.escape(name)} = \.\w+:0x([0-9A-Fa-f]+); // type:object size:0x([0-9A-Fa-f]+)",
                      SYMBOLS.read_text(encoding="utf-8"), re.M)
        if not m:
            print(f"symbol {name} not found")
            return 1
        lo = int(m.group(1), 16)
        hi = lo + int(m.group(2), 16)
    elif len(args) >= 2:
        lo, hi = int(args[0], 16), int(args[1], 16)
    else:
        print(__doc__)
        return 1

    blob = dol_read(lo, hi - lo)
    if blob is None:
        print("range not inside any DOL section")
        return 1
    va = lo
    i = 0
    while i < len(blob):
        chunk = blob[i:i + 8]
        hexs = chunk.hex()
        line = f"{va + i:08X}: {hexs:<16}"
        if len(chunk) == 8 and not f32:
            line += f"  f64={struct.unpack('>d', chunk)[0]!r}"
            line += f"  f32(hi)={struct.unpack('>f', chunk[:4])[0]!r}"
            i += 8
        elif len(chunk) >= 4:
            line += f"  f32={struct.unpack('>f', chunk[:4])[0]!r}  u32=0x{struct.unpack('>I', chunk[:4])[0]:08X}"
            i += 4
        else:
            i += len(chunk)
        print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
