#!/usr/bin/env python3
"""Who owns the canonical zero? Answered: the FUNCTION BOUNDARY decides.

A long-standing residual class in this project: a function stores a zero to a
global, makes calls, then runs a loop whose counter starts at zero. MWCC
materializes one zero before the calls, keeps it in a callee-saved register
across them, and starts several induction variables at zero in the loop
preheader -- and the two streams disagree about WHICH induction variable
inherits that register. Whichever one does is callee-saved BY CONSTRUCTION,
since its live range crosses the calls, and the other is born in the preheader
in a volatile. So one choice sets the class of both and the entire residual is
its shadow.

game/world/world::WorldSaveInitState was the worked example. With the loop
INLINE in the storing function, volregs read 111 target instructions against
111 of ours, 80 identical rows, 17 COLOUR, 12 SAVED-COLOUR, and exactly TWO
structural rows:

    @0x64   T: addi r5,r29,0        O: addi r12,r31,232
    @0x68   T: addi r12,r31,232     O: li r11,0

    target  r29 = i (stride 1) owns the zero; r5 = i*60 is a volatile COPY
    inlined r30 = i*60 owns the zero;         r11 = i is a fresh `li`

THE ANSWER, and it is not a spelling. Put the loop in its OWN function -- even
`static inline`, so it is inlined straight back and the instruction count does
not change -- with its own locals, including its own re-read of the base
pointer. Then the zero-store lives in the CALLER, the loop's preheader is no
longer downstream of a pre-call zero in the same function, and the merge falls
to the loop's own counter. `--controls` demonstrates this: the helper form
reproduces the target's assignment and preheader ORDER exactly, 73 synthetic
instructions, frame 24, save set r29/r30/r31, with `addi r5,r29,0` copying the
counter's zero into a volatile stride-60 IV. Both structural rows disappear.

Upstream reached the same place by extracting `sSaveWorldInitState` and typing
the loop body; world.c is Matching. This tool's value now is the GENERALIZATION,
because the residual class is not unique to that function:

    A residual that survives every spelling of a loop, in a function that also
    stores a constant before a call, may be a FUNCTION-BOUNDARY problem. The
    reconstruction inlined by hand a helper the original had as its own
    function. Try moving the loop out before trying another spelling.

WHAT WAS REFUTED GETTING HERE -- recorded so nobody repeats it. Roughly 60
source shapes on the synthetic and ~24 on the real functions, all at GC/1.2.5
with world.c's real flags, none of which moved the assignment:

  Statement order   INERT. The i*60 use moved last, all three body rotations,
                    and the loop-invariant `wobjsp` moved into the for-init,
                    after the counter's init, or inside the body.
  Declaration order INERT. All 24 permutations of the locals.
  Compiler version  INERT. 1.1p1, 1.2.5, 1.2.5n all agree; 1.3.2 does not
                    merge at all.
  17 flag axes      INERT: -opt nostrength / noprop / nolifetimes / nocse /
                    space / speed / level=2 / level=3 / noschedule /
                    nopeephole / nodeadcode, -inline off / all, -schedule off,
                    -proc 750 / 603e, -sym on.
  Loop form         INERT: for / while / do-while, i++ / ++i / i += 1,
                    i < n / i != n / n > i, s32 / u32 / long i, `register` i,
                    a cached bound, a use of i after the loop.
  IV count          INERT. Three generated IVs, two, or one: the largest-stride
                    generated IV wins every time; the counter never does.

  And the flips that DO happen from inside one function all cost more than they
  buy: forcing the counter's zero live before the calls (`g = i = 0;`) moves
  ownership to the counter and hoists EVERY zero-initialized IV with it -- four
  `li rN,0` in the prologue, six callee-saved registers, frame 32 instead of
  24. The hoist is all-or-nothing at that boundary. Making every derived index
  a source variable in byte units stops the merge entirely: no IV takes the
  callee-saved zero. Applied to the real function, that combination produced
  the target's ROLES and still measured worse (111 -> 112 instructions,
  2 structural rows -> 12, project fuzzy -0.00058) and was reverted.

Usage (from the repository root):
  python tools/gdl/composed_census/zero_ownership_probe.py --controls
  python tools/gdl/composed_census/zero_ownership_probe.py --fidelity
  python tools/gdl/composed_census/zero_ownership_probe.py --variants
  python tools/gdl/composed_census/zero_ownership_probe.py --all
"""
from __future__ import annotations

import argparse
import itertools
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent.parent
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import synthprobe as sp              # noqa: E402

TU = "game/world/world"
FUNCTION = "WorldSaveInitState"

DECLARATIONS = r'''
#include "types.h"

extern s32 gDisplay;
extern s32 gMemUsed;
extern s32* gCountP;
extern s32* gParents;
extern f32* gPos;
extern char gName[];
extern char gFmt[];
extern void DoThing(void);
extern void Report(char* fmt, s32 kb);
extern void* MakeThing(s32 n);
'''

