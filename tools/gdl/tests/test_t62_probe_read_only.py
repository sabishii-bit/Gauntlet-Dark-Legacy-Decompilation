"""`probe --no-build` reads; it never banks (run 62 item 3c).

THE OBSERVATION (DF lane). `--no-build` was in probe's KNOWN_FLAGS and
changed nothing: probe built the object anyway and, on a function with no
anchor yet, banked a BEST plus a session baseline plus a source snapshot
from whatever it scored. A lane that ran the flag over an EXPERIMENTAL
variant -- believing it was reading -- anchored the session to the
experiment, and the next probe, on the restored HEAD state, was scored
against the experiment's number and announced IMPROVED. A revert reported
as a win.

REPRODUCED at 6038ec628 on game/enemy/enemy::do_ai (an up-to-date tree, so
the build really was a no-op and the call really was a read):

    probe.py game/enemy/enemy do_ai --reset
    probe.py game/enemy/enemy do_ai --ops --no-build
    -> BASELINE real 0 (insns T192/O192, multiset 0t)
       [session baseline banked: probe.py --revert-baseline restores THIS]
    -> build/GUNE5D/gate/probe_game_enemy_enemy_do_ai.json  (best_real 0)
       build/GUNE5D/gate/snap_game_enemy_enemy.c
       build/GUNE5D/gate/snap_game_enemy_enemy____best_do_ai.c

THE DISCRIMINANT is not "which flags look read-only" but WHICH OBJECT was
scored: with `--no-build` the object on disk need not have been built from
the working tree at all, so nothing measured from it can be anchored to the
current source. That is why the flag now implies no bank of any kind, warns
when the source is newer than the object, and skips the fuzzy gate's report
build (a full link paid to price a bank that cannot happen).

THE NEGATIVE SIDE, kept deliberately narrow: `--ops` ALONE is a scoring
probe of a state the caller just built, and it must keep banking -- that is
the project's edit loop. `--no-bank` keeps its own meaning (build, measure
the tree, bank nothing) and `--stateless` keeps its own (no state, no
verdict at all).
"""
import json
import os
import subprocess
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))

import probe  # noqa: E402


class BankingGate(unittest.TestCase):
    def test_no_build_disables_banking_and_no_ops_alone_does_not(self):
        self.assertTrue(probe.banking_disabled(["--no-build"]))
        self.assertTrue(probe.banking_disabled(["--no-bank"]))
        self.assertTrue(probe.banking_disabled(["--ops", "--no-build"]))
        self.assertFalse(probe.banking_disabled(["--ops"]))
        self.assertFalse(probe.banking_disabled([]))
        self.assertFalse(probe.banking_disabled(["--raw", "--fuzzy"]))

    def test_a_baseline_under_the_gate_leaves_no_anchor(self):
        verdict, state = probe.classify({}, 44, "T99/O99", 0, no_bank=True)
        self.assertTrue(verdict.startswith("BASELINE"), verdict)
        self.assertIsNone(state.get("best_real"))

    def test_the_same_baseline_without_the_gate_still_anchors(self):
        verdict, state = probe.classify({}, 44, "T99/O99", 0)
        self.assertTrue(verdict.startswith("BASELINE"), verdict)
        self.assertEqual(state.get("best_real"), 44)

    def test_a_readout_does_not_bank_a_baseline_under_the_gate(self):
        self.assertTrue(probe.readout_banks_baseline(False, True, False))
        self.assertFalse(probe.readout_banks_baseline(False, True, True))

    def test_the_flag_is_still_a_known_flag(self):
        # It must stay accepted -- the cure is to give it meaning, not to
        # start refusing a spelling lanes already use.
        self.assertIn("--no-build", probe.KNOWN_FLAGS)


UNIT, FUNCTION = "game/enemy/enemy", "do_ai"
GATE = ROOT / "build/GUNE5D/gate"
LIVE = (ROOT / "build/GUNE5D/obj/game/enemy/enemy.o").is_file()


@unittest.skipUnless(LIVE, "needs the split target objects and a built tree")
class LiveReadOnlyProbe(unittest.TestCase):
    """End to end: the reported invocation, on the reported function."""

    def run_probe(self, *flags):
        return subprocess.run(
            [sys.executable, "tools/gdl/probe.py", UNIT, FUNCTION, *flags],
            cwd=str(ROOT), capture_output=True, text=True)

    def setUp(self):
        self.state = GATE / ("probe_%s_%s.json"
                             % (UNIT.replace("/", "_"), FUNCTION))
        self.saved = (self.state.read_bytes() if self.state.exists()
                      else None)
        self.addCleanup(self.restore)
        self.run_probe("--reset")

    def restore(self):
        if self.saved is None:
            if self.state.exists():
                os.remove(self.state)
        else:
            self.state.write_bytes(self.saved)

    def test_the_reported_call_banks_no_anchor_and_says_so(self):
        done = self.run_probe("--ops", "--no-build")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("NOTHING is banked", done.stdout)
        self.assertNotIn("session baseline banked", done.stdout)
        self.assertNotIn("fuzzy gate:", done.stdout)
        state = json.loads(self.state.read_text(encoding="utf-8"))
        self.assertNotIn("best_real", state)
        self.assertIn("last_real", state)

    def test_it_still_prints_the_score_and_the_ops_view(self):
        done = self.run_probe("--ops", "--no-build")
        self.assertIn("real ", done.stdout)
        self.assertIn("==== %s:" % FUNCTION, done.stdout)

    def test_an_ordinary_probe_still_banks_the_anchor(self):
        # The negative side, measured on the same function: without the
        # flag the anchor is exactly what the edit loop depends on.
        done = self.run_probe("--no-fuzzy-gate")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        state = json.loads(self.state.read_text(encoding="utf-8"))
        self.assertIn("best_real", state)


if __name__ == "__main__":
    unittest.main()
