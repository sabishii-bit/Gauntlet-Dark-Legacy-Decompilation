#!/usr/bin/env python3
"""Own-pool UND ghost screen, image-wide.

A NonMatching TU can reference its OWN .sdata2/.rodata pool entry through an
`extern <T> lbl_<ADDR>` placeholder and still score perfectly, because while
the TU is NonMatching the linker resolves that name against the ORIGINAL
extracted object. Flip the TU to Matching and nothing defines the symbol: the
link fails with `undefined: lbl_<ADDR>`. objdiff, fndiff --classify and
claimcheck are all blind to this
(claim.law.symbolified-own-pool-literal-blocks-tu-flip.20260831.v1).

Worse, the ghost makes the SCORE LOOK BETTER: our relocation borrows the very
placeholder name dtk gave the target's local pool symbol, so the reloc rows
compare equal, and spelling the literal instead (the correct fix) makes fndiff
`real` go UP while the object gets more correct
(claim.law.own-pool-ghost-extern-flatters-fndiff-reloc-rows.20260831.v1).
So this screen can never be inferred from percentages -- it must be run.

THE SOUND SCREEN, and why it is not a grep: grepping source for `extern
lbl_<ADDR>` inside the TU's own pool range over-reports badly, because MWCC
routinely emits those symbols into the TU's own pool as DEFINED symbols, and
an UNREFERENCED extern declaration emits no relocation and cannot fail a link.
The reliable discriminant is the object's UNDEFINED-symbol table: flag a TU
only when `nm -u <obj>` lists an UND lbl_/jumptable_<ADDR> whose address falls
inside that TU's OWN splits.txt range.

RE-RUN IT, DO NOT CITE IT. A "zero ghosts image-wide" result has a shelf life
measured in hours: the 2026-08-31 image-wide negative was contradicted the
same day and this screen found 10 ghosts across 3 TUs shortly after. The whole
pass costs ~30 seconds over 257 objects.

Usage:
  python tools/gdl/ghostsweep.py            # human-readable table
  python tools/gdl/ghostsweep.py --json     # machine-readable
Exit 1 when any ghost is found.
"""

import argparse
import json
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPLITS = ROOT / "config" / "GUNE5D" / "splits.txt"
NM = ROOT / "build" / "binutils" / "powerpc-eabi-nm.exe"
OBJROOT = ROOT / "build" / "GUNE5D" / "src"
CONFIGURE = ROOT / "configure.py"

RANGE_RE = re.compile(
    r"\s*(\S+)\s+start:(0x[0-9A-Fa-f]+)\s+end:(0x[0-9A-Fa-f]+)")
SYM_RE = re.compile(r"^(?:lbl|jumptable)_([0-9A-Fa-f]{8})$")


def parse_splits(path=SPLITS):
    """{unit: [(section, start, end)]} from config/GUNE5D/splits.txt."""
    units, cur, in_header = {}, None, False
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not raw.strip():
            continue
        if not raw.startswith((" ", "\t")):
            name = raw.strip().rstrip(":")
            in_header = name == "Sections"
            cur = None if in_header else name
            if cur:
                units[cur] = []
            continue
        if in_header or cur is None:
            continue
        match = RANGE_RE.match(raw)
        if match:
            units[cur].append(
                (match.group(1), int(match.group(2), 16), int(match.group(3), 16)))
    return units


def nonmatching_units(path=CONFIGURE):
    """The Object(NonMatching, "...") roster from configure.py."""
    text = path.read_text(encoding="utf-8", errors="replace")
    return {m.group(2) for m in
            re.finditer(r'Object\(\s*(\w+)\s*,\s*"([^"]+)"', text)
            if m.group(1) == "NonMatching"}


def undefined_symbols(obj):
    out = subprocess.run([str(NM), "-u", str(obj)],
                         capture_output=True, text=True, check=True).stdout
    return [line.split()[-1] for line in out.splitlines() if line.split()]


def sweep():
    units = parse_splits()
    nonmatching = nonmatching_units()
    ghosts, scanned, missing = [], 0, []
    for unit, ranges in sorted(units.items()):
        obj = OBJROOT / (unit.rsplit(".", 1)[0] + ".o")
        if not obj.exists():
            missing.append(unit)
            continue
        scanned += 1
        for name in undefined_symbols(obj):
            match = SYM_RE.match(name)
            if not match:
                continue
            addr = int(match.group(1), 16)
            owned = [r for r in ranges if r[1] <= addr < r[2]]
            if owned:
                ghosts.append({
                    "unit": unit,
                    "nonmatching": unit in nonmatching,
                    "symbol": name,
                    "addr": "0x%08X" % addr,
                    "sections": ["%s [0x%08X,0x%08X)" % s for s in owned],
                })
    return {"objects_scanned": scanned, "units_without_object": missing,
            "nonmatching_units": len(nonmatching), "ghosts": ghosts}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    result = sweep()
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        by_unit = defaultdict(list)
        for ghost in result["ghosts"]:
            by_unit[ghost["unit"]].append(ghost)
        for unit, found in sorted(by_unit.items()):
            flag = "" if found[0]["nonmatching"] else "  [MATCHING TU]"
            print("%s%s" % (unit, flag))
            for ghost in found:
                print("    %-16s %s  in own %s"
                      % (ghost["symbol"], ghost["addr"], ", ".join(ghost["sections"])))
        print("\n%d own-pool UND ghost(s) in %d TU(s); %d objects scanned, "
              "%d NonMatching units in roster"
              % (len(result["ghosts"]), len(by_unit), result["objects_scanned"],
                 result["nonmatching_units"]))
        if not result["ghosts"]:
            print("NOTE: a zero result is only true for THIS build. Re-run the "
                  "screen; never cite a previous negative.")
    return 1 if result["ghosts"] else 0


if __name__ == "__main__":
    sys.exit(main())
