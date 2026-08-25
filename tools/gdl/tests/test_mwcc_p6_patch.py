import hashlib
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.gdl.mwcc_p6 import patch_pe


class MwccP6PatchTests(unittest.TestCase):
    def test_alignment(self):
        self.assertEqual(patch_pe.align(0, 0x200), 0)
        self.assertEqual(patch_pe.align(1, 0x200), 0x200)
        self.assertEqual(patch_pe.align(0x400, 0x200), 0x400)

    def test_input_and_output_must_differ(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            compiler = root / "mwcceppc-input.exe"
            payload = root / "payload.bin"
            compiler.write_bytes(b"not a compiler")
            payload.write_bytes(b"not a payload")
            with patch.object(
                sys,
                "argv",
                ["patch_pe.py", str(compiler), str(payload), str(compiler)],
            ):
                with self.assertRaisesRegex(SystemExit, "input and output paths"):
                    patch_pe.main()

    def test_payload_and_output_must_differ(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            compiler = root / "mwcceppc-input.exe"
            payload = root / "mwcceppc-payload.exe"
            compiler.write_bytes(b"not a compiler")
            payload.write_bytes(b"not a payload")
            with patch.object(
                sys,
                "argv",
                ["patch_pe.py", str(compiler), str(payload), str(payload)],
            ):
                with self.assertRaisesRegex(SystemExit, "payload and output paths"):
                    patch_pe.main()

    def test_unknown_compiler_fails_without_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            compiler = root / "mwcceppc-input.exe"
            payload = root / "payload.bin"
            output = root / "mwcceppc-derived.exe"
            compiler.write_bytes(b"not a compiler")
            payload.write_bytes(b"not a payload")
            expected = hashlib.sha256(compiler.read_bytes()).hexdigest()
            with patch.object(
                sys,
                "argv",
                ["patch_pe.py", str(compiler), str(payload), str(output)],
            ):
                with self.assertRaisesRegex(SystemExit, expected):
                    patch_pe.main()
            self.assertFalse(output.exists())

    def test_output_name_is_explicit_and_mwcc_safe(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            compiler = root / "mwcceppc-input.exe"
            payload = root / "payload.bin"
            output = root / "derived_compiler.exe"
            compiler.write_bytes(b"not a compiler")
            payload.write_bytes(b"not a payload")
            with patch.object(
                sys,
                "argv",
                ["patch_pe.py", str(compiler), str(payload), str(output)],
            ):
                with self.assertRaisesRegex(SystemExit, "MWCC-safe"):
                    patch_pe.main()


if __name__ == "__main__":
    unittest.main()
