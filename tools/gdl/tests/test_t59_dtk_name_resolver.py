#!/usr/bin/env python3
"""T3 run-59 item 4: ONE dtk `_80XXXXXX` resolver, and its guard.

Three copies of the reduction existed — `fndiff.parse`,
`regnorm.strip_dtk_suffix`/`regnorm.resolve_name` and
`wf_ordered_datum_screen._strip`/`resolve_function` — and two were born
without the placeholder guard. dtk spells an unnamed function
`fn_800516F8`, a pool datum `lbl_80346840` and a switch table
`jumptable_80120B4C`; the tail of every one of them IS `_80` plus six hex
digits, so an unguarded strip maps a whole population onto the single key
`fn`, `lbl` or `jumptable`. Sixteen pins fell off a roster that way.

Two-sided. The valid side is that a REAL dtk-suffixed local static still
reduces and still resolves in both directions. The invalid side is the
population that must NOT reduce, asserted separately through EVERY
published spelling, so a module that grows its own unguarded copy again
fails here by name rather than silently under-reporting a roster.

`fndiff.dtk_name_reducer` covers the second half of the same duplication:
`parse`, `raw_signature` and `raw_words_signature` each carried the
two-pass collision count, and a signature keyed on a name the line table
does not hold is not comparable to anything.

Live calibration (build/t3_scratch/t3_resolver_calibration.py at
7dc6deecf, the commit before the centralization): 6,264 function keys over
every object in build/GUNE5D/{obj,src} and 314 pin resolutions over the 53
pinned units, old implementations against the core — 0 moved keys and 0
disagreements. The change closes a blind spot; it moves no live verdict.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))
sys.path.insert(0, str(REPO / "tools" / "gdl" / "composed_census"))

import fndiff  # noqa: E402
import regnorm  # noqa: E402
import wf_ordered_datum_screen as screen  # noqa: E402

# The names whose trailing `_80XXXXXX` IS the identity.
PLACEHOLDERS = ("fn_800516F8", "fn_80051C78", "lbl_80346840",
                "jumptable_80120B4C")
# A real dtk-suffixed file-local static, and its base.
LOCAL, LOCAL_BASE = "gendir_8004FBC8", "gendir"


class TheGuard(unittest.TestCase):
    """The invalid side: what must NEVER be reduced."""

    def test_the_core_never_strips_a_placeholder_name(self):
        for name in PLACEHOLDERS:
            self.assertEqual(fndiff.strip_dtk_suffix(name), name, name)

    def test_regnorms_spelling_never_strips_a_placeholder_name(self):
        for name in PLACEHOLDERS:
            self.assertEqual(regnorm.strip_dtk_suffix(name), name, name)

    def test_the_datum_screens_spelling_never_strips_a_placeholder(self):
        for name in PLACEHOLDERS:
            self.assertEqual(screen._strip(name), name, name)

    def test_a_tail_that_is_not_the_dtk_form_is_left_alone(self):
        for name in ("gendir_deadbeef", "gendir_81ABCDEF", "gendir_80ABCD",
                     "gendir_80ABCDEF0", "gendir"):
            self.assertEqual(fndiff.strip_dtk_suffix(name), name, name)

    def test_a_placeholder_query_never_resolves_onto_another_placeholder(self):
        table = {"fn_800516F8": [], "fn_80051C78": []}
        self.assertEqual(
            fndiff.resolve_function_name(table, "fn_800516F8"),
            "fn_800516F8")
        self.assertIsNone(
            fndiff.resolve_function_name(table, "fn_80099999"))
        self.assertIsNone(fndiff.resolve_function_name(table, "fn"))


class TheValidSide(unittest.TestCase):
    def test_a_real_local_static_reduces(self):
        self.assertEqual(fndiff.strip_dtk_suffix(LOCAL), LOCAL_BASE)

    def test_a_suffixed_query_finds_a_stripped_table(self):
        self.assertEqual(
            fndiff.resolve_function_name({LOCAL_BASE: [], "do_ai": []},
                                         LOCAL),
            LOCAL_BASE)

    def test_a_stripped_query_finds_a_suffixed_table(self):
        self.assertEqual(
            fndiff.resolve_function_name({LOCAL: [], "do_ai": []},
                                         LOCAL_BASE),
            LOCAL)

    def test_an_exact_key_wins_before_any_reduction(self):
        table = {LOCAL: [], LOCAL_BASE: []}
        self.assertEqual(fndiff.resolve_function_name(table, LOCAL), LOCAL)
        self.assertEqual(
            fndiff.resolve_function_name(table, LOCAL_BASE), LOCAL_BASE)

    def test_an_ambiguous_base_refuses_rather_than_guessing(self):
        table = {"dtor_800DB21C": [], "dtor_800DBB94": []}
        self.assertIsNone(fndiff.resolve_function_name(table, "dtor"))
        self.assertIsNone(
            fndiff.resolve_function_name(table, "dtor_800DC000"))

    def test_an_absent_function_is_None(self):
        self.assertIsNone(
            fndiff.resolve_function_name({LOCAL_BASE: []}, "no_such_fn"))
        self.assertIsNone(
            fndiff.resolve_function_name({LOCAL: []}, "gen"))


class EveryPublishedSpellingIsTheCore(unittest.TestCase):
    """`regnorm.resolve_name` and `screen.resolve_function` ARE the core.

    Answers are compared over the whole corpus rather than by identity of
    the function object, so a module may keep its own name and docstring
    but may not keep its own answers.
    """

    CORPUS = (
        ({LOCAL_BASE: [], "do_ai": []}, LOCAL),
        ({LOCAL: [], "do_ai": []}, LOCAL_BASE),
        ({LOCAL: [], LOCAL_BASE: []}, LOCAL),
        ({"dtor_800DB21C": [], "dtor_800DBB94": []}, "dtor"),
        ({"dtor_800DB21C": [], "dtor_800DBB94": []}, "dtor_800DC000"),
        ({"fn_800516F8": [], "fn_80051C78": []}, "fn_800516F8"),
        ({"fn_800516F8": [], "fn_80051C78": []}, "fn_80099999"),
        ({"fn_800516F8": [], "fn_80051C78": []}, "fn"),
        ({"lbl_80346840": []}, "lbl"),
        ({"jumptable_80120B4C": []}, "jumptable"),
        ({LOCAL_BASE: []}, "no_such_fn"),
    )

    def test_regnorm_resolve_name_agrees_with_the_core_everywhere(self):
        for table, name in self.CORPUS:
            self.assertEqual(regnorm.resolve_name(table, name),
                             fndiff.resolve_function_name(table, name),
                             (sorted(table), name))

    def test_the_datum_screen_agrees_with_the_core_everywhere(self):
        for table, name in self.CORPUS:
            self.assertEqual(screen.resolve_function(table, name),
                             fndiff.resolve_function_name(table, name),
                             (sorted(table), name))

    def test_no_module_keeps_a_private_dtk_suffix_regex(self):
        """The duplication itself, asserted at the source level."""
        for relative in ("regnorm.py",
                         "composed_census/wf_ordered_datum_screen.py"):
            text = (REPO / "tools" / "gdl" / relative).read_text(
                encoding="utf-8")
            self.assertNotIn('re.compile(r"_80[0-9A-Fa-f]{6}$")', text,
                             relative)


class TheCollisionReducer(unittest.TestCase):
    """`dtk_name_reducer`: the two-pass half `parse` and the two raw
    signature readers each carried separately."""

    @staticmethod
    def dump(*names):
        return "".join(
            f"{index:08x} <{name}>:\n       0:\t4e 80 00 20 \tblr\n"
            for index, name in enumerate(names))

    def test_a_unique_base_is_reduced(self):
        reduce = fndiff.dtk_name_reducer(self.dump(LOCAL, "do_ai"))
        self.assertEqual(reduce(LOCAL), LOCAL_BASE)

    def test_a_colliding_base_keeps_both_suffixes(self):
        reduce = fndiff.dtk_name_reducer(
            self.dump("dtor_800DB21C", "dtor_800DBB94"))
        self.assertEqual(reduce("dtor_800DB21C"), "dtor_800DB21C")
        self.assertEqual(reduce("dtor_800DBB94"), "dtor_800DBB94")

    def test_a_suffixed_name_never_overwrites_an_unsuffixed_twin(self):
        """`foo` and `foo_80ABCDEF` in one object are TWO functions."""
        reduce = fndiff.dtk_name_reducer(self.dump("foo", "foo_80ABCDEF"))
        self.assertEqual(reduce("foo"), "foo")
        self.assertEqual(reduce("foo_80ABCDEF"), "foo_80ABCDEF")

    def test_placeholder_names_are_never_collapsed_onto_one_key(self):
        reduce = fndiff.dtk_name_reducer(self.dump(*PLACEHOLDERS))
        for name in PLACEHOLDERS:
            self.assertEqual(reduce(name), name, name)

    def test_parse_and_the_raw_signature_readers_key_on_one_name(self):
        obj = REPO / "build" / "GUNE5D" / "obj" / "game" / "enemy" / "enemy.o"
        if not obj.exists() or not Path(fndiff.OBJDUMP).exists():
            self.skipTest("enemy target object or objdump missing")
        keys = set(fndiff.parse(obj))
        # The live pin this whole item is about: webfrank.json spells it
        # `gendir_8004FBC8`, the table keys it `gendir`, and the resolver
        # is what joins them.
        self.assertNotIn(LOCAL, keys)
        self.assertEqual(fndiff.resolve_function_name(keys, LOCAL),
                         LOCAL_BASE)
        self.assertEqual(keys, set(fndiff.raw_signature(obj)))
        self.assertEqual(keys, set(fndiff.raw_words_signature(obj)))


if __name__ == "__main__":
    unittest.main()
