#!/usr/bin/env python3
"""Apply one hash-guarded, fixed-size P6 branch-carrier repair.

This is deliberately separate from Frank and WebFrank.  It does not insert
instructions or copy target words.  A rule rotates one existing direct branch
atom across an explicitly audited region, re-encodes a fixed list of direct
branches from symbolic kinds/destinations, and then requires the complete
function and .text postimages to equal the extracted target.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from dataclasses import dataclass
from pathlib import Path

try:
    from .webfrank import Section, Symbol, _find_symbol, _sections, _symbols
except ImportError:  # Direct script execution from the repository root.
    from webfrank import Section, Symbol, _find_symbol, _sections, _symbols


SHT_RELA = 4
DIRECT_BRANCH_OPCODES = {16, 18}
CONTROL_OPCODES = {16, 17, 18, 19}


@dataclass(frozen=True)
class DirectBranch:
    offset: int
    kind: str
    destination: int


@dataclass(frozen=True)
class NamedRelocation:
    offset: int
    relocation_type: int
    symbol: str
    addend: int


def _sha256(data: bytes | bytearray) -> str:
    return hashlib.sha256(data).hexdigest()


def _parse_int(value: int | str) -> int:
    return value if isinstance(value, int) else int(value, 0)


def _word(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def _signed(value: int, bits: int) -> int:
    sign = 1 << (bits - 1)
    return (value ^ sign) - sign


def is_control(word: int) -> bool:
    return word >> 26 in CONTROL_OPCODES


def decode_direct_branch(word: int, offset: int) -> DirectBranch:
    opcode = word >> 26
    if opcode == 18:
        if word & 3:
            raise ValueError(f"branch at +0x{offset:x} uses AA/LK")
        displacement = _signed(word & 0x03FFFFFC, 26)
        return DirectBranch(offset, "b", offset + displacement)
    if opcode == 16:
        if word & 3:
            raise ValueError(f"conditional branch at +0x{offset:x} uses AA/LK")
        bo = (word >> 21) & 0x1F
        bi = (word >> 16) & 0x1F
        if bi != 2 or bo not in {4, 12}:
            raise ValueError(
                f"unsupported conditional branch at +0x{offset:x}: "
                f"BO={bo}, BI={bi}"
            )
        kind = "bne" if bo == 4 else "beq"
        displacement = _signed(word & 0xFFFC, 16)
        return DirectBranch(offset, kind, offset + displacement)
    raise ValueError(f"instruction at +0x{offset:x} is not a direct branch")


def encode_direct_branch(kind: str, offset: int, destination: int) -> int:
    displacement = destination - offset
    if displacement % 4:
        raise ValueError("branch destination is not word-aligned")
    if kind == "b":
        if not -(1 << 25) <= displacement < (1 << 25):
            raise ValueError("unconditional branch displacement is out of range")
        return (18 << 26) | (displacement & 0x03FFFFFC)
    if kind in {"beq", "bne"}:
        if not -(1 << 15) <= displacement < (1 << 15):
            raise ValueError("conditional branch displacement is out of range")
        bo = 12 if kind == "beq" else 4
        bi = 2
        return (16 << 26) | (bo << 21) | (bi << 16) | (displacement & 0xFFFC)
    raise ValueError(f"unsupported direct branch kind {kind!r}")


def _branch_specs(raw: list[dict]) -> list[DirectBranch]:
    return [
        DirectBranch(
            _parse_int(item["at"]), item["kind"], _parse_int(item["destination"])
        )
        for item in raw
    ]


def _validate_edges(data: bytes, expected: list[DirectBranch], label: str) -> None:
    observed = []
    for offset in range(0, len(data), 4):
        word = _word(data, offset)
        if word >> 26 in DIRECT_BRANCH_OPCODES and not (word & 1):
            observed.append(decode_direct_branch(word, offset))
    if observed != expected:
        raise ValueError(f"{label} control edge mismatch: {observed} != {expected}")


def transform_fixed_carrier(
    current: bytes,
    target: bytes,
    relocations: list[NamedRelocation],
    rule: dict,
) -> bytes:
    """Rotate/re-encode one audited fixed-size carrier, failing closed."""
    if len(current) != len(target) or len(current) % 4:
        raise ValueError("carrier and target must have equal word-aligned sizes")
    if len(current) != _parse_int(rule["function_size"]):
        raise ValueError("carrier function size changed")
    if _sha256(current) != rule["before_sha256"]:
        raise ValueError("carrier input hash changed")
    if _sha256(target) != rule["after_sha256"]:
        raise ValueError("target function hash changed")

    before_edges = _branch_specs(rule["before_edges"])
    after_edges = _branch_specs(rule["after_edges"])
    _validate_edges(current, before_edges, "carrier")

    expected_relocations = [
        NamedRelocation(
            _parse_int(item["at"]),
            _parse_int(item["type"]),
            item["symbol"],
            _parse_int(item.get("addend", 0)),
        )
        for item in rule["function_relocations"]
    ]
    if relocations != expected_relocations:
        raise ValueError(
            f"carrier relocation map changed: {relocations} != {expected_relocations}"
        )

    move_from = _parse_int(rule["move"]["from"])
    move_to = _parse_int(rule["move"]["to"])
    if move_from % 4 or move_to % 4 or not 0 <= move_to < move_from < len(current):
        raise ValueError("invalid fixed-carrier move")
    if any(move_to <= relocation.offset <= move_from for relocation in relocations):
        raise ValueError("relocation overlaps moved/crossed instruction atoms")

    moved = decode_direct_branch(_word(current, move_from), move_from)
    if moved.kind != "b":
        raise ValueError("moved atom is not the audited unconditional branch")
    expected_crossings = [_parse_int(value) for value in rule["move"]["crosses"]]
    observed_crossings = [
        offset
        for offset in range(move_to, move_from, 4)
        if is_control(_word(current, offset))
    ]
    if observed_crossings != expected_crossings:
        raise ValueError(
            f"crossing branch ambiguity: {observed_crossings} != {expected_crossings}"
        )
    for offset in observed_crossings:
        decode_direct_branch(_word(current, offset), offset)

    atoms = [current[offset:offset + 4] for offset in range(0, len(current), 4)]
    source = move_from // 4
    destination = move_to // 4
    atom = atoms.pop(source)
    atoms.insert(destination, atom)
    rotated = bytearray(b"".join(atoms))

    rewrites = _branch_specs(rule["reencode"])
    rewrite_offsets = {branch.offset for branch in rewrites}
    if rewrite_offsets != {_parse_int(value) for value in rule["changed_offsets"]}:
        raise ValueError("branch rewrite offsets do not match audited changed offsets")

    # Outside the explicitly re-encoded branch atoms, the rotated instruction
    # stream (including every register field) must already equal the target.
    for offset in range(0, len(rotated), 4):
        if offset in rewrite_offsets:
            if _word(rotated, offset) >> 26 not in DIRECT_BRANCH_OPCODES:
                raise ValueError(f"rewrite site +0x{offset:x} is not a direct branch")
            continue
        if rotated[offset:offset + 4] != target[offset:offset + 4]:
            raise ValueError(f"non-branch target mismatch at +0x{offset:x}")

    for branch in rewrites:
        struct.pack_into(
            ">I",
            rotated,
            branch.offset,
            encode_direct_branch(branch.kind, branch.offset, branch.destination),
        )

    result = bytes(rotated)
    _validate_edges(result, after_edges, "postimage")
    if result != target or _sha256(result) != rule["after_sha256"]:
        raise ValueError("fixed-carrier target postimage mismatch")
    return result


def _named_relocations(
    data: bytes | bytearray, sections: list[Section], section_index: int
) -> list[NamedRelocation]:
    result = []
    for relocation_section in sections:
        if relocation_section.section_type != SHT_RELA or relocation_section.info != section_index:
            continue
        symbol_table = sections[relocation_section.link]
        strings = sections[symbol_table.link]
        symbol_entry_size = symbol_table.entry_size or 16
        entry_size = relocation_section.entry_size or 12
        if entry_size != 12 or relocation_section.size % entry_size:
            raise ValueError("unsupported relocation section layout")
        for at in range(
            relocation_section.offset,
            relocation_section.offset + relocation_section.size,
            entry_size,
        ):
            offset, info, addend = struct.unpack_from(">IIi", data, at)
            symbol_index = info >> 8
            symbol_at = symbol_table.offset + symbol_index * symbol_entry_size
            name_at = struct.unpack_from(">I", data, symbol_at)[0]
            symbol = ""
            if name_at:
                end = data.index(0, strings.offset + name_at)
                symbol = bytes(data[strings.offset + name_at:end]).decode("ascii")
            result.append(NamedRelocation(offset, info & 0xFF, symbol, addend))
    return result


def _function_bytes(data: bytes | bytearray, sections: list[Section], symbol: Symbol) -> bytes:
    section = sections[symbol.section_index]
    start = section.offset + symbol.value
    return bytes(data[start:start + symbol.size])


def validate_siblings(
    current: dict[str, bytes], target: dict[str, bytes], expected: dict[str, str]
) -> None:
    for name, wanted_hash in expected.items():
        if name not in current or name not in target:
            raise ValueError(f"missing sibling {name}")
        if _sha256(current[name]) != wanted_hash:
            raise ValueError(f"sibling mutation: {name} input hash changed")
        if current[name] != target[name] or _sha256(target[name]) != wanted_hash:
            raise ValueError(f"sibling mutation: {name} is not target-exact")


def load_rule(config: dict, unit: str) -> dict:
    rule = config.get("units", {}).get(unit)
    if not rule:
        raise KeyError(f"no p6frank configuration for {unit!r}")
    return rule


def apply_object(current_data: bytes, target_data: bytes, rule: dict) -> bytes:
    data = bytearray(current_data)
    sections = _sections(data)
    target_sections = _sections(target_data)
    symbol = _find_symbol(data, sections, rule["function"])
    target_symbol = _find_symbol(target_data, target_sections, rule["function"])
    text = sections[symbol.section_index]
    target_text = target_sections[target_symbol.section_index]
    if text.name != ".text" or target_text.name != ".text" or text.size != target_text.size:
        raise ValueError("fixed carrier requires equal .text sections")
    for forbidden in rule.get("forbid_sections", []):
        if any(section.name == forbidden and section.size for section in sections):
            raise ValueError(f"unexpected section {forbidden}")
        if any(section.name == forbidden and section.size for section in target_sections):
            raise ValueError(f"unexpected target section {forbidden}")

    current_symbols = {item.name: _function_bytes(data, sections, item) for item in _symbols(data, sections) if item.size and item.section_index < len(sections) and sections[item.section_index].name == ".text"}
    target_symbols = {item.name: _function_bytes(target_data, target_sections, item) for item in _symbols(target_data, target_sections) if item.size and item.section_index < len(target_sections) and target_sections[item.section_index].name == ".text"}
    validate_siblings(current_symbols, target_symbols, rule["siblings"])

    all_relocations = _named_relocations(data, sections, symbol.section_index)
    function_relocations = [
        NamedRelocation(item.offset - symbol.value, item.relocation_type, item.symbol, item.addend)
        for item in all_relocations
        if symbol.value <= item.offset < symbol.value + symbol.size
    ]
    transformed = transform_fixed_carrier(
        current_symbols[symbol.name],
        target_symbols[target_symbol.name],
        function_relocations,
        rule,
    )
    start = text.offset + symbol.value
    data[start:start + symbol.size] = transformed

    output_text = bytes(data[text.offset:text.offset + text.size])
    wanted_text = bytes(target_data[target_text.offset:target_text.offset + target_text.size])
    if _sha256(output_text) != rule["after_text_sha256"] or output_text != wanted_text:
        raise ValueError("complete .text postimage mismatch")

    after_relocations = _named_relocations(data, sections, symbol.section_index)
    if after_relocations != all_relocations:
        raise ValueError("relocation table changed")
    return bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("config", type=Path)
    parser.add_argument("unit")
    parser.add_argument("--target", type=Path, required=True)
    args = parser.parse_args()
    rule = load_rule(json.loads(args.config.read_text(encoding="utf-8")), args.unit)
    result = apply_object(args.input.read_bytes(), args.target.read_bytes(), rule)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result)
    print(f"P6FRANK {args.unit}:{rule['function']} fixed-size target-exact")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
