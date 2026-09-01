"""Oracle census for two MWCC codegen shapes, joined against exactness.

Both modes walk the TARGET assembly under build/GUNE5D/asm/**/*.s and join
each enclosing function against build/GUNE5D/report.json.  A site inside a
function at fuzzy_match_percent == 100 is an ORACLE: its C source is a
VERIFIED producer of the shape under this project's real cflags, so it can
be read and copied instead of the shape being re-derived by guesswork.

  branch  (default)  `bcc ->(addr+8)` immediately followed by an
                     unconditional `b` -- a conditional branch hopping OVER
                     a branch, which our build otherwise fuses into a single
                     inverted conditional.  This is the shape
                     claim.law.GW_unfused-conditional-over-branch-is-a-
                     conditional-EXPRESSION-tell.20260901.v1 is about; that
                     law cites a census script that was never promoted, so
                     this is its runnable replacement.

  addic              `addic.`/`subic.` feeding an `mtctr` counted loop (and
                     usually a `beq` zero-trip guard) -- the fused
                     subtract-and-record form of
                     claim.law.MC_addic-fusion-needs-signed-counter-and-
                     unhoisted-subtraction.20260901.v1.

TWO JOIN HAZARDS, both hit and fixed while writing this (either one silently
yields "0 oracles", which reads as "shape unreachable" rather than "census
broken" -- always sanity-check the totals):

  1. report.json unit names carry a "main/" prefix; asm-derived paths do
     not.  An exact-string join therefore matches nothing.
  2. dtk renders `addic.` with a negative immediate using the extended
     mnemonic `subic.`, while tools/gdl/fnasm.py renders the same
     instruction word as `addic.`.  Matching only "addic." finds zero sites
     in a DOL that demonstrably contains them.

Usage, from the repository root:

    python tools/gdl/composed_census/ws_unfused_branch_census.py [branch|addic]
"""
import json
import os
import re
import sys

INSN = re.compile(
    r"^/\* ([0-9A-F]{8}) [0-9A-F]{8} +[0-9A-F ]+\*/\s+(\S+)\s*(.*)$")
FN = re.compile(r"^\.fn\s+([^,]+),")
ENDFN = re.compile(r"^\.endfn")
LABEL_ADDR = re.compile(r"\.L_([0-9A-F]{8})")

COND = ("beq", "bne", "blt", "bgt", "ble", "bge", "bso", "bns", "bc")


def paths():
    root = os.getcwd()
    asm = os.path.join(root, "build", "GUNE5D", "asm")
    report = os.path.join(root, "build", "GUNE5D", "report.json")
    if not os.path.isdir(asm) or not os.path.isfile(report):
        raise SystemExit(
            "run from the repository root: missing build/GUNE5D/asm or "
            "build/GUNE5D/report.json (run a full ninja first)")
    return asm, report


def exact_functions(report):
    with open(report, "r", encoding="utf-8") as fh:
        rep = json.load(fh)
    out = {}
    for unit in rep.get("units", []):
        name = unit.get("name") or ""
        if name.startswith("main/"):          # hazard 1
            name = name[len("main/"):]
        for fn in (unit.get("functions") or []):
            if fn.get("name"):
                out[(name, fn["name"])] = fn.get("fuzzy_match_percent")
    if not out:
        raise SystemExit("JOIN EMPTY: report.json yielded no functions")
    return out


def walk(asm, check):
    hits = []
    for dirpath, _dirs, files in os.walk(asm):
        for name in sorted(files):
            if not name.endswith(".s"):
                continue
            path = os.path.join(dirpath, name)
            unit = os.path.relpath(path, asm).replace("\\", "/")[:-2]
            cur, insns = None, []
            with open(path, "r", encoding="utf-8", errors="replace") as fh:
                for line in fh:
                    m = FN.match(line)
                    if m:
                        cur, insns = m.group(1).strip(), []
                        continue
                    if ENDFN.match(line):
                        if cur:
                            hits.extend(check(unit, cur, insns))
                        cur, insns = None, []
                        continue
                    if cur is None:
                        continue
                    m = INSN.match(line.rstrip("\n"))
                    if m:
                        insns.append((int(m.group(1), 16), m.group(2),
                                      m.group(3).strip()))
    return hits


def check_branch(unit, fn, insns):
    out = []
    for i in range(len(insns) - 1):
        addr, op, args = insns[i]
        naddr, nop, _na = insns[i + 1]
        if op not in COND or nop != "b" or naddr != addr + 4:
            continue
        m = LABEL_ADDR.search(args)
        if m and int(m.group(1), 16) == addr + 8:
            out.append((unit, fn, "0x%08X" % addr, op))
    return out


def check_addic(unit, fn, insns):
    out = []
    for i, (addr, op, args) in enumerate(insns):
        if op not in ("addic.", "subic."):    # hazard 2
            continue
        window = [o for _a, o, _g in insns[i:i + 8]]
        if "mtctr" in window:
            out.append((unit, fn, "0x%08X" % addr,
                        "%s %s%s" % (op, args,
                                     " +beq" if "beq" in window else "")))
    return out


def main():
    mode = (sys.argv[1] if len(sys.argv) > 1 else "branch").lower()
    if mode not in ("branch", "addic"):
        raise SystemExit(__doc__)
    asm, report = paths()
    exact = exact_functions(report)
    hits = walk(asm, check_branch if mode == "branch" else check_addic)

    oracles, other = [], []
    for unit, fn, addr, extra in hits:
        row = (unit, fn, addr, extra)
        (oracles if exact.get((unit, fn)) == 100 else other).append(row)

    print("mode: %s" % mode)
    print("sites total: %d   (report join covers %d functions)"
          % (len(hits), len(exact)))
    print("ORACLE sites (enclosing function byte-exact): %d in %d functions"
          % (len(oracles), len({(u, f) for u, f, _a, _e in oracles})))
    print("non-exact sites: %d" % len(other))
    print()
    seen = set()
    for unit, fn, addr, extra in sorted(oracles):
        if (unit, fn) in seen:
            continue
        seen.add((unit, fn))
        print("ORACLE  %-38s %-28s %s  %s" % (unit, fn, addr, extra))
    if mode == "addic":
        print()
        print("--- non-exact (open cases) ---")
        for unit, fn, addr, extra in sorted(other):
            print("        %-38s %-28s %s  %s" % (unit, fn, addr, extra))


if __name__ == "__main__":
    main()
