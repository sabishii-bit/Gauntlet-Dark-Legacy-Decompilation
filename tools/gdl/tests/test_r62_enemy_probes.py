"""Pure source-shape and full-image audit guards; no original game required."""
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "composed_census"))
import r62_enemy_pool_probe as pool
import r62_enemy_link_audit as link
import r62_address_fold_census as fold
import test_r61_enemy_probe as fixtures


class SourceGuards(unittest.TestCase):
    def test_rename_does_not_change_member_names(self):
        source = fixtures.AddressVariants().body()
        rows = dict(pool.variants(source, "current19"))
        self.assertIn("radius = e->rad;", rows["radius"])
        self.assertIn("height = e->hht;", rows["height"])
        self.assertNotIn("e->radius", rows["radius"])
        self.assertNotIn("e->height", rows["height"])

    def test_parameter_linkage_controls_preserve_function_content(self):
        source = "void do_enemy_move(s32 index);\n" + fixtures.AddressVariants().body()
        rows = dict(pool.variants(source, "current22"))
        self.assertEqual(rows["static"].count("static void do_enemy_move"), 2)
        self.assertEqual(rows["int_parameter"].count("void do_enemy_move(int index)"), 2)
        self.assertIn("e->stun_timer -= gFrameTicks", rows["static"])

    def test_int_local_control_preserves_pointer_argument_type(self):
        source = fixtures.AddressVariants().body().replace("    s32 alg;", "    s32 hitWorld;\n    s32 alg;")
        row = dict(pool.variants(source, "current22"))["int_locals"]
        self.assertIn("    s32 hitWorld;", row)
        self.assertIn("    int alg;", row)

    def test_edit_refuses_missing_function(self):
        with self.assertRaises((ValueError, SystemExit)):
            pool.edit_function("void x(void) {}", "do_enemy_move", lambda b: b)


class AddressFold(unittest.TestCase):
    ours = [0x7c7f0214, 0x3bc30e18, 0x80631024]
    target = [0x7fdf0214, 0x3bde0e18, 0x807e020c]

    def test_manifest_is_authority_not_directory_contents(self):
        row = {"target_path": "build/GUNE5D/obj/game/enemy/enemy.o"}
        self.assertEqual(fold.configured_units({"units": [row]}), ["game/enemy/enemy"])
        with self.assertRaisesRegex(ValueError, "duplicate"):
            fold.configured_units({"units": [row, row]})
        with self.assertRaisesRegex(ValueError, "unexpected"):
            fold.configured_units({"units": [{"target_path": "stale/enemy.o"}]})
        with self.assertRaisesRegex(ValueError, "invalid"):
            fold.configured_units({"units": [{"target_path": "build/GUNE5D/obj/../enemy.o"}]})

    def test_both_directions_and_equal_control(self):
        self.assertEqual(fold.prove_window(self.ours, self.target), "ours_folded")
        self.assertEqual(fold.prove_window(self.target, self.ours), "target_folded")
        self.assertEqual(fold.prove_window(self.target, self.target), "already_equal")
        self.assertEqual(fold.shape(self.ours), "folded")
        self.assertEqual(fold.shape(self.target), "unfolded")

    def test_modular_affine_arithmetic(self):
        self.assertEqual(fold.add((("constant", -1),), (("constant", 1),)), ())
        self.assertEqual(fold.add((("r3", 1),), (("r3", 1),)), (("r3", 2),))

    def test_wrong_displacement_and_live_out_refuse(self):
        wrong_load = self.target[:2] + [self.target[2] + 4]
        with self.assertRaisesRegex(ValueError, "effective addresses"):
            fold.prove_window(self.ours, wrong_load)
        # Same address loaded into r4 instead of r3 leaves the temp r3 live.
        wrong_destination = self.target[:2] + [self.target[2] + (1 << 21)]
        with self.assertRaisesRegex(ValueError, "final register"):
            fold.prove_window(self.ours, wrong_destination)

    def test_unmodelled_side_effects_and_zero_base_refuse(self):
        for bit in (1, 1 << 10):  # Rc and OE each have architectural effects.
            with self.assertRaises(ValueError):
                fold.prove_window([self.ours[0] | bit] + self.ours[1:], self.target)
        for slot in (1, 2):
            words = self.ours.copy()
            words[slot] &= ~(31 << 16)
            with self.assertRaisesRegex(ValueError, "RA-zero"):
                fold.prove_window(words, self.target)

    def test_window_boundary_checks(self):
        words = [0x60000000] + self.target + [0x4e800020]
        fold.boundary_check(words, 4, {}, {})
        with self.assertRaisesRegex(ValueError, "relocation inside"):
            fold.boundary_check(words, 4, {8: "x"}, {})
        with self.assertRaisesRegex(ValueError, "address-taken"):
            fold.boundary_check(words, 4, {}, {8})
        words[0] = 0x48000008  # b into the addi rather than the add.
        with self.assertRaisesRegex(ValueError, "enters window interior"):
            fold.boundary_check(words, 4, {}, {})


class LinkAudit(unittest.TestCase):
    def dol(self):
        result = bytearray(320)
        struct.pack_into(">I", result, 0, 256)
        struct.pack_into(">I", result, 0x48, 0x80003100)
        struct.pack_into(">I", result, 0x90, 64)
        return result

    def test_exact_image(self):
        image = self.dol()
        result = link.compare(image, image)
        self.assertTrue(result["whole_file_equal"])
        self.assertEqual(result["differing_section_bytes"], 0)
        self.assertEqual(len(result["sections"]), 1)

    def test_word_offsets_and_counts(self):
        expected = self.dol()
        built = expected.copy()
        built[256] = 1
        built[259] = 2
        built[264] = 3
        result = link.compare(expected, built)
        section = result["sections"][0]
        self.assertEqual(section["differing_bytes"], 3)
        self.assertEqual(section["differing_words"], 2)
        self.assertEqual(section["words"][1]["address"], "0x80003108")
        self.assertFalse(result["whole_file_equal"])

    def test_header_difference_is_not_hidden(self):
        expected = self.dol()
        built = expected.copy()
        built[0xE0] = 1
        result = link.compare(expected, built)
        self.assertEqual(result["differing_section_bytes"], 0)
        self.assertEqual(result["whole_file_differing_bytes"], 1)
        self.assertFalse(result["header_equal"])
        self.assertFalse(result["whole_file_equal"])

    def test_layout_difference_is_reported_before_position_pairing(self):
        expected = self.dol()
        built = expected.copy()
        struct.pack_into(">I", built, 0x48, 0x80003104)
        self.assertFalse(link.compare(expected, built)["layout_equal"])

    def test_bad_extent_refuses(self):
        image = self.dol()
        struct.pack_into(">I", image, 0x90, 128)
        with self.assertRaises(ValueError):
            link.compare(image, image)
        with self.assertRaises(ValueError):
            link.compare(b"", b"")


if __name__ == "__main__":
    unittest.main()
