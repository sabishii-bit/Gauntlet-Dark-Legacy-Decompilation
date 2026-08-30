#!/usr/bin/env python3
"""Compact target-asm dump for one function, from the dtk-extracted object.

Usage:
  python tools/gdl/fnasm.py game/pb_window pbProjCalc          # whole function
  python tools/gdl/fnasm.py game/pb_window pbProjCalc 40:120   # insn index slice
  python tools/gdl/fnasm.py game/pb_window pbProjCalc 0x68:0xa0  # offset slice
  python tools/gdl/fnasm.py game/pb_window pbProjCalc --ours   # OUR built object
  python tools/gdl/fnasm.py game/pb_window                     # list functions

Output is one line per instruction: function-relative hex offset, mnemonic,
operands, with any relocation folded onto the same line ("@sym"). Branch
targets stay as real function-relative offsets so label adjacency can be
verified (bge-to-next vs bge-over-one is visible at a glance).

Slices with a 0x prefix are byte-offset ranges (matching the printed offsets
and fndiff --ops @0x annotations); bare numbers are instruction indices.

Default reads build/GUNE5D/obj/<unit>.o (the dtk-extracted target), so it
works before any source exists and is immune to stale-object issues.
--ours reads build/GUNE5D/src/<unit>.o (our compile) instead — run ninja on
the object first or the dump is stale.

OBJDUMP is resolved to an absolute path so this works from any cwd/script.
"""

import re
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
OBJDUMP = (Path(__file__).resolve().parents[2]
           / "build" / "binutils" / "powerpc-eabi-objdump.exe")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    ours = "--ours" in sys.argv
    if not args or args[0] in ("--help", "-h", "help"):
        print(__doc__)
        return 1
    unit = args[0].replace("\\", "/").strip("/")
    if unit.startswith("src/"):
        unit = unit[len("src/"):]
    unit = re.sub(r"\.(c|cpp)$", "", unit)
    fn = args[1] if len(args) > 1 else None
    lo, hi = 0, 1 << 30
    by_offset = False
    if len(args) > 2 and ":" in args[2]:
        a, b = args[2].split(":")
        by_offset = a.startswith("0x") or b.startswith("0x")
        conv = (lambda s, d: int(s, 16) if s else d) if by_offset \
            else (lambda s, d: int(s) if s else d)
        lo, hi = conv(a, 0), conv(b, 1 << 30)

    kind = "src" if ours else "obj"
    obj = Path(f"build/{VERSION}/{kind}/{unit}.o")
    if not obj.exists():
        # dtk merges runs of tiny fns into auto_03_* objects and names auto
        # units after their first fn; try the common variants before giving up
        for cand in (f"auto_03_{unit}_text", f"auto_{unit}_text",
                     f"auto_03_{unit.replace('fn_', '')}_text"):
            c = Path(f"build/{VERSION}/obj/{cand}.o")
            if c.exists():
                obj = c
                break
    if not obj.exists():
        hint = (f"run ninja build/{VERSION}/src/{unit}.o first" if ours
                else "run ninja once so dtk extracts it")
        print(f"missing {obj} ({hint})")
        return 1
    out = subprocess.run([str(OBJDUMP), "-dr", str(obj)],
                         capture_output=True, text=True).stdout

    cur = None
    base = 0
    rows = []      # (offset, text) for the selected function
    names = []
    for line in out.splitlines():
        m = re.match(r"^([0-9a-f]+) <(.+)>:$", line)
        if m:
            cur = m.group(2)
            base = int(m.group(1), 16)
            names.append(cur)
            continue
        if cur != fn:
            continue
        m = re.match(r"^\s+([0-9a-f]+):\s+(?:[0-9a-f]{2} ){4}\s*(.+)$", line)
        if m:
            off = int(m.group(1), 16) - base
            ins = m.group(2).strip()
            # rewrite absolute branch targets as function-relative offsets
            bm = re.search(r"\b([0-9a-f]+) <[^>]*>\s*$", ins)
            if bm:
                tgt = int(bm.group(1), 16) - base
                ins = ins[: bm.start()] + f"->{tgt:x}"
            ins = re.sub(r"\s+", " ", ins)
            rows.append([off, ins])
        elif "R_PPC" in line and rows:
            parts = line.strip().split()
            rows[-1][1] += f"  @{parts[-1]}({parts[-2].replace('R_PPC_', '')})"

    if fn is None:
        print("\n".join(names))
        return 0
    if not rows:
        print(f"function {fn} not found; has: {', '.join(names)}")
        return 1
    for i, (off, ins) in enumerate(rows):
        key = off if by_offset else i
        if lo <= key < hi:
            print(f"{off:4x}: {ins}")
    print(f"[{len(rows)} insns{' (ours)' if ours else ''}]")
    return 0


if __name__ == "__main__":
    sys.exit(main())
