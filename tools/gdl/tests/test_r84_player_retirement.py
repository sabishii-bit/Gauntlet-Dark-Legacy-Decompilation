"""Two-sided tests of the manual ExpToLevel retirement certificate."""
import copy
import unittest
from tools.gdl.composed_census import r84_player_retirement_audit as audit


def inventory():
    return dict(functions={'changed':dict(offset=0,size=4,body='01020304',relocations=[]),'sibling':dict(offset=4,size=4,body='05060708',relocations=[])},sections={'.text':dict(bytes='0102030405060708',relocations=[]),'.data':dict(bytes='00010203',relocations=[])},symbols={'datum':dict(offset=0,size=4)},all_symbols=[['datum','.data',0,4,17,0]],exception_records=[])


def rules():
    return {'units':{audit.UNIT:[{'function':audit.FN},{'function':'do_exit','before_sha256':'body','instruction_permutation':{'offset':496,'order':[1,0],'before_relocations_sha256':'old','after_relocations_sha256':'old'}}]}}


class RetirementTests(unittest.TestCase):
    def test_monotonic_changed_bits(self):
        self.assertEqual(audit.monotonic_words(bytes.fromhex('00000003'),bytes.fromhex('00000001'),bytes(4)),dict(changed_words=1,new_exact_words=0))

    def test_newly_exact_word(self):
        self.assertEqual(audit.monotonic_words(bytes.fromhex('00000003'),bytes(4),bytes(4)),dict(changed_words=1,new_exact_words=1))

    def test_new_wrong_bit_refused(self):
        with self.assertRaises(ValueError):
            audit.monotonic_words(bytes.fromhex('00000003'),bytes.fromhex('00000004'),bytes(4))

    def test_changed_exact_word_refused(self):
        with self.assertRaises(ValueError):
            audit.monotonic_words(bytes(4),bytes.fromhex('00000001'),bytes(4))

    def test_count_mismatch_refused(self):
        with self.assertRaises(ValueError):
            audit.monotonic_words(bytes(4),bytes(8),bytes(4))

    def pair(self):
        a=inventory(); b=copy.deepcopy(a)
        b['functions']['changed']['body']='11121314'
        b['sections']['.text']['bytes']='1112131405060708'
        return a,b

    def test_allowed_body_substitution(self):
        a,b=self.pair()
        audit.substitute_bodies(a,b,('changed',))

    def reject_inventory(self,mutate):
        a,b=self.pair(); mutate(b)
        with self.assertRaises((ValueError,KeyError)):
            audit.substitute_bodies(a,b,('changed',))

    def test_unowned_function_change(self):
        self.reject_inventory(lambda b:b['functions']['sibling'].update(body='ffffffff'))

    def test_text_padding_change(self):
        self.reject_inventory(lambda b:b['sections']['.text'].update(bytes='1112131405060709'))

    def test_function_relocation_change(self):
        self.reject_inventory(lambda b:b['functions']['changed'].update(relocations=[[0,109,'wrong',0]]))

    def test_section_relocation_change(self):
        self.reject_inventory(lambda b:b['sections']['.text'].update(relocations=[[0,109,'wrong',0]]))

    def test_data_change(self):
        self.reject_inventory(lambda b:b['sections']['.data'].update(bytes='ffffffff'))

    def test_datum_binding_change(self):
        self.reject_inventory(lambda b:b['all_symbols'][0].__setitem__(2,4))

    def test_exception_metadata_change(self):
        self.reject_inventory(lambda b:b.update(exception_records=['bad']))

    def test_function_offset_change(self):
        self.reject_inventory(lambda b:b['functions']['changed'].update(offset=4))

    def rule_pair(self):
        a=rules(); b=copy.deepcopy(a)
        b['units'][audit.UNIT].pop(0)
        b['units'][audit.UNIT][0]['instruction_permutation'].update(before_relocations_sha256='new',after_relocations_sha256='new')
        return a,b

    def test_exact_rule_removal_plus_name_hash_refresh(self):
        a,b=self.rule_pair(); audit.rule_delta(a,b)

    def test_added_rule_refused(self):
        a,b=self.rule_pair(); b['units'][audit.UNIT].append({'function':'new'})
        with self.assertRaises(ValueError): audit.rule_delta(a,b)

    def test_changed_permutation_order_refused(self):
        a,b=self.rule_pair(); b['units'][audit.UNIT][0]['instruction_permutation']['order']=[0,1]
        with self.assertRaises(ValueError): audit.rule_delta(a,b)

    def test_changed_body_hash_refused(self):
        a,b=self.rule_pair(); b['units'][audit.UNIT][0]['before_sha256']='newbody'
        with self.assertRaises(ValueError): audit.rule_delta(a,b)


if __name__=='__main__':
    unittest.main()
