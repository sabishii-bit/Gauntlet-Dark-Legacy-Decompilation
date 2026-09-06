import copy
import unittest

from tools.gdl.composed_census import r69_options_shared_state_audit as audit


class OptionsSharedStateTests(unittest.TestCase):
    def test_exact_envelope(self):
        self.assertEqual(audit.validate_layout(audit.PARTS)['size'], 160)

    def test_changed_missing_or_extra_subobject_refused(self):
        for parts in ({}, dict(audit.PARTS, extra=(audit.END, 4)),
                      dict(audit.PARTS, optionsStack=(audit.BASE + 0x44, 16)),
                      dict(audit.PARTS, optionsStack=(audit.BASE + 0x40, 0))):
            with self.subTest(parts=parts), self.assertRaises(ValueError):
                audit.validate_layout(parts)

    def test_map_requires_unique_names_and_current_extent(self):
        text = '\n'.join(f'{name} = .bss:0x{address:X}; // type:object size:0x{size:X}'
                         for name, (address, size) in audit.PARTS.items())
        self.assertEqual(audit.read_layout(text), audit.PARTS)
        for bad in ('', text + '\n' + text, text.replace('0x80274E40', '0x80274E44')):
            with self.assertRaises(ValueError):
                audit.read_layout(bad)

    def test_raw_allocation_requires_fidelity_and_one_complete_bss(self):
        snapshot = dict(fidelity=True, unit='game/ui/options', raw_sha256='test', compiler='test', flags='test',
                        symbols=[dict(name='optglobals', section='.bss', size=160, value=0)],
                        sections={'.bss': dict(type=8, size=160)})
        self.assertEqual(audit.audit_raw(snapshot)['symbol']['size'], 160)
        variants = []
        for key, value in (('fidelity', False), ('unit', 'other'), ('symbols', [])):
            variants.append(dict(snapshot, **{key: value}))
        bad = copy.deepcopy(snapshot)
        bad['sections']['.bss']['size'] = 164
        variants.append(bad)
        for bad in variants:
            with self.assertRaises(ValueError):
                audit.audit_raw(bad)

    def test_empty_population_refused(self):
        with self.assertRaises(ValueError):
            audit.scan({})

    def test_retained_definitions_cover_one_exact_envelope(self):
        rows = [dict(unit='auto', definitions=[dict(name=name, value=address - audit.BASE + 12,
                                                   size=size, section='.bss', binding=1)
                                              for name, (address, size) in audit.PARTS.items()])]
        self.assertEqual(audit.validate_retained(rows)['section_offset'], 12)
        bad_size = copy.deepcopy(rows)
        bad_size[0]['definitions'][1]['size'] += 4
        for bad in ([], rows * 2, bad_size, [dict(unit='auto', definitions=rows[0]['definitions'][:-1])]):
            with self.assertRaises(ValueError):
                audit.validate_retained(bad)


if __name__ == '__main__':
    unittest.main()
