#!/usr/bin/env python3
# Based on Melee's Frank script by Ethan Roseman (ethteck), Copyright 2021,
# later modified by EpochFlame. Distributed under the MIT License.
"""Merge CodeWarrior's profile schedule into a normal PowerPC ELF object.

This is a library-friendly implementation of Melee's ``tools/frank.py``.
The original was written by Ethan Roseman (ethteck), modified by EpochFlame,
and released under the MIT license.  Melee used it to reproduce the fixed
1.2.5 epilogue schedule before the direct 1.2.5n compiler patch was available.

The profile-patched compiler inserts ``bl; nop`` markers at return sites.  The
markers alter scheduling; Frank removes them, repairs return instructions and
known epilogue orderings, and places the resulting .text into the normal
object so that its symbols, relocations, and data sections remain authoritative.
"""

from __future__ import annotations

import argparse
import struct
from dataclasses import dataclass
from pathlib import Path


CODESIZE_MAGIC = b"\x00\x00\x00\x06\x00\x00\x00\x00\x00\x00\x00\x34"
BLR = b"\x4e\x80\x00\x20"
MTLR = b"\x7c\x08\x03\xa6"
PROFILE_MARKER = b"\x48\x00\x00\x01\x60\x00\x00\x00"
TEXT_OFFSET = 0x34

# Conditional and unconditional branches to LR that the profile compiler may
# expand.  Frank always preserves the normal compiler's instruction here.
BLR_SEQUENCES = (
    BLR,
    b"\x4d\x80\x00\x20", b"\x4d\x80\x00\x21",
    b"\x4c\x81\x00\x20", b"\x4c\x81\x00\x21",
    b"\x4d\x82\x00\x20", b"\x4d\x82\x00\x21",
    b"\x4c\x80\x00\x20", b"\x4c\x80\x00\x21",
    b"\x4d\x81\x00\x20", b"\x4d\x81\x00\x21",
    b"\x4c\x82\x00\x20", b"\x4c\x82\x00\x21",
    b"\x4d\x83\x00\x20", b"\x4d\x83\x00\x21",
    b"\x4c\x83\x00\x20", b"\x4c\x83\x00\x21",
)


@dataclass(frozen=True)
class MergeStats:
    profile_markers: int
    text_bytes_changed: int
    used_vanilla_fallback: bool


def _find_aligned(data: bytes | bytearray, sequence: bytes, start: int) -> int:
    """Find the next four-byte-aligned occurrence of *sequence*."""
    while True:
        found = data.find(sequence, start)
        if found == -1 or found % 4 == 0:
            return found
        start = found + 1


def _legacy_text_layout(obj: bytes) -> tuple[int, int]:
    magic = obj.index(CODESIZE_MAGIC)
    start = magic + len(CODESIZE_MAGIC)
    return TEXT_OFFSET, int.from_bytes(obj[start:start + 4], "big")


def _text_layout(obj: bytes) -> tuple[int, int] | None:
    """Return the unique executable ELF section's file layout.

    Real MWCC objects are ELF32 big-endian. The old Frank magic scan is kept
    only for synthetic/non-ELF fixtures; scanning an ELF payload can mistake
    ordinary constants for a section header (as AX.OBJ demonstrates).
    """
    if obj[:4] != b"\x7fELF":
        return _legacy_text_layout(obj)
    if len(obj) < 0x34 or obj[4:6] != b"\x01\x02":
        raise ValueError("Frank requires an ELF32 big-endian object")
    section_offset = struct.unpack_from(">I", obj, 0x20)[0]
    entry_size = struct.unpack_from(">H", obj, 0x2E)[0]
    section_count = struct.unpack_from(">H", obj, 0x30)[0]
    names_index = struct.unpack_from(">H", obj, 0x32)[0]
    if (entry_size < 40 or names_index >= section_count
            or section_offset + entry_size * section_count > len(obj)):
        raise ValueError("Frank found an invalid ELF section table")

    headers = []
    for index in range(section_count):
        at = section_offset + index * entry_size
        headers.append(struct.unpack_from(">10I", obj, at))
    names = headers[names_index]
    names_offset, names_size = names[4], names[5]
    if names_offset + names_size > len(obj):
        raise ValueError("Frank found an invalid ELF section-name table")

    executable = []
    for header in headers:
        name_at, section_type, flags, _, offset, size, *_ = header
        if section_type != 1 or flags & 0x6 != 0x6:
            continue
        if name_at >= names_size or offset % 4 or size % 4 or offset + size > len(obj):
            raise ValueError("Frank found an invalid executable section")
        name_start = names_offset + name_at
        name_end = obj.find(b"\0", name_start, names_offset + names_size)
        if name_end == -1:
            raise ValueError("Frank found an unterminated ELF section name")
        executable.append((obj[name_start:name_end].decode("ascii"), offset, size))
    if not executable:
        return None
    if len(executable) != 1:
        raise ValueError(f"Frank requires one executable section, found {executable}")
    _, offset, size = executable[0]
    return offset, size


