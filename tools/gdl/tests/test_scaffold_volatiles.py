"""The `volatile` half of the scaffold re-audit, and the traps it closes.

`probe.py`'s standing reminder names "pragma/volatile scaffold row(s)"; the
campaign that landed 28 dead pragma removals covered only the pragma half.
This is the other half: 81 `volatile` locals inside a measured function across
the 57 NonMatching TUs, of which only 23 are frame-padding (`unused[8]`,
`_pad0[12]`) and 58 are `volatile` on a NAMED, USED variable -- overwhelmingly
`volatile f32 root` / `rootslot` / `tmp` / `result`, the idiom that forces a
float store-reload round trip.

WHY DEAD IS PROOF HERE AND NOT A SCREEN. The pragma path scores `real`, which
counts differing diff lines. This path compares the TU object by sha1 first, so
a DEAD verdict means MWCC emitted the byte-identical object without the
qualifier. Determinism was checked: two untouched rebuilds of bosscam.o gave
the same digest at 6547a375.

WHY REMOVING A DEAD `volatile` IS LEGITIMATE IN A DECOMP, even though
`volatile` is not a no-op in C: the target object is the specification, these
qualifiers are scaffolds added to coerce codegen, and a byte-identical object
says this compiler at these flags ignored it. The reasoning does not extend to
one that moves the object, nor to a global or a hardware address -- which is
what the brace-depth guard below exists to keep out of scope.

FOUR TRAPS THIS FILE PINS.

1. BRACE DEPTH. `enclosing_function` took the nearest definition above the
   line. A file-scope `volatile u32 g;` sitting after a function body would
   take that function's name and then be audited -- stripping `volatile` from a
   global, the one case where the keyword is least likely to be decoration.
   Measured at 6547a375 the guard rejects none of the 81 real sites, so it
   costs nothing.
2. AN INITIALIZED VOLATILE IS OUT OF SCOPE. `volatile int x = f();` is not
   matched, so the audit never rewrites a declaration whose initializer it
   would have to preserve.
3. A `DIFF` ROW WITH NO `real` TOKEN IS None, NOT 0. Folding it to 0 would let
   an unmeasured function read as exact and license a keep -- the same class of
   error as the 55 UNMEASURED regions fixed at a9c09f62, inverted.
4. THE OBJECT MUST COME BACK. The restore is verified by digest, not assumed;
   a mismatch is RESTORE-FAILED and aborts the run rather than reporting a
   verdict against a tree that no longer matches the baseline.
5. THIRTY-TWO INDIVIDUAL PROOFS ARE NOT A PROOF OF THE BATCH. Six DEAD sites
   are in ONE function (btricol::LineLineDist3D2D) and four more in another
   (camera::DiffRate_8002951C); each was measured alone, and removing all six
   together can free a frame slot that no single removal could. `apply_dead`
   re-gates each TU on the same digest and reverts the TU WHOLE if it moves.

TWO-SIDED throughout. Positive: pad and named forms match and rebuild without
the keyword, `register` survives, a body line resolves to its function, an
unchanged digest is DEAD. Negative: a post-body file-scope volatile resolves to
None, an initialized one and a commented one do not match, a moved object with
no `real` delta is MOVED-UNSCORED rather than DEAD, a failed restore is
RESTORE-FAILED rather than a verdict, and the source is restored even when the
build fails.
"""
import importlib.util
import tempfile
import unittest
from pathlib import Path

SPEC = (Path(__file__).resolve().parent.parent
        / "composed_census" / "scaffold_audit.py")


def load_module():
    spec = importlib.util.spec_from_file_location("scaffold_audit", SPEC)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def rebuild_without_volatile(mod, line):
    """What the audit writes back: the same line minus the keyword."""
    m = mod.VOLATILE.match(line)
    return None if m is None else m.group(1) + m.group(2) + m.group(3).lstrip()


class VolatileMatchTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.mod = load_module()

    # ---------- positive ----------

    def test_pad_and_named_forms_match(self):
        for line in ("    volatile u8 unused[8];",
                     "    volatile u8 _pad0[12];",
                     "    volatile f32 root;",
                     "    volatile f32 rootslot;",
                     "    volatile PBSCREEN* screen;",
                     "        volatile f32 v[3];"):
            self.assertIsNotNone(self.mod.VOLATILE.match(line), line)

    def test_rebuild_drops_only_the_keyword(self):
        self.assertEqual(rebuild_without_volatile(
            self.mod, "    volatile u8 unused[8];"), "    u8 unused[8];")
        self.assertEqual(rebuild_without_volatile(
            self.mod, "\tvolatile f32 root;  /* forced spill */"),
            "\tf32 root;  /* forced spill */")

    def test_register_survives_the_rewrite(self):
        self.assertEqual(rebuild_without_volatile(
            self.mod, "    register volatile int x;"),
            "    register int x;")

    def test_brace_depth_counts_net_braces_ignoring_line_comments(self):
        lines = ["void f(void)", "{", "    if (a) { b(); }",
                 "    int x;  // } not a brace", "}"]
        self.assertEqual(self.mod.brace_depth(lines, 0, 4), 1)
        self.assertEqual(self.mod.brace_depth(lines, 0, 5), 0)

    def test_a_body_line_resolves_to_its_function(self):
        lines = ["void other(void)", "{", "}", "", "void target(int a)", "{",
                 "    volatile f32 root;", "}"]
        scores = {("tu", "target"): 90.0, ("tu", "other"): 100.0}
        self.assertEqual(
            self.mod.enclosing_function(lines, 6, scores, "tu"), "target")

    def test_volatile_sites_reports_line_function_and_text(self):
        mod = self.mod
        with tempfile.TemporaryDirectory() as d:
            src = Path(d) / "x.c"
            src.write_text("void target(int a)\n{\n    volatile f32 root;\n"
                           "    return;\n}\n")
            self.addCleanup(setattr, mod, "source_of", mod.source_of)
            mod.source_of = lambda tu: src
            sites = mod.volatile_sites("tu", {("tu", "target"): 90.0})
        self.assertEqual(sites, [(3, "target", "volatile f32 root;")])

    def test_tu_reals_reads_ok_pool_and_diff_rows(self):
        mod = self.mod
        out = ("OK   PointViewDist\n"
               "POOL msgWidth  (0 real diff lines after pool-name)\n"
               "DIFF BossCamBossCalc  insns 905/905  lines 364  real 348\n"
               "ONLY-IN-BASE  bosscam_unused_refs\n")
        self.addCleanup(setattr, mod, "subprocess", mod.subprocess)
        mod.subprocess = _FakeSubprocess(out)
        self.assertEqual(mod.tu_reals("game/boss/bosscam"),
                         {"PointViewDist": 0, "msgWidth": 0,
                          "BossCamBossCalc": 348})

    # ---------- negative ----------

    def test_a_file_scope_volatile_after_a_body_is_not_attributed(self):
        """Trap 1: stripping `volatile` from a global is never in scope."""
        lines = ["void target(int a)", "{", "    return;", "}", "",
                 "volatile u32 gHardwareLatch;"]
        scores = {("tu", "target"): 90.0}
        self.assertIsNone(
            self.mod.enclosing_function(lines, 5, scores, "tu"))

    def test_an_initialized_volatile_is_not_matched(self):
        """Trap 2: the audit must not have to preserve an initializer."""
        for line in ("    volatile int x = 0;",
                     "    volatile f32 root = sqrtf(d);",
                     "    volatile u8 buf[4] = {0};"):
            self.assertIsNone(self.mod.VOLATILE.match(line), line)

    def test_a_commented_or_parameter_volatile_is_not_matched(self):
        for line in ("/* volatile f32 root; */",
                     " * volatile f32 root;",
                     "void f(volatile u32* p);",
                     "static volatile int x;"):
            self.assertIsNone(self.mod.VOLATILE.match(line), line)

    def test_a_diff_row_without_a_real_token_is_none_not_zero(self):
        """Trap 3: an unmeasured function must not read as exact."""
        mod = self.mod
        self.addCleanup(setattr, mod, "subprocess", mod.subprocess)
        mod.subprocess = _FakeSubprocess("DIFF msgPost  insns 387/387\n")
        self.assertIsNone(mod.tu_reals("game/ui/message")["msgPost"])


