"""wf_rederive_pin --apply surgical paste (run 34 item 9).

The one-call `probe --rederive-pin` pastes two derived relocation hashes back
into config/GUNE5D/webfrank.json. AGENTS.md trap 6: that file is edited with
surgical text swaps only, never a json.dump round-trip. These tests pin the
paste's safety — single-occurrence swaps, unchanged/absent skips, and an
ambiguous refusal — without needing built objects.
"""

import json
import shutil
import sys
import tempfile
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


class TwinnedRelocationHashTests(unittest.TestCase):
    """Run-35 item 5: the guard refused an ordinary re-derivation.

    A window whose permutation moves no RELOCATED instruction hashes its
    relocation set to the same value before and after, so the rule carries
    that one hash string in two slots. The raw-string swap counted two
    occurrences and refused the whole paste
    (claim.law.PC_wf-rederive-pin-apply-cannot-paste-a-twinned-relocation-hash).
    The slots differ by KEY, which is what the string swap threw away.
    """

    TWIN = "c" * 64
    REGION = "d" * 64
    OTHER_REGION = "e" * 64
    NEW_BEFORE = "3" * 64
    NEW_AFTER = "4" * 64

    def cfg(self):
        return (
            '{ "function": "fn", "instruction_permutation": [\n'
            f'  {{ "before_sha256": "{self.REGION}",\n'
            f'    "after_sha256": "{"9" * 64}",\n'
            f'    "before_relocations_sha256": "{self.TWIN}",\n'
            f'    "after_relocations_sha256":  "{self.TWIN}" }} ] }}')

    def test_the_legacy_string_swap_still_refuses_the_twin(self):
        """The defect, pinned: this is what --apply used to do."""
        with self.assertRaisesRegex(ValueError, "appears 2 times"):
            wf.apply_relocation_updates(
                self.cfg(), [(self.TWIN, self.NEW_BEFORE)])

    def test_the_keyed_form_swaps_each_twin_slot_independently(self):
        text, applied = wf.apply_relocation_updates(self.cfg(), [
            (self.REGION, "before_relocations_sha256", self.TWIN,
             self.NEW_BEFORE),
            (self.REGION, "after_relocations_sha256", self.TWIN,
             self.NEW_AFTER),
        ])
        self.assertEqual(len(applied), 2)
        self.assertIn(f'"before_relocations_sha256": "{self.NEW_BEFORE}"',
                      text)
        self.assertIn(f'"after_relocations_sha256":  "{self.NEW_AFTER}"',
                      text)
        self.assertNotIn(self.TWIN, text)
        # The file is edited by every lane: spacing must survive verbatim.
        self.assertIn('"after_relocations_sha256":  "', text)
        json.loads(text)

    def test_a_hash_repeated_across_windows_is_narrowed_by_the_anchor(self):
        """An empty relocation set hashes to a constant every window shares."""
        text = (
            '{ "instruction_permutation": [\n'
            f'  {{ "before_sha256": "{self.REGION}",\n'
            f'    "before_relocations_sha256": "{self.TWIN}" }},\n'
            f'  {{ "before_sha256": "{self.OTHER_REGION}",\n'
            f'    "before_relocations_sha256": "{self.TWIN}" }} ] }}')
        out, applied = wf.apply_relocation_updates(text, [
            (self.OTHER_REGION, "before_relocations_sha256", self.TWIN,
             self.NEW_AFTER)])
        self.assertEqual(len(applied), 1)
        # Only the SECOND window moved.
        loaded = json.loads(out)["instruction_permutation"]
        self.assertEqual(loaded[0]["before_relocations_sha256"], self.TWIN)
        self.assertEqual(loaded[1]["before_relocations_sha256"],
                         self.NEW_AFTER)

    def test_an_unnarrowable_repeat_still_refuses(self):
        """Fail closed: no anchor, two matches, no guess."""
        text = (f'{{ "before_relocations_sha256": "{self.TWIN}",\n'
                f'  "x": {{ "before_relocations_sha256": "{self.TWIN}" }} }}')
        with self.assertRaisesRegex(ValueError, "appears 2 times"):
            wf.apply_relocation_updates(text, [
                (None, "before_relocations_sha256", self.TWIN,
                 self.NEW_AFTER)])

    def test_a_brace_in_prose_does_not_derail_the_anchor_span(self):
        """webfrank rules carry `mechanism` notes; braces live in them."""
        text = (
            '{ "instruction_permutation": [\n'
            '  { "mechanism": "the } brace here must not close anything {",\n'
            f'    "before_sha256": "{self.REGION}",\n'
            f'    "before_relocations_sha256": "{self.TWIN}" }},\n'
            f'  {{ "before_sha256": "{self.OTHER_REGION}",\n'
            f'    "before_relocations_sha256": "{self.TWIN}" }} ] }}')
        out, applied = wf.apply_relocation_updates(text, [
            (self.REGION, "before_relocations_sha256", self.TWIN,
             self.NEW_AFTER)])
        self.assertEqual(len(applied), 1)
        loaded = json.loads(out)["instruction_permutation"]
        self.assertEqual(loaded[0]["before_relocations_sha256"],
                         self.NEW_AFTER)
        self.assertEqual(loaded[1]["before_relocations_sha256"], self.TWIN)

    def test_descriptors_stay_index_parallel_with_the_slots(self):
        rule = json.loads(self.cfg())
        rule["before_sha256"] = "7" * 64
        rule["after_sha256"] = "8" * 64
        self.assertEqual(len(wf.rule_hash_slots(rule)),
                         len(wf.rule_hash_descriptors(rule)))

    def test_a_window_never_anchors_its_own_before_sha256(self):
        rule = json.loads(self.cfg())
        rule["before_sha256"] = "7" * 64
        rule["after_sha256"] = "8" * 64
        for anchor, key in wf.rule_hash_descriptors(rule):
            if key == "before_sha256":
                self.assertIsNone(anchor)


