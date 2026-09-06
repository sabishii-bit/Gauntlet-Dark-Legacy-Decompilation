#!/usr/bin/env python3
"""Restore atree's retail cross-TU ELF symbol visibility.

The current source retains internal linkage for three large BSS arrays to
preserve their measured layout and code generation. Retail exports the same
storage to pb_diag. Promote only those three symbols without touching section
contents or relocations. This finite source constraint does not establish
that no alternative reconstruction could remove their remaining promotions.

sAtreeZero and natreelists now have ordinary source definitions and external
linkage. Require those definitions; never discover, rename or promote an
anonymous zero datum. Editable builds may change their values and layout.
"""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

EXPORTS = ("atree_handles", "atree_scroll", "whichatree")
SOURCE_EXPORTS = ("sAtreeZero", "natreelists")


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


def require_source_exports(symbols: list[SymbolFact]) -> None:
    for name in SOURCE_EXPORTS:
        definitions = [symbol for symbol in symbols
                       if symbol.name == name and symbol.section is not None]
        if len(definitions) != 1:
            raise ValueError(f"expected one source-defined {name}; found {len(definitions)}")
        symbol = definitions[0]
        if symbol.bind != "STB_GLOBAL" or symbol.kind != "STT_OBJECT":
            raise ValueError(f"{name}: expected source-defined global object")


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
    require_source_exports(before)

    command = [str(args.objcopy)]
    command.extend(f"--globalize-symbol={name}" for name in EXPORTS)
    command.append(str(export_input))
    if export_input.resolve() != args.output.resolve():
        command.append(str(args.output))
    subprocess.run(command, check=True)

    after = read_symbols(args.output)
    require_exports(after, "STB_GLOBAL")
    require_source_exports(after)
    print(
        "ATREE_EXPORTS: source-defined sAtreeZero, natreelists; promoted " + ", ".join(EXPORTS)
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
