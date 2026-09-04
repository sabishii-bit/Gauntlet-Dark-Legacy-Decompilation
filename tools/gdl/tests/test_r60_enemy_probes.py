"""Pure-core regressions for the scratch-only enemy experiments."""
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "composed_census"))
import r60_enemy_boundary_probe as boundary
import r60_enemy_pool_probe as pool
import r60_enemy_source_probe as source_probe


class ResourceForms(unittest.TestCase):
    BODY = """void fn_80051164(void)
{
    s32* p = lbl_80250E00;
    s32 i;

    for (i = 0; i < 45; i++) {
        p[345 + i] = 0;
        p[300 + i] = -1;
        p[255 + i] = 0;
    }
    for (i = 0; i < 8; i++) {
        p[8 + i] = -1;
    }
    lbl_8034471C = 0;
    lbl_80344738 = -1;
}
"""

    def test_joint_form_preserves_first_loop_and_restores_pragma(self):
        text = dict(source_probe.resource_variants(self.BODY))["propagation_off_second_alias"]
        self.assertTrue(text.startswith("#pragma opt_propagation off\n"))
        self.assertTrue(text.endswith("#pragma opt_propagation reset\n"))
        self.assertIn("p[345 + i] = 0;", text)
        self.assertIn("s32* row = p + i;\n        row[8] = -1;", text)
        self.assertIn("lbl_80344738 = -1;", text)

    def test_variants_are_distinct_and_input_shape_is_checked(self):
        forms = list(source_probe.resource_variants(self.BODY))
        self.assertEqual(len(forms), len({name for name, _ in forms}))
        with self.assertRaises(ValueError):
            list(source_probe.resource_variants("void f(void) {}\n"))


class PoolForms(unittest.TestCase):
    def forms(self, source="extern f64 lbl_80346818;\nvoid f(void) { sink(lbl_80346818); }\n"):
        with patch.object(pool, "read_datum", return_value=bytes.fromhex("4000000000000000")):
            return dict(pool.variants(source, [0x80346818]))

    def test_external_after_keeps_declaration_before_use(self):
        text = self.forms()["external_const_after"]
        self.assertLess(text.index("extern const f64"), text.index("void f"))
        self.assertGreater(text.index("const f64 lbl_80346818 = 2.0;"), text.index("void f"))

    def test_static_before_is_distinct_from_forward_definition(self):
        text = self.forms()["static_const_before"]
        self.assertLess(text.index("static const f64 lbl_80346818 = 2.0;"), text.index("void f"))
        self.assertNotIn("extern", text)

    def test_literal_mode_does_not_silently_claim_volatile_identity(self):
        forms = self.forms("extern f64 lbl_80346818;\nvoid f(void) { sink(*(volatile f64*)&lbl_80346818); }\n")
        text = forms["literals_VOLATILE_LOADS_REMOVED"]
        self.assertIn("sink((2.0))", text)
        self.assertIn("volatile", forms["external_const_after"])

    def test_unselected_symbol_is_untouched(self):
        forms = self.forms("extern f64 lbl_80346818;\nextern f32 lbl_80346820;\nvoid f(void) { sink(lbl_80346818, lbl_80346820); }\n")
        for text in forms.values():
            self.assertIn("extern f32 lbl_80346820;", text)

    def test_unknown_symbol_refused(self):
        with self.assertRaises(ValueError):
            list(pool.variants("", [0x80346818]))

    def test_array_refused(self):
        with self.assertRaises(ValueError):
            self.forms("extern f64 lbl_80346818[2];\n")


class BoundaryInventory(unittest.TestCase):
    def test_roster_has_twenty_two_entries_and_two_helpers(self):
        self.assertEqual(len(boundary.MOVED), 24)
        self.assertEqual(len(set(boundary.MOVED)), 24)
        self.assertEqual(boundary.MOVED[-2:], ["PlayersAverageLevel", "findWorldName"])

    def test_missing_pre_migration_function_is_explicit(self):
        with self.assertRaisesRegex(ValueError, "pre-migration function"):
            boundary.excerpt("void other(void) {}\n", "missing")

    def test_inventory_reports_missing_not_just_intersection(self):
        before = {"a": [(0, "4e800020", "blr")], "b": [(0, "4e800020", "blr")]}
        after = {"a": [(0, "4e800020", "blr")], "extra": [(0, "4e800020", "blr")]}
        row = boundary.changed_rows(before, after, {"a", "b"})
        self.assertEqual(row["missing"], ["b"])
        self.assertEqual(row["unexpected"], ["extra"])
        self.assertEqual(row["changed"], [])

    def test_changed_words_and_count_are_separate(self):
        before = {"a": [(0, "38600000", "li")], "b": [(0, "4e800020", "blr")]}
        after = {"a": [(0, "38800000", "li")], "b": [(0, "38600000", "li"), (4, "4e800020", "blr")]}
        rows = boundary.changed_rows(before, after, {"a", "b"})["changed"]
        self.assertEqual([(r["kind"], r["words"]) for r in rows], [("WORDS", 1), ("COUNT", 0)])


if __name__ == "__main__":
    unittest.main()
