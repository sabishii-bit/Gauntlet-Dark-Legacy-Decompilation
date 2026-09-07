import copy
import unittest

from tools.gdl.composed_census import r88_movie_literal_boundary_audit as audit


class MovieBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.before = {audit.FN: dict(body='00000000', size=4, binding=1),
                       'sibling': dict(body='11111111', size=4, binding=1)}
        self.target = copy.deepcopy(self.before)
        self.target[audit.FN]['body'] = '22222222'
        self.parts = [{audit.FN: self.target[audit.FN]}, {'sibling': self.before['sibling']}]

    def test_conditional_native_bodies(self):
        self.assertEqual(audit.check_functions(self.before, self.target, self.parts), 1)

    def test_sibling_drift_refused(self):
        parts = copy.deepcopy(self.parts)
        parts[1]['sibling']['body'] = 'ffffffff'
        with self.assertRaisesRegex(ValueError, 'sibling body'):
            audit.check_functions(self.before, self.target, parts)

    def test_weak_global_delete_style_binding_change_refused(self):
        parts = copy.deepcopy(self.parts)
        parts[1]['sibling']['binding'] = 2
        with self.assertRaisesRegex(ValueError, 'symbol binding'):
            audit.check_functions(self.before, self.target, parts)

    def test_duplicate_definitions_refused(self):
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            audit.check_functions(self.before, self.target, self.parts + [self.parts[0]])

    def test_omitted_function_refused(self):
        with self.assertRaisesRegex(ValueError, 'roster'):
            audit.check_functions(self.before, self.target, self.parts[:1])

    def test_opaque_eh_is_not_just_size(self):
        with self.assertRaisesRegex(ValueError, 'opaque bytes'):
            audit.check_opaque_eh({'extab': 'aabb'}, [{'extab': 'aacc'}])

    def test_first_suffix_entry_is_not_monotone_boundary(self):
        rows = [('early', 10, 100), ('later', 20, 300), ('delete', 30, 200)]
        self.assertEqual(audit.crossing_entries(rows, 30), [('later', 'delete')])
        self.assertEqual(audit.crossing_entries(rows, 20), [])

    def test_same_literals_wrong_order_are_not_pool_match(self):
        self.assertEqual(len(audit.POOL), len(audit.BIAS_FIRST_POOL))
        self.assertNotEqual(audit.POOL, audit.BIAS_FIRST_POOL)


if __name__ == '__main__':
    unittest.main()
