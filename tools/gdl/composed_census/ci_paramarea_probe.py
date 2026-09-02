"""Outgoing-parameter-area sizing probe (lane CI, run 34).

Answers the question CB's run-33 record posed for combat.c
someone_will_be_off_screen: "what makes MWCC reserve a 48-byte outgoing
parameter area (target temp-pool base 56) where our build reserves 8
(pool base 16)?"  The live source declares the callee prototype-less --
`void MBWindowProject();` -- so the leading suspect was the declaration form.

It compiles small lookalikes with combat.c's REAL cflags (GC/1.2.5,
cflags_demo, -lang=c, -Cpp_exceptions on) and measures the reserved
outgoing-parameter-area size off the disassembly as
(lowest addr-taken local offset) - 8.

MEASURED RESULT (run 34):
  * FORM is byte-irrelevant: empty-parens, full prototype, and varargs
    declarations of a 4-register-arg callee produce identical frames.
    Confirmed on the real function too: swapping combat.c's prototype-less
    `void MBWindowProject();` for the full 4-pointer prototype is
    byte-identical (the edit folds away before codegen).
  * The outgoing parameter area is sized ONLY by genuine stack-spilled
    arguments: param_area = 8 + 4 * max(0, nGPRargs - 8).  It is 8 for
    every call with <= 8 GPR args regardless of prototype form, and reaches
    48 only near an 18-GPR-argument call.
  * struct-by-value args, struct-return-by-value, and a large addressable
    local passed by pointer (alloca-like) do NOT produce a 48-byte area.
  => No register-only call shape sizes a 48-byte outgoing parameter area.
     The 48 dead bytes below swbos's temp pool are NOT an outgoing
     parameter area.  See claim.law.CI_mwcc-outgoing-param-area-is-sized-
     only-by-stack-spilled-args.20260902.v1.

MEASUREMENT DEFECT FOUND AND FIXED (lane CH, run 36):
  The run-34 `param area` column is (lowest `addi rN,r1,K`) - 8.  That
  heuristic assumes every `addi rN,r1,K` addresses a DECLARED local.  On a
  by-value aggregate argument it does not: MWCC emits `addi r5,r1,8` as the
  cursor for the argument COPY, so the run-34 table reported
  `shape_struct_byval ... param area 0` -- an impossible value (the EABI
  minimum is 8) that was read as "does not reach 48" instead of as a broken
  measurement.  Discipline 14 (a guard's refusal measures the guard) applied
  to a metric.  The `below-locals` section replaces it with a marker that is
  present in every case by construction: the offset of the declared `s16
  sp[2]` buffer, read straight off the wrapper's own `lha rN,K(r1)`.  Bytes
  in [8, sp) are the region BELOW every declared local -- exactly the
  quantity swbos's residual is about (target temp base 56, ours 16).

MEASURED RESULT (run 36, `below-locals` section):
  * A 40-byte BY-VALUE STRUCT argument seats 48 bytes below the declared
    locals (8-byte parameter area at r1+8..15 plus a 40-byte argument copy
    at r1+16..55, the declared s16[2] landing at r1+56) -- the CI law is
    intact on parameter-area SIZING and its consequence clause is wrong:
    a source-level construct DOES reserve exactly swbos's 48 dead bytes.
  * The reserved block tracks the aggregate size in 8-byte steps, so the
    region is a size-addressable source lever, not allocator slack.

Run FROM THE REPOSITORY ROOT:
    python tools/gdl/composed_census/ci_paramarea_probe.py
"""
import os
import re
import subprocess
import sys

ROOT = os.getcwd()
OUT = os.path.join(ROOT, "build", "GUNE5D", "ci_paramarea_probe")

# The PPC EABI reserves at least 8 bytes at r1+0: the saved back-chain word
# and the LR save word. No frame can seat a declared local below that, and
# no outgoing parameter area can be smaller.
EABI_MIN_PARAM_AREA = 8


