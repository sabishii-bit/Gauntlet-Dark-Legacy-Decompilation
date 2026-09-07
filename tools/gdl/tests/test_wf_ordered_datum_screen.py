"""Pin the ORDERED-datum screen's register-slot discriminator.

Every case below is a REFUSAL test in the sense AGENTS.md means: each fails
if its specific guard is removed, and each is a shape measured in the live
corpus rather than an invented one.  The two load-bearing ones are the pair
that decides the whole screen:

  * gamemain::fn_80057024 -- target `lfs f1,255.0f` then `lfs f4,0.0f`
    against ours `lfs f4,0.0f` then `lfs f1,255.0f`.  Each datum reaches
    the SAME register; only the emission order moved.  Drop the crossed-
    register REORDER test and this reads DEFECT, which is the false
    positive that would have shipped 44 functions as bugs.
  * enemy::move_logic00 -- both streams load f30, the target with pi and
    ours with 2pi.  Drop the same-slot DATUM-SWAP test and this reads
    BENIGN, which is fndiff's own documented blind spot re-opened.
"""
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import fndiff                                               # noqa: E402
import wf_ordered_datum_screen as screen                    # noqa: E402


class OrderedDatumScreen(unittest.TestCase):

    def test_identical_order_is_benign(self):
        result = screen.classify(
            ["B:aa", "B:bb"], ["B:aa", "B:bb"],
            ["lfs     f1,0(0)", "lfs     f4,0(0)"],
            ["lfs     f1,0(0)", "lfs     f4,0(0)"])
        self.assertEqual(result["verdict"], "BENIGN")
        self.assertEqual(result["ordered_mismatches"], 0)

    def test_crossed_registers_are_a_reorder_not_a_defect(self):
        # gamemain::fn_80057024 +0x... : f1 holds 255.0f and f4 holds 0.0f
        # in BOTH streams; only the emission order moved.
        result = screen.classify(
            ["B:437f0000", "B:00000000"], ["B:00000000", "B:437f0000"],
            ["lfs     f1,0(0)", "lfs     f4,0(0)"],
            ["lfs     f4,0(0)", "lfs     f1,0(0)"])
        self.assertEqual(result["verdict"], "BENIGN")
        self.assertEqual(result["mismatch_kinds"].get("REORDER"), 2)
        self.assertNotIn("DATUM-SWAP", result["mismatch_kinds"])

    def test_same_register_slot_is_a_datum_swap(self):
        # enemy::move_logic00: both streams load f30, target pi, ours 2pi.
        result = screen.classify(
            ["B:401921fb54524550", "B:400921fb54524550"],
            ["B:400921fb54524550", "B:401921fb54524550"],
            ["lfd     f30,0(0)", "lfd     f29,0(0)"],
            ["lfd     f30,0(0)", "lfd     f29,0(0)"])
        self.assertEqual(result["verdict"], "DEFECT")
        self.assertEqual(result["mismatch_kinds"].get("DATUM-SWAP"), 2)

    def test_store_pair_with_swapped_globals_is_a_datum_swap(self):
        # controls::ReadControls: `stw r3,A` / `stw r5,B` against
        # `stw r3,B` / `stw r5,A` -- the same value register writes the
        # other global.
        result = screen.classify(
            ["A:0x803445E4", "A:0x80344604"],
            ["A:0x80344604", "A:0x803445E4"],
            ["stw     r3,0(0)", "stw     r5,0(0)"],
            ["stw     r3,0(0)", "stw     r5,0(0)"])
        self.assertEqual(result["verdict"], "DEFECT")

    def test_conversion_magic_is_excluded(self):
        result = screen.classify(
            ["B:4330000080000000", "B:42700000"],
            ["B:42700000", "B:4330000080000000"],
            ["lfd     f2,0(0)", "lfs     f3,0(0)"],
            ["lfs     f3,0(0)", "lfd     f2,0(0)"])
        self.assertEqual(result["verdict"], "BENIGN")
        self.assertEqual(
            result["mismatch_kinds"].get("CONVERSION-MAGIC"), 2)

    def test_a_value_delta_is_never_reported_benign(self):
        # The multisets differ, so a wrong CONSTANT exists somewhere; this
        # screen answers the ORDER question only and must not clear it.
        result = screen.classify(
            ["B:aa", "B:bb"], ["B:aa", "B:cc"],
            ["lfs     f1,0(0)", "lfs     f4,0(0)"],
            ["lfs     f1,0(0)", "lfs     f4,0(0)"])
        self.assertEqual(result["verdict"], "UNDECIDABLE")
        self.assertIn("MULTISETS differ", result["why"])

    def test_unequal_relocation_counts_are_undecidable(self):
        result = screen.classify(
            ["B:aa", "B:bb"], ["B:aa"],
            ["lfs     f1,0(0)", "lfs     f4,0(0)"], ["lfs     f1,0(0)"])
        self.assertEqual(result["verdict"], "UNDECIDABLE")
        self.assertIn("relocation counts differ", result["why"])

    def test_split_form_mirror_is_undecidable_not_a_defect(self):
        # An @ha/@lo materialisation interleave mirrors under two DIFFERENT
        # mnemonics; it is unpaired evidence, never a datum verdict.
        result = screen.classify(
            ["B:aa", "B:bb"], ["B:bb", "B:aa"],
            ["lis     r3,0", "addi    r4,r5,0"],
            ["addi    r9,r5,0", "lis     r7,0"])
        self.assertEqual(result["verdict"], "UNDECIDABLE")
        self.assertEqual(result["mismatch_kinds"].get("SPLIT-FORM"), 2)

    def test_register_field_reads_the_first_operand(self):
        self.assertEqual(screen.register_field("stw     r3,0(0)"), "r3")
        self.assertEqual(screen.register_field("addi    r27,r12,0"), "r27")
        self.assertEqual(screen.register_field("lfd     f30,0(0)"), "f30")
        self.assertEqual(screen.register_field(""), "")

    def test_ordered_symbols_pairs_each_relocation_with_its_instruction(self):
        lines = ["lis     r3,0", "    R_PPC_ADDR16_HA  lbl_1",
                 "nop", "lfs     f1,0(0)", "    R_PPC_EMB_SDA21  lbl_2"]
        symbols, instructions = screen.ordered_symbols(
            lines, with_offsets=True)
        self.assertEqual(symbols, ["lbl_1", "lbl_2"])
        self.assertEqual(instructions, ["lis     r3,0", "lfs     f1,0(0)"])


