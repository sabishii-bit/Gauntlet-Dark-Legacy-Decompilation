import copy
import unittest

from tools.gdl.composed_census.r89_texture_retirement_audit import FN, UNIT, verify


class TextureRetirementAuditTests(unittest.TestCase):
    def setUp(self):
        function = dict(size=260, body='60000000'*65,
                        relocations=[[4*i,10,'callee',0] for i in range(8)])
        raw = dict(functions={FN:function}, symbols={'symbol':1},
                   all_symbols=['symbol'], exception_records=['eh'],
                   sections={'.text':'old', '.rodata':'datum'})
        raw['functions'].update({f'sibling{i}':dict(size=4,body='4e800020',relocations=[])
                                 for i in range(18)})
        self.before = dict(raw=raw,processed=copy.deepcopy(raw))
        self.after = copy.deepcopy(self.before)
        self.target = copy.deepcopy(raw)
        self.old_rules = dict(units={UNIT:[dict(function=FN),dict(function='sibling')]})
        self.new_rules = dict(units={UNIT:[dict(function='sibling')]})

    def check(self):
        return verify(self.before,self.after,self.target,self.old_rules,self.new_rules)

    def test_positive(self):
        self.assertEqual(self.check()['named_target_relocations'],8)

    def test_raw_sibling_refusal(self):
        self.after['raw']['functions']['sibling0']['body']='60000000'
        with self.assertRaisesRegex(ValueError,'sibling'):
            self.check()

    def test_target_binding_refusal(self):
        self.target['functions'][FN]['relocations'][0][2]='wrong'
        with self.assertRaisesRegex(ValueError,'bindings'):
            self.check()

    def test_processed_datum_refusal(self):
        self.after['processed']['sections']['.rodata']='wrong'
        with self.assertRaisesRegex(ValueError,'processed'):
            self.check()

    def test_raw_exception_refusal(self):
        self.after['raw']['exception_records']=['wrong']
        with self.assertRaisesRegex(ValueError,'metadata'):
            self.check()

    def test_other_rule_refusal(self):
        self.new_rules['units'][UNIT]=[]
        with self.assertRaisesRegex(ValueError,'configuration'):
            self.check()

    def test_empty_roster_refusal(self):
        self.after['raw']['functions']={}
        with self.assertRaisesRegex(ValueError,'roster'):
            self.check()


if __name__=='__main__':
    unittest.main()
