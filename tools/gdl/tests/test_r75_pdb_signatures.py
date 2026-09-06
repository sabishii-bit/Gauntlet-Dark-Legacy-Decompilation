"""Synthetic signature/scope tests; supports --draft PATH before promotion."""
import argparse
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path.cwd()))
DRAFT = None
MODULE = None


def load(path):
    spec = importlib.util.spec_from_file_location('r75_pdb_signatures_under_test', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


if __name__ != '__main__':
    from tools.gdl.composed_census import r75_pdb_signatures as MODULE
    DRAFT = Path(MODULE.__file__)


def record(kind, body=b''):
    return struct.pack('<HH', len(body)+2, kind)+body


def name(text):
    data = text.encode('ascii')
    return bytes([len(data)])+data


def types():
    rows = [record(0x1201, struct.pack('<II', 1, 0x74)),
            record(0x1008, struct.pack('<IBBHI', 0x403, 0, 0, 1, 0x1000)),
            record(0x1201, struct.pack('<II', 1, 0x75)),
            record(0x1008, struct.pack('<IBBHI', 0x403, 0, 0, 1, 0x1002))]
    body = b''.join(rows)
    return struct.pack('<5I', 19961031, 56, 0x1000, 0x1004, len(body))+bytes(36)+body


def procedure(text, typ, start, local=False):
    arg = record(0x1006, struct.pack('<iI', 4, 0x74 if typ == 0x1001 else 0x75)+name('amount'))
    local_rec = record(0x1006, struct.pack('<iI', -4, 0x74)+name('bidx')) if local else b''
    def header(end):
        return record(0x100b, struct.pack('<8IHB', 0, end, 0, 20, 0, 19, typ, 0x100, 1, 0)+name(text))
    end = start+len(header(0))+len(arg)+len(local_rec)
    return header(end)+arg+local_rec+record(6)


def symbols():
    first = procedure('Signed', 0x1001, 4, True)
    return struct.pack('<I', 2)+first+procedure('Unsigned', 0x1003, 4+len(first))


def streams():
    sym = symbols()
    mod = bytearray(64)
    struct.pack_into('<H', mod, 34, 4)
    struct.pack_into('<I', mod, 36, len(sym))
    mod += b'.\\Release\\AUDIO.OBJ\0fixture.obj\0'
    mod += bytes((-len(mod)) % 4)
    dbi = bytearray(64)
    struct.pack_into('<I', dbi, 24, len(mod))
    return [b'', b'', types(), bytes(dbi+mod), sym]


class SignatureTests(unittest.TestCase):
    def test_signed_unsigned_and_raw_records(self):
        result = MODULE.inspect_streams(streams(), {'AUDIO.OBJ': frozenset(('Signed', 'Unsigned'))})
        rows = result['modules'][0]['records']
        args = [r['procedure']['type']['arguments']['arguments'][0] for r in rows]
        self.assertEqual([a['primitive'] for a in args], ['int32', 'uint32'])
        self.assertEqual([a['index'] for a in args], ['0x74', '0x75'])
        self.assertEqual(bytes.fromhex(rows[0]['procedure']['bytes']),
                         symbols()[4:4+len(bytes.fromhex(rows[0]['procedure']['bytes']))])

    def test_parameter_and_local_inventory_stays_inside_endpoint(self):
        _, describe, _ = MODULE.parse_types(types())
        rows, roster = MODULE.symbol_records(symbols(), describe, {'Signed', 'Unsigned'})
        children = rows[0]['children']
        self.assertEqual([(r.get('name'), r.get('offset')) for r in children],
                         [('amount', 4), ('bidx', -4), (None, None)])
        self.assertEqual(children[0]['lexical_scope_offsets'], ['0x4'])
        self.assertEqual(children[1]['lexical_scope_offsets'], ['0x4'])
        self.assertEqual(children[2]['lexical_scope_offsets'], [])
        self.assertEqual(roster, ['Signed', 'Unsigned'])
        self.assertEqual(len(rows[1]['children']), 2)

    def test_endpoint_nonrecord_backward_and_nonend_refused(self):
        _, describe, _ = MODULE.parse_types(types())
        for bad in (3, 4, 5, 0xffffffff):
            buf = bytearray(symbols())
            struct.pack_into('<I', buf, 12, bad)
            with self.subTest(end=bad), self.assertRaises(ValueError):
                MODULE.symbol_records(bytes(buf), describe, {'Signed'})

    def test_nested_block_parent_and_inventory(self):
        _, describe, _ = MODULE.parse_types(types())
        def proc(end):
            return record(0x100b, struct.pack('<8IHB', 0,end,0,20,0,19,0x1001,0x100,1,0)+name('Signed'))
        block_at = 4+len(proc(0))
        def block(end, parent=4):
            return record(0x207, struct.pack('<4IH', parent,end,10,0x104,1)+name(''))
        local = record(0x1006, struct.pack('<iI', -4,0x74)+name('inner'))
        block_end = block_at+len(block(0))+len(local)
        buf = struct.pack('<I',2)+proc(block_end+4)+block(block_end)+local+record(6)+record(6)
        rows, _ = MODULE.symbol_records(buf, describe, {'Signed'})
        inner = next(r for r in rows[0]['children'] if r.get('name') == 'inner')
        self.assertEqual(inner['lexical_scope_offsets'], ['0x4', hex(block_at)])
        bad = bytearray(buf)
        struct.pack_into('<I', bad, block_at+4, block_at)
        with self.assertRaises(ValueError):
            MODULE.symbol_records(bytes(bad), describe, {'Signed'})

    def test_truncated_and_malformed_records_refused(self):
        for buf in (b'', types()[:-1], types()+b''):
            if buf == types():
                buf = bytearray(buf)
                struct.pack_into('<H', buf, 56, 0)
            with self.subTest(length=len(buf)), self.assertRaises(ValueError):
                MODULE.parse_types(buf)
        _, describe, _ = MODULE.parse_types(types())
        for buf in (b'', symbols()[:-1], symbols()+b'x'):
            with self.subTest(length=len(buf)), self.assertRaises(ValueError):
                MODULE.symbol_records(buf, describe, {'Signed'})
        bad = streams()
        bad[3] = bad[3][:-1]
        with self.assertRaises(ValueError):
            MODULE.inspect_streams(bad, {'AUDIO.OBJ': frozenset(('Signed',))})
        with tempfile.TemporaryDirectory(prefix='r75_pdb_test_') as folder:
            path = Path(folder)/'invalid.pdb'
            path.write_bytes(b'not a PDB')
            with self.assertRaises(ValueError):
                MODULE.inspect_pdb(path)

    def test_argument_count_mismatch_and_missing_roster_refused(self):
        bad = bytearray(types())
        proc_at = 56+len(record(0x1201, struct.pack('<II',1,0x74)))
        struct.pack_into('<H', bad, proc_at+10, 2)
        _, describe, _ = MODULE.parse_types(bytes(bad))
        with self.assertRaises(ValueError):
            describe(0x1001)
        with self.assertRaises(ValueError):
            MODULE.inspect_streams(streams(), {'AUDIO.OBJ': frozenset(('Absent',))})

    def test_import_is_silent_and_no_data_io(self):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err), \
             mock.patch.object(Path, 'read_bytes', side_effect=AssertionError('import read data')), \
             mock.patch.object(Path, 'write_text', side_effect=AssertionError('import wrote data')):
            load(DRAFT)
        self.assertEqual(out.getvalue(), '')
        self.assertEqual(err.getvalue(), '')

    def test_output_path_guard(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit), \
             mock.patch.object(MODULE, 'inspect_pdb', side_effect=AssertionError('should not read')):
            MODULE.main(['--out', 'r75_pdb_bad.json'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--draft', type=Path, required=True)
    parser.add_argument('--live', type=Path)
    args = parser.parse_args()
    DRAFT = args.draft.resolve()
    MODULE = load(DRAFT)
    # The draft has the eventual parents[3] ROOT; promotion keeps that exact line.
    MODULE.ROOT = Path.cwd()
    result = unittest.TextTestRunner(buffer=True).run(unittest.defaultTestLoader.loadTestsFromTestCase(SignatureTests))
    if not result.wasSuccessful():
        raise SystemExit(1)
    if args.live:
        MODULE.main([str(args.live), '--out', 'build/r75_pdb_signatures_live.json'])
