"""Decide whether a switch's chain-vs-jumptable residual is SOURCE-REACHABLE.

Run 37 (AT lane) built this to settle init_attract_mode (game/ui/attract.c),
whose target emits `cmplwi/bgt/slwi/lwzx/mtctr/bctr` where we emit a 12-insn
compare chain.  It answers three questions in one run:

  PHASE 1  What decides MWCC's table-vs-chain choice?  Compiles a family of
           switch shapes over one range and reports which emit `bctr`.
  PHASE 2  Can any COMPILE KNOB flip a given shape?  Sweeps -O levels, -opt,
           and the codegen pragmas over the caller-supplied shape.
  PHASE 3  Can any COMPILER in the local archive flip it?  Same shape across
           every mwcceppc.exe under build/compilers.

Run-37 result on init_attract_mode's shape (16 shape measurements, 17 flag
settings, 29 compilers): MWCC emits a jump table IFF the switch has >= 6
DISTINCT BRANCH DESTINATIONS, counting the default when it is reachable from
inside [min,max].  Case COUNT, label count, and range/density are NOT the
knob -- W6 holds 5 destinations over a range of 21 and still emits a chain,
and W2 (five separate empty case groups) is BYTE-IDENTICAL to W1 (one group
of five), proving empty-block merging happens BEFORE the decision.
init_attract_mode has exactly 5 destinations, so no source spelling reaches
the target's table under any compiler in the archive.

Usage, from the repository root:

    python tools/gdl/composed_census/at_switch_emission_threshold.py
    python tools/gdl/composed_census/at_switch_emission_threshold.py \
        --phases 1 --out build/GUNE5D/at_switch/result.json

Scratch objects go under --workdir (default build/GUNE5D/at_switch), which is
gitignored and per-worktree, so two lanes running this cannot collide.
"""
import argparse
import json
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
COMPILERS = os.path.join(ROOT, "build", "compilers")

# game/ui/attract.c's cflags, minus -O (phase 1 and 3 add -O4) and minus the
# include paths, which the self-contained probe sources do not need.
BASE_CFLAGS = [
    "-nodefaults", "-proc", "gekko", "-align", "powerpc", "-enum", "int",
    "-fp", "hardware", "-Cpp_exceptions", "off", "-inline", "auto",
    "-pragma", "cats off", "-pragma", "warn_notinlined off",
    "-maxerrors", "1", "-nosyspath", "-RTTI", "off", "-fp_contract", "on",
    "-str", "reuse", "-multibyte",
    "-DBUILD_VERSION=0", "-DVERSION_GUNE5D", "-DNDEBUG=1",
    "-Cpp_exceptions", "on", "-str", "reuse,readonly", "-lang=c",
]

SHELL = """typedef unsigned int u32;
extern int g_val;
extern int g_a, g_b, g_c, g_d;
extern void sink(int);

int probe(int screen) {
    int skip = 0;
    switch (%s) {
%s
    }
    return skip;
}
"""

# Bodies reused verbatim across shapes so only the case STRUCTURE varies.
B0 = "        if (screen < 0 && (g_a & 0xF) != 15) { skip = 1; }\n        break;"
B9 = "        if (g_a > 0) { skip = 1; }\n        break;"
B12 = ("        if (g_b != 0) { skip = 1; }\n"
       "        if (g_c == 4 && g_d == 0) { skip = 1; }\n        break;")
B4 = ("        sink(0);\n"
      "        if (g_c == 0 && g_d != 0) { skip = 1; }\n        break;")


def group(vals, body):
    return "\n".join("    case %d:" % v for v in vals) + "\n" + body


EMPTY = "        break;"

# init_attract_mode's shape: 4 live groups + a trailing empty group; the
# default is reachable from inside the range, so 5 distinct destinations.
LIVE4 = "\n".join([group([0], B0), group([9], B9), group([1, 2], B12),
                   group([4], B4)])
TARGET_SHAPE = LIVE4 + "\n" + group([3, 5, 6, 7, 8], EMPTY)

