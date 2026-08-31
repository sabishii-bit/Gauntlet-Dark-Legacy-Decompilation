#!/usr/bin/env python3
"""Slice TARGET functions by opcode shape — the positive-control finder.

The recorded way to settle "which C spelling does MWCC compile to this
shape?" is to find a function that is ALREADY byte-exact and contains the
shape, then read its source (the GetMilestonePos move, banked from the
alias-lever lane). lowmatch/nearmiss slice by score; nothing sliced by
opcode shape until now — workers kept writing this as a throwaway script.

Scans the split target disassembly (build/GUNE5D/asm/**/*.s, dtk `.fn`
format) for a consecutive-opcode sequence, attributes each hit to its
function, and joins fuzzy scores from build/GUNE5D/report.json.

Usage:
  python tools/gdl/shapegrep.py lfs,fcmpo,bge            # any function
  python tools/gdl/shapegrep.py add,lwzu --exact-only    # positive controls
  python tools/gdl/shapegrep.py stmw --max 40 --min-fuzzy 99

Pattern = comma-separated opcodes matched against CONSECUTIVE instructions.
`.` matches any single opcode. Output: function, fuzzy, hit offset(s), file.
"""

import json
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
ASM_DIR = REPO_ROOT / "build" / "GUNE5D" / "asm"
REPORT = REPO_ROOT / "build" / "GUNE5D" / "report.json"

FN_RE = re.compile(r"^\.fn\s+(\w+)")
ENDFN_RE = re.compile(r"^\.endfn")
INSN_RE = re.compile(r"^/\* \w{8} \w{8}  (?:\w\w ){4}\*/\t(\w+)")


def load_scores():
    scores = {}
    if REPORT.exists():
        report = json.loads(REPORT.read_text(encoding="utf-8"))
        for unit in report.get("units", []):
            for fn in unit.get("functions", []):
                scores[fn["name"]] = float(
                    fn.get("fuzzy_match_percent", 0.0))
    return scores


def main():
    args = sys.argv[1:]
    if not args or args[0] in ("--help", "-h"):
        print(__doc__)
        return 2
    pattern = [op.strip() for op in args[0].split(",") if op.strip()]
    exact_only = "--exact-only" in args
    max_hits = 25
    min_fuzzy = None
    if "--max" in args:
        max_hits = int(args[args.index("--max") + 1])
    if "--min-fuzzy" in args:
        min_fuzzy = float(args[args.index("--min-fuzzy") + 1])
    scores = load_scores()
    if exact_only and not scores:
        print("shapegrep: --exact-only needs build/GUNE5D/report.json"
              " (run a full ninja first)", file=sys.stderr)
        return 2
    hits = []
    for path in sorted(ASM_DIR.rglob("*.s")):
        fn_name = None
        window: list[tuple[str, int]] = []  # (opcode, index-in-fn)
        idx = 0
        for line in path.read_text(encoding="utf-8",
                                   errors="replace").splitlines():
            m = FN_RE.match(line)
            if m:
                fn_name, window, idx = m.group(1), [], 0
                continue
            if ENDFN_RE.match(line):
                fn_name = None
                continue
            if fn_name is None:
                continue
            m = INSN_RE.match(line)
            if not m:
                continue
            window.append((m.group(1), idx))
            idx += 1
            if len(window) > len(pattern):
                window.pop(0)
            if len(window) == len(pattern) and all(
                    p == "." or p == op for p, (op, _) in
                    zip(pattern, window)):
                fuzzy = scores.get(fn_name)
                if exact_only and fuzzy != 100.0:
                    continue
                if min_fuzzy is not None and (fuzzy is None
                                              or fuzzy < min_fuzzy):
                    continue
                hits.append((fn_name, fuzzy, window[0][1] * 4, path.name))
    if not hits:
        print("no hits" + (" among byte-exact functions" if exact_only
                           else ""))
        return 0
    seen_fns = set()
    shown = 0
    for fn_name, fuzzy, offset, fname in hits:
        marker = "" if fn_name not in seen_fns else "  (again)"
        seen_fns.add(fn_name)
        fz = f"{fuzzy:.2f}%" if fuzzy is not None else "  n/a"
        print(f"{fz:>8} {fn_name:40s} @+0x{offset:X}  {fname}{marker}")
        shown += 1
        if shown >= max_hits:
            print(f"... ({len(hits) - shown} more hits suppressed;"
                  " raise --max)")
            break
    distinct = len({hit[0] for hit in hits})
    print(f"[{len(hits)} hit(s), {distinct} function(s); source for a"
          " byte-exact hit = the proven spelling for this shape]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
