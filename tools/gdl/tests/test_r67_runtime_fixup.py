"""Runtime compatibility boundary controls; no private binaries required."""
import contextlib
import hashlib
import io
import os
from pathlib import Path
import runpy
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from tools import fix_exception_objects as fix
from tools.gdl.composed_census import r67_runtime_verify as verify


class RuntimeFixupTests(unittest.TestCase):
    def test_unknown_input_refuses_before_transform_and_preserves_output(self):
        with tempfile.TemporaryDirectory(prefix="r67_runtime_test_") as td:
            source, output = Path(td) / "raw.o", Path(td) / "fixed.o"
            source.write_bytes(b"MODIFIED!")
            output.write_bytes(b"previous output")
            with patch.object(fix, "fix_nmw", side_effect=AssertionError("rewritten")):
                with self.assertRaisesRegex(ValueError, "changed source/compiler"):
                    fix.fix_object("nmw", source, output)
            self.assertEqual(source.read_bytes(), b"MODIFIED!")
            self.assertEqual(output.read_bytes(), b"previous output")

    def test_same_path_refuses_before_rewrite(self):
        with tempfile.TemporaryDirectory(prefix="r67_runtime_test_") as td:
            source = Path(td) / "raw.o"
            source.write_bytes(b"keep raw")
            with self.assertRaisesRegex(ValueError, "distinct files"):
                fix.fix_object("nmw", source, source)
            self.assertEqual(source.read_bytes(), b"keep raw")

    def test_hardlink_alias_refuses_before_rewrite(self):
        with tempfile.TemporaryDirectory(prefix="r67_runtime_test_") as td:
            source, output = Path(td) / "raw.o", Path(td) / "alias.o"
            source.write_bytes(b"keep raw")
            try:
                os.link(source, output)
            except OSError as error:
                self.skipTest(f"hard links unavailable: {error}")
            with self.assertRaisesRegex(ValueError, "distinct files"):
                fix.fix_object("nmw", source, output)

    def guarded_fixture(self, source, output, transformed=b"fixed", expected=b"fixed", concurrent=False):
        before = b"raw"
        source.write_bytes(before)
        output.write_bytes(b"previous")
        before_stat = source.stat()

        def transform(scratch):
            self.assertNotEqual(Path(scratch), source)
            self.assertEqual(Path(scratch).read_bytes(), before)
            Path(scratch).write_bytes(transformed)
            if concurrent:
                source.write_bytes(b"concurrent source")

        guard = (hashlib.sha256(before).hexdigest(), hashlib.sha256(expected).hexdigest())
        fake_elf = SimpleNamespace(sec={}, save=lambda: None)
        with patch.dict(fix.OBJECT_HASHES, {"nmw": guard}), \
             patch.object(fix, "fix_nmw", side_effect=transform), \
             patch.object(fix, "Elf", return_value=fake_elf):
            result = fix.fix_object("nmw", source, output)
        self.assertEqual(source.read_bytes(), before)
        self.assertEqual(source.stat().st_mtime_ns, before_stat.st_mtime_ns)
        return result

    def test_verified_output_is_separate_and_raw_mtime_is_untouched(self):
        with tempfile.TemporaryDirectory(prefix="r67_runtime_test_") as td:
            source, output = Path(td) / "raw.o", Path(td) / "fixed.o"
            result = self.guarded_fixture(source, output)
            self.assertTrue(result["raw_retained"])
            self.assertEqual(output.read_bytes(), b"fixed")
            self.assertEqual(sorted(p.name for p in Path(td).iterdir()), ["fixed.o", "raw.o"])

    def test_bad_postimage_preserves_previous_output_and_cleans_scratch(self):
        with tempfile.TemporaryDirectory(prefix="r67_runtime_test_") as td:
            source, output = Path(td) / "raw.o", Path(td) / "fixed.o"
            with self.assertRaisesRegex(ValueError, "fixed object hash"):
                self.guarded_fixture(source, output, transformed=b"wrong")
            self.assertEqual(output.read_bytes(), b"previous")
            self.assertEqual(source.read_bytes(), b"raw")
            self.assertEqual(sorted(p.name for p in Path(td).iterdir()), ["fixed.o", "raw.o"])

    def test_concurrent_raw_change_refuses_publication(self):
        with tempfile.TemporaryDirectory(prefix="r67_runtime_test_") as td:
            source, output = Path(td) / "raw.o", Path(td) / "fixed.o"
            with self.assertRaisesRegex(ValueError, "changed during"):
                self.guarded_fixture(source, output, concurrent=True)
            self.assertEqual(source.read_bytes(), b"concurrent source")
            self.assertEqual(output.read_bytes(), b"previous")

    def test_old_inplace_cli_is_not_supported(self):
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as raised:
                fix.main(["a.o", "b.o", "stamp"])
        self.assertEqual(raised.exception.code, 2)


