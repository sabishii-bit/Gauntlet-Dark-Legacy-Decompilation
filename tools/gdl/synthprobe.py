#!/usr/bin/env python3
"""Compile a SYNTHETIC function with a real TU's flags and read its codegen.

Every source-shape question in this project has been answered by editing a real
TU, rebuilding it through ninja, and reading `real` or fuzzy. That loop is
correct but slow, it perturbs a file other work depends on, and it answers
"did the score move?" when the question is usually "what does MWCC DO with this
shape?".

This answers the second question directly. It writes a standalone C file under
build/, compiles it with the EXACT argv ninja uses for a chosen TU (flags,
compiler version and the wrapper prefix all taken from `ninja -t commands`, so
it works on a host where ninja runs the PE under wibo), and hands back the
disassembled instruction rows of one function. One compile is ~0.15s, nothing
under src/ is touched, and no ninja edge is invalidated.

IT IS ONLY USEFUL IF THE SYNTHETIC IS FAITHFUL, and that is a claim to be
checked, not assumed. The intended discipline: build the synthetic, then
compare its rows against the real object's rows for the function being
modelled, and do not believe a variant result until the baseline reproduces the
real codegen. `zero_ownership_probe` does this -- its 40-line synthetic
reproduces game/world/world::WorldSaveInitState's prologue, preheader and loop
body instruction-for-instruction and register-for-register, which is what makes
its negative results mean anything. A synthetic that does NOT reproduce the
baseline measures a different function than the one you care about.

Usage as a library (from the repository root):

    from synthprobe import Probe
    p = Probe("game/world/world")          # whose flags to borrow
    rows, err = p.compile("probe1", SOURCE_TEXT)
    print(p.frame(rows), p.saved(rows))

Usage as a command, to eyeball one source:

    python tools/gdl/synthprobe.py game/world/world path/to/synth.c [--fn name]
"""
from __future__ import annotations

import argparse
import importlib.util
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
OUT = REPO / "build" / "synthprobe"
INSN = re.compile(r"^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2} ){4}\s*(\S+)\s*(.*)$")
SAVED = frozenset(f"r{n}" for n in range(14, 32))
VOLATILE = frozenset(["r0"] + [f"r{n}" for n in range(3, 13)])


