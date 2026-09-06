import copy
import unittest

from tools.gdl.composed_census.r76_anim_retirement_audit import audit, K, MAGIC


def fixture():
    body = '00' * 424
    fn = dict(body=body, size=424, offset=0,
              relocations=[[0, 109, 'lbl_803457C0', 0],
                           [4, 109, ['.sdata2', 0, 8, 1, 0], 0]])
    pool = dict(bytes=MAGIC, size=8, alignment=8, flags=3, type=1, relocations=[])
    a = dict(functions={'InitAnim': fn, 'sibling': dict(body='12345678', size=4, offset=424, relocations=[])},
             sections={'.text': dict(bytes=body, relocations=copy.deepcopy(fn['relocations'])),
                       '.sdata2': pool}, exception_records={},
             all_symbols=[['lbl_803457C0', '', 0, 0, 16, 0],
                          [['.sdata2', 0, 8, 1, 0], '.sdata2', 0, 8, 1, 0]])
    b = copy.deepcopy(a)
    b['functions']['InitAnim']['relocations'] = [[0, 109, ['.sdata2', 0, 8, 1, 0], 0],
                                                [4, 109, ['.sdata2', 8, 8, 1, 0], 0]]
    b['sections']['.text']['relocations'] = copy.deepcopy(b['functions']['InitAnim']['relocations'])
    b['sections']['.sdata2'].update(bytes=K+MAGIC, size=16)
    b['all_symbols'] = [[['.sdata2', off, 8, 1, 0], '.sdata2', off, 8, 1, 0] for off in (0, 8)]
    return a, b, copy.deepcopy(b)


class AnimRetirementTests(unittest.TestCase):
    def test_exact_ownership_move(self):
        a, b, target = fixture()
        result = audit(a, b, target)
        self.assertEqual(result['status'], 'PASS')
        self.assertEqual(result['source_owned_new_bytes'], 8)

    def test_wrong_pool_value_or_order(self):
        a, b, target = fixture()
        b['sections']['.sdata2']['bytes'] = MAGIC+K
        with self.assertRaisesRegex(ValueError, 'pool extent/payload'):
            audit(a, b, target)

    def test_sibling_change_refused(self):
        a, b, target = fixture()
        b['functions']['sibling']['body'] = '11111111'
        with self.assertRaisesRegex(ValueError, 'function text/layout'):
            audit(a, b, target)

    def test_wrong_absolute_pool_binding_refused(self):
        a, b, target = fixture()
        b['functions']['InitAnim']['relocations'][0][2][1] = 8
        with self.assertRaisesRegex(ValueError, 'function bindings'):
            audit(a, b, target)

    def test_target_body_refused(self):
        a, b, target = fixture()
        target['functions']['InitAnim']['body'] = '11' * 424
        with self.assertRaisesRegex(ValueError, 'target body'):
            audit(a, b, target)

    def test_extra_section_refused(self):
        a, b, target = fixture()
        b['sections']['.data'] = {}
        with self.assertRaisesRegex(ValueError, 'section roster'):
            audit(a, b, target)

    def test_exception_change_refused(self):
        a, b, target = fixture()
        b['exception_records']['InitAnim'] = {'bad': True}
        with self.assertRaisesRegex(ValueError, 'exception metadata'):
            audit(a, b, target)

    def test_symbol_and_flags_refused(self):
        for change in ('flags', 'symbol'):
            a, b, target = fixture()
            if change == 'flags':
                b['sections']['.sdata2']['flags'] = 7
            else:
                b['all_symbols'][0][3] = 4
            with self.assertRaises(ValueError):
                audit(a, b, target)


if __name__ == '__main__':
    unittest.main()
