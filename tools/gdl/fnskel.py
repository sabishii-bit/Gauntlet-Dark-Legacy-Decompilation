#!/usr/bin/env python3
"""Conservative C-skeleton generator for one target function (MWCC-aware).

NOT a decompiler. Emits a *reading scaffold*: frame/arg analysis, block
labels, and C-ish lines ONLY where the MWCC idiom is certain (conversion
transits, bool-from-compare, symbol materializations, call argument
summaries). Everything uncertain stays as `## asm:` lines. The output is
deliberately non-compilable -- storage classes, TU structure and statement
grouping are the human's job (they are whole-TU properties this tool cannot
see; a plausible-but-wrong skeleton is worse than none).

Usage:
  python tools/gdl/fnskel.py game/fakelib sceOpen
  python tools/gdl/fnskel.py auto_03_800E539C_text fn_800E8280

Reads the dtk-extracted target object (never our build).
"""

import re
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parent.parent.parent
OBJDUMP = REPO / "build/binutils/powerpc-eabi-objdump.exe"

CONDBR = {"beq", "bne", "blt", "bge", "bgt", "ble", "bso", "bns", "bdnz", "bdz"}
TAKEN = {"beq": "==", "bne": "!=", "blt": "<", "bge": ">=", "bgt": ">", "ble": "<="}


def parse(objfile, want):
    out = subprocess.run([str(OBJDUMP), "-dr", str(objfile)],
                         capture_output=True, text=True).stdout
    rows, base, cur, names = [], 0, None, []
    for line in out.splitlines():
        m = re.match(r"^([0-9a-f]+) <(.+)>:$", line)
        if m:
            cur = m.group(2)
            if not cur.startswith("fn_"):
                cur = re.sub(r"_80[0-9A-Fa-f]{6}$", "", cur)
            base = int(m.group(1), 16)
            names.append(cur)
            continue
        if cur != want:
            continue
        m = re.match(r"^\s+([0-9a-f]+):\s+(?:[0-9a-f]{2} ){4}\s*(\S+)\s*(.*)$", line)
        if m:
            rows.append({"off": int(m.group(1), 16) - base,
                         "op": m.group(2),
                         "args": m.group(3).strip(),
                         "reloc": None})
        elif "R_PPC" in line and rows:
            parts = line.strip().split()
            rn = parts[-1]
            if not rn.startswith("fn_"):
                rn = re.sub(r"_80[0-9A-Fa-f]{6}(?=$|\+)", "", rn)
            rows[-1]["reloc"] = (parts[-2].replace("R_PPC_", ""), rn)
    return rows, names