# ---- the INLINE form: the loop sits in the function that stores the zero.
# This is what the reconstruction had, and it reproduces that residual.
INLINE = DECLARATIONS + r'''
__PRELUDE__

void probefn(void) {
__DECLS__
__PRE__
    base = gName;
    if (gCountP != 0) {
        u8** wobjsp;
        memBase = gMemUsed;
        gParents = (s32*)MakeThing(*gCountP * 4);
        gPos = (f32*)MakeThing(*gCountP * 12);
__INVARIANT__
__LOOP__
        Report(gFmt, (gMemUsed - memBase) >> 10);
        DoThing();
        gDisplay = 1;
    }
    gDisplay = gDisplay - 1;
    DoThing();
}
'''

# ---- the HELPER form: the loop is its own static inline function with its own
# locals and its own re-read of the base. This is what the target compiles to.
HELPER = DECLARATIONS + r'''
static inline void sHelper(void) {
    s32 i;
    s32 memBase;
    char* base = gName;
    u8** wobjsp;

    memBase = gMemUsed;
    gParents = (s32*)MakeThing(*gCountP * 4);
    gPos = (f32*)MakeThing(*gCountP * 12);
    wobjsp = (u8**)(base + 232);
    for (i = 0; i < *(s32*)(base + 324); i++) {
        gParents[i] = *(s32*)(*wobjsp + i * 60 + 0x18);
        gPos[i * 3] = *(f32*)(*wobjsp + i * 60 + 0x1c);
        gPos[i * 3 + 1] = *(f32*)(*wobjsp + i * 60 + 0x20);
        gPos[i * 3 + 2] = *(f32*)(*wobjsp + i * 60 + 0x24);
    }
    Report(gFmt, (gMemUsed - memBase) >> 10);
}

void probefn(void) {
    char* base;

    gDisplay = 0;
    base = gName;
    if (gCountP != 0) {
        sHelper();
        DoThing();
        gDisplay = 1;
    }
    gDisplay = gDisplay - 1;
    (void)base;
    DoThing();
}
'''

DECLS = "    s32 i;\n    s32 memBase;\n    char* base;"
PRE = "    gDisplay = 0;"
INVARIANT = "        wobjsp = (u8**)(base + 232);"
BOUND = "*(s32*)(base + 324)"
STMTS = [
    "            gParents[i] = *(s32*)(*wobjsp + i * 60 + 0x18);",
    "            gPos[i * 3] = *(f32*)(*wobjsp + i * 60 + 0x1c);",
    "            gPos[i * 3 + 1] = *(f32*)(*wobjsp + i * 60 + 0x20);",
    "            gPos[i * 3 + 2] = *(f32*)(*wobjsp + i * 60 + 0x24);",
]


def loop(stmts=None, head=None):
    body = "\n".join(STMTS if stmts is None else stmts)
    h = head or f"for (i = 0; i < {BOUND}; i++)"
    return f"        {h} {{\n{body}\n        }}"


def source(decls=DECLS, pre=PRE, invariant=INVARIANT, body=None, prelude=""):
    return (INLINE.replace("__DECLS__", decls).replace("__PRE__", pre)
            .replace("__INVARIANT__", invariant).replace("__PRELUDE__", prelude)
            .replace("__LOOP__", body if body is not None else loop()))


def assignment(rows):
    """{callee-saved register: the role it plays}, plus the zero's owner.

    Roles are read off USE, never guessed from position: `base` is whatever
    register forms the +232 displacement, `memBase` is whatever the `subf`
    subtracts, and an induction variable is named by its stride. Reading them
    by register NUMBER would be circular -- the numbering is exactly what
    differs between the streams, and the point is to report an assignment that
    can be compared against the target's.
    """
    roles = {}
    zeros = sp.zero_literals(rows)
    zero = zeros[0] if zeros else None
    for op, o in rows:
        p = sp.ops(o)
        if op == "addi" and len(p) == 3 and p[2] in ("232", "0xe8") \
                and p[1] in sp.SAVED:
            roles[p[1]] = "base"
        if op == "subf" and len(p) == 3 and p[1] in sp.SAVED:
            roles.setdefault(p[1], "memBase")
    for reg, stride in sp.induction_variables(rows).items():
        if reg in sp.SAVED:
            roles[reg] = "counter" if stride == 1 else f"IV{stride}"
    if zero and zero not in roles:
        roles[zero] = "zero"
    if zero in roles:
        roles[zero] += "(zero)"
    return roles, zero


def sig(rows):
    roles, _ = assignment(rows)
    return tuple(roles.get(f"r{n}") for n in (29, 30, 31))


