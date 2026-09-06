import copy
import unittest

from tools.gdl.composed_census import r77_tree_retirement_audit as tool
from tools.gdl.tests.test_r76_tree_helper_reconstruction import SOURCE, fixture


class TreeRetirementTests(unittest.TestCase):
    def test_shared_source_only_and_comments_allowed(self):
        source = tool.prior.reconstruct(SOURCE, 'shared')
        tool.check_source(SOURCE, '/* Approved compatibility */\n'+source)
        for bad in (source.replace('AllocMem(0x80)', 'AllocMem(0x84)'),
                    tool.prior.reconstruct(SOURCE, 'duplicate'), SOURCE):
            with self.assertRaises(ValueError):
                tool.check_source(SOURCE, bad)

    def test_only_one_rule_removal_allowed(self):
        before = {'units': {tool.prior.UNIT: [{'function': 'MBNewNode'}], 'other': [1]}}
        tool.check_rules(before, {'units': {'other': [1]}})
        for bad in (before, {'units': {}}, {'units': {'other': [2]}}):
            with self.assertRaises(ValueError):
                tool.check_rules(before, bad)

    def test_certificate_retires_one_and_preserves_all_inventory(self):
        inputs = fixture()
        saved = copy.deepcopy(inputs)
        result = tool.certify(*inputs)
        self.assertEqual(result['rules_retired'], 1)
        self.assertEqual(result['differing_words_after'], 0)
        self.assertEqual(result['status'], 'SOURCE_NATIVE_RETIREMENT')
        self.assertEqual(inputs, saved)
        inputs[1]['sections']['.sdata2']['bytes'] = '00000000'
        with self.assertRaises(ValueError):
            tool.certify(*inputs)


if __name__ == '__main__':
    unittest.main()
