"""Negative controls for the bounded auxscreen ownership evidence."""
import copy
import json
import unittest

from tools.gdl.composed_census import r68_aux_ownership_audit as audit
from tools.gdl.composed_census import r68_aux_bss_probe as probe


class InitializedObligationTests(unittest.TestCase):
    def setUp(self):
        self.data = b"".join(i.to_bytes(4, "big") for i in range(7))
        self.source = {"sections": {".sdata": {"type": 1, "alignment": 8,
                        "size": 28, "bytes": self.data.hex(), "relocations": []}},
                       "symbols": {n: {"section": ".sdata", "offset": i*4,
                        "size": 4, "binding": 1, "bytes": self.data[i*4:i*4+4].hex()}
                        for i, n in enumerate(audit.INITIALIZED)}}
        self.offsets = {n: i*4 for i, n in enumerate(audit.INITIALIZED)}

    def check(self, source=None, target=None):
        return audit.initialized_obligation(source or self.source,
                    target if target is not None else self.data + b"\0"*4, self.offsets)

    def test_complete_run(self):
        self.assertEqual(self.check()["status"], "PASS")

    def test_wrong_value_or_nonzero_slack(self):
        for index in (3, 31):
            target = bytearray(self.data + b"\0"*4)
            target[index] ^= 1
            with self.assertRaises(ValueError):
                self.check(target=target)

    def test_short_extent_and_hidden_relocation(self):
        for key, value in (("size", 24), ("alignment", 4), ("type", 8),
                           ("relocations", [(0, 1, "other", 0)])):
            source = copy.deepcopy(self.source)
            source["sections"][".sdata"][key] = value
            with self.assertRaises(ValueError):
                self.check(source=source)

    def test_missing_or_moved_definition(self):
        for key, value in (("offset", 4), ("size", 8), ("binding", 0)):
            source = copy.deepcopy(self.source)
            source["symbols"][audit.INITIALIZED[0]][key] = value
            with self.assertRaises(ValueError):
                self.check(source=source)
        del self.source["symbols"][audit.INITIALIZED[0]]
        with self.assertRaises(ValueError):
            self.check()

    def test_comparison_survives_json_roundtrip(self):
        source = {"f": dict(offset=0, size=4, body="12345678",
                            relocations=[[0, 1, "data", 0]], binding=1)}
        self.assertTrue(all(not rows for rows in audit.changed_functions(
            json.loads(json.dumps(source)), source).values()))
        source["f"]["relocations"][0][2] = "wrong"
        before = {"f": dict(source["f"], relocations=[[0, 1, "data", 0]])}
        self.assertEqual(audit.changed_functions(before, source)["relocations"], ["f"])


class BssObligationTests(unittest.TestCase):
    def setUp(self):
        self.source = {"sections": {".sbss": {"type": 8, "size": 92, "alignment": 8,
                        "bytes": None, "relocations": []}},
                       "symbols": {n: dict(section=".sbss", offset=i*4, size=4, binding=1)
                                   for i, n in enumerate(audit.BSS_ORDER)}}
        self.target = copy.deepcopy(self.source)
        self.target["sections"][".sbss"]["size"] = 96
        self.target["symbols"]["good_wiz_state"]["size"] = 8
        self.symbols = {n: (".sbss", 0x80344318+4*i, 8 if n == "good_wiz_state" else 4)
                        for i, n in enumerate(audit.BSS_ORDER)}

    def check(self):
        return audit.bss_obligation(self.source, self.target, self.symbols)

    def test_complete_bss_without_fake_bytes(self):
        self.assertTrue(self.check()["no_initialized_bytes"])

    def test_equal_zero_extents_do_not_prove_symbol_positions(self):
        self.source["symbols"]["map_fade_a"] = dict(section=".sbss", offset=0, size=4, binding=1)
        with self.assertRaises(ValueError):
            self.check()

    def test_wrong_offset_or_target_extent_refuses(self):
        self.source["symbols"]["map_route_blit"]["offset"] = 4
        with self.assertRaises(ValueError):
            self.check()
        self.source["symbols"]["map_route_blit"]["offset"] = 0
        self.target["symbols"]["good_wiz_state"]["size"] = 4
        with self.assertRaises(ValueError):
            self.check()

    def test_physical_byte_hash_is_not_bss_evidence(self):
        self.source["sections"][".sbss"]["bytes"] = "00" * 92
        with self.assertRaises(ValueError):
            self.check()


class BssSourceFormsTests(unittest.TestCase):
    def source(self):
        return b"\r\n".join(b"s32 " + n.encode() + b";" for n in probe.ORIGINAL_ORDER) + b"\r\nvoid f(void) { BODY; }\r\n"

    def test_both_axes_and_coupled_form_preserve_function(self):
        forms = probe.source_forms(self.source())
        self.assertEqual(forms["original"], self.source())
        for text in forms.values():
            self.assertTrue(text.endswith(b"void f(void) { BODY; }\r\n"))
        coupled = forms["target_reverse_and_foreign_extern"]
        self.assertNotIn(b"wiz_mode", coupled)
        self.assertIn(b"extern s32 good_wiz_enabled;", coupled)
        self.assertEqual(probe.source_forms(coupled)["original"], self.source())

    def test_unexpected_duplicate_or_referenced_wiz_refuses(self):
        for source in (self.source() + b"s32 map_route_blit;\r\n",
                       self.source().replace(b"BODY", b"wiz_mode = 1"),
                       self.source().replace(b"s32 movie_state;", b"s32 foreign;")):
            with self.assertRaises(ValueError):
                probe.source_forms(source)


if __name__ == "__main__":
    unittest.main()
