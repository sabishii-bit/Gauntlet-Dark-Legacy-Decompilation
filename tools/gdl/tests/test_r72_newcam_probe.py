import copy
import unittest

from tools.gdl.composed_census import r72_newcam_probe as probe


class R72NewcamProbeTests(unittest.TestCase):
    def test_unknown_form_refuses(self):
        with self.assertRaisesRegex(ValueError, 'unknown forms'):
            probe.validate_selection({'target_definiton_order'}, ['target_definition_order'])
        probe.validate_selection({'target_definition_order'}, ['target_definition_order'])

    def test_target_sequence_is_checked(self):
        target = {'functions': {name: {'offset': i * 4}
                               for i, name in enumerate(probe.TARGET_ORDER)}}
        self.assertEqual(probe.validate_target_order(target), probe.TARGET_ORDER)
        target['functions'][probe.TARGET_ORDER[0]]['offset'] = 10000
        with self.assertRaisesRegex(ValueError, 'sequence changed'):
            probe.validate_target_order(target)

    def test_private_datum_identity_requires_full_bytes_and_addend(self):
        before = dict(functions={'f': {'relocations': [[4, 109, '@1', 0]]}},
                      symbols={'@1': dict(section='.sdata2', binding=0, size=8, bytes='3ff0000000000000')},
                      sections={'.sdata2': {'relocations': []}})
        after = copy.deepcopy(before)
        after['functions']['f']['relocations'][0][2] = '@99'
        after['symbols']['@99'] = after['symbols'].pop('@1')
        self.assertEqual(probe.normalized_relocations(before), probe.normalized_relocations(after))
        after['symbols']['@99']['bytes'] = '3ff0000000000001'
        self.assertNotEqual(probe.normalized_relocations(before), probe.normalized_relocations(after))
        after['functions']['f']['relocations'][0][3] = 8
        with self.assertRaisesRegex(ValueError, 'outside object'):
            probe.normalized_relocations(after)

    def test_private_mutable_or_global_objects_refuse(self):
        base = dict(functions={'f': {'relocations': [[4, 109, '@1', 0]]}},
                    symbols={'@1': dict(section='.sdata2', binding=0, size=8, bytes='0'*16)},
                    sections={'.sdata2': {'relocations': []}, '.data': {'relocations': []}})
        for field, value in [('binding', 1), ('section', '.data')]:
            changed = copy.deepcopy(base)
            changed['symbols']['@1'][field] = value
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, 'local readonly pool'):
                probe.normalized_relocations(changed)

    def test_named_identity_and_relocation_site_remain_significant(self):
        before = dict(functions={'f': {'relocations': [[4, 10, 'callee', 0]]}}, symbols={})
        after = copy.deepcopy(before)
        after['functions']['f']['relocations'][0][2] = 'other_callee'
        self.assertNotEqual(probe.normalized_relocations(before), probe.normalized_relocations(after))

    def test_type_addend_missing_and_extra_rows_do_not_match(self):
        before = dict(functions={'f': {'relocations': [[4, 10, 'callee', 0]]}}, symbols={})
        candidates = [[[4, 1, 'callee', 0]], [[4, 10, 'callee', 4]], [],
                      [[4, 10, 'callee', 0], [8, 10, 'callee', 0]]]
        for rows in candidates:
            after = copy.deepcopy(before)
            after['functions']['f']['relocations'] = rows
            with self.subTest(rows=rows):
                self.assertNotEqual(probe.normalized_relocations(before), probe.normalized_relocations(after))
        after = copy.deepcopy(before)
        after['functions']['f']['relocations'][0][0] = 8
        self.assertNotEqual(probe.normalized_relocations(before), probe.normalized_relocations(after))


if __name__ == '__main__':
    unittest.main()
