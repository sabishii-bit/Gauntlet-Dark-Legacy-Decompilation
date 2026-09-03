"""Pin the two run-50 promotions: the FQ flip screen and the CV datum sweep.

Both tools lived only in retired lane scratch directories and were
re-derived from their records.  Each guard below is TWO-SIDED: the positive
case is the shape the tool exists to catch, the negative case is the shape
that must NOT fire, and each test fails if its own guard is removed.

  fq_flip_candidate_screen
    positive  a NonMatching unit with matched_code == total_code and every
              function matched is READY (atree.c's live shape).
    negative  a Matching unit with the same numbers is not a candidate, and
              a NonMatching unit one byte short is NEAR, not READY.
    refusal   the join is the whole trap -- report.json names carry a
              `main/` prefix and no extension, so a naive join matches ZERO
              rows and prints an empty candidate list that reads exactly
              like "nothing to flip".  A join rate under --min-join must
              exit 2 rather than report a false all-clear.

  cv_scalar_datum_sweep
    positive  a scalar the target references and we do not survives.
    negative  (a) a prefix pair (dtk's whole-.rodata-run symbol against our
              per-literal `@N`) cancels; (b) a COUNT difference is a CSE
              fact and never a row; (c) a datum longer than the cap is not
              admitted AT ALL -- passing the cap to `datum_key` instead
              truncates strings into 8-byte "scalars", which is the
              section-alias false-positive class the scalar cap exists to
              exclude (measured: 25 findings truncating vs 12 filtering, at
              run-50 HEAD).
"""
import json
import os
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import cv_scalar_datum_sweep as sweep                          # noqa: E402
import fq_flip_candidate_screen as flip                        # noqa: E402


def _unit(name, code, total_code, fns, total_fns, auto=False):
    percent = 100.0 * code / total_code if total_code else 0.0
    return {"name": name,
            "measures": {"matched_code": str(code),
                         "total_code": str(total_code),
                         "matched_functions": fns,
                         "total_functions": total_fns,
                         "matched_code_percent": percent,
                         "fuzzy_match_percent": percent,
                         "matched_data": "0", "total_data": "0"},
            "metadata": {"auto_generated": auto}}


class FlipCandidateScreen(unittest.TestCase):

    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix="t20_flip_")
        self.report = os.path.join(self.dir, "report.json")
        self.configure = os.path.join(self.dir, "configure.py")

    def _write(self, units, configure_text):
        with open(self.report, "w", encoding="utf-8") as handle:
            json.dump({"units": units}, handle)
        with open(self.configure, "w", encoding="utf-8") as handle:
            handle.write(configure_text)

    def test_ready_near_and_matching_are_separated(self):
        self._write(
            [_unit("main/game/anim/atree", 11632, 11632, 36, 36),
             _unit("main/game/pb/dbgtext", 9900, 10000, 40, 41),
             _unit("main/game/ui/select", 5000, 5000, 20, 20),
             _unit("main/auto_07_80118100_data", 0, 0, 0, 0, auto=True)],
            'Object(NonMatching, "game/anim/atree.c"),\n'
            'Object(NonMatching, "game/pb/dbgtext.c", cflags=cflags_x),\n'
            'Object(Matching, "game/ui/select.c"),\n')
        result = flip.screen(self.report, self.configure)
        self.assertEqual(result["rows"], 3, "auto_generated unit not dropped")
        self.assertEqual(result["joined"], 3)
        self.assertEqual(result["census"]["NonMatching"], 2)
        self.assertEqual([row["unit"] for row in result["ready"]],
                         ["game/anim/atree"])
        # A Matching unit with identical numbers is finished, not a candidate.
        self.assertNotIn("game/ui/select",
                         [row["unit"] for row in result["ready"]])
        # One byte short is the NEAR band, never READY.
        self.assertEqual([row["unit"] for row in result["near"]],
                         ["game/pb/dbgtext"])

    def test_all_functions_matched_with_code_short_is_its_own_class(self):
        self._write([_unit("main/game/x/y", 900, 1000, 12, 12)],
                    'Object(NonMatching, "game/x/y.c"),\n')
        result = flip.screen(self.report, self.configure)
        self.assertEqual(result["ready"], [])
        self.assertEqual([row["unit"] for row in result["fns_done"]],
                         ["game/x/y"])

    def test_broken_join_refuses_instead_of_reporting_no_candidates(self):
        # The naive join (prefix kept, extension kept) matches nothing.  The
        # tool must exit 2, because an empty READY list is the same output
        # as a healthy image with nothing to flip.
        self._write([_unit("main/game/anim/atree", 11632, 11632, 36, 36)],
                    'Object(NonMatching, "totally/other/unit.c"),\n')
        result = flip.screen(self.report, self.configure)
        self.assertEqual(result["joined"], 0)
        self.assertEqual(result["census"]["unjoined"], 1)
        argv = sys.argv
        sys.argv = ["fq_flip_candidate_screen.py",
                    "--report", self.report, "--configure", self.configure]
        try:
            self.assertEqual(flip.main(), 2)
        finally:
            sys.argv = argv


