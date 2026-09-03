#!/usr/bin/env python3
"""T16 run-46 item 5: the shared objdump memo, and the staleness it must not have.

Two-sided: the hits that must happen are tested against the misses that MUST
happen. The miss side is the one that matters — probe.py builds an object and
then reads it in the same process, so a path-keyed memo would serve it the
pre-build disassembly, and a green gate describing bytes that no longer exist
is the worst failure mode this project has.

Measured at 05b3e534a over the first 40 objects of build/GUNE5D/src:
fndiff.parse cost 0.66s for a first pass and 0.52s for an identical second
pass in the same process; after the memo the second pass is 0.07s (7.4x).
The premise the item inherited ("6 image passes at 2-4 min each") did NOT
reproduce: the seven heaviest passes in the tree measured 0.4s to 13.2s
(t15_operand_provenance 13.2, webfrank_audit 11.1, lowmatch 6.4, nearmiss
6.0/6.2/6.5, t15_whoemits 3.1, mt_region_census 0.4), which is why no
on-disk cross-process cache was built.
"""

import sys
import tempfile
import time
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import fndiff  # noqa: E402


class FakeRun:
    def __init__(self):
        self.calls = []

    def __call__(self, argv, **kwargs):
        self.calls.append(tuple(argv))

        class Result:
            stdout = "dump-%d" % len(self.calls)
        return Result()


class ObjdumpMemo(unittest.TestCase):
    def setUp(self):
        fndiff._OBJDUMP_CACHE.clear()
        self.real = fndiff.subprocess.run
        self.fake = FakeRun()
        fndiff.subprocess.run = self.fake
        self.tmp = tempfile.TemporaryDirectory()
        self.path = Path(self.tmp.name) / "a.o"
        self.path.write_bytes(b"\x00" * 16)

    def tearDown(self):
        fndiff.subprocess.run = self.real
        fndiff._OBJDUMP_CACHE.clear()
        self.tmp.cleanup()

    def test_a_repeat_read_is_served_from_the_memo(self):
        first = fndiff.objdump(self.path, "-dr")
        second = fndiff.objdump(self.path, "-dr")
        self.assertEqual(first, second)
        self.assertEqual(len(self.fake.calls), 1)

    def test_different_flags_are_different_entries(self):
        fndiff.objdump(self.path, "-dr")
        fndiff.objdump(self.path, "-t")
        self.assertEqual(len(self.fake.calls), 2)

    def test_a_REBUILT_object_misses(self):
        """The staleness case: same path, new bytes, same process."""
        fndiff.objdump(self.path, "-dr")
        time.sleep(0.01)
        self.path.write_bytes(b"\x01" * 32)
        fndiff.objdump(self.path, "-dr")
        self.assertEqual(len(self.fake.calls), 2)

    def test_a_size_change_alone_misses(self):
        fndiff.objdump(self.path, "-dr")
        stat = self.path.stat()
        self.path.write_bytes(b"\x00" * 24)
        import os
        os.utime(self.path, ns=(stat.st_atime_ns, stat.st_mtime_ns))
        fndiff.objdump(self.path, "-dr")
        self.assertEqual(len(self.fake.calls), 2)

    def test_a_missing_file_is_never_cached(self):
        gone = Path(self.tmp.name) / "nope.o"
        fndiff.objdump(gone, "-dr")
        fndiff.objdump(gone, "-dr")
        self.assertEqual(len(self.fake.calls), 2)

    def test_the_memo_is_bounded(self):
        self.assertIn("4096", (REPO / "tools" / "gdl" / "fndiff.py")
                      .read_text(encoding="utf-8"))


class SectionCacheIdentity(unittest.TestCase):
    def test_the_section_memo_keys_on_the_files_identity_too(self):
        text = (REPO / "tools" / "gdl" / "fndiff.py").read_text(
            encoding="utf-8")
        head = text[text.index("_SECTION_CACHE.get(key)") - 800:
                    text.index("_SECTION_CACHE.get(key)")]
        self.assertIn("st_mtime_ns", head)


class LiveEquivalence(unittest.TestCase):
    def test_the_memo_returns_what_objdump_returns(self):
        obj = REPO / "build" / "GUNE5D" / "src" / "zlib" / "inflate.o"
        if not obj.exists() or not Path(fndiff.OBJDUMP).exists():
            self.skipTest("zlib/inflate object or objdump missing")
        fndiff._OBJDUMP_CACHE.clear()
        direct = fndiff.subprocess.run(
            [str(fndiff.OBJDUMP), "-t", str(obj)],
            capture_output=True, text=True).stdout
        self.assertEqual(fndiff.objdump(obj, "-t"), direct)
        self.assertEqual(fndiff.objdump(obj, "-t"), direct)


if __name__ == "__main__":
    unittest.main()
