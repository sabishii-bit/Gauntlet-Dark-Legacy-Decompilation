"""t15_promotion_queue: the promotion-backlog instrument.

THE OBSERVATION (run-45 item 3). The USER's 2026-09-03 promotion-backlog
directive -- every rule-served function is promotion debt, a source-exact
close DELETES its rule in the same commit -- had no instrument.
`config/GUNE5D/webfrank.json` is 295 KB in which the rules are not enumerable
by eye, and the number that says how far a function is from source-exact (its
RAW differing-word count) was per-function only.

These tests pin the parts that are pure over the config, in particular the
two that a reader would otherwise have to trust: that `copy_register_fields:
true` declares ZERO atoms (so a blanket recolor is ranked by WORDS, which is
what a source lane must actually close, and never by declarations), and that
the proof mode reports the human escape by name.
"""

import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

import t15_promotion_queue as queue  # noqa: E402


class RuleSizeTests(unittest.TestCase):
    def test_a_blanket_field_copy_declares_no_atoms(self):
        rule = {"function": "f", "copy_register_fields": True}
        self.assertEqual(queue.rule_size(rule), 0)

    def test_permutation_atoms_are_counted_per_window(self):
        rule = {"function": "f", "instruction_permutation": [
            {"start": "0x18", "end": "0x30", "order": [4, 0, 1, 2, 5, 3]},
            {"start": "0x40", "end": "0x48", "order": [1, 0]}]}
        self.assertEqual(queue.rule_size(rule), 8)

    def test_a_single_permutation_object_counts_too(self):
        """The config carries both shapes: a bare object and a list."""
        rule = {"function": "f",
                "instruction_permutation": {"start": "0x14", "end": "0x1c",
                                            "order": [1, 0]}}
        self.assertEqual(queue.rule_size(rule), 2)

    def test_form_sites_and_declarations_are_counted(self):
        rule = {"function": "f",
                "equivalent_copy_form": [{"at": "0x1c"}, {"at": "0x38"}],
                "register_fields": [{"at": "0x4"}],
                "copy_register_fields": True}
        self.assertEqual(queue.rule_size(rule), 3)

    def test_audit_and_prose_keys_are_never_atoms(self):
        rule = {"function": "f", "mechanism": "words", "audit": {"x": 1},
                "before_sha256": "a", "after_sha256": "b"}
        self.assertEqual(queue.rule_size(rule), 0)


class ProofModeTests(unittest.TestCase):
    def test_the_human_escape_outranks_every_other_label(self):
        self.assertEqual(
            queue.proof_mode(["copy_register_fields", "value_equality_recolor",
                              "unproven_recolor_audit"]), "UNPROVEN")

    def test_value_equality_is_reported_separately_from_strict(self):
        self.assertEqual(
            queue.proof_mode(["copy_register_fields",
                              "value_equality_recolor"]), "value-eq")
        self.assertEqual(queue.proof_mode(["copy_register_fields"]), "strict")


class ProvenanceCitationTests(unittest.TestCase):
    def test_a_record_id_in_the_mechanism_is_found(self):
        found = queue.RECORD_ID_RE.findall(
            "attempt.CL_print-n-of-m-a-loaded-local-outranks-a-parameter-copy-"
            "local.20260903.v2 (source-exhaustion provenance)")
        self.assertEqual(len(found), 1)
        self.assertTrue(found[0].endswith(".v2"))

    def test_prose_naming_no_record_is_not_a_citation(self):
        """`Banked in the graph` prose is not a citation (AGENTS.md)."""
        self.assertEqual(queue.RECORD_ID_RE.findall(
            "Symmetric two-web register crossing over 8 words; banked in the "
            "graph."), [])


class RankingTests(unittest.TestCase):
    def test_smallest_residual_ranks_first(self):
        rows = [{"words": 8, "insns": 68, "unit": "u", "function": "b"},
                {"words": 1, "insns": 75, "unit": "u", "function": "a"},
                {"words": None, "insns": 10, "unit": "u", "function": "c"}]
        rows.sort(key=queue._sort_key)
        self.assertEqual([row["function"] for row in rows], ["a", "b", "c"])

    def test_an_unmeasurable_row_sorts_last_not_first(self):
        """A missing count must never masquerade as a zero-word rule."""
        rows = [{"words": None, "insns": 1, "unit": "u", "function": "z"},
                {"words": 204, "insns": 441, "unit": "u", "function": "y"}]
        rows.sort(key=queue._sort_key)
        self.assertEqual([row["function"] for row in rows], ["y", "z"])


if __name__ == "__main__":
    unittest.main()
