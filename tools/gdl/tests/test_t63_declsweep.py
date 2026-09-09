"""composed_census/declsweep.py: the gated declaration-order sweep (item 2).

WHY IT IS GATED THE WAY IT IS. Lane P2 committed
`game/game/player::load_player_model_sub` real 110 -> 92 (e62b44d5d) on a
TU-wide `fndiff --relocs` count and reverted it (6f266fdf7) once the
PER-FUNCTION line read MNEMONIC DIVERGENCE 0 -> 5 and RELOC-SYMBOL MISMATCH
0 -> 2; lane B's `items::SetItem` 516w -> 502w is the same trap. So the
acceptance rule under test is: `real` must DROP, and `mnem`, `reloc` and
`anon` must NOT RISE.

TWO-SIDED. Positive: a candidate that drops `real` with the relocation
columns flat is taken, and a climb reaches a fixpoint. Negative: the lane-P2
candidate is REJECTED and counted; an unmeasurable row is not silently
ranked; a failed build is skipped rather than scored; a dirty source file
refuses the whole run; a killed candidate still leaves the source restored;
and the parser must not offer two ASSIGNMENTS as a permutable block — the
false positive that the shipped pattern was written against, measured live
on game/mb/mb_particle::MBDrawPsys.

THE SWEEP LOOP IS DRIVEN WITH A SYNTHETIC UNIT: a temporary source file and
injected `build`/`measure_fn` callables. The compiler is not the thing under
test here, and a test that needed one could not assert on a REJECTION —
there would be no way to make a build produce a chosen relocation count.
"""
import io
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

import declsweep  # noqa: E402

TOOL = "tools/gdl/composed_census/declsweep.py"
LIVE = (ROOT / "build/GUNE5D/src/game/game/player.o").is_file()

LEADING = """\
void f(void* vslot)
{
    Player* slot = vslot;
    s32 cls;
    s32 tier;

    cls = slot->id;
    tier = cls + 1;
}
"""

NESTED = """\
void g(s32 n)
{
    s32 i;

    for (i = 0; i < n; i++) {
        Player* rec = &gPlayers[i];
        s32 state = rec->state;

        if (state) {
            int bit = i - 1;
            int mask = 1 << bit;
            use(mask);
        }
    }
}
"""

#: The live false positive the separator rule exists for: two ASSIGNMENTS
#: that lane P2's pattern parses as `rat e = ...` / `d t = ...`.
ASSIGNMENTS = """\
void h(void)
{
    if (p->flags & 1) {
        rate = 100000.0f;
        dt = 1;
        goto phaseD;
    }
}
"""


def row(real, words=0, mnem=0, reloc=0, anon=0):
    return {"real": real, "words": words, "mnem": mnem, "reloc": reloc,
            "anon": anon, "function": "f", "unit": "u", "insns": 10,
            "asymmetric": False, "reason": ""}


