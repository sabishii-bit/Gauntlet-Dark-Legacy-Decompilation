#!/usr/bin/env python3
"""T3 run-59 item 2: value equality is not placement.

THE OBSERVATION (CR lane). `fndiff --clean` normalizes a named-versus-
anonymous pool row away and classes it POOL-KIND-EQUAL when the two entries
hold the same constant; `defake_gate` then keeps it with

    KIND-EQUAL-VALUE: the target's 'X' and our '@N' hold the SAME datum ...
    the relocation did NOT move; only the pool SPELLING did. Not a
    regression; keep it

That is the right answer to the SPELLING question and it is silent on
PLACEMENT. A section claim puts our WHOLE compiled pool at ONE base B, so
every relocation inside a BYTE-EXACT body forces B = target address - (our
pool offset + our addend). Two bindings that force different B cannot both
be satisfied by any placement, and a keep accepted under kind-equal-value
can add one while every row still reads benign.

MEASURED at c546ef915 on game/enemy/critter, with the CR lane's own
candidate object (W:/Temp/gdl-cr-critter-controls-20260907/rmt_literal_init/
cr_rmt_critter.o):

    HEAD raw body     .sdata2 5 required bases  .rodata 4  .data 1
    CR candidate      .sdata2 8 required bases  .rodata 4  .data 1
    regressions       8 NEW .sdata2 bases + @131 MOVED 0x80346470 ->
                      0x80346458  = 9 rows

The eight are byte-identical to the CR lane's own proof
(cr_cr_poolproof.json, computed by a different reader through
r68_aux_ownership_audit.object_inventory), which is an independent-reader
agreement on the whole set.

CALIBRATION, the other side. The CritterLineNodeColSub retirement
(76d7ac20b) rebinds to an EXISTING pool entry. Compiling critter.c at
76d7ac20b~1 through the real Ninja edge and comparing
(build/t3_scratch/t3_item2_calibration.py): .sdata2 5 -> 5, .rodata 4 -> 4,
.data 1 -> 1, ZERO placement regressions. And a gate baseline taken before
this feature carries no `placement` key at all, which yields no row rather
than reporting every current base as new.

SCOPE. `fndiff --clean` gets the table as EVIDENCE: it scores one function
at a time and has no before state, so it can report that a section admits
no contiguous claim but cannot call anything a regression. `defake_gate`
holds the before state and does call it one, as a REGRESSION on the
reserved `__sections__` row -- the whole-TU name it already uses, so
nothing that filters per-function verdicts by name mistakes it for a
sibling.
"""

import json
import os
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(TOOLS))

import fndiff                                                # noqa: E402
from tools.gdl import defake_gate as gate                    # noqa: E402

CRITTER_RAW = ROOT / "build/GUNE5D/src/game/enemy/.postprocess/body/critter.o"
CRITTER_TARGET = ROOT / "build/GUNE5D/obj/game/enemy/critter.o"
CANDIDATE = Path(r"W:\Temp\gdl-cr-critter-controls-20260907"
                 r"\rmt_literal_init\cr_rmt_critter.o")


class Regressions(unittest.TestCase):
    """`pool_placement_regressions`, over hand-built base sets."""

    def test_an_unchanged_base_set_is_clean(self):
        placement = {".sdata2": {0x80346470: ["@1", "@2"]}}
        self.assertEqual(
            fndiff.pool_placement_regressions(placement, placement), [])

    def test_a_NEW_base_is_a_regression(self):
        before = {".sdata2": {0x80346470: ["@1"]}}
        after = {".sdata2": {0x80346470: ["@1"], 0x80346480: ["@9"]}}
        rows = fndiff.pool_placement_regressions(before, after)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0][0], ".sdata2")
        self.assertIn("NEW required base 0x80346480", rows[0][1])
        self.assertIn("@9", rows[0][1])

    def test_a_MOVED_datum_is_a_regression(self):
        before = {".sdata2": {0x80346470: ["@1", "@2"]}}
        after = {".sdata2": {0x80346470: ["@1"], 0x80346480: ["@2"]}}
        rows = fndiff.pool_placement_regressions(before, after)
        self.assertTrue(any("@2 MOVED" in text for _s, text in rows), rows)

    def test_a_base_that_DISAPPEARS_is_not_a_regression(self):
        """Fewer constraints is not worse placement."""
        before = {".sdata2": {0x1000: ["@1"], 0x2000: ["@2"]}}
        after = {".sdata2": {0x1000: ["@1"]}}
        self.assertEqual(fndiff.pool_placement_regressions(before, after), [])

    def test_each_section_is_judged_on_its_own(self):
        before = {".sdata2": {0x1000: ["@1"]}}
        after = {".sdata2": {0x1000: ["@1"]},
                 ".rodata": {0x2000: ["@2"]}}
        rows = fndiff.pool_placement_regressions(before, after)
        self.assertEqual([section for section, _t in rows], [".rodata"])


