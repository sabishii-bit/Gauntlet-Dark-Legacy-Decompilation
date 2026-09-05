"""Pure guards for the R61 scratch-only source experiments."""
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "composed_census"))
import r61_enemy_probe as probe


class AddressVariants(unittest.TestCase):
    def body(self):
        return """void do_enemy_move(s32 index)
{
    Enemy* e;
    s32 alg;
    f32 rad;
    f32 hht;
    s32 blocked;
    Enemy* other;
    f32 mat[16];
    u8 unused4[12];
""" + probe.ENTRY + """
    alg = e->algorithm;
    rad = e->rad;
    hht = e->hht;
    blocked = 0;
""" + probe.GUARD + """
        e->trans[0] = 0.0f;
        e->trans[1] = 0.0f;
        e->trans[2] = 0.0f;
    }
}
"""

    def test_timer_capture_updates_from_the_same_read(self):
        variants = dict(probe.timer_variants(self.body()))
        captured = variants["s32_const_condition"]
        self.assertIn("(timer = ((const Enemy*)e)->stun_timer)", captured)
        self.assertIn("e->stun_timer = timer - gFrameTicks;", captured)
        self.assertNotIn("e->stun_timer -=", captured)
        self.assertIn(probe.ENTRY, captured)
        self.assertNotIn("volatile", captured)

    def test_all_axes_generate_unique_names_and_balanced_functions(self):
        body = self.body()
        for generator in (probe.timer_variants, probe.optimizer_joint_variants,
                          probe.lookup_variants, probe.first_use_variants,
                          probe.carrier_variants, probe.allocation_variants,
                          probe.address_tree_variants):
            rows = list(generator(body))
            self.assertEqual(len(rows), len({name for name, _ in rows}))
            for name, text in rows:
                with self.subTest(generator=generator.__name__, name=name):
                    self.assertEqual(text.count("{"), text.count("}"))
                    self.assertEqual(text.count("void do_enemy_move("), 1)
                    self.assertNotIn("asm ", text)

    def test_pragma_controls_restore_their_option(self):
        variants = dict(probe.optimizer_joint_variants(self.body()))
        for name in ("opt_propagation", "opt_common_subs", "opt_lifetimes",
                     "scheduling", "peephole"):
            self.assertTrue(variants[name].startswith("#pragma " + name + " off"))
            self.assertTrue(variants[name].endswith("#pragma " + name + " reset\n"))

    def test_changed_live_source_refuses_instead_of_silently_testing_wrong_text(self):
        body = self.body().replace("index * sizeof(Enemy)", "index * 1024")
        with self.assertRaises(ValueError):
            list(probe.address_tree_variants(body))

    def test_assignment_tree_controls_do_not_add_a_side_effect(self):
        variants = dict(probe.address_tree_variants(self.body()))
        self.assertIn("index * sizeof(Enemy) + (u8*)lbl_80250E00",
                      variants["commuted"])
        self.assertNotIn("volatile", variants["commuted"])
        self.assertEqual(variants["commuted"].count("stun_timer"), 2)


class PoolGuards(unittest.TestCase):
    def test_literalization_stops_before_next_tu_and_preserves_other_symbols(self):
        source = """extern f64 lbl_80346810;
extern f64 lbl_80346850;
extern f32 lbl_80346AB8;
void f(void) {
    a = lbl_80346810;
    b = *(volatile f64*)&lbl_80346850;
    c = lbl_80346AB8;
}
"""
        with patch.object(probe, "literal_for", side_effect=
                          lambda name, *args: "1.0" if name.endswith("6810") else "-3.141592654"):
            result = probe.literal_pool(source)
        self.assertNotIn("lbl_80346810", result)
        self.assertNotIn("lbl_80346850", result)
        self.assertIn("b = (-3.141592654);", result)
        self.assertIn("extern f32 lbl_80346AB8;", result)
        self.assertIn("c = lbl_80346AB8;", result)
        self.assertEqual(probe.POOL_END - probe.POOL_START, 668)

    def test_bss_comparison_ignores_undefined_and_absolute_symbols(self):
        sections = [SimpleNamespace(name=""), SimpleNamespace(name=".bss"),
                    SimpleNamespace(name=".text")]
        symbols = [
            SimpleNamespace(name="pool", value=64, size=692, section_index=1),
            SimpleNamespace(name="undefined", value=0, size=4, section_index=0),
            SimpleNamespace(name="absolute", value=0, size=4, section_index=0xfff1),
            SimpleNamespace(name="function", value=0, size=4, section_index=2),
        ]
        with patch.object(probe.wf, "_sections", return_value=sections), \
             patch.object(probe.wf, "_symbols", return_value=symbols):
            self.assertEqual(probe.bss_symbols(b""), {"pool": (64, 692)})


if __name__ == "__main__":
    unittest.main()