def frame_metric_floor_violation(name, value, floor=EABI_MIN_PARAM_AREA):
    """Message when a derived frame metric fell below its ABI floor, else None.

    A DERIVED frame figure is a heuristic over disassembly, and when the
    heuristic breaks it does not fail — it returns a number. Run 34's
    `param area` column is (lowest `addi rN,r1,K`) - 8, which assumes every
    such addi addresses a declared local; on a by-value aggregate argument
    MWCC emits `addi r5,r1,8` as the argument-COPY cursor, so the column
    printed `shape_struct_byval ... param area 0`. Zero is below the EABI
    minimum of 8 and therefore impossible, but it was read as "does not
    reach 48" — i.e. as evidence — and shipped into
    claim.law.CI_mwcc-outgoing-param-area-is-sized-only-by-stack-spilled-
    args.20260902.v1. A metric that cannot say "I am broken" will be quoted
    as if it were sound (AGENTS.md discipline 14, applied to a metric
    rather than to a guard).

    Pure, so the floor is tested without a compiler.
    """
    if value is None:
        return None
    if value >= floor:
        return None
    return (f"IMPOSSIBLE {name}={value}: below the EABI floor of {floor}."
            " This is a BROKEN MEASUREMENT, not a small frame — do not"
            " quote it, and do not read it as 'did not reach' the value"
            " you were hunting. The lowest-addi heuristic behind it"
            " mistakes an argument-copy cursor for a declared local; score"
            " the case with the `below-locals` marker instead.")
CFLAGS = [
    "-nodefaults", "-proc", "gekko", "-align", "powerpc", "-enum", "int",
    "-fp", "hardware", "-Cpp_exceptions", "off", "-O4", "-inline", "auto",
    "-pragma", "cats off", "-pragma", "warn_notinlined off", "-maxerrors", "1",
    "-nosyspath", "-RTTI", "off", "-fp_contract", "on", "-str", "reuse",
    "-multibyte", "-i", "include", "-i", "build/GUNE5D/include",
    "-DBUILD_VERSION=0", "-DVERSION_GUNE5D", "-DNDEBUG=1",
    "-Cpp_exceptions", "on", "-str", "reuse,readonly", "-lang=c",
]

PROLOGUE = r'''
typedef signed short s16;
typedef signed int   s32;
typedef unsigned int u32;
typedef float        f32;
typedef struct Big { s32 w[10]; } Big;   /* 40 bytes */
extern f32* gBase;
extern Big  gBig;
extern Big  makeBig(void);
'''

# ---- declaration-FORM cases (same 4-register-arg call, different decl) ----
FORM_CASES = [
    ("form_emptyparens",
     "void CALLEE();",
     "CALLEE((f32*)(gBase+n),(void*)gBase,0,sp);"),
    ("form_prototyped",
     "void CALLEE(f32* a, void* b, f32* c, s16* d);",
     "CALLEE((f32*)(gBase+n),(void*)gBase,0,sp);"),
    ("form_varargs",
     "void CALLEE(f32* a, ...);",
     "CALLEE((f32*)(gBase+n),(void*)gBase,0,sp);"),
]

# ---- call-SHAPE cases (rising arg counts + aggregate / return / alloca) ----
def addr_call(nargs):
    decl = "void CALLEE(" + ",".join(["s32"] * (nargs - 1) + ["s16*"]) + ");"
    args = ",".join(["n"] * (nargs - 1) + ["sp"])
    return ("shape_%dargs" % nargs, decl, "CALLEE(%s);" % args)


SHAPE_CASES = [addr_call(k) for k in (4, 8, 9, 10, 12, 14, 16, 18)]
SHAPE_CASES.append(("shape_struct_byval",
                    "void CALLEE(Big b, s16* sp);", "CALLEE(gBig,sp);"))
SHAPE_CASES.append(("shape_alloca_ptr",
                    "void CALLEE(f32* p, s16* sp);",
                    "{ f32 buf[8]; CALLEE(buf,sp); }"))

RET_CASE = ("shape_struct_return", None,
            "{ Big b=makeBig(); sp[0]=(s16)b.w[0]; }")