class GateVerdicts(unittest.TestCase):
    """`defake_gate.pool_placement_verdicts`, and what it refuses to say."""

    BASE = {"data": {}, "placement": {".sdata2": {"0x80346470": ["@1"]}}}

    def test_an_unchanged_placement_produces_no_row(self):
        self.assertEqual(gate.pool_placement_verdicts(self.BASE, self.BASE),
                         [])

    def test_a_new_base_is_a_REGRESSION_on_the_whole_TU_row(self):
        after = {"data": {},
                 "placement": {".sdata2": {"0x80346470": ["@1"],
                                           "0x80346480": ["@9"]}}}
        rows = gate.pool_placement_verdicts(self.BASE, after)
        self.assertEqual(len(rows), 1)
        name, kind, detail = rows[0]
        self.assertEqual(name, "__sections__")
        self.assertEqual(kind, "REGRESSION")
        self.assertIn("PLACEMENT REGRESSION", detail)
        self.assertIn("0x80346480", detail)
        self.assertIn(".sdata2 1->2", detail)

    def test_a_baseline_without_the_key_produces_no_row(self):
        """ABSENT is not EMPTY: an older gate file must not cry wolf."""
        after = {"data": {},
                 "placement": {".sdata2": {"0x80346470": ["@1"]}}}
        self.assertEqual(gate.pool_placement_verdicts({"data": {}}, after), [])
        self.assertEqual(gate.pool_placement_verdicts(None, after), [])
        self.assertEqual(gate.pool_placement_verdicts(self.BASE, {"data": {}}),
                         [])

    def test_a_malformed_banked_placement_produces_no_row(self):
        bad = {"placement": {".sdata2": {"not-a-base": ["@1"]}}}
        self.assertEqual(gate.pool_placement_verdicts(self.BASE, bad), [])

    def test_compare_routes_the_row_through_the_reserved_key(self):
        after = {"data": {},
                 "placement": {".sdata2": {"0x80346470": ["@1"],
                                           "0x80346480": ["@9"]}}}
        verdicts = gate.compare({"__sections__": self.BASE},
                                {"__sections__": after})
        self.assertTrue(
            any(kind == "REGRESSION" and "PLACEMENT REGRESSION" in detail
                for _n, kind, detail in verdicts), verdicts)


class LiveCritter(unittest.TestCase):
    """The unit the observation names, against the built objects."""

    @classmethod
    def setUpClass(cls):
        if not (CRITTER_RAW.exists() and CRITTER_TARGET.exists()):
            raise unittest.SkipTest("critter objects are not built")
        os.chdir(ROOT)
        cls.head = fndiff.pool_placement(CRITTER_RAW, CRITTER_TARGET)

    def test_the_unit_already_admits_no_contiguous_sdata2_claim(self):
        self.assertGreater(len(self.head[".sdata2"]), 1,
                           "critter's .sdata2 used to require 5 bases")

    def test_every_pinned_datum_is_ours_and_in_the_section(self):
        symbols, _blobs = fndiff.object_sections(CRITTER_RAW, readable=None)
        for section, bases in self.head.items():
            for names in bases.values():
                for name in names:
                    self.assertEqual(symbols[name][0], section, name)

    def test_the_snapshot_round_trips_through_the_gate_form(self):
        snapshot = gate.pool_placement_snapshot(CRITTER_RAW, CRITTER_TARGET)
        self.assertEqual(gate._placement_bases(snapshot), self.head)
        self.assertEqual(json.loads(json.dumps(snapshot)), snapshot)

    def test_a_missing_object_yields_None_not_an_empty_placement(self):
        self.assertIsNone(gate.pool_placement_snapshot(
            ROOT / "build/GUNE5D/src/no/such.o", CRITTER_TARGET))


class LiveCandidate(unittest.TestCase):
    """The keep that KIND-EQUAL-VALUE accepted, measured."""

    @classmethod
    def setUpClass(cls):
        if not (CANDIDATE.exists() and CRITTER_RAW.exists()
                and CRITTER_TARGET.exists()):
            raise unittest.SkipTest("the CR lane's candidate object is absent")
        os.chdir(ROOT)
        cls.before = fndiff.pool_placement(CRITTER_RAW, CRITTER_TARGET)
        cls.after = fndiff.pool_placement(CANDIDATE, CRITTER_TARGET)

    def test_the_candidate_needs_more_sdata2_bases_than_HEAD(self):
        self.assertGreater(len(self.after[".sdata2"]),
                           len(self.before[".sdata2"]))

    def test_the_gate_reports_it_as_a_placement_regression(self):
        rows = fndiff.pool_placement_regressions(self.before, self.after)
        self.assertTrue(rows)
        self.assertTrue(all(section == ".sdata2" for section, _t in rows),
                        rows)

    def test_the_base_set_matches_the_CR_lanes_independent_proof(self):
        proof = CANDIDATE.parent.parent / "cr_cr_poolproof.json"
        if not proof.exists():
            self.skipTest("the CR lane's proof file is absent")
        expected = json.loads(proof.read_text(encoding="utf-8"))
        measured = {hex(base): names
                    for base, names in self.after[".sdata2"].items()}
        self.assertEqual(measured, expected)


if __name__ == "__main__":
    unittest.main()
