"""A pin abort names the pin and prints the raw loop (run 61 item 4).

THE OBSERVATION (ER2 lane): "a pinned TU cannot be probed with probe.py --
the shipped-object build trips the pin; the only safe loop is `ninja
build/GUNE5D/src/<dir>/.postprocess/body/<unit>.o` + `wf_word_diff --unit
--json`, and no tool documents it."

HALF OF IT IS STALE AND MEASURED SO. `probe.py <unit> <fn> --raw` already
builds exactly that body object (run-39 item 10) and already prints the raw
differing-word count (run-48 item 1). Verified at 9dc9c0e26:

    python tools/gdl/probe.py game/enemy/enemy do_enemy_move --raw --stateless
    [--raw: building build/GUNE5D/src/game/enemy/.postprocess/body/enemy.o
     - the compiler's own output, WITHOUT driving the WEBFRANK edge, so a
     stale pin cannot block this score]
    STATELESS real 6 (insns T905/O905)
    RAW WORDS = 3 of 905 insns; CLASS: RECOLOR-SHAPED BUT NOT RECOLOURABLE
      DECODE: REGFIELD-ONLY 2, IMMEDIATE 1 ...  PINNED

THE HALF THAT REPRODUCED. The DEFAULT loop dumps a build tail and says
nothing about `--raw`; and `pin_named_by_build` returned None on the abort a
stale pin actually produces. Captured without touching any tracked file, by
copying game/enemy/enemy's own body object, flipping ONE bit inside the
pinned do_enemy_move and running the real WebFrank edge over the copy
(build/t4_scratch/t4_pin_abort_probe2.py):

    ValueError: do_enemy_move: input hash e403b1d0af9906edaca7f63805a75afe
                622d492379d12ae7a6a15c996b13e058
                != expected a2927c9a7e7c9e4a4c29bb6398b603af63c8c4d669
                37d066b8714a613eba3b19
    probe.pin_named_by_build(...) -> None
"""
import unittest

from tools.gdl import probe

# The captured abort, verbatim from the reproduction above.
BODY_HASH_ABORT = (
    "[7/9] WEBFRANK build/GUNE5D/src/game/enemy/enemy.o\n"
    "Traceback (most recent call last):\n"
    "  File \"tools/gdl/webfrank.py\", line 5626, in main\n"
    "    _, _, changed = apply_patch(data, patch, target_data,\n"
    "ValueError: do_enemy_move: input hash "
    "e403b1d0af9906edaca7f63805a75afe622d492379d12ae7a6a15c996b13e058"
    " != expected "
    "a2927c9a7e7c9e4a4c29bb6398b603af63c8c4d66937d066b8714a613eba3b19\n"
    "ninja: build stopped: subcommand failed.\n")

OUTPUT_HASH_ABORT = BODY_HASH_ABORT.replace("input hash", "output hash")

MISSING_SYMBOL_ABORT = (
    "  File \"tools/gdl/webfrank.py\", line 128, in _find_symbol\n"
    "    raise KeyError(f\"symbol {name!r} not found\")\n"
    "KeyError: \"symbol 'do_enemy_move' not found\"\n")

REDERIVE_HINT_ABORT = (
    "WEBFRANK: this is the RELOCATION-hash class - the window's instruction"
    " bytes are unchanged and only its relocation hashes moved.\n"
    "    python tools/gdl/probe.py game/game/player do_exit --rederive-pin\n"
    "  Add --transient if this is a throwaway A/B.")


class AbortDetection(unittest.TestCase):
    def test_the_body_hash_abort_now_names_its_pin(self):
        self.assertEqual(probe.pin_abort_details(BODY_HASH_ABORT),
                         ("do_enemy_move", "body-hash"))
        self.assertEqual(probe.pin_named_by_build(BODY_HASH_ABORT),
                         "do_enemy_move")

    def test_the_output_hash_abort_is_the_same_class(self):
        self.assertEqual(probe.pin_abort_details(OUTPUT_HASH_ABORT),
                         ("do_enemy_move", "body-hash"))

    def test_a_missing_pinned_symbol_is_its_own_class(self):
        self.assertEqual(probe.pin_abort_details(MISSING_SYMBOL_ABORT),
                         ("do_enemy_move", "missing-symbol"))

    def test_the_rederive_hint_still_wins_and_is_labelled(self):
        self.assertEqual(probe.pin_abort_details(REDERIVE_HINT_ABORT),
                         ("do_exit", "rederive-hint"))

    def test_a_build_failure_that_is_NOT_a_pin_abort_detects_nothing(self):
        for text in ("ninja: build stopped: subcommand failed.",
                     "src/game/enemy/enemy.c:41: error: undeclared 'foo'",
                     "ValueError: something: input hash is not sixty-four",
                     "", None):
            with self.subTest(text=text):
                self.assertIsNone(probe.pin_abort_details(text))
                self.assertIsNone(probe.pin_named_by_build(text))


class EscapeText(unittest.TestCase):
    UNIT, FN = "game/enemy/enemy", "closest_enemy"
    BODY = "build/GUNE5D/src/game/enemy/.postprocess/body/enemy.o"

    def test_the_refusal_prints_the_raw_flag_and_the_manual_loop(self):
        text = probe.pinned_build_escape(self.UNIT, self.FN, BODY_HASH_ABORT)
        self.assertIn(f"probe.py {self.UNIT} {self.FN} --raw", text)
        self.assertIn(f"ninja {self.BODY}", text)
        self.assertIn(f"wf_word_diff.py --unit {self.UNIT} --json", text)
        self.assertIn("do_enemy_move", text)
        self.assertIn("did not fail on your source", text)

    def test_a_body_hash_abort_does_NOT_advise_re_deriving(self):
        text = probe.pinned_build_escape(self.UNIT, self.FN, BODY_HASH_ABORT)
        self.assertNotIn("--rederive-pin", text)
        self.assertIn("Re-deriving is NOT the cure", text)

    def test_a_relocation_hash_abort_DOES_advise_re_deriving(self):
        text = probe.pinned_build_escape("game/game/player", "do_players",
                                         REDERIVE_HINT_ABORT)
        self.assertIn("probe.py game/game/player do_exit --rederive-pin", text)

    def test_a_non_pin_build_failure_gets_no_escape_text(self):
        self.assertIsNone(probe.pinned_build_escape(
            self.UNIT, self.FN, "ninja: build stopped: subcommand failed."))

    def test_the_body_target_is_the_one_raw_object_resolution_names(self):
        # If these ever diverge the refusal would print a target that does
        # not exist, which is worse than no advice at all.
        import os
        os.chdir(str(probe.Path.cwd()))
        text = probe.pinned_build_escape(self.UNIT, self.FN, BODY_HASH_ABORT)
        try:
            resolved = probe.raw_object_target(self.UNIT)
        except Exception as error:                # unbuilt/unconfigured tree
            raise unittest.SkipTest("raw object unresolvable: %s" % error)
        self.assertIn(f"ninja {resolved}", text)


if __name__ == "__main__":
    unittest.main()
