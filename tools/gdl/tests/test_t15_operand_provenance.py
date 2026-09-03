"""t15_operand_provenance: the screen that asks what VALUE each stream reads.

THE OBSERVATION (run-45 item 1). game/world/btricol::PointLineDist2D is
REGISTER_ONLY by every shipped view -- `webfrank_audit`'s classification,
`wf_census.classify`'s per-word `regfield` label, `fndiff --ops`'s identical
opcode multiset, `wf_word_diff`'s CLASS RECOLOR -- while its +0x30 `fmul`
multiplies a DIFFERENT operand pair in each stream (ours x*r, retail r*r), as
attempt.WC_pointlinedist2d-its-pin-covers-a-reassociated-product-not-a-recolor
.20260903.v1 measured.  Its rule ships on the corpus's one human-inspection
escape.  These tests pin the discriminant that separates the two cases, and
the three refinements the two-sided calibration forced:

  * commutation, straight and NESTED (InitAnim: an exchanged multiply feeding
    an exchanged multiply);
  * the CFG reaching-definition (CritterLineCollide: a linear backwards scan
    calls a value defined round a loop back edge an incoming parameter);
  * millicode calls preserve registers (CritterLineCollide again: every value
    read after the prologue `bl _savefpr_26` otherwise resolves to the call).

Also pinned: the screen never asserts a difference it did not expand -- a
comparison that bottoms out at the `--depth` limit is UNDECIDED, not
OPERAND-VALUE-DIFF.  Five of the six remaining false positives in the
calibration run were exactly that shape.
"""

import struct
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

import t15_operand_provenance as t15  # noqa: E402

BLR = 0x4E800020


def li(destination, value):
    return (14 << 26) | (destination << 21) | (value & 0xFFFF)


def add(destination, a, b):
    return (31 << 26) | (destination << 21) | (a << 16) | (b << 11) | (266 << 1)


def fmul(destination, a, c):
    return (63 << 26) | (destination << 21) | (a << 16) | (c << 6) | (25 << 1)


def fadds(destination, a, b):
    return (59 << 26) | (destination << 21) | (a << 16) | (b << 11) | (21 << 1)


def frsqrte(destination, b):
    return (63 << 26) | (destination << 21) | (b << 11) | (26 << 1)


def branch(here, there, link=False):
    return (18 << 26) | ((there - here) * 4 & 0x03FFFFFC) | (1 if link else 0)


def stream(words, relocations=None):
    blob = b"".join(struct.pack(">I", word) for word in words)
    return t15.Stream(blob, relocations or {})


def verdict(ours, target, depth=t15.DEFAULT_DEPTH, relocations=None):
    return t15.screen_pair(stream(ours, relocations),
                           stream(target, relocations), depth)[1]


class ReassociatedProductTests(unittest.TestCase):
    """The PointLineDist2D shape, reduced to four words."""

    def head(self):
        # f0 = x, f5 = frsqrte(x): identical in both streams.
        return [fadds(0, 3, 3), frsqrte(5, 0)]

    def test_a_reassociated_product_is_an_operand_value_difference(self):
        ours = self.head() + [fmul(1, 0, 5), BLR]      # x * r
        target = self.head() + [fmul(1, 5, 5), BLR]    # r * r
        self.assertEqual(verdict(ours, target), "OPERAND-VALUE-DIFF")

    def test_only_the_register_fields_differ_in_that_pair(self):
        """The premise the shipped views act on: this IS register-field-only,
        which is why every encoding-level classifier calls it a recolor."""
        ours, target = fmul(1, 0, 5), fmul(1, 5, 5)
        import webfrank as wf
        self.assertEqual((ours ^ target) & ~wf.register_slot_mask(ours), 0)

    def test_the_same_pair_exchanged_is_commuted_not_different(self):
        ours = self.head() + [fmul(1, 0, 5), BLR]
        target = self.head() + [fmul(1, 5, 0), BLR]
        self.assertEqual(verdict(ours, target), "PROVENANCE-CONSISTENT")

    def test_a_nested_exchange_is_absorbed_too(self):
        """InitAnim +0xb0 feeding +0xb4: the outer pair is only equal once the
        inner exchange is canonicalised inside the value token."""
        head = [fadds(0, 3, 3), frsqrte(5, 0)]
        ours = head + [fmul(2, 0, 5), fmul(3, 2, 0), BLR]
        target = head + [fmul(2, 5, 0), fmul(3, 0, 2), BLR]
        self.assertEqual(verdict(ours, target), "PROVENANCE-CONSISTENT")

    def test_a_consistent_renaming_is_not_flagged(self):
        ours = [li(3, 7), add(4, 3, 3), BLR]
        target = [li(5, 7), add(4, 5, 5), BLR]
        self.assertEqual(verdict(ours, target), "PROVENANCE-CONSISTENT")

    def test_a_non_register_bit_difference_is_out_of_scope(self):
        ours = [li(3, 7), BLR]
        target = [li(3, 8), BLR]
        self.assertEqual(verdict(ours, target), "NOT-REGISTER-ONLY")


