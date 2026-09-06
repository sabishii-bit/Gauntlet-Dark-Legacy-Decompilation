import unittest
from unittest import mock

from tools.gdl.composed_census import r71_texture_source_probe as probe


class TextureSourceControlTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (probe.ROOT / 'src/game/pb/pb_texture.c').read_text()

    def test_original_and_crlf_produce_the_same_control_forms(self):
        forms = probe.source_forms(self.source)
        self.assertEqual(forms, probe.source_forms(self.source.replace('\n', '\r\n')))
        self.assertEqual(forms['baseline'], self.source)
        self.assertEqual(forms['decl_0123'], forms['baseline'])
        declarations = [name for name in forms if name.startswith('decl_')]
        self.assertEqual(len(declarations), 24)
        self.assertEqual(len(set(forms[name] for name in declarations)), 24)

    def test_source_drift_cannot_silently_turn_a_control_into_a_noop(self):
        for source in ('', self.source + '\n', self.source.replace('u32 tlut_size', 'int tlut_size'),
                       self.source.replace('slot + 0x30,', 'slot + 0x40,')):
            with self.subTest(source=source[-60:]), self.assertRaises(ValueError):
                probe.source_forms(source)

    def test_header_and_enum_axes_are_separated(self):
        forms = probe.source_forms(self.source)
        self.assertIn('void GXInitTlutRegion(void* region, u32 tmem_addr, u32 tlut_size);', forms['header_only'])
        self.assertIn('void GXInitTlutRegion(void* region, u32 tmem_addr, GXTlutSize tlut_size);', forms['enum_arg'])
        self.assertIn('void GXInitTlutRegion(GXTlutRegion* region, u32 tmem_addr, GXTlutSize tlut_size);', forms['full_type'])
        self.assertEqual(forms['full_type'].count('GXInitTlutRegion((GXTlutRegion*)(slot + 0x30),'), 2)

    def test_helper_joint_control_uses_existing_definition(self):
        forms = probe.source_forms(self.source)
        for name in ('existing_callback_defined_first', 'callback_member_joint_defined_first'):
            source = forms[name]
            self.assertEqual(source.count('static void* sTlutRegionCallback(u32 name) {'), 1)
            self.assertLess(source.index('static void* sTlutRegionCallback(u32 name) {'),
                            source.index('void pbInitTlutRegions(void) {'))
            self.assertEqual(source.count('GXInitTlutRegion(sTlutRegionCallback(i),'), 2)

    def test_unknown_form_refuses_before_compiling(self):
        with mock.patch.object(probe.cv, 'read_edges', return_value={probe.UNIT: {
                'src': 'src/game/pb/pb_texture.c', 'body_o': 'src/game/pb/pb_texture.c'}}):
            with mock.patch.object(probe, 'capture') as capture, self.assertRaises(SystemExit) as error:
                probe.main(['--only', 'not_a_real_form'])
            self.assertEqual(error.exception.code, 2)
            capture.assert_not_called()


if __name__ == '__main__':
    unittest.main()
