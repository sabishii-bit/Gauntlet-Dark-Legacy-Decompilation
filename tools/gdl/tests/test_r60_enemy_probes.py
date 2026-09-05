"""Pure-core regressions for the scratch-only enemy experiments."""
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "composed_census"))
import r60_enemy_boundary_probe as boundary
import r60_enemy_pool_probe as pool
import r60_enemy_source_probe as source_probe


class BaselineFidelity(unittest.TestCase):
    def test_overlapping_expected_path_does_not_compare_output_to_itself(self):
        with tempfile.TemporaryDirectory() as directory:
            obj = Path(directory) / "baseline.o"
            obj.write_bytes(b"old verified object")

            def overwritten(*args):
                obj.write_bytes(b"different generated object")
                return obj, None

            with patch.object(source_probe, "compile_with", side_effect=overwritten):
                with self.assertRaisesRegex(ValueError, "fidelity failed"):
                    source_probe.compile_baseline({"mw": "test", "cflags": ""}, Path("source.c"), obj, obj, Path(directory))


class EnemyTypeControls(unittest.TestCase):
    def test_controls_preserve_live_arguments_and_restore_pragmas(self):
        body = "s32 GetEnemyType(s32 w, s32 l) { ErrorPrintf(lbl_801124EC, findWorldName(w), w, l); }\n"
        forms = list(source_probe.enemy_type_control_variants(body))
        self.assertEqual(len(forms), 3)
        for name, text in forms:
            control = name.removesuffix("_off")
            self.assertEqual(text, "#pragma " + control + " off\n" + body + "\n#pragma " + control + " reset\n")

    def test_old_missing_argument_body_is_refused(self):
        with self.assertRaises(ValueError):
            list(source_probe.enemy_type_control_variants("ErrorPrintf(lbl_801124EC, name);"))


class TargetPlayerColors(unittest.TestCase):
    BODY = """void fn_800516F8(s32 slot)
{
    f32 dist;
    f64 kPi;
    f32 range;
    f32 bestSpecial;
                    {
                        f32 measuredDistance;
                        measuredDistance = measure(slot);
                        range = dist = measuredDistance;
                    }
                    if (range > sight) return;
    use(dist, kPi, range, bestSpecial);
}
"""

    def test_permutation_census_is_complete_and_body_unchanged(self):
        forms = dict(source_probe.target_player_color_variants(self.BODY))
        orders = {name: text for name, text in forms.items() if name.startswith("declarations_")}
        self.assertEqual(len(orders), 23)
        for text in orders.values():
            self.assertEqual(text[text.index("                    {"):], self.BODY[self.BODY.index("                    {"):])
            for declaration in ("f32 dist;", "f64 kPi;", "f32 range;", "f32 bestSpecial;"):
                self.assertEqual(text.count(declaration), 1)

    def test_chain_controls_are_distinct_and_unknown_shape_refused(self):
        forms = dict(source_probe.target_player_color_variants(self.BODY))
        self.assertIn("dist = range = measuredDistance;", forms["scoped_chain_reversed"])
        self.assertIn("if ((range = dist) >", forms["condition_chain"])
        with self.assertRaises(ValueError):
            list(source_probe.target_player_color_variants("void f(void) {}"))


class InitVarsForms(unittest.TestCase):
    BODY = """void init_enemy_vars(s32 slot, s32 spew, f32 scale)
{
    u8* e;
    s32 i4;
    s32 tier;
    f32 t;
    f32 hi;
    f32 lo;
    f32 t2;
    f32 hi2;
    f32 lo2;
    enemy->action = 0;
    for (i4 = 0; i4 < 20; i4 += 4) {
        *(f32*)(e + i4 + 692) = z;
    }
    z2 = lbl_80346820;
    enemy->push_cnt = 0;
    row = tbl + *(s32*)e * 4;
    enemy->hht = (f32)(lbl_80346830 * ((f32*)row)[452]);
    row = tbl + *(s32*)e * 4;
    enemy->rad = ((f32*)row)[486];
    row = tbl + *(s32*)e * 4;
    t = gCurLevel->ene_health * ((f32*)row)[690];
    tier = 0;
    if (scale > hi) tier = 3;
    else if (scale > lo) tier = 2;
    else if (scale > z2) tier = 1;
    enemy->org_lvl = (s16)tier;
    enemy->mode2 = 0;
    row = tbl + *(s32*)e * 4;
    enemy->algorithm = (s16)((s32*)row)[894];
    format_brain(slot);
    row = tbl + *(s32*)e * 4;
    enemy->atts.invspeed = (f32)(lbl_80346810 / ((f32*)row)[588]);
    if (!(ht > hi2)) {
        if (ty != 30) {
            if (ht > lo2) spd = (f32)(lbl_80346A30 * spd);
            else spd = (f32)(lbl_80346A28 * spd);
        }
    }
    enemy->atts.fight = spd;
    row = tbl + *(s32*)e * 4;
    enemy->atts.armor = ((f32*)row)[656];
    row = tbl + *(s32*)e * 4;
    enemy->atts.damagetype = ((s32*)row)[928];
    row = tbl + *(s32*)e * 4;
    enemy->atts.armortype = ((s32*)row)[962];
}
"""

    def test_named_joint_form_preserves_slot_and_has_no_clear_wrapper(self):
        forms = dict(source_probe.init_vars_variants(self.BODY))
        text = forms["named_helpers_frame_8"]
        self.assertIn("for (i4 = 0; i4 < 5; i4++)", text)
        self.assertIn("enemy->fxhittime[i4] = z;", text)
        self.assertNotIn("enemy_clear_hit_times", text)
        self.assertIn("format_brain(slot);", text)
        self.assertIn("u8 unrecovered_locals[8];", text)
        self.assertTrue(text.endswith("#pragma opt_propagation reset\n"))

    def test_parameter_order_census_and_shape_guard(self):
        forms = list(source_probe.init_vars_variants(self.BODY))
        self.assertEqual(len(forms), len({name for name, _ in forms}))
        for helper in ("enemy_health_tier", "enemy_tier_damage"):
            self.assertEqual(sum(name.startswith(helper + "_args_") for name, _ in forms), 23)
        with self.assertRaises(ValueError):
            list(source_probe.init_vars_variants("void f(void) {}"))


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
