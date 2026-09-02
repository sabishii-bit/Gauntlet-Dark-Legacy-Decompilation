"""pooldump --ours: the OUR-SIDE constant pool inspector (run-38 item 8).

Every pool inspector in this project resolved names through
config/GUNE5D/symbols.txt, which describes the RETAIL image only. Our
unlinked object names its pool entries `@674`; those names exist in no
symbol table but our own and have no virtual address at all, so
`pooldump.py --sym @674` answered `symbol @674 not found` and the worker
guessed. That guess was a wrong diagnosis two runs running.
"""

import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import pooldump  # noqa: E402
from pooldump import describe, our_object  # noqa: E402


class DescribeTests(unittest.TestCase):
    """Every interpretation that FITS the width, and no others."""

    def test_a_4_byte_entry_reads_as_f32_and_u32(self):
        out = describe(struct.pack(">f", 0.5))
        self.assertIn("f32=0.5", out)
        self.assertIn("u32=0x3F000000", out)
        self.assertNotIn("f64=", out)

    def test_an_8_byte_entry_also_reads_as_f64(self):
        """The MWCC float-to-int magic double, as it sits in our pool."""
        out = describe(bytes.fromhex("4330000080000000"))
        self.assertIn("f64=4503601774854144.0", out)

    def test_the_raw_hex_is_always_first(self):
        self.assertTrue(describe(b"\x3f\x80\x00\x00").startswith("3f800000"))

    def test_a_real_string_gets_a_preview(self):
        out = describe(b"Bad window id.\n\x00")
        self.assertIn('str="Bad window id.\\n"', out)

    def test_a_float_is_NOT_decorated_with_a_string_preview(self):
        """`str="?..."` on every float is noise standing exactly where a
        real string preview would go."""
        self.assertNotIn("str=", describe(struct.pack(">f", 1.0)))

    def test_a_long_string_is_truncated_and_says_so(self):
        out = describe(b"x" * 60 + b"\x00")
        self.assertIn("...", out)

    def test_a_2_byte_entry_reads_as_hex_only(self):
        self.assertEqual(describe(b"\x12\x34"), "1234")


class OurObjectTests(unittest.TestCase):
    """The pinned-TU trap: the plain src/ object is POST-rewrite, so its
    pool is the postprocessor's view of the question, not the
    compiler's."""

    def test_it_accepts_every_unit_spelling(self):
        for spelling in ("game/pb/pb_window", "src/game/pb/pb_window.c",
                         "game\\pb\\pb_window.c", "/game/pb/pb_window"):
            path, _note = our_object(spelling)
            self.assertTrue(path.as_posix().endswith("pb_window.o"), spelling)

    def test_it_prefers_the_pre_postprocess_body_when_one_exists(self):
        path, note = our_object("game/pb/pb_window")
        if ".postprocess" not in path.as_posix():
            self.skipTest("no body object in this worktree")
        self.assertIn("pre-webfrank", note)

    def test_it_reads_from_our_build_tree_not_the_retail_dol(self):
        path, _note = our_object("game/x/y")
        self.assertIn("build/GUNE5D/src", path.as_posix())
        self.assertNotEqual(path, pooldump.DOL)


if __name__ == "__main__":
    unittest.main()