def merge_objects(vanilla_obj: bytes, profile_obj: bytes) -> tuple[bytes, MergeStats]:
    """Return a Frank-merged object and diagnostics.

    ``vanilla_obj`` may come from any body compiler, not only vanilla 1.2.5,
    provided its stripped .text size and relocation layout agree with the
    profile object.  This is useful for testing GDL's configured 1.2.5n body
    together with the 1.2.5e profile scheduler.
    """
    try:
        vanilla_layout = _text_layout(vanilla_obj)
    except ValueError:
        if vanilla_obj[:4] == b"\x7fELF":
            raise
        return vanilla_obj, MergeStats(0, 0, False)
    if vanilla_layout is None:
        return vanilla_obj, MergeStats(0, 0, False)

    profile_layout = _text_layout(profile_obj)
    if profile_layout is None:
        raise ValueError("Frank profile object has no executable section")
    vanilla_offset, vanilla_size = vanilla_layout
    profile_offset, profile_size = profile_layout
    header = vanilla_obj[:vanilla_offset]
    footer = vanilla_obj[vanilla_offset + vanilla_size:]
    vanilla = bytearray(vanilla_obj[vanilla_offset:vanilla_offset + vanilla_size])
    profile = bytearray(profile_obj[profile_offset:profile_offset + profile_size])

    epilogues: list[int] = []
    marker_pos = 0
    shift = 0
    marker_count = 0

    while True:
        marker_pos = _find_aligned(profile, PROFILE_MARKER, marker_pos)
        if marker_pos == -1:
            break

        marker_count += 1
        vanilla_pos = marker_pos - shift
        shift += len(PROFILE_MARKER)

        if marker_pos >= 4 and marker_pos + 16 < profile_size \
                and vanilla_pos >= 4 and vanilla_pos + 8 < vanilla_size:
            epilogues.append(vanilla_pos)
            va = vanilla[vanilla_pos - 4:vanilla_pos]
            vb = vanilla[vanilla_pos:vanilla_pos + 4]
            vc = vanilla[vanilla_pos + 4:vanilla_pos + 8]
            pa = profile[marker_pos - 4:marker_pos]
            pb = profile[marker_pos + 8:marker_pos + 12]
            pc = profile[marker_pos + 12:marker_pos + 16]

            as_int = lambda insn: int.from_bytes(insn, "big")
            ra = lambda insn: (as_int(insn) >> 16) & 0x1F
            opcode_a = va[0] >> 2
            opcode_b = vb[0] >> 2
            opcode_c = vc[0] >> 2
            lwz = 0x80 >> 2
            lfs = 0xC0 >> 2
            addi = 0x38 >> 2
            lmw = 0xB8 >> 2
            fdivs = 0xEC >> 2

            if (opcode_a == lwz and opcode_b in (addi, lfs, fdivs)
                    and va == pb and vb == pa and vc == pc
                    and not (opcode_b == addi and ra(vb) != 0)
                    and opcode_c != addi):
                profile[marker_pos - 4:marker_pos + 16] = (
                    va + PROFILE_MARKER + vb + profile[marker_pos + 12:marker_pos + 16]
                )
            elif (opcode_b == lwz and opcode_c == lmw
                  and vb == pc and vc == pb):
                profile[marker_pos + 8:marker_pos + 16] = vb + vc

        marker_pos += len(PROFILE_MARKER)

    final = bytearray(profile.replace(PROFILE_MARKER, b""))
    fallback = len(final) != len(vanilla)
    if fallback:
        final = bytearray(vanilla)
        epilogues.clear()

    for sequence in BLR_SEQUENCES:
        position = 0
        while True:
            position = _find_aligned(vanilla, sequence, position)
            if position == -1:
                break
            final[position:position + 4] = vanilla[position:position + 4]
            position += len(sequence)

    # Move mtlr back next to its paired blr, retaining intervening scheduling.
    position = 0
    while True:
        mtlr_pos = _find_aligned(final, MTLR, position)
        if mtlr_pos == -1:
            break
        blr_pos = _find_aligned(final, BLR, mtlr_pos)
        if blr_pos == -1:
            break
        final[mtlr_pos:blr_pos + 4] = (
            final[mtlr_pos + 4:blr_pos] + final[mtlr_pos:mtlr_pos + 4]
            + final[blr_pos:blr_pos + 4]
        )
        position = blr_pos + 4

    # Repair the known profile LMW,LWZ,LFD* ordering when vanilla proves the
    # corresponding LWZ,LFD*,LMW schedule.
    lwz_opcode, lmw_opcode, lfd_opcode = 32, 46, 50
    for position in epilogues:
        if position + 8 >= len(final):
            continue
        if (final[position] >> 2 != lmw_opcode
                or final[position + 4] >> 2 != lwz_opcode
                or vanilla[position] >> 2 != lwz_opcode):
            continue
        lmw_bytes = final[position:position + 4]
        lwz_bytes = final[position + 4:position + 8]
        if vanilla[position:position + 4] != lwz_bytes:
            continue
        cursor = position + 4
        lfd_bytes = bytearray()
        while cursor < len(vanilla) and vanilla[cursor] >> 2 == lfd_opcode:
            lfd_bytes += vanilla[cursor:cursor + 4]
            cursor += 4
        if vanilla[cursor:cursor + 4] != lmw_bytes:
            continue
        if final[position + 8:position + 8 + len(lfd_bytes)] != lfd_bytes:
            continue
        final[position:cursor + 4] = lwz_bytes + lfd_bytes + lmw_bytes

    if len(final) != vanilla_size:
        raise ValueError("Frank produced a .text section with the wrong size")
    changed = sum(a != b for a, b in zip(vanilla, final))
    merged = header + bytes(final) + footer
    return merged, MergeStats(marker_count, changed, fallback)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("vanilla", type=Path)
    parser.add_argument("profile", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    merged, stats = merge_objects(args.vanilla.read_bytes(), args.profile.read_bytes())
    args.output.write_bytes(merged)
    if args.verbose:
        print(f"profile markers: {stats.profile_markers}")
        print(f"changed .text bytes: {stats.text_bytes_changed}")
        print(f"vanilla fallback: {stats.used_vanilla_fallback}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
