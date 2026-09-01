"""EH-island frame-layout probe (lane EH, run 31).

Answers, for an MWCC C++ function whose only exception scaffolding is one
inlined `throw()` helper, the question "where does MWCC put the
`addi rN,r31,<K> / bl __unexpected` island argument, and what can move it?"

It compiles a standalone lookalike of game/movie/movieplayer.cpp's
fn_800D9C5C with that TU's real cflags, so the answers are the compiler's,
not a model's -- the baseline reproduces the TU object exactly
(38 insns, frame 72, island r31+20).

MEASURED LAW (run 31, ~1370 compiles):
    island_offset = eh_block_start + 12          (k = 12 is CONSTANT)
    eh_block_start = 8 + below_block
    below_block    = 0                      if no touched-but-dead
                                            addressable local exists
                   = max(4, max alignof(L)) over those locals otherwise
  and an 8-ALIGNED such local costs +16 frame bytes, never +8.
  Therefore "island +8 at constant frame size" is UNOBTAINABLE from source.

Modes:
  lattice   enumerate local-declaration shapes; print the reachable
            (frame, island, insns) lattice with a witness for each cell
  archive   compile the baseline with every compiler in build/compilers/GC
  flags     one-flag deltas against the TU's real cflag set
  actions   EH cleanup-action shapes (destructible temporaries, empty
            destructors, ctor/dtor, base/derived, inner scopes)
  all       all of the above

Usage, FROM THE REPOSITORY ROOT:
    python tools/gdl/composed_census/eh_island_layout_probe.py [mode]
"""
import itertools
import os
import re
import subprocess
import sys

ROOT = os.getcwd()
OUT = os.path.join(ROOT, "build", "eh_island_probe")

CFLAGS = [
    "-nodefaults", "-proc", "gekko", "-align", "powerpc", "-enum", "int",
    "-fp", "hardware", "-Cpp_exceptions", "off", "-O4", "-inline", "auto",
    "-pragma", "cats off", "-pragma", "warn_notinlined off", "-maxerrors", "1",
    "-nosyspath", "-RTTI", "off", "-fp_contract", "on", "-str", "reuse",
    "-multibyte", "-i", "include", "-i", "build/GUNE5D/include",
    "-DBUILD_VERSION=0", "-DVERSION_GUNE5D", "-DNDEBUG=1",
    "-Cpp_exceptions", "on", "-str", "reuse,readonly", "-lang=c++",
]

TEMPLATE = r'''
#include "types.h"

extern "C" {
extern s32 gMovieAllocCount;
extern void ResetAllocTot(void);
extern void* AllocHiMem(u32 size, u32 tag);
}

typedef struct MovieRingBuffer {
    u8* buffer;
    u32 size;
    u32 writePos;
    u32 readPos;
} MovieRingBuffer;

static inline void MovieReleaseAllocEH(void* p) throw() {
    if (p != NULL) {
        gMovieAllocCount--;
        if (gMovieAllocCount == 0) {
            ResetAllocTot();
        }
    }
}

__PRELUDE__

extern "C" void probefn(MovieRingBuffer* p, int n) {
__LOCALS__
    MovieReleaseAllocEH(p->buffer);
    p->buffer = 0;
    p->size = n;
    p->buffer = (u8*)AllocHiMem(p->size, (u32)gMovieAllocCount++);
    p->readPos = 0;
    p->writePos = 0;
}
'''

TARGET_SIG = (38, 72, [28])   # what fn_800D9C5C's retail bytes demand
OURS_SIG = (38, 72, [20])     # what the TU actually builds today


def _sjis():
    return os.path.join(ROOT, "build", "tools", "sjiswrap.exe")


def compile_src(tag, src, mw="1.2.5", cflags=None):
    if not os.path.isdir(OUT):
        os.makedirs(OUT)
    cpp = os.path.join(OUT, "%s.cpp" % tag)
    obj = os.path.join(OUT, "%s.o" % tag)
    with open(cpp, "w", newline="\n") as f:
        f.write(src)
    exe = os.path.join(ROOT, "build", "compilers", "GC", mw, "mwcceppc.exe")
    if not os.path.exists(exe):
        return None, "missing compiler GC/%s" % mw
    cmd = [_sjis(), exe] + (cflags or CFLAGS) + ["-c", cpp, "-o", obj]
    r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        return None, ((r.stdout or "") + (r.stderr or "")).strip()
    return obj, None


def dump(obj):
    exe = os.path.join(ROOT, "build", "binutils", "powerpc-eabi-objdump.exe")
    return subprocess.run([exe, "-d", "-r", obj],
                          capture_output=True, text=True).stdout


