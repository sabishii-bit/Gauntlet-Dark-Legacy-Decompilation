#!/usr/bin/env python3
"""T21 run-51 item 1: the dtk address-suffix strip must GUARD placeholder
names, in probe.py and in fndiff.py alike.

THE OBSERVATION, reproduced verbatim at 0f4151839 on
game/movie/movieplayer::fn_800D967C (claim.law.MP_probe-raw-drops-the-raw-
word-count-for-every-address-suffixed-name-and-its-tu-gate-false-alarms-on-a-
pinned-tu.20260903.v1 reported it):

    $ python tools/gdl/probe.py game/movie/movieplayer fn_800D967C --raw
    BASELINE  real 0 (insns exact, multiset 0t)
    [RAW WORDS: not measurable — the two streams are count-asymmetric, or the
     raw body could not be read. A count-asymmetric function is outside every
     postprocessor class by construction, so there is no word residual to
     arbitrate on.]

    $ python tools/gdl/composed_census/wf_word_diff.py \
          game/movie/movieplayer fn_800D967C
    game/movie/movieplayer::fn_800D967C (raw postprocess body): 13 insns,
    DIFFERING WORDS = 0, MNEMONIC DIVERGENCE = 0, RELOC-SYMBOL MISMATCH = 0,
    PINNED = no

probe.py stripped `_80XXXXXX` from the user-supplied name and passed the
STRIPPED name alone to `raw_word_residual`, so `fn_800D967C` became the
string `fn`, `webfrank._find_symbol` raised `KeyError: "symbol 'fn' not
found"`, and the blanket `except Exception` turned that into the fallback
message above — which asserts the ONE conclusion (count-asymmetric, outside
every postprocessor class) that is false for this function.

TWO-SIDED CALIBRATION of the guard, over the 1,641 function names in the 53
raw postprocess bodies `--raw` scores (T21_scratch/t21_suffix_census.py):

    275 names match the strip regex, 275 of 275 (100%) are `fn_*`
    POSITIVES  260 measurable under the FULL spelling and not the stripped
               one — raw word counts the loop could never print
    NEGATIVES    0 measurable under the stripped spelling and not the full
               one, so the guard removes nothing
    NEITHER     15 genuinely count-asymmetric, which is what the fallback
               message is for

SIBLING SCREEN (AGENTS.md discipline 18 — what else has this shape):
fndiff.parse guards `lbl` and `jumptable` in its RELOCATION strip but never
added `fn`, so 2,004 relocations across 641 objects recorded their callee as
the bare symbol `fn`; and `symbol_addresses` registered its stripped aliases
unguarded, minting `lbl` (from 4,282 names), `fn` (307) and `jumptable` (97)
as keys resolving to whichever name parsed first. Two-sided over all 3,032
comparable function pairs in 257 units (T21_scratch/t21_relguard_impact.py):
0 pairs change verdict in either direction — this closes a blind spot rather
than fixing a live miscall, which is also why it costs no lane a rebaseline.
"""

import os
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import fndiff  # noqa: E402
import probe   # noqa: E402

MOVIEPLAYER = "game/movie/movieplayer"
RAW_BODY = (REPO / "build" / "GUNE5D" / "src" / "game" / "movie"
            / ".postprocess" / "body" / "movieplayer.o")


class StripGuard(unittest.TestCase):
    """A placeholder name's suffix IS its identity."""

    def test_fn_names_are_never_stripped(self):
        self.assertEqual(probe.strip_dtk_suffix("fn_800D967C"),
                         "fn_800D967C")

    def test_lbl_and_jumptable_are_never_stripped(self):
        self.assertEqual(probe.strip_dtk_suffix("lbl_8023D000"),
                         "lbl_8023D000")
        self.assertEqual(probe.strip_dtk_suffix("jumptable_80118904"),
                         "jumptable_80118904")

    def test_a_real_dtk_local_static_still_strips(self):
        # The case the strip exists for: the target names our file-static
        # `long2str` as `long2str_800E74B8`.
        self.assertEqual(probe.strip_dtk_suffix("long2str_800E74B8"),
                         "long2str")
        self.assertEqual(probe.strip_dtk_suffix("DiffRate_8002951C"),
                         "DiffRate")

    def test_an_unsuffixed_name_is_returned_unchanged(self):
        self.assertEqual(probe.strip_dtk_suffix("PlayerControls"),
                         "PlayerControls")

    def test_probe_and_fndiff_share_one_prefix_set(self):
        # They drifted apart once already — fndiff guarded lbl/jumptable in
        # its relocation path while probe guarded nothing — so the two are
        # asserted equal rather than each asserted separately.
        self.assertEqual(probe.PLACEHOLDER_NAME_PREFIXES,
                         fndiff.PLACEHOLDER_NAME_PREFIXES)
        self.assertEqual(
            fndiff.PLACEHOLDER_STEMS,
            tuple(p[:-1] for p in fndiff.PLACEHOLDER_NAME_PREFIXES))
        self.assertIn("fn", fndiff.PLACEHOLDER_STEMS)


