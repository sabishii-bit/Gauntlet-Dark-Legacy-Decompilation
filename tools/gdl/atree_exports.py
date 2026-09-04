#!/usr/bin/env python3
"""Restore atree's retail cross-TU ELF symbol visibility.

GC 1.2.5 must see atree's BSS objects as internal-linkage definitions to
reproduce their retail layout and code generation.  The retail object exports
the same storage to pb_diag, however.  Promote those existing symbols after
compilation without touching section contents or relocations.

The compiler also emits the shared 0.0f datum as an anonymous local pool
object while atree's other functions refer to it as ``sAtreeZero``.  Discover
that object by ELF form and bytes rather than relying on MWCC's unstable @NN
name, then name and promote it.  A mod that supplies an ordinary defined
``sAtreeZero`` instead bypasses the discovery path.
"""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

EXPORTS = ("atree_handles", "atree_scroll", "whichatree", "natreelists")


@dataclass(frozen=True)
class SymbolFact:
    name: str
    bind: str
    kind: str
    section: str | None
    value: int
    size: int
    data: bytes | None


def read_symbols(path: Path) -> list[SymbolFact]:
    data = path.read_bytes()
    if data[:6] != b"\x7fELF\x01\x02":
        raise ValueError(f"{path}: expected ELF32 big-endian")
    section_at = struct.unpack_from(">I", data, 0x20)[0]
    entry_size, count, names_index = struct.unpack_from(">HHH", data, 0x2E)
    if entry_size < 40 or names_index >= count:
        raise ValueError(f"{path}: invalid section table")
    headers = [
        struct.unpack_from(">10I", data, section_at + i * entry_size)
        for i in range(count)
    ]
    names_header = headers[names_index]
    names_blob = data[names_header[4]:names_header[4] + names_header[5]]
    section_names = [
        names_blob[header[0]:].split(b"\0", 1)[0].decode("ascii")
        for header in headers
    ]
    try:
        symtab_index = section_names.index(".symtab")
    except ValueError as err:
        raise ValueError(f"{path}: no .symtab") from err
    symtab = headers[symtab_index]
    strings = headers[symtab[6]]
    string_blob = data[strings[4]:strings[4] + strings[5]]
    symbol_size = symtab[9] or 16
    facts = []
    bindings = {0: "STB_LOCAL", 1: "STB_GLOBAL", 2: "STB_WEAK"}
    kinds = {0: "STT_NOTYPE", 1: "STT_OBJECT", 2: "STT_FUNC"}
    for offset in range(symtab[4], symtab[4] + symtab[5], symbol_size):
        name_at, value, size, info, _, section_index = struct.unpack_from(
            ">IIIBBH", data, offset
        )
        name = string_blob[name_at:].split(b"\0", 1)[0].decode("latin1")
        section = None
        payload = None
        if 0 < section_index < len(headers):
            section = section_names[section_index]
            header = headers[section_index]
            if header[1] != 8 and size:  # SHT_NOBITS has no file payload.
                payload = data[header[4] + value:header[4] + value + size]
        facts.append(SymbolFact(
            name=name,
            bind=bindings.get(info >> 4, f"STB_{info >> 4}"),
            kind=kinds.get(info & 0xF, f"STT_{info & 0xF}"),
            section=section,
            value=value,
            size=size,
            data=payload,
        ))
    return facts


def choose_zero_symbol(symbols: list[SymbolFact]) -> str | None:
    defined = [
        symbol for symbol in symbols
        if symbol.name == "sAtreeZero" and symbol.section is not None
    ]
    if defined:
        if len(defined) != 1:
            raise ValueError("sAtreeZero has multiple defined symbols")
        return None

    candidates = [
        symbol for symbol in symbols
        if symbol.bind == "STB_LOCAL"
        and symbol.kind == "STT_OBJECT"
        and symbol.section == ".sdata2"
        and symbol.size == 4
        and symbol.data == b"\0\0\0\0"
    ]
    if len(candidates) != 1:
        names = ", ".join(symbol.name for symbol in candidates) or "none"
        raise ValueError(
            "expected one anonymous local 4-byte zero in .sdata2; "
            f"found {len(candidates)} ({names}). Define sAtreeZero explicitly "
            "if edited source intentionally creates multiple zero datums."
        )
    return candidates[0].name


def require_exports(symbols: list[SymbolFact], bind: str) -> None:
    for name in EXPORTS:
        rows = [
            symbol for symbol in symbols
            if symbol.name == name and symbol.section is not None
        ]
        if len(rows) != 1:
            raise ValueError(f"expected one defined {name}; found {len(rows)}")
        if rows[0].bind != bind:
            raise ValueError(
                f"{name}: expected binding {bind}, found {rows[0].bind}"
            )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--objcopy", required=True, type=Path)
    parser.add_argument("--webfrank-config", type=Path)
    parser.add_argument("--webfrank-unit")
    parser.add_argument("--target", type=Path)
    parser.add_argument("--image", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    export_input = args.input
    if args.webfrank_config is not None:
        if not args.webfrank_unit or args.target is None or args.image is None:
            raise ValueError(
                "--webfrank-config requires --webfrank-unit, --target, and --image"
            )
        webfrank = Path(__file__).with_name("webfrank.py")
        subprocess.run([
            sys.executable, str(webfrank), str(args.input), str(args.output),
            str(args.webfrank_config), args.webfrank_unit,
            "--target", str(args.target), "--image", str(args.image),
        ], check=True)
        export_input = args.output

    before = read_symbols(export_input)
    require_exports(before, "STB_LOCAL")
    zero_name = choose_zero_symbol(before)

    command = [str(args.objcopy)]
    if zero_name is not None:
        command.extend((
            f"--redefine-sym={zero_name}=sAtreeZero",
            "--globalize-symbol=sAtreeZero",
        ))
    command.extend(f"--globalize-symbol={name}" for name in EXPORTS)
    command.append(str(export_input))
    if export_input.resolve() != args.output.resolve():
        command.append(str(args.output))
    subprocess.run(command, check=True)

    after = read_symbols(args.output)
    require_exports(after, "STB_GLOBAL")
    zero_defs = [
        symbol for symbol in after
        if symbol.name == "sAtreeZero"
        and symbol.section == ".sdata2"
        and symbol.bind == "STB_GLOBAL"
        and symbol.size == 4
        and symbol.data == b"\0\0\0\0"
    ]
    if len(zero_defs) != 1:
        raise ValueError(
            f"expected one defined global sAtreeZero; found {len(zero_defs)}"
        )
    print(
        "ATREE_EXPORTS: promoted sAtreeZero, " + ", ".join(EXPORTS)
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
