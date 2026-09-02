#!/usr/bin/env python3
"""Dump constant pool VALUES — from the retail DOL, or from OUR own object.

Usage:
  python tools/gdl/pooldump.py 0x80349470 0x803494D0        # hex + f64/f32
  python tools/gdl/pooldump.py 0x80349470 0x803494D0 --f32  # force f32 view
  python tools/gdl/pooldump.py --sym lbl_80349470           # one symbol
  python tools/gdl/pooldump.py --ours game/pb/pb_window     # OUR pool, all
  python tools/gdl/pooldump.py --ours game/pb/pb_window @674 @51   # named

The retail modes read orig/GUNE5D/sys/main.dol section headers directly, so
they show the true constant VALUES behind relocs (fdlibm coefficients,
epsilons, magic doubles) that no reloc-normalized diff can see.

--ours ANSWERS THE OTHER HALF OF THE QUESTION (run-38 item 8). Every
inspector in this project resolved names through config/GUNE5D/symbols.txt,
which describes the RETAIL image only: our unlinked object names its pool
entries `@674`, those names are in no symbol table but our own, and they
have no virtual address at all — so "what is @674 in OUR object" returned
`symbol @674 not found` and a worker had to guess. That guess was a wrong
diagnosis two runs running. --ours reads the pool straight out of
build/GUNE5D/src/<unit>.o (the pre-postprocess body object when there is
one, so a pinned TU shows the COMPILER's pool rather than the
postprocessor's) and prints the same interpretations.

Pair it with `fndiff --relocs`, whose positional pass names WHICH
relocation points at a different address; this says what the two addresses
actually contain.
"""

import re
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
DOL = REPO / "orig/GUNE5D/sys/main.dol"
SYMBOLS = REPO / "config/GUNE5D/symbols.txt"
VERSION = "GUNE5D"
POOL_SECTIONS = (".sdata2", ".rodata", ".sdata", ".data")


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


def our_object(unit):
    """(path, note) for OUR object of `unit`, body form preferred.

    On a WebFrank-pinned TU the plain src/ object is POST-rewrite, so its
    pool is the postprocessor's view. The question "what did the compiler
    put in the pool" is answered by the .postprocess/body object.
    """
    unit = unit.replace("\\", "/").strip("/")
    if unit.startswith("src/"):
        unit = unit[len("src/"):]
    unit = re.sub(r"\.(c|cpp)$", "", unit)
    plain = REPO / f"build/{VERSION}/src/{unit}.o"
    body = plain.parent / ".postprocess" / "body" / plain.name
    if body.is_file():
        return body, "pre-webfrank compiler output (.postprocess/body)"
    return plain, "compiled object"


def our_pool(objfile):
    """[(name, section, offset, size, bytes)] for every pool datum in OUR
    object, in section/offset order."""
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from webfrank import _sections, _symbols  # noqa: E402

    data = objfile.read_bytes()
    sections = _sections(data)
    wanted = {index: section for index, section in enumerate(sections)
              if section.name in POOL_SECTIONS}
    rows = []
    for symbol in _symbols(data, sections):
        section = wanted.get(symbol.section_index)
        if section is None or symbol.size == 0:
            continue
        blob = data[section.offset + symbol.value:
                    section.offset + symbol.value + symbol.size]
        rows.append((symbol.name, section.name, symbol.value, symbol.size,
                     blob))
    rows.sort(key=lambda row: (row[1], row[2]))
    return rows


def describe(blob):
    """Every interpretation that fits the width, as one string."""
    parts = [blob.hex()]
    if len(blob) == 8:
        parts.append(f"f64={struct.unpack('>d', blob)[0]!r}")
    if len(blob) >= 4:
        parts.append(f"f32={struct.unpack('>f', blob[:4])[0]!r}")
        parts.append(f"u32=0x{struct.unpack('>I', blob[:4])[0]:08X}")
    # Only for something that actually IS a string. Showing a preview
    # whenever ANY byte happened to be printable decorated every float in
    # the pool with `str="?..."`, which is noise standing exactly where a
    # real string would go.
    body = blob[:-1] if blob.endswith(b"\0") else blob
    if body and all(32 <= byte < 127 or byte in (9, 10, 13) for byte in body):
        preview = body[:40].decode("ascii").replace("\n", "\\n")
        parts.append(f'str="{preview}"'
                     + ("..." if len(body) > 40 else ""))
    return "  ".join(parts)


def dump_ours(unit, names):
    objfile, note = our_object(unit)
    if not objfile.exists():
        print(f"missing: {objfile} — run ninja for this unit first")
        return 1
    rows = our_pool(objfile)
    if names:
        wanted = set(names)
        rows = [row for row in rows if row[0] in wanted]
        missing = wanted - {row[0] for row in rows}
        for name in sorted(missing):
            print(f"{name}: no such pool datum in {objfile.name}"
                  " (our object's own symbol table is the ONLY authority for"
                  " an @N name — symbols.txt describes the retail image and"
                  " never carries one)")
    print(f"OUR POOL {unit}  ({objfile.name}, {note}): {len(rows)} entr(ies)")
    for name, section, offset, size, blob in rows:
        print(f"  {name:>12}  {section:<9} +0x{offset:04x} ({size}B)"
              f"  {describe(blob)}")
    if not rows and not names:
        print("  (no sized pool datum in this object)")
    return 0


def main():
    args = sys.argv[1:]
    f32 = "--f32" in args
    args = [a for a in args if not a.startswith("--f32")]
    if args and args[0] == "--ours":
        if len(args) < 2:
            print(__doc__)
            return 1
        return dump_ours(args[1], args[2:])
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
