"""Synthetic invariants for the manual finite MEMPOOL source replay."""
import contextlib
import copy
import hashlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from tools.gdl.composed_census import r76_pool_source_controls as probe

BODY = '''s32 pool_garbage_collect(MemPoolLists* pool,
                         s32 (*gapCallback)(MemListNode*, u32)) {
    s32 result;
    s32 count;
    MemListNode* node;
    s32 i;
    u32 currentEnd;
    MemListNode** entries = lbl_8031EB00;

    result = 1;
    count = 0;
    if ((node = pool->secondary.head) != NULL) {
        do {
            entries[count++] = node;
            node = node->next;
        } while (node != pool->secondary.head);
    }

    qsort(entries, count, sizeof(MemListNode*), pool_query);

    currentEnd = entries[0]->flags;
    node = pool->primary.head;
    if (node != NULL) {
        do {
            if (currentEnd > node->flags) {
                currentEnd = node->flags;
            }
            node = node->next;
        } while (node != pool->primary.head);
    }

    for (i = 0; i < count; i++) {
        if (entries[i]->flags > currentEnd &&
            gapCallback(entries[i], currentEnd) != 0) {
            result = 0;
            break;
        }
        currentEnd += entries[i]->key;
    }
    return result;
}
'''


def fixture():
    return dict(functions={probe.FN: dict(body='00000000', size=4, offset=0, relocations=[]),
                           'sibling': dict(body='11223344', size=4, offset=4, relocations=[])},
                sections={'.text': dict(bytes='0000000011223344', relocations=[]),
                          '.data': dict(bytes='aabbccdd', relocations=[])},
                symbols={}, all_symbols=[], exception_records=[])


