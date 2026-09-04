"""Run-56 item 7: proving a rule with the datum screen at reduced strength.

REPRODUCED and PRICED at 5366a3a2f. `ha_close.prove()` called
`webfrank.apply_patch(data, patch, target_data)` -- three arguments, so the
retail `image` defaulted to None, the L3 DATUM level of
`verify_datum_binding` could not run, and the words it would have decided
fell through to L4, the pool CORRESPONDENCE. ha_close DERIVES rules, so the
weaker screen was deciding what got authored.

TWO-SIDED, over all 162 shipped rules run through `apply_patch` twice with
nothing but that argument changed:
  POSITIVE  59 rules (36%) lose datum strength without the image. The
            verdict is BYTE-EQUAL either way; the only visible difference is
            the line `N word(s) rest on the pool correspondence alone`.
  NEGATIVE  98 rules are unchanged, so this is not a blanket problem and a
            blanket refusal would be wrong -- the image is what changes,
            not the rules.
  5 rules are not comparable (before-hash mismatch against the raw body).

CALL-SITE CENSUS, by AST: 26 sites, 3 passed the image before this run
(webfrank's own main, t16_rederive_body, wr_try_rule -- the last fixed by
run 44 item 3, which wrote the mechanism down and left the other 22 alone).
ha_close makes 4. The remaining 22 are declared debt in
`t26_apply_patch_image_audit.py` and this file pins the accounting, so the
count can shrink but a NEW image-less call cannot appear unannounced.
"""
import subprocess
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
REPO = TOOLS.parent.parent
sys.path.insert(0, str(TOOLS / "composed_census"))

import t26_apply_patch_image_audit as audit                     # noqa: E402


class CallSiteAccounting(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.rows = audit.call_sites()
        (cls.clean, cls.debt, cls.new_omission,
         cls.regression) = audit.classify(cls.rows)

    def test_every_call_site_is_accounted_for(self):
        self.assertEqual(
            [f"{p}:{n}" for p, n, *_ in self.new_omission], [],
            "a NEW apply_patch call omits the retail image: pass it (see "
            "wr_try_rule.py) or declare it in KNOWN_DEBT with a reason")

    def test_the_debt_list_has_no_stale_entries(self):
        self.assertEqual(
            [f"{p}:{n}" for p, n, *_ in self.regression], [],
            "a file listed as debt now passes the image: shrink KNOWN_DEBT")

    def test_the_audit_actually_found_call_sites(self):
        """A census that finds nothing passes every check above vacuously."""
        self.assertGreater(len(self.rows), 20)


class HaCloseProvesAtFullStrength(unittest.TestCase):

    def test_ha_close_is_in_the_clean_set(self):
        self.assertIn("tools/gdl/composed_census/ha_close.py", audit.CLEAN)
        self.assertNotIn("tools/gdl/composed_census/ha_close.py",
                         audit.KNOWN_DEBT)

    def test_its_apply_patch_call_passes_five_arguments(self):
        rows = [r for r in audit.call_sites()
                if r[0] == "tools/gdl/composed_census/ha_close.py"]
        self.assertTrue(rows)
        for _path, _line, positional, keywords, has_image in rows:
            self.assertTrue(has_image, (positional, keywords))

    def test_a_missing_image_is_a_refusal_not_a_downgrade(self):
        """The wr_try_rule rule: a screen that quietly stops screening is
        how claim.law.CQ_... shipped for ten days."""
        import ha_close
        saved_path, saved_cached = ha_close.RETAIL_IMAGE, ha_close._IMAGE
        try:
            ha_close.RETAIL_IMAGE = str(REPO / "no-such-image.dol")
            ha_close._IMAGE = None
            with self.assertRaises(SystemExit) as caught:
                ha_close.retail_image()
            self.assertIn("REFUSING", str(caught.exception))
            self.assertIn("provision_worktree", str(caught.exception))
        finally:
            ha_close.RETAIL_IMAGE, ha_close._IMAGE = saved_path, saved_cached

    def test_the_refusal_is_a_systemexit_not_a_silent_none(self):
        """AGENTS.md discipline 20: SystemExit is the project's refusal
        idiom, and `except Exception` cannot swallow it."""
        self.assertTrue(issubclass(SystemExit, BaseException))
        self.assertFalse(issubclass(SystemExit, Exception))


class TheAuditRunsAsACommand(unittest.TestCase):

    def test_it_exits_zero_on_a_clean_tree(self):
        proc = subprocess.run(
            [sys.executable,
             str(TOOLS / "composed_census" /
                 "t26_apply_patch_image_audit.py")],
            cwd=REPO, capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertIn("every call site is accounted for", proc.stdout)


if __name__ == "__main__":
    unittest.main()
