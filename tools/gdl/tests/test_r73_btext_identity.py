"""Contract tests for the finite R73 FontInit source experiment."""
import copy
import unittest
from unittest.mock import patch

from tools.gdl.composed_census import r73_btext_identity_probe as probe


class SourceControls(unittest.TestCase):
    def setUp(self):
        self.source = ('extern s32 gScrollModes_80343BB0[2];\n'
                       'void StringInitSub(u32 mode, StrList* p) {}\n'
                       '#pragma opt_lifetimes off\n#pragma opt_propagation off\n'
                       +probe.OLD+'\n#pragma opt_propagation on\n#pragma opt_lifetimes reset\n')

    def forms(self, source=None):
        with patch.object(probe,'SOURCE_SHA',probe.sha(self.source.encode())):
            return probe.source_forms(self.source if source is None else source)

    def test_baseline_and_newline_control(self):
        self.assertEqual(self.forms()['baseline'],self.source)
        self.assertEqual(self.forms(self.source.replace('\n','\r\n')),self.forms())

    def test_changed_source_rejected(self):
        with self.assertRaises(ValueError):
            self.forms(self.source+'\n')

    def test_split_carriers_only_second_loop(self):
        forms=self.forms()
        split=forms['split_font_index']
        self.assertIn('for (; (s32)i < 2; i++, modeIndex++)',split)
        self.assertIn('for (fontIndex = 1; fontIndex < 0xd; fontIndex++)',split)
        self.assertIn('LoadFonts(fontIndex, gFontDefs8x8[fontIndex], gFontDefs[fontIndex])',split)
        self.assertIn('u32 fontIndex;',forms['scoped_font_index'])
        self.assertEqual(forms['both_scoped_indices'].count('u32 i;'),1)

    def test_type_axes_independent_and_joint(self):
        forms=self.forms()
        self.assertIn('extern int gScrollModes',forms['table_element_int'])
        self.assertIn('StringInitSub(u32 mode',forms['table_element_int'])
        self.assertIn('extern s32 gScrollModes',forms['callee_mode_uint'])
        self.assertIn('StringInitSub(unsigned int mode',forms['callee_mode_uint'])
        self.assertIn('extern int gScrollModes',forms['table_callee_type_joint'])
        self.assertIn('StringInitSub(unsigned int mode',forms['table_callee_type_joint'])

    def test_default_pragmas_and_natural_index_independent(self):
        forms=self.forms()
        independent=forms['single_index']
        self.assertNotIn('modeIndex',independent)
        self.assertIn('#pragma opt_lifetimes off',independent)
        self.assertIn('#pragma opt_propagation off',independent)
        both=forms['single_index_default_propagation_default_lifetimes']
        self.assertNotIn('#pragma',both)
        self.assertNotIn('modeIndex',both)
        self.assertIn('LoadFonts(i, gFontDefs8x8[i], gFontDefs[i])',both)
        self.assertNotIn('opt_propagation',forms['shared_index_default_propagation'])
        self.assertIn('modeIndex',forms['shared_index_default_propagation'])
        self.assertIn('opt_lifetimes',forms['shared_index_default_propagation'])

    def test_ambiguous_anchor_rejected_even_with_matching_digest(self):
        source=self.source+'extern s32 gScrollModes_80343BB0[2];\n'
        with patch.object(probe,'SOURCE_SHA',probe.sha(source.encode())):
            with self.assertRaises(ValueError):
                probe.source_forms(source)


class Measurements(unittest.TestCase):
    def setUp(self):
        self.inv=dict(functions={probe.FN:dict(body='3b600000',relocations=[[2,109,'datum',0]]),
                                 'sibling':dict(body='4e800020',relocations=[])},
                      sections={'.text':{},'.data':{'bytes':'12345678'},'.bss':{'size':8}},
                      all_symbols=['datum'],exception_records=['eh'])

    def measure(self, after=None, target=None):
        return probe.differences(self.inv,self.inv if after is None else after,
                                 self.inv if target is None else target)

    def test_equal_baseline_and_sda_convention(self):
        target=copy.deepcopy(self.inv)
        target['functions'][probe.FN]['relocations'][0][0]=0
        row=self.measure(target=target)
        self.assertEqual(row['differing_words'],0)
        self.assertTrue(row['target_relocation_bindings_equal'])

    def test_wrong_relocation_not_hidden_by_exact_words(self):
        target=copy.deepcopy(self.inv)
        target['functions'][probe.FN]['relocations'][0][2]='wrong'
        row=self.measure(target=target)
        self.assertEqual(row['differing_words'],0)
        self.assertFalse(row['target_relocation_bindings_equal'])

    def test_word_count_and_sibling_nontext_eh_changes(self):
        after=copy.deepcopy(self.inv)
        after['functions'][probe.FN]['body']='3b6000004e800020'
        after['functions']['sibling']['body']='60000000'
        after['sections']['.bss']['size']=12
        after['exception_records']=[]
        row=self.measure(after=after)
        self.assertEqual((row['ours_count'],row['target_count'],row['differing_words']),(2,1,1))
        self.assertEqual(row['changed_bodies'],['FontInit','sibling'])
        self.assertFalse(row['nontext_equal'])
        self.assertFalse(row['exception_records_equal'])

    def test_empty_or_partial_instruction_refused(self):
        for body in ('','00'):
            after=copy.deepcopy(self.inv)
            after['functions'][probe.FN]['body']=body
            with self.assertRaises(ValueError):
                self.measure(after=after)

    def test_fidelity_requires_complete_raw_equal_and_no_error(self):
        probe.require_fidelity(b'abc',b'abc',None)
        for raw,error in ((None,None),(b'abd',None),(b'abc','compiler failure')):
            with self.assertRaises(ValueError):
                probe.require_fidelity(raw,b'abc',error)


if __name__=='__main__':
    unittest.main()
