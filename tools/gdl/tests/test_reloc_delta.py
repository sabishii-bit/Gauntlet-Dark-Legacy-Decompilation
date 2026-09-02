"""fndiff --relocs: the relocation-symbol-set delta (run 34 item 4).

`real` DROPS every reloc line and `--clean` NORMALIZES pool names, so a
wrong-callee / wrong-datum relocation reads as MATCH in every other fndiff
view — a REL24 callee lives ENTIRELY in its relocation, carrying no target
in the unlinked word. This delta resolves each symbol to its symbols.txt
address so two spellings of ONE address (a benign rename) cancel while a
genuinely different callee surfaces. CB hand-rolled it twice; it was
decisive both times.
"""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from fndiff import (reloc_rows_from_lines, reloc_set_delta,  # noqa: E402
                    truncate_ops)


# A deterministic resolver standing in for symbols.txt: two names denote one
# address (a rename), a third denotes a different address (a wrong callee),
# and locals resolve to None (compared by normalized name).
ADDR = {
    "sFlags": 0x803445CC,
    "gControllerButtons": 0x803445C8,  # +0x4 == sFlags
    "rightCallee": 0x80012340,
    "wrongCallee": 0x80099990,
}


def resolve(symbol):
    symbol = (symbol or "").strip()
    if symbol.startswith(("lbl", "jumptable", "@")):
        return None
    if "+0x" in symbol:
        name, _, addend = symbol.partition("+0x")
        base = ADDR.get(name)
        return None if base is None else base + int(addend, 16)
    return ADDR.get(symbol)


class RelocSetDeltaTests(unittest.TestCase):
    def test_identical_sets_have_no_delta(self):
        rows = [("R_PPC_REL24", "rightCallee"), ("R_PPC_ADDR16_HA", "sFlags")]
        t_only, o_only, common = reloc_set_delta(rows, list(rows),
                                                 resolve=resolve)
        self.assertEqual((t_only, o_only), ([], []))
        self.assertEqual(common, 2)

    def test_a_rename_to_the_same_address_cancels(self):
        """gControllerButtons+0x4 and sFlags are one address — not a defect."""
        t = [("R_PPC_ADDR16_LO", "sFlags")]
        b = [("R_PPC_ADDR16_LO", "gControllerButtons+0x4")]
        t_only, o_only, common = reloc_set_delta(t, b, resolve=resolve)
        self.assertEqual((t_only, o_only), ([], []))
        self.assertEqual(common, 1)

    def test_a_wrong_callee_is_a_delta_in_both_directions(self):
        t = [("R_PPC_REL24", "rightCallee")]
        b = [("R_PPC_REL24", "wrongCallee")]
        t_only, o_only, common = reloc_set_delta(t, b, resolve=resolve)
        self.assertEqual(len(t_only), 1)
        self.assertEqual(len(o_only), 1)
        self.assertIn("rightCallee", t_only[0])
        self.assertIn("wrongCallee", o_only[0])
        self.assertEqual(common, 0)

    def test_the_display_resolves_the_address(self):
        t_only, _, _ = reloc_set_delta([("R_PPC_REL24", "rightCallee")], [],
                                       resolve=resolve)
        self.assertIn("0x80012340", t_only[0])

    def test_a_reloc_type_change_at_one_symbol_is_a_delta(self):
        t = [("R_PPC_ADDR16_HA", "sFlags")]
        b = [("R_PPC_ADDR16_LO", "sFlags")]
        t_only, o_only, _ = reloc_set_delta(t, b, resolve=resolve)
        self.assertEqual((len(t_only), len(o_only)), (1, 1))

    def test_multiset_a_duplicate_on_one_side_only_is_one_row(self):
        t = [("R_PPC_REL24", "rightCallee"),
             ("R_PPC_REL24", "rightCallee")]
        b = [("R_PPC_REL24", "rightCallee")]
        t_only, o_only, common = reloc_set_delta(t, b, resolve=resolve)
        self.assertEqual(len(t_only), 1)
        self.assertEqual(o_only, [])
        self.assertEqual(common, 1)

    def test_unresolved_locals_compare_by_normalized_name(self):
        """Two anonymous pool locals must not be forced into a false delta."""
        t = [("R_PPC_EMB_SDA21", "@123")]
        b = [("R_PPC_EMB_SDA21", "@456")]
        t_only, o_only, _ = reloc_set_delta(t, b, resolve=resolve)
        self.assertEqual((t_only, o_only), ([], []))

    def test_dtk_suffixed_local_labels_cancel_by_type_and_count(self):
        """dtk names a local jump table jumptable_80120B4C; our unlinked
        object emits it at a different address. Same type + count is benign
        local churn, not a wrong-callee defect."""
        t = [("R_PPC_ADDR16_HA", "jumptable_80120B4C"),
             ("R_PPC_ADDR16_LO", "lbl_80347F3C")]
        b = [("R_PPC_ADDR16_HA", "jumptable_800AAAAA"),
             ("R_PPC_ADDR16_LO", "lbl_80100000")]
        t_only, o_only, common = reloc_set_delta(t, b, resolve=resolve)
        self.assertEqual((t_only, o_only), ([], []))
        self.assertEqual(common, 2)

    def test_a_missing_local_of_a_TYPE_still_shows(self):
        """A count mismatch is a real structural delta even for locals."""
        t = [("R_PPC_ADDR16_HA", "jumptable_80120B4C"),
             ("R_PPC_ADDR16_HA", "jumptable_80120B50")]
        b = [("R_PPC_ADDR16_HA", "jumptable_800AAAAA")]
        t_only, o_only, _ = reloc_set_delta(t, b, resolve=resolve)
        self.assertEqual((len(t_only), len(o_only)), (1, 0))

    def test_rows_extract_only_the_indented_reloc_lines(self):
        lines = [
            "80012340:  bl someFunc",
            "    R_PPC_REL24 someFunc",
            "80012344:  lis r3, sFlags@ha",
            "    R_PPC_ADDR16_HA sFlags",
        ]
        self.assertEqual(
            reloc_rows_from_lines(lines),
            [("R_PPC_REL24", "someFunc"), ("R_PPC_ADDR16_HA", "sFlags")])


