import unittest
from tools.gdl.composed_census.r79_newcam_retirement_audit import (
    ROOT, checked_output, trusted_manifest, verify_envelope, verify_relocations,
)


class NewcamRetirementAuditTests(unittest.TestCase):
    def test_untrusted_manifest_cannot_change_the_claimed_edge(self):
        with self.assertRaisesRegex(ValueError, 'manifest hash'):
            trusted_manifest(b'{"edge":{"cflags":"-O0"}}')

    def test_only_exact_function_replacement_is_allowed(self):
        before = b'header' + bytes(468) + b'data symbols relocs EH'
        target = bytes([1]) * 468
        after = before[:6] + target + before[474:]
        verify_envelope(before, after, target, 6)
        for offset in (0, 6, 400, 474, len(after) - 1):
            bad = bytearray(after)
            bad[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                verify_envelope(before, bytes(bad), target, 6)
        with self.assertRaises(ValueError):
            verify_envelope(before, after + b'extra', target, 6)
        with self.assertRaises(ValueError):
            verify_envelope(before, after, target[:-1], 6)
        with self.assertRaises(ValueError):
            verify_envelope(before, after, target, -1)

    def test_positional_relocations_bind_symbol_type_and_addend(self):
        target = [[i * 4, 109, 'symbol' + str(i), 0] for i in range(11)]
        actual = [[off + 2, kind, symbol, addend] for off, kind, symbol, addend in target]
        verify_relocations(actual, target)
        for field, value in [(0, 8), (1, 10), (2, 'wrong'), (3, 4)]:
            bad = [row.copy() for row in actual]
            bad[0][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                verify_relocations(bad, target)
        with self.assertRaises(ValueError):
            verify_relocations(actual[:-1], target)

    def test_output_stays_inside_lane_build(self):
        self.assertEqual(checked_output(ROOT / 'build/r79_newcam_test.json'), ROOT / 'build/r79_newcam_test.json')
        for path in [ROOT / 'src/r79_newcam_test.json', ROOT / 'build/foreign.json', ROOT / 'build/../r79_newcam_test.json']:
            with self.assertRaises(ValueError):
                checked_output(path)


if __name__ == '__main__':
    unittest.main()
