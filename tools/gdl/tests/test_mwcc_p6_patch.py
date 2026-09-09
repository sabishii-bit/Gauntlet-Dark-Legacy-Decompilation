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

    def test_patch_call_verifies_and_encodes_relative_target(self):
        image = bytearray(b"xx" + patch_pe.ORIGINAL_CALLSITE + b"yy")
        patch_pe.patch_call(
            image, 2, patch_pe.CALL_VA, 0x005A5000,
            patch_pe.ORIGINAL_CALLSITE, "P6 layout",
        )
        displacement = 0x005A5000 - (patch_pe.CALL_VA + 5)
        self.assertEqual(image[2:7], b"\xE8" + displacement.to_bytes(
            4, byteorder="little", signed=True
        ))

    def test_patch_call_rejects_tampered_callsite(self):
        image = bytearray(b"\x90" * 5)
        with self.assertRaisesRegex(ValueError, "dead-removal"):
            patch_pe.patch_call(
                image, 0, patch_pe.DEAD_REMOVE_CALL_VA, 0x005A5020,
                patch_pe.ORIGINAL_DEAD_REMOVE_CALLSITE, "dead-removal",
            )

    def test_payload_absolute_operands_are_position_pinned(self):
        payload = bytearray(0x6B)
        for offset, value in patch_pe.ABSOLUTE_OPERANDS.items():
            payload[offset:offset + 4] = value.to_bytes(4, "little")
        patch_pe.validate_payload_operands(payload)
        payload[0x67] ^= 1
        with self.assertRaisesRegex(ValueError, "absolute operands moved"):
            patch_pe.validate_payload_operands(payload)

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
