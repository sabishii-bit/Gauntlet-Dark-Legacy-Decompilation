import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock, patch

from tools import download_tool
from tools.gdl.mwcc_p6 import setup_compilers


class DownloadToolSpecialCompilerTests(unittest.TestCase):
    def test_special_setup_runs_after_the_compiler_archive_download(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "compilers"
            payload = Path(directory) / "payload.bin"
            response = MagicMock()
            response.__enter__.return_value = response
            response.__exit__.return_value = False
            installed = [output / "GC/1.2.5s/mwcceppc.exe"]
            argv = [
                "download_tool.py",
                "compilers",
                str(output),
                "--tag",
                "test",
                "--gdl-special-compilers",
                "--gdl-special-payload",
                str(payload),
            ]
            with (
                patch.object(sys, "argv", argv),
                patch.object(download_tool.urllib.request, "urlopen", return_value=response),
                patch.object(download_tool, "download") as download,
                patch.object(
                    setup_compilers,
                    "install_special_compilers",
                    return_value=installed,
                ) as setup,
            ):
                download_tool.main()
            download.assert_called_once()
            setup.assert_called_once_with(output, payload)

    def test_special_setup_is_rejected_for_noncompiler_downloads(self):
        argv = [
            "download_tool.py",
            "wibo",
            "wibo.exe",
            "--tag",
            "test",
            "--gdl-special-compilers",
        ]
        with patch.object(sys, "argv", argv), self.assertRaises(SystemExit) as error:
            download_tool.main()
        self.assertEqual(error.exception.code, 2)

    def test_payload_option_requires_special_setup(self):
        argv = [
            "download_tool.py",
            "compilers",
            "compilers",
            "--tag",
            "test",
            "--gdl-special-payload",
            "payload.bin",
        ]
        with patch.object(sys, "argv", argv), self.assertRaises(SystemExit) as error:
            download_tool.main()
        self.assertEqual(error.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
