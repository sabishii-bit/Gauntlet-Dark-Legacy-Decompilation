"""A short download used to be written out and reported as success.

Measured this session: the ninja TOOL step fetched `dtk-linux-x86_64` at
9,075,662 of 9,107,048 bytes. Nothing checked the length, so the truncated file
was left on disk marked executable, its ELF header still named a section-header
offset past EOF, and EVERY dtk invocation segfaulted -- `--version` included.
That presents as a corrupt binary rather than a bad fetch, and it cost a lane a
diagnosis. A 1 GB compiler archive failing the same way would be worse.

`download` now compares the bytes written against the server's Content-Length,
deletes the partial file, and raises SystemExit naming both numbers.

TWO-SIDED. Positive: a complete download of either kind is written and left in
place, and a server that sends no Content-Length is still accepted (many do
not, and refusing those would break the build). Negative: a short plain file
and a short zip both raise, the partial file is REMOVED rather than left
executable, and a malformed Content-Length is treated as absent rather than
crashing the parse.
"""
import importlib.util
import io
import os
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

SPEC = Path(__file__).resolve().parent.parent.parent / "download_tool.py"


def load_module():
    spec = importlib.util.spec_from_file_location("download_tool", SPEC)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class FakeResponse(io.BytesIO):
    def __init__(self, data, length=None):
        super().__init__(data)
        self.headers = {} if length is None else {"Content-Length": str(length)}


class DownloadLengthTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.mod = load_module()

    def setUp(self):
        self.tmp = TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.out = Path(self.tmp.name) / "dtk"

    # ---------- positive ----------

    def test_a_complete_plain_download_is_kept(self):
        payload = b"\x7fELF" + b"x" * 100
        self.mod.download("http://h/dtk-linux-x86_64",
                          FakeResponse(payload, len(payload)), self.out)
        self.assertTrue(self.out.exists())
        self.assertEqual(self.out.read_bytes(), payload)
        self.assertTrue(os.stat(self.out).st_mode & 0o100, "should be executable")

    def test_no_content_length_is_accepted(self):
        """Many servers omit it; refusing those would break the build."""
        payload = b"payload"
        self.mod.download("http://h/dtk", FakeResponse(payload), self.out)
        self.assertEqual(self.out.read_bytes(), payload)

    def test_expected_length_reads_the_header(self):
        self.assertEqual(
            self.mod.expected_length(FakeResponse(b"", 1234)), 1234)
        self.assertIsNone(self.mod.expected_length(FakeResponse(b"")))

    # ---------- negative ----------

    def test_a_short_plain_download_raises_and_removes_the_partial(self):
        payload = b"\x7fELF" + b"x" * 100          # 104 bytes
        with self.assertRaises(SystemExit) as cm:
            self.mod.download("http://h/dtk-linux-x86_64",
                              FakeResponse(payload, 9107048), self.out)
        self.assertIn("SHORT DOWNLOAD", str(cm.exception))
        self.assertIn("9107048", str(cm.exception))
        self.assertFalse(self.out.exists(),
                         "a truncated tool must not be left on disk")

    def test_a_short_zip_raises_before_extracting(self):
        blob = b"PK\x03\x04 not really a zip"
        outdir = Path(self.tmp.name) / "compilers"
        with self.assertRaises(SystemExit) as cm:
            self.mod.download("http://h/compilers_20251118.zip",
                              FakeResponse(blob, 1_000_000_000), outdir)
        self.assertIn("SHORT DOWNLOAD", str(cm.exception))
        self.assertFalse(outdir.exists(), "nothing should have been extracted")

    def test_a_malformed_content_length_is_treated_as_absent(self):
        r = FakeResponse(b"data")
        r.headers = {"Content-Length": "not-a-number"}
        self.assertIsNone(self.mod.expected_length(r))

    def test_verify_length_passes_when_the_lengths_agree(self):
        self.out.write_bytes(b"x" * 10)
        self.mod.verify_length("http://h/x", self.out, 10, 10)
        self.assertTrue(self.out.exists())


if __name__ == "__main__":
    unittest.main()
