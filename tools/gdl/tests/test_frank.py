import sys
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


if __name__ == "__main__":
    unittest.main()
