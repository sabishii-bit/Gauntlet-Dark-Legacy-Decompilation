"""Two-sided tests for tools/gdl/pool_owner.py.

The pure model is calibrated on hand-built inputs whose right answer is
readable from the test itself; the report is then calibrated on three live
units of this checkout with DIFFERENT shapes, so a passing suite means the
tool distinguishes them rather than printing one plausible page:

    game/sys/ml_mem     owns its .rodata run and its pool order AGREES
    game/enemy/enemy    owns its .sdata2 run, references datums outside it,
                        and its pool order DISAGREES
    game/game/gamemain  claims NO pool run at all, and every pool datum its
                        text reads is UNCLAIMED

MEASURED PREMISE CORRECTION (306e80654). The task that commissioned this
tool described game/enemy/enemy's .sdata2 as UNCLAIMED, with a target run
0x80346810..0x80346A70 holding 87 datums. Reproduced instead:
config/GUNE5D/splits.txt line 142 CLAIMS `.sdata2 start:0x80346810
end:0x80346AAC` for game/enemy/enemy.c, the extracted target object carries
668 bytes there, symbols.txt names 96 datums inside it, and our raw object
emits 96 anonymous entries every one of which matches its target datum's
bytes. What IS unclaimed is a separate set of 6 datums the TU reads from
OUTSIDE its own run. The tests below assert the measured numbers.
"""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

GDL = Path(__file__).resolve().parents[1]
ROOT = GDL.parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(GDL))

from tools.gdl import pool_owner as po  # noqa: E402

RUNS = [
    ("game/sys/ml_mem", ".rodata", 0x801161B0, 0x80116450),
    ("game/enemy/enemy", ".sdata2", 0x80346810, 0x80346AAC),
    ("game/enemy/enemy", ".text", 0x800444C0, 0x800520CC),
]


class SplitTests(unittest.TestCase):
    def write(self, text):
        folder = Path(tempfile.mkdtemp())
        path = folder / "splits.txt"
        path.write_text(text, encoding="utf-8")
        return path

    def test_runs_are_read_with_their_unit_and_extension_stripped(self):
        path = self.write("Sections:\n\t.text type:code align:4\n\n"
                          "game/sys/ml_mem.c:\n"
                          "\t.text       start:0x800BE964 end:0x800BFC80\n"
                          "\t.rodata     start:0x801161B0 end:0x80116450\n")
        self.assertEqual(po.load_splits(path),
                         [("game/sys/ml_mem", ".text", 0x800BE964, 0x800BFC80),
                          ("game/sys/ml_mem", ".rodata", 0x801161B0, 0x80116450)])

    def test_a_split_file_with_no_runs_refuses(self):
        path = self.write("Sections:\n\t.text type:code align:4\n")
        with self.assertRaises(po.Refused):
            po.load_splits(path)

    def test_a_missing_split_file_refuses(self):
        with self.assertRaises(po.Refused):
            po.load_splits(Path("no-such-splits.txt"))

    def test_a_run_does_not_leak_across_units(self):
        path = self.write("game/a.c:\n\t.rodata start:0x100 end:0x200\n\n"
                          "game/b.c:\n\t.rodata start:0x200 end:0x300\n")
        runs = po.load_splits(path)
        self.assertEqual(po.owner_of(runs, ".rodata", 0x1FF), "game/a")
        self.assertEqual(po.owner_of(runs, ".rodata", 0x200), "game/b")


class OwnerTests(unittest.TestCase):
    def test_an_address_inside_a_run_is_owned_by_it(self):
        self.assertEqual(po.owner_of(RUNS, ".rodata", 0x801161B0), "game/sys/ml_mem")
        self.assertEqual(po.owner_of(RUNS, ".sdata2", 0x80346AAB), "game/enemy/enemy")

    def test_the_end_address_is_exclusive(self):
        self.assertEqual(po.owner_of(RUNS, ".sdata2", 0x80346AAC), "UNCLAIMED")

    def test_an_address_below_a_run_is_unclaimed(self):
        self.assertEqual(po.owner_of(RUNS, ".sdata2", 0x80346770), "UNCLAIMED")

    def test_the_section_must_match_too(self):
        """The same address in another section is not the same datum."""
        self.assertEqual(po.owner_of(RUNS, ".sdata", 0x80346900), "UNCLAIMED")

    def test_no_runs_means_unclaimed_not_an_error(self):
        self.assertEqual(po.owner_of([], ".rodata", 0x80116000), "UNCLAIMED")