# (name, controlling expression, case body, expected distinct destinations)
SHAPES = [
    ("S1_init_attract_mode_shape", "(u32)(g_val - 0x8000)", TARGET_SHAPE, 5),
    ("S2_empty_group_deleted", "(u32)(g_val - 0x8000)", LIVE4, 5),
    ("S3_ten_groups_distinct_bodies", "(u32)(g_val - 0x8000)",
     "\n".join(group([i], "        sink(%d);\n        break;" % i)
               for i in range(10)), 10),
    ("S4_ascending_order_empties_interleaved", "(u32)(g_val - 0x8000)",
     "\n".join([group([0], B0), group([1, 2], B12), group([3], EMPTY),
                group([4], B4), group([5, 6, 7, 8], EMPTY),
                group([9], B9)]), 5),
    ("S5_explicit_default_added", "(u32)(g_val - 0x8000)",
     TARGET_SHAPE + "\n    default:\n" + EMPTY, 5),
    ("S6_empty_group_made_live", "(u32)(g_val - 0x8000)",
     LIVE4 + "\n" + group([3, 5, 6, 7, 8],
                          "        sink(1);\n        break;"), 5),
    ("S7_signed_no_cast", "g_val - 0x8000", TARGET_SHAPE, 5),
    ("S8_five_separate_empty_groups", "(u32)(g_val - 0x8000)",
     LIVE4 + "\n" + "\n".join(group([v], EMPTY) for v in (3, 5, 6, 7, 8)), 5),
    ("S9_five_groups_two_bodies_identical", "(u32)(g_val - 0x8000)",
     LIVE4 + "\n" + group([6], B9) + "\n" + group([3, 5, 7, 8], EMPTY), 6),
    ("S10_five_groups_all_distinct", "(u32)(g_val - 0x8000)",
     LIVE4 + "\n" + group([6], "        sink(6);\n        break;") + "\n"
     + group([3, 5, 7, 8], EMPTY), 6),
    ("S11_six_live_groups", "(u32)(g_val - 0x8000)",
     LIVE4 + "\n" + group([6], "        sink(6);\n        break;") + "\n"
     + group([7], "        sink(7);\n        break;"), 7),
    ("S12_same_destinations_range_21", "(u32)(g_val - 0x8000)",
     "\n".join([group([0], B0), group([20], B9), group([1, 2], B12),
                group([4], B4)]), 5),
]

FLAG_SETS = [
    ("-O4 (attract.c baseline)", ["-O4"], None),
    ("-O4,p", ["-O4,p"], None),
    ("-O4,s", ["-O4,s"], None),
    ("-O3", ["-O3"], None),
    ("-O3,s", ["-O3,s"], None),
    ("-O2", ["-O2"], None),
    ("-O1", ["-O1"], None),
    ("-O0", ["-O0"], None),
    ("-O4 -opt space", ["-O4", "-opt", "space"], None),
    ("-O4 -opt speed", ["-O4", "-opt", "speed"], None),
    ("-O4 -sym on", ["-O4", "-sym", "on"], None),
    ("-O4 -inline off", ["-O4", "-inline", "off"], None),
    ("-O4 -func_align 4", ["-O4", "-func_align", "4"], None),
    ("-O4 +pragma optimize_for_size on", ["-O4"], "optimize_for_size on"),
    ("-O4 +pragma peephole off", ["-O4"], "peephole off"),
    ("-O4 +pragma opt_propagation off", ["-O4"], "opt_propagation off"),
    ("-O4 +pragma opt_common_subs off", ["-O4"], "opt_common_subs off"),
]

BCTR = bytes.fromhex("4E800420")


def find_compilers():
    out = []
    for dirpath, _dirs, files in os.walk(COMPILERS):
        for name in files:
            if name.lower() == "mwcceppc.exe":
                out.append(os.path.join(dirpath, name))
    return sorted(out)