class _FakeSubprocess:
    def __init__(self, out, returncode=0):
        self._out, self._rc = out, returncode

    def run(self, *a, **kw):
        from types import SimpleNamespace
        return SimpleNamespace(stdout=self._out, stderr="",
                               returncode=self._rc)


class AuditVolatileTest(unittest.TestCase):
    """audit_volatile's verdicts and its restore contract, with no compiler."""

    def setUp(self):
        self.mod = load_module()
        self.dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.dir.cleanup)
        self.src = Path(self.dir.name) / "x.c"
        self.text = ("void target(int a)\n{\n    volatile f32 root;\n"
                     "    root = a;\n}\n")
        self.src.write_text(self.text)
        self.mod.source_of = lambda tu: self.src
        self.mod.build_tu = lambda tu: True

    def stub(self, shas, reals):
        """Serve digests then reals in call order."""
        self.mod.obj_sha1 = lambda tu: shas.pop(0)
        self.mod.tu_reals = lambda tu: reals.pop(0)

    # ---------- positive ----------

    def test_an_unchanged_digest_is_dead_and_the_source_is_restored(self):
        self.stub(["aaa", "aaa", "aaa"], [{"target": 7}])
        r = self.mod.audit_volatile("tu", 3, "target")
        self.assertEqual(r["verdict"], "DEAD")
        self.assertIs(r["identical"], True)
        self.assertEqual(self.src.read_text(), self.text)

    def test_a_worsening_real_is_load_bearing(self):
        self.stub(["aaa", "bbb", "aaa"],
                  [{"target": 7}, {"target": 21}])
        r = self.mod.audit_volatile("tu", 3, "target")
        self.assertEqual(r["verdict"], "LOAD-BEARING")
        self.assertEqual(r["worse"], ["target"])
        self.assertEqual(self.src.read_text(), self.text)

    def test_an_improving_real_is_harmful(self):
        self.stub(["aaa", "bbb", "aaa"],
                  [{"target": 21}, {"target": 7}])
        r = self.mod.audit_volatile("tu", 3, "target")
        self.assertEqual(r["verdict"], "HARMFUL")
        self.assertEqual(r["better"], ["target"])

    # ---------- negative ----------

    def test_a_moved_object_with_no_real_delta_is_not_dead(self):
        """Trap: `real` drops reloc rows, so it cannot see every change."""
        self.stub(["aaa", "bbb", "aaa"],
                  [{"target": 7}, {"target": 7}])
        r = self.mod.audit_volatile("tu", 3, "target")
        self.assertEqual(r["verdict"], "MOVED-UNSCORED")
        self.assertNotEqual(r["verdict"], "DEAD")

    def test_a_failed_restore_is_reported_not_scored(self):
        """Trap 4: never report a verdict against a tree that drifted."""
        self.stub(["aaa", "aaa", "ccc"], [{"target": 7}])
        r = self.mod.audit_volatile("tu", 3, "target")
        self.assertEqual(r["verdict"], "RESTORE-FAILED")
        self.assertIn("aaa", r["why"])
        self.assertIn("ccc", r["why"])

    def test_a_non_volatile_line_is_skipped_without_building(self):
        built = []
        self.mod.build_tu = lambda tu: built.append(tu) or True
        r = self.mod.audit_volatile("tu", 4, "target")
        self.assertEqual(r["verdict"], "SKIPPED")
        self.assertEqual(built, [])

    def test_the_source_is_restored_even_when_the_build_fails(self):
        self.mod.obj_sha1 = lambda tu: "aaa"
        self.mod.tu_reals = lambda tu: {"target": 7}
        self.mod.build_tu = lambda tu: False
        r = self.mod.audit_volatile("tu", 3, "target")
        self.assertEqual(r["verdict"], "BUILD-FAILED")
        self.assertEqual(self.src.read_text(), self.text)


