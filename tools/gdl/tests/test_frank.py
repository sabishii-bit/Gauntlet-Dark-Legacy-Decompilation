import sys
import struct
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from frank import (
    BLR,
    BLR_SEQUENCES,
    CODESIZE_MAGIC,
    MTLR,
    PROFILE_MARKER,
    TEXT_OFFSET,
    merge_objects,
)


NOP = bytes.fromhex("60000000")
LWZ = bytes.fromhex("80010004")
LI = bytes.fromhex("38600000")
LMW = bytes.fromhex("bba1000c")
LFD = bytes.fromhex("cbe10008")

# The historical Melee script contains duplicate entries; retain its literal
# table here so the differential fixture does not share Frank's deduplicated
# production constant.
MELEE_BLR_SEQUENCES = (
    BLR,
    bytes.fromhex("4d800020"), bytes.fromhex("4d800021"),
    bytes.fromhex("4c810020"), bytes.fromhex("4c810021"),
    bytes.fromhex("4d820020"), bytes.fromhex("4d820021"),
    bytes.fromhex("4c800020"), bytes.fromhex("4c800021"),
    bytes.fromhex("4d810020"), bytes.fromhex("4d810021"),
    bytes.fromhex("4c800020"), bytes.fromhex("4c800021"),
    bytes.fromhex("4c820020"), bytes.fromhex("4c820021"),
    bytes.fromhex("4c810020"), bytes.fromhex("4c810021"),
    bytes.fromhex("4d830020"), bytes.fromhex("4d830021"),
    bytes.fromhex("4c830020"), bytes.fromhex("4c830021"),
    bytes.fromhex("4d830020"), bytes.fromhex("4d830021"),
    bytes.fromhex("4c830020"), bytes.fromhex("4c830021"),
)


def object_with_text(text: bytes, footer: bytes = b"footer") -> bytes:
    header = bytearray(TEXT_OFFSET)
    header[:len(CODESIZE_MAGIC)] = CODESIZE_MAGIC
    size_at = len(CODESIZE_MAGIC)
    header[size_at:size_at + 4] = len(text).to_bytes(4, "big")
    return bytes(header) + text + footer


def elf_with_text(text: bytes, *, text_offset: int = 0x100, decoy: bool = False) -> bytes:
    names = b"\0.shstrtab\0.text\0"
    section_offset = 0x80
    names_offset = 0x60
    size = max(text_offset + len(text), section_offset + 3 * 40)
    obj = bytearray(size)
    obj[:7] = b"\x7fELF\x01\x02\x01"
    struct.pack_into(">I", obj, 0x20, section_offset)
    struct.pack_into(">HHH", obj, 0x2E, 40, 3, 1)
    obj[names_offset:names_offset + len(names)] = names
    if decoy:
        obj[0x40:0x40 + len(CODESIZE_MAGIC)] = CODESIZE_MAGIC
        struct.pack_into(">I", obj, 0x40 + len(CODESIZE_MAGIC), 0x12000001)
    struct.pack_into(">10I", obj, section_offset + 40,
                     1, 3, 0, 0, names_offset, len(names), 0, 0, 1, 0)
    struct.pack_into(">10I", obj, section_offset + 80,
                     11, 1, 6, 0, text_offset, len(text), 0, 0, 4, 0)
    obj[text_offset:text_offset + len(text)] = text
    return bytes(obj)


def _find_aligned(data: bytes, sequence: bytes, start: int) -> int:
    while True:
        found = data.find(sequence, start)
        if found == -1 or found % 4 == 0:
            return found
        start = found + 1


