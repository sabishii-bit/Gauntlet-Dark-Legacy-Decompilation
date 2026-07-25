#!/usr/bin/env python3
"""Scan a target object for functions matching a compiled reference source.

Compiles a reference .c (e.g. a melee MSL TU) with mwcc, opcode-normalizes
every function, then aligns them against the functions of a target object
(dtk-extracted unit obj or auto_* obj). Prints, in target address order,
the best reference match per target function with a similarity score.

Use it to identify TU boundaries and function names in unclaimed regions
when public/reference source exists -- the discovery step before writing
splits. Scores: 1.000 = identical opcode stream (registers/relocs ignored;
real flag/codegen differences still show as <1).

Usage:
  python tools/refscan.py W:/path/ref/printf.c auto_03_800E539C \\
      -i W:/path/ref [--cflags "..."] [--mw GC/1.2.5] [--min 0.5]

Target may be a unit path (game/foo) or an auto object stem (auto_03_...).
"""

import argparse
import difflib
import re
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
OBJDUMP = Path("build/binutils/powerpc-eabi-objdump.exe")
SJISWRAP = Path("build/tools/sjiswrap.exe")
SCRATCH = Path("build/refscan")

DEFAULT_CFLAGS = ("-nodefaults -proc gekko -align powerpc -enum int -fp hardware "
                  "-O4,p -inline auto -nosyspath -maxerrors 1 -pragma \"cats off\"")


def ops_of(objfile: Path):
    """{fn: [opcodes]} with size info."""
    out = subprocess.run([str(OBJDUMP), "-d", str(objfile)],
                         capture_output=True, text=True).stdout
    funcs, cur = {}, None
    for line in out.splitlines():
        m = re.match(r"^[0-9a-f]+ <(.+)>:$", line)
        if m:
            cur = m.group(1)
            funcs[cur] = []
            continue
        m = re.match(r"^\s+[0-9a-f]+:\s+(?:[0-9a-f]{2} ){4}\s*(\S+)", line)
        if m and cur:
            funcs[cur].append(m.group(1))
    return {k: v for k, v in funcs.items() if v}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ref", help="reference .c source")
    ap.add_argument("target", help="unit path or auto obj stem")
    ap.add_argument("-i", action="append", default=[], help="extra include dir")
    ap.add_argument("--cflags", default=DEFAULT_CFLAGS)
    ap.add_argument("--mw", default="GC/1.2.5")
    ap.add_argument("--min", type=float, default=0.45, help="min score to report")
    args = ap.parse_args()

    ref = Path(args.ref)
    SCRATCH.mkdir(parents=True, exist_ok=True)
    obj = SCRATCH / (ref.stem + ".o")
    cc = f"build/compilers/{args.mw}/mwcceppc.exe"
    inc = " ".join(f"-i {d}" for d in ["include", str(ref.parent)] + args.i)
    cmd = f'{SJISWRAP} {cc} {args.cflags} {inc} -c {ref} -o {obj}'
    r = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if r.returncode != 0 or not obj.exists():
        print(f"REF COMPILE FAILED ({ref.name}):")
        print((r.stdout + r.stderr)[-1500:])
        return 1

    # locate target object
    t = args.target
    cands = [Path(f"build/{VERSION}/obj/{t}.o"),
             Path(f"build/{VERSION}/obj/{t}_text.o"),
             Path(f"build/{VERSION}/obj/{t}")]
    tobj = next((p for p in cands if p.exists()), None)
    if tobj is None:
        print(f"target object not found, tried: {[str(c) for c in cands]}")
        return 1

    rfns = ops_of(obj)
    tfns = ops_of(tobj)
    print(f"ref {ref.name}: {len(rfns)} fns   target {tobj.name}: {len(tfns)} fns")
    used = set()
    for tname, tops in tfns.items():
        best, score = None, 0.0
        for rname, rops in rfns.items():
            if abs(len(rops) - len(tops)) > max(8, len(tops) // 2):
                continue
            s = difflib.SequenceMatcher(a=tops, b=rops, autojunk=False).ratio()
            if s > score:
                best, score = rname, s
        if best and score >= args.min:
            mark = "*" if best in used else " "
            used.add(best)
            print(f"  {tname:<24} -> {best:<28} {score:.3f} ({len(tops)}/{len(rfns[best])} ops){mark}")
        else:
            print(f"  {tname:<24} -> ?                        ({len(tops)} ops)")
    missing = [r for r in rfns if r not in used]
    if missing:
        print("ref fns unmatched:", ", ".join(missing))
    return 0


if __name__ == "__main__":
    sys.exit(main())
