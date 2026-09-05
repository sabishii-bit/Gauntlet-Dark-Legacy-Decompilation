"""Pure controls for the isolated sound-boundary experiment."""
from pathlib import Path
import json
import struct
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from tools.gdl.composed_census import r67_sound_boundary_probe as probe
from tools.gdl.composed_census import r67_sound_pdb_type_probe as pdb


class SoundBoundaryProbeTests(unittest.TestCase):
    def test_rejects_production_or_non_lane_output_before_compile(self):
        with tempfile.TemporaryDirectory(prefix="r67_sound_test_") as td:
            root = Path(td)
            with patch.object(probe, "ROOT", root), patch.object(
                    probe.cv, "read_edges", side_effect=AssertionError("read build")):
                for name in ("src/sounds.c", "build/GUNE5D/report.json", "build/r67_sound.c"):
                    with self.assertRaisesRegex(ValueError, "build/r67_sound"):
                        probe.run(root / name)

    def test_merge_seams_preserve_required_return_and_reset_is_explicit(self):
        evt = ("extern int AudioWithName(int id, int pidx, f32 vol, int s4, int s5);\n"
               "extern void sndFxPlay3DTracked(int soundId, int pos, int p2, int flags);\n")
        sound = "extern int sndFxPlay3DTracked(int a, int b, int c, int d);\n"
        a, b, joined = probe.merge_sources(evt, sound)
        self.assertIn("extern void AudioWithName", a)
        self.assertIn("extern int sndFxPlay3DTracked", a)
        self.assertEqual(b, sound)
        self.assertIn("#undef offsetof", joined)
        self.assertNotIn("#pragma", joined)
        self.assertIn("#pragma peephole reset", probe.merge_sources(evt, sound, reset=True)[2])

    def test_literal_rewrite_refuses_missing_site_or_decl(self):
        with patch.object(probe.sp, "pool_entry", return_value="S_%sBITE"):
            with self.assertRaisesRegex(ValueError, "expected 19"):
                probe.boss_literals("formats + 512\n")
            with self.assertRaisesRegex(ValueError, "declaration drifted"):
                probe.boss_literals("formats + 512\n" * 19)
            source = "    register char* formats = lbl_80114A48;\n" + "formats + 512\n" * 19
            result = probe.boss_literals(source)
            self.assertEqual(result.count('"S_%sBITE"'), 19)
            self.assertNotIn("formats", result)

    def test_missing_functions_do_not_count_as_equal(self):
        row = probe.compare_roster({"a": ["blr"], "b": ["blr"]}, {"a": ["blr"]})
        self.assertEqual(row["functions"], 2)
        self.assertEqual(row["normalized_equal"], 1)
        self.assertEqual(row["changed"], {"b": "MISS"})

    def test_census_reads_current_config_not_stale_asm(self):
        with tempfile.TemporaryDirectory(prefix="r67_sound_test_") as td:
            root = Path(td)
            folder = root / "build/GUNE5D/asm"
            folder.mkdir(parents=True)
            (folder.parent / "config.json").write_text(json.dumps({"units": [
                {"object": "build/GUNE5D/obj/live.o"}]}))
            (folder / "live.s").write_text(".obj live, global\n.4byte lbl_80114A48\n")
            (folder / "stale.s").write_text(".obj stale, global\n.4byte lbl_80114A48\n")
            with patch.object(probe, "ROOT", root), patch.object(probe.sp, "pool_entry", return_value="abc"):
                row = probe.literal_prefix_census()
            self.assertEqual([r["owner"] for r in row["references"]], ["live"])
            self.assertEqual(row["missing_current_asm"], [])

    def test_data_certificate_requires_every_site_and_defined_addr32(self):
        null = SimpleNamespace(index=0, name="", offset=0, size=0)
        dat = SimpleNamespace(index=1, name=".data", offset=0, size=0x154)
        pool = SimpleNamespace(index=2, name=".rodata", offset=0x154, size=4)
        symbol = SimpleNamespace(section_index=2, value=0)
        raw = bytes(0x154) + b"foo\0"
        target = bytearray(0x154)
        for at in range(0x54, 0xD4, 4):
            struct.pack_into(">I", target, at, 0x80114A48)
        relocs = {at: (1, "s", 0) for at in range(0x54, 0xD4, 4)}
        def dol_read(address, size):
            return bytes(target) if address == 0x801232C8 else b"foo\0"
        path = SimpleNamespace(read_bytes=lambda: raw)
        with patch.object(probe.wf, "_sections", return_value=[null, dat, pool]), \
                patch.object(probe.wf, "_symbol_index", return_value={"s": symbol}), \
                patch.object(probe.wf, "_function_text_relocations_full", return_value=relocs), \
                patch.object(probe.sp, "dol_read", side_effect=dol_read):
            good = probe.typed_island_check(path)
            self.assertTrue(good["equal_at_retail_bases"])
            self.assertEqual(good["pointer_count"], 32)
            relocs[0x54] = (4, "s", 0)
            with self.assertRaisesRegex(ValueError, "defined R_PPC_ADDR32"):
                probe.typed_island_check(path)
            relocs[0x54] = (1, "missing", 0)
            with self.assertRaisesRegex(ValueError, "defined R_PPC_ADDR32"):
                probe.typed_island_check(path)
            del relocs[0x54]
            with self.assertRaisesRegex(ValueError, "all 32"):
                probe.typed_island_check(path)

    def test_pdb_reader_refuses_non_pdb_and_truncated_header(self):
        with tempfile.TemporaryDirectory(prefix="r67_sound_test_") as td:
            path = Path(td) / "x.pdb"
            path.write_bytes(b"not a PDB")
            with self.assertRaisesRegex(ValueError, "expected PDB"):
                pdb.inspect(path)
            path.write_bytes(b"Microsoft C/C++ program database 2.00")
            with self.assertRaisesRegex(ValueError, "truncated PDB"):
                pdb.inspect(path)
            path.write_bytes(b"Microsoft C/C++ program database 2.00".ljust(64, b"\0"))
            with self.assertRaisesRegex(ValueError, "invalid PDB page size"):
                pdb.inspect(path)


if __name__ == "__main__":
    unittest.main()
