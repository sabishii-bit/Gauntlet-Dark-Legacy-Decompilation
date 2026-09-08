"""An unclaimed section's binding is UNRESOLVED, not a difference (item 2).

THE OBSERVATION (CS2 lane). `retire_audit.positional_relocations` refused
every critter retirement while game/enemy/critter claims no `.sdata2` run:
our object spells a pool datum `('@', '.sdata2', 24, 8, 1, 0)`, dtk spells
the same datum by address, splits.txt claims no base to resolve ours with,
and the row was reported as a DIFFERENCE. Reproduced at 20c0d7ea1:

    python tools/gdl/retire_audit.py game/enemy/critter \\
        CritterLookForCriticalMove --before-ref e758f3d16~1 --skip-link

    [   FAIL] positional_relocations
              our_count: 4   target_count: 4
              unresolved_ours: [["@", ".sdata2", 24, 8, 1, 0]]

for a binding that is byte-identical before and after the retirement.

THE CURE compares an unresolvable row against the SAME POSITION IN THE
BEFORE IMAGE -- the question a retirement actually asks -- and reports the
missing target comparison as UNRESOLVED the way `datadiff` does, never as
PASS and never as a failure. Everything measurable still has to hold: the
offset, type and addend must match the target verbatim, and a row that
MOVED still fails by name.

LIVE CALIBRATION at 1f59fadb8, `--skip-link`:

    game/enemy/enemy::gendir_8004FBC8     6eaec136d~1  PASS  14/14 certified
    game/sys/memcard::memCardErrorPrompt  e7f9d740b~1  PASS   6/6  certified
    game/mb/mb_camera::MBCameraUpdate     30582a770~1  PASS  17/17 certified
    game/game/player::ExpToLevel          37f9daac5~1  positional_relocations
        PASS (0 rows); the certificate as a whole no longer reproduces
        because player.c and its rules moved on after 37f9daac5 --
        sibling_bodies, rule_delta and rule_replay, none of them this check.
    game/enemy/critter::CritterLookForCriticalMove  e758f3d16~1
        positional_relocations FAIL -> UNRESOLVED, 3 certified + 1 carried.
"""
import unittest

from tools.gdl import retire_audit as ra

SDA21 = ra.SDA21
POOL = ["@", ".sdata2", 24, 8, 1, 0]
BOUND = ["address", 0x80346A4C]
OTHER = ["address", 0x80346A50]


def row(name, offset=0x28, kind=SDA21, addend=0):
    return (offset, kind, name, addend)


class UnresolvableRows(unittest.TestCase):
    def test_an_unchanged_unresolvable_row_is_unresolved_not_failed(self):
        ours = [row(POOL)]
        target = [row(BOUND)]
        verdict = ra.compare_relocation_tables(ours, target, previous=[row(POOL)])
        self.assertEqual(verdict["status"], "UNRESOLVED")
        self.assertEqual(verdict["mismatches"], [])
        self.assertEqual(len(verdict["unresolved"]), 1)
        self.assertIn("not claimed in splits.txt",
                      verdict["unresolved"][0]["reason"])

    def test_a_MOVED_unresolvable_row_fails_by_name(self):
        ours = [row(POOL)]
        target = [row(BOUND)]
        moved = [row(["@", ".sdata2", 32, 8, 1, 0])]
        verdict = ra.compare_relocation_tables(ours, target, previous=moved)
        self.assertEqual(verdict["status"], "FAIL")
        self.assertIn("MOVED", verdict["mismatches"][0]["reason"])

    def test_no_before_image_row_is_a_failure_not_a_free_pass(self):
        ours, target = [row(POOL)], [row(BOUND)]
        for previous in (None, [], [row(POOL), row(POOL)]):
            with self.subTest(previous=previous):
                verdict = ra.compare_relocation_tables(ours, target, previous)
                self.assertEqual(verdict["status"], "FAIL")
                self.assertIn("NO comparable before",
                              verdict["mismatches"][0]["reason"])

    def test_an_unresolvable_row_whose_offset_or_addend_moved_still_fails(self):
        for target in ([row(BOUND, offset=0x2C)], [row(BOUND, addend=4)],
                       [row(BOUND, kind=4)]):
            with self.subTest(target=target):
                verdict = ra.compare_relocation_tables(
                    [row(POOL)], target, previous=[row(POOL)])
                self.assertEqual(verdict["status"], "FAIL")
                self.assertIn("offset, type or addend",
                              verdict["mismatches"][0]["reason"])


class ResolvedRows(unittest.TestCase):
    def test_equal_bound_rows_are_certified(self):
        verdict = ra.compare_relocation_tables([row(BOUND)], [row(BOUND)],
                                               previous=[row(BOUND)])
        self.assertEqual(verdict["status"], "PASS")
        self.assertEqual(verdict["certified"], 1)

    def test_a_mis_bound_row_still_fails_however_stable_it_is(self):
        # Both sides resolve, and the row is identical to the before image:
        # the before/after comparison must not rescue a wrong binding.
        verdict = ra.compare_relocation_tables([row(BOUND)], [row(OTHER)],
                                               previous=[row(BOUND)])
        self.assertEqual(verdict["status"], "FAIL")
        self.assertIn("different addresses", verdict["mismatches"][0]["reason"])

    def test_a_count_difference_is_still_a_plain_failure(self):
        verdict = ra.compare_relocation_tables([row(BOUND)], [], previous=[])
        self.assertEqual(verdict["status"], "FAIL")
        self.assertIn("COUNT differs", verdict["reason"])

    def test_an_empty_table_is_not_promoted_to_unresolved(self):
        verdict = ra.compare_relocation_tables([], [], previous=[])
        self.assertEqual(verdict["status"], "PASS")
        self.assertEqual(verdict["certified"], 0)

    def test_mixed_rows_report_both_halves(self):
        ours = [row(BOUND, offset=0x10), row(POOL, offset=0x28)]
        target = [row(BOUND, offset=0x10), row(["address", 0x1234], 0x28)]
        verdict = ra.compare_relocation_tables(ours, target, previous=ours)
        self.assertEqual(verdict["status"], "UNRESOLVED")
        self.assertEqual(verdict["certified"], 1)
        self.assertEqual(len(verdict["unresolved"]), 1)


class BoundPredicate(unittest.TestCase):
    def test_only_a_two_element_address_pair_counts_as_bound(self):
        self.assertTrue(ra._bound(["address", 1]))
        for name in ("lbl_80346A4C", POOL, ["address"], ["address", 1, 2],
                     None, 0x80346A4C):
            with self.subTest(name=name):
                self.assertFalse(ra._bound(name))


if __name__ == "__main__":
    unittest.main()
