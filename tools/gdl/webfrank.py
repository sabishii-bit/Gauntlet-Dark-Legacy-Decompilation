#!/usr/bin/env python3
"""Apply hash-guarded, individually audited MWCC corrections to an ELF object.

Frank changes scheduling by compiling with a synthetic profile side effect and
then removing the instrumentation.  ``webfrank`` is the deliberately narrower
allocator analogue. Its ordinary rules change only PowerPC register fields in
audited functions. A rule may name explicit GPR recolors or copy the four
five-bit register slots from the extracted target after proving every other
instruction bit already agrees.

The narrower scheduler extension may explicitly permute a proved-independent,
straight-line list of existing instruction atoms. It rejects control
instructions, requires a complete bijection and dependency audit, moves each
relocation with its instruction atom, and verifies region, relocation, and full
function hashes before and after. It never copies opcodes, immediates, branch
encodings, relocation payloads, or data from the target.

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
SHT_RELA = 4
REGISTER_FIELD_SHIFTS = (6, 11, 16, 21)
REGISTER_FIELD_MASK = sum(0x1F << shift for shift in REGISTER_FIELD_SHIFTS)


@dataclass(frozen=True)
class Section:
    index: int
    name: str
    section_type: int
    offset: int
    size: int
    link: int
    info: int
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
        name_at, section_type, _, _, offset, size, link, info, _, item_size = header
        name = _cstring(data, names_offset + name_at) if name_at else ""
        result.append(
            Section(index, name, section_type, offset, size, link, info, item_size)
        )
    return result


def _symbols(data: bytes | bytearray, sections: list[Section]) -> list[Symbol]:
    result = []
    for table in sections:
        if table.section_type != SHT_SYMTAB:
            continue
        strings = sections[table.link]
        item_size = table.entry_size or 16
        for offset in range(table.offset, table.offset + table.size, item_size):
            name_at, value, size = struct.unpack_from(">III", data, offset)
            section_index = _u16(data, offset + 14)
            symbol_name = _cstring(data, strings.offset + name_at) if name_at else ""
            if symbol_name:
                result.append(Symbol(symbol_name, value, size, section_index))
    return result


def _find_symbol(data: bytes | bytearray, sections: list[Section], name: str) -> Symbol:
    for symbol in _symbols(data, sections):
        if symbol.name == name:
            return symbol
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


def _relocation_sha256(relocations: list[tuple[int, int, int]]) -> str:
    payload = bytearray()
    for offset, info, addend in relocations:
        payload += struct.pack(">IIi", offset, info, addend)
    return _sha256(payload)


def _is_control_instruction(word: int) -> bool:
    """Return true for PPC branch/call/system/XL control instructions."""
    opcode = word >> 26
    return opcode in {16, 17, 18, 19}


def permute_instruction_atoms(
    current: bytes,
    order: list[int],
    relocations: list[tuple[int, int, int]],
    *,
    before_sha256: str,
    after_sha256: str,
    before_relocations_sha256: str,
    after_relocations_sha256: str,
) -> tuple[bytes, list[tuple[int, int, int]], int]:
    """Apply one explicit instruction-atom permutation, failing closed.

    ``order[destination]`` names the source instruction atom. Relocations use
    byte offsets relative to ``current`` and move with their source atom while
    retaining the original within-instruction byte offset, symbol, and addend.
    """
    if _sha256(current) != before_sha256:
        raise ValueError("instruction permutation input hash changed")
    if len(current) % 4:
        raise ValueError("instruction permutation region is not word-aligned")

    count = len(current) // 4
    if len(order) != count or sorted(order) != list(range(count)):
        raise ValueError("instruction permutation is not a bijection")

    atoms = [current[offset:offset + 4] for offset in range(0, len(current), 4)]
    for atom in atoms:
        if _is_control_instruction(_u32(atom, 0)):
            raise ValueError("instruction permutation region contains a control op")

    if _relocation_sha256(relocations) != before_relocations_sha256:
        raise ValueError("instruction permutation relocation input hash changed")

    destination_by_source = {
        source: destination for destination, source in enumerate(order)
    }
    moved_relocations = []
    for offset, info, addend in relocations:
        if not 0 <= offset < len(current):
            raise ValueError("instruction permutation relocation is outside region")
        source = offset // 4
        within_atom = offset % 4
        destination = destination_by_source[source]
        moved_relocations.append((destination * 4 + within_atom, info, addend))
    moved_relocations.sort(key=lambda item: item[0])

    # The transform may change relocation offsets and ordering, never their
    # symbol/addend payload or their byte position within an instruction atom.
    before_payload = sorted(
        (offset % 4, info, addend) for offset, info, addend in relocations
    )
    after_payload = sorted(
        (offset % 4, info, addend)
        for offset, info, addend in moved_relocations
    )
    if before_payload != after_payload or len(moved_relocations) != len(relocations):
        raise ValueError("instruction permutation failed to preserve relocations")
    if _relocation_sha256(moved_relocations) != after_relocations_sha256:
        raise ValueError("instruction permutation relocation output hash changed")

    output = b"".join(atoms[source] for source in order)
    if _sha256(output) != after_sha256:
        raise ValueError("instruction permutation output hash changed")
    moved = sum(destination != source for destination, source in enumerate(order))
    return output, moved_relocations, moved


def copy_register_fields(current: bytes, target: bytes) -> tuple[bytes, int]:
    """Copy only PPC's four five-bit register slots from *target*.

    All other instruction bits must already agree. Complete function hashes in
    the caller make this a fail-closed allocator correction, not a fuzzy patch.
    """
    if len(current) != len(target) or len(current) % 4:
        raise ValueError("register-field functions must have equal aligned sizes")
    output = bytearray(current)
    changed = 0
    for offset in range(0, len(current), 4):
        word = _u32(current, offset)
        wanted = _u32(target, offset)
        difference = word ^ wanted
        if not difference:
            continue
        if difference & ~REGISTER_FIELD_MASK:
            raise ValueError(
                f"non-register instruction bits differ at +0x{offset:x}: "
                f"0x{difference:08x}"
            )
        recolored = (word & ~REGISTER_FIELD_MASK) | (wanted & REGISTER_FIELD_MASK)
        struct.pack_into(">I", output, offset, recolored)
        changed += 1
    if output != target:
        raise ValueError("register-field copy did not reproduce target bytes")
    return bytes(output), changed


def apply_patch(
    data: bytearray, patch: dict, target_data: bytes | None = None
) -> tuple[str, str, int]:
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
    permutation = patch.get("instruction_permutation")
    if permutation:
        if target_data is None:
            raise ValueError(f"{symbol.name}: target object is required")
        target_sections = _sections(target_data)
        target_symbol = _find_symbol(target_data, target_sections, symbol.name)
        if target_symbol.size != symbol.size:
            raise ValueError(
                f"{symbol.name}: target/current function size mismatch "
                f"({target_symbol.size} != {symbol.size})"
            )

        relative_start = _parse_int(permutation["start"])
        relative_end = _parse_int(permutation["end"])
        if (relative_start % 4 or relative_end % 4
                or not 0 <= relative_start < relative_end <= symbol.size):
            raise ValueError(
                f"{symbol.name}: invalid instruction permutation range"
            )

        relocation_sections = [
            section for section in sections
            if section.section_type == SHT_RELA
            and section.info == symbol.section_index
        ]
        if len(relocation_sections) != 1:
            raise ValueError(
                f"{symbol.name}: expected one relocation section for text, "
                f"found {len(relocation_sections)}"
            )
        relocation_section = relocation_sections[0]
        entry_size = relocation_section.entry_size or 12
        if entry_size != 12 or relocation_section.size % entry_size:
            raise ValueError(f"{symbol.name}: unsupported relocation layout")

        records = []
        for offset in range(
            relocation_section.offset,
            relocation_section.offset + relocation_section.size,
            entry_size,
        ):
            records.append(struct.unpack_from(">IIi", data, offset))

        section_region_start = symbol.value + relative_start
        section_region_end = symbol.value + relative_end
        region_records = [
            (offset - section_region_start, info, addend)
            for offset, info, addend in records
            if section_region_start <= offset < section_region_end
        ]
        region = bytes(data[start + relative_start:start + relative_end])
        permuted, moved_records, moved = permute_instruction_atoms(
            region,
            [_parse_int(index) for index in permutation["order"]],
            region_records,
            before_sha256=permutation["before_sha256"],
            after_sha256=permutation["after_sha256"],
            before_relocations_sha256=permutation["before_relocations_sha256"],
            after_relocations_sha256=permutation["after_relocations_sha256"],
        )
        data[start + relative_start:start + relative_end] = permuted

        outside_records = [
            record for record in records
            if not section_region_start <= record[0] < section_region_end
        ]
        records = outside_records + [
            (section_region_start + offset, info, addend)
            for offset, info, addend in moved_records
        ]
        records.sort(key=lambda item: item[0])
        if len(records) * entry_size != relocation_section.size:
            raise ValueError(f"{symbol.name}: relocation count changed")
        for index, record in enumerate(records):
            struct.pack_into(
                ">IIi", data,
                relocation_section.offset + index * entry_size,
                *record,
            )
        changed += moved

    if patch.get("copy_register_fields"):
        if target_data is None:
            raise ValueError(f"{symbol.name}: target object is required")
        target_sections = _sections(target_data)
        target_symbol = _find_symbol(target_data, target_sections, symbol.name)
        target_text = target_sections[target_symbol.section_index]
        target_start = target_text.offset + target_symbol.value
        target_end = target_start + target_symbol.size
        target_function = target_data[target_start:target_end]
        if _sha256(target_function) != patch["after_sha256"]:
            raise ValueError(f"{symbol.name}: target function hash changed")
        recolored, field_changes = copy_register_fields(
            bytes(data[start:end]), target_function
        )
        data[start:end] = recolored
        changed += field_changes

    for region in patch.get("recolors", []):
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

    seen_edits = set()
    for edit in patch.get("register_fields", []):
        relative = _parse_int(edit["at"])
        if relative % 4 or not 0 <= relative <= symbol.size - 4:
            raise ValueError(f"{symbol.name}: invalid register-field offset {edit}")
        if relative in seen_edits:
            raise ValueError(f"{symbol.name}: duplicate register-field edit {edit}")
        seen_edits.add(relative)
        offset = start + relative
        word = _u32(data, offset)
        recolored = word
        for raw_shift, raw_value in edit["set"].items():
            shift = int(raw_shift)
            value = _parse_int(raw_value)
            if shift not in {6, 11, 16, 21} or not 0 <= value < 32:
                raise ValueError(f"{symbol.name}: invalid register field {edit}")
            recolored = (recolored & ~(0x1F << shift)) | (value << shift)
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
    parser.add_argument("--target", type=Path,
                        help="extracted target object for register-field rules")
    args = parser.parse_args()

    config = json.loads(args.config.read_text(encoding="utf-8"))
    patches = config.get("units", {}).get(args.unit)
    if not patches:
        raise KeyError(f"no webfrank configuration for {args.unit!r}")

    data = bytearray(args.input.read_bytes())
    target_data = args.target.read_bytes() if args.target else None
    total = 0
    for patch in patches:
        _, _, changed = apply_patch(data, patch, target_data)
        total += changed
        print(
            f"WEBFRANK {patch['function']}: "
            f"adjusted {changed} instruction atoms/fields"
        )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    print(f"WEBFRANK {args.unit}: {total} instruction atoms/fields total")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