class DecodeTests(unittest.TestCase):
    def test_a_declared_double_prefers_f64(self):
        row = po.decode_value(bytes.fromhex("3FF0000000000000"), 8, "double")
        self.assertEqual(row["preferred"], "f64")
        self.assertEqual(row["f64"], 1.0)

    def test_a_four_byte_kind_in_an_eight_byte_slot_never_prefers_f64(self):
        """`gErrorCode ... size:0x8 data:4byte` -- reading f64 splices the
        next datum's bytes into the value and prints `5.4e-312`."""
        row = po.decode_value(bytes.fromhex("000000FF00000000"), 8, "4byte")
        self.assertEqual(row["preferred"], "u32")
        self.assertEqual(row["u32"], 255)

    def test_a_float_datum_prefers_f32(self):
        row = po.decode_value(bytes.fromhex("40A00000"), 4, "float")
        self.assertEqual((row["preferred"], row["f32"]), ("f32", 5.0))

    def test_a_string_datum_prefers_its_text(self):
        row = po.decode_value(b"IT\0\0", 4, "string")
        self.assertEqual((row["preferred"], row["string"]), ("string", "IT"))

    def test_a_merged_string_run_is_split_into_its_segments(self):
        row = po.decode_value(b"one\0two\0three\0", 14, None)
        self.assertEqual(row["preferred"], "string_run")
        self.assertEqual(row["string_run"], ["one", "two", "three"])

    def test_binary_bytes_are_not_reported_as_a_string(self):
        row = po.decode_value(bytes.fromhex("0102FF0400"), 5, None)
        self.assertNotIn("string", row)
        self.assertNotIn("string_run", row)
        self.assertEqual(row["preferred"], "bytes")

    def test_the_raw_bytes_are_always_present(self):
        row = po.decode_value(bytes.fromhex("40A0000000000000"), 4, "float")
        self.assertEqual(row["bytes"], "40a00000")


def datum(name, address, section, offset, owned, generated=True,
          disposition="POOL"):
    return {"name": name, "address": address, "section": section,
            "owner": "u" if owned else "other", "owned_by_this_unit": owned,
            "first_reference_text_offset": offset,
            "compiler_generated_name": generated, "disposition": disposition,
            "reference_sites": [{"function": "f", "offset": 0,
                                 "text_offset": offset, "relocation_type": 6}]}


