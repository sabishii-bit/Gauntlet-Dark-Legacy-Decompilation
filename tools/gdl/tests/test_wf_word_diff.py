"""wf_word_diff sweep-screen tests (T10, run 40 item 10).

The word count alone was measured to mislead a sweep two independent ways:
it merges the recolor and schedule-reorder classes, whose cures are
disjoint, and it ranks webfrank-PINNED functions first because this tool
reads the raw pre-postprocess body. Both screens now travel with the count.

Exercised over raw word bytes and a synthetic config, so no built object,
toolchain or webfrank backend is required.
"""

import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

from wf_word_diff import (basic_block_leaders,  # noqa: E402
                          mnemonic_divergence, ops_clusters, ops_regions,
                          region_word_counts, _locally_aligned)
from unabsorbed import rule_served_functions  # noqa: E402


def words(*values):
    return b"".join(struct.pack(">I", value) for value in values)


# addi r3,r4,0 / addi r3,r5,0 — same primary opcode (14), different
# register field. This is the RECOLOR shape.
ADDI_R4 = 0x38640000
ADDI_R5 = 0x38650000
# lwz (primary 32) and lfs (primary 48) — different mnemonics entirely.
LWZ = 0x80640000
LFS = 0xC0640000


class MnemonicDivergenceTests(unittest.TestCase):
    def test_register_field_differences_are_not_divergences(self):
        """The whole point: a recolor differs in every word and in no
        mnemonic, so the word count is large and this count is zero."""
        ours = words(ADDI_R4, ADDI_R4, ADDI_R4)
        tgt = words(ADDI_R5, ADDI_R5, ADDI_R5)
        self.assertNotEqual(ours, tgt)
        self.assertEqual(mnemonic_divergence(ours, tgt), 0)

    def test_a_migrated_instruction_is_counted_twice(self):
        """A swap shows up at BOTH indices — the count measures how far the
        schedule moved, not how many instructions moved."""
        ours = words(LWZ, LFS, ADDI_R4)
        tgt = words(LFS, LWZ, ADDI_R4)
        self.assertEqual(mnemonic_divergence(ours, tgt), 2)

    def test_identical_streams_are_zero(self):
        stream = words(LWZ, LFS, ADDI_R4)
        self.assertEqual(mnemonic_divergence(stream, stream), 0)

    def test_extended_opcode_primaries_are_distinguished(self):
        """Primary 31 covers dozens of mnemonics; masking only the primary
        would call `add` and `and` the same instruction."""
        add = 0x7C641A14        # add  r3,r4,r3
        and_ = 0x7C641838       # and  r4,r3,r3
        self.assertEqual(mnemonic_divergence(words(add), words(and_)), 1)


class PinScreenTests(unittest.TestCase):
    """The PINNED column, and the parser trap the RC law names."""

    def _config(self, payload):
        root = Path(tempfile.mkdtemp(prefix="t10-wfwd-"))
        (root / "config" / "GUNE5D").mkdir(parents=True)
        (root / "config" / "GUNE5D" / "webfrank.json").write_text(
            json.dumps(payload), encoding="utf-8")
        return root

    def test_rules_are_read_from_the_units_key(self):
        root = self._config({
            "version": 1,
            "units": {"game/ui/screensaver": [
                {"function": "end_inventory_panel"},
                {"function": "show_piles"},
            ]},
        })
        self.assertEqual(
            rule_served_functions("game/ui/screensaver", root),
            {"end_inventory_panel", "show_piles"})

    def test_a_unit_with_no_rules_is_empty_not_an_error(self):
        root = self._config({"version": 1, "units": {}})
        self.assertEqual(rule_served_functions("game/ui/screensaver", root),
                         set())

    def test_rules_at_the_ROOT_are_not_found(self):
        """The trap the RC law names verbatim: a parser that iterates the
        root finds 2 keys and 0 pins, which reads like 'no pins exist'.
        This asserts we read `units` and nothing else, so a config that
        ever moved would fail loudly here rather than silently returning
        an all-clear."""
        root = self._config({
            "version": 1,
            "game/ui/screensaver": [{"function": "end_inventory_panel"}],
        })
        self.assertEqual(rule_served_functions("game/ui/screensaver", root),
                         set())

    def test_a_missing_config_is_empty_not_a_crash(self):
        root = Path(tempfile.mkdtemp(prefix="t10-wfwd-none-"))
        self.assertEqual(rule_served_functions("game/x/y", root), set())


B_FWD_8 = 0x48000008        # b   +8   (unconditional, no link)
BL_FWD_8 = 0x48000009       # bl  +8   (a CALL, not a terminator)
BLR = 0x4E800020            # blr
BNE_FWD_8 = 0x40820008      # bc  4,2,+8


