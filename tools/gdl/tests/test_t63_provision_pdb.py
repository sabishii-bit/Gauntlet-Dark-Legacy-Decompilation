"""provision_worktree.py --pdb-from: copy the gitignored Xbox PDB (item 5).

WHY IT EXISTS. `research/xbox_symbols/shell3D.pdb` is gitignored
(`.gitignore`: `*shell3D.pdb`) while the rest of that directory is tracked,
so a freshly created worktree has every header and `functions_by_module.txt`
but not the PDB, and `pdb20_dump.py`, `pdb_types.py` and `pdb_globals.py`
are unusable there until somebody copies it by hand. Three run-63 lanes
each repeated that step.

TWO-SIDED. Positive: a copy into a worktree that lacks it, and a second run
over an identical file that reports rather than re-copying. Negative: a
source checkout without the PDB, a destination whose bytes DIFFER (refused,
because a research input is not a build artifact and two lanes silently
disagreeing about what the PDB says is worse than an error), a flag with no
value, and the source and destination being one file.

NOTHING HERE PROVISIONS. Every test drives `copy_pdb` / `take_valued_flag`
against temporary directories: `main()` copies orig/, runs configure.py and
runs ninja, which is not a unit test's business.
"""
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

import provision_worktree as pw  # noqa: E402


class CopyPdb(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.source = Path(self.tmp.name) / "source-checkout"
        self.dest = Path(self.tmp.name) / "worker-worktree"
        (self.source / pw.PDB_RELATIVE).parent.mkdir(parents=True)
        (self.dest / "research").mkdir(parents=True)

    def write_source(self, payload=b"MSF\x00fake pdb payload"):
        (self.source / pw.PDB_RELATIVE).write_bytes(payload)

    def test_it_copies_the_pdb_and_says_how_many_bytes(self):
        self.write_source()
        message = pw.copy_pdb(self.source, self.dest)
        self.assertTrue((self.dest / pw.PDB_RELATIVE).is_file())
        self.assertEqual((self.dest / pw.PDB_RELATIVE).read_bytes(),
                         (self.source / pw.PDB_RELATIVE).read_bytes())
        self.assertIn("copied", message)
        self.assertIn("bytes", message)

    def test_it_creates_the_research_subdirectory_when_absent(self):
        self.write_source()
        empty = Path(self.tmp.name) / "bare-worktree"
        empty.mkdir()
        pw.copy_pdb(self.source, empty)
        self.assertTrue((empty / pw.PDB_RELATIVE).is_file())

    def test_a_second_run_over_identical_bytes_reports_and_does_nothing(self):
        self.write_source()
        pw.copy_pdb(self.source, self.dest)
        stamp = (self.dest / pw.PDB_RELATIVE).stat().st_mtime_ns
        message = pw.copy_pdb(self.source, self.dest)
        self.assertIn("already present and identical", message)
        self.assertEqual((self.dest / pw.PDB_RELATIVE).stat().st_mtime_ns,
                         stamp)

    def test_a_DIFFERENT_existing_pdb_is_REFUSED_not_overwritten(self):
        """A research input is not a build artifact."""
        self.write_source(b"MSF\x00version A")
        target = self.dest / pw.PDB_RELATIVE
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(b"MSF\x00version B, longer")
        with self.assertRaises(SystemExit):
            pw.copy_pdb(self.source, self.dest)
        self.assertEqual(target.read_bytes(), b"MSF\x00version B, longer")

    def test_same_size_but_different_bytes_is_still_REFUSED(self):
        """The size check is a cheap gate, not the comparison."""
        self.write_source(b"MSF\x00AAAAAAAA")
        target = self.dest / pw.PDB_RELATIVE
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(b"MSF\x00BBBBBBBB")
        with self.assertRaises(SystemExit):
            pw.copy_pdb(self.source, self.dest)

    def test_a_source_without_the_pdb_REFUSES_with_a_message(self):
        with self.assertRaises(SystemExit):
            pw.copy_pdb(self.source, self.dest)
        self.assertFalse((self.dest / pw.PDB_RELATIVE).exists())

    def test_copying_a_checkout_onto_itself_is_a_no_op_not_a_truncation(self):
        self.write_source()
        message = pw.copy_pdb(self.source, self.source)
        self.assertIn("already this checkout's own file", message)
        self.assertTrue((self.source / pw.PDB_RELATIVE).read_bytes())


class FlagParsing(unittest.TestCase):
    def test_the_flag_and_its_value_are_removed_from_argv(self):
        rest, value = pw.take_valued_flag(
            ["--pdb-from", "W:/Repositories/Main", "--resplit"], "--pdb-from")
        self.assertEqual(rest, ["--resplit"])
        self.assertEqual(value, "W:/Repositories/Main")

    def test_absence_leaves_argv_untouched(self):
        rest, value = pw.take_valued_flag(["path", "branch"], "--pdb-from")
        self.assertEqual(rest, ["path", "branch"])
        self.assertIsNone(value)

    def test_the_value_survives_next_to_the_two_argument_form(self):
        rest, value = pw.take_valued_flag(
            ["W:/wt", "claude/x", "--pdb-from", "W:/Main"], "--pdb-from")
        self.assertEqual(rest, ["W:/wt", "claude/x"])
        self.assertEqual(value, "W:/Main")

    def test_a_missing_value_REFUSES_rather_than_eating_the_next_flag(self):
        with self.assertRaises(SystemExit):
            pw.take_valued_flag(["--pdb-from", "--resplit"], "--pdb-from")

    def test_a_trailing_flag_with_no_value_REFUSES(self):
        with self.assertRaises(SystemExit):
            pw.take_valued_flag(["--pdb-from"], "--pdb-from")


class Sha1Helper(unittest.TestCase):
    def test_it_hashes_a_file_larger_than_one_chunk(self):
        import hashlib
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "big.bin"
            payload = bytes(range(256)) * (1 << 13)      # 2 MiB
            path.write_bytes(payload)
            self.assertEqual(pw._sha1(path),
                             hashlib.sha1(payload).hexdigest())


class Documentation(unittest.TestCase):
    def test_the_help_text_documents_the_flag(self):
        self.assertIn("--pdb-from <checkout>", pw.__doc__)
        self.assertIn("GITIGNORED", pw.__doc__)


if __name__ == "__main__":
    unittest.main()