class TruncateOpsTests(unittest.TestCase):
    """Run 34 item 5: a truncated --ops view must announce dropped IMMEDIATE
    rows, never silently cut them (they sit below the clusters, and one such
    cut read as a frame collapse when the residual was a changed literal)."""

    def _dump(self, n_head, n_imm):
        head = [f"  replace T[{i}] O[{i}]" for i in range(n_head)]
        imm = [f"  IMMEDIATE T[{i}]@0  O[{i}]@0   T: li r3,{i}   O: li r3,{i+1}"
               for i in range(n_imm)]
        return "\n".join(head + imm)

    def test_short_output_is_returned_whole(self):
        text = self._dump(3, 1)
        self.assertEqual(truncate_ops(text, 16), text.strip())

    def test_a_dropped_immediate_is_counted(self):
        text = self._dump(16, 3)
        out = truncate_ops(text, 16)
        self.assertIn("3 IMMEDIATE row(s) suppressed", out)

    def test_a_dropped_immediate_is_flagged_as_eligibility_deciding(self):
        out = truncate_ops(self._dump(16, 2), 16)
        self.assertIn("eligibility", out)
        self.assertIn("fndiff --ops", out)

    def test_non_immediate_overflow_still_notes_the_cut(self):
        out = truncate_ops(self._dump(30, 0), 16)
        self.assertIn("suppressed", out)
        self.assertNotIn("IMMEDIATE", out)

    def test_kept_lines_are_the_first_limit(self):
        out = truncate_ops(self._dump(20, 0), 16)
        self.assertEqual(len(out.splitlines()), 17)  # 16 kept + 1 note


if __name__ == "__main__":
    unittest.main()
