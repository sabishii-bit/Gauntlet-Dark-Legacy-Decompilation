"""Private-binary-independent bounds for the finite pad identity counterexample."""
import copy
import unittest

from tools.gdl.composed_census import r71_pad_identity_probe as probe


def inventories():
    p = bytearray(192)
    c = bytearray(192)
    for offset, pair in probe.FRAME_PAIRS.items():
        p[offset:offset+4], c[offset:offset+4] = map(bytes.fromhex, pair)
    fn = dict(offset=0, size=192, body=p.hex(), relocations=[], binding=1)
    sibling = dict(offset=192, size=4, body="4e800020", relocations=[], binding=1)
    before = dict(functions={probe.FUNCTION: fn, "Sibling": sibling},
                  sections={".data": dict(size=4, alignment=4, type=1, flags=3,
                                           bytes="12345678", relocations=[]),
                            "extabindex": dict(size=0, alignment=4, type=1, flags=2,
                                                bytes="", relocations=[])},
                  exception_records={"parsed_fixture": ["same-resolved-record"]})
    after = copy.deepcopy(before)
    after["functions"][probe.FUNCTION]["body"] = c.hex()
    return before, after


class IdentityProbeTests(unittest.TestCase):
    def test_strict_source_preconditions(self):
        self.assertEqual(probe.replace_once("abc", "b", "x"), "axc")
        for original in ("aaa", "abc"):
            with self.assertRaises(ValueError):
                probe.replace_once(original, "a" if original == "aaa" else "z", "x")

    def test_five_frame_words_are_partial_not_exact(self):
        result = probe.partial_shape_witness(*inventories())
        self.assertEqual(result["status"], "PASS")
        self.assertFalse(result["full_match"])
        self.assertFalse(result["production_rule_retired"])

    def test_nonframe_word_or_count_change_refused(self):
        for key, value in (("size", 188), ("body", "00000000"),
                           ("body", "01000000" + inventories()[1]["functions"][probe.FUNCTION]["body"][8:])):
            before, after = inventories()
            after["functions"][probe.FUNCTION][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                probe.partial_shape_witness(before, after)

    def test_sibling_and_relocation_drift_refused(self):
        for name, key, value in (("Sibling", "body", "48000000"),
                                 ("Sibling", "offset", 196),
                                 (probe.FUNCTION, "relocations", [[24, 4, "foreign", 0]])):
            before, after = inventories()
            after["functions"][name][key] = value
            with self.subTest(name=name, key=key), self.assertRaises(ValueError):
                probe.partial_shape_witness(before, after)

    def test_extra_function_and_changed_data_or_eh_refused(self):
        for mode in ("extra", "data", "eh", "empty_eh"):
            before, after = inventories()
            if mode == "extra":
                after["functions"]["extra"] = after["functions"]["Sibling"]
            elif mode == "data":
                after["sections"][".data"]["bytes"] = "87654321"
            elif mode == "eh":
                after["exception_records"] = {"different": []}
            else:
                before["exception_records"] = after["exception_records"] = {}
            with self.subTest(mode=mode), self.assertRaises(ValueError):
                probe.partial_shape_witness(before, after)

    def test_target_manager_payload_requires_exact_named_binding(self):
        target, candidate = inventories()
        target_body = bytearray.fromhex(target["functions"][probe.FUNCTION]["body"])
        candidate_body = bytearray.fromhex(candidate["functions"][probe.FUNCTION]["body"])
        for offset, tw, cw in ((4, "3c608029", "3c600000"), (24, "3be3645c", "3be30000")):
            target_body[offset:offset+4], candidate_body[offset:offset+4] = bytes.fromhex(tw), bytes.fromhex(cw)
        target["functions"][probe.FUNCTION]["body"] = target_body.hex()
        candidate["functions"][probe.FUNCTION]["body"] = candidate_body.hex()
        candidate["functions"][probe.FUNCTION]["relocations"] = [
            [6, 6, "gPadManager", 0], [26, 4, "gPadManager", 0], [36, 10, "G3DGetPadStatusBuffer", 0]]
        symbols = "gPadManager = .bss:0x8029645C; // type:object size:0x1C scope:global"
        self.assertEqual(probe.bound_target_address(target, candidate, symbols)["address"], "0x8029645c")
        with self.assertRaises(ValueError):
            probe.bound_target_address(target, candidate, symbols.replace("8029645C", "80296460"))
        candidate["functions"][probe.FUNCTION]["relocations"][1][3] = 4
        with self.assertRaises(ValueError):
            probe.bound_target_address(target, candidate, symbols)


if __name__ == "__main__":
    unittest.main()
