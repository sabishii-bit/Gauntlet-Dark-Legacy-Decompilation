import copy
import unittest
from tools.gdl.composed_census import r69_camera_import_control as control


class CameraImportControlTests(unittest.TestCase):
    def providers(self):
        return {name: {'binding': 1} for name in control.MAPPING.values()}

    def test_explicit_map_excludes_argument_repair(self):
        self.assertEqual(len(control.MAPPING), 6)
        self.assertNotIn('DiffRate', control.MAPPING)
        self.assertEqual(len(set(control.MAPPING.values())), 6)

    def test_global_providers_pass(self):
        control.provider_check(self.providers())

    def test_local_provider_refused(self):
        providers = self.providers()
        providers[next(iter(providers))]['binding'] = 0
        with self.assertRaisesRegex(ValueError, 'GLOBAL provider'):
            control.provider_check(providers)

    def test_missing_provider_refused(self):
        providers = self.providers()
        providers.pop(next(iter(providers)))
        with self.assertRaisesRegex(ValueError, 'GLOBAL provider'):
            control.provider_check(providers)

    def test_calls_preserve_relocation_and_word_evidence(self):
        functions = {'caller': {'body': '38600000480000014e800020',
                                'relocations': [[4, 10, 'callee', 0]]}}
        original = copy.deepcopy(functions)
        rows = control.calls(functions, {'callee'})
        self.assertEqual(rows, [{'caller': 'caller', 'offset': 4, 'kind': 10,
                                'symbol': 'callee', 'addend': 0,
                                'preceding_words': [{'offset': 0, 'word': '38600000'}]}])
        self.assertEqual(control.calls(functions, {'other'}), [])
        self.assertEqual(functions, original)


if __name__ == '__main__':
    unittest.main()
