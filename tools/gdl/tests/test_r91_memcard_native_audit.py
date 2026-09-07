import copy
import unittest

from tools.gdl.composed_census import r91_memcard_native_audit as audit


class R91MemcardAuditTests(unittest.TestCase):
    def functions(self):
        target = dict(size=100, binding=1, body="00" * 100,
                      relocations=[(4, 109, "options", 0), (20, 10, "prompt", 0),
                                   (60, 109, "serial1", 0), (72, 109, "state", 0),
                                   (76, 109, "present", 0), (80, 109, "serial0", 0)])
        candidate = copy.deepcopy(target)
        candidate["relocations"] = [(o + 2 if k == 109 else o, k, n, a)
                                    for o, k, n, a in target["relocations"]]
        return candidate, target

    def test_native_named_bindings(self):
        audit.native_function(*self.functions())

    def test_wrong_word(self):
        candidate, target = self.functions()
        candidate["body"] = "01" + candidate["body"][2:]
        with self.assertRaisesRegex(ValueError, "native body"):
            audit.native_function(candidate, target)

    def test_wrong_named_datum(self):
        candidate, target = self.functions()
        candidate["relocations"][2] = (62, 109, "serial0", 0)
        with self.assertRaisesRegex(ValueError, "positional datum"):
            audit.native_function(candidate, target)

    def test_swapped_relocated_instruction(self):
        candidate, target = self.functions()
        candidate["relocations"][2] = (66, 109, "serial1", 0)
        with self.assertRaisesRegex(ValueError, "positional datum"):
            audit.native_function(candidate, target)

    def test_no_other_relocation_offset_normalization(self):
        candidate, target = self.functions()
        candidate["relocations"][1] = (22, 10, "prompt", 0)
        with self.assertRaisesRegex(ValueError, "positional datum"):
            audit.native_function(candidate, target)

    def test_missing_binding_and_strength(self):
        candidate, target = self.functions()
        candidate["relocations"].pop()
        with self.assertRaisesRegex(ValueError, "relocation census"):
            audit.native_function(candidate, target)
        candidate, target = self.functions()
        candidate["binding"] = 2
        with self.assertRaisesRegex(ValueError, "symbol strength"):
            audit.native_function(candidate, target)

    def test_only_one_rule_removed(self):
        before = {"units": {audit.UNIT: [{"function": audit.FN}, {"function": "sibling"}],
                            "foreign": [{"function": "unowned"}]}}
        after = copy.deepcopy(before)
        after["units"][audit.UNIT].pop(0)
        audit.rule_delta(before, after)
        after["units"]["foreign"] = []
        with self.assertRaisesRegex(ValueError, "rule delta"):
            audit.rule_delta(before, after)

    def test_only_existing_scalar_alias(self):
        before = "s32 memCardErrorPrompt(const char* msg)\n{\n        register s32 zero = 0;\n        lbl_80344A24 = zero;\n}\n/* next */"
        after = before.replace("        register s32 zero = 0;",
                               "        s32* serial = &lbl_80344A24;\n        register s32 zero = 0;")
        after = after.replace("        lbl_80344A24 = zero;", "        *serial = zero;")
        audit.source_delta(before, after)
        with self.assertRaisesRegex(ValueError, "source delta"):
            audit.source_delta(before, after.replace("&lbl_80344A24", "&lbl_80344A20"))


if __name__ == "__main__":
    unittest.main()
