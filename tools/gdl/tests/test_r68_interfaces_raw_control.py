"""Private-binary-independent failure controls for interface visibility audits."""
import copy
import unittest

from tools.gdl.composed_census.r68_interfaces_raw_control import compare


def snapshot():
    return dict(fidelity=True, unit='game/world/example', compiler='GC/1.2.5',
                flags='-O4', raw_sha256='before', functions={
                    'Public': dict(body='1234', relocations=[[0, 10, 'callee', 0]],
                                   offset=0, size=4, binding=0),
                    'Private': dict(body='5678', relocations=[], offset=4, size=4, binding=0)},
                sections={'.text': dict(type=1, flags=6, size=8, alignment=4, bytes='12345678'),
                          '.bss': dict(type=8, flags=3, size=32, alignment=8, bytes=None),
                          'extab': dict(type=1, flags=2, size=4, alignment=4, bytes='00000000')},
                relocations={'.text': [[0, 10, 'callee', 0]], 'extabindex': [[0, 1, 'Public', 0]]},
                symbols=[dict(name='Public', value=0, size=4, type=2, binding=0, other=0, section='.text'),
                         dict(name='Private', value=4, size=4, type=2, binding=0, other=0, section='.text')])


class VisibilityComparisonTests(unittest.TestCase):
    def setUp(self):
        self.before = snapshot()
        self.after = copy.deepcopy(self.before)
        self.after['raw_sha256'] = 'after'
        self.after['functions']['Public']['binding'] = 1
        self.after['symbols'][0]['binding'] = 1

    def result(self):
        return compare(self.before, self.after, ['Public'])

    def test_binding_only_and_symbol_order_are_allowed(self):
        self.after['symbols'].reverse()
        result = self.result()
        self.assertEqual(result['status'], 'PASS')
        self.assertEqual(result['functions'], 2)
        self.assertEqual(result['relocations'], 2)
        self.assertEqual(result['exports']['Public'], dict(before=0, after=1))

    def test_function_body_layout_or_relocation_drift_fails(self):
        for field, value in [('body', 'abcd'), ('offset', 8), ('size', 8),
                             ('relocations', [[0, 10, 'different', 0]])]:
            with self.subTest(field=field):
                changed = copy.deepcopy(self.after)
                changed['functions']['Private'][field] = value
                self.assertEqual(compare(self.before, changed, ['Public'])['status'], 'FAIL')

    def test_eh_relocation_data_and_bss_layout_drift_fail(self):
        for section, field, value in [('extab', 'bytes', '00000001'), ('.text', 'bytes', 'deadbeef'),
                                      ('.bss', 'size', 36), ('.bss', 'alignment', 16)]:
            with self.subTest(section=section, field=field):
                changed = copy.deepcopy(self.after)
                changed['sections'][section][field] = value
                self.assertEqual(compare(self.before, changed, ['Public'])['status'], 'FAIL')
        self.after['relocations']['extabindex'][0][3] = 4
        self.assertIn('relocations', self.result()['changes'])

    def test_unrequested_binding_or_symbol_changes_fail(self):
        self.after['functions']['Private']['binding'] = 1
        self.assertEqual(self.result()['status'], 'FAIL')
        self.after = copy.deepcopy(self.before)
        self.after['symbols'][1]['other'] = 1
        self.assertIn('other symbol changes', self.result()['changes'])

    def test_missing_requested_export_or_roster_change_fails(self):
        self.assertEqual(compare(self.before, self.after, ['Missing'])['status'], 'FAIL')
        del self.after['functions']['Private']
        self.assertIn('function roster', self.result()['changes'])

    def test_empty_roster_fails(self):
        self.before['functions'] = self.after['functions'] = {}
        self.assertEqual(compare(self.before, self.after, [])['status'], 'FAIL')

    def test_already_global_and_wrong_binding_direction_fail(self):
        self.before['functions']['Public']['binding'] = 1
        self.assertEqual(self.result()['status'], 'FAIL')
        self.after['functions']['Public']['binding'] = 0
        self.assertEqual(self.result()['status'], 'FAIL')

    def test_unfaithful_or_different_control_refused(self):
        for key, value in [('fidelity', False), ('unit', 'other'), ('compiler', 'GC/1.2.5s'),
                           ('flags', '-O0')]:
            with self.subTest(key=key):
                changed = copy.deepcopy(self.after)
                changed[key] = value
                with self.assertRaises(ValueError):
                    compare(self.before, changed, ['Public'])


if __name__ == '__main__':
    unittest.main()