class RuntimeBuildConfigTests(unittest.TestCase):
    def config(self, *args):
        from tools import project
        with patch.object(sys, "argv", ["configure.py", *args]), \
             patch.object(project, "generate_build") as generate:
            runpy.run_path(str(verify.ROOT / "configure.py"), run_name="__main__")
        generate.assert_called_once()
        return generate.call_args.args[0]

    def test_native_matching_links_recovered_exceptionppc_without_rewriting(self):
        config = self.config()
        self.assertTrue(config.native_only)
        self.assertEqual(config.object_postprocesses,{})
        self.assertEqual(config.custom_build_rules,[])
        self.assertEqual(config.custom_build_steps,{})
        self.assertFalse(config.objects()['Runtime.PPCEABI.H/NMWException.cpp'].completed)
        self.assertTrue(config.objects()['Runtime.PPCEABI.H/ExceptionPPC.cpp'].completed)

    def test_exceptionppc_claim_includes_discarded_weak_vtable_extent(self):
        from tools.gdl.pool_owner import load_splits
        rows = [(start, end) for unit, section, start, end in load_splits()
                if unit == 'Runtime.PPCEABI.H/ExceptionPPC' and section == '.data']
        # The duplicate weak exception vtable is discarded by mwld, but its
        # sixteen-byte zero extent remains. A separate auto object for this
        # tail duplicates it and moves __files and its consumers by 16 bytes.
        self.assertEqual(rows, [(0x802383B8, 0x802384B0)])

    def test_editable_mode_has_no_retail_byte_rewrites(self):
        config = self.config("--non-matching")
        self.assertEqual(config.object_postprocesses, {})
        self.assertEqual(config.custom_build_steps, {})
        self.assertNotIn("fix_exception_objects", {r["name"] for r in config.custom_build_rules})

    def test_runtime_stage_does_not_hardcode_build_directory(self):
        config = self.config("--build-dir", "build/r67_runtime_alternate")
        self.assertEqual(config.build_dir, Path("build/r67_runtime_alternate"))
        resolved = next(obj.resolve(config, lib) for lib in config.libs for obj in lib["objects"]
                        if obj.name == "Runtime.PPCEABI.H/NMWException.cpp")
        self.assertEqual(resolved.src_obj_path, Path("build/r67_runtime_alternate/GUNE5D/src/Runtime.PPCEABI.H/NMWException.o"))


class RuntimeLinkedVerifierTests(unittest.TestCase):
    def fixture(self, root):
        function_address, string_address = 0x80004000, 0x80018010
        body = struct.pack(">3I", 0x3C608002, 0x38638010, 0x4E800020)
        literal = b"MODIFIED!\0"
        data = body + b"\0" * 4 + literal
        fake = SimpleNamespace(
            data=data, symcount=1,
            symname=lambda index: verify.prior.FUNCTION,
            sym=lambda index: [0, function_address, 12, 0x12, 0, 1],
            sh=[[0, 1, 2, function_address, 0, 12], [0, 1, 2, string_address, 16, len(literal)]],
        )
        elf_path, dol_path = root / "main.elf", root / "main.dol"
        elf_path.write_bytes(b"fixture ELF; parsed with a controlled model")
        dol = bytearray(0x100) + body + literal
        struct.pack_into(">I", dol, 0, 0x100)
        struct.pack_into(">I", dol, 7 * 4, 0x10C)
        struct.pack_into(">I", dol, 0x48, function_address)
        struct.pack_into(">I", dol, 0x48 + 7 * 4, string_address)
        struct.pack_into(">I", dol, 0x90, 12)
        struct.pack_into(">I", dol, 0x90 + 7 * 4, len(literal))
        dol_path.write_bytes(dol)
        return fake, elf_path, dol_path

    def test_resolves_signed_low_half_and_compares_dol_value(self):
        with tempfile.TemporaryDirectory(prefix="r67_runtime_test_") as td:
            fake, elf_path, dol_path = self.fixture(Path(td))
            with patch.object(verify.fix, "Elf", return_value=fake):
                result = verify.linked_observation(elf_path, dol_path)
            self.assertEqual(result["string"], "MODIFIED!")
            self.assertEqual(result["string_address"], "0x80018010")
            self.assertTrue(result["elf_and_dol_agree"])

    def test_matching_instructions_do_not_hide_wrong_linked_datum(self):
        with tempfile.TemporaryDirectory(prefix="r67_runtime_test_") as td:
            fake, elf_path, dol_path = self.fixture(Path(td))
            dol = bytearray(dol_path.read_bytes())
            dol[0x10C] = ord("X")
            dol_path.write_bytes(dol)
            with patch.object(verify.fix, "Elf", return_value=fake):
                with self.assertRaisesRegex(ValueError, "actual datum"):
                    verify.linked_observation(elf_path, dol_path)

    def test_unsupported_function_shape_refuses(self):
        with tempfile.TemporaryDirectory(prefix="r67_runtime_test_") as td:
            fake, elf_path, dol_path = self.fixture(Path(td))
            fake.data = b"\0" * 4 + fake.data[4:]
            with patch.object(verify.fix, "Elf", return_value=fake):
                with self.assertRaisesRegex(ValueError, "unsupported linked"):
                    verify.linked_observation(elf_path, dol_path)

    def test_output_cannot_clobber_build_snapshot(self):
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as raised:
                verify.main(["--mode", "matching", "--out", "build/GUNE5D/build_edges.json"])
        self.assertEqual(raised.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
