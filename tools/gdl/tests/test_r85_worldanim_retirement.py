"""Negative controls for the manual world-animation retirement certificate."""
import copy
import tempfile
from pathlib import Path
import unittest

from tools.gdl.composed_census import r85_worldanim_retirement_audit as audit


def inventories():
    zero='00000000'*63
    exact='00000001'*13+'00000000'*50
    bindings=[[i*4+2,109,'symbol'+str(i),0] for i in range(12)]
    def inv(body):
        return dict(functions={audit.FN:dict(body=body,size=252,offset=0,relocations=copy.deepcopy(bindings)),
                               'sibling':dict(body='01020304',size=4,offset=252,relocations=[])},
                    sections={'.text':dict(bytes=body+'01020304',relocations=copy.deepcopy(bindings)),
                              '.data':dict(bytes='12345678',relocations=[])},
                    symbols={'owned':dict(section='.data',offset=0,size=4)},
                    all_symbols=[['owned','.data',0,4,17,0]],exception_records=[['test',32]])
    before=inv(zero); after=inv(exact); target=inv(exact)
    for row in target['functions'][audit.FN]['relocations']:
        row[0]-=2
    return before,copy.deepcopy(after),after,copy.deepcopy(after),target


class WorldAnimationRetirementTests(unittest.TestCase):
    def test_exact_control(self):
        result=audit.allocation_proof(*inventories())
        self.assertEqual(result['raw_repaired_words'],13)
        self.assertEqual(result['target_positionally_bound_relocations'],12)
        self.assertEqual(result['raw_siblings_unchanged'],1)

    def test_raw_collateral_rejected(self):
        for field in ('sibling','outside_text','data','symbol','symbol_info','EH','relocation'):
            with self.subTest(field=field):
                values=inventories(); changed=values[2]
                if field=='sibling': changed['functions']['sibling']['body']='12345678'
                elif field=='outside_text': changed['sections']['.text']['bytes']+='00000000'
                elif field=='data': changed['sections']['.data']['bytes']='87654321'
                elif field=='symbol': changed['symbols']['owned']['offset']=4
                elif field=='symbol_info': changed['all_symbols'][0][4]=16
                elif field=='EH': changed['exception_records'][0][1]=40
                else: changed['functions'][audit.FN]['relocations'][0][2]='wrong'
                with self.assertRaises(ValueError): audit.allocation_proof(*values)

    def test_processed_drift_rejected(self):
        values=inventories(); values[3]['sections']['.data']['bytes']='00000000'
        with self.assertRaisesRegex(ValueError,'processed allocated'): audit.allocation_proof(*values)

    def test_wrong_target_body_rejected(self):
        values=inventories(); values[4]['functions'][audit.FN]['body']='00000000'*63
        with self.assertRaisesRegex(ValueError,'native byte exact'): audit.allocation_proof(*values)

    def test_binding_fields_preserved(self):
        for field,value in enumerate((52,108,'wrong',4)):
            with self.subTest(field=field):
                values=inventories(); values[4]['functions'][audit.FN]['relocations'][0][field]=value
                with self.assertRaisesRegex(ValueError,'positional relocation'): audit.allocation_proof(*values)

    def test_instruction_relocation_transposition_rejected(self):
        values=inventories(); rel=values[4]['functions'][audit.FN]['relocations']
        rel[0][2],rel[1][2]=rel[1][2],rel[0][2]
        with self.assertRaisesRegex(ValueError,'positional relocation'): audit.allocation_proof(*values)

    def test_only_sda_offset_convention_normalized(self):
        values=inventories()
        for i in (2,3): values[i]['functions'][audit.FN]['relocations'][0][1]=4
        values[0]['functions'][audit.FN]['relocations'][0][1]=4
        values[1]['functions'][audit.FN]['relocations'][0][1]=4
        values[4]['functions'][audit.FN]['relocations'][0][1]=4
        with self.assertRaisesRegex(ValueError,'positional relocation'): audit.allocation_proof(*values)

    def test_rule_delta(self):
        before={'units':{audit.UNIT:[{'function':audit.FN},{'function':'keep','hash':'unchanged'}]}}
        after={'units':{audit.UNIT:[{'function':'keep','hash':'unchanged'}]}}
        audit.rule_delta(before,after)
        after['units'][audit.UNIT][0]['hash']='changed'
        with self.assertRaises(ValueError): audit.rule_delta(before,after)

    def test_original_foreign_change_still_rejected(self):
        before={'units':{audit.UNIT:[{'function':audit.FN}],'foreign':[{'function':'old'}]}}
        after={'units':{audit.UNIT:[],'foreign':[]}}
        with self.assertRaisesRegex(ValueError,'one pin retirement'): audit.rule_delta(before,after)

    def test_current_foreign_integration_is_reported_not_certified(self):
        original={'version':1,'units':{audit.UNIT:[{'function':'keep'}],'foreign':[{'function':'old'}]}}
        current=copy.deepcopy(original); current['units']['foreign']=[]
        self.assertEqual(audit.current_rule_scope(original,current),
                         [dict(unit='foreign',certified_here=False,original_functions=['old'],current_functions=[])])

    def test_current_owned_unit_extra_deletion_rejected(self):
        original={'units':{audit.UNIT:[{'function':'keep'}]}}
        current={'units':{audit.UNIT:[]}}
        with self.assertRaisesRegex(ValueError,'owned-unit'): audit.current_rule_scope(original,current)

    def test_top_level_config_drift_rejected(self):
        original={'version':1,'units':{audit.UNIT:[]}}
        current=copy.deepcopy(original); current['version']=2
        with self.assertRaisesRegex(ValueError,'top-level'): audit.current_rule_scope(original,current)

    def test_missing_duplicate_and_extra_rule_rejected(self):
        for rows in ([],[{'function':audit.FN}]*2):
            with self.subTest(rows=rows),self.assertRaises(ValueError):
                audit.rule_delta({'units':{audit.UNIT:rows}},{'units':{audit.UNIT:[]}})
        with self.assertRaises(ValueError):
            audit.rule_delta({'units':{audit.UNIT:[{'function':audit.FN}]}},
                             {'units':{audit.UNIT:[{'function':'new'}]}})

    def test_untrusted_manifest_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)
            (path/'r85_worldanim_manifest.json').write_text('{}')
            with self.assertRaisesRegex(ValueError,'untrusted control manifest'): audit.audit(path)


if __name__=='__main__':
    unittest.main()
