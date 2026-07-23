#!/usr/bin/env python3
"""Per-function disassembly diff between the target object (extracted from the
DOL by dtk) and the base object (our compile), with addresses and branch
targets normalized so only real codegen differences survive.

Usage:
  python tools/fndiff.py dolphin/dvd/dvd.c              # all mismatching functions
  python tools/fndiff.py dolphin/dvd/dvd.c DVDInit      # specific function(s)
  python tools/fndiff.py dolphin/si/SIBios.c -l         # just list match status

Run from the repo root after `ninja build/GUNE5D/src/<unit>.o`.
"""

import difflib
import re
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
OBJDUMP = Path("build/binutils/powerpc-eabi-objdump.exe")

BRANCH_RE = re.compile(
    r"\b(b|bl|ba|bla|beq|bne|bgt|blt|bge|ble|bso|bns|bdnz|bdz)([+-]?)\s+[0-9a-f]+\s*$"
)


def parse(objfile: Path):
    """Return {function_name: [normalized instruction/reloc lines]}."""
    out = subprocess.run(
        [str(OBJDUMP), "-dr", str(objfile)], capture_output=True, text=True
    ).stdout
    funcs = {}
    cur = None
    for line in out.splitlines():
        m = re.match(r"^[0-9a-f]+ <(.+)>:$", line)
        if m:
            cur = m.group(1)
            funcs[cur] = []
            continue
        if cur is None:
            continue
        m = re.match(r"^\s+[0-9a-f]+:\s+(?:[0-9a-f]{2} ){4}\s*(.+)$", line)
        if m:
            ins = re.sub(r"<[^>]+>", "", m.group(1).strip())
            ins = BRANCH_RE.sub(r"\1\2 <tgt>", ins)
            funcs[cur].append(ins.strip())
        elif "R_PPC" in line:
            rel = line.strip().split(maxsplit=1)[1]
            # dtk suffixes local symbol names with their address; strip so
            # target "changed_80345368" pairs with our "changed"
            rel = re.sub(r"_80[0-9A-Fa-f]{6}(?=$|\+)", "", rel)
            funcs[cur].append("    " + rel)
    return funcs


def main():
    args = [a for a in sys.argv[1:] if a != "-l"]
    list_only = "-l" in sys.argv
    if not args:
        print(__doc__)
        return 1

    unit = args[0].replace("\\", "/")
    unit = re.sub(r"\.(c|cpp)$", "", unit)
    target_o = Path(f"build/{VERSION}/obj/{unit}.o")
    base_o = Path(f"build/{VERSION}/src/{unit}.o")
    for p in (target_o, base_o):
        if not p.exists():
            print(f"missing: {p} (run ninja / check unit path)")
            return 1

    target, base = parse(target_o), parse(base_o)
    names = args[1:] or sorted(
        set(target) | set(base), key=lambda n: list(target).index(n) if n in target else 999
    )

    for name in names:
        t, b = target.get(name), base.get(name)
        if t == b:
            if list_only or args[1:]:
                print(f"OK   {name}")
            continue
        if t is None or b is None:
            side = "target" if t is None else "base"
            print(f"ONLY-IN-{'BASE' if t is None else 'TARGET'}  {name}"
                  f"  (extra {side} fns are usually deadstripped statics)")
            continue
        if list_only:
            print(f"DIFF {name}")
            continue
        print("=" * 20, name)
        for line in difflib.unified_diff(t, b, "target", "base", lineterm="", n=2):
            print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