def analyse(text):
    frame = saves = None
    islands = []
    seg = text.split("<probefn>:", 1)
    ninsn = 0
    if len(seg) > 1:
        for ln in seg[1].splitlines():
            if re.match(r"^\s*[0-9a-f]+:\s+([0-9a-f]{2} ){4}", ln):
                ninsn += 1
            elif ln.strip().endswith(">:"):
                break
    lines = text.splitlines()
    for i, ln in enumerate(lines):
        m = re.search(r"stwu\s+r1,-(\d+)\(r1\)", ln)
        if m and frame is None:
            frame = int(m.group(1))
        m = re.search(r"stmw\s+r(\d+),(\d+)\(r1\)", ln)
        if m and saves is None:
            saves = (int(m.group(1)), int(m.group(2)))
        m = re.search(r"addi\s+r3,r31,(\d+)", ln)
        if m and "__unexpected" in "\n".join(lines[i:i + 3]):
            islands.append(int(m.group(1)))
    return frame, saves, islands, ninsn


def render(src, prelude="", locals_=""):
    return (TEMPLATE.replace("__PRELUDE__", prelude)
                    .replace("__LOCALS__", locals_)) if src is None else src


def mark(sig):
    if list(sig) == list(TARGET_SIG):
        return "   <<<< TARGET SIGNATURE"
    if sig[2] == [28]:
        return "   <- island 28"
    return ""


# ------------------------------------------------------------------ lattice
ATOMS = [
    ("-", None, None),
    ("u8x1", "u8 %s[1];", "%s[0] = (u8)n;"),
    ("u8x2", "u8 %s[2];", "%s[0] = (u8)n;"),
    ("u8x4", "u8 %s[4];", "%s[0] = (u8)n;"),
    ("u8x6", "u8 %s[6];", "%s[0] = (u8)n;"),
    ("u8x8", "u8 %s[8];", "%s[0] = (u8)n;"),
    ("u8x12", "u8 %s[12];", "%s[0] = (u8)n;"),
    ("s16x2", "s16 %s[2];", "%s[0] = (s16)n;"),
    ("s16x4", "s16 %s[4];", "%s[0] = (s16)n;"),
    ("u32x1", "u32 %s[1];", "%s[0] = (u32)n;"),
    ("u32x2", "u32 %s[2];", "%s[0] = (u32)n;"),
    ("u32x3", "u32 %s[3];", "%s[0] = (u32)n;"),
    ("f32x1", "f32 %s[1];", "%s[0] = 0.0f;"),
    ("f64x1", "f64 %s[1];", "%s[0] = 0.0;"),
    ("s64x1", "s64 %s[1];", "%s[0] = 0;"),
    ("pad4", "u8 %s[4];", None),
    ("pad8", "u8 %s[8];", None),
    ("pad12", "u8 %s[12];", None),
]
PLACEMENTS = ["top", "inner"]


def build_locals(combo):
    out = []
    for i, (atom, place) in enumerate(combo):
        _, decl, touch = atom
        if decl is None:
            continue
        var = "v%d" % i
        d, t = decl % var, (touch % var if touch else None)
        if place == "inner" and t:
            out.append("    { %s %s }" % (d, t))
        else:
            out.append("    %s" % d)
            if t:
                out.append("    %s" % t)
    return "\n".join(out)


def combo_label(combo):
    return "+".join("%s/%s" % (a[0], p[0])
                    for a, p in combo if a[1] is not None) or "(none)"


def mode_lattice():
    combos = [[(a, p)] for a in ATOMS for p in PLACEMENTS]
    for a, b in itertools.product(ATOMS, repeat=2):
        if a[1] is None and b[1] is None:
            continue
        for pa, pb in itertools.product(PLACEMENTS, repeat=2):
            combos.append([(a, pa), (b, pb)])
    bysrc = {}
    for c in combos:
        bysrc.setdefault(build_locals(c), c)
    print("lattice: %d unique local-declaration shapes" % len(bysrc))
    seen, hits = {}, []
    for locals_, c in bysrc.items():
        obj, _ = compile_src("lat", render(None, "", locals_))
        if obj is None:
            continue
        frame, _, islands, ninsn = analyse(dump(obj))
        if not islands:
            continue
        seen.setdefault((frame, tuple(islands), ninsn), combo_label(c))
        if (ninsn, frame, islands) == list(TARGET_SIG) or \
           (ninsn, frame, islands) == TARGET_SIG:
            hits.append(combo_label(c))
    print("\nREACHABLE LATTICE (frame, islands, insns) -> witness")
    for k in sorted(seen):
        flag = "  <<<< TARGET" if k == (72, (28,), 38) else ""
        print("  %-26s %s%s" % (str(k), seen[k], flag))
    print("\nTARGET-SIGNATURE HITS:", hits or "(none)")
    return hits


