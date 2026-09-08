"""objneutral: is this edit neutral against a BANKED object? (item 3e)

THE GAP. Every comparator in the repository scores our object against the
TARGET -- fndiff, probe, defake_gate, datadiff, wf_word_diff. A byte-identity
campaign asks the other question: did this source change emit the same object
as before? A de-fakematch conversion, a shared-header edit gated across four
consumers, a literal recovery -- each claims neutrality, and "the fuzzy score
did not move" is not a proof of it. objneutral compares per-function .text
bytes and positional relocation tuples of the CURRENT object against a BANKED
one.

THE ONE EQUIVALENCE it grants: our compiler numbers anonymous pool entries
(`@NN`) per object, so an unrelated edit renumbers them without changing what
any instruction reads. Two relocations at the same offset, of the same type,
with the same addend, whose symbols are BOTH anonymous, are RENUMBERED.

THE NEGATIVE SIDE, and the reason the rule is written that narrowly: an
anonymous symbol paired with a NAMED one is a CHANGE. That pairing is exactly
what a literal recovery produces, and calling it renumbering would let the
tool certify the one edit class it exists to measure.

Calibrated live at 0048a0452 on game/enemy/critter's object: 88 functions,
55240 .text bytes, 1419 relocations, of which 166 name one of 43 distinct
anonymous pool entries.
"""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(ROOT))

from tools.gdl import objneutral  # noqa: E402


def fn(body=b"\x00\x00\x00\x01", relocs=(), size=None):
    return {"start": 0, "size": len(body) if size is None else size,
            "bytes": body, "relocs": list(relocs)}


class AnonymousRule(unittest.TestCase):
    def test_only_at_number_names_are_anonymous(self):
        for name in ("@131", "@1", "@99999"):
            self.assertTrue(objneutral.is_anonymous(name), name)
        for name in ("lbl_80346490", "gPlayers", "@", "@12a", "x@13",
                     "@13+0x4", "", "sFlags"):
            self.assertFalse(objneutral.is_anonymous(name), name)

    def test_a_pool_renumber_is_not_a_change(self):
        changed, renumbered = objneutral.compare_relocs(
            [(0x10, 9, "@131", 0)], [(0x10, 9, "@642", 0)])
        self.assertEqual(changed, [])
        self.assertEqual(renumbered[0]["before"], "@131")

    def test_an_anonymous_to_named_pair_is_a_change_not_renumbering(self):
        changed, renumbered = objneutral.compare_relocs(
            [(0x10, 9, "@131", 0)], [(0x10, 9, "lbl_80346490", 0)])
        self.assertEqual(renumbered, [])
        self.assertEqual(len(changed), 1)
        self.assertIn("named symbol", changed[0]["reason"])

    def test_a_different_addend_is_a_change_even_between_anonymous_names(self):
        changed, renumbered = objneutral.compare_relocs(
            [(0x10, 9, "@131", 0)], [(0x10, 9, "@642", 4)])
        self.assertEqual(renumbered, [])
        self.assertEqual(len(changed), 1)

    def test_a_different_type_or_offset_is_a_change(self):
        for after in [(0x10, 10, "@642", 0), (0x14, 9, "@642", 0)]:
            with self.subTest(after=after):
                changed, renumbered = objneutral.compare_relocs(
                    [(0x10, 9, "@131", 0)], [after])
                self.assertEqual(renumbered, [])
                self.assertEqual(len(changed), 1)

    def test_a_count_difference_is_reported_as_such(self):
        changed, renumbered = objneutral.compare_relocs(
            [(0x10, 9, "@131", 0)], [])
        self.assertEqual(renumbered, [])
        self.assertIn("COUNT", changed[0]["reason"])


class Verdicts(unittest.TestCase):
    def test_identical_functions_are_identical(self):
        rows = objneutral.compare_functions({"f": fn()}, {"f": fn()})
        self.assertEqual(rows[0]["verdict"], "IDENTICAL")

    def test_equal_bytes_with_renumbered_pool_are_renumbered(self):
        before = {"f": fn(relocs=[(4, 9, "@131", 0)])}
        after = {"f": fn(relocs=[(4, 9, "@642", 0)])}
        row = objneutral.compare_functions(before, after)[0]
        self.assertEqual(row["verdict"], "RENUMBERED")
        self.assertEqual(len(row["renumbered_relocations"]), 1)

    def test_differing_bytes_name_the_first_word(self):
        before = {"f": fn(b"\x00\x00\x00\x01\x00\x00\x00\x02")}
        after = {"f": fn(b"\x00\x00\x00\x01\x00\x00\x00\x03")}
        row = objneutral.compare_functions(before, after)[0]
        self.assertEqual(row["verdict"], "CHANGED")
        self.assertEqual(row["first_differing_byte"], 7)
        self.assertEqual(row["differing_words"], 1)

    def test_equal_bytes_with_a_rebound_relocation_are_changed(self):
        # The wrong-pool class: the unlinked word is zero on both sides, so
        # only the relocation shows it.
        before = {"f": fn(relocs=[(4, 9, "@131", 0)])}
        after = {"f": fn(relocs=[(4, 9, "gPlayers", 0)])}
        self.assertEqual(objneutral.compare_functions(before, after)[0]
                         ["verdict"], "CHANGED")

    def test_an_added_or_removed_function_is_never_neutral(self):
        rows = objneutral.compare_functions({"a": fn()}, {"b": fn()})
        self.assertEqual(sorted(row["verdict"] for row in rows),
                         ["ADDED", "REMOVED"])

    def test_verdict_of_is_the_single_decision_point(self):
        self.assertEqual(objneutral.verdict_of(True, [], []), "IDENTICAL")
        self.assertEqual(objneutral.verdict_of(True, [], [1]), "RENUMBERED")
        self.assertEqual(objneutral.verdict_of(True, [1], [1]), "CHANGED")
        self.assertEqual(objneutral.verdict_of(False, [], []), "CHANGED")


