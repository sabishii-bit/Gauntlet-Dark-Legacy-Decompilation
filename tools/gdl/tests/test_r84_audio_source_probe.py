import unittest

from tools.gdl.composed_census.r84_audio_source_probe import measure_body


class AudioSourceProbeTests(unittest.TestCase):
    def test_parity_and_frame(self):
        result = measure_body('9421ffd860000000', '9421ffd838600000')
        self.assertEqual(result['instructions'], [2, 2])
        self.assertEqual(result['differing_words'], 1)
        self.assertEqual(result['frame'], 40)

    def test_count_asymmetry_is_not_a_full_word_diff(self):
        result = measure_body('600000004e800020', '60000000')
        self.assertEqual(result['instructions'], [2, 1])
        self.assertIsNone(result['differing_words'])
        self.assertEqual(result['overlap_prefix_differing_words'], 0)
        self.assertIsNone(result['frame'])

    def test_refuse_partial_word(self):
        with self.assertRaises(ValueError):
            measure_body('00', '')

    def test_mflr_precedes_frame_allocation(self):
        body = '7c0802a6900100049421ffd8'
        self.assertEqual(measure_body(body, body)['frame'], 40)


if __name__ == '__main__':
    unittest.main()
