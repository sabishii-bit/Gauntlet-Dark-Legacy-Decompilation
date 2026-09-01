"""webfrank_audit reject-message tests (run-31 item 8).

Every rejection the audit emitted (11 of 11, measured 2026-09-01) was a
verify_consistent_recolor refusal — the class hv_repair exists to repair —
and not one of them said so. Worse, the handoff did not even work: the
audit prints the bare exception text "+0xNN: use of ..." while hv_repair's
offset regex required apply_patch's parenthesised "(+0xNN: use of ...)", so
a lane pasting an audit reason into hv_repair got refusal_offset None and no
search at all. Measured before the fix: 0 of 11 reasons parsed.

Per AGENTS.md discipline 14, a guard's refusal is a measurement of the
GUARD as much as of the function, so the refusal must carry its own
next step.
"""

import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

import hv_repair  # noqa: E402
from webfrank_audit import REPAIR_TOOL, classify_rejection  # noqa: E402

# The eleven live rejections were all this shape.
BARE = "+0x2c: use of g3 does not correspond to g3 under the running renaming"
PARENTHESISED = (
    "recolor refused (+0x30: use of f0 does not correspond to f1"
    " under the running renaming)")


class ClassifyRejectionTests(unittest.TestCase):
    def test_a_recolor_refusal_is_marked_repairable(self):
        got = classify_rejection("game/game/controls", "InitControls", BARE)
        self.assertTrue(got["repair_candidate"])
        self.assertEqual(got["refusal_offset"], "0x2c")

    def test_it_names_the_exact_command_to_run(self):
        got = classify_rejection("game/game/controls", "InitControls", BARE)
        self.assertEqual(
            got["next"],
            f"python {REPAIR_TOOL} game/game/controls InitControls")

    def test_the_hint_says_a_refusal_is_a_pointer_not_a_verdict(self):
        hint = classify_rejection("u", "f", BARE)["repair_hint"]
        self.assertIn("POINTER", hint)
        self.assertIn("UPSTREAM", hint)

    def test_the_parenthesised_apply_patch_spelling_also_classifies(self):
        got = classify_rejection("u", "f", PARENTHESISED)
        self.assertTrue(got["repair_candidate"])
        self.assertEqual(got["refusal_offset"], "0x30")

    def test_a_different_rejection_class_is_NOT_marked_repairable(self):
        """Over-claiming would send lanes at a search that cannot help."""
        for reason in (
            "non-register bits differ at +0x1c: 0x00000040",
            "function sizes are not equal, aligned words",
            "lmw/stmw fields may not differ",
        ):
            got = classify_rejection("u", "f", reason)
            self.assertFalse(got["repair_candidate"], reason)
            self.assertNotIn("next", got)

    def test_an_empty_reason_is_not_repairable(self):
        self.assertFalse(classify_rejection("u", "f", "")["repair_candidate"])


class HandoffInteropTests(unittest.TestCase):
    """The cross-reference is only worth printing if the handoff parses."""

    def test_hv_repair_parses_the_audits_BARE_reason(self):
        self.assertEqual(hv_repair.refusal_offset(BARE), 0x2C)

    def test_hv_repair_still_parses_apply_patchs_parenthesised_reason(self):
        self.assertEqual(hv_repair.refusal_offset(PARENTHESISED), 0x30)

    def test_the_offset_the_audit_reports_matches_what_hv_repair_reads(self):
        reported = classify_rejection("u", "f", BARE)["refusal_offset"]
        self.assertEqual(int(reported, 16), hv_repair.refusal_offset(BARE))

    def test_a_reason_with_no_offset_reads_none(self):
        self.assertIsNone(hv_repair.refusal_offset("no offset here"))


if __name__ == "__main__":
    unittest.main()
