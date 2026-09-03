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

THE PROFITABILITY COLUMN (run-46 item 6, zero builds). `diff` now ranks its
work queue instead of listing it. Each missing branch pair accounts for
exactly one extra TARGET instruction, so

    profit = (target_insns - our_insns) - (target_sites - our_sites)

is the part of the count residual the pairs do NOT explain. profit == 0
(`WHOLE-RESIDUAL`) means the pairs are the entire COUNT difference.
`partial(+N)` means N more instructions are missing besides the pairs, and
`overshoot(-N)` means our function is LONGER than the pairs can account for,
which is a different residual wearing this one's clothes. The instruction
counts live in `nm_branchpair_<side>_insns.json` next to the text side files;
a census written before run 46 has no counts and is regenerated rather than
printed without its ranking.

WHAT THE COLUMN DOES AND DOES NOT PREDICT, measured at d9d4c9ae7 over the
live 41-row queue with the opcode-MULTISET delta as arbiter (raw `real` rows
are the wrong arbiter here: one missing branch reorders everything after it):

    profit == 0   n=8    multiset delta is BRANCH OPCODES ONLY in 4  (50%)
    partial(+N)   n=9                                            0  ( 0%)
    overshoot(-N) n=22                                           1  ( 5%)

So the column is a real discriminant for "the residual is branch-shaped" --
50% against 3% (1 of 31) across every nonzero row. It is NOT a predictor of
exact closure, and the inherited premise that it is does not survive
measurement: the eight profit-0 rows carry `real` 45, 65, 93, 143, 171, 171,
261 and 627. Rank on it; do not promise on it.

