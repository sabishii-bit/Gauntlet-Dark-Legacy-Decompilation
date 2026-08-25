import sys
import struct
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from frank import CODESIZE_MAGIC, PROFILE_MARKER, TEXT_OFFSET, merge_objects


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
