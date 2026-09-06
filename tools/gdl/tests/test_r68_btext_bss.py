import copy
import unittest
from tools.gdl.composed_census.r68_btext_bss_audit import BASE, END, OBJECTS, obligation


class BtextBssTests(unittest.TestCase):
    def setUp(self):
        self.snapshot = dict(sections={".bss": dict(type=8, flags=3, size=END-BASE, alignment=8, bytes=None)},
                             relocations={}, symbols=[])
        self.target = {}
        offset = 0
        for name, size in OBJECTS:
            self.snapshot["symbols"].append(dict(name=name, section=".bss", type=1, value=offset, size=size, binding=1))
            self.target[name] = (".bss", BASE+offset, size)
            offset += size

    def test_complete_layout(self):
        self.assertEqual(len(obligation(self.snapshot, self.target)), 5)

    def test_wrong_position_size_or_binding_refused(self):
        for key, value in (("value", 4), ("size", 4), ("binding", 0)):
            changed = copy.deepcopy(self.snapshot)
            changed["symbols"][0][key] = value
            with self.assertRaises(ValueError):
                obligation(changed, self.target)

    def test_missing_symbol_and_section_slack_refused(self):
        for action in (lambda s: s["symbols"].pop(),
                       lambda s: s["sections"][".bss"].update(size=END-BASE+4),
                       lambda s: s["relocations"].update({".bss": [(0, 1, "x", 0)]})):
            changed = copy.deepcopy(self.snapshot)
            action(changed)
            with self.assertRaises(ValueError):
                obligation(changed, self.target)

    def test_wrong_target_boundary_refused(self):
        self.target["font_info"] = (".bss", BASE+4, 0x38)
        with self.assertRaises(ValueError):
            obligation(self.snapshot, self.target)
