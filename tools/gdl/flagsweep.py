#!/usr/bin/env python3
"""Per-TU compiler flag/version sweep vs the dtk target object (walls harness).

Answers "does ANY flag, -opt suboption, -proc model, or archive compiler
version move this function?" in one run. Used to close the parked-walls flag
axis (2026-07): no parked residual moved under any variant; see PARKED.txt.

Compiles one source file with the project's exact command line plus a variant
flag set, extracts one function's disassembly from both the variant object and
the dtk target object, normalizes (addresses, branch targets, reloc names),
and reports the diff line count plus an optional site-pattern grep.

Usage:
  python walls_flagsweep.py <src> <target_obj> <fn> [--site REGEX] [--variant "FLAGS" ...]
Example:
  python walls_flagsweep.py src/game/pb/pb_tree.cpp \
      build/GUNE5D/obj/game/pb/pb_tree.o pbTraverseDrawObjects \
      --site "4096\\(r" --variant "-opt nostrength" --variant "-proc 750"

With no --variant args, runs the built-in battery. Objects/artifacts go to
the scratch dir (never the build tree). Compiler defaults to GC/1.2.5n; a
variant starting with "CC=" swaps the compiler dir instead of adding flags:
  --variant "CC=2.7"
"""
import argparse
import difflib
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OBJDUMP = ROOT / "build/binutils/powerpc-eabi-objdump.exe"
SCRATCH = Path(os.environ.get("WALLS_SCRATCH", tempfile.gettempdir())) / "walls_sweep"
SCRATCH.mkdir(parents=True, exist_ok=True)

def ninja_base_cmd(src: Path):
    """Pull the project's exact mwcc argv for this source from ninja."""
    import shlex
    rel = src.relative_to(ROOT) if src.is_absolute() else src
    objpath = "build/GUNE5D/" + re.sub(r"\.(c|cpp)$", ".o", str(rel).replace("\\", "/"))
    out = subprocess.run(
        ["ninja", "-t", "commands", objpath],
        capture_output=True, text=True, cwd=str(ROOT),
    ).stdout
    for line in out.splitlines():
        if "mwcceppc.exe" in line and str(rel.name) in line:
            toks = shlex.split(line.replace("\\", "/"))
            i = next(j for j, t in enumerate(toks) if t.endswith("mwcceppc.exe"))
            cc_default = toks[i]
            args = toks[i + 1:]
            # strip -MMD, -c src, -o out, -lang (re-added later)
            keep, skip = [], 0
            for j, t in enumerate(args):
                if skip:
                    skip -= 1
                    continue
                if t in ("-MMD",):
                    continue
                if t in ("-c", "-o"):
                    skip = 1
                    continue
                if t.startswith("-lang"):
                    continue
                keep.append(t)
            m = re.search(r"compilers/GC/([^/]+)/mwcceppc", cc_default)
            return keep, (m.group(1) if m else "1.2.5n")
    raise SystemExit(f"no mwcc command found for {objpath}")

BATTERY = [
    "",  # control
    "-opt nostrength",
    "-opt noschedule",
    "-opt nopeephole",
    "-opt space",
    "-opt speed",
    "-opt noprop",
    "-opt nolifetimes",
    "-opt nocse",
    "PROC=750",
    "PROC=603e",
    "-sym on",
    "-inline auto,level=1",
    "-inline auto,level=2",
    "-inline all",
    "-schedule off",
    "-schedule on",
    "CC=1.2.5",
    "CC=1.1p1",
    "CC=1.3.2",
    "CC=2.0p1",
    "CC=2.5",
    "CC=2.6",
    "CC=2.7",
]

BRANCH_RE = re.compile(
    r"^(b|bl|ba|bla|beq|bne|bgt|blt|bge|ble|bso|bns|bdnz|bdz)([+-]?)(\s+cr\d,)?\s+[0-9a-f]+\b"
)