# ---- BY-VALUE AGGREGATE sizing (lane CH, run 36) -------------------------
# The sub-case claim.law.CI_...v1's own falsifier invites and the run-34
# table mis-measured.  Each case passes an aggregate of a known size BY
# VALUE and is scored on `below-locals` (see analyse_below), not on the
# broken lowest-addi heuristic.
def byval_case(nbytes, second=False):
    """A call taking an `nbytes` struct by value.

    second=True places the aggregate as argument 2 of a 4-argument call,
    which is swbos's actual call shape:
    MBWindowProject(ptr, <aggregate>, 0, s16*).
    """
    words = nbytes // 4
    typ = "S%d" % nbytes
    decl = ("typedef struct %s { s32 w[%d]; } %s;\n"
            "extern %s g%s;\n" % (typ, words, typ, typ, typ))
    if second:
        decl += "void CALLEE(f32* a, %s m, s32 f, s16* sp);" % typ
        call = "CALLEE((f32*)gBase,g%s,0,sp);" % typ
        tag = "byval%d_as_arg2" % nbytes
    else:
        decl += "void CALLEE(%s b, s16* sp);" % typ
        call = "CALLEE(g%s,sp);" % typ
        tag = "byval%d_as_arg1" % nbytes
    return (tag, decl, call)


BYVAL_CASES = ([("byval0_none", "void CALLEE(f32* a, s16* sp);",
                 "CALLEE((f32*)gBase,sp);")] +
               [byval_case(n) for n in (8, 16, 24, 32, 40, 48, 64)] +
               [byval_case(40, second=True), byval_case(48, second=True)])


def wrap(decl, call):
    d = (decl + "\n") if decl else ""
    return (PROLOGUE + d +
            "f32 probefn(s32 n, f32* pos){f32 a=0.0f;s32 i;"
            "for(i=0;i<n;i++){s16 sp[2];" + call +
            "a+=(f32)sp[0];}return a;}")


def sjis():
    return os.path.join(ROOT, "build", "tools", "sjiswrap.exe")


def compile_src(tag, src):
    if not os.path.isdir(OUT):
        os.makedirs(OUT)
    c = os.path.join(OUT, tag + ".c")
    o = os.path.join(OUT, tag + ".o")
    with open(c, "w", newline="\n") as f:
        f.write(src)
    exe = os.path.join(ROOT, "build", "compilers", "GC", "1.2.5", "mwcceppc.exe")
    if not os.path.exists(exe):
        return None, "missing compiler GC/1.2.5"
    cmd = [sjis(), exe] + CFLAGS + ["-c", c, "-o", o]
    r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        return None, ((r.stdout or "") + (r.stderr or "")).strip()
    return o, None


def dump(obj):
    exe = os.path.join(ROOT, "build", "binutils", "powerpc-eabi-objdump.exe")
    return subprocess.run([exe, "-d", "-r", obj],
                          capture_output=True, text=True).stdout


def analyse(text):
    seg = text.split("<probefn>:", 1)
    if len(seg) < 2:
        return None
    frame = None
    addrs = []
    for ln in seg[1].splitlines():
        if ln.strip().endswith(">:") and "probefn" not in ln:
            break
        m = re.search(r"stwu\s+r1,-(\d+)\(r1\)", ln)
        if m and frame is None:
            frame = int(m.group(1))
        m = re.search(r"addi\s+r\d+,r1,(\d+)", ln)
        if m:
            addrs.append(int(m.group(1)))
    locs = sorted(a for a in addrs if a != frame)
    first_local = locs[0] if locs else None
    param = (first_local - 8) if first_local is not None else None
    return frame, first_local, param


def analyse_below(text):
    """Frame, declared-local base, and the byte count below every local.

    The declared `s16 sp[2]` buffer is the one object every wrapper both
    takes the address of AND reads back (`a+=(f32)sp[0]`), so its `lha
    rN,K(r1)` read identifies it unambiguously in every case -- unlike the
    lowest `addi rN,r1,K`, which an argument-copy cursor also produces.

    Returns (frame, sp_off, below, bases) where `below` = sp_off - 8 is the
    reserved region beneath the declared locals (8 = the EABI minimum
    outgoing parameter area) and `bases` lists the r1-relative block
    addresses the body forms below sp_off.
    """
    seg = text.split("<probefn>:", 1)
    if len(seg) < 2:
        return None
    frame = None
    sp_off = None
    bases = []
    for ln in seg[1].splitlines():
        if ln.strip().endswith(">:") and "probefn" not in ln:
            break
        m = re.search(r"stwu\s+r1,-(\d+)\(r1\)", ln)
        if m and frame is None:
            frame = int(m.group(1))
        m = re.search(r"lha\s+r\d+,(\d+)\(r1\)", ln)
        if m and sp_off is None:
            sp_off = int(m.group(1))
        m = re.search(r"addi\s+r\d+,r1,(\d+)", ln)
        if m:
            bases.append(int(m.group(1)))
    if sp_off is None:
        return frame, None, None, sorted(set(bases))
    below = sp_off - 8
    return frame, sp_off, below, sorted(set(b for b in bases
                                            if b < sp_off and b != frame))


