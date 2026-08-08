#!/usr/bin/env python3
"""Emit a Metrowerks inline-assembly skeleton from a target function.

This is intended for the small class of functions whose portable C body is
already correct but is blocked by a compiler scheduling/register-allocation
wall.  It does not edit source files: it prints a guarded-asm-ready function
body, with local branch labels and common PowerPC relocations rewritten to MW
syntax.  Keep the existing C implementation as the non-MWERKS fallback.

Usage:
  python tools/gdl/mwbody.py game/sys/ml_mem AllocMem32 \
      --signature "void* AllocMem32(int size)"
"""

import argparse
import re
import subprocess
from pathlib import Path


REPO = Path(__file__).resolve().parent.parent.parent
VERSION = "GUNE5D"
OBJDUMP = REPO / "build/binutils/powerpc-eabi-objdump.exe"


def read_function(unit: str, function: str):
    obj = REPO / "build" / VERSION / "obj" / f"{unit}.o"
    if not obj.exists():
        raise SystemExit(f"missing target object: {obj}")
    result = subprocess.run(
        [str(OBJDUMP), "-dr", str(obj)], capture_output=True, text=True,
        check=True,
    )

    current = None
    base = 0
    rows = []
    for line in result.stdout.splitlines():
        header = re.match(r"^([0-9a-f]+) <(.+)>:$", line)
        if header:
            current = header.group(2)
            base = int(header.group(1), 16)
            continue
        if current != function:
            continue
        insn = re.match(
            r"^\s+([0-9a-f]+):\s+(?:[0-9a-f]{2} ){4}\s*(.+)$", line
        )
        if insn:
            rows.append({
                "offset": int(insn.group(1), 16) - base,
                "text": re.sub(r"\s+", " ", insn.group(2).strip()),
                "reloc": None,
            })
            continue
        reloc = re.search(r"R_PPC_([A-Z0-9_]+)\s+(.+)$", line)
        if reloc and rows:
            rows[-1]["reloc"] = (reloc.group(1), reloc.group(2).strip())
    if not rows:
        raise SystemExit(f"function not found: {function} in {obj}")
    return rows


def local_target(text: str, function: str):
    mnemonic = text.split(" ", 1)[0]
    if not mnemonic.startswith("b") or mnemonic in {"blr", "bctr", "bctrl"}:
        return None
    match = re.search(
        rf"\b[0-9a-f]+\s+<{re.escape(function)}(?:\+0x([0-9a-f]+))?>$",
        text,
    )
    if not match:
        return None
    return int(match.group(1), 16) if match.group(1) else 0


def replace_last_immediate(text: str, replacement: str):
    return re.sub(r"(?<![\w])(?:0x)?0(?=(?:\([^)]*\))?$)", replacement, text)


def format_signature(signature: str):
    if signature.startswith("static "):
        return "static asm " + signature.removeprefix("static ")
    return "asm " + signature


def format_instruction(row, labels, function):
    text = row["text"]
    mnemonic = text.split(" ", 1)[0]
    target = local_target(text, function)
    if target is not None and target in labels:
        text = re.sub(r"\b[0-9a-f]+\s+<[^>]+>$", labels[target], text)

    reloc = row["reloc"]
    if reloc:
        kind, symbol = reloc
        symbol = symbol.replace(" ", "")
        if kind == "REL24":
            text = f"{mnemonic} {symbol}"
        elif kind in {"ADDR16_HA", "ADDR16_HI", "ADDR16_LO"}:
            suffix = {"ADDR16_HA": "@ha", "ADDR16_HI": "@h",
                      "ADDR16_LO": "@l"}[kind]
            text = replace_last_immediate(text, symbol + suffix)
        elif kind == "EMB_SDA21":
            text = re.sub(r"0\((?:0|r0)\)$", f"{symbol}(r0)", text)

    if " " not in text:
        return f"    {text}"
    op, operands = text.split(" ", 1)
    return f"    {op} {operands}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("unit", help="unit path without main/ or extension")
    parser.add_argument("function")
    parser.add_argument("--signature", help="C signature used after 'asm'")
    args = parser.parse_args()

    unit = re.sub(r"\.(c|cpp)$", "", args.unit.replace("\\", "/"))
    rows = read_function(unit, args.function)
    offsets = {row["offset"] for row in rows}
    branch_targets = {
        target for row in rows
        if row["reloc"] is None
        and (target := local_target(row["text"], args.function)) in offsets
    }
    labels = {offset: f"{args.function}_L{offset:04X}"
              for offset in sorted(branch_targets)}

    signature = args.signature or f"void {args.function}(void)"
    print(format_signature(signature))
    print("{")
    print("    nofralloc")
    for row in rows:
        if row["offset"] in labels:
            print(labels[row["offset"]] + ":")
        print(format_instruction(row, labels, args.function))
    print("}")


if __name__ == "__main__":
    main()
