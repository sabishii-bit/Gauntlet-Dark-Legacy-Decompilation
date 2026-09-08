#!/usr/bin/env python3
"""Run 62 lane G: wf_recolor_probe.py must survive its own premise going stale.

THE OBSERVATION. `wf_recolor_probe.py` is a one-shot investigation script
pinned by literal byte offsets (`WINDOWS`) to game/anim/atree::fn_8001267C.
It was recorded when BOTH pinned sites emitted a zero-web copy (`mr`/`addi
rD,rS,0`) where the retail target emits a fresh `li rD,0`.

Lane G improved the +0x378 site (`off = i = 0;`), so our stream there is now
the target's own order. The recorded permutation therefore no longer applies,
`webfrank.copy_register_fields` correctly refuses on non-register bits, and
the unguarded script died with that ValueError -- taking the three
test_t59_help_contract cases with it and turning a genuine source improvement
into a red build.

A dead investigation script must report a stale premise, not raise it. Both
sides are asserted here: `--help` still answers on stdout with exit 0, and a
live run whose pinned premise no longer holds exits 0 while SAYING so.
"""

import os
import subprocess
import sys
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
CC = os.path.join(ROOT, "tools", "gdl", "composed_census")
TOOL = os.path.join(CC, "wf_recolor_probe.py")
SIM = os.path.join(CC, "wf_sim.py")


def run(*args, tool=None):
    return subprocess.run([sys.executable, tool or TOOL, *args],
                          capture_output=True, text=True, cwd=ROOT)


class RecolorProbeStalePremise(unittest.TestCase):
    def test_help_answers_on_stdout_with_exit_zero(self):
        r = run("--help")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("usage:", r.stdout)
        self.assertNotIn("Traceback", r.stdout + r.stderr)

    def test_live_run_never_raises_through_a_stale_pin(self):
        r = run()
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertNotIn("Traceback", r.stderr)
        # Either the pin still describes our stream (ACCEPT/REFUSE verdict) or
        # it has been retired by an improvement -- never an escaping exception.
        self.assertRegex(r.stdout, r"verify (ACCEPT|REFUSE)|PREMISE STALE")

    def test_sibling_simulation_script_has_the_same_guard(self):
        """wf_sim.py pins the SAME function and windows; same premise, same
        cure. Guarding only one of the pair leaves the build red."""
        self.assertEqual(run("--help", tool=SIM).returncode, 0)
        r = run(tool=SIM)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertNotIn("Traceback", r.stderr)

    def test_stale_premise_names_the_cause_not_just_the_error(self):
        r = run()
        if "PREMISE STALE" not in r.stdout:
            self.skipTest("pin still current; nothing stale to report")
        self.assertIn("Re-derive", r.stdout)


if __name__ == "__main__":
    unittest.main()
