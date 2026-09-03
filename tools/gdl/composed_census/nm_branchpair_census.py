#!/usr/bin/env python3
"""NM run-44 census: the 'branch-over-one + unconditional b' residual class.

Finds every site where a CONDITIONAL branch whose destination is the second
instruction after it is immediately followed by an UNCONDITIONAL `b`:

    cmpwi   rX, N
    bge     +8        <- branch over exactly one instruction
    b       <exit>    <- the instruction it branches over
    ...               <- the branch's destination

That shape is what `fndiff --ops` reports as
`target-only: +1 b +1 bge   ours-only: -1 blt` and it is NOT a peephole
artifact: see
claim.law.NM_branch-pair-fusion-is-blocked-by-a-return-not-by-a-goto.20260903.v1
for the discriminator (a `return` then-block keeps the pair, a `goto`
then-block gets folded) and for the cure.

Usage, FROM THE REPOSITORY ROOT:

  python tools/gdl/composed_census/nm_branchpair_census.py ours
  python tools/gdl/composed_census/nm_branchpair_census.py target
  python tools/gdl/composed_census/nm_branchpair_census.py diff

`ours` scans our compiled objects (build/GUNE5D/src), `target` the
dtk-extracted retail objects (build/GUNE5D/obj), and `diff` runs both and
prints only the functions where the TARGET carries more sites than OURS --
that list is the work queue. Results are written under build/GUNE5D/ (which
is gitignored and per-worktree) unless --out says otherwise; `diff` reads the
two side files if they already exist and regenerates them otherwise.

Measured 2026-09-03 at commit ca4074cb1: 503 sites in 257 of our objects,
563 in 326 target objects, and 51 functions where the target has more.
"""
import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    "..", "..", ".."))
OBJDUMP = os.path.join(ROOT, "build", "binutils", "powerpc-eabi-objdump.exe")
if not os.path.exists(OBJDUMP):
    OBJDUMP = os.path.join(ROOT, "build", "binutils", "powerpc-eabi-objdump")
DEFAULT_OUT = os.path.join(ROOT, "build", "GUNE5D")

COND = re.compile(r"^b(?!l?r?$)(eq|ne|lt|le|gt|ge|dnz|so|ns)\b")
LINE = re.compile(r"^\s*([0-9a-f]+):\s+[0-9a-f ]+\t([a-z][a-z0-9._+]*)\s*(.*)$")
SYM = re.compile(r"^[0-9a-f]+ <([^>]+)>:$")


def objects_for(which):
    base = os.path.join(ROOT, "build", "GUNE5D",
                        "src" if which == "ours" else "obj")
    out = []
    for dirpath, _dirnames, filenames in os.walk(base):
        if ".postprocess" in dirpath:
            continue
        for name in sorted(filenames):
            if name.endswith(".o"):
                out.append(os.path.join(dirpath, name))
    return sorted(out)


def scan_object(path):
    """Return {function_name: [(offset_hex, mnemonic), ...]} for one object."""
    try:
        out = subprocess.run([OBJDUMP, "-d", "-j", ".text", path],
                             capture_output=True, text=True,
                             timeout=180).stdout
    except Exception:
        return {}
    hits = {}
    cur = None
    insns = []

    def flush(fn, ins):
        rows = []
        for i in range(len(ins) - 1):
            addr, mnem, ops = ins[i]
            nxt_mnem = ins[i + 1][1]
            if not COND.match(mnem) or nxt_mnem != "b":
                continue
            m = (re.search(r"([0-9a-f]+)\s*<", ops)
                 or re.search(r"0x([0-9a-f]+)", ops))
            if not m:
                continue
            if int(m.group(1), 16) == addr + 8:
                rows.append((hex(addr), mnem))
        if rows:
            hits.setdefault(fn, []).extend(rows)

    for ln in out.splitlines():
        s = SYM.match(ln.strip())
        if s:
            if cur:
                flush(cur, insns)
            cur, insns = s.group(1), []
            continue
        m = LINE.match(ln)
        if m and cur:
            insns.append((int(m.group(1), 16), m.group(2), m.group(3)))
    if cur:
        flush(cur, insns)
    return hits


def census(which, out_path):
    objs = objects_for(which)
    lines = []
    total = 0
    for path in objs:
        rel = os.path.relpath(path, ROOT).replace("\\", "/")
        unit = re.sub(r"^build/GUNE5D/(obj|src)/", "", rel)
        unit = re.sub(r"\.o$", "", unit)
        for fn, rows in sorted(scan_object(path).items()):
            total += len(rows)
            lines.append("  %-40s %-38s %s"
                         % (unit, fn, ", ".join("%s %s" % r for r in rows)))
    header = ("scanned %d objects (%s)\nbranch-over-one + unconditional-b sites: %d"
              % (len(objs), which, total))
    text = header + "\n" + "\n".join(lines) + "\n"
    with open(out_path, "w", encoding="ascii", errors="replace") as f:
        f.write(text)
    return text


def load(path):
    d = {}
    with open(path, "r", encoding="ascii", errors="replace") as f:
        for ln in f.read().splitlines():
            m = re.match(r"^\s{2}(\S+)\s+(\S+)\s+(.*)$", ln)
            if m:
                d[(m.group(1), m.group(2))] = len(
                    [x for x in m.group(3).split(",") if x.strip()])
    return d


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mode", choices=["ours", "target", "diff"])
    ap.add_argument("--out", default=DEFAULT_OUT,
                    help="directory for the census side files "
                         "(default: build/GUNE5D/)")
    args = ap.parse_args(argv)

    if not os.path.exists(OBJDUMP):
        print("objdump not found at %s -- run `ninja -j2` first" % OBJDUMP)
        return 2
    os.makedirs(args.out, exist_ok=True)
    paths = {w: os.path.join(args.out, "nm_branchpair_%s.txt" % w)
             for w in ("ours", "target")}

    if args.mode in ("ours", "target"):
        text = census(args.mode, paths[args.mode])
        print(text.rstrip())
        print("wrote %s" % paths[args.mode])
        return 0

    for w in ("target", "ours"):
        if not os.path.exists(paths[w]):
            census(w, paths[w])
    t, o = load(paths["target"]), load(paths["ours"])
    rows = [(k[0], k[1], n, o.get(k, 0)) for k, n in sorted(t.items())
            if o.get(k, 0) < n]
    print("functions where TARGET has more branch-over-one+b sites than OURS: %d"
          % len(rows))
    for unit, fn, tn, on in rows:
        print("  %-34s %-38s target=%d ours=%d" % (unit, fn, tn, on))
    print("(sides: %s , %s)" % (paths["target"], paths["ours"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