class PredictionTests(unittest.TestCase):
    def test_first_use_matching_address_order_agrees(self):
        rows = [datum("lbl_00000010", "0x00000010", ".sdata2", 100, True),
                datum("lbl_00000018", "0x00000018", ".sdata2", 200, True)]
        result = po.predict_first_use(rows, "u")
        self.assertTrue(result["orders_agree"])
        self.assertIsNone(result["first_disagreement_index"])

    def test_a_swapped_pair_disagrees_and_names_the_index(self):
        rows = [datum("lbl_00000010", "0x00000010", ".sdata2", 900, True),
                datum("lbl_00000018", "0x00000018", ".sdata2", 100, True)]
        result = po.predict_first_use(rows, "u")
        self.assertFalse(result["orders_agree"])
        self.assertEqual(result["first_disagreement_index"], 0)
        self.assertEqual(result["first_disagreement"]["predicted"], "lbl_00000018")
        self.assertEqual(result["first_disagreement"]["actual"], "lbl_00000010")
        self.assertTrue(result["first_disagreement"]["implied_missing_early_reference"])

    def test_foreign_datums_are_not_in_the_prediction(self):
        rows = [datum("lbl_00000010", "0x00000010", ".sdata2", 100, True),
                datum("lbl_00000018", "0x00000018", ".sdata2", 50, False)]
        result = po.predict_first_use(rows, "u")
        self.assertEqual(result["predicted_first_use_order"], ["lbl_00000010"])

    def test_a_declared_global_is_excluded_from_the_model(self):
        rows = [datum("lbl_00000010", "0x00000010", ".rodata", 900, True),
                datum("mlmThing", "0x00000008", ".rodata", 100, True, generated=False)]
        result = po.predict_first_use(rows, "u")
        self.assertEqual(result["excluded_named_or_writable"], ["mlmThing"])
        self.assertTrue(result["orders_agree"])

    def test_a_writable_sdata_datum_is_excluded_from_the_model(self):
        rows = [datum("lbl_00000010", "0x00000010", ".sdata", 900, True,
                      disposition="DATA")]
        result = po.predict_first_use(rows, "u")
        self.assertFalse(result["applicable"])
        self.assertFalse(result["orders_agree"])

    def test_nothing_to_predict_is_not_reported_as_agreement(self):
        result = po.predict_first_use([], "u")
        self.assertFalse(result["applicable"])
        self.assertFalse(result["orders_agree"])


def run_cli(*args):
    return subprocess.run([sys.executable, str(GDL / "pool_owner.py"), *args],
                          capture_output=True, text=True, cwd=str(ROOT))