class PoolSourceControlsTests(unittest.TestCase):
    def test_twenty_distinct_forms_and_preserved_baseline(self):
        variants = probe.forms(BODY)
        self.assertEqual(len(variants), 21)
        self.assertEqual(len(set(variants.values())), 21)
        self.assertEqual(next(iter(variants)), 'baseline')
        self.assertEqual(variants['baseline'], BODY)
        self.assertEqual(hashlib.sha256(BODY.encode()).hexdigest(), probe.BODY_SHA256)
        self.assertNotIn('entries', variants['global_direct'])
        self.assertIn('MemListNode* (*entries)[] = &lbl_8031EB00;', variants['array_address'])
        self.assertIn('(*entries)[count++]', variants['array_address'])
        self.assertIn('int (*gapCallback)(MemListNode*, u8*)', variants['pdb_int_locals'])
        self.assertIn('u8* currentEnd;', variants['byte_address_node_scoped'])
        self.assertIn('entries[count++] = first;', variants['node_scoped_first'])

    def test_source_shape_and_boundaries_fail_closed(self):
        for body in (BODY.replace('result = 1;', 'result = 0;'), BODY+BODY, BODY+'\n'):
            with self.subTest(body=body[:30]), self.assertRaises(ValueError):
                probe.forms(body)
        text = 'prefix\n'+BODY+'\n/* 0x800D54A4 suffix */'
        prefix, body, suffix = probe.split_source(text)
        self.assertEqual((prefix, body, suffix), ('prefix\n', BODY, '\n/* 0x800D54A4 suffix */'))
        for invalid in ('', text+text, '/* 0x800D54A4 */'+BODY):
            with self.assertRaises(ValueError):
                probe.split_source(invalid)

    def test_complete_raw_fidelity_not_inventory_or_score(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'raw.o'
            path.write_bytes(b'ELF complete metadata')
            probe.require_fidelity(path, None, b'ELF complete metadata')
            for obj, error, wanted in ((path, None, b'ELF same text'), (path, 'rejected option', path.read_bytes()), (None, None, b'')):
                with self.assertRaises(ValueError):
                    probe.require_fidelity(obj, error, wanted)

    def test_freeze_detects_changed_missing_and_roster(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'input'
            path.write_bytes(b'frozen')
            paths, frozen = {'input': path}, {'input': b'frozen'}
            probe.check_frozen(paths, frozen)
            path.write_bytes(b'drift')
            with self.assertRaises(ValueError):
                probe.check_frozen(paths, frozen)
            path.unlink()
            with self.assertRaises(ValueError):
                probe.check_frozen(paths, frozen)
            with self.assertRaises(ValueError):
                probe.check_frozen({}, frozen)

    def test_missing_unreadable_candidate_cannot_report_pass(self):
        # Adversarial compile_with result (None, None) is an explicit failure.
        self.assertIn('no candidate', probe.candidate_error(None, None))
        self.assertEqual(probe.probe_status({'trial': {'error': None}}), 'FAIL')
        self.assertEqual(probe.probe_status({}), 'FAIL')
        self.assertEqual(probe.probe_status({'trial': {'error': None, 'inventory': {}}}), 'PASS')
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'missing.o'
            self.assertIn('missing', probe.candidate_error(path, None))
            path.write_bytes(b'object')
            with mock.patch.object(Path, 'read_bytes', side_effect=PermissionError('denied')):
                self.assertIn('unreadable', probe.candidate_error(path, None))
        self.assertEqual(probe.probe_status({'trial': {'error': 'failure', 'inventory': {}}}), 'FAIL')

    def test_only_owned_body_is_exempted(self):
        before, current = fixture(), fixture()
        current['functions'][probe.FN]['body'] = '55667788'
        current['sections']['.text']['bytes'] = '5566778811223344'
        result = probe.compare_inventory(before, current, fixture())
        self.assertTrue(result['siblings_metadata_equal'])
        self.assertEqual(result['changed_bodies'], [probe.FN])
        self.assertEqual(result['changed_function_metadata'], [])
        self.assertEqual(result['words'], [dict(offset=0, ours='55667788', target='00000000')])
        self.assertEqual(before, fixture())

    def test_metadata_offset_and_binding_changes_are_not_body_changes(self):
        for mutate in (lambda x: x['functions']['sibling'].update(offset=8),
                       lambda x: x['functions']['sibling'].update(relocations=[[0, 6, 'wrong', 0]])):
            current = fixture()
            mutate(current)
            result = probe.compare_inventory(fixture(), current, fixture())
            self.assertEqual(result['changed_bodies'], [])
            self.assertEqual(result['changed_function_metadata'], ['sibling'])
            self.assertFalse(result['siblings_metadata_equal'])
        current = fixture()
        current['sections']['.data']['bytes'] = 'ffffffff'
        result = probe.compare_inventory(fixture(), current, fixture())
        self.assertEqual(result['changed_nontext_sections'], ['.data'])
        self.assertFalse(result['siblings_metadata_equal'])

    def test_count_asymmetry_is_explicit_and_cannot_hide_layout(self):
        current = fixture()
        current['functions'][probe.FN].update(body='00'*8, size=8)
        result = probe.compare_inventory(fixture(), current, fixture())
        self.assertEqual((result['target_insns'], result['ours_insns']), (1, 2))
        self.assertIsNone(result['differing_words'])
        self.assertIsNone(result['words'])
        self.assertFalse(result['siblings_metadata_equal'])
        self.assertEqual(result['changed_function_metadata'], [probe.FN])

    def test_output_guard_and_no_overwrite(self):
        with tempfile.TemporaryDirectory() as folder, mock.patch.object(probe, 'ROOT', Path(folder)):
            (Path(folder)/'build').mkdir()
            out = Path(folder)/'build/r76_pool_fixture'
            self.assertEqual(probe.output_directory(out), out)
            for bad in (out, Path(folder)/'r76_pool_outside', Path(folder)/'build/foreign'):
                with self.assertRaises(ValueError):
                    probe.output_directory(bad)

    def test_silent_import_has_no_experiment_io(self):
        spec = importlib.util.spec_from_file_location('r76_pool_probe_test_import', probe.__file__)
        module = importlib.util.module_from_spec(spec)
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err), \
             mock.patch.object(probe.cv, 'read_edges', side_effect=AssertionError('import read build')), \
             mock.patch.object(Path, 'read_bytes', side_effect=AssertionError('import read input')), \
             mock.patch.object(Path, 'write_text', side_effect=AssertionError('import wrote input')):
            spec.loader.exec_module(module)
        self.assertEqual((out.getvalue(), err.getvalue()), ('', ''))


if __name__ == '__main__':
    unittest.main()