def disasm_fn(obj: Path, fn: str):
    out = subprocess.run(
        [str(OBJDUMP), "-dr", str(obj)], capture_output=True, text=True
    ).stdout
    lines = []
    grab = False
    for ln in out.splitlines():
        m = re.match(r"^[0-9a-f]+ <(.+)>:", ln)
        if m:
            grab = m.group(1) == fn
            continue
        if not grab:
            continue
        rm = re.match(r"^\s+[0-9a-f]+:\s+R_PPC_(\S+)\s+(\S+)", ln)
        if rm:
            kind, sym = rm.groups()
            sym = re.sub(r"^\.{3}(bss|sbss|sdata|data|rodata)\.\d+.*", "SECREL", sym)
            sym = re.sub(r"^@\d+$", "POOL", sym)
            sym = re.sub(r"^lbl_[0-9A-Fa-f]+$", "POOL", sym)
            lines.append(f"  RELOC {kind} {sym}")
            continue
        im = re.match(r"^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2} ){4}\s*(\S+)\s*(.*)$", ln)
        if im:
            op, args = im.groups()
            args = args.strip()
            bm = BRANCH_RE.match(f"{op} {args}".strip()) if args else None
            if op.startswith("b") and re.search(r"\b[0-9a-f]+ <", args):
                args = re.sub(r"[0-9a-f]+ <[^>]*>", "TGT", args)
            elif op[0] == "b" and re.match(r"^[0-9a-f]+$", args.split(",")[-1].strip()):
                parts = args.split(",")
                parts[-1] = "TGT"
                args = ",".join(parts)
            lines.append(f"{op} {args}".strip())
    return lines


def compile_variant(src: Path, variant: str, tag: str, base):
    base_flags, cc_default = base
    cc_dir = cc_default
    flags = list(base_flags)
    extra = []
    for tok in variant.split(";"):
        tok = tok.strip()
        if not tok:
            continue
        if tok.startswith("CC="):
            cc_dir = tok[3:]
        elif tok.startswith("PROC="):
            flags[flags.index("gekko")] = tok[5:]
        else:
            extra += tok.split()
    cc = ROOT / f"build/compilers/GC/{cc_dir}/mwcceppc.exe"
    if not cc.exists():
        return None, f"no compiler {cc_dir}"
    outdir = SCRATCH / tag
    outdir.mkdir(parents=True, exist_ok=True)
    lang = ["-lang=c++"] if src.suffix == ".cpp" else []
    cmd = [str(cc)] + flags + extra + lang + ["-c", str(src), "-o", str(outdir)]
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=str(ROOT))
    obj = outdir / (src.stem + ".o")
    if r.returncode != 0 or not obj.exists():
        msg = (r.stdout + r.stderr).strip().replace("\n", " | ")[:200]
        return None, f"compile failed: {msg}"
    return obj, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("target_obj")
    ap.add_argument("fn")
    ap.add_argument("--site", default=None)
    ap.add_argument("--variant", action="append", default=None)
    ap.add_argument("--show", default=None,
                    help="print the normalized unified diff for ONE variant")
    args = ap.parse_args()

    src = ROOT / args.src
    base = ninja_base_cmd(ROOT / args.src)
    tgt = disasm_fn(ROOT / args.target_obj, args.fn)
    if not tgt:
        print(f"target fn {args.fn} not found in {args.target_obj}")
        return 1
    site_re = re.compile(args.site) if args.site else None
    variants = args.variant if args.variant else BATTERY
    if args.show is not None:
        obj, err = compile_variant(src, args.show, "show", base)
        if err:
            print(err)
            return 1
        got = disasm_fn(obj, args.fn)
        for ln in difflib.unified_diff(tgt, got, "target", "variant", lineterm="", n=2):
            print(ln)
        return 0

    print(f"target: {len(tgt)} lines ({args.fn})")
    for i, v in enumerate(variants):
        tag = f"v{i:02d}"
        obj, err = compile_variant(src, v, tag, base)
        name = v if v else "(control)"
        if err:
            print(f"{name:34s} -- {err}")
            continue
        got = disasm_fn(obj, args.fn)
        if not got:
            print(f"{name:34s} -- fn missing from object")
            continue
        sm = difflib.SequenceMatcher(a=tgt, b=got, autojunk=False)
        diff = sum(
            max(i2 - i1, j2 - j1)
            for op, i1, i2, j1, j2 in sm.get_opcodes()
            if op != "equal"
        )
        sitehit = ""
        if site_re:
            hit = any(site_re.search(l) for l in got)
            want = any(site_re.search(l) for l in tgt)
            sitehit = f"  site[{args.site}]: base={'YES' if hit else 'no'} target={'YES' if want else 'no'}"
        print(f"{name:34s} -- {len(got)} lines, diff {diff}{sitehit}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
