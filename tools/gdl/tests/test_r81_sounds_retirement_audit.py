import copy
import unittest

from tools.gdl.composed_census.r81_sounds_retirement_audit import FN, verify


class SoundsRetirementTests(unittest.TestCase):
    def setUp(self):
        body = '60000000' * 68
        before_body = '60000001' + body[8:]
        relocs = [[i * 4, 10, 'callee', 0] for i in range(12)]
        function = dict(offset=4, size=272, body=body, relocations=relocs, binding=1)
        after = dict(functions={FN: function, 'sibling': {'body': '11223344'}},
                     sections={'.text': {'bytes': '11223344' + body}, '.data': {'bytes': 'aabbccdd'}},
                     all_symbols=[['data', '.data', 0, 4, 17, 0]],
                     exception_records={FN: {'frame': 8}})
        before = copy.deepcopy(after)
        before['functions'][FN]['body'] = before_body
        before['sections']['.text']['bytes'] = '11223344' + before_body
        self.data = dict(before=before, after=after, processed_before=copy.deepcopy(after), target=copy.deepcopy(after))

    def test_valid_native_substitution(self):
        self.assertEqual(verify(**self.data)['raw_words'], 0)

    def test_wrong_body(self):
        self.data['after']['functions'][FN]['body'] = '00000000' * 68
        with self.assertRaisesRegex(ValueError, 'raw target body'):
            verify(**self.data)

    def test_count_change(self):
        self.data['after']['functions'][FN]['size'] += 4
        with self.assertRaisesRegex(ValueError, 'extent'):
            verify(**self.data)

    def test_relocation_offset(self):
        self.data['after']['functions'][FN]['relocations'][0][0] += 4
        with self.assertRaisesRegex(ValueError, 'relocation'):
            verify(**self.data)

    def test_relocation_symbol(self):
        self.data['after']['functions'][FN]['relocations'][0][2] = 'other'
        with self.assertRaisesRegex(ValueError, 'relocation'):
            verify(**self.data)

    def test_relocation_addend(self):
        self.data['after']['functions'][FN]['relocations'][0][3] = 4
        with self.assertRaisesRegex(ValueError, 'relocation'):
            verify(**self.data)

    def test_relocation_kind(self):
        self.data['after']['functions'][FN]['relocations'][0][1] = 4
        with self.assertRaisesRegex(ValueError, 'relocation'):
            verify(**self.data)

    def test_sibling_change(self):
        self.data['after']['functions']['sibling']['body'] = '00000000'
        with self.assertRaisesRegex(ValueError, 'raw envelope'):
            verify(**self.data)

    def test_data_change(self):
        self.data['after']['sections']['.data']['bytes'] = '00000000'
        with self.assertRaisesRegex(ValueError, 'raw envelope'):
            verify(**self.data)

    def test_eh_change(self):
        self.data['after']['exception_records'][FN]['frame'] = 16
        with self.assertRaisesRegex(ValueError, 'raw envelope'):
            verify(**self.data)

    def test_symbol_change(self):
        self.data['after']['all_symbols'][0][2] = 4
        with self.assertRaisesRegex(ValueError, 'raw envelope'):
            verify(**self.data)

    def test_previous_processed_state_matters(self):
        self.data['processed_before']['sections']['.data']['bytes'] = '00000000'
        with self.assertRaisesRegex(ValueError, 'prior processed'):
            verify(**self.data)


if __name__ == '__main__':
    unittest.main()
