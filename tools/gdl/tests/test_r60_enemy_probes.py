"""Pure-core regressions for the scratch-only enemy experiments."""
import sys
import struct
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


class ExceptionMetadata(unittest.TestCase):
    def fixture(self, relocation_type=1, partial=False):
        # Mock section discovery only; actual symbol and relocation decoding
        # uses a binary fixture with dotless extab/extabindex names.
        section = source_probe.wf.Section
        sections = [
            section(0, "", 0, 0, 0, 0, 0, 0),
            section(1, ".text", 1, 16, 8, 0, 0, 0),
            section(2, "extab", 1, 32, 8, 0, 0, 0),
            section(3, "extabindex", 1, 48, 11 if partial else 12, 0, 0, 0),
            section(4, ".strtab", 3, 64, 9, 0, 0, 0),
            section(5, ".symtab", 2, 80, 48, 4, 0, 16),
            section(6, ".relaextabindex", 4, 128, 24, 5, 3, 12),
        ]
        data = bytearray(160)
        data[:6] = b"\x7fELF\x01\x02"
        data[32:40] = bytes.fromhex("3088000000000000")
        struct.pack_into(">I", data, 52, 8)
        data[64:73] = b"\0fn\0meta\0"
        struct.pack_into(">IIIBBH", data, 96, 1, 0, 8, 2, 0, 1)
        struct.pack_into(">IIIBBH", data, 112, 4, 0, 8, 1, 0, 2)
        struct.pack_into(">IIi", data, 128, 0, 256 | relocation_type, 0)
        struct.pack_into(">IIi", data, 140, 8, 512 | 1, 0)
        return bytes(data), sections

    def test_dotless_metadata_is_read_from_object_bytes(self):
        data, sections = self.fixture()
        with patch.object(source_probe.wf, "_sections", return_value=sections):
            self.assertEqual(source_probe.exception_records(data), {"fn": {"length": 8, "metadata": "3088000000000000"}})

    def test_wrong_relocation_and_partial_record_refuse(self):
        for kwargs in ({"relocation_type": 10}, {"partial": True}):
            data, sections = self.fixture(**kwargs)
            with patch.object(source_probe.wf, "_sections", return_value=sections):
                with self.assertRaises(ValueError):
                    source_probe.exception_records(data)

    def test_comparison_separates_extra_from_changed_and_missing(self):
        record = {"length": 8, "metadata": "00"}
        result = source_probe.compare_exception_records({"fn": record}, {"helper": record, "fn": record})
        self.assertEqual(result["changed"], {})
        self.assertEqual(result["missing"], [])
        self.assertEqual(set(result["extra"]), {"helper"})
        result = source_probe.compare_exception_records({"fn": record, "lost": record}, {"fn": dict(record, length=12)})
        self.assertEqual(result["missing"], ["lost"])
        self.assertEqual(set(result["changed"]), {"fn"})


class NormalizationResultWeb(unittest.TestCase):
    BLOCK = """            {
                f64 av;
                if ((av = cand) > 3.141592654) {
                    av -= 6.283185308;
                } else if (av <= -3.141592654) {
                    av = 6.283185308 + av;
                }
                cand = av;
            }"""

    def fixture(self):
        arms = "                cand = cand + q[1095];\n                cand = cand - q[1095];\n" * 3
        fallback = "            } else {\n                cand = lbl_80344720;\n            }\n"
        return "        f32 cand;\n" + arms + fallback + self.BLOCK + "\n        use(cand);\n"

    def test_float_operands_and_single_normalization_narrowing_are_preserved(self):
        text = source_probe.normalization_arm_result(self.fixture(), self.BLOCK)
        self.assertIn("f32 cand;", text)
        self.assertIn("f64 normalAngle;", text)
        self.assertEqual(text.count("normalAngle = cand + q[1095];"), 3)
        self.assertEqual(text.count("normalAngle = cand - q[1095];"), 3)
        self.assertEqual(text.count("cand = normalAngle;"), 1)
        self.assertIn("normalAngle = lbl_80344720;", text)
        self.assertIn("normalAngle = 6.283185308 + normalAngle;", text)
        self.assertNotIn("f64 cand", text)
        self.assertNotIn("(f32)cand", text)
        self.assertTrue(text.endswith("\n        use(cand);\n"))

    def test_incomplete_branch_set_and_unknown_join_refuse(self):
        incomplete = self.fixture().replace("cand = cand + q[1095];", "other();", 1)
        with self.assertRaisesRegex(ValueError, "three float-result arms"):
            source_probe.normalization_arm_result(incomplete, self.BLOCK)
        with self.assertRaises(ValueError):
            source_probe.normalization_arm_result(self.fixture(), "missing block")