class ApplyDeadTest(unittest.TestCase):
    """The batch gate. Individual DEAD proofs do not license a batch."""

    def setUp(self):
        self.mod = load_module()
        self.dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.dir.cleanup)
        self.src = Path(self.dir.name) / "x.c"
        self.text = ("void target(void)\n{\n    volatile f32 a;\n"
                     "    volatile f32 b;\n    int plain;\n}\n")
        self.src.write_text(self.text)
        self.mod.source_of = lambda tu: self.src

    def results(self, *verdicts):
        return [{"tu": "tu", "line": ln, "verdict": v}
                for ln, v in verdicts]

    # ---------- positive ----------

    def test_strip_rewrites_every_named_line_in_one_pass(self):
        original, done = self.mod.strip_volatile_lines("tu", [3, 4])
        self.assertEqual(done, [3, 4])
        self.assertEqual(original, self.text)
        self.assertEqual(self.src.read_text(),
                         "void target(void)\n{\n    f32 a;\n"
                         "    f32 b;\n    int plain;\n}\n")

    def test_an_unchanged_digest_keeps_the_whole_tu(self):
        self.mod.obj_sha1 = lambda tu: "aaa"
        self.mod.build_tu = lambda tu: True
        out = self.mod.apply_dead(self.results((3, "DEAD"), (4, "DEAD")))
        self.assertEqual(len(out), 1)
        self.assertTrue(out[0]["kept"])
        self.assertEqual(out[0]["lines"], [3, 4])
        self.assertNotIn("volatile", self.src.read_text())

    # ---------- negative ----------

    def test_a_moved_digest_reverts_the_tu_whole(self):
        """Trap 5: the batch can move an object no single site moved."""
        digests = iter(["aaa", "bbb", "aaa"])
        self.mod.obj_sha1 = lambda tu: next(digests)
        self.mod.build_tu = lambda tu: True
        out = self.mod.apply_dead(self.results((3, "DEAD"), (4, "DEAD")))
        self.assertFalse(out[0]["kept"])
        self.assertEqual(out[0]["why"], "object digest moved")
        self.assertEqual(self.src.read_text(), self.text)

    def test_a_build_failure_reverts_and_never_reads_a_digest_as_equal(self):
        self.mod.obj_sha1 = lambda tu: "aaa"
        self.mod.build_tu = lambda tu: False
        out = self.mod.apply_dead(self.results((3, "DEAD")))
        self.assertFalse(out[0]["kept"])
        self.assertEqual(out[0]["why"], "build failed")
        self.assertIsNone(out[0]["sha1_after"])
        self.assertEqual(self.src.read_text(), self.text)

    def test_only_dead_verdicts_are_applied(self):
        self.mod.obj_sha1 = lambda tu: "aaa"
        self.mod.build_tu = lambda tu: True
        out = self.mod.apply_dead(self.results((3, "HARMFUL"),
                                               (4, "LOAD-BEARING")))
        self.assertEqual(out, [])
        self.assertEqual(self.src.read_text(), self.text)

    def test_a_non_volatile_line_is_skipped_not_corrupted(self):
        _original, done = self.mod.strip_volatile_lines("tu", [3, 5])
        self.assertEqual(done, [3])
        self.assertIn("    int plain;", self.src.read_text())


