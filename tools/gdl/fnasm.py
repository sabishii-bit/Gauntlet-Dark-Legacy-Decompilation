#!/usr/bin/env python3
"""Compact target-asm dump for one function, from the dtk-extracted object.

Usage:
  python tools/gdl/fnasm.py game/pb_window pbProjCalc          # whole function
  python tools/gdl/fnasm.py game/pb_window pbProjCalc 40:120   # insn index slice
  python tools/gdl/fnasm.py game/pb_window                     # list functions

Output is one line per instruction: function-relative hex offset, mnemonic,
operands, with any relocation folded onto the same line ("@sym"). Branch
targets stay as real function-relative offsets so label adjacency can be
verified (bge-to-next vs bge-over-one is visible at a glance).

Reads build/GUNE5D/obj/<unit>.o (the dtk-extracted target), never our build,
so it works before any source exists and is immune to stale-object issues.
"""

import re
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
OBJDUMP = Path("build/binutils/powerpc-eabi-objdump.exe")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    unit = re.sub(r"\.(c|cpp)$", "", sys.argv[1].replace("\\", "/"))
    fn = sys.argv[2] if len(sys.argv) > 2 else None
    lo, hi = 0, 1 << 30
    if len(sys.argv) > 3 and ":" in sys.argv[3]:
        a, b = sys.argv[3].split(":")
        lo, hi = int(a or 0), int(b or (1 << 30))

    obj = Path(f"build/{VERSION}/obj/{unit}.o")
    if not obj.exists():
        print(f"missing {obj} (run ninja once so dtk extracts it)")
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
        if lo <= i < hi:
            print(f"{off:4x}: {ins}")
    print(f"[{len(rows)} insns]")
    return 0


if __name__ == "__main__":
    sys.exit(main())