class MovementReloadForms(unittest.TestCase):
    BODY = """void do_enemy_move(s32 index)
{
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + ENEMY_POOL_OFF);
    s32 alg = e->algorithm;
    f32 rad = e->rad;
    f32 hht = e->hht;
    s32 blocked = 0;
    /* stun freeze + knockback integration */
                half[0] = oldpos[0] + e->trans[0];
                rad2 = (f32)(rad * 1.5);
                if (*(u32*)((u8*)e->coll_ip + 100) != 0) {
                    first();
                }
                if (*(u32*)((u8*)e->coll_ip + 100) != 0) {
                    route((f32*)((u8*)e->coll_ip + 52));
                }
}
"""

    def test_only_later_route_uses_new_owner(self):
        text = dict(source_probe.movement_reload_variants(self.BODY))["collision_owner_route"]
        self.assertEqual(text.count("const Enemy* contactOwner = e;"), 1)
        self.assertEqual(text.count("*(u32*)((u8*)e->coll_ip + 100)"), 2)
        self.assertIn("route((f32*)((u8*)contactOwner->coll_ip + 52));", text)
        self.assertLess(text.index("first();"), text.index("const Enemy* contactOwner"))
        self.assertNotIn("volatile", text)

    def test_radius_copy_scale_preserves_double_multiply_and_named_owner(self):
        text = dict(source_probe.movement_reload_variants(self.BODY))["owner_route_radius_copy_scale"]
        self.assertIn("rad2 = rad;\n                rad2 *= 1.5;\n                half[0] =", text)
        self.assertNotIn("1.5f", text)
        self.assertIn("route((f32*)((u8*)contactOwner->coll_ip + 52));", text)

    def test_missing_or_already_converted_guard_is_refused(self):
        with self.assertRaises(ValueError):
            list(source_probe.movement_reload_variants(self.BODY.replace("                if (*(u32*)((u8*)e->coll_ip + 100) != 0) {", "if (1) {", 1)))
        text = dict(source_probe.movement_reload_variants(self.BODY))["collision_owner_route"]
        with self.assertRaisesRegex(ValueError, "historical"):
            list(source_probe.movement_reload_variants(text))


class MovePageForms(unittest.TestCase):
    BODY = """void move_logic10(s32 index)
{
    type = *(s32*)(e0 + OFF_E(type));
    e = (Enemy*)(e0 + ENEMY_POOL_OFF);
    e0 += ENEMY_POOL_OFF;
    speed = read_speed(type);
    consume(e, e0);
}
"""

    def test_member_lowering_preserves_consumer_chronology(self):
        forms = dict(source_probe.move_page_variants(self.BODY))
        self.assertEqual(set(forms), {"array_then_copy", "array_twice", "array_and_byte_advance"})
        text = forms["array_twice"]
        self.assertIn("type = ((EnemyMovePage05*)e0)->enemies[0].type;", text)
        self.assertIn("e = ((EnemyMovePage05*)e0)->enemies;\n    e0 = (u8*)((EnemyMovePage05*)e0)->enemies;", text)
        self.assertTrue(text.endswith("    speed = read_speed(type);\n    consume(e, e0);\n}\n"))
        self.assertNotIn("typedef", text)
        self.assertNotIn("volatile", text)

    def test_already_converted_and_missing_alias_refuse(self):
        converted = dict(source_probe.move_page_variants(self.BODY))["array_twice"]
        with self.assertRaises(ValueError):
            list(source_probe.move_page_variants(converted))
        with self.assertRaises(ValueError):
            list(source_probe.move_page_variants(self.BODY.replace("    e0 += ENEMY_POOL_OFF;\n", "")))


