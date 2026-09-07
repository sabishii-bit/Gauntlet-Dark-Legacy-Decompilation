import unittest

from tools.gdl.composed_census.r87_enemy_reset_order_probe import STORES, TAIL, source_controls

BODY = '''void fn_80051164(void)
{
    s32* p = lbl_80250E00;
    s32 i;
    for (i = 0; i < 45; i++) {
''' + '\n'.join(STORES) + '''
    }
    for (i = 0; i < 8; i++) {
        s32* row = p + i;
        row[8] = -1;
    }
''' + TAIL + '\n}'


class EnemyResetOrderTests(unittest.TestCase):
    def test_six_orders_are_complete_and_unique(self):
        forms = source_controls(BODY)
        self.assertEqual(len(forms), 6)
        self.assertEqual(len(set(forms.values())), 6)
        self.assertEqual(forms['stores_012'], BODY)
        for body in forms.values():
            self.assertCountEqual(body.splitlines(), BODY.splitlines())
            self.assertIn(TAIL, body)
            for line in STORES:
                self.assertEqual(body.count(line), 1)

    def test_tail_joint_changes_only_tail(self):
        forms = source_controls(BODY, True)
        self.assertEqual(len(forms), 12)
        for name, body in source_controls(BODY).items():
            expected = body.replace(TAIL, '\n'.join(reversed(TAIL.splitlines())))
            self.assertEqual(forms[name+'_tail_reverse'], expected)

    def test_ranges_are_pairwise_disjoint(self):
        ranges = [set(range(base, base+45)) for base in (345, 300, 255)]
        ranges.append(set(range(8, 16)))
        for i, left in enumerate(ranges):
            for right in ranges[i+1:]:
                self.assertFalse(left & right)

    def test_source_drift_refused(self):
        for changed in (BODY.replace('i < 45', 'i < 44').replace(STORES[0], ''),
                        BODY.replace(TAIL, ''), BODY+'\n'+TAIL):
            with self.assertRaises(ValueError):
                source_controls(changed)


if __name__ == '__main__':
    unittest.main()
