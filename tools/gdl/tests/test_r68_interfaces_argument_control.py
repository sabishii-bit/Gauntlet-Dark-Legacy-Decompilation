"""Independent negative controls for the bounded missing-argument certificate."""
import copy
import struct
import unittest

from tools.gdl.composed_census.r68_interfaces_argument_control import (
    CONSUMER, FUNCTION, PRODUCER, audit, gpr_writes, interval)
from tools.gdl.tests.test_r68_interfaces_raw_control import snapshot


def function(words):
    return dict(body=struct.pack('>' + 'I' * len(words), *words).hex(), offset=0,
                size=len(words) * 4, binding=1,
                relocations=[[0, 10, PRODUCER, 0], [(len(words) - 1) * 4, 10, CONSUMER, 0]])


class ArgumentControlTests(unittest.TestCase):
    def test_returned_slot_is_preserved(self):
        fn = function([0x48000001, 0x7C641B79, 0xD0040000, 0x4C401382, 0x48000001])
        self.assertTrue(interval(fn)['preserves_slot_to_consumer'])

    def test_missing_copy_and_clobber_are_not_certified(self):
        fn = function([0x48000001, 0x2C030000, 0x7C932214, 0x48000001])
        self.assertFalse(interval(fn)['preserves_slot_to_consumer'])
        fn = function([0x48000001, 0x7C641B79, 0x38800000, 0x48000001])
        self.assertFalse(interval(fn)['preserves_slot_to_consumer'])

    def test_immediate_r4_bit_pattern_does_not_fake_a_write(self):
        # lwz r3,0x2000(r5): immediate bits are not a destination register.
        self.assertEqual(gpr_writes(0x80652000), {3})

    def test_update_forms_and_logical_ra_destinations_are_writes(self):
        self.assertIn(4, gpr_writes(0x84640004))  # lwzu r3,4(r4)
        self.assertIn(4, gpr_writes(0x94640004))  # stwu r3,4(r4)
        self.assertIn(4, gpr_writes(0x6C640000))  # xoris r4,r3,0

    def test_unknown_call_and_ambiguous_call_sites_refused(self):
        for word in (0, 0x48000001, 0x4E800020):
            with self.subTest(word=word):
                with self.assertRaises(ValueError):
                    interval(function([0x48000001, 0x7C641B79, word, 0x48000001]))
        fn = function([0x48000001, 0x7C641B79, 0x48000001])
        fn['relocations'].append([0, 10, PRODUCER, 0])
        with self.assertRaises(ValueError):
            interval(fn)

    def test_whole_tu_isolation_and_real_defect_required(self):
        old = function([0x48000001, 0x2C030000, 0x7C932214, 0x48000001])
        new = function([0x48000001, 0x7C641B79, 0x7C731A14, 0x48000001])
        before = snapshot()
        before['functions'] = {FUNCTION: old}
        before['sections']['.text']['bytes'] = old['body']
        before['sections']['.text']['size'] = old['size']
        after = copy.deepcopy(before)
        after['functions'][FUNCTION] = new
        after['sections']['.text']['bytes'] = new['body']
        self.assertEqual(audit(before, after, {FUNCTION: new})['status'], 'PASS')
        after['sections']['extab']['bytes'] = '00000001'
        self.assertEqual(audit(before, after, {FUNCTION: new})['status'], 'FAIL')
        with self.assertRaises(ValueError):
            audit(after, after, {FUNCTION: new})


if __name__ == '__main__':
    unittest.main()
