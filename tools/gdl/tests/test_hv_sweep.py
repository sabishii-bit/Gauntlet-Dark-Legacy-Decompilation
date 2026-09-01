"""hv_sweep argument and ownership tests (run-31 item 7).

Two field-confirmed defects: the sweep was image-or-nothing (a lane wanting
one TU's roster paid a full-image sweep), and it wrote three untracked JSON
files beside itself, i.e. into a TRACKED directory of the repo. Ownership
was a hardcoded tuple that had already drifted a full run out of date; a
stale courtesy gate is worse than none, because it sweeps the TUs it was
meant to protect while skipping ones nobody owns any more.
"""

import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

import hv_sweep  # noqa: E402


class UnitsFromClaimsTests(unittest.TestCase):
    PAYLOAD = {
        "claims": [
            {"state": "active", "owner": "lane-A",
             "scope": "owns game/sys/memcard and game/world/gauntworld",
             "function": "function:saveMount"},
            {"state": "active", "owner": "lane-B",
             "scope": "movieplayer work in game/movie/movieplayer",
             "function": ""},
            {"state": "released", "owner": "lane-C",
             "scope": "game/enemy/enemy", "function": ""},
            {"state": "active", "owner": "me-lane",
             "scope": "tools only; game/ui/select mentioned in passing",
             "function": ""},
        ]
    }

    def test_active_foreign_claims_contribute_their_TUs(self):
        got = hv_sweep.units_from_claims(self.PAYLOAD, me="me-lane")
        self.assertIn("game/sys/memcard", got)
        self.assertIn("game/world/gauntworld", got)
        self.assertIn("game/movie/movieplayer", got)

    def test_a_released_claim_is_ignored(self):
        self.assertNotIn("game/enemy/enemy",
                         hv_sweep.units_from_claims(self.PAYLOAD, "me-lane"))

    def test_my_own_claim_never_excludes_my_own_TUs(self):
        """Otherwise the lane running the sweep skips exactly its own work."""
        self.assertNotIn("game/ui/select",
                         hv_sweep.units_from_claims(self.PAYLOAD, "me-lane"))

    def test_without_me_every_active_claim_counts(self):
        self.assertIn("game/ui/select",
                      hv_sweep.units_from_claims(self.PAYLOAD, None))

    def test_no_claims_means_no_exclusions_not_a_fallback(self):
        self.assertEqual(hv_sweep.units_from_claims({"claims": []}), ())

    def test_result_is_sorted_and_deduplicated(self):
        payload = {"claims": [
            {"state": "active", "owner": "x",
             "scope": "game/b/b game/a/a game/b/b", "function": ""}]}
        self.assertEqual(hv_sweep.units_from_claims(payload),
                         ("game/a/a", "game/b/b"))

    def test_prose_without_a_TU_path_yields_nothing(self):
        payload = {"claims": [
            {"state": "active", "owner": "x",
             "scope": "TOOL-AMENDMENT lane, owns tools/gdl, no TU scope",
             "function": "InitControls"}]}
        self.assertEqual(hv_sweep.units_from_claims(payload), ())


class ToolErrorReportingTests(unittest.TestCase):
    """A crash is not a measurement (item 11's half, asserted here too)."""

    def setUp(self):
        self._saved = list(hv_sweep.TOOL_ERRORS)
        hv_sweep.TOOL_ERRORS.clear()

    def tearDown(self):
        hv_sweep.TOOL_ERRORS[:] = self._saved

    def test_no_errors_exits_zero(self):
        self.assertEqual(hv_sweep.report_tool_errors(), 0)

    def test_any_error_exits_nonzero(self):
        hv_sweep.TOOL_ERRORS.append(
            ("game/x/y", "fn", "tier A", "ValueError: needs the symbol name"))
        self.assertEqual(hv_sweep.report_tool_errors(), 1)


if __name__ == "__main__":
    unittest.main()
