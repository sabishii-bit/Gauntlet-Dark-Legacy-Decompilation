import copy
import struct
import unittest

from tools.gdl.composed_census import r69_camera_argument_control as control
from tools.gdl.tests.test_r68_interfaces_raw_control import snapshot


def function(words, call, name='DiffRate_8002951C'):
    return dict(body=struct.pack('>' + 'I' * len(words), *words).hex(),
                size=len(words)*4, offset=0, binding=1,
                relocations=[[call, 10, name, 0]])


def audit_fixture():
    words = [0x60000000] * (0x2E4 // 4)
    words[0x2A4//4] = 0x80600000  # bad frame-ticks destination
    words[0x2B4//4] = 0x906100AC
    words[0x2B8//4] = 0xC8400000
    words[0x2BC//4] = 0xFC000050
    words[0x2DC//4] = 0x48000001
    words[0x2E0//4] = 0x4E800020
    old = function(words, 0x2DC, 'DiffRate')
    old['relocations'] = [[0x2A6, 109, 'gFrameTicks', 0], [0x2BA, 109, '@520', 0], [0x2DC, 10, 'DiffRate', 0]]
    before = snapshot()
    before['functions'] = {control.FUNCTION: old}
    before['sections']['.text'].update(bytes=old['body'], size=old['size'])
    before['symbols'] = [dict(name='DiffRate', section='', binding=1)]
    before['relocations'] = {'.text': copy.deepcopy(old['relocations'])}
    words[0x2A4//4] = 0x80800000
    words[0x2B4//4] = 0x908100AC
    words[0x2B8//4], words[0x2BC//4] = words[0x2BC//4], words[0x2B8//4]
    new = function(words, 0x2DC)
    new['relocations'] = [[0x2A6, 109, 'gFrameTicks', 0], [0x2BE, 109, '@520', 0], [0x2DC, 10, control.CONSUMER, 0]]
    after = copy.deepcopy(before)
    after['functions'][control.FUNCTION] = new
    after['sections']['.text']['bytes'] = new['body']
    after['symbols'][0]['name'] = control.CONSUMER
    after['relocations']['.text'] = copy.deepcopy(new['relocations'])
    provider = dict(body='00000000'*3 + '1c03018c', binding=1)
    return before, after, {control.FUNCTION: copy.deepcopy(new)}, {'raw': provider, 'target': copy.deepcopy(provider)}


class CameraArgumentControlTests(unittest.TestCase):
    def test_preserved_across_both_diamond_paths(self):
        fn = function([0x41820008, 0x60000000, 0x48000001, 0x4E800020], 8)
        self.assertTrue(control.entry_argument(fn, control.CONSUMER)['preserves_entry_r3'])

    def test_one_path_clobber_is_not_preserved(self):
        fn = function([0x41820008, 0x38600007, 0x48000001, 0x4E800020], 8)
        result = control.entry_argument(fn, control.CONSUMER)
        self.assertFalse(result['preserves_entry_r3'])
        self.assertEqual(result['r3_writes'], [{'offset': 4, 'word': '38600007'}])

    def test_exit_only_path_clobber_is_irrelevant(self):
        fn = function([0x4182000C, 0x38600007, 0x4E800020, 0x48000001, 0x4E800020], 12)
        self.assertTrue(control.entry_argument(fn, control.CONSUMER)['preserves_entry_r3'])

    def test_intervening_call_refused(self):
        fn = function([0x48000001, 0x48000001, 0x4E800020], 4)
        with self.assertRaisesRegex(ValueError, 'intervening call'):
            control.entry_argument(fn, control.CONSUMER)

    def test_unknown_operands_and_indirect_flow_refused(self):
        for word in (0, 0x4E800420, 0x4E800421):
            with self.subTest(word=word), self.assertRaises(ValueError):
                control.entry_argument(function([word, 0x48000001, 0x4E800020], 4), control.CONSUMER)

    def test_consumer_identity_and_instruction_are_bound(self):
        fn = function([0x60000000, 0x4E800020], 0)
        with self.assertRaises(ValueError):
            control.entry_argument(fn, control.CONSUMER)
        fn = function([0x48000001, 0x4E800020], 0)
        fn['relocations'][0][3] = 4
        with self.assertRaises(ValueError):
            control.entry_argument(fn, control.CONSUMER)

    def test_tuple_and_list_relocation_readers_agree(self):
        fn = function([0x48000001, 0x4E800020], 0)
        expected = control.entry_argument(fn, control.CONSUMER)
        fn['relocations'] = [tuple(r) for r in fn['relocations']]
        self.assertEqual(control.entry_argument(fn, control.CONSUMER), expected)

    def test_full_control_and_eh_rejection(self):
        before, after, target, provider = audit_fixture()
        self.assertEqual(control.audit(before, after, target, provider)['status'], 'PASS')
        after['sections']['extab']['bytes'] = '00000001'
        self.assertEqual(control.audit(before, after, target, provider)['status'], 'FAIL')

    def test_other_relocation_change_rejected(self):
        before, after, target, provider = audit_fixture()
        after['relocations']['.text'][0][2] = 'unrelated'
        self.assertEqual(control.audit(before, after, target, provider)['status'], 'FAIL')

    def test_wrong_target_or_provider_refused(self):
        before, after, target, provider = audit_fixture()
        target[control.FUNCTION]['body'] = target[control.FUNCTION]['body'][:0x2B4*2] + '900100ac' + target[control.FUNCTION]['body'][0x2B8*2:]
        with self.assertRaisesRegex(ValueError, 'differs from target'):
            control.audit(before, after, target, provider)
        before, after, target, provider = audit_fixture()
        provider['raw']['body'] = '00000000'*4
        with self.assertRaisesRegex(ValueError, 'consume r3'):
            control.audit(before, after, target, provider)

    def test_provider_set_requires_both_and_no_extras(self):
        before, after, target, provider = audit_fixture()
        for keys in ((), ('raw',), ('target',), ('raw', 'target', 'other')):
            supplied = {key: copy.deepcopy(provider['raw']) for key in keys}
            with self.subTest(keys=keys), self.assertRaisesRegex(ValueError, 'exactly target and raw'):
                control.audit(before, after, target, supplied)


if __name__ == '__main__':
    unittest.main()