def emits_table(mwcc, flags, pragma, ctrl, body, workdir, tag):
    src = os.path.join(workdir, "%s.c" % tag)
    obj = os.path.join(workdir, "%s.o" % tag)
    text = SHELL % (ctrl, body)
    if pragma:
        text = "#pragma %s\n%s" % (pragma, text)
    with open(src, "w", newline="\n") as fh:
        fh.write(text)
    proc = subprocess.run([mwcc] + BASE_CFLAGS + flags + ["-c", "-o", obj, src],
                          cwd=ROOT, capture_output=True, text=True)
    if proc.returncode != 0 or not os.path.exists(obj):
        return None, (proc.stderr or proc.stdout).strip()[:160], None
    with open(obj, "rb") as fh:
        data = fh.read()
    return data.count(BCTR) > 0, None, len(data)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--workdir", default=os.path.join("build", "GUNE5D",
                                                      "at_switch"))
    ap.add_argument("--out", default=None,
                    help="write the full result table as JSON here")
    ap.add_argument("--phases", default="1,2,3",
                    help="comma-separated subset of 1,2,3 (default all)")
    args = ap.parse_args()

    workdir = args.workdir
    if not os.path.isabs(workdir):
        workdir = os.path.join(ROOT, workdir)
    os.makedirs(workdir, exist_ok=True)

    compilers = find_compilers()
    if not compilers:
        sys.exit("no mwcceppc.exe under %s -- run `ninja` once to download "
                 "the compiler archive" % COMPILERS)
    primary = next((c for c in compilers
                    if os.sep + "1.2.5" + os.sep in c and "1.2.5n" not in c),
                   compilers[0])
    phases = {p.strip() for p in args.phases.split(",")}
    result = {"compiler": os.path.relpath(primary, ROOT), "phases": {}}

    if "1" in phases:
        print("=== PHASE 1: which SHAPE emits a table? (GC/1.2.5, -O4) ===")
        print("%-42s %-6s %-7s %s" % ("SHAPE", "DESTS", "EMITS", "predicted"))
        print("-" * 74)
        rows = []
        for name, ctrl, body, dests in SHAPES:
            tbl, err, size = emits_table(primary, ["-O4"], None, ctrl, body,
                                         workdir, name)
            if tbl is None:
                print("%-42s %-6s ERROR   %s" % (name, dests, err))
                continue
            predicted = dests >= 6
            flag = "ok" if predicted == tbl else "MISFIT"
            print("%-42s %-6d %-7s %s (%s)"
                  % (name, dests, "TABLE" if tbl else "chain",
                     "TABLE" if predicted else "chain", flag))
            rows.append({"shape": name, "destinations": dests,
                         "table": tbl, "rule_predicts_table": predicted,
                         "agrees": predicted == tbl, "obj_bytes": size})
        result["phases"]["1"] = rows
        bad = [r["shape"] for r in rows if not r["agrees"]]
        print("\nRULE: table IFF distinct branch destinations >= 6 "
              "(default counted when reachable in range)")
        print("counterexamples: %s" % (bad if bad else "NONE (%d/%d agree)"
                                       % (len(rows), len(rows))))

    if "2" in phases:
        print("\n=== PHASE 2: can a COMPILE KNOB flip init_attract_mode's "
              "shape? ===")
        print("%-40s %s" % ("FLAGS", "EMITS"))
        print("-" * 60)
        rows = []
        for i, (name, flags, pragma) in enumerate(FLAG_SETS):
            tbl, err, _ = emits_table(primary, flags, pragma,
                                      "(u32)(g_val - 0x8000)", TARGET_SHAPE,
                                      workdir, "flag%d" % i)
            print("%-40s %s" % (name, "ERROR " + err if tbl is None
                                else ("TABLE" if tbl else "chain")))
            rows.append({"flags": name, "table": tbl})
        result["phases"]["2"] = rows
        print("flag settings emitting a TABLE: %d/%d"
              % (sum(1 for r in rows if r["table"]), len(rows)))

    if "3" in phases:
        print("\n=== PHASE 3: can any ARCHIVE COMPILER flip it? (-O4) ===")
        print("%-46s %s" % ("COMPILER", "EMITS"))
        print("-" * 62)
        rows = []
        for i, mwcc in enumerate(compilers):
            rel = os.path.relpath(mwcc, COMPILERS)
            tbl, err, _ = emits_table(mwcc, ["-O4"], None,
                                      "(u32)(g_val - 0x8000)", TARGET_SHAPE,
                                      workdir, "cc%d" % i)
            print("%-46s %s" % (rel, "ERROR " + err if tbl is None
                                else ("TABLE" if tbl else "chain")))
            rows.append({"compiler": rel, "table": tbl})
        result["phases"]["3"] = rows
        print("compilers emitting a TABLE: %d/%d"
              % (sum(1 for r in rows if r["table"]), len(rows)))

    if args.out:
        out = args.out if os.path.isabs(args.out) else os.path.join(ROOT,
                                                                    args.out)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w") as fh:
            json.dump(result, fh, indent=2)
        print("\nwrote %s" % out)


if __name__ == "__main__":
    main()