class SpawnPoolForms(unittest.TestCase):
    BODY = """s32 generate_enemy(void)
{
    u8* tbl = lbl_8011AF48;
    s32 slot;
    s32 otype;
    Enemy* e;
    if (gGameMode == 0) return -1;
    use(lbl_802512B0[type], lbl_802511FC[type]);
    e = &gEnemies[slot];
    e->generator = gen;
    use(lbl_80251148[type]);
}
"""

    def test_complete_owner_and_preoffset_store_are_kept_together(self):
        forms = dict(source_probe.generate_pool_variants(self.BODY))
        text = forms["whole_pool_row_e_before_slot"]
        for symbol in ("lbl_80251148", "lbl_802511FC", "lbl_802512B0"):
            self.assertIn("pool->" + symbol + "[type]", text)
            self.assertIn("s32 " + symbol + "[45];", text)
        self.assertIn("slot * sizeof(Enemy)", text)
        self.assertIn("row->gEnemies[0].generator = gen;\n    e = row->gEnemies;", text)
        self.assertLess(text.index("    Enemy* e;"), text.index("    s32 slot;"))
        self.assertNotIn("volatile", text)

    def test_missing_owner_member_and_already_retained_shape_refuse(self):
        with self.assertRaisesRegex(ValueError, "missing spawn pool symbol"):
            list(source_probe.generate_pool_variants(self.BODY.replace("lbl_802511FC[type]", "0")))
        with self.assertRaisesRegex(ValueError, "historical"):
            list(source_probe.generate_pool_variants(self.BODY.replace("lbl_802511FC[type]", "pool->lbl_802511FC[type]")))


class MovementAddressForms(unittest.TestCase):
    BODY = """void do_enemy_move(s32 index)
{
    Enemy* e = (Enemy*)((u8*)lbl_80250E00 + index * 916 + ENEMY_POOL_OFF);
    s32 alg = e->algorithm;
    f32 rad = e->rad;
    f32 hht = e->hht;
    s32 blocked = 0;
    Enemy* other;
    /* stun freeze + knockback integration */
    use(e, alg, rad, hht, blocked);
}
"""

    def test_staged_assignments_follow_all_declarations_in_original_order(self):
        forms = dict(source_probe.movement_address_variants(self.BODY))
        text = forms["staged_self"]
        self.assertLess(text.index("    Enemy* other;"), text.index("    e ="))
        self.assertIn("e = (Enemy*)((u8*)e + ENEMY_POOL_OFF);", text)
        self.assertIn("alg = e->algorithm;\n    rad = e->rad;\n    hht = e->hht;\n    blocked = 0;", text)
        self.assertTrue(text.endswith("    use(e, alg, rad, hht, blocked);\n}\n"))
        self.assertNotIn("volatile", text)
        self.assertIn("e = (Enemy*)((u8*)other + ENEMY_POOL_OFF);", forms["staged_other"])

    def test_already_staged_and_missing_owner_refuse(self):
        retained = dict(source_probe.movement_address_variants(self.BODY))["staged_self"]
        with self.assertRaises(ValueError):
            list(source_probe.movement_address_variants(retained))
        with self.assertRaisesRegex(ValueError, "existing other"):
            list(source_probe.movement_address_variants(self.BODY.replace("    Enemy* other;\n", "")))


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