def melee_reference_merge(vanilla_obj: bytes, profile_obj: bytes) -> bytes:
    """Faithful fixture implementation of Melee tools/frank.py@96f654e."""
    size_at = vanilla_obj.index(CODESIZE_MAGIC) + len(CODESIZE_MAGIC)
    vanilla_size = int.from_bytes(vanilla_obj[size_at:size_at + 4], "big")
    profile_size_at = profile_obj.index(CODESIZE_MAGIC) + len(CODESIZE_MAGIC)
    profile_size = int.from_bytes(
        profile_obj[profile_size_at:profile_size_at + 4], "big"
    )
    header = vanilla_obj[:TEXT_OFFSET]
    footer = vanilla_obj[TEXT_OFFSET + vanilla_size:]
    vanilla = vanilla_obj[TEXT_OFFSET:TEXT_OFFSET + vanilla_size]
    profile = profile_obj[TEXT_OFFSET:TEXT_OFFSET + profile_size]

    epilogues = []
    marker_pos = 0
    shift = 0
    while (marker_pos := _find_aligned(profile, PROFILE_MARKER, marker_pos)) != -1:
        vanilla_pos = marker_pos - shift
        shift += len(PROFILE_MARKER)
        if (marker_pos >= 4 and marker_pos + 16 < profile_size
                and vanilla_pos >= 4 and vanilla_pos + 8 < vanilla_size):
            epilogues.append(vanilla_pos)
            va = vanilla[vanilla_pos - 4:vanilla_pos]
            vb = vanilla[vanilla_pos:vanilla_pos + 4]
            vc = vanilla[vanilla_pos + 4:vanilla_pos + 8]
            pa = profile[marker_pos - 4:marker_pos]
            pb = profile[marker_pos + 8:marker_pos + 12]
            pc = profile[marker_pos + 12:marker_pos + 16]
            opcode_a = va[0] >> 2
            opcode_b = vb[0] >> 2
            opcode_c = vc[0] >> 2
            ra = lambda instruction: (int.from_bytes(instruction, "big") >> 16) & 0x1F
            if (opcode_a == 0x80 >> 2
                    and opcode_b in (0x38 >> 2, 0xC0 >> 2, 0xEC >> 2)
                    and va == pb and vb == pa and vc == pc
                    and not (opcode_b == 0x38 >> 2 and ra(vb) != 0)
                    and opcode_c != 0x38 >> 2):
                profile = (
                    profile[:marker_pos - 4] + va + PROFILE_MARKER + vb
                    + profile[marker_pos + 12:]
                )
            elif (opcode_b == 0x80 >> 2 and opcode_c == 0xB8 >> 2
                  and vb == pc and vc == pb):
                profile = (
                    profile[:marker_pos + 8] + vb + vc
                    + profile[marker_pos + 16:]
                )
        marker_pos += len(PROFILE_MARKER)

    final = profile.replace(PROFILE_MARKER, b"")
    if len(final) != len(vanilla):
        final = vanilla
        epilogues.clear()

    for sequence in MELEE_BLR_SEQUENCES:
        position = 0
        while (position := _find_aligned(vanilla, sequence, position)) != -1:
            final = final[:position] + vanilla[position:position + 4] + final[position + 4:]
            position += len(sequence)

    position = 0
    while (mtlr_pos := _find_aligned(final, MTLR, position)) != -1:
        blr_pos = _find_aligned(final, BLR, mtlr_pos)
        if blr_pos == -1:
            break
        final = (
            final[:mtlr_pos] + final[mtlr_pos + 4:blr_pos]
            + final[mtlr_pos:mtlr_pos + 4] + final[blr_pos:]
        )
        position = blr_pos + 4

    for position in epilogues:
        if (final[position] >> 2 != 46 or final[position + 4] >> 2 != 32
                or vanilla[position] >> 2 != 32):
            continue
        lmw = final[position:position + 4]
        lwz = final[position + 4:position + 8]
        if vanilla[position:position + 4] != lwz:
            continue
        cursor = position + 4
        lfds = b""
        while vanilla[cursor] >> 2 == 50:
            lfds += vanilla[cursor:cursor + 4]
            cursor += 4
        if vanilla[cursor:cursor + 4] != lmw:
            continue
        if final[position + 8:position + 8 + len(lfds)] != lfds:
            continue
        final = final[:position] + lwz + lfds + lmw + final[cursor + 4:]

    if len(final) != vanilla_size:
        raise AssertionError("Melee reference produced the wrong code size")
    return header + final + footer


