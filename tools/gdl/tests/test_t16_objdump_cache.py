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
    """A CompletedProcess double.

    `returncode`/`stderr` are part of the contract since run-52 item 9:
    `objdump()` is fail-closed and raises `ObjdumpFailed` on a nonzero
    exit, so a double that models only `stdout` describes a subprocess
    interface that no longer exists. `returncode` defaults to 0 (the
    memo tests are about caching, not failure) and is settable per
    instance for the failure cases below.
    """

    def __init__(self, returncode=0, stderr=""):
        self.calls = []
        self.returncode = returncode
        self.stderr = stderr

    def __call__(self, argv, **kwargs):
        self.calls.append(tuple(argv))
        outer = self

        class Result:
            stdout = "dump-%d" % len(outer.calls)
            returncode = outer.returncode
            stderr = outer.stderr
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


DUMP = """
SYMBOL TABLE:
00000000 l    d  .text\t00000000 .text

Disassembly of section .text:

00000000 <foo>:
       0:\t7c 08 02 a6 \t%s
       4:\t4e 80 00 20 \tblr
"""


class ParseMemo(unittest.TestCase):
    """Run-59 item 6: the PARSED table is memoized too, on the same identity.

    The objdump memo above already made a repeat `parse` skip both
    subprocesses, and `parse` still re-ran the whole two-pass regex
    reduction on the cached text. Measured with
    build/t3_scratch/t3_parse_repro.py at 2651955ed on game/enemy/enemy:
    composing the TU relocation screen per function made 168 `parse` calls
    costing 8.744 s; after the memo the same 168 calls cost 0.039 s.

    The miss side is again what matters: a stale parse table is a false
    MATCH describing a pre-build object, so the rebuild case asserts the
    NEW instruction text comes back, not merely that the parser re-ran.
    """

    def setUp(self):
        fndiff._PARSE_CACHE.clear()
        fndiff._OBJDUMP_CACHE.clear()
        self.real_objdump = fndiff.objdump
        self.real_parse_uncached = fndiff._parse_uncached
        self.mnemonic = "mflr r0"
        self.parses = []

        def fake_objdump(objfile, *flags):
            return DUMP % self.mnemonic

        def counting_parse_uncached(objfile):
            self.parses.append(str(objfile))
            return self.real_parse_uncached(objfile)

        fndiff.objdump = fake_objdump
        fndiff._parse_uncached = counting_parse_uncached
        self.tmp = tempfile.TemporaryDirectory()
        self.path = Path(self.tmp.name) / "a.o"
        self.path.write_bytes(b"\x00" * 16)

    def tearDown(self):
        fndiff.objdump = self.real_objdump
        fndiff._parse_uncached = self.real_parse_uncached
        fndiff._PARSE_CACHE.clear()
        fndiff._OBJDUMP_CACHE.clear()
        self.tmp.cleanup()

    def test_a_repeat_parse_is_served_from_the_memo(self):
        first = fndiff.parse(self.path)
        second = fndiff.parse(self.path)
        self.assertEqual(first, second)
        self.assertEqual(first["foo"], ["mflr r0", "blr"])
        self.assertEqual(len(self.parses), 1)

    def test_a_REBUILT_object_misses_and_reports_the_NEW_body(self):
        """The stale-cache false MATCH this memo must never produce."""
        self.assertEqual(fndiff.parse(self.path)["foo"], ["mflr r0", "blr"])
        time.sleep(0.01)
        self.mnemonic = "mfctr r0"
        self.path.write_bytes(b"\x01" * 32)
        self.assertEqual(fndiff.parse(self.path)["foo"], ["mfctr r0", "blr"])
        self.assertEqual(len(self.parses), 2)

    def test_a_size_change_alone_misses(self):
        import os
        fndiff.parse(self.path)
        stat = self.path.stat()
        self.mnemonic = "mfctr r0"
        self.path.write_bytes(b"\x00" * 24)
        os.utime(self.path, ns=(stat.st_atime_ns, stat.st_mtime_ns))
        self.assertEqual(fndiff.parse(self.path)["foo"], ["mfctr r0", "blr"])
        self.assertEqual(len(self.parses), 2)

    def test_a_different_path_is_a_different_entry(self):
        other = Path(self.tmp.name) / "b.o"
        other.write_bytes(b"\x00" * 16)
        fndiff.parse(self.path)
        fndiff.parse(other)
        self.assertEqual(len(self.parses), 2)

    def test_a_caller_that_edits_its_table_cannot_poison_the_next_reader(self):
        table = fndiff.parse(self.path)
        table["foo"].append("SPURIOUS")
        del table["foo"]
        table["injected"] = []
        fresh = fndiff.parse(self.path)
        self.assertEqual(fresh["foo"], ["mflr r0", "blr"])
        self.assertNotIn("injected", fresh)

    def test_the_parse_memo_is_bounded(self):
        source = (REPO / "tools" / "gdl" / "fndiff.py").read_text(
            encoding="utf-8")
        head = source[source.index("_PARSE_CACHE[key] = table") - 400:
                      source.index("_PARSE_CACHE[key] = table")]
        self.assertIn("_PARSE_CACHE.clear()", head)
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

    def test_the_parse_memo_returns_what_the_parser_returns(self):
        obj = REPO / "build" / "GUNE5D" / "src" / "zlib" / "inflate.o"
        if not obj.exists() or not Path(fndiff.OBJDUMP).exists():
            self.skipTest("zlib/inflate object or objdump missing")
        fndiff._PARSE_CACHE.clear()
        direct = fndiff._parse_uncached(obj)
        self.assertTrue(direct)
        self.assertEqual(fndiff.parse(obj), direct)
        self.assertEqual(fndiff.parse(obj), direct)


if __name__ == "__main__":
    unittest.main()