def _flagsweep():
    """flagsweep owns the ninja argv extraction; do not reimplement it.

    Its `ninja_base_cmd` already returns the wrapper prefix separately, which
    is the whole reason this works off Windows -- see commit 092332ac, where
    dropping that prefix made every synthetic compile die with PermissionError
    trying to exec a PE directly.
    """
    spec = importlib.util.spec_from_file_location(
        "flagsweep", Path(__file__).resolve().parent / "flagsweep.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def source_of(tu):
    for ext in (".c", ".cpp"):
        p = REPO / "src" / (tu + ext)
        if p.exists():
            return p
    return None


def ops(operands):
    """Operand string -> list of trimmed fields."""
    return [x.strip() for x in operands.split(",")]


def parse_rows(text, fn):
    """objdump output -> [(mnemonic, operands)] for one function."""
    seg = text.split(f"<{fn}>:", 1)
    if len(seg) < 2:
        return []
    rows = []
    for line in seg[1].splitlines():
        m = INSN.match(line)
        if m:
            rows.append((m.group(1), m.group(2).strip()))
        elif rows and re.match(r"^[0-9a-f]{8} <", line):
            break          # the next symbol's disassembly starts here
    return rows


def frame(rows):
    """Stack frame bytes from the prologue `stwu r1,-N(r1)`, or None."""
    for op, o in rows:
        if op == "stwu":
            m = re.search(r"-(\d+)\(r1\)", o)
            return int(m.group(1)) if m else None
    return None


def saved(rows):
    """Callee-saved registers the prologue stores, lowest first.

    `stmw rN,off(r1)` saves rN..r31 -- expanding that is the point, since the
    COUNT of saved registers is what a frame-size question turns on and a
    single stmw hides it.
    """
    out = []
    for op, o in rows:
        p = ops(o)
        if not re.search(r"\(r1\)$", p[-1] if p else ""):
            continue
        if op == "stmw" and p[0] in SAVED:
            n = int(p[0][1:])
            return [f"r{k}" for k in range(n, 32)]
        if op == "stw" and p[0] in SAVED and p[0] not in out:
            out.append(p[0])
    return sorted(out, key=lambda r: int(r[1:]))


def self_increments(rows):
    """{register: sorted distinct nonzero strides} for every `addi rX,rX,K`.

    TWO EXCLUSIONS, both measured mistakes rather than caution:

    r1 is excluded because the epilogue's `addi r1,r1,N` is a stack pop, and
    reading it as an induction variable made every function report a spurious
    IV whose stride happened to equal the frame size.

    A ZERO stride is excluded because `addi rX,rX,0` is how MWCC completes an
    address -- `lis rX,sym@ha` then `addi rX,rX,sym@l` -- and objdump prints
    the low half as literal 0 with the relocation on a separate line. Reading
    it as an IV reported `r3+=0` on the WorldSaveInitState model, where r3 is
    genuinely a stride-12 IV as well, so a last-wins dict silently replaced the
    real stride with the address artifact.

    Distinct strides are KEPT as a list rather than collapsed, because a
    register carrying two of them is exactly the case a single answer would
    misreport. Use `induction_variables` for the unambiguous ones.
    """
    out = {}
    for op, o in rows:
        p = ops(o)
        if op != "addi" or len(p) != 3 or p[0] != p[1] or p[0] == "r1":
            continue
        try:
            stride = int(p[2], 0)
        except ValueError:
            continue
        if stride == 0:
            continue
        out.setdefault(p[0], set()).add(stride)
    return {r: sorted(v) for r, v in out.items()}


def induction_variables(rows):
    """{register: stride} for registers with exactly ONE nonzero stride."""
    return {r: v[0] for r, v in self_increments(rows).items() if len(v) == 1}


def zero_literals(rows, klass=SAVED):
    """Registers given a literal 0 by `li rX,0`, in emission order."""
    out = []
    for op, o in rows:
        p = ops(o)
        if op == "li" and len(p) == 2 and p[1] in ("0", "0x0") \
                and p[0] in klass and p[0] not in out:
            out.append(p[0])
    return out


def copies_of(rows, reg):
    """Registers that receive `addi rD,reg,0` -- MWCC's copy spelling."""
    out = []
    for op, o in rows:
        p = ops(o)
        if op == "addi" and len(p) == 3 and p[1] == reg and p[2] in ("0", "0x0"):
            out.append(p[0])
    return out


class Probe:
    """A compiler bench pinned to one TU's real flags."""

    def __init__(self, tu, out=None):
        src = source_of(tu)
        if src is None:
            raise SystemExit(f"no source for TU {tu!r} under src/")
        self.tu = tu
        self.flags, self.cc, self.wrapper = _flagsweep().ninja_base_cmd(
            src.relative_to(REPO))
        if not self.flags:
            raise SystemExit(f"no mwcc edge for {tu!r}; run configure.py first")
        self.out = Path(out) if out else OUT
        self.out.mkdir(parents=True, exist_ok=True)

    def argv(self, cfile, ofile, cc=None, extra=None, lang="c"):
        exe = REPO / "build" / "compilers" / "GC" / (cc or self.cc) \
            / "mwcceppc.exe"
        return (list(self.wrapper) + [str(exe)] + list(self.flags)
                + list(extra or []) + [f"-lang={lang}", "-c", str(cfile),
                                       "-o", str(ofile)])

    def compile(self, tag, text, fn="probefn", cc=None, extra=None, lang="c"):
        """(rows, error). rows is [] with no error if `fn` is absent."""
        cfile = self.out / f"{tag}.c"
        ofile = self.out / f"{tag}.o"
        cfile.write_text(text, newline="\n")
        r = subprocess.run(
            self.argv(cfile.relative_to(REPO), ofile.relative_to(REPO),
                      cc=cc, extra=extra, lang=lang),
            cwd=REPO, capture_output=True, text=True, timeout=300)
        if r.returncode != 0:
            tail = (r.stdout + r.stderr).strip().splitlines()
            return None, (tail[-1] if tail else f"exit {r.returncode}")
        return parse_rows(self.disasm(ofile), fn), None

    @staticmethod
    def disasm(obj):
        exe = REPO / "build" / "binutils" / "powerpc-eabi-objdump.exe"
        if not exe.exists():
            raise SystemExit(f"missing {exe}; see claimcheck.py's note")
        return subprocess.run([str(exe), "-d", "-r", str(obj)],
                              capture_output=True, text=True,
                              timeout=300).stdout

    def real_rows(self, fn):
        """The same function's rows from OUR real build, for a fidelity check."""
        obj = REPO / "build" / "GUNE5D" / "src" / (self.tu + ".o")
        if not obj.exists():
            return []
        return parse_rows(self.disasm(obj), fn)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("tu", help="TU whose flags to borrow, e.g. game/world/world")
    ap.add_argument("source", help="synthetic .c file to compile")
    ap.add_argument("--fn", default="probefn", help="function to disassemble")
    ap.add_argument("--cc", help="override the compiler version")
    args = ap.parse_args(argv)
    p = Probe(args.tu)
    rows, err = p.compile("cli", Path(args.source).read_text(), fn=args.fn,
                          cc=args.cc)
    if err:
        print(f"COMPILE FAILED: {err}", file=sys.stderr)
        return 1
    print(f"{args.fn}: {len(rows)} instructions, frame {frame(rows)}, "
          f"saved {','.join(saved(rows)) or 'none'}")
    ivs = self_increments(rows)
    if ivs:
        print("  self-incrementing registers: "
              + ", ".join(f"{r}+={'/'.join(str(x) for x in v)}"
                          for r, v in sorted(ivs.items())))
    for i, (op, o) in enumerate(rows):
        print(f"  {i:4} {op:10} {o}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