class TransientBankTests(unittest.TestCase):
    """--transient banks a pin's pre-probe hashes so a revert can undo it.

    Run-34 criticism (GW): even with --apply, ~2 of 15 probe cycles were pure
    pin plumbing — a revert restores the SOURCE and leaves the re-derived
    hashes in webfrank.json, so the pin has to be walked back by hand.
    """

    UNIT = "game/world/btricol"
    FN = "btriCollide"
    OLD = {"body_before": "a" * 64, "body_after": "b" * 64,
           "win_before": "c" * 64, "win_after": "d" * 64,
           "rel_before": "e" * 64, "rel_after": "f" * 64}
    NEW_REL_BEFORE = "1" * 64
    NEW_REL_AFTER = "2" * 64

    def setUp(self):
        self.root = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.root, ignore_errors=True)
        self.config = self.root / "webfrank.json"
        self.write_config(self.OLD["rel_before"], self.OLD["rel_after"])
        self.bank = str(self.root / "bank.json")

    def write_config(self, rel_before, rel_after):
        self.config.write_text(json.dumps({"units": {self.UNIT: [{
            "function": self.FN,
            "before_sha256": self.OLD["body_before"],
            "after_sha256": self.OLD["body_after"],
            "instruction_permutation": {
                "start": "0x10", "end": "0x20", "order": ["1", "0"],
                "before_sha256": self.OLD["win_before"],
                "after_sha256": self.OLD["win_after"],
                "before_relocations_sha256": rel_before,
                "after_relocations_sha256": rel_after}}]}},
            indent=1), encoding="utf-8")

    def rederive(self):
        """Simulate wf_rederive_pin --apply pasting two new reloc hashes."""
        self.write_config(self.NEW_REL_BEFORE, self.NEW_REL_AFTER)

    def test_bank_path_is_under_build_and_lane_neutral(self):
        path = wf.bank_path(self.UNIT, root="R").replace("\\", "/")
        self.assertIn("build/GUNE5D/gate/", path)
        self.assertTrue(path.endswith("wfpin_game_world_btricol.json"))

    def test_slots_cover_body_and_every_window_hash(self):
        rule = json.loads(self.config.read_text(
            encoding="utf-8"))["units"][self.UNIT][0]
        self.assertEqual(wf.rule_hash_slots(rule), [
            self.OLD["body_before"], self.OLD["body_after"],
            self.OLD["win_before"], self.OLD["win_after"],
            self.OLD["rel_before"], self.OLD["rel_after"]])

    def test_a_list_of_windows_yields_four_slots_each(self):
        rule = {"before_sha256": "x", "after_sha256": "y",
                "instruction_permutation": [
                    {"before_sha256": "1", "after_sha256": "2",
                     "before_relocations_sha256": "3",
                     "after_relocations_sha256": "4"},
                    {"before_sha256": "5", "after_sha256": "6",
                     "before_relocations_sha256": "7",
                     "after_relocations_sha256": "8"}]}
        self.assertEqual(len(wf.rule_hash_slots(rule)), 10)

    def test_bank_then_rederive_then_restore_returns_the_original(self):
        before = self.config.read_text(encoding="utf-8")
        self.assertTrue(wf.bank_transient(self.UNIT, self.FN,
                                          str(self.config), self.bank))
        self.rederive()
        self.assertIn(self.NEW_REL_BEFORE,
                      self.config.read_text(encoding="utf-8"))
        restored, notes = wf.restore_transient(self.UNIT, str(self.config),
                                               self.bank)
        self.assertEqual(restored, [self.FN])
        self.assertEqual(notes, [])
        self.assertEqual(self.config.read_text(encoding="utf-8"), before)

    def test_the_bank_is_consumed_by_a_clean_restore(self):
        wf.bank_transient(self.UNIT, self.FN, str(self.config), self.bank)
        self.rederive()
        wf.restore_transient(self.UNIT, str(self.config), self.bank)
        self.assertFalse(Path(self.bank).exists())

    def test_the_first_bank_wins_across_repeated_rederives(self):
        """Three re-derives in one A/B must still return to the pre-probe pin.

        The same rule probe's session baseline follows, for the same reason.
        """
        wf.bank_transient(self.UNIT, self.FN, str(self.config), self.bank)
        self.rederive()
        wf.bank_transient(self.UNIT, self.FN, str(self.config), self.bank)
        banked = json.loads(Path(self.bank).read_text(encoding="utf-8"))
        self.assertEqual(banked["pins"][self.FN][4], self.OLD["rel_before"])

    def test_no_bank_is_a_silent_noop(self):
        self.assertEqual(wf.restore_transient(self.UNIT, str(self.config),
                                              self.bank), ([], []))

    def test_a_reshaped_rule_refuses_to_pair_and_keeps_the_bank(self):
        wf.bank_transient(self.UNIT, self.FN, str(self.config), self.bank)
        config = json.loads(self.config.read_text(encoding="utf-8"))
        rule = config["units"][self.UNIT][0]
        rule["instruction_permutation"] = [rule["instruction_permutation"],
                                           dict(rule["instruction_permutation"])]
        self.config.write_text(json.dumps(config, indent=1), encoding="utf-8")
        restored, notes = wf.restore_transient(self.UNIT, str(self.config),
                                               self.bank)
        self.assertEqual(restored, [])
        self.assertTrue(any("SHAPE changed" in note for note in notes))
        self.assertTrue(Path(self.bank).exists())

    def test_a_vanished_rule_is_reported_not_crashed_on(self):
        wf.bank_transient(self.UNIT, self.FN, str(self.config), self.bank)
        self.config.write_text(json.dumps({"units": {}}), encoding="utf-8")
        restored, notes = wf.restore_transient(self.UNIT, str(self.config),
                                               self.bank)
        self.assertEqual(restored, [])
        self.assertTrue(any("no rule" in note for note in notes))

    def test_banking_an_unknown_function_reports_false(self):
        self.assertFalse(wf.bank_transient(self.UNIT, "notAFunction",
                                           str(self.config), self.bank))
        self.assertFalse(Path(self.bank).exists())


if __name__ == "__main__":
    unittest.main()