INLINED = ("memBase", "IV60(zero)", "base")
TARGET = ("counter(zero)", "memBase", "base")


def describe(name, rows, width=38):
    s = sig(rows)
    mark = ("  <<< TARGET" if s == TARGET
            else "" if s == INLINED else "  (neither)")
    return (f"{name:{width}} {str(s[0]):15} {str(s[1]):15} {str(s[2]):9} "
            f"{len(rows):4} {str(sp.frame(rows)):4} "
            f"{len(sp.saved(rows))}{mark}")


def header(width=38):
    return (f"{'case':{width}} {'r29':15} {'r30':15} {'r31':9} insn frm #sv\n"
            + "-" * (width + 58))


# ---------------------------------------------------------------- modes

def run_controls(probe):
    """The A/B that answers the question: one function, or two?"""
    print(header(38))
    out = 0
    for name, text in (("inline (the old reconstruction)", source()),
                       ("static inline helper (the target)", HELPER)):
        rows, err = probe.compile("ctl" + name[:6], text)
        if err:
            print(f"{name:38} COMPILE FAILED: {err[:40]}")
            out = 1
            continue
        print(describe(name, rows))
        if "helper" in name:
            copy = sp.copies_of(rows, sp.zero_literals(rows)[0]) \
                if sp.zero_literals(rows) else []
            print(f"\n  the helper form copies the counter's zero into "
                  f"{','.join(copy) or 'nothing'} -- the target's "
                  f"`addi r5,r29,0`")
            if sig(rows) != TARGET:
                print("  UNEXPECTED: the helper form no longer reproduces the "
                      "target; re-derive before trusting the docstring")
                out = 1
    print("\nSame instruction count, same frame, same save set. The only "
          "difference is\nwhich function the loop lives in -- and that decides "
          "the whole residual.")
    return out


def run_fidelity(probe):
    """Assert the HELPER form reproduces the real object, which now matches."""
    rows, err = probe.compile("fidelity", HELPER)
    if err:
        print(f"COMPILE FAILED: {err}", file=sys.stderr)
        return 1
    real = probe.real_rows(FUNCTION)
    if not real:
        print(f"no real rows for {FUNCTION}; build {TU}.o first",
              file=sys.stderr)
        return 1
    print(f"synthetic (helper form) : {len(rows):3} insns, "
          f"frame {sp.frame(rows)}, saved {','.join(sp.saved(rows))}")
    print(f"real {FUNCTION:22}: {len(real):3} insns, "
          f"frame {sp.frame(real)}, saved {','.join(sp.saved(real))}")
    print(f"synthetic roles : {assignment(rows)[0]}")
    print(f"real roles      : {assignment(real)[0]}")
    ok = (sp.frame(rows) == sp.frame(real)
          and sp.saved(rows) == sp.saved(real)
          and sig(rows) == sig(real) == TARGET)
    print("\nFIDELITY " + ("PASS -- frame, save set and callee-saved role map "
                           "all agree, and both are the TARGET assignment"
                           if ok else
                           "FAIL -- the synthetic models a DIFFERENT function; "
                           "no variant result is evidence about the real one"))
    return 0 if ok else 1


def variant_table():
    """(name, kwargs for source()) -- the axes that turned out inert."""
    alt = f"for (; i < {BOUND}; i++)"
    v = [("00 inline baseline", {})]
    v += [
        ("01 gDisplay = i = 0",
         dict(pre="    gDisplay = i = 0;", body=loop(head=alt))),
        ("02 i = 0 then the store",
         dict(pre="    i = 0;\n    gDisplay = 0;", body=loop(head=alt))),
        ("03 the store then i = 0",
         dict(pre="    gDisplay = 0;\n    i = 0;", body=loop(head=alt))),
        ("04 no early store at all", dict(pre="")),
        ("05 early store of 1, not 0", dict(pre="    gDisplay = 1;")),
        ("06 i as u32",
         dict(decls="    u32 i;\n    s32 memBase;\n    char* base;")),
        ("07 register i",
         dict(decls="    register s32 i;\n    s32 memBase;\n    char* base;")),
        ("08 i != n", dict(body=loop(head=f"for (i = 0; i != {BOUND}; i++)"))),
        ("09 ++i", dict(body=loop(head=f"for (i = 0; i < {BOUND}; ++i)"))),
        ("10 while",
         dict(body=f"        i = 0;\n        while (i < {BOUND}) {{\n"
                   + "\n".join(STMTS) + "\n            i++;\n        }")),
        ("11 do-while",
         dict(body="        i = 0;\n        do {\n" + "\n".join(STMTS)
                   + f"\n            i++;\n        }} while (i < {BOUND});")),
        ("12 bound cached in a local",
         dict(decls=DECLS + "\n    s32 n;",
              invariant=INVARIANT + f"\n        n = {BOUND};",
              body=loop(head="for (i = 0; i < n; i++)"))),
        ("13 i used after the loop",
         dict(body=loop() + "\n        gParents[0] = i;")),
        ("14 invariant in the for-init",
         dict(invariant="",
              body=loop(head=f"for (wobjsp = (u8**)(base + 232), i = 0;"
                             f" i < {BOUND}; i++)"))),
        ("15 explicit stride-60 IV", dict(
            decls=DECLS + "\n    s32 off;",
            body=loop([s.replace("i * 60", "off") for s in STMTS],
                      head=f"for (i = 0, off = 0; i < {BOUND};"
                           f" i++, off += 60)"))),
    ]
    for rot in range(1, 4):
        v.append((f"1{5 + rot} body rotation {rot}",
                  dict(body=loop(STMTS[rot:] + STMTS[:rot]))))
    return v


