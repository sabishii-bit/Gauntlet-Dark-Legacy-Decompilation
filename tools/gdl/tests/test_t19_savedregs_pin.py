#!/usr/bin/env python3
"""T19 run-49 item 6: savedregs must say when it read a PINNED body.

THE DEFECT (DA). Without `--raw`, savedregs decodes our POSTPROCESSED
object. On a webfrank-pinned function that body is driven toward the target
by construction, so the table reports the postprocessor's work as if it were
the compiler's -- and prints a confident `ASSIGNMENT MATCHES` about source
that does not produce that assignment.

TWO-SIDED CENSUS at 976321418 over all 156 pinned functions in
config/GUNE5D/webfrank.json (every one decodable in both modes):

    default table == --raw table                24  (15%)
    default table DIFFERS from --raw           132  (85%)
    ... of which the ASSIGNMENT VERDICT flips    84  (54%)

All 84 flip the same way: the default says `ASSIGNMENT MATCHES` and `--raw`
contradicts it; none flips the other way. Reproduced end to end on
game/anim/anim::InitAnim, where the default reports 2 differing rows the
table cannot see and `--raw` reports 5.

THE NEGATIVE SIDE decided that this is a WARNING on the default rather than
a change of default: on an UNPINNED function the two reads are the same
object (measured identical on game/world/camera::camera_mode_dest and
game/game/player::do_players), and in a TU with no `.postprocess` stage
`--raw` refuses outright -- so warning there would be noise and switching
the default would break every unpinned caller for nothing.
"""

import json
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import savedregs  # noqa: E402

PINS = {("game/anim/anim", "InitAnim")}


class Warning(unittest.TestCase):
    def test_a_pinned_function_read_without_raw_is_warned(self):
        note = savedregs.pin_warning("game/anim/anim", "InitAnim", False, PINS)
        self.assertIn("WEBFRANK-PINNED", note)
        self.assertIn("POSTPROCESSED", note)

    def test_the_warning_names_the_exact_command_to_re_read_with(self):
        note = savedregs.pin_warning("game/anim/anim", "InitAnim", False, PINS)
        self.assertIn(
            "python tools/gdl/savedregs.py game/anim/anim InitAnim --raw",
            note)

    def test_the_warning_quotes_the_census_that_justifies_it(self):
        note = savedregs.pin_warning("game/anim/anim", "InitAnim", False, PINS)
        self.assertIn("156", note)
        self.assertIn("132", note)
        self.assertIn("84", note)

    def test_raw_itself_is_never_warned(self):
        """--raw IS the correct read; warning there would tell a worker to
        do what they just did."""
        self.assertIsNone(
            savedregs.pin_warning("game/anim/anim", "InitAnim", True, PINS))

    def test_an_unpinned_function_is_never_warned(self):
        """The negative side: default and --raw decode the same object."""
        self.assertIsNone(savedregs.pin_warning(
            "game/world/camera", "camera_mode_dest", False, PINS))

    def test_no_pins_known_means_no_warning_not_a_crash(self):
        self.assertIsNone(
            savedregs.pin_warning("game/anim/anim", "InitAnim", False, set()))


class PinLoading(unittest.TestCase):
    def test_the_live_config_yields_the_pin_set(self):
        pins = savedregs.webfrank_pins()
        self.assertIn(("game/anim/anim", "InitAnim"), pins)
        self.assertNotIn(("game/world/camera", "camera_mode_dest"), pins)

    def test_a_checkout_with_no_config_fails_soft(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(savedregs.webfrank_pins(root=tmp), set())

    def test_a_corrupt_config_fails_soft(self):
        with tempfile.TemporaryDirectory() as tmp:
            cfg = Path(tmp) / "config" / "GUNE5D"
            cfg.mkdir(parents=True)
            (cfg / "webfrank.json").write_text("{not json",
                                               encoding="utf-8")
            self.assertEqual(savedregs.webfrank_pins(root=tmp), set())

    def test_a_rule_with_no_function_is_skipped(self):
        with tempfile.TemporaryDirectory() as tmp:
            cfg = Path(tmp) / "config" / "GUNE5D"
            cfg.mkdir(parents=True)
            (cfg / "webfrank.json").write_text(json.dumps(
                {"units": {"game/x/y": [{"function": "f"}, {"note": "no fn"}],
                           "game/z/w": "not a list"}}), encoding="utf-8")
            self.assertEqual(savedregs.webfrank_pins(root=tmp),
                             {("game/x/y", "f")})


class FormatTable(unittest.TestCase):
    ROWS = [(0x1c, "addi r31,r5,0")]

    def test_the_table_stays_PURE_and_takes_its_pins_as_an_argument(self):
        """format_table is on the IMPORTABLE CORE line, so it must not read
        webfrank.json itself: main() reads it once, and a sweep passes one
        set for every function instead of re-reading per call."""
        text = savedregs.format_table("game/anim/anim", "InitAnim",
                                      self.ROWS, self.ROWS)
        self.assertNotIn("WEBFRANK-PINNED", text)
        text = savedregs.format_table("game/anim/anim", "InitAnim",
                                      self.ROWS, self.ROWS, pins=PINS)
        self.assertIn("WEBFRANK-PINNED", text)

    def test_the_warning_sits_above_the_scope_lines(self):
        """It decides whether the verdict describes the compiler at all,
        which outranks what the table did not compare."""
        text = savedregs.format_table("game/anim/anim", "InitAnim",
                                      self.ROWS, self.ROWS, pins=PINS)
        self.assertLess(text.index("WEBFRANK-PINNED"),
                        text.index("SCOPE OF THIS TABLE"))

    def test_raw_suppresses_it_through_format_table_too(self):
        text = savedregs.format_table("game/anim/anim", "InitAnim",
                                      self.ROWS, self.ROWS, pins=PINS,
                                      raw=True)
        self.assertNotIn("WEBFRANK-PINNED", text)


class Wiring(unittest.TestCase):
    SRC = (REPO / "tools" / "gdl" / "savedregs.py").read_text(encoding="utf-8")

    def test_main_passes_both_the_raw_flag_and_the_pin_set(self):
        self.assertIn("raw=raw,", self.SRC)
        self.assertIn("pins=webfrank_pins()", self.SRC)

    def test_the_docstring_states_the_pinned_rule(self):
        self.assertIn("ON A WEBFRANK-PINNED FUNCTION, READ `--raw`", self.SRC)

    def test_webfrank_pins_is_NOT_advertised_as_importable_core(self):
        """It reads a config file; the line promises no I/O."""
        line = next(x for x in self.SRC.splitlines()
                    if x.startswith("IMPORTABLE CORE:"))
        block = self.SRC[self.SRC.index(line):
                         self.SRC.index("no side effects")]
        self.assertIn("pin_warning", block)
        self.assertNotIn("webfrank_pins", block)


if __name__ == "__main__":
    unittest.main()