class DepthTests(unittest.TestCase):
    def test_a_depth_limited_leaf_is_undecided_not_a_difference(self):
        ours = [fadds(0, 3, 3), frsqrte(5, 0), fmul(1, 0, 5), BLR]
        target = [fadds(0, 3, 3), frsqrte(5, 0), fmul(1, 5, 5), BLR]
        self.assertEqual(verdict(ours, target, depth=0), "UNDECIDED")

    def test_compare_values_is_three_valued(self):
        self.assertEqual(t15.compare_values(("const", 1), ("const", 1)),
                         t15.EQUAL)
        self.assertEqual(t15.compare_values(("const", 1), ("const", 2)),
                         t15.DIFFERENT)
        self.assertEqual(t15.compare_values(("insn", 4), ("const", 2)),
                         t15.UNDECIDED)

    def test_a_phi_is_compared_as_a_multiset(self):
        left = ("phi", (("const", 1), ("const", 2)))
        right = ("phi", (("const", 2), ("const", 1)))
        self.assertEqual(t15.compare_values(left, right), t15.EQUAL)
        other = ("phi", (("const", 2), ("const", 3)))
        self.assertEqual(t15.compare_values(left, other), t15.DIFFERENT)


class ControlFlowTests(unittest.TestCase):
    def test_a_definition_round_a_back_edge_reaches_its_use(self):
        """CritterLineCollide's shape: the function branches INTO the loop, so
        the reaching definition of the compared register sits textually AFTER
        the use.  A backwards linear scan reports the incoming parameter and
        calls two identical constants different values."""
        ours = [branch(0, 2), add(3, 5, 5), li(5, 7), branch(3, 1)]
        target = [branch(0, 2), add(3, 6, 6), li(6, 7), branch(3, 1)]
        self.assertEqual(verdict(ours, target), "PROVENANCE-CONSISTENT")

    def test_the_backwards_scan_reading_would_have_said_entry(self):
        """The refuted premise, pinned: at index 1 nothing textually earlier
        defines r5, so a linear scan has only the entry value to report."""
        ours = stream([branch(0, 2), add(3, 5, 5), li(5, 7), branch(3, 1)])
        self.assertEqual(t15.value_token(ours, 1, "g", 5, 3), ("const", 7))


class MillicodeTests(unittest.TestCase):
    def call_pair(self, name):
        relocations = {4: (10, name)}
        ours = [li(3, 5), branch(1, 9, link=True), add(6, 3, 3), BLR]
        target = [li(4, 5), branch(1, 9, link=True), add(6, 4, 4), BLR]
        return verdict(ours, target, relocations=relocations)

    def test_a_save_helper_preserves_the_value(self):
        self.assertEqual(self.call_pair("_savefpr_26"),
                         "PROVENANCE-CONSISTENT")

    def test_an_ordinary_call_clobbers_the_volatile_registers(self):
        """The negative control: without the millicode exemption the same two
        streams read two different call results."""
        self.assertEqual(self.call_pair("SomeCallee"), "OPERAND-VALUE-DIFF")


class PoolSymbolTests(unittest.TestCase):
    def test_the_two_pool_spellings_normalise_to_one_wildcard(self):
        """dtk names a contiguous run `lbl_ADDR`, our object emits `@N` or a
        section symbol: three spellings of a datum identity this screen does
        not decide (the datum screens do)."""
        for name in ("@114", "lbl_803457C8", "...bss.0", "..rodata.0"):
            self.assertEqual(t15._normalize_symbol(name), "pool-datum", name)

    def test_a_real_symbol_keeps_its_name(self):
        self.assertEqual(t15._normalize_symbol("gPlayers"), "gPlayers")
        self.assertIsNone(t15._normalize_symbol(None))


if __name__ == "__main__":
    unittest.main()
