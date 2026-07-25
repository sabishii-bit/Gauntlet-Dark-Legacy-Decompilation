#!/usr/bin/env python3
"""Per-function disassembly diff between the target object (extracted from the
DOL by dtk) and the base object (our compile), with addresses and branch
targets normalized so only real codegen differences survive.

Usage:
  python tools/fndiff.py dolphin/dvd/dvd.c              # all mismatching functions
  python tools/fndiff.py dolphin/dvd/dvd.c DVDInit      # specific function(s)
  python tools/fndiff.py dolphin/si/SIBios.c -l         # just list match status
  python tools/fndiff.py zlib/infblock.c --ops          # opcode-cluster view

--ops collapses each function to its opcode stream (registers, operands and
relocs ignored) and prints only the structurally inserted/deleted/replaced
clusters. Use it to separate real shape differences (missing statements,
moved blocks, extra calls) from register-renumber noise -- this view is what
located infblock's missing t<19 clamp and stripped error-path frees.

--count prints one summary line per function (target/base insn counts, total
diff lines, and "real" diff lines excluding reloc-name-only noise) -- use it
as the per-iteration score instead of piping through grep -c.

The base object is rebuilt via ninja automatically whenever the source file
is newer (pass --no-build to skip). This prevents analyzing stale objects.
"""

import difflib
import re
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
OBJDUMP = Path("build/binutils/powerpc-eabi-objdump.exe")

BRANCH_RE = re.compile(
    r"\b(b|bl|ba|bla|beq|bne|bgt|blt|bge|ble|bso|bns|bdnz|bdz)([+-]?)\s+(cr\d,)?[0-9a-f]+\s*$"
)


def parse(objfile: Path):
    """Return {function_name: [normalized instruction/reloc lines]}."""
    out = subprocess.run(
        [str(OBJDUMP), "-dr", str(objfile)], capture_output=True, text=True
    ).stdout
    funcs = {}
    cur = None
    for line in out.splitlines():
        m = re.match(r"^[0-9a-f]+ <(.+)>:$", line)
        if m:
            cur = re.sub(r"_80[0-9A-Fa-f]{6}$", "", m.group(1))
            funcs[cur] = []
            continue
        if cur is None:
            continue
        m = re.match(r"^\s+[0-9a-f]+:\s+(?:[0-9a-f]{2} ){4}\s*(.+)$", line)
        if m:
            ins = re.sub(r"<[^>]+>", "", m.group(1).strip())
            ins = BRANCH_RE.sub(lambda m: f"{m.group(1)}{m.group(2)} {m.group(3) or ''}<tgt>", ins)
            funcs[cur].append(ins.strip())
        elif "R_PPC" in line:
            rel = line.strip().split(maxsplit=1)[1]
            # dtk suffixes local symbol names with their address; strip so
            # target "changed_80345368" pairs with our "changed"
            rel = re.sub(r"_80[0-9A-Fa-f]{6}(?=$|\+)", "", rel)
            funcs[cur].append("    " + rel)
    return funcs


def opcodes(lines):
    """Instruction lines only (no relocs), reduced to the mnemonic."""
    return [ln.split()[0] for ln in lines if ln and not ln.startswith("    ")]


def ops_diff(name, t, b):
    to, bo = opcodes(t), opcodes(b)
    sm = difflib.SequenceMatcher(None, to, bo, autojunk=False)
    clusters = [x for x in sm.get_opcodes() if x[0] != "equal"]
    print(f"==== {name}: target {len(to)} insns, ours {len(bo)}"
          + (" (opcode streams identical -- diffs are register/reloc only)"
             if not clusters else ""))
    for tag, i1, i2, j1, j2 in clusters:
        print(f"  {tag:7} T[{i1}:{i2}]={to[i1:i2]}  O[{j1}:{j2}]={bo[j1:j2]}")


def main():
    flags = ("-l", "--ops", "--count", "--no-build")
    args = [a for a in sys.argv[1:] if a not in flags]
    list_only = "-l" in sys.argv
    ops_only = "--ops" in sys.argv
    count_only = "--count" in sys.argv
    no_build = "--no-build" in sys.argv
    if not args:
        print(__doc__)
        return 1

    unit = args[0].replace("\\", "/")
    unit = re.sub(r"\.(c|cpp)$", "", unit)
    target_o = Path(f"build/{VERSION}/obj/{unit}.o")
    base_o = Path(f"build/{VERSION}/src/{unit}.o")

    # rebuild the base object if the source is newer (stale-object trap)
    if not no_build:
        src = next((Path(f"src/{unit}{ext}") for ext in (".c", ".cpp")
                    if Path(f"src/{unit}{ext}").exists()), None)
        if src and (not base_o.exists()
                    or src.stat().st_mtime > base_o.stat().st_mtime):
            r = subprocess.run(["ninja", str(base_o)], capture_output=True, text=True)
            if r.returncode != 0:
                print(f"NINJA FAILED rebuilding {base_o}:")
                tail = (r.stdout + r.stderr).splitlines()
                print(chr(10).join(tail[-15:]))
                return 1
            print(f"(rebuilt {base_o.name})")

    for p in (target_o, base_o):
        if not p.exists():
            print(f"missing: {p} (run ninja / check unit path)")
            return 1

    target, base = parse(target_o), parse(base_o)
    names = args[1:] or sorted(
        set(target) | set(base), key=lambda n: list(target).index(n) if n in target else 999
    )

    for name in names:
        t, b = target.get(name), base.get(name)
        if t == b:
            if list_only or args[1:]:
                print(f"OK   {name}")
            continue
        if t is None or b is None:
            side = "target" if t is None else "base"
            print(f"ONLY-IN-{'BASE' if t is None else 'TARGET'}  {name}"
                  f"  (extra {side} fns are usually deadstripped statics)")
            continue
        if list_only:
            print(f"DIFF {name}")
            continue
        if count_only:
            diff = [l for l in difflib.unified_diff(t, b, lineterm="", n=0)
                    if l[:1] in "+-" and l[:3] not in ("+++", "---")]
            real = [l for l in diff if "R_PPC" not in l]
            ti = sum(1 for l in t if "R_PPC" not in l)
            bi = sum(1 for l in b if "R_PPC" not in l)
            print(f"DIFF {name}  insns {ti}/{bi}  lines {len(diff)}  real {len(real)}")
            continue
        if ops_only:
            ops_diff(name, t, b)
            continue
        print("=" * 20, name)
        for line in difflib.unified_diff(t, b, "target", "base", lineterm="", n=2):
            print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