class ConfirmVolatileTest(unittest.TestCase):
    """The fuzzy arbiter for a HARMFUL volatile, and its restore contract."""

    def setUp(self):
        self.mod = load_module()
        self.dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.dir.cleanup)
        self.src = Path(self.dir.name) / "x.c"
        self.text = "void t(void)\n{\n    volatile s32 leave;\n}\n"
        self.src.write_text(self.text)
        self.mod.source_of = lambda tu: self.src

    def fuzzies(self, *vals):
        it = iter(vals)
        self.mod.tu_fuzzy = lambda tu: next(it)

    def test_a_fuzzy_gain_confirms(self):
        self.fuzzies(90.0, 90.5, 90.0)
        c = self.mod.confirm_volatile("tu", 3)
        self.assertIs(c["confirmed"], True)
        self.assertEqual(c["delta"], 0.5)

    def test_a_fuzzy_loss_does_not_confirm_and_restores(self):
        """`real` improving while fuzzy falls is exactly what this catches."""
        self.fuzzies(90.0, 89.8, 90.0)
        c = self.mod.confirm_volatile("tu", 3)
        self.assertIs(c["confirmed"], False)
        self.assertEqual(self.src.read_text(), self.text)

    def test_an_unchanged_fuzzy_does_not_confirm(self):
        self.fuzzies(90.0, 90.0, 90.0)
        self.assertIs(self.mod.confirm_volatile("tu", 3)["confirmed"], False)

    def test_a_non_volatile_line_is_not_confirmed_and_touches_nothing(self):
        self.fuzzies(90.0)
        c = self.mod.confirm_volatile("tu", 2)
        self.assertIsNone(c["confirmed"])
        self.assertEqual(self.src.read_text(), self.text)


class ApplyDeadPragmaTest(unittest.TestCase):
    """The pragma half of the batch gate, and the ordering trap it closes."""

    def setUp(self):
        self.mod = load_module()
        self.dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.dir.cleanup)
        self.src = Path(self.dir.name) / "x.c"
        self.text = ("#pragma opt_lifetimes off\n"
                     "void a(void) { }\n"
                     "#pragma opt_lifetimes reset\n"
                     "#pragma opt_common_subs off\n"
                     "void b(void) { }\n"
                     "#pragma opt_common_subs reset\n")
        self.src.write_text(self.text)
        self.mod.source_of = lambda tu: self.src
        self.mod.build_tu = lambda tu: True

    # ---------- positive ----------

    def test_two_regions_in_one_tu_both_disappear(self):
        """THE ORDERING TRAP. Deleting ascending shifts every later line, so
        region two would take the wrong lines. mb_particle has EIGHT regions
        (sixteen lines) in one file, which is where this bites."""
        self.mod.obj_sha1 = lambda tu: "aaa"
        out = self.mod.apply_dead([
            {"tu": "tu", "verdict": "DEAD", "lines": [1, 3]},
            {"tu": "tu", "verdict": "DEAD", "lines": [4, 6]},
        ])
        self.assertTrue(out[0]["kept"])
        self.assertEqual(out[0]["kind"], "pragma")
        self.assertEqual(out[0]["lines"], [1, 3, 4, 6])
        self.assertEqual(self.src.read_text(),
                         "void a(void) { }\nvoid b(void) { }\n")

    def test_delete_lines_works_highest_first(self):
        _orig, done = self.mod.delete_lines("tu", [1, 3])
        self.assertEqual(done, [1, 3])
        self.assertEqual(self.src.read_text().split("\n")[0],
                         "void a(void) { }")

    # ---------- negative ----------

    def test_a_non_pragma_line_is_never_deleted(self):
        """A stale line number must not remove a line of real code."""
        _orig, done = self.mod.delete_lines("tu", [2, 5, 999])
        self.assertEqual(done, [])
        self.assertEqual(self.src.read_text(), self.text)

    def test_a_moved_digest_reverts_the_pragma_batch_whole(self):
        digests = iter(["aaa", "bbb", "aaa"])
        self.mod.obj_sha1 = lambda tu: next(digests)
        out = self.mod.apply_dead([{"tu": "tu", "verdict": "DEAD",
                                    "lines": [1, 3]}])
        self.assertFalse(out[0]["kept"])
        self.assertEqual(self.src.read_text(), self.text)

    def test_volatile_and_pragma_records_do_not_mix(self):
        """One TU can appear in both halves; each gets its own gated batch."""
        self.mod.obj_sha1 = lambda tu: "aaa"
        out = self.mod.apply_dead([
            {"tu": "tu", "verdict": "DEAD", "lines": [1, 3]},
            {"tu": "tu", "verdict": "DEAD", "line": 2},
        ])
        self.assertEqual(sorted(o["kind"] for o in out),
                         ["pragma", "volatile"])


if __name__ == "__main__":
    unittest.main()
