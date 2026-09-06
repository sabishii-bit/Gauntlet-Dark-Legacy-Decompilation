import copy
import unittest

from tools.gdl.composed_census import r70_sysservice_type_control as control


class SysserviceTypeControlTests(unittest.TestCase):
    def test_stock_identity_is_digest_not_directory_name(self):
        control.require_stock_hash(control.STOCK_SHA256)
        for value in ('GC/1.2.5', '', None, '0' * 64):
            with self.assertRaises(ValueError):
                control.require_stock_hash(value)

    def test_only_private_type_and_one_rule_can_change(self):
        rules = [{'function': name} for name in ('sysPollResetButton', 'sysClearFlags', 'sysResetService')]
        before = dict(source=control.OLD, config={'units': {control.UNIT: rules}})
        after = dict(source=control.NEW, config={'units': {control.UNIT: [rules[0], rules[2]]}})
        control.source_and_config(before, after)
        for source in (control.OLD, control.NEW + '\nother edit', control.NEW.replace('static ', '')):
            with self.assertRaises(ValueError):
                control.source_and_config(before, dict(after, source=source))
        bad = copy.deepcopy(after)
        bad['config']['units'][control.UNIT].pop()
        with self.assertRaises(ValueError):
            control.source_and_config(before, bad)
        with self.assertRaises(ValueError):
            control.source_and_config(dict(before, source=control.OLD * 2), after)

    def test_complete_object_scope_and_exact_target_required(self):
        target = bytes.fromhex('808000007c801878900000004e800020')
        old = bytes.fromhex('800000007c001878900000004e800020')
        before, after = b'head' + old + b'tail', b'head' + target + b'tail'
        result = control.byte_certificate(before, after, 4, 16, target)
        self.assertEqual(result['changed_word_offsets'], [0, 4])
        for changed in (b'HEAD' + target + b'tail', after + b'extra', b'head' + old + b'tail'):
            with self.assertRaises(ValueError):
                control.byte_certificate(before, changed, 4, 16, target)
        with self.assertRaises(ValueError):
            control.byte_certificate(after, after, 4, 16, target)
        with self.assertRaises(ValueError):
            control.byte_certificate(before, after, 4, 12, target)

    def test_relocation_normalization_is_sda_only(self):
        self.assertEqual(control.canonical_relocations([(2, 109, 'flags', 0)]), [(0, 109, 'flags', 0)])
        self.assertEqual(control.canonical_relocations([(2, 6, 'flags', 0)]), [(2, 6, 'flags', 0)])
        self.assertNotEqual(control.canonical_relocations([(2, 109, 'flags', 0)]),
                            control.canonical_relocations([(2, 109, 'other', 0)]))

    def test_snapshot_identity_and_hashes_fail_closed(self):
        source, body = control.OLD + '\n', b'raw'
        functions = {control.FUNCTION: {}}
        functions.update({'sibling' + str(i): {} for i in range(16)})
        value = dict(schema_version=1, source=source, raw_bytes=body.hex(), raw=dict(
            schema_version=1, fidelity=True, unit=control.UNIT, compiler='GC/1.2.5',
            raw_sha256=control.sha(body), source_sha256=control.sha(source.encode()), functions=functions))
        control.validate_snapshot(value)
        crlf = copy.deepcopy(value)
        crlf['raw']['source_sha256'] = control.sha(source.replace('\n', '\r\n').encode())
        control.validate_snapshot(crlf)
        for key, badvalue in (('fidelity', 1), ('unit', 'other'), ('compiler', 'GC/1.2.5s'),
                              ('schema_version', 0), ('raw_sha256', 'bad'), ('source_sha256', 'bad'),
                              ('functions', {})):
            bad = copy.deepcopy(value)
            bad['raw'][key] = badvalue
            with self.assertRaises(ValueError):
                control.validate_snapshot(bad)


if __name__ == '__main__':
    unittest.main()
