#!/usr/bin/env python3
"""Query the Xbox PDB module/function inventory (functions_by_module.txt).

The PDB records functions in per-module source order, so once one function of
a GCN TU is anchored, neighbors in the module usually name the neighboring GCN
functions too.

Usage (from repo root):
  python tools/gdl/xbmod.py <pattern>            # find symbols, show their module
  python tools/gdl/xbmod.py --module gcontrolpads   # list a module's code symbols in order
  python tools/gdl/xbmod.py --modules            # list all module names
"""

import argparse
import re
import sys
from pathlib import Path

# The generated text inventory is private workflow memory, but older checkouts
# kept it under research/.  Prefer the current location and retain the legacy
# fallback so the helper works across both layouts.
ROOT = Path(__file__).resolve().parent.parent.parent
TXT_CANDIDATES = (
    ROOT / "research" / "xbox_symbols" / "functions_by_module.txt",
    ROOT / "research" / "xbox_symbols" / "functions_by_module.txt",
)
TXT = next((path for path in TXT_CANDIDATES if path.exists()), TXT_CANDIDATES[0])

MOD_RE = re.compile(r"^== \.\\Release\\(.+?) \(")
SYM_RE = re.compile(r"^\[(\d{4}):([0-9A-Fa-f]{8})\]\s+([0-9A-Fa-f]+)\s+([GLD])\s+(.*)$")


def load():
    """[(module, [(seg, off, size, kind, name)])] preserving file order."""
    mods = []
    cur = None
    for line in TXT.read_text(encoding="utf-8", errors="replace").splitlines():
        m = MOD_RE.match(line)
        if m:
            cur = (m.group(1), [])
            mods.append(cur)
            continue
        m = SYM_RE.match(line)
        if m and cur is not None:
            seg, off, size, kind, name = m.groups()
            cur[1].append((seg, int(off, 16), int(size, 16), kind, name.strip()))
    return mods


def code_syms(syms):
    return [s for s in syms if s[0] == "0001" and s[4]]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pattern", nargs="?", help="case-insensitive substring/regex over symbol names")
    ap.add_argument("--module", help="list one module's code symbols in source order")
    ap.add_argument("--modules", action="store_true", help="list all module names")
    ap.add_argument("--data", action="store_true", help="include data symbols too")
    args = ap.parse_args()

    mods = load()

    if args.modules:
        for name, syms in mods:
            print(f"{name:40} {len(code_syms(syms)):4} fns")
        return 0

    if args.module:
        pat = args.module.lower()
        for name, syms in mods:
            if pat in name.lower():
                print(f"== {name}")
                shown = syms if args.data else code_syms(syms)
                for seg, off, size, kind, sym in shown:
                    tag = {"G": "global", "L": "local", "D": "data"}.get(kind, kind)
                    print(f"  [{seg}:{off:08X}] size {size:5X} {tag:6} {sym}")
        return 0

    if not args.pattern:
        print(__doc__)
        return 1

    rx = re.compile(args.pattern, re.I)
    for name, syms in mods:
        hits = [s for s in syms if rx.search(s[4])]
        if not hits:
            continue
        for seg, off, size, kind, sym in hits:
            print(f"{name:32} [{seg}:{off:08X}] size {size:5X} {kind} {sym}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
