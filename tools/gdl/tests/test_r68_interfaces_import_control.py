"""Name-remapping controls refuse effects beyond exact undefined imports."""
import copy
import unittest

from tools.gdl.composed_census.r68_interfaces_import_control import audit
from tools.gdl.tests.test_r68_interfaces_raw_control import snapshot


class ImportControlTests(unittest.TestCase):
    def setUp(self):
        self.before = snapshot()
        self.before['symbols'].append(dict(name='callee', value=0, size=0, type=0,
                                           binding=1, other=0, section=''))
        self.after = copy.deepcopy(self.before)
        self.after['symbols'][-1]['name'] = 'callee_80001234'
        self.after['functions']['Public']['relocations'][0][2] = 'callee_80001234'
        self.after['relocations']['.text'][0][2] = 'callee_80001234'
        self.mapping = {'callee': 'callee_80001234'}
        self.target = {'Public': {'relocations': [[12, 10, 'callee_80001234', 0]]}}

    def result(self):
        return audit(self.before, self.after, self.mapping, self.target)

    def test_exact_undefined_import_rename_passes(self):
        result = self.result()
        self.assertEqual(result['status'], 'PASS')
        self.assertEqual(result['import_renames']['callee']['target_witnesses'],
                         [('Public', 12, 10, 0)])

    def test_extra_body_or_addend_changes_fail(self):
        self.after['functions']['Public']['body'] = 'abcd'
        self.assertEqual(self.result()['status'], 'FAIL')
        self.after['functions']['Public']['body'] = '1234'
        self.after['relocations']['.text'][0][3] = 4
        self.assertEqual(self.result()['status'], 'FAIL')

    def test_defined_symbol_rename_refused(self):
        self.before['symbols'][-1]['section'] = '.data'
        with self.assertRaises(ValueError):
            self.result()

    def test_existing_destination_stale_name_or_no_witness_refused(self):
        for which in ('existing', 'stale', 'witness'):
            with self.subTest(which=which):
                before, after, target = copy.deepcopy((self.before, self.after, self.target))
                if which == 'existing':
                    before['symbols'].append(copy.deepcopy(after['symbols'][-1]))
                elif which == 'stale':
                    after['symbols'].append(copy.deepcopy(before['symbols'][-1]))
                else:
                    target = {}
                with self.assertRaises(ValueError):
                    audit(before, after, self.mapping, target)

    def test_empty_overlapping_or_many_to_one_mapping_refused(self):
        for mapping in ({}, {'callee': 'callee'}, {'a': 'x', 'b': 'x'}):
            with self.subTest(mapping=mapping):
                with self.assertRaises(ValueError):
                    audit(self.before, self.after, mapping, self.target)

    def test_witness_in_wrong_function_or_wrong_type_is_refused(self):
        with self.assertRaises(ValueError):
            audit(self.before, self.after, self.mapping, {'Unrelated': self.target['Public']})
        self.target['Public']['relocations'][0][1] = 1
        with self.assertRaises(ValueError):
            self.result()


if __name__ == '__main__':
    unittest.main()