class DeclarationParser(unittest.TestCase):
    def test_the_leading_run_is_block_zero(self):
        found = declsweep.blocks(LEADING, "f")
        self.assertEqual(len(found), 1)
        self.assertEqual([line.strip() for line in found[0]["decls"]],
                         ["Player* slot = vslot;", "s32 cls;", "s32 tier;"])
        self.assertEqual(found[0]["depth"], 0)

    def test_a_nested_block_is_found_without_a_hand_written_entry(self):
        """Lane B kept a separate b_hand.py for exactly these."""
        found = declsweep.blocks(NESTED, "g")
        self.assertEqual([len(block["decls"]) for block in found], [2, 2])
        self.assertEqual([block["depth"] for block in found], [1, 2])
        self.assertEqual([line.strip() for line in found[1]["decls"]],
                         ["int bit = i - 1;", "int mask = 1 << bit;"])

    def test_the_leading_run_stops_at_the_first_statement(self):
        found = declsweep.blocks(LEADING, "f")
        self.assertNotIn("cls = slot->id;",
                         [line.strip() for line in found[0]["decls"]])

    def test_two_ASSIGNMENTS_are_NOT_a_permutable_block(self):
        """The negative side that matters most: permuting these would be a
        STATEMENT reorder, i.e. a semantic change presented as a sweep."""
        self.assertEqual(declsweep.blocks(ASSIGNMENTS, "h"), [])
        self.assertIsNone(declsweep.DECL_RE.match("        rate = 100000.0f;"))
        self.assertIsNone(declsweep.DECL_RE.match("        dt = 1;"))

    def test_ordinary_declarations_still_match(self):
        for line in ("    s32 x;", "    u8* q;", "    u8 *q;",
                     "    PlayerModelSlot* slot = vslot;",
                     "    u8 image[80];", "    static const f32 k = 1.0f;",
                     "    struct PSlot* next;", "    unsigned x;",
                     "    u32 r = pbRand() & 0x7FFF;",
                     "    u8 unused[8];             /* a comment */"):
            self.assertIsNotNone(declsweep.DECL_RE.match(line), line)

    def test_statements_that_look_like_declarations_do_not_match(self):
        for line in ("    return x;", "    p->e_phase = 2;", "    age += 1;",
                     "    use(mask);", "    goto phaseD;", "    a = b;",
                     "    s32 a, b;", "    if (x) {", "    } else {"):
            matched = (declsweep.DECL_RE.match(line)
                       and not declsweep.NOT_DECL.match(line))
            self.assertFalse(matched, line)

    def test_an_unclosable_body_is_None_not_a_guess(self):
        self.assertIsNone(declsweep.blocks("void f(void)\n{\n    s32 x;\n",
                                           "f"))

    def test_a_brace_inside_a_string_does_not_open_a_block(self):
        text = 'void f(void)\n{\n    char* s = "{";\n    s32 x;\n    use(x);\n}\n'
        found = declsweep.blocks(text, "f")
        self.assertEqual(len(found), 1)
        self.assertEqual(len(found[0]["decls"]), 2)


class ApplyOrder(unittest.TestCase):
    def test_a_move_rewrites_only_the_block(self):
        block = declsweep.blocks(LEADING, "f")[0]
        order = [block["decls"][1], block["decls"][0], block["decls"][2]]
        out = declsweep.apply_order(LEADING, block, order)
        self.assertIn("    s32 cls;\n    Player* slot = vslot;\n", out)
        self.assertIn("    cls = slot->id;", out)
        self.assertEqual(len(out), len(LEADING))

    def test_CRLF_stays_CRLF(self):
        """A whole-file ending rewrite would diff every line and rebuild the
        whole TU — after the sweep had already spent its budget."""
        crlf = LEADING.replace("\n", "\r\n")
        block = declsweep.blocks(crlf, "f")[0]
        self.assertEqual(block["eol"], "\r\n")
        out = declsweep.apply_order(crlf, block,
                                    list(reversed(block["decls"])))
        self.assertEqual(out.count("\r\n"), crlf.count("\r\n"))
        self.assertEqual(out.count("\n"), out.count("\r\n"))
        self.assertIn("    s32 tier;\r\n    s32 cls;\r\n", out)

    def test_LF_stays_LF(self):
        block = declsweep.blocks(LEADING, "f")[0]
        out = declsweep.apply_order(LEADING, block,
                                    list(reversed(block["decls"])))
        self.assertEqual(out.count("\r"), 0)

    def test_a_stale_offset_REFUSES_instead_of_writing(self):
        block = declsweep.blocks(LEADING, "f")[0]
        moved = "/* an inserted line */\n" + LEADING
        with self.assertRaises(SystemExit):
            declsweep.apply_order(moved, block, block["decls"])

    def test_every_variant_is_distinct_and_excludes_the_original(self):
        """A SINGLE MOVE, not every permutation: from abc a single move
        reaches bac, bca, acb and cab — cba needs two. Sweeping the full
        n! would multiply the budget for orders no single edit produces."""
        decls = ["    s32 a;", "    s32 b;", "    s32 c;"]
        orders = [tuple(order) for order, _label in declsweep.variants(decls)]
        self.assertEqual(len(orders), len(set(orders)))
        self.assertNotIn(tuple(decls), orders)
        self.assertEqual(len(orders), 4)
        self.assertNotIn(("    s32 c;", "    s32 b;", "    s32 a;"), orders)


