#!/usr/bin/env python3
"""Apply hash-guarded PowerPC register-web recolors to an MWCC ELF object.

Frank changes scheduling by compiling with a synthetic profile side effect and
then removing the instrumentation.  ``webfrank`` is the deliberately narrower
allocator analogue: it changes only decoded GPR operand fields in explicitly
listed function-relative ranges.  It does not copy target code, alter opcodes,
move instructions, or touch relocations/data.

Every patch is guarded by complete before/after function SHA-256 hashes.  A
source, compiler, or layout change therefore fails the build instead of
silently applying a stale binary rewrite.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from dataclasses import dataclass
from pathlib import Path


SHT_SYMTAB = 2


@dataclass(frozen=True)
class Section:
    index: int
    name: str
    section_type: int
    offset: int
    size: int
    link: int
    entry_size: int


@dataclass(frozen=True)
class Symbol:
    name: str
    value: int
    size: int
    section_index: int


def _u16(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def _u32(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def _cstring(data: bytes | bytearray, offset: int) -> str:
    end = data.index(0, offset)
    return bytes(data[offset:end]).decode("ascii")


def _sections(data: bytes | bytearray) -> list[Section]:
    section_header = _u32(data, 0x20)
    entry_size = _u16(data, 0x2E)
    count = _u16(data, 0x30)
    names_index = _u16(data, 0x32)
    raw = []
    for index in range(count):
        offset = section_header + index * entry_size
        raw.append(struct.unpack_from(">10I", data, offset))
    names_offset = raw[names_index][4]
    result = []
    for index, header in enumerate(raw):
        name_at, section_type, _, _, offset, size, link, _, _, item_size = header
        name = _cstring(data, names_offset + name_at) if name_at else ""
        result.append(Section(index, name, section_type, offset, size, link, item_size))
    return result


def _find_symbol(data: bytes | bytearray, sections: list[Section], name: str) -> Symbol:
    for table in sections:
        if table.section_type != SHT_SYMTAB:
            continue
        strings = sections[table.link]
        item_size = table.entry_size or 16
        for offset in range(table.offset, table.offset + table.size, item_size):
            name_at, value, size = struct.unpack_from(">III", data, offset)
            section_index = _u16(data, offset + 14)
            symbol_name = _cstring(data, strings.offset + name_at) if name_at else ""
            if symbol_name == name:
                return Symbol(symbol_name, value, size, section_index)
    raise KeyError(f"symbol {name!r} not found")


def _parse_int(value: int | str) -> int:
    return value if isinstance(value, int) else int(value, 0)


def _map_field(word: int, shift: int, mapping: dict[int, int]) -> int:
    old = (word >> shift) & 0x1F
    return (word & ~(0x1F << shift)) | (mapping.get(old, old) << shift)


def recolor_instruction(word: int, mapping: dict[int, int]) -> int:
    """Recolor GPR operands in the small, audited PPC integer-form subset."""
    opcode = word >> 26

    # D-form integer loads/stores and addi: RT/RS, RA, immediate.
    if opcode in {14, 32, 33, 34, 35, 36, 37, 38, 39,
                  40, 41, 42, 43, 44, 45, 46, 47}:
        word = _map_field(word, 21, mapping)
        return _map_field(word, 16, mapping)

    # rlwinm (including slwi/srwi aliases): RS, RA, SH/MB/ME.
    if opcode == 21:
        word = _map_field(word, 21, mapping)
        return _map_field(word, 16, mapping)

    if opcode == 31:
        xo = (word >> 1) & 0x3FF
        # subf, mulhw and add: RT, RA, RB.
        if xo in {40, 75, 266}:
            word = _map_field(word, 21, mapping)
            word = _map_field(word, 16, mapping)
            return _map_field(word, 11, mapping)
        # srawi: RS, RA, SH.
        if xo == 824:
            word = _map_field(word, 21, mapping)
            return _map_field(word, 16, mapping)

    raise ValueError(f"unsupported instruction 0x{word:08x} (opcode {opcode})")


def _sha256(data: bytes | bytearray) -> str:
    return hashlib.sha256(data).hexdigest()


def apply_patch(data: bytearray, patch: dict) -> tuple[str, str, int]:
    sections = _sections(data)
    symbol = _find_symbol(data, sections, patch["function"])
    text = sections[symbol.section_index]
    start = text.offset + symbol.value
    end = start + symbol.size
    before = _sha256(data[start:end])
    if before != patch["before_sha256"]:
        raise ValueError(
            f"{symbol.name}: input hash {before} != expected {patch['before_sha256']}"
        )

    changed = 0
    for region in patch["recolors"]:
        relative_start = _parse_int(region["start"])
        relative_end = _parse_int(region["end"])
        if relative_start % 4 or relative_end % 4 or not (0 <= relative_start <= relative_end <= symbol.size):
            raise ValueError(f"{symbol.name}: invalid recolor range {region}")
        mapping = {int(old): int(new) for old, new in region["gpr"].items()}
        if any(not 0 <= register < 32 for pair in mapping.items() for register in pair):
            raise ValueError(f"{symbol.name}: GPR map out of range: {mapping}")
        if len(set(mapping.values())) != len(mapping):
            raise ValueError(f"{symbol.name}: GPR map is not injective: {mapping}")

        for relative in range(relative_start, relative_end, 4):
            offset = start + relative
            word = _u32(data, offset)
            recolored = recolor_instruction(word, mapping)
            if recolored != word:
                struct.pack_into(">I", data, offset, recolored)
                changed += 1

    after = _sha256(data[start:end])
    if after != patch["after_sha256"]:
        raise ValueError(
            f"{symbol.name}: output hash {after} != expected {patch['after_sha256']}"
        )
    return before, after, changed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("config", type=Path)
    parser.add_argument("unit", help="unit key in the config, e.g. game/g3d/sndvoice")
    args = parser.parse_args()

    config = json.loads(args.config.read_text(encoding="utf-8"))
    patches = config.get("units", {}).get(args.unit)
    if not patches:
        raise KeyError(f"no webfrank configuration for {args.unit!r}")

    data = bytearray(args.input.read_bytes())
    total = 0
    for patch in patches:
        _, _, changed = apply_patch(data, patch)
        total += changed
        print(f"WEBFRANK {patch['function']}: recolored {changed} instructions")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    print(f"WEBFRANK {args.unit}: {total} instructions total")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
