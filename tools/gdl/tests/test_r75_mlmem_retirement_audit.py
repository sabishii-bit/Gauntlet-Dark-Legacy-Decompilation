import copy
import unittest

from tools.gdl.composed_census import r75_mlmem_retirement_audit as audit


def fixture():
    original = bytearray(b'\x60\x00\x00\x00' * 56)
    wanted = bytearray(original)
    for i in audit.SITES:
        wanted[i+3] = 1
    body = dict(body=original.hex(), size=224, offset=0, relocations=[])
    before = dict(functions={audit.FN: body, 'sibling': dict(body='4e800020', size=4, offset=224, relocations=[])},
                  sections={'.text': dict(bytes=(original+b'\x4e\x80\x00\x20').hex()), '.data': dict(bytes='01020304')},
                  symbols={}, all_symbols=[], exception_records=[])
    after = copy.deepcopy(before)
    after['functions'][audit.FN]['body'] = wanted.hex()
    after['sections']['.text']['bytes'] = (wanted+b'\x4e\x80\x00\x20').hex()
    target = copy.deepcopy(after)
    rules = {'units': {audit.UNIT: [{'function': audit.FN}, {'function': 'AllocFile'}]}}
    new_rules = {'units': {audit.UNIT: [{'function': 'AllocFile'}]}}
    return before, after, target, copy.deepcopy(after), copy.deepcopy(after), rules, new_rules


class RetirementTests(unittest.TestCase):
    def test_exact_eight_word_retirement(self):
        result = audit.audit(*fixture())
        self.assertEqual((result['changed_raw_words'], result['retired_bytes']), (8, 224))
        self.assertEqual(result['raw_siblings_unchanged'], 1)

    def test_refuses_sibling_data_metadata_and_relocation_drift(self):
        mutations = (
            lambda x: x['functions']['sibling'].update(body='60000000'),
            lambda x: x['sections']['.data'].update(bytes='00000000'),
            lambda x: x['exception_records'].append({'unexpected': 1}),
            lambda x: x['functions'][audit.FN]['relocations'].append([0, 10, 'wrong', 0]),
        )
        for mutate in mutations:
            with self.subTest(mutation=mutate):
                args = list(fixture())
                mutate(args[1])
                with self.assertRaisesRegex(ValueError, 'raw changes exceed'):
                    audit.audit(*args)

    def test_refuses_compensating_processed_change(self):
        args = list(fixture())
        args[4]['sections']['.data']['bytes'] = '00000000'
        with self.assertRaisesRegex(ValueError, 'processed allocation'):
            audit.audit(*args)

    def test_refuses_extra_rule_removal(self):
        args = list(fixture())
        args[6]['units'][audit.UNIT] = []
        with self.assertRaisesRegex(ValueError, 'configuration differs'):
            audit.audit(*args)

    def test_refuses_unreviewed_baseline(self):
        args = list(fixture())
        args[0]['functions'][audit.FN]['body'] = '12345678' + args[0]['functions'][audit.FN]['body'][8:]
        with self.assertRaisesRegex(ValueError, 'eight-word residual'):
            audit.audit(*args)

    def test_source_reconstruction_is_scoped(self):
        before = '''void* AllocMem(u32 size) { return 0; }
void* AllocMem32(int size)
{
    u8 unused[8];
    int pad;
    void* result;
    pad = 0;
    if (mlmMemReserved != 0) { old(); }
    return result;
}
'''
        after = audit.reconstruct(before)
        self.assertIn('void* AllocMem(int size)', after)
        self.assertIn('result = AllocMem(size);', after)
        self.assertNotIn('unused[8]', after)
        with self.assertRaises(ValueError):
            audit.reconstruct(after)


if __name__ == '__main__':
    unittest.main()
