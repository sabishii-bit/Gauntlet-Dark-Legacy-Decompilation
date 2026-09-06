import copy
import unittest

from tools.gdl.composed_census.r67_linked_shadow_audit import choose_rows, exact_linked_bytes


class LinkedShadowAuditTests(unittest.TestCase):
    def fixture(self):
        return {'schema_version': 1, 'status': 'PASS', 'relocation_comparison': {
            'source_linked_demotions': 1, 'changes': [
                {'unit': 'main/u', 'function': 'f', 'source_linked': True, 'lost_report_100': True},
                {'unit': 'main/v', 'function': 'g', 'source_linked': False, 'lost_report_100': True}]}}

    def test_fallback_function_never_gets_linked_credit(self):
        self.assertEqual([r['function'] for r in choose_rows(self.fixture())], ['f'])

    def test_failed_preflight_cannot_be_reused(self):
        value = self.fixture()
        value['status'] = 'FAIL'
        with self.assertRaises(ValueError):
            choose_rows(value)

    def test_duplicate_rows_refuse_even_with_consistent_count(self):
        value = self.fixture()
        value['relocation_comparison']['changes'].append(copy.deepcopy(value['relocation_comparison']['changes'][0]))
        value['relocation_comparison']['source_linked_demotions'] = 2
        with self.assertRaises(ValueError):
            choose_rows(value)

    def test_wrong_population_count_refuses(self):
        value = self.fixture()
        value['relocation_comparison']['source_linked_demotions'] = 2
        with self.assertRaises(ValueError):
            choose_rows(value)

    def test_empty_cohort_is_not_a_vacuous_pass(self):
        value = self.fixture()
        value['relocation_comparison'].update(changes=[], source_linked_demotions=0)
        with self.assertRaises(ValueError):
            choose_rows(value)

    def test_actual_linked_bytes_must_agree_three_ways(self):
        self.assertEqual(exact_linked_bytes(b'1234', b'1234', b'1234', 4), 'PASS')
        self.assertEqual(exact_linked_bytes(b'1234', b'1234', b'1235', 4), 'FAIL')
        self.assertEqual(exact_linked_bytes(b'1234', b'1235', b'1234', 4), 'FAIL')
        self.assertEqual(exact_linked_bytes(b'1235', b'1234', b'1234', 4), 'FAIL')

    def test_absence_or_zero_width_is_unresolved_not_equal(self):
        for triple, size in [((None, None, None), 4), ((b'', b'', b''), 0),
                             ((b'12', b'12', b'12'), 4)]:
            self.assertEqual(exact_linked_bytes(*triple, size), 'UNRESOLVED')


if __name__ == '__main__':
    unittest.main()