class NameResolutionAndRefusal(unittest.TestCase):
    """Run-58 item 3: the screen skipped PINS and exited successful.

    `fndiff.parse` strips dtk's `_80XXXXXX` disambiguation suffix from a
    file-local symbol when the stripped base is unique in the object, while
    config/GUNE5D/webfrank.json spells the same function WITH the suffix.
    The lookup was an exact-name `in`, so those pins reported

        UNDECIDABLE  game/enemy/enemy::gendir_8004FBC8
            function absent                                 EXIT 0

    TWO-SIDED CALIBRATION over the whole live pinned population at 5ef108eb0
    (build/T2_item3_calibrate.py; 161 rules across the units with both
    objects on disk):

      exact-name lookup   156 screened, 5 FUNCTION ABSENT at exit 0
      resolved lookup     161 screened -- 151 BENIGN, 10 UNDECIDABLE,
                          0 UNRESOLVED

    The 5 recovered are all file-local statics, and every one is BENIGN, so
    the skip was hiding nothing this time -- which is the point: nobody
    could know that while the tool answered "absent" and exited 0.

      game/game/combat::DiffRate_8002951C            -> DiffRate
      game/game/combat::adjust_radius_8002B2D4       -> adjust_radius
      game/game/combat::cam_orient_to_80029E8C       -> cam_orient_to
      game/enemy/enemy::gendir_8004FBC8              -> gendir
      game/ui/btext::FindStringMessageListSub_8001FC4C
                                              -> FindStringMessageListSub
    """

    def test_an_exact_name_resolves_to_itself(self):
        self.assertEqual(screen.resolve_function({"gendir": []}, "gendir"),
                         "gendir")

    def test_a_suffixed_query_finds_a_stripped_table_entry(self):
        # THE LIVE CASE: the rule names gendir_8004FBC8, objdump's table
        # (through fndiff.parse) says gendir.
        self.assertEqual(
            screen.resolve_function({"gendir": [], "do_ai": []},
                                    "gendir_8004FBC8"),
            "gendir")

    def test_a_stripped_query_finds_a_suffixed_table_entry(self):
        # The other direction: fndiff.parse KEEPS the suffix when two
        # file-locals strip to one base, so a plain query must still find a
        # unique suffixed key.
        self.assertEqual(
            screen.resolve_function({"gendir_8004FBC8": [], "do_ai": []},
                                    "gendir"),
            "gendir_8004FBC8")

    def test_an_ambiguous_base_resolves_to_nothing(self):
        # THE INVALID INPUT. dtk keeps both suffixes for dtor_800DB21C and
        # dtor_800DBB94; answering with one of them would hand a caller the
        # WRONG function's rows, which is worse than answering nothing.
        table = {"dtor_800DB21C": [], "dtor_800DBB94": []}
        self.assertIsNone(screen.resolve_function(table, "dtor"))
        self.assertIsNone(screen.resolve_function(table, "dtor_800DC000"))

    def test_a_genuinely_absent_name_resolves_to_nothing(self):
        self.assertIsNone(
            screen.resolve_function({"gendir": []}, "no_such_function"))

    def test_a_dtk_unnamed_function_is_never_stripped(self):
        # `fn_800516F8` ends in `_80` plus six hex digits. An unguarded
        # strip maps EVERY dtk-unnamed function onto the base `fn`, so a
        # query for one could resolve to a different one whenever the exact
        # name is absent. Measured in regnorm while calibrating run-58
        # item 2: the pin roster read 143 instead of 159 under the same
        # unguarded strip. `fndiff.parse` carries this guard too.
        table = {"fn_800516F8": [], "closest_enemy": []}
        self.assertIsNone(screen.resolve_function(table, "fn_80051C78"))
        self.assertEqual(screen.resolve_function(table, "fn_800516F8"),
                         "fn_800516F8")

    def test_a_partial_name_is_not_a_prefix_match(self):
        # `gen` must not resolve to `gendir_8004FBC8`: only dtk's exact
        # `_80XXXXXX` suffix is erasable.
        self.assertIsNone(
            screen.resolve_function({"gendir_8004FBC8": []}, "gen"))
        self.assertIsNone(
            screen.resolve_function({"gendir_deadbeef": []}, "gendir"))


