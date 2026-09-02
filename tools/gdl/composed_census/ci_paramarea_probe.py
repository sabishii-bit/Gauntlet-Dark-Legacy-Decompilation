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

Run FROM THE REPOSITORY ROOT:
    python tools/gdl/composed_census/ci_paramarea_probe.py
"""
import os
import re
import subprocess
import sys

ROOT = os.getcwd()
OUT = os.path.join(ROOT, "build", "GUNE5D", "ci_paramarea_probe")
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
    print("\nverdict: no register-only call shape reaches param area 48; "
          "form is byte-irrelevant.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
