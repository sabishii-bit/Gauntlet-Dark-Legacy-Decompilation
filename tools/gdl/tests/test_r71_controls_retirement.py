"""Two-sided checks for the manual InitControls source-retirement certificate."""
import copy
import unittest

from tools.gdl.composed_census import r71_controls_retirement_audit as audit
from tools.gdl.composed_census import r71_controls_identity as probes


def fixture():
    fn=dict(size=108,body='00'*108,offset=0,binding=1,
            relocations=[[6,6,'...bss.0',0],[86,109,'ctrls_initialized',0]])
    inv=dict(functions={audit.FN:fn,'sibling':dict(size=4,body='11223344',offset=108,binding=1,relocations=[])},
             all_symbols=[['...bss.0','.bss',0,0,0,0]],symbols={},exception_records=[],
             sections={'.text':dict(bytes='00'*112),'.data':dict(bytes='00000001')})
    addresses={}
    for i,suffix in enumerate(('B8','C8','D8','E8','F8')):
        name='lbl_802407'+suffix
        inv['symbols'][repr(name)]=dict(section='.bss',offset=16*i,size=16)
        addresses[name]=0x802407B8+16*i
    target=copy.deepcopy(inv)
    target['all_symbols']=[['lbl_802407B8','',0,0,16,0]]
    target['functions'][audit.FN]['relocations']=[[6,6,'lbl_802407B8',0],[84,109,'ctrls_initialized',0]]
    old=dict(schema_version=1,fidelity=True,source=audit.OLD,
             edge=dict(mw='GC/1.2.5',cflags='flags',command_template='compile',rule='mwcc'),
             hashes=dict(compiler='compiler',target='target'),
             config={'units':{audit.UNIT:[{'function':audit.FN},{'function':'other'}]}},
             inventories=dict(raw=copy.deepcopy(inv),processed=copy.deepcopy(inv),target=target),
             pad_array_addresses=addresses)
    new=copy.deepcopy(old)
    new['source']=audit.NEW
    new['config']['units'][audit.UNIT]=[{'function':'other'}]
    return old,new


class RetirementAuditTests(unittest.TestCase):
    def test_valid_and_sda_halfword_normalization(self):
        old,new=fixture()
        self.assertEqual(audit.audit(old,new)['status'],'PASS')

    def reject(self,mutate):
        old,new=fixture()
        mutate(new)
        with self.assertRaises((ValueError,KeyError)):
            audit.audit(old,new)

    def test_source_scaffold_is_not_accepted(self):
        self.reject(lambda n:n.update(source=audit.NEW+'\nint unrelated;'))

    def test_compiler_drift(self):
        self.reject(lambda n:n['hashes'].update(compiler='different'))

    def test_unknown_schema(self):
        self.reject(lambda n:n.update(schema_version=2))

    def test_rule_remaining(self):
        self.reject(lambda n:n['config']['units'][audit.UNIT].append({'function':audit.FN}))

    def test_changed_processed_text(self):
        self.reject(lambda n:n['inventories']['processed']['sections']['.text'].update(bytes='bad'))

    def test_raw_sibling(self):
        self.reject(lambda n:n['inventories']['raw']['functions']['sibling'].update(body='bad'))

    def test_raw_exception_metadata(self):
        self.reject(lambda n:n['inventories']['raw'].update(exception_records=['bad']))

    def test_raw_nontext(self):
        self.reject(lambda n:n['inventories']['raw']['sections']['.data'].update(bytes='bad'))

    def test_nonexact_function(self):
        self.reject(lambda n:n['inventories']['raw']['functions'][audit.FN].update(body='ff'*108))

    def test_named_bss_mapping(self):
        self.reject(lambda n:n['pad_array_addresses'].update(lbl_802407F8=0x802407F4))

    def test_wrong_target_relocation(self):
        self.reject(lambda n:n['inventories']['target']['functions'][audit.FN]['relocations'][0].__setitem__(2,'wrong'))

    def test_nonzero_target_addend(self):
        self.reject(lambda n:n['inventories']['target']['functions'][audit.FN]['relocations'][0].__setitem__(3,4))

    def test_experiments_accept_both_active_spellings(self):
        suffix='\n/* 0x8003480C after */'
        a=probes.forms(audit.OLD+suffix)
        b=probes.forms(audit.NEW+suffix)
        self.assertEqual(a,b)
        self.assertIn(audit.NEW,a['existing_helper'])

    def test_experiments_reject_unreviewed_source(self):
        with self.assertRaises(ValueError):
            probes.forms('void InitControls(void) {}')


if __name__=='__main__':
    unittest.main()
