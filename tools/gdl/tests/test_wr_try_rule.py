"""wr_try_rule.py — the retail image is passed, and its absence REFUSES.

Run-44 item 3, from WS. `apply_patch` takes the retail image as its fifth
argument; this tool called it with four, so every rule authored through it
was screened with `image=None`. Without the image the L3 DATUM level of
`verify_datum_binding` cannot run and every word it would have decided falls
through to L4, the pool CORRESPONDENCE — a strictly weaker proof that says
nothing about WHICH datum each end of a one-to-one map holds, which is the
exact hole claim.law.CQ_copy-register-fields-can-rotate-constant-load-homes-
without-their-relocations.20260903.v1 records.

Measured at ca4074cb1 on the shipped game/mb/mb_font::MBRenderText rule:
L1 27 / L2 0 / L3 0 / L4 10 with the four-argument call, and
L1 27 / L2 0 / L3 10 / L4 0 with the image passed. Same rule, same bytes,
same BYTE-EQUAL verdict.
"""

import os
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

import wr_try_rule  # noqa: E402


class DefaultImagePathTests(unittest.TestCase):
    def test_it_matches_webfranks_own_resolution(self):
        """webfrank main() resolves `config/<version>/webfrank.json` to
        `<repo>/orig/<version>/sys/main.dol`; spelled in both places, the
        two must not drift."""
        self.assertEqual(
            os.path.join("R", "orig", "GUNE5D", "sys", "main.dol"),
            wr_try_rule.default_image_path("R"))

    def test_the_version_is_a_parameter_not_a_hardcode(self):
        self.assertIn("OTHER", wr_try_rule.default_image_path("R", "OTHER"))


class MissingImageRefusesTests(unittest.TestCase):
    """Fail CLOSED. A screen that quietly stops screening is the failure
    mode; the refusal fires before any object is opened, so it cannot be
    mistaken for a build problem."""

    def run_tool(self, image):
        argv = sys.argv
        sys.argv = ["wr_try_rule", "game/mb/mb_font", "MBRenderText",
                    "no-such-fragment.json", "--image", image]
        try:
            return wr_try_rule.main()
        finally:
            sys.argv = argv

    def test_a_missing_image_returns_one_and_reads_no_object(self):
        # The fragment path does not exist either: reaching it would raise
        # rather than return 1, so the exit code proves the image check
        # ran FIRST.
        self.assertEqual(1, self.run_tool(str(
            Path(__file__).resolve().parent / "no-such-image.dol")))


if __name__ == "__main__":
    unittest.main()
