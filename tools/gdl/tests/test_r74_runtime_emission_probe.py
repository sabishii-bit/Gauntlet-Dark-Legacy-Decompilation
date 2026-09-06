import copy
import unittest

from tools.gdl.composed_census import r74_runtime_emission_probe as probe


class RuntimeEmissionControlsTests(unittest.TestCase):
    def test_full_source_forms_are_explicit_and_leave_input_unchanged(self):
        header = (probe.ROOT / 'include/NMWException.h').read_text()
        for unit in probe.base.UNITS:
            source = (probe.ROOT / 'src' / (unit + '.cpp')).read_text()
            rows = probe.variants(unit, source, header)
            self.assertEqual(len(rows), 32)
            self.assertEqual(rows['overlay_control__base'], (source, header, ''))
            self.assertEqual(rows['overlay_control__deferred'][:2], (source, header))
            self.assertEqual(rows['inline_what__deferred'][2], '-inline deferred')
            self.assertIn('return "exception";', rows['inline_what__deferred'][1])
            for pragma in ('force_active', 'defer_codegen'):
                for setting in ('on', 'off'):
                    body, hdr, flags = rows['overlay_control__header_' + pragma + '_' + setting]
                    self.assertEqual(body, source)
                    self.assertEqual(flags, '')
                    self.assertIn('#pragma ' + pragma + ' ' + setting, hdr)
                    self.assertIn('#pragma ' + pragma + ' reset', hdr)

    def test_sensitivity_sees_data_only_movement_and_does_not_invent_it(self):
        control = dict(missing_functions=[], extra_functions=[], changed_bodies=[],
                       rodata_hex='00', small_data=[], allocated_geometry={},
                       relocations=[], functions={})
        rows = {'overlay_control__' + name: copy.deepcopy(control) for name in probe.OPTIONS}
        rows['overlay_control__deferred']['rodata_hex'] = '0000'
        result = probe.summarize(rows)
        self.assertTrue(result['option_control_changed']['deferred'])
        self.assertFalse(result['option_control_changed']['noauto'])
        self.assertEqual(result['changed_fact_categories']['overlay_control__deferred'], ['rodata_hex'])

    def test_wrong_header_anchor_refuses(self):
        with self.assertRaises(ValueError):
            probe.variants(probe.base.UNITS[0], 'wrong source', 'wrong header')


if __name__ == '__main__':
    unittest.main()