def reg_args(args):
    return [a.strip() for a in args.split(",")] if args else []


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    unit = re.sub(r"\.(c|cpp)$", "", sys.argv[1].replace("\\", "/"))
    fn = sys.argv[2]
    cands = [REPO / f"build/{VERSION}/obj/{unit}.o",
             REPO / f"build/{VERSION}/obj/{unit}_text.o"]
    obj = next((p for p in cands if p.exists()), None)
    if obj is None:
        print(f"target object not found: {[str(c) for c in cands]}")
        return 1
    rows, names = parse(obj, fn)
    if not rows:
        print(f"function {fn} not found; has: {', '.join(names)}")
        return 1

    n = len(rows)
    # ---- pass 1: prologue / frame ------------------------------------
    frame = 0
    saves = None
    params = {}          # rN -> home reg (prologue copies of r3..r10)
    for r in rows[:12]:
        if r["op"] == "stwu" and "(r1)" in r["args"]:
            frame = -int(reg_args(r["args"])[1].split("(")[0], 0)
        if r["op"] == "stmw":
            saves = reg_args(r["args"])[0]
        m = re.match(r"^(r\d+),(r([3-9]|10))$", r["args"].replace(" ", "")) \
            if r["op"] == "mr" else None
        if m:
            params[m.group(2)] = m.group(1)
        if r["op"] == "addi":
            a = reg_args(r["args"])
            if len(a) == 3 and a[2] == "0" and a[1] in [f"r{i}" for i in range(3, 11)]:
                params[a[1]] = a[0]
        if r["op"] == "mr." :
            a = reg_args(r["args"])
            if a[1] in [f"r{i}" for i in range(3, 11)]:
                params[a[1]] = a[0]

    # ---- pass 2: labels ----------------------------------------------
    targets = set()
    for r in rows:
        if r["op"].startswith("b") and r["op"] not in ("blr", "blrl", "bctr", "bctrl"):
            m = re.search(r"([0-9a-f]+)\s*<", r["args"] + " <")
            m2 = re.search(r"\b([0-9a-f]+) <", r["args"])
            if m2 and not r["reloc"]:
                targets.add(int(m2.group(1), 16) - (rows[0]["off"]) - 0)  # rel to fn
    # objdump prints absolute-within-section; recompute via <fn+0x..> form
    targets = set()
    for r in rows:
        m = re.search(r"<[^>]*\+0x([0-9a-f]+)>", r["args"])
        if m and not r["reloc"]:
            targets.add(int(m.group(1), 16))
        elif re.match(r"^b(eq|ne|lt|ge|gt|le|dnz|dz)?[+-]?$", r["op"]) and "<" in r["args"] and "+0x" not in r["args"] and not r["reloc"]:
            targets.add(0)

    # ---- pass 3: linear register/content tracking + emission ----------
    print(f"// {fn}: {n} insns, frame 0x{frame:X}"
          + (f", saves {saves}-r31" if saves else "")
          + (f", params: " + ", ".join(f"{k}->{v}" for k, v in params.items())
             if params else ""))
    print("// skeleton only -- verify every line against the asm before use")
    print(f"void {fn}(/* r3.. */)\n{{")

    content = {}         # reg -> descriptive string
    for k, v in params.items():
        content[v] = f"arg_{k}"
    i = 0
    while i < n:
        r = rows[i]
        off = r["off"]
        op, args, reloc = r["op"], r["args"], r["reloc"]
        A = reg_args(args)
        line = None
        eaten = 1

        if off in targets:
            print(f"L{off:x}:")

        # --- idiom: u32->float magic pair (xoris; stw hi; stw magic; lfd; fsubs)
        if op == "xoris" and i + 4 < n and "32768" in args:
            w = [rows[i + k]["op"] for k in range(1, 5)]
            if w[:2] == ["stw", "stw"] or (rows[i+1]["op"] == "stw" and "stw" in w):
                line = f"fX = (f32) (s32) {content.get(A[1], A[1])};  // int->float transit"
                # consume up to fsubs
                j = i + 1
                while j < n and rows[j]["op"] != "fsubs" and j < i + 7:
                    j += 1
                eaten = (j - i + 1) if j < n and rows[j]["op"] == "fsubs" else 1

        # --- idiom: fctiwz transit
        elif op == "fctiwz":
            j = i + 1
            lw = None
            while j < min(i + 6, n):
                if rows[j]["op"] == "lwz" and "(r1)" in rows[j]["args"]:
                    lw = reg_args(rows[j]["args"])[0]
                    break
                j += 1
            if lw:
                line = f"{lw} = (s32) {A[1]};  // float->int transit"
                eaten = j - i + 1

        # --- idiom: bool (x != 0): neg/addic/subfe
        elif op == "neg" and i + 2 < n and rows[i+1]["op"] == "addic" and rows[i+2]["op"] == "subfe":
            d = reg_args(rows[i+2]["args"])[0]
            line = f"{d} = ({content.get(A[1], A[1])} != 0);"
            eaten = 3

        # --- idiom: (x >= 0): srawi/srwi + subfc + adde
        elif op == "srawi" and i + 3 < n and rows[i+2]["op"] == "subfc" and rows[i+3]["op"] == "adde":
            line = f"rX = ({content.get(A[1], A[1])} >= 0);"
            eaten = 4

        # --- symbol materializations
        elif reloc and op in ("lis",):
            content[A[0]] = f"&{reloc[1]}@h"
        elif reloc and op == "addi" and A[1] in content and "@h" in content.get(A[1], ""):
            sym = content[A[1]].replace("@h", "")
            content[A[0]] = sym
            line = f"{A[0]} = {sym};"
        elif reloc and op == "li":
            content[A[0]] = f"&{reloc[1]}"
            line = f"{A[0]} = &{reloc[1]};  // sda"
        elif reloc and op in ("lwz", "lfs", "lfd", "lbz", "lhz") and "SDA21" in reloc[0]:
            content[A[0]] = reloc[1]
            line = f"{A[0]} = {reloc[1]};"
        elif reloc and op in ("stw", "stfs", "stfd", "stb", "sth") and "SDA21" in reloc[0]:
            line = f"{reloc[1]} = {A[0]};"

        # --- loads/stores through tracked pointers
        elif op in ("lwz", "lbz", "lhz", "lfs", "lfd") and "(" in args and not reloc:
            m = re.match(r"(\S+),(-?\d+)\((r\d+)\)", args.replace(" ", ""))
            if m and m.group(3) in content and m.group(3) != "r1":
                base = content[m.group(3)]
                line = f"{m.group(1)} = {base}->+0x{int(m.group(2)) & 0xffff:X};"
                content[m.group(1)] = f"{base}->+0x{int(m.group(2)) & 0xffff:X}"
        elif op in ("stw", "stb", "sth", "stfs", "stfd") and "(" in args and not reloc:
            m = re.match(r"(\S+),(-?\d+)\((r\d+)\)", args.replace(" ", ""))
            if m and m.group(3) in content and m.group(3) != "r1":
                line = f"{content[m.group(3)]}->+0x{int(m.group(2)) & 0xffff:X} = {m.group(1)};"

        # --- calls
        elif op == "bl" and reloc:
            argv = []
            used = set()
            for j in range(i - 1, max(i - 12, -1), -1):
                pa = reg_args(rows[j]["args"])
                if pa and re.match(r"^r([3-9]|10)$", pa[0]) and pa[0] not in used:
                    used.add(pa[0])
                    argv.append((pa[0], content.get(pa[0], rows[j]["op"] + " " + rows[j]["args"])))
                if rows[j]["op"] == "bl":
                    break
            argv = [f"{a}={d}" for a, d in sorted(argv)]
            line = f"{reloc[1]}({', '.join(argv)});"
            content = {k: v for k, v in content.items()
                       if k in ("r14", "r15") or (k[0] == "r" and k[1:].isdigit() and int(k[1:]) >= 14)}

        # --- compare + branch pairs
        elif op in ("cmpwi", "cmplwi") and i + 1 < n and rows[i+1]["op"] in TAKEN:
            br = rows[i + 1]
            m = re.search(r"\+0x([0-9a-f]+)>", br["args"])
            tgt = f"L{int(m.group(1), 16):x}" if m else "L?"
            v = content.get(A[-2] if len(A) > 2 else A[0], A[-2] if len(A) > 2 else A[0])
            line = f"if ({v} {TAKEN[br['op']]} {A[-1]}) goto {tgt};   // {op} {args}; {br['op']}"
            eaten = 2
        elif op == "blr":
            line = "return;  // r3 = " + content.get("r3", "?")

        if line:
            print(f"    {line}")
        else:
            print(f"    ## {op:8}{args}"
                  + (f"   @{reloc[1]}({reloc[0]})" if reloc else ""))
            # track simple copies
            if op in ("mr", "mr.") and len(A) == 2:
                content[A[0]] = content.get(A[1], A[1])
            elif op == "addi" and len(A) == 3 and A[2] == "0":
                content[A[0]] = content.get(A[1], A[1])
            elif A and A[0].startswith("r"):
                content.pop(A[0], None)
        i += eaten

    print("}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
