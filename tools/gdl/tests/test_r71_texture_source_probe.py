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

    def test_reviewed_r89_context_preserves_historical_controls(self):
        old = self.source.replace(
            '    PbTexMgr* wg = gWinGlobals;\n    s32 m;\n    s32 loaded = 0;',
            '    s32 m;\n    s32 loaded = 0;\n    PbTexMgr* wg = gWinGlobals;')
        old = old.replace('        u8** ep = &((TEXDESCENT*)wg->tbl)[m].desc;',
                          '        u8* e = (u8*)wg->tbl + m * 0x10;\n        u8** ep = (u8**)(e + 0x4);')
        old = old.replace('((u8*)&((TEXDESCENT*)wg->tbl)[m] + 0x10)', '(e + 0x10)')
        old = old.replace('                FatalErrorf(lbl_80116AC0, 0x200, t + 1,',
                          '                u8* tb = (u8*)wg->tbl + 0x4;\n                FatalErrorf(lbl_80116AC0, 0x200, t + 1,')
        old = old.replace('                            ((TEXDESCENT*)wg->tbl)[m].desc);',
                          '                            *(void**)(m * 0x10 + tb));')
        self.assertEqual(probe.sha(old.encode()), probe.SOURCE_SHA256)
        self.assertEqual(probe.sha(self.source.encode()), probe.R89_SOURCE_SHA256)
        historical, current = probe.source_forms(old), probe.source_forms(self.source)
        self.assertEqual(len(historical), 61)
        self.assertEqual(historical.keys(), current.keys())
        def exclude_changed_function(source):
            start = source.index('void fn_800C72DC(void) {')
            end = source.index('\n}\n', start)+2
            return source[:start] + source[end:]
        for name in historical:
            self.assertEqual(exclude_changed_function(historical[name]),
                             exclude_changed_function(current[name]), name)

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