def run_variants(probe):
    print(header())
    hits = []
    for name, kw in variant_table():
        rows, err = probe.compile("v" + name.split()[0], source(**kw))
        if err:
            print(f"{name:38} COMPILE FAILED: {err[:40]}")
            continue
        print(describe(name, rows))
        if sig(rows) == TARGET:
            hits.append(name)
    print("\nMATCHED THE TARGET FROM INSIDE ONE FUNCTION:",
          ", ".join(hits) if hits else "none -- see --controls")
    return 0


def run_declaration_orders(probe):
    decl = {"i": "    s32 i;", "memBase": "    s32 memBase;",
            "base": "    char* base;"}
    print(header(26))
    seen = {}
    for perm in itertools.permutations(sorted(decl)):
        name = ",".join(perm)
        rows, err = probe.compile("d" + "".join(k[0] for k in perm),
                                  source(decls="\n".join(decl[k]
                                                         for k in perm)))
        if err:
            print(f"{name:26} COMPILE FAILED: {err[:40]}")
            continue
        print(describe(name, rows, 26))
        seen.setdefault(sig(rows), []).append(name)
    print(f"\n{len(seen)} distinct assignment(s) across "
          f"{sum(len(v) for v in seen.values())} declaration orders")
    return 0


def run_compilers(probe):
    root = REPO / "build" / "compilers" / "GC"
    print(header(12))
    for cc in sorted(p.name for p in root.iterdir() if p.is_dir()):
        rows, err = probe.compile(f"cc{cc}", source(), cc=cc)
        if err:
            print(f"{cc:12} COMPILE FAILED: {err[:40]}")
            continue
        print(describe(cc, rows, 12))
    return 0


FLAG_AXES = [
    ["-opt", "nostrength"], ["-opt", "noprop"], ["-opt", "nolifetimes"],
    ["-opt", "nocse"], ["-opt", "space"], ["-opt", "speed"],
    ["-opt", "noschedule"], ["-opt", "nopeephole"], ["-opt", "nodeadcode"],
    ["-opt", "level=2"], ["-opt", "level=3"], ["-inline", "off"],
    ["-inline", "all"], ["-schedule", "off"], ["-proc", "750"],
    ["-proc", "603e"], ["-sym", "on"],
]


def run_flags(probe):
    print(header(22))
    for axis in FLAG_AXES:
        rows, err = probe.compile("f" + "".join(axis).replace("=", ""),
                                  source(), extra=axis)
        label = " ".join(axis)
        if err:
            print(f"{label:22} COMPILE FAILED: {err[:40]}")
            continue
        print(describe(label, rows, 22))
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--controls", action="store_true",
                    help="the inline-vs-helper A/B that answers the question")
    ap.add_argument("--fidelity", action="store_true",
                    help="assert the helper form reproduces the real object")
    ap.add_argument("--variants", action="store_true")
    ap.add_argument("--declarations", action="store_true")
    ap.add_argument("--compilers", action="store_true")
    ap.add_argument("--flags", action="store_true")
    ap.add_argument("--all", action="store_true")
    args = ap.parse_args(argv)
    modes = [("controls", run_controls), ("fidelity", run_fidelity),
             ("variants", run_variants),
             ("declarations", run_declaration_orders),
             ("compilers", run_compilers), ("flags", run_flags)]
    chosen = [(n, f) for n, f in modes if args.all or getattr(args, n)]
    if not chosen:
        chosen = [("controls", run_controls)]
    probe = sp.Probe(TU)
    print(f"# {TU} flags, GC/{probe.cc}, wrapper "
          f"{' '.join(probe.wrapper) or '(none)'}")
    print(f"# INLINED r29/r30/r31 = {INLINED}")
    print(f"# TARGET  r29/r30/r31 = {TARGET}\n")
    rc = 0
    for name, fn in chosen:
        print(f"===== {name}")
        rc |= fn(probe)
        print()
    return rc


if __name__ == "__main__":
    sys.exit(main())