def mode_archive():
    base = render(None, "", "    u8 unused[8];")
    root = os.path.join(ROOT, "build", "compilers", "GC")
    print("archive sweep (baseline source, pad8):")
    for mw in sorted(os.listdir(root)):
        obj, err = compile_src("arc", base, mw)
        if obj is None:
            print("  %-12s %s" % ("GC/" + mw, err.splitlines()[-1:] or "FAIL"))
            continue
        frame, _, islands, ninsn = analyse(dump(obj))
        print("  %-12s insns=%-4s frame=%-5s islands=%s%s" % (
            "GC/" + mw, ninsn, frame, islands, mark((ninsn, frame, islands))))


def mode_flags():
    base = render(None, "", "    u8 unused[8];")

    def sub(o, nw):
        f = list(CFLAGS)
        for i in range(len(f) - 1):
            if f[i] == o[0] and f[i + 1] == o[1]:
                f[i:i + 2] = list(nw)
                break
        return f

    deltas = [
        ("(tu baseline)", list(CFLAGS)),
        ("-RTTI on", sub(("-RTTI", "off"), ("-RTTI", "on"))),
        ("-align mac68k", sub(("-align", "powerpc"), ("-align", "mac68k"))),
        ("-O4,p", CFLAGS + ["-O4,p"]),
        ("-O3", CFLAGS + ["-O3"]),
        ("-inline all", CFLAGS + ["-inline", "all"]),
        ("-inline auto,level=1", CFLAGS + ["-inline", "auto,level=1"]),
        ("-fp fmadd", sub(("-fp", "hardware"), ("-fp", "fmadd"))),
        ("-enum min", sub(("-enum", "int"), ("-enum", "min"))),
        ("-sym on", CFLAGS + ["-sym", "on"]),
        ("-opt schedule", CFLAGS + ["-opt", "schedule"]),
        ("-proc 750", sub(("-proc", "gekko"), ("-proc", "750"))),
    ]
    print("one-flag deltas (baseline source, pad8):")
    for i, (label, f) in enumerate(deltas):
        obj, err = compile_src("flg%d" % i, base, "1.2.5", f)
        if obj is None:
            print("  %-24s COMPILE FAIL" % label)
            continue
        frame, _, islands, ninsn = analyse(dump(obj))
        print("  %-24s insns=%-4s frame=%-5s islands=%s%s" % (
            label, ninsn, frame, islands, mark((ninsn, frame, islands))))


ACTIONS = {
    "ref_pad8": ("", "    u8 unused[8];"),
    "ref_nopad": ("", ""),
    "emptydtor1": ("struct E1 { ~E1() { } };", "    E1 e;"),
    "emptydtor2": ("struct E2 { ~E2() { } };", "    E2 a; E2 b;"),
    "emptydtor_inner": ("struct E4 { ~E4() { } };", "    { E4 a; }"),
    "dtor_sz8": ("struct D8 { u32 x, y; ~D8() { } };", "    D8 a;"),
    "dtor_base": ("struct B { ~B() { } };\nstruct Dv : B { ~Dv() { } };",
                  "    Dv a;"),
    "ctordtor": ("struct C1 { C1() { } ~C1() { } };", "    C1 a;"),
}


def mode_actions():
    print("EH cleanup-action shapes:")
    for name, (prelude, locals_) in ACTIONS.items():
        obj, err = compile_src("act", render(None, prelude, locals_))
        if obj is None:
            print("  %-20s COMPILE FAIL" % name)
            continue
        frame, _, islands, ninsn = analyse(dump(obj))
        print("  %-20s insns=%-4s frame=%-5s islands=%s%s" % (
            name, ninsn, frame, islands, mark((ninsn, frame, islands))))


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "all"
    if not os.path.exists(_sjis()):
        print("run me from the REPOSITORY ROOT (build/tools/sjiswrap.exe "
              "not found from %s)" % ROOT)
        return 2
    # self-check: the harness must reproduce the TU's real codegen
    obj, err = compile_src("selfcheck", render(None, "", "    u8 unused[8];"))
    if obj is None:
        print("SELF-CHECK COMPILE FAILED:\n", err)
        return 2
    frame, _, islands, ninsn = analyse(dump(obj))
    got = (ninsn, frame, islands)
    print("self-check: baseline builds insns=%s frame=%s islands=%s -- %s" % (
        ninsn, frame, islands,
        "matches the TU object (38/72/[20])" if got == list(OURS_SIG) or
        got == OURS_SIG else "DIVERGED from the TU object, results suspect"))
    print()
    if mode in ("lattice", "all"):
        mode_lattice()
        print()
    if mode in ("archive", "all"):
        mode_archive()
        print()
    if mode in ("flags", "all"):
        mode_flags()
        print()
    if mode in ("actions", "all"):
        mode_actions()
    return 0


if __name__ == "__main__":
    sys.exit(main())
