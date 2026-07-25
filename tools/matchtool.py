#!/usr/bin/env python3
"""Parallel match explorer: compile one or many source candidates under one or
many mwcc flag presets, score every function against the dtk-extracted target
object, and print a ranked table. Replaces the serial edit-compile-fndiff loop
when hunting for the right flag family or source shape.

Modes:
  probe   Compile the unit's checked-in source under every flag preset (or the
          full --matrix) and rank presets per function. Use on a NEW TU to
          identify its flag family in one shot.
  sweep   Compile every candidate .c file in --dir against one preset and rank
          them, optionally for a single --fn. Use when iterating source shapes.

Usage (from repo root):
  python tools/matchtool.py probe game/gcontrolpads
  python tools/matchtool.py probe game/foo --matrix --mw GC/1.2.5n
  python tools/matchtool.py sweep game/foo --dir W:/scratch/variants --preset demo --fn BarFn
  python tools/matchtool.py sweep game/foo --dir variants --preset demo --fn BarFn --show

Scoring: per function, count of differing normalized instruction lines vs the
target ("OK" = byte-shape identical incl. reloc targets; "L+n" = length differs
by n). Reloc *names* are normalized away, same as fndiff.
"""

import argparse
import concurrent.futures as cf
import itertools
import re
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from fndiff import parse as _parse  # noqa: E402  (objdump-based, shared normalizer)

# Compiler-local symbols get arbitrary names (@37, lbl_..., jumptable_...,
# ...rodata.0) that differ between our object and the dtk extraction even when
# the bytes match; normalize them (with addends) to <local> for scoring.
# fndiff remains the precise reloc-level check.
LOCAL_SYM = re.compile(
    r"(@\w+|\blbl(?:_[0-9A-Fa-f]+)?\b|\bjumptable(?:_[0-9A-Fa-f]+)?\b|\.{3}\S+)"
    r"(\+0x[0-9A-Fa-f]+)?"
)


def parse(objfile):
    fns = _parse(objfile)
    # dtk suffixes local FUNCTION names with their address too; strip so the
    # target "long2str_800E74B8" pairs with our static "long2str"
    out = {}
    for name, lines in fns.items():
        name = re.sub(r"_80[0-9A-Fa-f]{6}$", "", name)
        out[name] = [LOCAL_SYM.sub("<local>", ln) if "R_PPC" in ln else ln for ln in lines]
    return out


# Full symbol normalization: blank every reloc target name (keeping the reloc
# type and any addend). Used to separate "wrong code" from "same bytes, only
# the symbol NAMES differ" (dtk generic lbl/jumptable vs our real static
# names) -- those score "OK~" and the link/sha1 is the true arbiter.
FULL_SYM = re.compile(r"^(    R_PPC_\S+\s+)(.+?)(\+0x[0-9A-Fa-f]+)?$")


def full_norm(lines):
    return [FULL_SYM.sub(lambda m: m.group(1) + "<sym>" + (m.group(3) or ""), ln)
            if ln.startswith("    R_PPC") else ln
            for ln in lines]


VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parent.parent
DEFAULT_MW = "GC/1.2.5n"
# Probe compiles under every compiler this game has actually used (zlib lib is
# GC/1.2.5, everything else so far GC/1.2.5n -- see game-code-frontier notes).
# This is a lookup across proven versions, not an archive sweep.
PROBE_MW = ["GC/1.2.5n", "GC/1.2.5"]

# Flags shared by every known GDL TU family.
COMMON = [
    "-nodefaults", "-proc", "gekko", "-align", "powerpc", "-enum", "int",
    "-fp", "hardware", "-maxerrors", "1", "-nosyspath", "-RTTI", "off",
    "-fp_contract", "on", "-multibyte",
    "-pragma", "cats off", "-pragma", "warn_notinlined off",
    "-i", str(REPO / "include"), "-i", str(REPO / "build" / VERSION / "include"),
    "-DBUILD_VERSION=0", f"-DVERSION_{VERSION}", "-DNDEBUG=1",
]

# Named presets = the flag families observed so far in GDL. Keep in sync with
# configure.py (cflags_base / cflags_demo / cflags_runtime / cflags_inline1).
PRESETS = {
    # SDK + most Midway game code (gpads, gutil, registry, texPalette, sndvoice)
    "sdk":      ["-Cpp_exceptions", "off", "-O4,p", "-inline", "auto", "-str", "reuse"],
    # demo-library family (DEMOInit, gcontrolpads): no peephole, exceptions on,
    # readonly strings (.rodata / small ones .sdata2)
    "demo":     ["-Cpp_exceptions", "on", "-O4", "-inline", "auto", "-str", "reuse,readonly"],
    # demo flags but exceptions off (in case a TU has no extab attribution)
    "demo_nox": ["-Cpp_exceptions", "off", "-O4", "-inline", "auto", "-str", "reuse,readonly"],
    # sdk flags + exceptions on
    "sdk_x":    ["-Cpp_exceptions", "on", "-O4,p", "-inline", "auto", "-str", "reuse"],
    # peephole on + readonly strings
    "sdk_ro":   ["-Cpp_exceptions", "off", "-O4,p", "-inline", "auto", "-str", "reuse,readonly"],
    # MSL runtime family
    "runtime":  ["-Cpp_exceptions", "off", "-O4,p", "-inline", "auto",
                 "-use_lmw_stmw", "on", "-str", "reuse,pool,readonly", "-common", "off"],
    # si/exi family (shallow auto-inline)
    "inline1":  ["-Cpp_exceptions", "off", "-O4,p", "-inline", "auto,level=1", "-str", "reuse"],
    # demo family with shallow inline
    "demo_i1":  ["-Cpp_exceptions", "on", "-O4", "-inline", "auto,level=1", "-str", "reuse,readonly"],
}