class FrankTests(unittest.TestCase):
    def test_identical_text_is_unchanged(self):
        text = bytes.fromhex("38600000 4e800020")
        vanilla = object_with_text(text)
        merged, stats = merge_objects(vanilla, object_with_text(text))
        self.assertEqual(merged, vanilla)
        self.assertEqual(stats.profile_markers, 0)
        self.assertEqual(stats.text_bytes_changed, 0)
        self.assertFalse(stats.used_vanilla_fallback)

    def test_profile_marker_is_removed_and_profile_schedule_is_used(self):
        first = bytes.fromhex("38600000")
        second = bytes.fromhex("38800000")
        vanilla = object_with_text(first + second)
        profile = object_with_text(PROFILE_MARKER + second + first)
        merged, stats = merge_objects(vanilla, profile)
        self.assertEqual(merged[TEXT_OFFSET:TEXT_OFFSET + 8], second + first)
        self.assertEqual(merged[TEXT_OFFSET + 8:], b"footer")
        self.assertEqual(stats.profile_markers, 1)
        self.assertEqual(stats.text_bytes_changed, 2)
        self.assertFalse(stats.used_vanilla_fallback)

    def test_size_mismatch_falls_back_to_vanilla_text(self):
        text = bytes.fromhex("38600000 38800000")
        vanilla = object_with_text(text)
        profile = object_with_text(PROFILE_MARKER + text + bytes.fromhex("38a00000"))
        merged, stats = merge_objects(vanilla, profile)
        self.assertEqual(merged, vanilla)
        self.assertTrue(stats.used_vanilla_fallback)

    def test_first_peephole_swap_is_repaired(self):
        vanilla_text = LWZ + LI + NOP + NOP
        profile_text = LI + PROFILE_MARKER + LWZ + NOP + NOP
        vanilla = object_with_text(vanilla_text)
        merged, stats = merge_objects(vanilla, object_with_text(profile_text))
        self.assertEqual(merged, vanilla)
        self.assertEqual(stats.profile_markers, 1)
        self.assertFalse(stats.used_vanilla_fallback)

    def test_post_marker_lwz_lmw_swap_is_repaired(self):
        vanilla_text = NOP + LWZ + LMW + NOP
        profile_text = NOP + PROFILE_MARKER + LMW + LWZ + NOP
        vanilla = object_with_text(vanilla_text)
        merged, stats = merge_objects(vanilla, object_with_text(profile_text))
        self.assertEqual(merged, vanilla)
        self.assertEqual(stats.profile_markers, 1)

    def test_every_melee_blr_family_instruction_is_restored(self):
        self.assertEqual(set(BLR_SEQUENCES), set(MELEE_BLR_SEQUENCES))
        replacement = bytes.fromhex("38800000")
        for sequence in set(MELEE_BLR_SEQUENCES):
            with self.subTest(sequence=sequence.hex()):
                vanilla = object_with_text(NOP + sequence + NOP)
                profile = object_with_text(NOP + replacement + NOP)
                merged, _ = merge_objects(vanilla, profile)
                self.assertEqual(merged, vanilla)

    def test_conditional_return_expansion_falls_back_to_vanilla(self):
        conditional_blr = bytes.fromhex("4c820020")
        vanilla = object_with_text(conditional_blr + NOP)
        expanded = bytes.fromhex("40820008") + PROFILE_MARKER + BLR + NOP
        merged, stats = merge_objects(vanilla, object_with_text(expanded))
        self.assertEqual(merged, vanilla)
        self.assertTrue(stats.used_vanilla_fallback)

    def test_mtlr_is_reunified_with_blr(self):
        vanilla = object_with_text(LI + MTLR + BLR)
        profile = object_with_text(MTLR + LI + BLR)
        merged, stats = merge_objects(vanilla, profile)
        self.assertEqual(merged, vanilla)
        self.assertFalse(stats.used_vanilla_fallback)

    def test_lmw_lwz_lfd_family_is_repaired(self):
        vanilla_text = NOP + LWZ + LFD + LFD + LMW + NOP
        profile_text = NOP + PROFILE_MARKER + LMW + LWZ + LFD + LFD + NOP
        vanilla = object_with_text(vanilla_text)
        merged, stats = merge_objects(vanilla, object_with_text(profile_text))
        self.assertEqual(merged, vanilla)
        self.assertEqual(stats.profile_markers, 1)

    def test_whole_object_matches_authoritative_melee_reference(self):
        footer = b"\xde\xadrelocations-symbols-and-data\x00\x01"
        vanilla_text = LWZ + LI + NOP + NOP + LI + MTLR + BLR
        profile_text = LI + PROFILE_MARKER + LWZ + NOP + NOP + MTLR + LI + BLR
        vanilla = object_with_text(vanilla_text, footer)
        profile = object_with_text(profile_text, b"profile-footer-must-not-survive")
        merged, _ = merge_objects(vanilla, profile)
        reference = melee_reference_merge(vanilla, profile)
        self.assertEqual(merged, reference)
        self.assertEqual(merged[-len(footer):], footer)

    def test_elf_layout_ignores_magic_decoy_and_per_object_offsets(self):
        first = bytes.fromhex("38600000")
        second = bytes.fromhex("38800000")
        vanilla = elf_with_text(first + second, text_offset=0x100, decoy=True)
        profile = elf_with_text(
            PROFILE_MARKER + second + first, text_offset=0x120, decoy=True
        )
        merged, stats = merge_objects(vanilla, profile)
        self.assertEqual(merged[0x100:0x108], second + first)
        self.assertEqual(len(merged), len(vanilla))
        self.assertEqual(stats.profile_markers, 1)
        self.assertFalse(stats.used_vanilla_fallback)


if __name__ == "__main__":
    unittest.main()
