#!/usr/bin/env python3
"""T21 run-51 item 2: a TU measurement that did not happen must not read as
a measurement that every function is gone.

THE OBSERVATION (claim.law.MP_probe-raw-drops-the-raw-word-count-for-every-
address-suffixed-name-and-its-tu-gate-false-alarms-on-a-pinned-tu
.20260903.v1, second half): a `--raw` probe carrying a file-scope change on
game/movie/movieplayer printed

    TU-SCOPE REGRESSED  51 BYTE-EXACT sibling(s) lost
      PlayVQMovie  function vanished from object
      __dt__5CodecFv  function vanished from object
      ...

while the object it had just built held all 52 functions.

REPRODUCED at 5010e1b74, at the layer the fabrication lives in (the field
trigger is a WEBFRANK abort under `--raw`; any failing build reaches the same
code, and this is the deterministic form):

    run_fndiff --classify stdout length: 1058
    first 300 chars: 'NINJA FAILED rebuilding
      build\\GUNE5D\\src\\game\\movie\\movieplayer.o:\\n[1/2] MWCC ...'
    snapshot entries: 0
    verdict rows: 52  REGRESSION: 52  'vanished': 52
        ('DTextInitColorRamp', 'REGRESSION', 'function vanished from object')
        ('PlayVQMovie', 'REGRESSION', 'function vanished from object')

MECHANISM: `defake_gate.run_fndiff` refused only when the child's output
contained `missing:`. fndiff's OTHER failure prints `NINJA FAILED rebuilding
<obj>:` on stdout and returns 1, so the build log flowed on as if it were a
roster; `parse_classify` finds no rows in a build log, `snapshot` returns
`{}`, and `compare(baseline, {})` calls every baseline entry vanished. Under
`--raw` this is the ORDINARY case, not an edge one: `--raw` exists because a
drifted pin blocks the postprocessed build, and the TU cross-check the same
invocation runs needs exactly that build.

TWO-SIDED CALIBRATION of the discriminant, over all 257 units x 3 flags =
771 invocations on a healthy tree (T21_scratch/t21_runfndiff_calib.py):

  POSITIVES  1 measured failure fabricates 52 destroyed functions; every
             `--classify`/`--count`/`--clean` invocation whose child exits
             non-zero has the same shape
  NEGATIVES  0 of 771 healthy invocations exit non-zero, so the refusal
             takes nothing away

AND THE CALIBRATION KILLED THE FIRST DESIGN. Refusing on an EMPTY SNAPSHOT
is the obvious fix and it is wrong: 100 of those 771 healthy invocations
return exit 0 with EMPTY stdout — every fully matched unit emits no `--count`
DIFF line at all (MSL/abort_exit, MSL/bsearch, MSL/ctype, ...). Emptiness is
a legitimate measurement; the RETURN CODE is the discriminant.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "gdl"))

import defake_gate  # noqa: E402
import probe        # noqa: E402

NINJA_FAILURE = (
    "NINJA FAILED rebuilding build\\GUNE5D\\src\\game\\movie\\movieplayer.o:\n"
    "[1/2] MWCC build\\GUNE5D\\src\\game\\movie\\.postprocess\\body\\"
    "movieplayer.o\n"
    "FAILED: [code=2] build/GUNE5D/src/game/movie/.postprocess/body/"
    "movieplayer.o \n"
    "#   Error: undefined identifier 'T21ForceBuildFailure'\n"
    "ninja: build stopped: subcommand failed.\n")

BASELINE = {
    "PlayVQMovie": {"status": "EXACT", "real": 0},
    "__dt__5CodecFv": {"status": "EXACT", "real": 0},
    "DTextInitColorRamp": {"status": "EXACT", "real": 0},
}


class _FakeCompleted:
    def __init__(self, returncode, stdout, stderr=""):
        self.returncode, self.stdout, self.stderr = returncode, stdout, stderr


class RunFndiffRefusal(unittest.TestCase):
    """The return code decides, not the text and not the emptiness."""

    def _patch(self, completed):
        original = defake_gate.subprocess.run
        defake_gate.subprocess.run = lambda *a, **k: completed
        self.addCleanup(setattr, defake_gate.subprocess, "run", original)

    def test_a_ninja_failure_refuses_instead_of_returning_the_build_log(self):
        self._patch(_FakeCompleted(1, NINJA_FAILURE))
        with self.assertRaises(SystemExit) as caught:
            defake_gate.run_fndiff("game/movie/movieplayer", "--classify")
        message = str(caught.exception)
        self.assertIn("did not measure game/movie/movieplayer", message)
        self.assertIn("exit 1", message)
        # the child's own output is carried, not swallowed
        self.assertIn("NINJA FAILED", message)

    def test_the_missing_object_failure_still_refuses(self):
        # The one case the old guard caught; it must keep refusing.
        self._patch(_FakeCompleted(1, "missing: build/GUNE5D/obj/x.o\n"))
        with self.assertRaises(SystemExit):
            defake_gate.run_fndiff("game/x/y", "--classify")

    def test_an_empty_roster_from_a_SUCCESSFUL_run_is_a_measurement(self):
        # The negative half that killed the empty-snapshot design: 100 of 771
        # healthy invocations return exit 0 with empty stdout, because a fully
        # matched unit emits no --count DIFF line.
        self._patch(_FakeCompleted(0, ""))
        self.assertEqual(
            defake_gate.run_fndiff("MSL/bsearch", "--count"), "")

    def test_a_successful_run_returns_its_stdout_unchanged(self):
        self._patch(_FakeCompleted(0, "EXACT               PlayVQMovie\n"))
        self.assertEqual(defake_gate.run_fndiff("game/movie/movieplayer",
                                                "--classify"),
                         "EXACT               PlayVQMovie\n")


class FabricationShape(unittest.TestCase):
    """What the old path produced, pinned so it cannot come back."""

    def test_an_empty_snapshot_still_reads_as_a_total_wipeout(self):
        # `compare` is not the defect and is not changed: fed nothing, it
        # correctly reports everything missing. That is precisely why nothing
        # may hand it an unmeasured unit.
        verdicts = defake_gate.compare(BASELINE, {})
        vanished = [row for row in verdicts
                    if row[1] == "REGRESSION"
                    and "vanished" in row[2]]
        self.assertEqual(len(vanished), len(BASELINE))

    def test_a_build_log_parses_to_no_functions_at_all(self):
        self.assertEqual(defake_gate.parse_classify(NINJA_FAILURE), {})


class ProbeReportsUngated(unittest.TestCase):
    """probe turns the refusal into TU-SCOPE UNGATED, not a sibling list."""

    def test_a_systemexit_from_the_cross_check_is_caught(self):
        module = probe._defake_gate_module()
        self.assertIsNotNone(module)
        original = module.measure_unit

        def boom(*_a, **_k):
            raise SystemExit("fndiff --classify did not measure X (exit 1)")

        module.measure_unit = boom
        self.addCleanup(setattr, module, "measure_unit", original)
        # gate_path/load_baseline may or may not find a baseline in this
        # tree; either way the answer must be (None, why) and never a raise.
        verdicts, note = probe.tu_sibling_regressions("game/movie/movieplayer")
        self.assertIsNone(verdicts)
        self.assertIsInstance(note, str)
        self.assertTrue(note)

    def test_no_verdicts_renders_as_UNGATED_and_banks_nothing(self):
        text = probe.tu_scope_refusal(
            [("decl changed", "class Codec ...")], None, None,
            "the TU cross-check could not measure the unit — fndiff"
            " --classify did not measure game/movie/movieplayer (exit 1)",
            "game/movie/movieplayer", "fn_800D967C")
        self.assertIn("TU-SCOPE UNGATED", text)
        self.assertIn("NOTHING banked", text)
        self.assertNotIn("vanished", text)
        self.assertNotIn("BYTE-EXACT sibling(s) lost", text)


if __name__ == "__main__":
    unittest.main()
