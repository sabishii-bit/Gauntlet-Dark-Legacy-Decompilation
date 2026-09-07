#!/usr/bin/env python3
"""T3 run-59 item 7: a permitted sibling listed as unexpected reads as a
contradiction.

THE OBSERVATION (T2). `retire_audit`'s `sibling_bodies` check put a
permitted sibling whose RELOCATIONS had also changed under BOTH
`permitted_changes` and `unexpected_changes`. Both statements are true of
different aspects -- `--allow-changed-sibling` grants a monotonic BODY
change and never a relocation change -- but a certificate that names one
function as permitted and unexpected in the same breath cannot be read.

THE CURE separates the aspects instead of the names:

    permitted_changes    the BODY moved, was granted, and is monotonic
    relocation_changes   the RELOCATIONS moved -- no grant covers that
    unexpected_changes   the BODY moved and no grant covered it
    permission_errors    WHY a listed sibling could not be permitted
    verdict              one sentence naming which of the two failed

The verdict is unchanged in strength: FAIL when either list is non-empty.
The tests below hold both sides of that -- a permitted body change alone
still PASSES, and a relocation change still FAILS, now under a field whose
name says what happened.
"""

import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(TOOLS))

from tools.gdl import retire_audit as ra                      # noqa: E402

TARGET_WORD = "3860000f"          # li r3,15
BEFORE_WORD = "3860ffff"          # every target bit set, plus extra bits
AFTER_WORD = "3860000f"           # moved exactly onto the target
AWAY_WORD = "38700000"            # introduces a bit the target does not have
RELOC_A = [[0, 109, "gEnemyTable", 0]]
RELOC_B = [[0, 109, "gOtherTable", 0]]


def image(bodies, relocations=None):
    relocations = relocations or {}
    return {"functions": {
        name: {"body": body, "size": len(body) // 2, "binding": 1,
               "section": ".text", "offset": 0,
               "relocations": relocations.get(name, RELOC_A)}
        for name, body in bodies.items()}}


class SiblingReport(unittest.TestCase):
    def audit(self, before, after, target, allow=()):
        audit = ra.Audit("game/x/y", "retired_fn", "HEAD~1",
                         allow_changed=allow)
        audit.before_image = before
        audit.fresh_image = after
        audit.target_image = target
        audit.siblings()
        return audit.checks[-1]

    def build(self, *, sibling_after, sibling_relocs_after=None, allow=()):
        bodies_before = {"retired_fn": "60000000", "sibling": BEFORE_WORD}
        bodies_after = {"retired_fn": "38600001", "sibling": sibling_after}
        bodies_target = {"retired_fn": "38600001", "sibling": TARGET_WORD}
        after_relocs = {"sibling": sibling_relocs_after} \
            if sibling_relocs_after else None
        return self.audit(image(bodies_before), image(bodies_after,
                                                      after_relocs),
                          image(bodies_target), allow=allow)

    # -- the positive side -------------------------------------------------
    def test_an_untouched_sibling_passes_and_is_counted(self):
        check = self.build(sibling_after=BEFORE_WORD)
        self.assertEqual(check["status"], "PASS")
        self.assertEqual(check["changed"], [])
        self.assertEqual(check["siblings_compared"], 1)
        self.assertEqual(check["siblings_byte_equal"], 1)
        self.assertEqual(check["relocation_changes"], [])
        self.assertEqual(check["permission_errors"], {})

    def test_a_permitted_monotonic_body_change_passes(self):
        check = self.build(sibling_after=AFTER_WORD, allow=("sibling",))
        self.assertEqual(check["status"], "PASS")
        self.assertIn("sibling", check["permitted_changes"])
        self.assertEqual(check["permitted_changes"]["sibling"],
                         {"changed_words": 1, "new_exact_words": 1})
        self.assertEqual(check["unexpected_changes"], [])
        self.assertEqual(check["relocation_changes"], [])

    # -- the negative side -------------------------------------------------
    def test_an_ungranted_body_change_fails_and_says_why(self):
        check = self.build(sibling_after=AFTER_WORD)
        self.assertEqual(check["status"], "FAIL")
        self.assertEqual(check["unexpected_changes"], ["sibling"])
        self.assertEqual(check["permitted_changes"], {})
        self.assertIn("--allow-changed-sibling",
                      check["permission_errors"]["sibling"])
        self.assertIn("not permitted", check["verdict"])

    def test_a_non_monotonic_change_fails_with_its_refusal_text(self):
        check = self.build(sibling_after=AWAY_WORD, allow=("sibling",))
        self.assertEqual(check["status"], "FAIL")
        self.assertEqual(check["unexpected_changes"], ["sibling"])
        self.assertNotIn("sibling", check["permitted_changes"])
        self.assertIn("non-target bits",
                      check["permission_errors"]["sibling"])

    def test_a_relocation_change_fails_under_its_OWN_field(self):
        """The item itself: permitted body, forbidden relocation."""
        check = self.build(sibling_after=AFTER_WORD,
                           sibling_relocs_after=RELOC_B, allow=("sibling",))
        self.assertEqual(check["status"], "FAIL")
        self.assertEqual(check["relocation_changes"], ["sibling"])
        self.assertEqual(check["unexpected_changes"], [],
                         "a permitted body change must not read as"
                         " unexpected")
        self.assertIn("sibling", check["permitted_changes"])
        self.assertIn("ALSO changed relocations", check["verdict"])

    def test_no_name_is_ever_both_permitted_and_unexpected(self):
        """The contradiction, asserted directly over every shape above."""
        for kwargs in ({"sibling_after": BEFORE_WORD},
                       {"sibling_after": AFTER_WORD},
                       {"sibling_after": AFTER_WORD,
                        "allow": ("sibling",)},
                       {"sibling_after": AWAY_WORD, "allow": ("sibling",)},
                       {"sibling_after": AFTER_WORD,
                        "sibling_relocs_after": RELOC_B,
                        "allow": ("sibling",)}):
            check = self.build(**kwargs)
            self.assertEqual(
                set(check["permitted_changes"])
                & set(check["unexpected_changes"]), set(), kwargs)

    def test_a_changed_roster_still_fails_first(self):
        check = self.audit(image({"retired_fn": "60000000", "gone": "60000000"}),
                           image({"retired_fn": "60000000"}),
                           image({"retired_fn": "60000000"}))
        self.assertEqual(check["status"], "FAIL")
        self.assertEqual(check["removed"], ["gone"])


if __name__ == "__main__":
    unittest.main()
