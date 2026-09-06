"""Two-sided checks for the finite audio-helper experiment and audit gates."""
import copy
import unittest

from tools.gdl.composed_census import r74_audio_helper_probe as probe


class AudioHelperProbeTests(unittest.TestCase):
    def test_lookup_name_tracks_the_pdb_part_signature(self):
        self.assertIn('static inline s32 AudioFindPart(char* bankName)', probe.HELPER)
        self.assertNotIn('AudioFindBank', probe.HELPER)

    def test_source_snapshot_refuses_drift(self):
        with self.assertRaisesRegex(ValueError, 'snapshot changed'):
            probe.source_forms(b'void AudioBankQueueName(void) {}')

    def test_fidelity_requires_complete_bytes_and_success(self):
        probe.require_fidelity(b'complete', b'complete')
        for actual,error in ((b'complet',None),(None,None),(b'complete','compile failed')):
            with self.assertRaises(ValueError):
                probe.require_fidelity(actual,b'complete',error)

    def test_function_span_requires_definition_not_prototype(self):
        source = 'void F(void);\nvoid F(void)\n{\n    if (1) { }\n}\nvoid G(void) {}'
        a,b = probe.function_span(source,'F')
        self.assertEqual(source[a:b],'void F(void)\n{\n    if (1) { }\n}')
        with self.assertRaises(ValueError):
            probe.function_span(source,'Missing')

    @staticmethod
    def fixture():
        raw = b'\x60\0\0\0'*20
        before = dict(functions={},sections={'.text':dict(bytes=(raw*2).hex()),
                                             '.rodata':dict(bytes='01020304')},
                      all_symbols=['binding'],exception_records=['EH'])
        target = dict(functions={})
        for i,name in enumerate(probe.FUNCTIONS[:2]):
            row = dict(offset=i*len(raw),size=len(raw),body=raw.hex(),
                       relocations=[[2,109,'global',0]])
            before['functions'][name] = row
            new = bytearray(raw)
            new[0x34:0x38] = b'\x3b\xbc\0\0'
            target['functions'][name] = dict(row,body=new.hex(),relocations=[[0,109,'global',0]])
        after = copy.deepcopy(before)
        after['sections']['.text']['bytes'] = ''.join(target['functions'][n]['body'] for n in probe.FUNCTIONS[:2])
        for name in probe.FUNCTIONS[:2]:
            after['functions'][name]['body'] = target['functions'][name]['body']
        return before,after,target

    def test_audit_accepts_only_the_two_target_word_changes(self):
        before,after,target = self.fixture()
        row = probe.audit_retirement(before,after,target,{'processed':1},{'processed':1})
        self.assertEqual(row['raw_changed_words'],2)
        self.assertEqual(row['retired_bytes'],160)
        self.assertTrue(row['target_positional_relocations_equal'])

    def test_audit_rejects_sibling_data_eh_binding_and_processed_drift(self):
        for axis in ('data','EH','binding','body','processed'):
            with self.subTest(axis=axis):
                before,after,target = self.fixture()
                old_processed,new_processed = {'processed':1},{'processed':1}
                if axis=='data':
                    after['sections']['.rodata']['bytes']='01020305'
                elif axis=='EH':
                    after['exception_records']=['changed EH']
                elif axis=='binding':
                    target['functions'][probe.FUNCTIONS[0]]['relocations'][0][2]='wrong'
                elif axis=='body':
                    after['functions'][probe.FUNCTIONS[0]]['body']='00'
                else:
                    new_processed={'processed':2}
                with self.assertRaises(ValueError):
                    probe.audit_retirement(before,after,target,old_processed,new_processed)

    def test_audit_rejects_wrong_baseline_residual(self):
        before,after,target = self.fixture()
        before['functions'][probe.FUNCTIONS[0]]['body'] = target['functions'][probe.FUNCTIONS[0]]['body']
        with self.assertRaisesRegex(ValueError,'reviewed one-word'):
            probe.audit_retirement(before,after,target,{}, {})


if __name__ == '__main__':
    unittest.main()
