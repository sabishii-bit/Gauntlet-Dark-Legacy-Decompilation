#!/usr/bin/env python3
"""Print the literal value behind pool/data labels, straight from the retail DOL.

The extern-scalar-read law (claim.law.extern-scalar-read-defeats-fp-literal-
hoist) makes `extern f32 lbl_80348xxx;` reads a matchable defect: the target
loads the CONSTANT from its own function's sdata2 pool, so the C source must
spell the literal, not an extern. Applying it requires knowing each label's
value — which workers were recovering by hand-parsing splitter .s files.

This reads the bytes from orig/GUNE5D/sys/main.dol via config/GUNE5D/
symbols.txt addresses, so it works for any named label in any data section.

Usage:
  python tools/gdl/poolval.py lbl_80347B30 lbl_80347BE8 0x80348010
  python tools/gdl/poolval.py --sweep src/game/game/pmotion.c
      # dump every `extern f32/f64 <label>;` the TU declares — the
      # mechanical entry point for an extern-literal sweep

Interpretations printed per label: raw bytes, f32/f64 where the width fits,
u32/s32, and a printable-string preview. The literal to write into C is the
f32/f64 line for float-class labels (check the symbols.txt data: annotation).
"""

import re
import struct
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
DOL_PATH = REPO_ROOT / "orig" / "GUNE5D" / "sys" / "main.dol"
SYMBOLS = REPO_ROOT / "config" / "GUNE5D" / "symbols.txt"

SYM_RE = re.compile(
    r"^(?P<name>\S+)\s*=\s*(?P<sect>\.\w+):0x(?P<addr>[0-9A-Fa-f]+);"
    r"\s*//(?P<attrs>.*)$")
EXTERN_RE = re.compile(
    r"^\s*extern\s+(?:const\s+)?(?P<ty>f32|f64|float|double)\s+"
    r"(?P<name>\w+)\s*(?:\[\s*\])?\s*;", re.M)


def load_symbols():
    table = {}
    for line in SYMBOLS.read_text(encoding="utf-8",
                                  errors="replace").splitlines():
        m = SYM_RE.match(line.strip())
        if not m:
            continue
        attrs = m.group("attrs")
        size = re.search(r"size:0x([0-9A-Fa-f]+)", attrs)
        kind = re.search(r"data:(\w+)", attrs)
        table[m.group("name")] = {
            "section": m.group("sect"),
            "addr": int(m.group("addr"), 16),
            "size": int(size.group(1), 16) if size else None,
            "data": kind.group(1) if kind else None,
        }
    return table


def load_dol():
    blob = DOL_PATH.read_bytes()
    sections = []
    for i in range(18):  # 7 text + 11 data
        off, addr, size = (
            struct.unpack_from(">I", blob, 0x00 + i * 4)[0],
            struct.unpack_from(">I", blob, 0x48 + i * 4)[0],
            struct.unpack_from(">I", blob, 0x90 + i * 4)[0],
        )
        if off and addr and size:
            sections.append((addr, addr + size, off))
    return blob, sections


def read_va(blob, sections, addr, count):
    for start, end, off in sections:
        if start <= addr < end:
            take = min(count, end - addr)
            return blob[off + (addr - start): off + (addr - start) + take]
    return None


def render(name, info, blob, sections):
    addr = info["addr"]
    size = info["size"] or 4
    data = read_va(blob, sections, addr, max(size, 8))
    if data is None:
        print(f"{name}: 0x{addr:08X} not inside any DOL section (BSS?)")
        return
    shown = data[:size]
    parts = [f"{name} @0x{addr:08X} {info['section']} size 0x{size:X}"
             + (f" data:{info['data']}" if info["data"] else "")]
    parts.append("  bytes " + " ".join(f"{b:02X}" for b in shown[:16])
                 + (" …" if size > 16 else ""))
    if size >= 4:
        u32 = struct.unpack_from(">I", data)[0]
        f32 = struct.unpack_from(">f", data)[0]
        parts.append(f"  f32 {f32!r}   u32 0x{u32:08X} ({u32})"
                     f"   s32 {struct.unpack_from('>i', data)[0]}")
    if size >= 8 or (info["data"] == "double"):
        parts.append(f"  f64 {struct.unpack_from('>d', data)[0]!r}")
    if all(32 <= b < 127 for b in shown.rstrip(b'\x00')) and \
            shown.rstrip(b'\x00'):
        parts.append("  str " + repr(shown.rstrip(b'\x00').decode('ascii',
                                                                  'replace')))
    print("\n".join(parts))


def main():
    args = sys.argv[1:]
    if not args or args[0] in ("--help", "-h"):
        print(__doc__)
        return 2
    table = load_symbols()
    blob, sections = load_dol()
    names = []
    if args[0] == "--sweep":
        source = Path(args[1])
        text = source.read_text(encoding="utf-8", errors="replace")
        names = sorted({m.group("name") for m in EXTERN_RE.finditer(text)})
        print(f"{source}: {len(names)} float-class extern label(s) declared\n")
    else:
        for arg in args:
            if arg.startswith("0x"):
                addr = int(arg, 16)
                hits = [n for n, i in table.items() if i["addr"] == addr]
                names.extend(hits or [arg])
            else:
                names.append(arg)
    missing = []
    for name in names:
        info = table.get(name)
        if info is None:
            missing.append(name)
            continue
        render(name, info, blob, sections)
        print()
    if missing:
        print("NOT IN symbols.txt: " + ", ".join(missing))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
