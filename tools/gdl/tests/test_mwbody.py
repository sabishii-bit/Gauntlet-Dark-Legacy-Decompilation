import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from mwbody import format_instruction, local_target


class MwBodyTests(unittest.TestCase):
    def test_local_branch_uses_function_relative_offset(self):
        self.assertEqual(
            local_target("bge a54 <AllocMem32+0x3c>", "AllocMem32"),
            0x3C,
        )

    def test_external_branch_is_not_local(self):
        self.assertIsNone(
            local_target("bl 0 <FatalErrorf>", "AllocMem32")
        )

    def test_sda_relocation_uses_metrowerks_syntax(self):
        row = {
            "offset": 0,
            "text": "lwz r5,0(0)",
            "reloc": ("EMB_SDA21", "mlmMemUsed"),
        }
        self.assertEqual(
            format_instruction(row, {}, "AllocMem32"),
            "    lwz r5,mlmMemUsed(r0)",
        )


if __name__ == "__main__":
    unittest.main()