class ZeroWordLine(unittest.TestCase):
    """A CLASS verdict over zero differing words is a sentence about nothing.

    Only reachable since this item: an `fn_*` function could never get a
    count at all, and the first one that did printed
    `RAW WORDS = 0 of 13 insns; CLASS: RECOLOR — index-aligned, only register
    fields differ.`
    """

    def test_zero_words_reports_no_residual_instead_of_a_class(self):
        line = probe.raw_words_line(0, None, 13, 0, False,
                                    {"REGFIELD-ONLY": 0, "IMMEDIATE": 0,
                                     "BRANCH": 0, "OPCODE": 0,
                                     "RELOCATED": 0})
        self.assertIn("RAW WORDS = 0", line)
        self.assertNotIn("CLASS:", line)
        self.assertIn("no", line.lower())

    def test_zero_words_still_carries_its_delta(self):
        self.assertIn("(-8 vs the last probe's 8)",
                      probe.raw_words_line(0, 8, 13, 0, False))

    def test_a_pinned_zero_is_named_as_a_promotion_candidate(self):
        # The promotion directive's own signal: the rule discharges nothing.
        line = probe.raw_words_line(0, None, 13, 0, True)
        self.assertIn("DELETED", line)

    def test_a_nonzero_count_keeps_its_class_line(self):
        self.assertIn("CLASS: RECOLOR",
                      probe.raw_words_line(8, None, 86, 0, False))

    def test_unmeasurable_is_still_distinct_from_zero(self):
        self.assertIn("not measurable",
                      probe.raw_words_line(None, None, None, None, False))


class AliasTable(unittest.TestCase):
    """symbol_addresses must not mint a key from a placeholder stem."""

    def test_placeholder_stems_are_not_alias_keys(self):
        table = fndiff.symbol_addresses()
        for stem in fndiff.PLACEHOLDER_STEMS:
            self.assertNotIn(stem, table)

    def test_real_local_static_aliases_are_preserved(self):
        # The 27 genuine dtk local statics are what the alias is FOR.
        table = fndiff.symbol_addresses()
        self.assertEqual(table.get("DiffRate"), 0x8002951C)
        self.assertEqual(table.get("adjust_radius"), 0x8002B2D4)


@unittest.skipUnless(RAW_BODY.exists(), f"needs a built {RAW_BODY}")
class RawWordResidualSpellings(unittest.TestCase):
    """The measurement itself, against the object the law names."""

    def test_the_full_placeholder_spelling_measures(self):
        measured = probe.raw_word_residual(
            MOVIEPLAYER, "fn_800D967C",
            probe.strip_dtk_suffix("fn_800D967C"))
        self.assertIsNotNone(measured)
        self.assertEqual(measured[1], 13)          # insns, per wf_word_diff

    def test_the_collapsed_spelling_alone_measures_nothing(self):
        # The pre-fix call: this is exactly what probe used to pass.
        self.assertIsNone(probe.raw_word_residual(MOVIEPLAYER, "fn"))

    def test_a_plain_name_still_measures(self):
        measured = probe.raw_word_residual(
            MOVIEPLAYER, "PlayVQMovie",
            probe.strip_dtk_suffix("PlayVQMovie"))
        self.assertIsNotNone(measured)

    def test_a_count_asymmetric_function_returns_none_not_systemexit(self):
        # word_streams raises SystemExit, which is a BaseException: the old
        # blanket `except Exception` never caught it, so the fallback line
        # the docstring promises was unreachable and the probe died instead.
        unit, fn = "game/enemy/enemy", "fn_80051C78"
        if not (REPO / "build" / "GUNE5D" / "src" / "game" / "enemy"
                / ".postprocess" / "body" / "enemy.o").exists():
            self.skipTest("needs a built enemy raw body")
        self.assertIsNone(probe.raw_word_residual(
            unit, fn, probe.strip_dtk_suffix(fn)))


@unittest.skipUnless(
    (REPO / "build" / "GUNE5D" / "obj" / "game" / "anim"
     / "anim_play.o").exists(), "needs the dtk split objects")
class RelocationText(unittest.TestCase):
    """A call to an unnamed function keeps the address that identifies it."""

    def test_no_relocation_is_recorded_as_a_bare_placeholder_stem(self):
        obj = (REPO / "build" / "GUNE5D" / "obj" / "game" / "anim"
               / "anim_play.o")
        stems = set(fndiff.PLACEHOLDER_STEMS)
        bare, suffixed = 0, 0
        for lines in fndiff.parse(obj).values():
            for line in lines:
                if not line.startswith("    "):
                    continue
                parts = line.strip().split(maxsplit=1)
                if len(parts) < 2:
                    continue
                if parts[1] in stems:
                    bare += 1
                if parts[1].startswith("fn_80"):
                    suffixed += 1
        self.assertEqual(bare, 0)
        # and the calls are still there, under their identifying names
        self.assertGreater(suffixed, 0)


if __name__ == "__main__":
    os.environ.setdefault("GDL_CLAIM_SCREEN", "off")
    unittest.main()
