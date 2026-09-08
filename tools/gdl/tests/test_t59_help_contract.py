#!/usr/bin/env python3
"""T3 run-59 item 9: `--help` succeeds, on stdout, for EVERY tools/gdl tool.

THE OBSERVATION (EB lane). `wf_rederive_pin.py --help` and `wf_dump.py
--help` exited NON-ZERO and wrote their text to STDERR, so the capture
pattern AGENTS.md documents —

    $o = python <tool> --help; $code = $LASTEXITCODE

— reports a FAILURE for a request that succeeded, and a caller that filters
stdout for a verdict sees nothing at all.

THE CENSUS (build/t3_scratch/t3_help_census.py at 434460f28, all 276
modules under tools/gdl and tools/gdl/composed_census):

    OK (exit 0, text on stdout)   215
    NONZERO-ON-STDERR              36   SystemExit(__doc__), or a traceback
    NONZERO-ON-STDOUT              20   exit 1 or 2 with the text on stdout
    SILENT (exit 0, no output)      5   library modules with no CLI

61 offenders, and the census found more than a reporting defect. NINE of
them could not run AT ALL from their promoted location, because their repo
root was one level too shallow — a lane scratch directory's depth, never
re-rooted on promotion (AGENTS.md discipline 17):

    ch_census26, ch_closable, ch_delta, ch_harvest, ch_roster
        `HERE/../tools/gdl` = tools/gdl/tools/gdl -> ModuleNotFoundError
    ch_shipped        ROOT = HERE/.. -> FileNotFoundError on webfrank.json
    build_rule_pw, rule_derive_pw
        `parents[1]` = tools/gdl -> ModuleNotFoundError: No module named
        'tools'
    ha_close          RETAIL_IMAGE = tools/orig/GUNE5D/sys/main.dol, which
        never exists, so `retail_image()` REFUSED on every worktree and the
        tool could not prove a rule at all

`rule_derive_pw.py` now runs end to end (it prints pbWinSetup's 10 differing
words); `ha_close.py` now finds orig/GUNE5D/sys/main.dol.

THE CURE is `cliscreen.help_only`, the help half of the run-53 screen,
adopted corpus-wide. It never REFUSES anything, so no live invocation can
change behaviour: the unknown-flag half stays per-tool because it needs each
tool's own flag vocabulary and its own negative control.

TWO-SIDED. The positive side is the sweep below: every tool exits 0 with
text on stdout. The negative side is that a help request and a bad ARGUMENT
request stay DIFFERENT events — `probe.py` with no arguments still exits 2,
`fnasm.py` with none still exits 1 — so this did not flatten every refusal
into success.

THE SECOND DEFECT the census turned up, by accident and then on purpose:
an `exit 0 with stdout` verdict is NOT proof the flag was handled. A tool
that IGNORES `--help` and runs its whole census also exits 0 and prints.
Timing the sweep serially against `git status --porcelain` (build/t3_scratch
/t3_help_does_work.py) separated them — median `--help` 0.13 s, against
19 tools taking 3 s to 23 s, and THREE that WROTE A TRACKED FILE:

    gen_xbox_structs.py    -> research/xbox_symbols/xbox_structs.tsv
    ch_sweep26.py          -> tools/gdl/composed_census/ch_sweep26.json
    cn_final.py            -> tools/gdl/composed_census/cn_found.json

which is run-53 item 2's exact shape (`build_rule.py --help` wrote a rules
JSON) still live in three more places. They were found because a `--help`
sweep dirtied this worktree. All 21 tools whose `--help` wrote a file or
cost more than 1 s now screen the flag first; the slowest `--help` in the
corpus is now under 1 s and none writes anything.

RESIDUAL, named rather than implied: 18 tools still have no explicit help
path in their source (cn_cluster_a/bc/c, wp_guard_two_sided,
wf_recolor_probe, wf_unproven_audit_screen, wf_sim, wf_dcs_screen,
eh_island_layout_probe, cv_crossjoin, cv_taxonomy, ch_zero, ch_show,
wf_schema, and the four library modules exception_metadata, raw_object,
reloc_symbols, ppc_address_fold). Each answers `--help` at exit 0 on stdout
by printing its report or its docstring, each costs under 0.3 s, and none
writes -- so each satisfies the contract this file gates, without the flag
being read. They are a backlog item, not a passing gate hiding a failure.

COST: the sweep spawns one interpreter per tool. It cost 46 s at 16 workers
BEFORE the second fix, because 19 tools were running their censuses; with
every `--help` now screened it is 6.7 s for this whole file, against 66 s
for the rest of the suite. The gate got cheaper by fixing the defect.
It is a subprocess sweep and not an in-process `runpy` one deliberately:
executing 276 arbitrary scripts inside the test interpreter would let one
tool's `os.chdir`, `sys.path` edit or module-level state decide another
test's result.
"""

import os
import subprocess
import sys
import unittest
from concurrent.futures import ThreadPoolExecutor

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(os.path.dirname(TOOLS))
FOLDERS = (TOOLS, os.path.join(TOOLS, "composed_census"))
TIMEOUT = 120


def tool_paths():
    """Every tool module, repo-relative, in a stable order."""
    out = []
    for folder in FOLDERS:
        for name in sorted(os.listdir(folder)):
            if name.endswith(".py") and name != "__init__.py":
                out.append(os.path.relpath(os.path.join(folder, name), ROOT)
                           .replace("\\", "/"))
    return out