class ScalarDatumSweep(unittest.TestCase):

    @staticmethod
    def _local(entries):
        return {name: (".rodata", len(blob), blob)
                for name, blob in entries.items()}

    @staticmethod
    def _lines(symbols):
        out = []
        for symbol in symbols:
            out.append("lfs     f1,0(r2)")
            out.append("    R_PPC_EMB_SDA21  " + symbol)
        return out

    def test_a_scalar_only_one_side_references_survives(self):
        local = self._local({"a": b"\x3f\x80\x00\x00",
                             "b": b"\x40\x00\x00\x00"})
        target = sweep.scalar_keys(self._lines(["a"]), local, 8)
        ours = sweep.scalar_keys(self._lines(["b"]), local, 8)
        only_target = {k: v for k, v in target.items() if k not in ours}
        only_ours = {k: v for k, v in ours.items() if k not in target}
        kept_t, kept_o = sweep.cancel_prefixes(only_target, only_ours)
        self.assertEqual(len(kept_t), 1)
        self.assertEqual(len(kept_o), 1)

    def test_prefix_pair_cancels(self):
        # dtk names the whole run; our compiler names the first entry only.
        only_target = {"B:3f80000040000000": b"\x3f\x80\x00\x00\x40\x00\x00"
                                             b"\x00"}
        only_ours = {"B:3f800000": b"\x3f\x80\x00\x00"}
        kept_t, kept_o = sweep.cancel_prefixes(only_target, only_ours)
        self.assertEqual(kept_t, {})
        self.assertEqual(kept_o, {})

    def test_count_difference_is_not_a_row(self):
        # camera_mode_follow loads 0.6 three times in the target and once in
        # ours: same value, same uses, a CSE fact.
        local = self._local({"a": b"\x3f\x19\x99\x9a"})
        target = sweep.scalar_keys(self._lines(["a", "a", "a"]), local, 8)
        ours = sweep.scalar_keys(self._lines(["a"]), local, 8)
        self.assertEqual(set(target), set(ours))

    def test_long_datum_is_excluded_not_truncated(self):
        # `trbo_full_new` truncated to 8 bytes reads as an f64 and would be
        # compared against `16_%sCOIN` -- two names for storage both streams
        # reach.  The cap must EXCLUDE it.
        local = self._local({"s": b"trbo_full_new\x00",
                             "t": b"16_%sCOIN\x00"})
        target = sweep.scalar_keys(self._lines(["s"]), local, 8)
        ours = sweep.scalar_keys(self._lines(["t"]), local, 8)
        self.assertEqual(target, {})
        self.assertEqual(ours, {})

    def test_cap_admits_a_datum_of_exactly_cap_bytes(self):
        local = self._local({"d": b"\x40\x00\x00\x00\x00\x00\x00\x00"})
        keys = sweep.scalar_keys(self._lines(["d"]), local, 8)
        self.assertEqual(list(keys), ["B:4000000000000000"])


if __name__ == "__main__":
    unittest.main()
