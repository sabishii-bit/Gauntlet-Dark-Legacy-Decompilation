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

from wf_word_diff import mnemonic_divergence  # noqa: E402
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


if __name__ == "__main__":
    unittest.main()