class Refusals(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def test_a_missing_object_refuses(self):
        with self.assertRaises(objneutral.Unavailable):
            objneutral.read_object(self.root / "absent.o")

    def test_a_non_elf_file_refuses(self):
        path = self.root / "x.o"
        path.write_bytes(b"not an elf at all, but long enough to index" * 4)
        with self.assertRaisesRegex(objneutral.Unavailable, "not an ELF"):
            objneutral.read_object(path)

    def test_a_truncated_elf_refuses_rather_than_reading_garbage(self):
        path = self.root / "y.o"
        path.write_bytes(b"\x7fELF" + b"\x00" * 40)
        with self.assertRaises(objneutral.Unavailable):
            objneutral.read_object(path)

    def test_checking_without_a_bank_refuses_and_names_the_bank_command(self):
        done = subprocess.run(
            [sys.executable, "tools/gdl/objneutral.py", "check",
             "game/enemy/critter", "--tag", "c62_no_such_bank"],
            cwd=str(ROOT), capture_output=True, text=True)
        self.assertEqual(done.returncode, 2)
        self.assertIn("REFUSED", done.stdout)
        self.assertIn("objneutral.py bank", done.stdout)


UNIT = "game/enemy/critter"
LIVE = (ROOT / "build/GUNE5D/src/game/enemy/critter.o").is_file()


@unittest.skipUnless(LIVE, "needs a built object")
class LiveObject(unittest.TestCase):
    """Read a real object, then bank/patch/check the whole loop."""

    TAG = "c62_test"

    def setUp(self):
        self.object_path, self.meta_path = objneutral.bank_paths(UNIT, self.TAG)
        self.addCleanup(self.cleanup)
        done = self.run_tool("bank", "--tag", self.TAG)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)

    def cleanup(self):
        for path in (self.object_path, self.meta_path):
            if path.exists():
                path.unlink()

    def run_tool(self, action, *flags):
        return subprocess.run(
            [sys.executable, "tools/gdl/objneutral.py", action, UNIT, *flags],
            cwd=str(ROOT), capture_output=True, text=True)

    def test_reading_a_real_object_finds_functions_and_relocations(self):
        obj = objneutral.read_object(ROOT / "build/GUNE5D/src"
                                     / (UNIT + ".o"))
        rows = objneutral.function_rows(obj)
        self.assertGreater(len(rows), 50)
        self.assertTrue(any(row["relocs"] for row in rows.values()))
        total = sum(len(row["relocs"]) for row in rows.values())
        self.assertLessEqual(total, len(obj["relocs"]))
        self.assertTrue(any(objneutral.is_anonymous(r["symbol"])
                            for r in obj["relocs"]))

    def test_an_unchanged_object_is_neutral_and_exits_zero(self):
        done = self.run_tool("check", "--tag", self.TAG)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("VERDICT: NEUTRAL", done.stdout)
        self.assertIn("byte-identical", done.stdout)

    def test_one_patched_instruction_makes_it_not_neutral_and_exits_one(self):
        # Patch the BANKED copy so the current object reads as the edit.
        data = bytearray(self.object_path.read_bytes())
        obj = objneutral.read_object(self.object_path)
        rows = objneutral.function_rows(obj)
        name = next(n for n, row in rows.items() if row["size"] > 16)
        # .text's file offset is recoverable by locating the body bytes.
        body = rows[name]["bytes"]
        at = bytes(data).find(body)
        self.assertGreater(at, 0)
        data[at + 8] ^= 0x01
        self.object_path.write_bytes(bytes(data))
        done = self.run_tool("check", "--tag", self.TAG, "--json")
        self.assertEqual(done.returncode, 1, done.stdout[:400])
        result = json.loads(done.stdout)
        self.assertFalse(result["neutral"])
        changed = [row for row in result["functions"]
                   if row["verdict"] == "CHANGED"]
        self.assertEqual([row["function"] for row in changed], [name])
        self.assertEqual(changed[0]["differing_words"], 1)

    def test_a_renumbered_pool_entry_alone_stays_neutral(self):
        # Rename one anonymous entry IN THE BANK, same length, so the two
        # objects differ only in a pool NUMBER.
        data = self.object_path.read_bytes()
        self.assertEqual(data.count(b"@131\x00"), 1)
        self.object_path.write_bytes(data.replace(b"@131\x00", b"@931\x00"))
        done = self.run_tool("check", "--tag", self.TAG, "--json")
        self.assertEqual(done.returncode, 0, done.stdout[:400])
        result = json.loads(done.stdout)
        self.assertTrue(result["neutral"])
        self.assertFalse(result["object_identical"])
        renumbered = [row for row in result["functions"]
                      if row["verdict"] == "RENUMBERED"]
        self.assertTrue(renumbered)
        entry = renumbered[0]["renumbered_relocations"][0]
        self.assertEqual({entry["before"], entry["after"]}, {"@931", "@131"})

    def test_a_bank_for_another_unit_is_refused_not_compared(self):
        meta = json.loads(self.meta_path.read_text(encoding="utf-8"))
        meta["unit"] = "game/game/player"
        self.meta_path.write_text(json.dumps(meta), encoding="utf-8")
        done = self.run_tool("check", "--tag", self.TAG)
        self.assertEqual(done.returncode, 2)
        self.assertIn("belongs to", done.stdout)

    def test_list_names_the_bank(self):
        done = subprocess.run(
            [sys.executable, "tools/gdl/objneutral.py", "list", UNIT],
            cwd=str(ROOT), capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn(self.TAG, done.stdout)


if __name__ == "__main__":
    unittest.main()