# --matrix: full cartesian of the dimensions that have actually distinguished
# GDL TU families. 24 combos; all run in parallel.
MATRIX_DIMS = {
    "O":   ["-O4,p", "-O4"],
    "x":   ["on", "off"],
    "str": ["reuse", "reuse,readonly", "reuse,pool,readonly"],
    "lmw": [None, "on"],
}


def matrix_presets():
    out = {}
    for o, x, s, lmw in itertools.product(*MATRIX_DIMS.values()):
        name = f"{'O4p' if ',p' in o else 'O4'}" \
               f"_x{x}_{s.replace('reuse', 'r').replace(',', '')}" + ("_lmw" if lmw else "")
        flags = ["-Cpp_exceptions", x, o, "-inline", "auto", "-str", s]
        if lmw:
            flags += ["-use_lmw_stmw", "on"]
        out[name] = flags
    return out


def compile_one(mwcc: Path, flags, src: Path, out_o: Path):
    # the unit's own directory is always searchable (local headers)
    extra = ["-i", str(src.parent)]
    r = subprocess.run(
        [str(mwcc)] + COMMON + flags + extra + ["-c", str(src), "-o", str(out_o)],
        capture_output=True, text=True, cwd=str(REPO),
    )
    return r.returncode, (r.stderr or r.stdout)


def score(target_fns, base_fns, only_fn=None):
    """{fn: 'OK' | 'OK~' | int diffcount | 'L+n' | 'MISS'} for target functions.

    'OK~' = instructions identical and reloc addends identical; only reloc
    symbol NAMES differ (usually dtk lbl/jumptable vs our real names).
    """
    res = {}
    for name, t in target_fns.items():
        if only_fn and name != only_fn:
            continue
        b = base_fns.get(name)
        if b is None:
            res[name] = "MISS"
        elif t == b:
            res[name] = "OK"
        elif full_norm(t) == full_norm(b):
            res[name] = "OK~"
        elif len(t) != len(b):
            res[name] = f"L{len(b) - len(t):+d}"
        else:
            res[name] = sum(1 for x, y in zip(t, b) if x != y)
    return res


def total_key(row):
    tot = 0
    for v in row.values():
        if v == "OK":
            continue
        elif v == "OK~":
            tot += 1
        elif isinstance(v, int):
            tot += v
        elif isinstance(v, str) and v.startswith("L"):
            tot += 100 + 5 * abs(int(v[1:]))
        else:
            tot += 10000  # MISS dominates
    return tot


def run_jobs(jobs, target_fns, only_fn, workers):
    """jobs: [(label, mwcc, flags, src)] -> {label: scores|str}"""
    results = {}
    with tempfile.TemporaryDirectory(prefix="matchtool_") as td:
        def one(job):
            label, mwcc, flags, src = job
            out_o = Path(td) / (re.sub(r"\W", "_", label) + ".o")
            rc, err = compile_one(mwcc, flags, src, out_o)
            if rc:
                first = next((ln for ln in err.splitlines() if ln.strip()), "?")
                return label, f"COMPILE FAIL: {first[:60]}"
            return label, score(target_fns, parse(out_o), only_fn)

        with cf.ThreadPoolExecutor(max_workers=workers) as ex:
            for label, res in ex.map(one, jobs):
                results[label] = res
    return results