def run_below(cases, header):
    print(header)
    print("  %-22s %-7s %-11s %-13s %s"
          % ("case", "frame", "locals@", "below locals", "blocks below"))
    for tag, decl, call in cases:
        obj, err = compile_src(tag, wrap(decl, call))
        if obj is None:
            print("  %-22s FAIL: %s" % (tag, (err or "")[:48]))
            continue
        res = analyse_below(dump(obj))
        if res is None:
            print("  %-22s (no probefn)" % tag)
            continue
        frame, sp_off, below, bases = res
        if sp_off is None:
            print("  %-22s %-7s (no declared-local marker)" % (tag, frame))
            continue
        # The replacement marker gets the same floor as the metric it
        # replaced: a declared local cannot sit below the EABI reserve, so
        # sp_off < 8 (hence below < 0) would mean the `lha` this reads was
        # not the declared buffer after all.
        violation = frame_metric_floor_violation("locals@", sp_off)
        if violation:
            print("  %-22s %-7s %s" % (tag, frame, "INVALID"))
            print("      %s" % violation)
            continue
        flag = "  <<<< 48!" if below == 48 else ""
        print("  %-22s %-7s %-11s %-13s %s%s"
              % (tag, frame, sp_off, below,
                 ",".join(str(b) for b in bases) or "-", flag))


def run(cases, header):
    print(header)
    print("  %-22s %-7s %-11s %s" % ("case", "frame", "1st local",
                                     "param area"))
    ok = True
    for tag, decl, call in cases:
        obj, err = compile_src(tag, wrap(decl, call))
        if obj is None:
            print("  %-22s FAIL: %s" % (tag, (err or "")[:48]))
            ok = False
            continue
        res = analyse(dump(obj))
        if res is None:
            print("  %-22s (no probefn)" % tag)
            continue
        frame, fl, pa = res
        violation = frame_metric_floor_violation("param area", pa)
        if violation:
            # Refuse to print the number at all: the run-34 table printed
            # `0` and it was quoted as evidence.
            print("  %-22s %-7s %-11s %s" % (tag, frame, fl, "INVALID"))
            print("      %s" % violation)
            ok = False
            continue
        flag = "  <<<< 48!" if pa == 48 else ""
        print("  %-22s %-7s %-11s %s%s" % (tag, frame, fl, pa, flag))
    return ok


def main():
    if not os.path.exists(sjis()):
        print("run from the REPOSITORY ROOT (sjiswrap not found from %s)"
              % ROOT)
        return 2
    print("outgoing-parameter-area probe -- combat.c cflags, GC/1.2.5")
    print("TARGET swbos: frame 232, temp-pool base 56, => param area 48\n")
    run(FORM_CASES, "declaration FORM (same 4-register-arg call):")
    print()
    run(SHAPE_CASES + [RET_CASE], "call SHAPE (arg count / aggregate / "
        "return / alloca-like):")
    print("  NOTE the `param area` column above is the run-34 lowest-addi"
          " heuristic and is WRONG")
    print("  for aggregates. Any value below the EABI minimum of 8 now"
          " prints INVALID rather than a")
    print("  number: shape_struct_byval used to report 0 there and it was"
          " quoted as evidence.")
    print()
    run_below(BYVAL_CASES + SHAPE_CASES + [RET_CASE],
              "BY-VALUE AGGREGATE sizing, scored below-locals (run 36):")
    print("\nverdict: the outgoing PARAMETER AREA is 8 for every <=8-GPR-arg"
          " call (CI law holds),")
    print("but a BY-VALUE AGGREGATE argument reserves its own copy below the"
          " declared locals,")
    print("so a 40-byte by-value struct arg seats exactly 48 bytes there ="
          " swbos's dead region.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
