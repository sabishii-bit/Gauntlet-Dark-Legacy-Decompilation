"""wg_identical_slot_provenance: the operand-provenance screen's complement.

THE OBSERVATION (run 46, WG lane).  `t15_operand_provenance` compares VALUES
only where the register ENCODING differs.  It skips twice --
`if our_word == target_word: continue` and, inside a differing word,
`if our_reg == target_reg: continue` -- and a per-web reallocation defeats
both, because one register NUMBER can carry two different webs across the two
streams and then read different values while spelling identically.

game/audio/mempool::pool_garbage_collect +0x30 is that shape, and it is the
corpus's LAST `unproven_recolor_audit` escape:

    ours    0x28 addi r0,r3,@l   (the entries base into the scratch r0)
    ours    0x30 mr r31,r0       <- copies the ENTRIES BASE
    target  0x20 lwz r0,4(r3)    (the node into the scratch r0)
    target  0x30 mr r3,r0        <- copies the NODE

The word differs (dest r31 against r3) so t15 examines it, but both SOURCE
slots read g0, so t15's second skip drops them and the screen reports no
operand-value difference at the one site the rule's own note documents as
one.  These tests pin both halves of
claim.law.WG_a-clean-operand-provenance-verdict-cannot-clear-a-pin
.20260903.v1 on the minimal synthetic form of that shape: t15 stays silent,
and the complement fires -- and the complement stays silent when the
identical slot really does read the same value, which is the half that keeps
it from being a firing machine.
"""

import struct
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

import t15_operand_provenance as t15                    # noqa: E402
import wg_identical_slot_provenance as wg               # noqa: E402

BLR = 0x4E800020


def li(destination, value):
    return (14 << 26) | (destination << 21) | (value & 0xFFFF)


def mr(destination, source):
    return ((31 << 26) | (source << 21) | (destination << 16)
            | (source << 11) | (444 << 1))


def stream(words):
    blob = b"".join(struct.pack(">I", word) for word in words)
    return t15.Stream(blob, {})


# The copy-role swap, minimally: both streams park two values in two
# registers and copy ONE of them out of g0, but not the same one.
OURS = [li(5, 1), li(0, 2), mr(31, 0), BLR]
TARGET = [li(0, 1), li(29, 2), mr(3, 0), BLR]

# The control: same shape, same recolor, but g0 carries the SAME value into
# the copy on both sides.
OURS_AGREEING = [li(5, 1), li(0, 2), mr(31, 0), BLR]
TARGET_AGREEING = [li(29, 1), li(0, 2), mr(3, 0), BLR]


class TheShippedScreenIsBlindHere(unittest.TestCase):
    def test_t15_reports_no_operand_value_difference(self):
        rows, verdict = t15.screen_pair(stream(OURS), stream(TARGET))
        self.assertNotEqual(verdict, "OPERAND-VALUE-DIFF")
        self.assertEqual(
            [], [row for row in rows if row["verdict"] == "VALUE-DIFF"])

    def test_the_copy_appears_to_t15_only_as_a_destination_rename(self):
        rows, _ = t15.screen_pair(stream(OURS), stream(TARGET))
        at_copy = [row for row in rows if row["at"] == "0x8"]
        self.assertEqual(["DEST-RENAME"],
                         [row["verdict"] for row in at_copy])


class TheComplementSeesIt(unittest.TestCase):
    def test_it_flags_the_identically_encoded_source_slot(self):
        rows, verdict = wg.screen_identical_slots(stream(OURS),
                                                  stream(TARGET))
        self.assertEqual("IDENTICAL-SLOT-VALUE-DIFF", verdict)
        flagged = [row for row in rows
                   if row["verdict"] == "IDENTICAL-SLOT-VALUE-DIFF"]
        self.assertTrue(flagged)
        for row in flagged:
            self.assertEqual("0x8", row["at"])
            self.assertEqual("r0", row["reg"])
            self.assertTrue(row["word_differs"])

    def test_it_is_silent_when_the_identical_slot_agrees(self):
        rows, verdict = wg.screen_identical_slots(stream(OURS_AGREEING),
                                                  stream(TARGET_AGREEING))
        self.assertEqual("CLEAN", verdict)
        self.assertEqual([], rows)

    def test_an_identical_pair_of_streams_has_nothing_to_say(self):
        rows, verdict = wg.screen_identical_slots(stream(OURS), stream(OURS))
        self.assertEqual("CLEAN", verdict)
        self.assertEqual([], rows)


class TheExclusionsAreDeliberate(unittest.TestCase):
    """A symbol-spelling difference is decided by the datum screens, not by
    operand provenance (AGENTS.md residual discipline 3), so it must not be
    reported as a value difference."""

    def test_pool_datum_against_a_real_name_is_excluded(self):
        ours = ("expr", (0x38000000, "pool-datum"), ())
        target = ("expr", (0x38000000, "sndDbTable"), ())
        self.assertEqual("DATUM-SPELLING",
                         wg._excluded_symbol_diff(ours, target))

    def test_two_real_names_for_one_address_are_excluded_separately(self):
        ours = ("expr", (0x80000000, "gControllerButtons"), ())
        target = ("expr", (0x80000000, "sFlags"), ())
        self.assertEqual("NAMING-DRIFT",
                         wg._excluded_symbol_diff(ours, target))

    def test_a_different_instruction_form_is_never_excluded(self):
        ours = ("expr", (0x38000000, "pool-datum"), ())
        target = ("expr", (0x1C000000, None), ())
        self.assertFalse(wg._excluded_symbol_diff(ours, target))


if __name__ == "__main__":
    unittest.main()