def print_table(results, target_fns, only_fn, brief=False):
    fns = [n for n in target_fns if not only_fn or n == only_fn]
    ok_rows = sorted(
        ((label, r) for label, r in results.items() if isinstance(r, dict)),
        key=lambda kv: total_key(kv[1]),
    )
    if brief and ok_rows:
        # best row only; list just the non-OK cells, plus how many other
        # candidate rows tie it (identical cell dict)
        best_label, best = ok_rows[0]
        ties = sum(1 for _, r in ok_rows[1:] if r == best)
        bad = {n: best.get(n, "-") for n in fns
               if str(best.get(n, "-")) not in ("OK", "OK~")}
        okc = sum(1 for n in fns if str(best.get(n, "-")) == "OK")
        okt = sum(1 for n in fns if str(best.get(n, "-")) == "OK~")
        parts = [f"{n}={v}" for n, v in bad.items()]
        print(f"best {best_label}  OK:{okc} OK~:{okt} bad:{len(bad)}"
              + (f" ties:{ties}" if ties else ""))
        if parts:
            print("  " + "  ".join(parts))
        for label, r in results.items():
            if not isinstance(r, dict):
                print(f"  {label}: {r}")
        return best_label
    short = {n: (n if len(n) <= 18 else n[:17] + "~") for n in fns}
    width = max((len(label) for label in results), default=8) + 2
    print(f"{'':{width}}" + "".join(f"{short[n]:>20}" for n in fns))
    for label, r in ok_rows:
        cells = "".join(f"{str(r.get(n, '-')):>20}" for n in fns)
        print(f"{label:{width}}{cells}")
    for label, r in results.items():
        if not isinstance(r, dict):
            print(f"{label:{width}}{r}")
    if ok_rows:
        print(f"\nbest: {ok_rows[0][0]}")
    return ok_rows[0][0] if ok_rows else None


def show_best_diff(mwcc, flags, src, target_fns, fn):
    import difflib
    with tempfile.TemporaryDirectory(prefix="matchtool_") as td:
        out_o = Path(td) / "best.o"
        compile_one(mwcc, flags, src, out_o)
        base_fns = parse(out_o)
    t, b = target_fns.get(fn), base_fns.get(fn)
    if t is None or b is None:
        print(f"(cannot diff {fn}: missing on one side)")
        return
    for line in difflib.unified_diff(t, b, "target", "base", lineterm="", n=1):
        print(line)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mode", choices=["probe", "sweep"])
    ap.add_argument("unit", help="unit path like game/gcontrolpads (no extension)")
    ap.add_argument("--fn", help="restrict scoring to one function")
    ap.add_argument("--src", help="source file override (default: src/<unit>.c)")
    ap.add_argument("--dir", help="sweep: directory of candidate .c files")
    ap.add_argument("--preset", default="demo", help="sweep: preset name (default demo)")
    ap.add_argument("--matrix", action="store_true", help="probe: full flag cartesian instead of named presets")
    ap.add_argument("--mw", default=None,
                    help=f"compiler version dir (probe default: all of {PROBE_MW}; "
                         f"sweep default: {DEFAULT_MW})")
    ap.add_argument("--show", action="store_true", help="print the best candidate's remaining diff (needs --fn)")
    ap.add_argument("--brief", action="store_true", help="print only the best row's non-OK cells (token-cheap)")
    ap.add_argument("-j", type=int, default=8, help="parallel jobs (default 8)")
    args = ap.parse_args()

    unit = re.sub(r"\.(c|cpp)$", "", args.unit.replace("\\", "/"))
    target_o = REPO / "build" / VERSION / "obj" / f"{unit}.o"
    if not target_o.exists():
        print(f"missing target object: {target_o} (run ninja once so dtk extracts it)")
        return 1
    target_fns = parse(target_o)
    if args.fn and args.fn not in target_fns:
        print(f"function {args.fn} not in target object; has: {', '.join(target_fns)}")
        return 1

    def compiler(mw):
        p = REPO / "build" / "compilers" / mw / "mwcceppc.exe"
        if not p.exists():
            print(f"missing compiler: {p}")
            sys.exit(1)
        return p

    if args.mode == "probe":
        src = Path(args.src) if args.src else REPO / "src" / f"{unit}.c"
        if not src.exists():
            print(f"missing source: {src}")
            return 1
        presets = matrix_presets() if args.matrix else PRESETS
        mws = [args.mw] if args.mw else PROBE_MW
        jobs, job_info = [], {}
        for mw in mws:
            mwcc = compiler(mw)
            tag = mw.split("/")[-1]
            for name, flags in presets.items():
                label = f"{name}@{tag}" if len(mws) > 1 else name
                jobs.append((label, mwcc, flags, src))
                job_info[label] = (mwcc, flags)
        results = run_jobs(jobs, target_fns, args.fn, args.j)
        best = print_table(results, target_fns, args.fn, args.brief)
        if args.show and args.fn and best:
            print("=" * 60)
            show_best_diff(*job_info[best], src, target_fns, args.fn)
    else:  # sweep
        if not args.dir:
            print("sweep needs --dir with candidate .c files")
            return 1
        mwcc = compiler(args.mw or DEFAULT_MW)
        flags = PRESETS[args.preset]
        cands = sorted(Path(args.dir).glob("*.c"))
        if not cands:
            print(f"no .c files in {args.dir}")
            return 1
        jobs = [(c.stem, mwcc, flags, c) for c in cands]
        results = run_jobs(jobs, target_fns, args.fn, args.j)
        best = print_table(results, target_fns, args.fn, args.brief)
        if args.show and args.fn and best:
            print("=" * 60)
            show_best_diff(mwcc, flags, Path(args.dir) / f"{best}.c", target_fns, args.fn)
    return 0


if __name__ == "__main__":
    sys.exit(main())
