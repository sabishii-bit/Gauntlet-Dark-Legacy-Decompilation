import copy
import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.gdl.composed_census import r69_options_shared_state_audit as audit


class OptionsSharedStateTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.objects = self.root / 'build/GUNE5D/obj'
        self.objects.mkdir(parents=True)
        self.active = self.objects / 'active.o'
        self.active.write_bytes(b'active')
        self.ninja = b'current ninja graph\n'
        self.manifest = dict(schema_version=1, version='GUNE5D',
                             ninja_sha256=hashlib.sha256(self.ninja).hexdigest(),
                             units=[dict(name='active.c', extracted_object='build/GUNE5D/obj/active.o')])

    def targets(self, manifest=None):
        return audit.active_targets(self.manifest if manifest is None else manifest, self.ninja, self.root)

    def test_stale_on_disk_object_is_not_in_active_population(self):
        (self.objects / 'stale_owner.o').write_bytes(b'invalid ELF or duplicate old owner')
        targets = self.targets()
        self.assertEqual(targets, {'active': self.active})
        with patch.object(audit, 'ROOT', self.root), patch.object(audit, 'object_rows', return_value=([], [])) as read:
            result = audit.scan(targets)
        self.assertEqual(result['configured_objects'], 1)
        read.assert_called_once_with(self.active)

    def test_missing_active_object_or_empty_population_refused(self):
        self.active.unlink()
        with self.assertRaisesRegex(ValueError, 'missing active target'):
            self.targets()
        for units in ([], None, [None], [dict(name='active.c')], [dict(name='', extracted_object='')]):
            with self.subTest(units=units), self.assertRaises(ValueError):
                self.targets(dict(self.manifest, units=units))

    def test_duplicate_active_names_and_paths_refused(self):
        (self.objects / 'second.o').write_bytes(b'second')
        for row in (dict(name='active.c', extracted_object='build/GUNE5D/obj/second.o'),
                    dict(name='active.cpp', extracted_object='build/GUNE5D/obj/second.o'),
                    dict(name='second.c', extracted_object='build/GUNE5D/obj/active.o'),
                    dict(name='second.c', extracted_object='build/GUNE5D/obj/sub/../active.o')):
            with self.subTest(row=row), self.assertRaisesRegex(ValueError, 'duplicate/ambiguous'):
                self.targets(dict(self.manifest, units=self.manifest['units'] + [row]))

    def test_manifest_schema_version_and_ninja_hash_required(self):
        for key, value in (('schema_version', 2), ('schema_version', True), ('version', 'other'),
                           ('ninja_sha256', 'stale')):
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                self.targets(dict(self.manifest, **{key: value}))
        with self.assertRaises(ValueError):
            audit.active_targets(self.manifest, b'different ninja', self.root)

    def test_active_target_paths_must_stay_inside_object_directory(self):
        outside = self.root / 'build/GUNE5D/outside.o'
        outside.write_bytes(b'outside')
        for value in ('build/GUNE5D/outside.o', 'build/GUNE5D/obj/../outside.o', str(outside),
                      'build/GUNE5D/obj/active.txt'):
            with self.subTest(path=value), self.assertRaisesRegex(ValueError, 'escapes'):
                self.targets(dict(self.manifest, units=[dict(name='active.c', extracted_object=value)]))

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