class BasicBlockLeaderTests(unittest.TestCase):
    """--by-region (run 41 item 4): the cut points the cluster partition
    lacks. A recolor lives entirely inside the matcher's equal runs, so the
    --ops partition alone put 58 of fn_800D8BCC's 66 words in ONE region."""

    def test_a_straight_line_function_is_one_block(self):
        self.assertEqual(
            basic_block_leaders(words(ADDI_R4, ADDI_R5, LWZ)), [0])

    def test_a_branch_starts_a_block_at_its_target_and_its_fallthrough(self):
        # index 0 branches to index 2; index 1 is the fallthrough leader.
        stream = words(B_FWD_8, ADDI_R4, ADDI_R5, LWZ)
        self.assertEqual(basic_block_leaders(stream), [0, 1, 2])

    def test_a_call_is_not_a_terminator(self):
        """bl returns; splitting on every call would shred the partition."""
        self.assertEqual(
            basic_block_leaders(words(BL_FWD_8, ADDI_R4, ADDI_R5)), [0])

    def test_a_conditional_branch_cuts_both_ways(self):
        stream = words(ADDI_R4, BNE_FWD_8, ADDI_R5, LWZ)
        self.assertEqual(basic_block_leaders(stream), [0, 2, 3])

    def test_blr_cuts_the_following_instruction(self):
        self.assertEqual(
            basic_block_leaders(words(ADDI_R4, BLR, ADDI_R5)), [0, 2])

    def test_a_leader_past_the_end_is_dropped(self):
        """A terminator in the LAST slot has no fallthrough to name."""
        self.assertEqual(basic_block_leaders(words(ADDI_R4, BLR)), [0])


class RegionDecompositionTests(unittest.TestCase):
    def test_regions_cover_the_function_without_gaps_or_overlap(self):
        ours = words(ADDI_R4, B_FWD_8, ADDI_R4, LWZ, ADDI_R4)
        tgt = words(ADDI_R5, B_FWD_8, ADDI_R5, LFS, ADDI_R5)
        regions = ops_regions(ours, tgt)
        bounds = [(lo, hi) for _tag, lo, hi, _a, _b in regions]
        self.assertEqual(bounds[0][0], 0)
        self.assertEqual(bounds[-1][1], 5)
        for (_lo, hi), (lo2, _hi2) in zip(bounds, bounds[1:]):
            self.assertEqual(hi, lo2)

    def test_cluster_boundaries_are_never_lost_to_the_block_split(self):
        ours = words(ADDI_R4, LWZ, ADDI_R4)
        tgt = words(ADDI_R4, LFS, ADDI_R4)
        cluster_edges = {lo for _t, lo, _hi, _a, _b in ops_clusters(ours, tgt)}
        region_edges = {lo for _t, lo, _hi, _a, _b in ops_regions(ours, tgt)}
        self.assertTrue(cluster_edges <= region_edges)

    def test_every_differing_word_lands_in_exactly_one_region(self):
        ours = words(ADDI_R4, B_FWD_8, ADDI_R4, LWZ, ADDI_R4)
        tgt = words(ADDI_R5, B_FWD_8, ADDI_R5, LWZ, ADDI_R5)
        rows = [(0, 0, 0), (8, 0, 0), (16, 0, 0)]
        counts = region_word_counts(rows, ops_regions(ours, tgt))
        self.assertEqual(sum(row[3] for row in counts), len(rows))

    def test_a_region_carrying_nothing_reports_zero(self):
        ours = words(ADDI_R4, B_FWD_8, ADDI_R5)
        tgt = words(ADDI_R5, B_FWD_8, ADDI_R5)
        # One equal cluster, cut once at the branch's fallthrough leader.
        counts = region_word_counts([(0, 0, 0)], ops_regions(ours, tgt))
        self.assertEqual([(row[1], row[2], row[3]) for row in counts],
                         [(0, 2, 1), (2, 3, 0)])


class RelocPairingReliabilityTests(unittest.TestCase):
    """An unlinked `bl` is one word whatever it calls, so an offset-paired
    relocation row is only trustworthy where the mnemonics align."""

    def test_identical_streams_are_aligned_everywhere(self):
        stream = words(*([ADDI_R4] * 12))
        self.assertTrue(_locally_aligned(stream, stream, 6))

    def test_a_nearby_mnemonic_divergence_marks_the_row(self):
        ours = words(*([ADDI_R4] * 6 + [LWZ] + [ADDI_R4] * 5))
        tgt = words(*([ADDI_R4] * 6 + [LFS] + [ADDI_R4] * 5))
        self.assertFalse(_locally_aligned(ours, tgt, 6))
        self.assertFalse(_locally_aligned(ours, tgt, 9))   # inside window 4
        self.assertTrue(_locally_aligned(ours, tgt, 11))   # outside it

    def test_register_field_differences_do_not_unalign_a_row(self):
        ours = words(*([ADDI_R4] * 9))
        tgt = words(*([ADDI_R5] * 9))
        self.assertTrue(_locally_aligned(ours, tgt, 4))


if __name__ == "__main__":
    unittest.main()
