import hashlib
import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from p6_probe import (  # noqa: E402
    Relocation,
    Symbol,
    plan_branch_pair_expansion,
)


FIXTURES = Path(__file__).resolve().parent / "fixtures"


class P6CompilerEvidenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.evidence = json.loads((FIXTURES / "p6_probe_expected.json").read_text())

    def test_all_three_compilers_preserve_the_fold(self):
        for artifact in ("configured", "vanilla", "profile"):
            for function, result in self.evidence["artifacts"][artifact]["functions"].items():
                with self.subTest(artifact=artifact, function=function):
                    self.assertTrue(result["has_expected_fold"])
        expected_words = {
            "p6_regfind": "41820018",
            "p6_tally": "41810008",
            "p6_shared_tail": "40800008",
        }
        for function, word in expected_words.items():
            self.assertEqual(
                self.evidence["artifacts"]["profile"]["functions"][function]["fold_word"],
                word,
            )

    def test_profile_adds_only_one_marker_pair_per_function(self):
        markers = self.evidence["profile_markers"]
        self.assertEqual(len(markers), 3)
        self.assertEqual(
            [marker["owners"][0]["function"] for marker in markers],
            ["p6_regfind", "p6_tally", "p6_shared_tail"],
        )
        for function in ("p6_regfind", "p6_tally", "p6_shared_tail"):
            normal = self.evidence["artifacts"]["vanilla"]["functions"][function]
            profile = self.evidence["artifacts"]["profile"]["functions"][function]
            self.assertEqual(profile["instruction_count"], normal["instruction_count"] + 2)

    def test_historical_and_local_frank_are_byte_identical_without_fallback(self):
        merge = self.evidence["merge"]
        self.assertTrue(merge["historical_local_byte_equal"])
        self.assertFalse(merge["local_vanilla"]["used_vanilla_fallback"])
        self.assertFalse(merge["local_configured"]["used_vanilla_fallback"])
        self.assertEqual(merge["local_vanilla"]["profile_markers"], 3)
        hashes = {
            self.evidence["artifacts"][name]["object_sha256"]
            for name in (
                "configured",
                "historical_frank",
                "local_configured_frank",
                "local_vanilla_frank",
            )
        }
        self.assertEqual(len(hashes), 1)


class P6SyntheticExpansionTests(unittest.TestCase):
    FOLDED = 0x41820008  # beq old+8
    PAIR = 0x40820008  # bne new+8
    CMP = 0x2C030000
    CALL = 0x48000001
    BLR = 0x4E800020

    def text(self, *words):
        return b"".join(word.to_bytes(4, "big") for word in words)

    def plan(self, text, **changes):
        arguments = {
            "site": 4,
            "expected_sha256": hashlib.sha256(text).hexdigest(),
            "expected_folded": self.FOLDED,
            "pair_conditional": self.PAIR,
            "relocations": (Relocation(8, "R_PPC_REL24", "callee"),),
            "symbols": (Symbol("probe", 0, 16), Symbol("next", 16, 4)),
        }
        arguments.update(changes)
        return plan_branch_pair_expansion(text, **arguments)

    def test_expansion_grows_text_and_remaps_symbols_and_relocation(self):
        original = self.text(self.CMP, self.FOLDED, self.CALL, self.BLR)
        plan = self.plan(original)
        self.assertEqual(plan.old_size, 16)
        self.assertEqual(plan.new_size, 20)
        self.assertEqual(
            plan.new_text,
            self.text(self.CMP, self.PAIR, 0x48000008, self.CALL, self.BLR),
        )
        self.assertEqual(
            plan.relocations,
            (Relocation(12, "R_PPC_REL24", "callee"),),
        )
        self.assertEqual(
            plan.symbols,
            (Symbol("probe", 0, 20), Symbol("next", 20, 4)),
        )

    def test_stale_hash_is_rejected(self):
        original = self.text(self.CMP, self.FOLDED, self.CALL, self.BLR)
        with self.assertRaisesRegex(ValueError, "stale text hash"):
            self.plan(original, expected_sha256="0" * 64)

    def test_ambiguous_folded_instruction_is_rejected(self):
        original = self.text(self.FOLDED, self.FOLDED, self.CALL, self.BLR)
        with self.assertRaisesRegex(ValueError, "ambiguous folded instruction"):
            self.plan(original)

    def test_candidate_relocation_is_rejected(self):
        original = self.text(self.CMP, self.FOLDED, self.CALL, self.BLR)
        relocations = (
            Relocation(4, "R_PPC_REL14", "label"),
            Relocation(8, "R_PPC_REL24", "callee"),
        )
        with self.assertRaisesRegex(ValueError, "candidate branch carries relocation"):
            self.plan(original, relocations=relocations)

    def test_unsupported_moved_control_instruction_is_rejected(self):
        bctr = 0x4E800420
        original = self.text(self.CMP, self.FOLDED, bctr, self.BLR)
        with self.assertRaisesRegex(ValueError, "unsupported indirect control"):
            self.plan(original, relocations=())

    def test_unrelocated_call_is_rejected(self):
        original = self.text(self.CMP, self.FOLDED, self.CALL, self.BLR)
        with self.assertRaisesRegex(ValueError, "unrelocated call"):
            self.plan(original, relocations=())


if __name__ == "__main__":
    unittest.main()