IMPORTABLE CORE: profitability, load, load_counts -- pure, no build.
"""
import argparse
import json
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
    """({function: [(offset_hex, mnemonic), ...]}, {function: insn_count}).

    The instruction count comes free from the same disassembly the site scan
    already reads, and it is what turns this census into a RANKING: see
    `profitability` below. Run-46 item 6.
    """
    try:
        out = subprocess.run([OBJDUMP, "-d", "-j", ".text", path],
                             capture_output=True, text=True,
                             timeout=180).stdout
    except Exception:
        return {}, {}
    hits = {}
    counts = {}
    cur = None
    insns = []

    def flush(fn, ins):
        counts[fn] = len(ins)
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
    return hits, counts


def census(which, out_path, counts_path=None):
    objs = objects_for(which)
    lines = []
    total = 0
    all_counts = {}
    for path in objs:
        rel = os.path.relpath(path, ROOT).replace("\\", "/")
        unit = re.sub(r"^build/GUNE5D/(obj|src)/", "", rel)
        unit = re.sub(r"\.o$", "", unit)
        hits, counts = scan_object(path)
        for fn, n in counts.items():
            all_counts["%s\t%s" % (unit, fn)] = n
        for fn, rows in sorted(hits.items()):
            total += len(rows)
            lines.append("  %-40s %-38s %s"
                         % (unit, fn, ", ".join("%s %s" % r for r in rows)))
    header = ("scanned %d objects (%s)\nbranch-over-one + unconditional-b sites: %d"
              % (len(objs), which, total))
    text = header + "\n" + "\n".join(lines) + "\n"
    with open(out_path, "w", encoding="ascii", errors="replace") as f:
        f.write(text)
    if counts_path:
        # A SEPARATE file, not a new column: the text side files are read by
        # eye and by `load()` below, and widening their format would have
        # silently dropped every row of an existing census.
        with open(counts_path, "w", encoding="ascii") as f:
            json.dump(all_counts, f, indent=0, sort_keys=True)
    return text


def profitability(target_sites, our_sites, insn_delta):
    """(profit, verdict) for one candidate row. Pure; zero builds.

    `insn_delta` is target_insns - our_insns and `target_sites - our_sites`
    is how many branch pairs we are missing. Each missing pair accounts for
    exactly ONE extra target instruction (the conditional branch and its
    unconditional partner replace one of ours), so

        profit = insn_delta - (target_sites - our_sites)

    is the part of the count residual the branch pairs do NOT explain.
    profit == 0 means the pairs ARE the whole count residual, which is the
    shape BF measured as predicting exact closure. A nonzero profit does not
    forbid closure; it says something else is also missing and the pair fix
    alone will not finish the function, so the row ranks below the zeros.
    A NEGATIVE profit means our function is LONGER than the target by more
    than the pairs explain — a different residual wearing this one's clothes.
    """
    missing = target_sites - our_sites
    profit = insn_delta - missing
    if profit == 0:
        verdict = "WHOLE-RESIDUAL"
    elif profit > 0:
        verdict = "partial(+%d)" % profit
    else:
        verdict = "overshoot(%d)" % profit
    return profit, verdict


def load(path):
    d = {}
    with open(path, "r", encoding="ascii", errors="replace") as f:
        for ln in f.read().splitlines():
            m = re.match(r"^\s{2}(\S+)\s+(\S+)\s+(.*)$", ln)
            if m:
                d[(m.group(1), m.group(2))] = len(
                    [x for x in m.group(3).split(",") if x.strip()])
    return d


def load_counts(path):
    """{"unit\\tfunction": insn_count}, or {} when the file predates run 46."""
    try:
        with open(path, "r", encoding="ascii") as f:
            data = json.load(f)
    except (OSError, ValueError):
        return {}
    return data if isinstance(data, dict) else {}


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
    count_paths = {w: os.path.join(args.out, "nm_branchpair_%s_insns.json" % w)
                   for w in ("ours", "target")}

    if args.mode in ("ours", "target"):
        text = census(args.mode, paths[args.mode], count_paths[args.mode])
        print(text.rstrip())
        print("wrote %s and %s"
              % (paths[args.mode], count_paths[args.mode]))
        return 0

    for w in ("target", "ours"):
        # The instruction counts are what the profitability column is made
        # of, so a side file written before run 46 is REGENERATED rather than
        # reported without its ranking.
        if not (os.path.exists(paths[w]) and os.path.exists(count_paths[w])):
            census(w, paths[w], count_paths[w])
    t, o = load(paths["target"]), load(paths["ours"])
    tc, oc = load_counts(count_paths["target"]), load_counts(count_paths["ours"])
    rows = []
    for key, n in sorted(t.items()):
        on = o.get(key, 0)
        if on >= n:
            continue
        flat = "%s\t%s" % key
        ti, oi = tc.get(flat), oc.get(flat)
        if ti is None or oi is None:
            rows.append((key[0], key[1], n, on, None, None, "no-insn-count"))
            continue
        profit, verdict = profitability(n, on, ti - oi)
        rows.append((key[0], key[1], n, on, ti - oi, profit, verdict))
    # WHOLE-RESIDUAL first, then smallest unexplained remainder: the ranking
    # IS the deliverable (run-46 item 6). Unranked rows sink to the bottom.
    rows.sort(key=lambda r: (r[5] is None, abs(r[5]) if r[5] is not None else 0,
                             r[0], r[1]))
    whole = sum(1 for r in rows if r[6] == "WHOLE-RESIDUAL")
    print("functions where TARGET has more branch-over-one+b sites than OURS: %d"
          % len(rows))
    print("  of these, %d have profit 0: the pairs are the WHOLE COUNT"
          " residual, which is a RANKING, not a promise of exact closure"
          " (measured d9d4c9ae7: 50%% of profit-0 rows have a branch-only"
          " opcode-multiset delta vs 3%% of the rest, but their `real` runs"
          " 45..627)" % whole)
    for unit, fn, tn, on, delta, _profit, verdict in rows:
        extra = ("insn_delta=%+d %s" % (delta, verdict)
                 if delta is not None else
                 "insn_delta=?  (regenerate the side files for the ranking)")
        print("  %-34s %-38s target=%d ours=%d %s"
              % (unit, fn, tn, on, extra))
    print("(sides: %s , %s)" % (paths["target"], paths["ours"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
