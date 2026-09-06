"""Fixed-witness and experiment-integrity controls; no compiler required."""
import unittest

from tools.gdl.composed_census import r70_dbgtext_lifetimes as audit


def witness_fixture():
    # Independently captured complete [+0x7c,+0x1c4) intervals. Bytes outside
    # the interval are synthetic; these tests do not claim whole-CFG proof.
    target = bytearray(0x1C4)
    raw = bytearray(0x1C4)
    target[0x7C:] = bytes.fromhex(
        "3a0000003a6000003b0000003e800001800000007c80c21482a400102c15000041800114"
        "806000003813000c3a4013127c03002e280000005411b2be418200102811000040820008"
        "3a2000011c110064806400144cc631827ce0f3963800000038bd00003911000054041838"
        "38da004048000001806000003815000b38bd00004cc631827cc3c2148066001454041838"
        "480000011eb10060380013127ed5039740810040480000013800001ec020000054041838"
        "3a2300003884000138bf0001480000017c9593963871000038a000044800000138710000"
        "389401014800000180600000381800142c1600007ec3002e4081003c480000013800001e"
        "c02000003a2300005404183838bf0002480000017c9593963871000038a0000448000001"
        "3871000038960000480000013bbd00083bff00083a1000012c10004a3a7300103b18001c"
        "4180fecc")
    raw[0x7C:] = bytes.fromhex(
        "3ac000003a6000003a4000003f200001800000007c809214830400102c18000041800114"
        "806000003813000c3aa013127c03002e280000005414b2be418200102814000040820008"
        "3a8000011c140064806400144cc631827ce0d3963800000038be00003914000054041838"
        "38dc004048000001806000003818000b38be00004cc631827cc392148066001454041838"
        "480000011f140060380013127e18039740810040480000013800001ec020000054041838"
        "3a2300003884000138bf0001480000017c98ab963871000038a000044800000138710000"
        "389901014800000180600000381200142c1000007e23002e4081003c480000013800001e"
        "c02000003a0300005404183838bf0002480000017c98ab963870000038a0000448000001"
        "3870000038910000480000013bde00083bff00083ad600012c16004a3a7300103a52001c"
        "4180fecc")
    return target, raw


class DbgtextLifetimeTests(unittest.TestCase):
    def test_fixed_counter_witness_binds_complete_intervals(self):
        result = audit.reviewed_loop_witness(*witness_fixture())
        self.assertEqual(result["function_offset_range"], ["0x7c", "0x1c4"])
        self.assertEqual(result["counter_sites"][0]["raw"], "li r22,0")
        self.assertIn("not a promotion", result["conclusion"])

    def test_every_intervening_word_mutation_is_rejected(self):
        target, raw = witness_fixture()
        for side in (0, 1):
            for offset in range(0x7C, 0x1C4, 4):
                with self.subTest(side=side, offset=hex(offset)):
                    pair = [bytearray(target), bytearray(raw)]
                    pair[side][offset + 3] ^= 1
                    with self.assertRaisesRegex(ValueError, "fixed reviewed loop witness"):
                        audit.reviewed_loop_witness(*pair)

    def test_truncated_witness_rejected(self):
        target, raw = witness_fixture()
        with self.assertRaises(ValueError):
            audit.reviewed_loop_witness(target, raw[:-4])

    def test_emb_sda21_only_offset_normalization(self):
        rows = [(2, 109, "x", 0), (6, 6, "y", 4)]
        self.assertEqual(audit.canonical_relocations(rows), [(0, 109, "x", 0), (6, 6, "y", 4)])
        self.assertNotEqual(audit.canonical_relocations(rows), [(0, 109, "different", 0), (6, 6, "y", 4)])

    def test_failed_or_missing_compile_cannot_report_pass(self):
        self.assertEqual(audit.completed_status({}), "FAIL")
        self.assertEqual(audit.completed_status({"x": {"error": None}}), "FAIL")
        self.assertEqual(audit.completed_status({"x": {"error": "failed", "inventory": {}}}), "FAIL")
        self.assertEqual(audit.completed_status({"x": {"error": None, "inventory": {}}}), "PASS")

    def test_registered_forms_keep_unselected_siblings_and_signature(self):
        source = (audit.ROOT / "src/game/pb/dbgtext.c").read_text(encoding="utf-8")
        forms = audit.source_forms(source)
        prefix, _, suffix = audit.split_body(source)
        for name in ("i_per_mode", "i_quad_joint", "mode3_row_counter", "locals_int", "quad_per_allocation"):
            a, body, b = audit.split_body(forms[name])
            self.assertEqual((a, b), (prefix, suffix))
            self.assertTrue(body.startswith("s32 fn_800C03E0(s32 mode)"))
        self.assertIn("s32 i3;", forms["i_mode3"])
        self.assertNotIn("s32 i2;", forms["i_mode3"])
        self.assertIn("s32 fixedIndex;", forms["bind_distinct_counter"])

    def test_source_drift_cannot_silently_create_noop_probe(self):
        source = (audit.ROOT / "src/game/pb/dbgtext.c").read_text(encoding="utf-8")
        for changed in (source.replace("    u32 div;", "    unsigned int div;"),
                        source + "\nextern s32 dbgTextActive;\n"):
            with self.assertRaisesRegex(ValueError, "source-form anchor changed"):
                audit.source_forms(changed)


if __name__ == "__main__":
    unittest.main()
