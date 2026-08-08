#!/usr/bin/env python3
"""Emit a Metrowerks inline-assembly skeleton from a target function.

This is intended for the small class of functions whose portable C body is
already correct but is blocked by a compiler scheduling/register-allocation
wall.  By default it prints a guarded-asm-ready function body, with local
branch labels and common PowerPC relocations rewritten to MW syntax.  With
``--apply``, it wraps the named definition in-place and keeps its existing C
implementation as the non-MWERKS fallback.

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
SYMBOLS = REPO / "config" / VERSION / "symbols.txt"
_SYMBOL_NAMES = None


def configured_symbol_names():
    global _SYMBOL_NAMES
    if _SYMBOL_NAMES is None:
        _SYMBOL_NAMES = {
            match.group(1)
            for line in SYMBOLS.read_text().splitlines()
            if (match := re.match(r"^([A-Za-z_][A-Za-z0-9_]*)\s*=", line))
        }
    return _SYMBOL_NAMES


def canonical_symbol(symbol: str):
    """Undo DTK's address suffix when the unsuffixed link symbol exists."""
    match = re.match(r"^(.+)_([0-9A-Fa-f]{8})$", symbol)
    if match and match.group(1) in configured_symbol_names():
        return match.group(1)
    return symbol


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
        symbol = canonical_symbol(symbol.replace(" ", ""))
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


def render_body(rows, function: str, signature: str):
    unsupported = [
        row for row in rows
        if row["reloc"] and row["reloc"][0] == "EMB_SDA21"
        and not re.search(r"0\((?:0|r0)\)$", row["text"])
    ]
    if unsupported:
        details = ", ".join(
            f"0x{row['offset']:X}:{row['reloc'][1]}" for row in unsupported
        )
        raise ValueError(
            "unsupported bare-immediate SDA21 relocation(s): " + details
        )
    offsets = {row["offset"] for row in rows}
    branch_targets = {
        target for row in rows
        if row["reloc"] is None
        and (target := local_target(row["text"], function)) in offsets
    }
    labels = {offset: f"{function}_L{offset:04X}"
              for offset in sorted(branch_targets)}

    lines = [format_signature(signature), "{", "    nofralloc"]
    for row in rows:
        if row["offset"] in labels:
            lines.append(labels[row["offset"]] + ":")
        lines.append(format_instruction(row, labels, function))
    lines.append("}")
    return "\n".join(lines)


def find_definition(source: str, signature: str):
    """Return the exact [start, end) range of a C function definition."""
    for match in re.finditer(re.escape(signature), source):
        cursor = match.end()
        while cursor < len(source) and source[cursor].isspace():
            cursor += 1
        if cursor >= len(source) or source[cursor] != "{":
            continue

        depth = 0
        state = "code"
        i = cursor
        while i < len(source):
            char = source[i]
            nxt = source[i + 1] if i + 1 < len(source) else ""
            if state == "code":
                if char == "/" and nxt == "*":
                    state = "block_comment"
                    i += 1
                elif char == "/" and nxt == "/":
                    state = "line_comment"
                    i += 1
                elif char == '"':
                    state = "string"
                elif char == "'":
                    state = "char"
                elif char == "{":
                    depth += 1
                elif char == "}":
                    depth -= 1
                    if depth == 0:
                        return match.start(), i + 1
            elif state == "block_comment" and char == "*" and nxt == "/":
                state = "code"
                i += 1
            elif state == "line_comment" and char == "\n":
                state = "code"
            elif state in {"string", "char"}:
                if char == "\\":
                    i += 1
                elif (state == "string" and char == '"') or (
                    state == "char" and char == "'"
                ):
                    state = "code"
            i += 1
    raise ValueError(f"definition with signature not found: {signature}")


def wrap_portable_definition(source: str, signature: str, asm_body: str):
    start, end = find_definition(source, signature)
    if source[max(0, start - 32):start].rstrip().endswith("#ifdef __MWERKS__"):
        raise ValueError(f"definition is already guarded: {signature}")
    portable = source[start:end]
    replacement = (
        "#ifdef __MWERKS__\n" + asm_body + "\n#else\n" + portable
        + "\n#endif"
    )
    return source[:start] + replacement + source[end:]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("unit", help="unit path without main/ or extension")
    parser.add_argument("function")
    parser.add_argument("--signature", help="C signature used after 'asm'")
    parser.add_argument(
        "--apply", metavar="SOURCE",
        help="wrap this source definition in-place, preserving portable C",
    )
    args = parser.parse_args()

    unit = re.sub(r"\.(c|cpp)$", "", args.unit.replace("\\", "/"))
    rows = read_function(unit, args.function)
    signature = args.signature or f"void {args.function}(void)"
    body = render_body(rows, args.function, signature)
    if args.apply:
        path = REPO / args.apply
        source = path.read_text()
        path.write_text(wrap_portable_definition(source, signature, body))
    else:
        print(body)


if __name__ == "__main__":
    main()