class GateRule(unittest.TestCase):
    def test_a_flat_candidate_is_not_a_rise(self):
        self.assertEqual(declsweep.rose(row(92), row(110)), [])

    def test_the_lane_P2_candidate_is_a_rise(self):
        self.assertEqual(
            sorted(declsweep.rose(row(92, 44, mnem=5, reloc=2), row(110, 55))),
            ["mnem", "reloc"])

    def test_an_anonymous_pool_rise_counts(self):
        self.assertEqual(declsweep.rose(row(92, anon=1), row(110)), ["anon"])

    def test_an_unmeasured_column_is_flagged_not_treated_as_flat(self):
        current = row(92)
        current["reloc"] = None
        self.assertEqual(declsweep.rose(current, row(110)), ["reloc?"])


class SweepLoop(unittest.TestCase):
    """The whole loop over a SYNTHETIC unit, with injected build/measure."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.path = os.path.join(self.tmp.name, "synthetic.c")
        with io.open(self.path, "w", encoding="utf-8", newline="") as handle:
            handle.write(LEADING)
        self.original = LEADING
        self.builds = []
        declsweep.source_path = lambda unit: self.path
        declsweep.dirty_source = lambda path: ""
        self.addCleanup(self.restore)

    def restore(self):
        import importlib
        importlib.reload(declsweep)
        self.tmp.cleanup()

    def current(self):
        with io.open(self.path, "r", encoding="utf-8", newline="") as handle:
            return handle.read()

    def build(self, ok=True):
        def run():
            self.builds.append(self.current())
            return ok, ""
        return run

    def scoring(self, table, default):
        """measure_fn keyed on the first declaration currently on disk."""
        def run():
            first = [line.strip() for line in self.current().splitlines()
                     if line.startswith("    ")][0]
            return table.get(first, default)
        return run

    def test_a_clean_improvement_is_TAKEN(self):
        record = declsweep.sweep(
            "u/synthetic", "f", build=self.build(),
            measure_fn=self.scoring({"s32 cls;": row(80, 40)}, row(110, 55)),
            log=lambda *a: None)
        self.assertEqual(record["verdict"], "IMPROVED")
        self.assertEqual(record["best"]["real"], 80)
        # The winning MOVE is the one that leaves `s32 cls;` first.
        self.assertEqual(record["winner"],
                         "Player* slot = vslot; -> pos 1")
        self.assertEqual(record["candidates_rejected"], 0)

    def test_a_relocation_regressing_improvement_is_REJECTED_and_counted(self):
        record = declsweep.sweep(
            "u/synthetic", "f", build=self.build(),
            measure_fn=self.scoring(
                {"s32 cls;": row(80, 40, mnem=5, reloc=2)}, row(110, 55)),
            log=lambda *a: None)
        self.assertEqual(record["verdict"], "CAPPED")
        self.assertIsNone(record["winner"])
        self.assertEqual(record["candidates_rejected"], 2)
        self.assertEqual(record["rejected_by"], {"mnem": 2, "reloc": 2})

    def test_the_source_is_RESTORED_even_when_a_candidate_wins(self):
        declsweep.sweep(
            "u/synthetic", "f", build=self.build(),
            measure_fn=self.scoring({"s32 cls;": row(80, 40)}, row(110, 55)),
            log=lambda *a: None)
        self.assertEqual(self.current(), self.original)

    def test_the_source_is_RESTORED_when_the_measurement_raises(self):
        def explode():
            raise RuntimeError("objdump died")
        with self.assertRaises(RuntimeError):
            declsweep.sweep("u/synthetic", "f", build=self.build(),
                            measure_fn=explode, log=lambda *a: None)
        self.assertEqual(self.current(), self.original)

    def test_the_LAST_build_of_the_run_is_of_the_ORIGINAL_source(self):
        """A later reader must never be served an object built from a
        candidate: the restore happens BEFORE the final rebuild."""
        declsweep.sweep(
            "u/synthetic", "f", build=self.build(),
            measure_fn=self.scoring({"s32 cls;": row(80, 40)}, row(110, 55)),
            log=lambda *a: None)
        self.assertEqual(self.builds[-1], self.original)

    def test_a_failed_CANDIDATE_build_is_skipped_not_scored(self):
        """A candidate that does not compile has no measurement, so it must
        not be counted as one — and must not be ranked at the baseline."""
        seen = []

        def build():
            seen.append(self.current())
            return len(seen) == 1, ""      # only the baseline compiles

        record = declsweep.sweep(
            "u/synthetic", "f", build=build,
            measure_fn=self.scoring({}, row(110, 55)), log=lambda *a: None)
        self.assertEqual(record["verdict"], "CAPPED")
        self.assertEqual(record["candidates_built"], 0)
        self.assertEqual(record["candidates_rejected"], 0)

    def test_an_unmeasurable_candidate_is_skipped_not_ranked(self):
        calls = []

        def measure_fn():
            calls.append(1)
            return row(110, 55) if len(calls) == 1 else None

        record = declsweep.sweep(
            "u/synthetic", "f", build=self.build(), measure_fn=measure_fn,
            log=lambda *a: None)
        self.assertEqual(record["verdict"], "CAPPED")
        self.assertIsNone(record["winner"])
        self.assertEqual(record["candidates_rejected"], 0)

    def test_a_failed_BASELINE_build_refuses_before_touching_the_source(self):
        with self.assertRaises(SystemExit) as caught:
            declsweep.sweep("u/synthetic", "f", build=self.build(ok=False),
                            measure_fn=lambda: row(1), log=lambda *a: None)
        self.assertIn("BASELINE build failed", str(caught.exception))
        self.assertEqual(self.current(), self.original)

    def test_a_dirty_source_REFUSES_the_whole_run(self):
        declsweep.dirty_source = lambda path: " M src/game/game/player.c"
        with self.assertRaises(SystemExit) as caught:
            declsweep.sweep("u/synthetic", "f", build=self.build(),
                            measure_fn=lambda: row(1), log=lambda *a: None)
        self.assertIn("REFUSED", str(caught.exception))

    def test_an_out_of_range_block_REFUSES(self):
        with self.assertRaises(SystemExit) as caught:
            declsweep.sweep("u/synthetic", "f", block_index=9,
                            build=self.build(), measure_fn=lambda: row(1),
                            log=lambda *a: None)
        self.assertIn("--block 9 is out of range", str(caught.exception))

    def test_a_climb_stops_at_a_fixpoint_rather_than_burning_the_rounds(self):
        record = declsweep.sweep(
            "u/synthetic", "f", rounds=5, build=self.build(),
            measure_fn=self.scoring({"s32 cls;": row(80, 40)}, row(110, 55)),
            log=lambda *a: None)
        self.assertEqual(record["rounds_taken"], 2)
        self.assertEqual(record["rounds_requested"], 5)

    def test_the_budget_stops_the_sweep_instead_of_a_kill(self):
        record = declsweep.sweep(
            "u/synthetic", "f", budget=-1.0, build=self.build(),
            measure_fn=self.scoring({}, row(110, 55)), log=lambda *a: None)
        self.assertEqual(record["verdict"], "BUDGET")
        self.assertEqual(record["candidates_built"], 0)
        self.assertEqual(self.current(), self.original)


class CommandLine(unittest.TestCase):
    def run_tool(self, *args):
        return subprocess.run([sys.executable, TOOL, *args], cwd=str(ROOT),
                              capture_output=True, text=True)

    def test_help_exits_zero_on_stdout(self):
        done = self.run_tool("--help")
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("Declaration-order sweep", done.stdout)
        self.assertIn("COST", done.stdout)

    def test_an_unknown_flag_is_refused_not_swallowed(self):
        done = self.run_tool("game/game/player", "f", "--nope")
        self.assertEqual(done.returncode, 2)

    @unittest.skipUnless(LIVE, "needs a built game/game/player object")
    def test_list_blocks_reads_the_live_capped_function(self):
        done = self.run_tool("game/game/player", "load_player_model_sub",
                             "--list-blocks")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("11 declaration(s)", done.stdout)
        self.assertIn("u8* pot = (u8*) lbl_80274EA0;", done.stdout)

    @unittest.skipUnless(LIVE, "needs a built game/game/player object")
    def test_list_blocks_finds_the_nested_runs_b_hand_needed_by_hand(self):
        done = self.run_tool("game/mb/mb_particle", "MBDrawPsys",
                             "--list-blocks")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("depth 2", done.stdout)
        self.assertNotIn("rate = 100000.0f;", done.stdout)

    @unittest.skipUnless(LIVE, "needs a built game/game/player object")
    def test_an_unknown_function_REFUSES(self):
        done = self.run_tool("game/game/player", "no_such_fn",
                             "--list-blocks")
        self.assertEqual(done.returncode, 2)
        self.assertIn("cannot find or close", done.stdout)


if __name__ == "__main__":
    unittest.main()
