import hashlib
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.gdl.mwcc_p6 import setup_compilers as setup


class MwccSpecialCompilerSetupTests(unittest.TestCase):
    def fixture(self, root: Path):
        for version in ("1.2.5", "1.2.5n"):
            directory = root / "GC" / version
            directory.mkdir(parents=True)
            (directory / "mwcceppc.exe").write_bytes(version.encode())
            (directory / "lmgr326b.dll").write_bytes((version + "-dll").encode())
        payload = root / "payload.bin"
        payload.write_bytes(b"reviewed payload")
        return payload

    def test_installs_both_profiles_only_after_both_derivations_pass(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            payload = self.fixture(root)
            stock_output = b"derived stock"
            ninji_output = b"derived ninji"
            specs = (
                ("1.2.5", "1.2.5s", "mwcceppc-125s.exe",
                 hashlib.sha256(stock_output).hexdigest()),
                ("1.2.5n", "1.2.5sn", "mwcceppc-125sn.exe",
                 hashlib.sha256(ninji_output).hexdigest()),
            )

            def fake_run(argv, *, check):
                self.assertTrue(check)
                source = Path(argv[2])
                output = Path(argv[4])
                output.write_bytes(
                    stock_output if source.parent.name == "1.2.5" else ninji_output
                )
                return subprocess.CompletedProcess(argv, 0)

            with (
                patch.object(setup, "SPECIAL_COMPILERS", specs),
                patch.object(
                    setup.patch_pe,
                    "PAYLOAD_SHA256",
                    hashlib.sha256(payload.read_bytes()).hexdigest(),
                ),
            ):
                installed = setup.install_special_compilers(
                    root, payload, run=fake_run
                )

            self.assertEqual(
                installed,
                [
                    root / "GC/1.2.5s/mwcceppc.exe",
                    root / "GC/1.2.5sn/mwcceppc.exe",
                ],
            )
            self.assertEqual(installed[0].read_bytes(), stock_output)
            self.assertEqual(installed[1].read_bytes(), ninji_output)
            self.assertEqual(
                (root / "GC/1.2.5s/lmgr326b.dll").read_bytes(), b"1.2.5-dll"
            )
            self.assertEqual(
                (root / "GC/1.2.5sn/lmgr326b.dll").read_bytes(),
                b"1.2.5n-dll",
            )

    def test_missing_payload_fails_before_creating_profiles(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "GC").mkdir()
            with self.assertRaisesRegex(ValueError, "payload does not exist"):
                setup.install_special_compilers(root, root / "missing.bin")
            self.assertFalse((root / "GC/1.2.5s").exists())
            self.assertFalse((root / "GC/1.2.5sn").exists())

    def test_second_derivation_failure_installs_neither_profile(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            payload = self.fixture(root)
            calls = 0

            def fake_run(argv, *, check):
                nonlocal calls
                self.assertTrue(check)
                calls += 1
                if calls == 2:
                    raise subprocess.CalledProcessError(1, argv)
                Path(argv[4]).write_bytes(b"first output")
                return subprocess.CompletedProcess(argv, 0)

            specs = (
                (
                    "1.2.5",
                    "1.2.5s",
                    "mwcceppc-125s.exe",
                    hashlib.sha256(b"first output").hexdigest(),
                ),
                ("1.2.5n", "1.2.5sn", "mwcceppc-125sn.exe", "unused"),
            )
            with (
                patch.object(setup, "SPECIAL_COMPILERS", specs),
                patch.object(
                    setup.patch_pe,
                    "PAYLOAD_SHA256",
                    hashlib.sha256(payload.read_bytes()).hexdigest(),
                ),
            ):
                with self.assertRaises(subprocess.CalledProcessError):
                    setup.install_special_compilers(root, payload, run=fake_run)

            self.assertEqual(calls, 2)
            self.assertFalse((root / "GC/1.2.5s").exists())
            self.assertFalse((root / "GC/1.2.5sn").exists())

    def test_bad_payload_hash_fails_before_running_patcher(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            payload = self.fixture(root)
            with self.assertRaisesRegex(ValueError, "payload hash mismatch"):
                setup.install_special_compilers(root, payload)
            self.assertFalse((root / "GC/1.2.5s").exists())


if __name__ == "__main__":
    unittest.main()