def worktree_state():
    """`git status --porcelain` as a set, or None when git is unavailable.

    The whole-corpus side effect gate: a `--help` sweep must leave the
    worktree exactly as it found it. Three tools failed this before run-59
    item 9, and they were found because the census dirtied a worktree.
    """
    try:
        done = subprocess.run(["git", "status", "--porcelain"], cwd=ROOT,
                              capture_output=True, text=True,
                              errors="replace", timeout=TIMEOUT)
    except (OSError, subprocess.TimeoutExpired):
        return None
    if done.returncode != 0:
        return None
    return frozenset(done.stdout.splitlines())


def run_help(relative):
    try:
        done = subprocess.run([sys.executable, relative, "--help"], cwd=ROOT,
                              capture_output=True, text=True,
                              errors="replace", timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
        return relative, None, "", "TIMEOUT"
    return relative, done.returncode, done.stdout.strip(), done.stderr.strip()


class HelpContract(unittest.TestCase):
    """One process per tool, in parallel; the whole corpus, every run."""

    @classmethod
    def setUpClass(cls):
        cls.before = worktree_state()
        with ThreadPoolExecutor(max_workers=16) as pool:
            cls.results = list(pool.map(run_help, tool_paths()))
        cls.after = worktree_state()

    def test_the_corpus_is_not_empty(self):
        """A sweep over nothing must not read as a clean sheet."""
        self.assertGreater(len(self.results), 200, len(self.results))

    def test_every_tool_exits_zero_for_help(self):
        bad = [(rel, rc) for rel, rc, _out, _err in self.results if rc != 0]
        self.assertEqual(bad, [], f"{len(bad)} tool(s) fail a help request")

    def test_every_tool_answers_help_on_stdout(self):
        silent = [rel for rel, _rc, out, _err in self.results if not out]
        self.assertEqual(
            silent, [],
            f"{len(silent)} tool(s) answer --help with nothing on stdout")

    def test_no_tool_reports_help_through_a_traceback(self):
        crashed = [rel for rel, _rc, _out, err in self.results
                   if "Traceback (most recent call last)" in err]
        self.assertEqual(crashed, [])

    def test_the_sweep_leaves_the_worktree_untouched(self):
        """`--help` must not WRITE -- run-53 item 2's shape, corpus-wide."""
        if self.before is None or self.after is None:
            self.skipTest("git status unavailable")
        self.assertEqual(
            sorted(self.after - self.before), [],
            "a --help sweep modified the worktree")

    def test_the_two_tools_the_item_names(self):
        named = {rel: (rc, out) for rel, rc, out, _err in self.results
                 if rel.endswith(("wf_rederive_pin.py", "wf_dump.py"))}
        self.assertEqual(len(named), 2, sorted(named))
        for rel, (rc, out) in named.items():
            self.assertEqual(rc, 0, rel)
            self.assertTrue(out, rel)


class HelpIsNotArgumentValidation(unittest.TestCase):
    """THE NEGATIVE SIDE: a missing-argument refusal is still a refusal.

    Widening `--help` to exit 0 must not turn every usage error into a
    success. These three keep the statuses they had before run-59 item 9.
    """

    def run_tool(self, relative, *argv):
        return subprocess.run([sys.executable, relative, *argv], cwd=ROOT,
                              capture_output=True, text=True,
                              errors="replace", timeout=TIMEOUT)

    def test_probe_with_no_arguments_is_still_a_usage_error(self):
        self.assertEqual(self.run_tool("tools/gdl/probe.py").returncode, 2)

    def test_fnasm_with_no_arguments_is_still_a_usage_error(self):
        self.assertEqual(self.run_tool("tools/gdl/fnasm.py").returncode, 1)

    def test_wf_dump_with_too_few_arguments_is_still_a_refusal(self):
        done = self.run_tool("tools/gdl/composed_census/wf_dump.py",
                             "game/enemy/enemy")
        self.assertNotEqual(done.returncode, 0)
        self.assertIn("usage:", done.stderr)


class HelpDoesNoWork(unittest.TestCase):
    """`--help` must not build, must not measure and must not WRITE.

    Run-53 item 2 found `build_rule.py --help` writing a rules JSON on its
    way to printing help. The corpus-wide adoption is the moment to assert
    the property for the tools whose help path sits closest to a write.
    """

    def test_help_writes_no_file_for_the_tools_that_write_one(self):
        # Run-61 item 6 moved these artifacts out of the source tree into
        # build/<version>/composed_census/, so the witness path moved with
        # them; the property under test is unchanged.
        witnesses = {
            "tools/gdl/composed_census/ch_shipped.py":
                "build/GUNE5D/composed_census/ch_shipped.json",
            "tools/gdl/composed_census/ch_census26.py":
                "build/GUNE5D/composed_census/ch_census26.json",
        }
        for relative, artifact in witnesses.items():
            full = os.path.join(ROOT, artifact)
            existed = os.path.exists(full)
            done = subprocess.run([sys.executable, relative, "--help"],
                                  cwd=ROOT, capture_output=True, text=True,
                                  errors="replace", timeout=TIMEOUT)
            self.assertEqual(done.returncode, 0, relative)
            self.assertEqual(os.path.exists(full), existed,
                             f"{relative} --help wrote {artifact}")


if __name__ == "__main__":
    unittest.main()
