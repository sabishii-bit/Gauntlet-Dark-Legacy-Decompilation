import struct
import unittest
from pathlib import Path

from tools.gdl.composed_census.r85_wizard_source_probe import source_controls


class WizardSourceControlsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = (Path(__file__).resolve().parents[3] / 'src/game/ui/auxscreen.c').read_text()
        start = source.index('void calc_wizard_pos(f32* out)\n{')
        stop = source.index('\n}\n', start) + 2
        cls.forms = source_controls(source[start:stop])

    def test_finite_matrix(self):
        self.assertEqual(len(self.forms), 42)
        for text in self.forms.values():
            self.assertEqual(text.count('{'), text.count('}'))

    def test_existing_fields_only(self):
        text = self.forms['typed_local']
        self.assertIn('p->state', text)
        self.assertIn('f32* py = &p->pos[1];', text)
        self.assertNotIn('offsetof(Player', text)

    def test_correct_literal_widths_and_values(self):
        text = self.forms['typed_literals_7']
        self.assertIn('f32 count = 1.0f;', text)
        self.assertIn('if (1.0 == count)', text)
        self.assertIn('(f32)(2.0 * (f64)*py)', text)
        self.assertIn('(f32)(count + 1.0)', text)
        self.assertEqual(struct.pack('>f', 1.0).hex(), '3f800000')
        self.assertEqual(struct.pack('>d', 1.0).hex(), '3ff0000000000000')
        self.assertEqual(struct.pack('>d', 2.0).hex(), '4000000000000000')

    def test_single_literal_controls_hold_other_names_fixed(self):
        text = self.forms['typed_literals_1']
        self.assertNotIn('lbl_80345A40', text)
        self.assertIn('lbl_80345A28', text)
        self.assertIn('lbl_80345A48', text)


if __name__ == '__main__':
    unittest.main()
