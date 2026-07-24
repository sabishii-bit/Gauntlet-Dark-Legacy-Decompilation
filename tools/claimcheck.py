#!/usr/bin/env python3
"""Section-claim linter: compare the sections our compiled object actually
emits against the ranges claimed for that unit in splits.txt. Catches the
"object emits a .data jumptable nobody claimed" class of failure BEFORE the
link silently shifts every later section (see infcodes, 6aeec7b).

Usage (from repo root):
  python tools/claimcheck.py zlib/inflate            # one unit
  python tools/claimcheck.py zlib/inflate game/pbutils
  python tools/claimcheck.py --matching              # every Matching unit in configure.py

Exit code 1 if any hard problem found (unclaimed emitted section, or object
bigger than its claim).
"""

import re
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parent.parent
OBJDUMP = REPO / "build" / "binutils" / "powerpc-eabi-objdump.exe"
SPLITS = REPO / "config" / VERSION / "splits.txt"

# dtk attributes exception tables to claimed units automatically; their claim
# lines appear/regenerate on configure, so their absence is not an error.
AUTO_SECTIONS = {"extab", "extabindex"}
# Compiler metadata that never reaches the DOL.
IGNORE_SECTIONS = {".comment", ".mwcats"}
# Alignment padding commonly folded into the claim's end.
PAD_SILENT = 8
PAD_INFO = 32


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


def object_sections(obj: Path):
    out = subprocess.run([str(OBJDUMP), "-h", str(obj)], capture_output=True, text=True).stdout
    secs = {}
    for line in out.splitlines():
        m = re.match(r"^\s*\d+\s+(\S+)\s+([0-9a-f]{8})\s", line)
        if m:
            size = int(m.group(2), 16)
            if size:
                secs[m.group(1)] = size
    return secs


def check_unit(unit: str, claims_by_unit, verbose=True) -> int:
    """Only the 'emitted section with NO claim' class is a hard error: that is
    what silently shifts every later section at link (infcodes, 6aeec7b).
    Size mismatches in either direction are advisory because mwld dead-strips
    unreferenced functions AND statics from Matching objects (vi.c, DEMOInit
    etc. are sha1-green with object > claim)."""
    unit_c = unit if unit.endswith((".c", ".cpp")) else unit + ".c"
    obj = REPO / "build" / VERSION / "src" / re.sub(r"\.(c|cpp)$", ".o", unit_c)
    if not obj.exists():
        print(f"[{unit_c}] SKIP: object not built ({obj})")
        return 0
    claims = claims_by_unit.get(unit_c)
    if claims is None:
        print(f"[{unit_c}] WARN: no splits.txt entry")
        return 0

    problems = 0
    notes = []
    secs = {n: s for n, s in object_sections(obj).items() if n not in IGNORE_SECTIONS}
    for name, size in sorted(secs.items()):
        if name in AUTO_SECTIONS:
            continue  # dtk attributes extab/extabindex itself
        if name not in claims:
            print(f"[{unit_c}] ERROR: object emits {name} ({size:#x}) but splits.txt claims nothing"
                  f" -- link will shift every later section (jumptable? static data?)")
            problems += 1
            continue
        start, end = claims[name]
        claim = end - start
        if size > claim:
            notes.append(f"{name} obj {size:#x} > claim {claim:#x}"
                         f" (dead-strip is normal; if NEW data appeared, widen the claim)")
        elif claim - size >= PAD_INFO:
            notes.append(f"{name} claim exceeds obj by {claim - size:#x}")
    for name in claims:
        if name not in secs and name not in AUTO_SECTIONS:
            notes.append(f"claimed {name} but object emits none")
    if problems:
        return problems
    if verbose:
        summary = ", ".join(f"{n} {s:#x}" for n, s in sorted(secs.items()))
        print(f"[{unit_c}] ok ({summary})")
        for n in notes:
            print(f"[{unit_c}]   note: {n}")
    return 0


def matching_units():
    text = (REPO / "configure.py").read_text(encoding="utf-8")
    return re.findall(r'Object\(Matching, "([^"]+)"', text)


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 1
    units = matching_units() if args == ["--matching"] else args
    claims = parse_splits()
    problems = 0
    for u in units:
        u = re.sub(r"\.(c|cpp)$", "", u.replace("\\", "/"))
        problems += check_unit(u, claims)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
