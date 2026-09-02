"""wf_rederive_pin --apply surgical paste (run 34 item 9).

The one-call `probe --rederive-pin` pastes two derived relocation hashes back
into config/GUNE5D/webfrank.json. AGENTS.md trap 6: that file is edited with
surgical text swaps only, never a json.dump round-trip. These tests pin the
paste's safety — single-occurrence swaps, unchanged/absent skips, and an
ambiguous refusal — without needing built objects.
"""

import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

import wf_rederive_pin as wf  # noqa: E402


class ApplyRelocationUpdatesTests(unittest.TestCase):
    A = "a" * 64
    B = "b" * 64
    NEW_A = "1" * 64
    NEW_B = "2" * 64

    def cfg(self):
        return (f'{{ "before_relocations_sha256": "{self.A}",\n'
                f'   "after_relocations_sha256": "{self.B}" }}')

    def test_a_changed_pair_is_swapped_in_place(self):
        text, applied = wf.apply_relocation_updates(
            self.cfg(), [(self.A, self.NEW_A), (self.B, self.NEW_B)])
        self.assertIn(self.NEW_A, text)
        self.assertIn(self.NEW_B, text)
        self.assertNotIn(self.A, text)
        self.assertEqual(len(applied), 2)

    def test_only_the_hash_bytes_change(self):
        text, _ = wf.apply_relocation_updates(self.cfg(), [(self.A, self.NEW_A)])
        # structure/whitespace untouched: swap old->new gives back the same
        # string as rebuilding cfg() with NEW_A.
        self.assertEqual(text, self.cfg().replace(self.A, self.NEW_A))

    def test_an_unchanged_pair_is_skipped(self):
        text, applied = wf.apply_relocation_updates(
            self.cfg(), [(self.A, self.A)])
        self.assertEqual(applied, [])
        self.assertEqual(text, self.cfg())

    def test_an_absent_old_hash_is_a_noop(self):
        text, applied = wf.apply_relocation_updates(
            self.cfg(), [("f" * 64, self.NEW_A)])
        self.assertEqual(applied, [])
        self.assertEqual(text, self.cfg())

    def test_an_empty_old_hash_is_skipped(self):
        text, applied = wf.apply_relocation_updates(
            self.cfg(), [(None, self.NEW_A)])
        self.assertEqual(applied, [])
        self.assertEqual(text, self.cfg())

    def test_an_ambiguous_hash_refuses(self):
        doubled = self.cfg() + f'\n"dup": "{self.A}"'
        with self.assertRaisesRegex(ValueError, "appears 2 times"):
            wf.apply_relocation_updates(doubled, [(self.A, self.NEW_A)])


if __name__ == "__main__":
    unittest.main()
