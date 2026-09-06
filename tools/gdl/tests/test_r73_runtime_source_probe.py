import unittest
from tools.gdl.composed_census import r73_runtime_source_probe as probe


class RuntimeSourceFormsTests(unittest.TestCase):
    def fixture(self):
        source = '#include "NMWException.h"\n' + probe.HANDLERS
        source += 'exception::~exception() throw() {}\n' + probe.WHAT + '\n'
        header = ('#ifdef NMWEXCEPTION_CPP\nvirtual ~exception() throw();\n'
                  '#else\nvirtual ~exception() throw() {}\n#endif\n'
                  'virtual const char* what() const;\n')
        return source, header

    def test_controls_preserve_source_and_header(self):
        source, header = self.fixture()
        forms = probe.forms(probe.UNITS[0], source, header)
        self.assertEqual(forms['control'], (source, header))
        self.assertEqual(forms['overlay_control'], (source, header))
        self.assertIn('#error ' + probe.SENTINEL, forms['overlay_refusal'][1])
        self.assertEqual(forms['implicit_overlay_refusal'], forms['overlay_refusal'])

    def test_independent_and_joint_handler_inline_axes(self):
        source, header = self.fixture()
        forms = probe.forms(probe.UNITS[0], source, header)
        late, hdr = forms['late_handlers']
        self.assertEqual(hdr, header)
        self.assertGreater(late.index(probe.HANDLERS), late.index(probe.WHAT))
        joint, hdr = forms['inline_what_late_handlers']
        self.assertNotIn(probe.WHAT, joint)
        self.assertIn('virtual const char* what() const { return "exception"; }', hdr)
        self.assertIn('exception::~exception() throw() {}', joint)
        self.assertEqual(joint.count(probe.HANDLERS), 1)

    def test_unified_inline_removes_only_out_of_line_member_definitions(self):
        source, header = self.fixture()
        body, hdr = probe.forms(probe.UNITS[0], source, header)['unified_inline']
        self.assertNotIn('exception::~exception()', body)
        self.assertNotIn(probe.WHAT, body)
        self.assertIn(probe.HANDLERS, body)
        self.assertNotIn('NMWEXCEPTION_CPP', hdr)

    def test_exceptionppc_body_held_fixed_on_header_axes(self):
        source, header = self.fixture()
        for body, hdr in probe.forms(probe.UNITS[1], source, header).values():
            self.assertEqual(body, source)

    def test_explicit_header_path_and_duplicate_refusal(self):
        source, _ = self.fixture()
        self.assertIn('#include "build/trial/NMWException.h"',
                      probe.overlay_include(source, 'build/trial/NMWException.h'))
        with self.assertRaises(ValueError):
            probe.overlay_include(source + source, 'build/unused.h')


if __name__ == '__main__':
    unittest.main()