class LiveRefusal(unittest.TestCase):
    """The CLI half: an unscreened pin must be LOUD and NON-ZERO."""

    @classmethod
    def setUpClass(cls):
        cls.objects = os.path.join(ROOT, "build", "GUNE5D", "obj",
                                   "game", "enemy", "enemy.o")
        if not os.path.exists(cls.objects):
            raise unittest.SkipTest("checkout is not built")

    def run_cli(self, function):
        import subprocess
        return subprocess.run(
            [sys.executable,
             os.path.join("tools", "gdl", "composed_census",
                          "wf_ordered_datum_screen.py"),
             "--unit", "game/enemy/enemy", "--function", function],
            cwd=ROOT, capture_output=True, text=True)

    def test_the_reported_pin_is_now_screened_and_exits_zero(self):
        proc = self.run_cli("gendir_8004FBC8")
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertIn("BENIGN", proc.stdout)
        self.assertIn("resolved to target 'gendir'", proc.stdout)

    def test_an_absent_function_is_unresolved_and_exits_one(self):
        proc = self.run_cli("no_such_function_at_all")
        self.assertEqual(proc.returncode, 1, proc.stdout + proc.stderr)
        self.assertIn("UNRESOLVED", proc.stdout)
        self.assertIn("NOTHING WAS SCREENED", proc.stdout)
        self.assertNotIn("BENIGN", proc.stdout)

    def test_every_shipped_pin_resolves(self):
        """The calibration in the class docstring, asserted live.

        A pin this screen cannot locate is a pin nothing screens, so the
        population number is the gate: 0 UNRESOLVED, not "most of them".
        """
        import json
        config = os.path.join(ROOT, "config", "GUNE5D", "webfrank.json")
        with open(config, encoding="utf-8") as handle:
            data = json.load(handle)
        unresolved = []
        for unit, rules in (data.get("units") or {}).items():
            paths = [os.path.join(ROOT, "build", "GUNE5D", kind,
                                  unit + ".o") for kind in ("obj", "src")]
            if not all(os.path.exists(path) for path in paths):
                continue
            tables = [fndiff.parse(path) for path in paths]
            for rule in rules:
                name = rule.get("function")
                if not name:
                    continue
                if any(screen.resolve_function(table, name) is None
                       for table in tables):
                    unresolved.append(f"{unit}::{name}")
        self.assertEqual(unresolved, [])


if __name__ == "__main__":
    unittest.main()
