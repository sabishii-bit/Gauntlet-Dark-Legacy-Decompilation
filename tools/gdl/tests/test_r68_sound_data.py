"""Pure safety/scope controls for the sound data reconstruction evidence."""
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from tools.gdl.composed_census import r68_sound_data_audit as audit
from tools.gdl.composed_census import r68_sound_data_recovery as pdb


def record(kind, body=b""):
    return struct.pack("<HH", len(body)+2, kind) + body


def data_record(name, offset=0):
    encoded = name.encode()
    return record(0x1007, struct.pack("<IIHB", 0x2488, offset, 9, len(encoded)) + encoded)


class SoundDataTests(unittest.TestCase):
    def test_pdb_rejects_wrong_and_truncated_headers(self):
        with self.assertRaisesRegex(ValueError, "expected PDB"):
            pdb.pdb_streams(b"random")
        with self.assertRaisesRegex(ValueError, "truncated PDB"):
            pdb.pdb_streams(b"Microsoft C/C++ program database 2.00")

    def test_pdb_rejects_invalid_page_size(self):
        header = b"Microsoft C/C++ program database 2.00".ljust(64, b"\0")
        with self.assertRaisesRegex(ValueError, "invalid page size"):
            pdb.pdb_streams(header)

    def test_explicit_proc_end_decides_scope_not_last_seen_function(self):
        name = b"AudioPlayerTurbo"
        inside = data_record("snd_turboA")
        proc_size = 4+35+1+len(name)
        pend = 4+proc_size+len(inside)
        proc = record(0x100B, struct.pack("<8IHB", 0, pend, 0, 10, 0, 10, 0, 123, 1, 0)
                      + bytes([len(name)]) + name)
        buf = bytes(4) + proc + inside + record(6) + data_record("file_static")
        rows, procs = pdb.sound_symbols(buf, lambda index: {"type_index": index})
        self.assertEqual(rows[0]["enclosing_function"], "AudioPlayerTurbo")
        self.assertIsNone(rows[1]["enclosing_function"])
        self.assertEqual(procs[0]["end_record_offset"], pend)

    def test_invalid_symbol_length_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "invalid symbol record"):
            pdb.sound_symbols(bytes(4) + struct.pack("<HH", 99, 0x1007), lambda n: n)

    def test_nested_array_dimensions(self):
        child = record(0x1003, struct.pack("<IIH", 0x74, 0x74, 16) + b"\0")
        parent = record(0x1003, struct.pack("<IIH", 0x1000, 0x74, 128) + b"\0")
        tpi = struct.pack("<5I", 19990903, 20, 0x1000, 0x1002, len(child)+len(parent)) + child + parent
        array = pdb.describe_types(tpi)(0x1001)
        self.assertEqual((array["count"], array["element"]["count"]), (8, 4))
        self.assertEqual(array["element"]["element"]["c_type"], "int")

    def test_audit_rejects_non_lane_output_before_compiling(self):
        with tempfile.TemporaryDirectory(prefix="r68_sound_test_") as td:
            root = Path(td)
            with patch.object(audit, "ROOT", root), patch.object(audit.r67.cv, "read_edges", side_effect=AssertionError("read graph")):
                for name in ("src/sounds.c", "build/report.json", "build/r68_sound.c"):
                    with self.assertRaisesRegex(ValueError, "build/r68_sound"):
                        audit.audit(root / name)

    def test_data_certificate_requires_full_extent(self):
        dat = SimpleNamespace(index=1, name=".data", offset=0, size=4232)
        path = SimpleNamespace(read_bytes=lambda: bytes(4232))
        with patch.object(audit.r67.wf, "_sections", return_value=[dat]), patch.object(audit.r67.wf, "_symbol_index", return_value={}):
            with self.assertRaisesRegex(ValueError, "4492"):
                audit.data_certificate(path)

    def test_data_certificate_requires_every_pointer_site(self):
        dat = SimpleNamespace(index=1, name=".data", offset=0, size=4492)
        path = SimpleNamespace(read_bytes=lambda: bytes(4492))
        with patch.object(audit.r67.wf, "_sections", return_value=[dat]), \
                patch.object(audit.r67.wf, "_symbol_index", return_value={}), \
                patch.object(audit.r67.wf, "_function_text_relocations_full", return_value={}), \
                patch.object(audit.r67.sp, "dol_read", return_value=bytes(4492)):
            with self.assertRaisesRegex(ValueError, "32 bank and 65"):
                audit.data_certificate(path)

    def test_literal_requires_bounded_own_section_nul(self):
        section = SimpleNamespace(offset=0, size=4)
        with self.assertRaisesRegex(ValueError, "outside"):
            audit.literal_certificate(b"abc\0", section, 4, 0x80114A48)
        with self.assertRaisesRegex(ValueError, "bounded NUL"):
            audit.literal_certificate(b"abcd", section, 0, 0x80114A48)
        with self.assertRaisesRegex(ValueError, "bounded NUL"):
            audit.literal_certificate(b"abc\0", section, 0, 0x80114A48, limit=3)

    def test_literal_mutation_fails_even_when_every_pointer_is_exact(self):
        sections = [SimpleNamespace(name="", index=0),
                    SimpleNamespace(index=1, name=".data", offset=0, size=4492),
                    SimpleNamespace(index=2, name=".rodata", offset=4492, size=4),
                    SimpleNamespace(index=3, name=".text", offset=4496, size=4)]
        symbols = {"@bank": SimpleNamespace(name="@bank", section_index=2, value=0, size=4),
                   "AudioPlayerTurbo": SimpleNamespace(name="AudioPlayerTurbo", section_index=3, value=0, size=4)}
        relocs = {at: (1, "@bank", 0) for at in range(0x54, 0xD4, 4)}
        relocs.update({at: (1, "AudioPlayerTurbo", 0) for at in range(0x1088, 0x118C, 4)})
        target = bytearray(4492)
        for at in relocs:
            struct.pack_into(">I", target, at, 0x80114A48 if at < 0xD4 else 0x800A0000)

        def retail(address, size):
            if address == 0x801232C8:
                return bytes(target[:size])
            self.assertEqual(address, 0x80114A48)
            return b"foo\0"[:size]

        with patch.object(audit.r67.wf, "_sections", return_value=sections), \
                patch.object(audit.r67.wf, "_symbol_index", return_value=symbols), \
                patch.object(audit.r67.wf, "_function_text_relocations_full", return_value=relocs), \
                patch.object(audit.r67.sp, "dol_read", side_effect=retail), \
                patch.object(Path, "read_text", return_value="AudioPlayerTurbo = .text:0x800A0000;"):
            for literal, expected in ((b"foo\0", True), (b"goo\0", False), (b"fo\0\0", False)):
                path = SimpleNamespace(read_bytes=lambda: bytes(4492) + literal + bytes(4))
                result = audit.data_certificate(path)
                self.assertTrue(result["all_equal"])
                self.assertEqual(result["literal_contents_equal"], expected)
                self.assertEqual(len(result["literal_contents"]), 32)


if __name__ == "__main__":
    unittest.main()
