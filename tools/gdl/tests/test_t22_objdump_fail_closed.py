"""A dump that did not happen must not read as an empty one (run 52 item 9).

`fndiff.objdump` returned `subprocess.run(...).stdout` unconditionally, so
a missing object or a flag this binutils does not know produced `''`, and
every reader built on it returned an EMPTY result indistinguishable from a
genuine "nothing here". Reproduced at e7b394dc6 against
`build/GUNE5D/src/game/enemy/NOSUCH.o`:

    compiler_private_aliases -> 0   <-- EMPTY, reads as CLEAN
    relocation_symbols       -> 0   <-- EMPTY, reads as CLEAN
    raw_signature            -> 0   <-- EMPTY, reads as CLEAN
    parse                    -> 0   <-- EMPTY, reads as CLEAN

all four IMPORTABLE-CORE, all four returning exactly what a byte-clean
object returns. That is claim.law.T21_a-measurement-that-did-not-happen-
must-not-parse-as-a-measurement-of-zero.20260904.v1 sitting under every
score in the project.

NOTE ON THE ORDER'S WORDING: run-52's item 9 states the observation as
"fndiff.objdump raises on missing flags instead of returning ''". Measured,
it was the exact opposite — it RETURNED '' and raised nothing. The defect
is real; the direction in the item is inverted.

TWO-SIDED CALIBRATION for the return-code trigger, at e7b394dc6:
  positive -- missing object, unrecognized option, and both together all
              exit 1 with empty stdout;
  negative -- 641 objects in build/GUNE5D/{src,obj} x the five flag sets
              this project uses (-t, -r, "-d -r -z --no-show-raw-insn",
              -h, -s) = 3,205 real dumps: ZERO exit nonzero, ZERO come
              back empty at exit 0.
"""

import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import fndiff  # noqa: E402


class FailClosedTests(unittest.TestCase):
    """Driven through the real objdump binary; skipped without it."""

    @classmethod
    def setUpClass(cls):
        if not Path(fndiff.OBJDUMP).exists():
            raise unittest.SkipTest("binutils objdump not provisioned")
        cls.good = REPO / "build" / "GUNE5D" / "src" / "zlib" / "inflate.o"
        if not cls.good.exists():
            raise unittest.SkipTest("zlib/inflate object missing")

    def setUp(self):
        fndiff._OBJDUMP_CACHE.clear()

    def missing(self):
        return Path(tempfile.gettempdir()) / "t22_no_such_object.o"

    def test_a_missing_object_raises_instead_of_dumping_nothing(self):
        with self.assertRaises(fndiff.ObjdumpFailed) as caught:
            fndiff.objdump(self.missing(), "-t")
        message = str(caught.exception)
        self.assertIn("objdump exited 1", message)
        self.assertIn("No such file", message)
        self.assertIn("would read as a CLEAN object", message)

    def test_an_unrecognized_flag_raises(self):
        with self.assertRaises(fndiff.ObjdumpFailed) as caught:
            fndiff.objdump(self.good, "--not-a-real-flag")
        self.assertIn("unrecognized option", str(caught.exception))

    def test_the_importable_core_readers_no_longer_fabricate_a_clean_sheet(self):
        gone = self.missing()
        for name in ("compiler_private_aliases", "relocation_symbols",
                     "raw_signature", "parse"):
            with self.subTest(reader=name):
                with self.assertRaises(fndiff.ObjdumpFailed):
                    getattr(fndiff, name)(gone)

    def test_a_real_object_still_dumps(self):
        self.assertIn("SYMBOL TABLE", fndiff.objdump(self.good, "-t"))
        self.assertGreater(len(fndiff.parse(self.good)), 0)

    def test_a_failed_dump_is_never_cached(self):
        gone = self.missing()
        for _ in range(2):
            with self.assertRaises(fndiff.ObjdumpFailed):
                fndiff.objdump(gone, "-t")
        self.assertEqual(fndiff._OBJDUMP_CACHE, {})

    def test_it_is_an_Exception_so_fail_soft_guards_still_work(self):
        # AGENTS.md discipline 20: a SystemExit here would escape every
        # `except Exception` guard in the tree and kill the process.
        self.assertTrue(issubclass(fndiff.ObjdumpFailed, Exception))
        self.assertFalse(issubclass(fndiff.ObjdumpFailed, SystemExit))
        try:
            fndiff.objdump(self.missing(), "-t")
        except Exception:
            pass
        else:
            self.fail("the blanket guard did not see it")


if __name__ == "__main__":
    unittest.main()