class LiveUnitTests(unittest.TestCase):
    """The three live shapes, with the numbers measured at 306e80654."""

    @classmethod
    def setUpClass(cls):
        if not (ROOT / "build/GUNE5D/obj/game/sys/ml_mem.o").exists():
            raise unittest.SkipTest("target objects are not split in this checkout")

    def test_ml_mem_owns_its_rodata_run_and_its_pool_order_agrees(self):
        result = po.analyze("game/sys/ml_mem")
        self.assertEqual(result["claimed_pool_extent"], [{
            "section": ".rodata", "start": "0x801161B0", "end": "0x80116450",
            "size": 672, "named_datums_in_symbols": 11,
            "target_section_bytes": 672}])
        self.assertFalse(result["claims_no_pool_run"])
        own = [d for d in result["datums"] if d["owned_by_this_unit"]]
        self.assertEqual(len(own), 11)
        self.assertTrue(result["first_use_prediction"]["orders_agree"])
        self.assertEqual(result["first_use_prediction"]["modelled_generated_literals"], 10)

    def test_ml_mem_names_its_foreign_and_unclaimed_reads(self):
        result = po.analyze("game/sys/ml_mem")
        self.assertEqual(result["referenced_foreign"], ["dolphin/os/OSAlloc"])
        self.assertEqual(result["referenced_unclaimed"], 5)
        heap = next(d for d in result["datums"] if d["name"] == "__OSCurrHeap")
        self.assertEqual(heap["owner"], "dolphin/os/OSAlloc")
        self.assertEqual(heap["disposition"], "DATA")
        self.assertEqual(heap["value"]["preferred"], "u32")

    def test_ml_mem_merged_string_runs_match_segment_by_segment(self):
        """A whole-run byte compare fails by construction; segments do not."""
        result = po.analyze("game/sys/ml_mem")
        row = next(d for d in result["datums"] if d["name"] == "lbl_801161B0")
        self.assertEqual(row["our_equal_value_count"], 0)
        self.assertEqual(len(row["our_string_segment_matches"]), 2)
        self.assertEqual(row["our_string_segments_matched"], 2)

    def test_enemy_claims_its_sdata2_run_refuting_the_unclaimed_premise(self):
        result = po.analyze("game/enemy/enemy")
        self.assertEqual(result["claimed_pool_extent"], [{
            "section": ".sdata2", "start": "0x80346810", "end": "0x80346AAC",
            "size": 668, "named_datums_in_symbols": 96,
            "target_section_bytes": 668}])
        own = [d for d in result["datums"] if d["owned_by_this_unit"]]
        self.assertEqual(len(own), 96)
        self.assertTrue(all(d["owner"] == "game/enemy/enemy" for d in own))

    def test_every_enemy_owned_datum_has_an_equal_value_entry_in_our_object(self):
        result = po.analyze("game/enemy/enemy")
        own = [d for d in result["datums"] if d["owned_by_this_unit"]]
        self.assertEqual([d["name"] for d in own if not d["our_equal_value_count"]], [])
        first = next(d for d in own if d["name"] == "lbl_80346810")
        self.assertEqual(first["value"]["f64"], 1.0)
        self.assertEqual(first["our_equal_value_entries"][0]["offset"], 0)

    def test_enemy_reads_six_datums_from_outside_any_claimed_run(self):
        result = po.analyze("game/enemy/enemy")
        self.assertEqual(result["referenced_unclaimed"], 6)
        outside = [d["name"] for d in result["datums"] if d["owner"] == "UNCLAIMED"]
        self.assertIn("lbl_80346770", outside)  # below enemy's own .sdata2 run

    def test_enemy_pool_order_disagrees_and_implies_an_earlier_read(self):
        prediction = po.analyze("game/enemy/enemy")["first_use_prediction"]
        self.assertFalse(prediction["orders_agree"])
        self.assertEqual(prediction["first_disagreement_index"], 22)
        self.assertEqual(prediction["first_disagreement"]["actual"], "lbl_803468B8")
        self.assertTrue(prediction["first_disagreement"]["implied_missing_early_reference"])

    def test_a_unit_with_no_claimed_pool_says_so(self):
        result = po.analyze("game/game/gamemain")
        self.assertTrue(result["claims_no_pool_run"])
        self.assertEqual(result["claimed_pool_extent"], [])
        self.assertEqual(result["referenced_unclaimed"], result["referenced_pool_datums"])
        self.assertGreater(result["referenced_pool_datums"], 0)
        self.assertFalse(result["first_use_prediction"]["applicable"])

    def test_the_no_pool_report_never_claims_agreement(self):
        text = po.format_report(po.analyze("game/game/gamemain"))
        self.assertIn("CLAIMED POOL EXTENT: none", text)
        self.assertIn("FIRST-USE ORDER: NOT APPLICABLE", text)
        self.assertNotIn("agrees with the address order", text)

    def test_the_report_distinguishes_the_three_units(self):
        pages = {unit: po.format_report(po.analyze(unit)) for unit in
                 ("game/sys/ml_mem", "game/enemy/enemy", "game/game/gamemain")}
        self.assertEqual(len(set(pages.values())), 3)
        self.assertIn("agrees with the address order", pages["game/sys/ml_mem"])
        self.assertIn("DISAGREES at index 22", pages["game/enemy/enemy"])

    def test_the_json_report_round_trips(self):
        proc = run_cli("game/sys/ml_mem", "--json")
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(json.loads(proc.stdout)["unit"], "game/sys/ml_mem")


class RefusalTests(unittest.TestCase):
    def test_an_unsplit_unit_refuses(self):
        proc = run_cli("game/nope/nope")
        self.assertEqual(proc.returncode, 2)
        self.assertIn("no entry in splits.txt", proc.stderr)
        self.assertEqual(proc.stdout, "")

    def test_out_must_stay_under_build(self):
        proc = run_cli("game/sys/ml_mem", "--out", "pool.json")
        self.assertEqual(proc.returncode, 2)
        self.assertIn("build/", proc.stderr)

    def test_a_unit_without_a_target_object_refuses(self):
        """A split entry is not an extracted object."""
        runs = po.load_splits() + [("game/ghost/ghost", ".rodata", 1, 2)]
        with self.assertRaises(po.Refused) as caught:
            po.analyze("game/ghost/ghost", runs=runs)
        self.assertIn("missing target object", str(caught.exception))

    def test_an_empty_symbol_table_refuses(self):
        with self.assertRaises(po.Refused):
            po.analyze("game/sys/ml_mem", symbols={})


if __name__ == "__main__":
    unittest.main()
