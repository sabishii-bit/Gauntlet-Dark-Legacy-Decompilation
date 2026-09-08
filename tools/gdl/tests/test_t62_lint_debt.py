"""Two-sided tests for tools/gdl/lint_debt.py, the read-only lint ledger.

The ledger reads the reconstruction linter's JSON and reports where review
debt sits and whether it moved. Two properties matter more than the tables:

  * it must NEVER touch the linter, its rules or its policy -- not by
    importing it, not by running it, not by writing beside its report; and
  * a movement report that is wrong is worse than none, so a delta is
    measured as a multiset over fingerprints AND as per-(TU, rule) counts,
    and neither may be fabricated from an unusable report.

Every fixture here is a small synthetic report, so the tests do not need a
scan; the two live checks are skipped when no report exists.
"""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

from tools.gdl import lint_debt  # noqa: E402


def finding(rule="FM007", path="src/game/a/a.c", line=1, scope="fn_a",
            fingerprint=None, suppressed=False, severity="error", **extra):
    row = {"rule": rule, "path": path, "line": line, "column": 1,
           "scope": scope, "message": "candidate", "confidence": "heuristic",
           "excerpt": "0x20", "severity": severity, "guidance_id": rule,
           "suppressed": suppressed,
           "fingerprint": fingerprint or ("%s|%s|%s|%s" % (path, rule, scope,
                                                           line))}
    row.update(extra)
    return row


def report(findings, **extra):
    active = [row for row in findings if not row["suppressed"]]
    data = {
        "schema_version": 1, "status": "SCAN_COMPLETE",
        "engine": "ast-grep (test fixture)", "source_scope": "src/game",
        "interpretation": "Review candidates, not proven fakematches.",
        "findings": findings, "files_scanned": len({r["path"] for r in findings}),
        "unsuppressed": len(active), "suppressed": len(findings) - len(active),
        "errors": sum(1 for r in active if r["severity"] != "warning"),
        "warnings": sum(1 for r in active if r["severity"] == "warning"),
        "rules_selected": sorted({r["rule"] for r in findings}),
        "parse_recovery": [],
        "by_rule": {}, "by_file": {}, "source_sha256": {},
        "remediation_guidance": {"schema_version": 1, "common": {},
                                 "rules": {}},
    }
    for row in active:
        data["by_rule"][row["rule"]] = data["by_rule"].get(row["rule"], 0) + 1
        data["by_file"][row["path"]] = data["by_file"].get(row["path"], 0) + 1
    data.update(extra)
    return data


def write(root, name, data):
    path = Path(root) / name
    path.write_text(json.dumps(data), encoding="utf-8")
    return path


class Refusals(unittest.TestCase):
    """The invalid side: nothing may be reported from an unusable input."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def test_a_missing_report_refuses_and_names_the_scan_command(self):
        with self.assertRaises(lint_debt.ReportUnavailable) as caught:
            lint_debt.load_report(self.root / "absent.json")
        self.assertIn("fakematch_lint.py", str(caught.exception))

    def test_malformed_json_refuses(self):
        path = self.root / "bad.json"
        path.write_text("{not json", encoding="utf-8")
        with self.assertRaises(lint_debt.ReportUnavailable):
            lint_debt.load_report(path)

    def test_an_unknown_schema_version_refuses(self):
        path = write(self.root, "v.json", report([finding()],
                                                 schema_version=2))
        with self.assertRaisesRegex(lint_debt.ReportUnavailable, "schema"):
            lint_debt.load_report(path)

    def test_a_report_without_a_findings_list_refuses(self):
        path = write(self.root, "n.json", dict(report([]), findings=None))
        with self.assertRaisesRegex(lint_debt.ReportUnavailable, "findings"):
            lint_debt.load_report(path)

    def test_a_finding_missing_a_required_field_refuses(self):
        broken = finding()
        del broken["fingerprint"]
        path = write(self.root, "f.json", report([broken]))
        with self.assertRaisesRegex(lint_debt.ReportUnavailable,
                                    "fingerprint"):
            lint_debt.load_report(path)

    def test_a_json_list_is_not_a_report(self):
        path = self.root / "list.json"
        path.write_text("[]", encoding="utf-8")
        with self.assertRaises(lint_debt.ReportUnavailable):
            lint_debt.load_report(path)

    def test_the_cli_exits_two_and_prints_nothing_it_did_not_measure(self):
        done = subprocess.run(
            [sys.executable, "tools/gdl/lint_debt.py",
             str(self.root / "absent.json")],
            cwd=str(ROOT), capture_output=True, text=True)
        self.assertEqual(done.returncode, 2)
        self.assertIn("REFUSED", done.stdout)
        self.assertNotIn("PER-TU DEBT", done.stdout)


class Tables(unittest.TestCase):
    def rows(self):
        return [finding(rule="FM007", path="src/game/a/a.c", scope="fn_a",
                        line=1),
                finding(rule="FM007", path="src/game/a/a.c", scope="fn_a",
                        line=2),
                finding(rule="FM001", path="src/game/a/a.c", scope="fn_b",
                        line=3),
                finding(rule="FM006", path="src/game/a/a.c", scope="fn_b",
                        line=4, severity="warning"),
                finding(rule="FM007", path="src/game/a/a.c", scope="fn_c",
                        line=5, suppressed=True,
                        review_reason="verified enum value"),
                finding(rule="FM003", path="src/game/b/b.c", scope="fn_d",
                        line=1)]

    def test_a_reviewed_exception_is_not_counted_as_open_but_is_shown(self):
        rows = {row["path"]: row for row in lint_debt.tu_rows(report(self.rows()))}
        first = rows["src/game/a/a.c"]
        self.assertEqual(first["open"], 4)
        self.assertEqual(first["exceptions"], 1)
        self.assertEqual((first["errors"], first["warnings"]), (3, 1))
        self.assertIn("FM007:2", first["rules"])
        self.assertIn("FM007:1", first["exception_rules"])

    def test_tu_rows_rank_by_open_debt(self):
        rows = lint_debt.tu_rows(report(self.rows()))
        self.assertEqual([row["path"] for row in rows],
                         ["src/game/a/a.c", "src/game/b/b.c"])

    def test_family_rows_split_severity_and_count_distinct_files(self):
        families = {row["rule"]: row
                    for row in lint_debt.family_rows(report(self.rows()))}
        self.assertEqual(families["FM007"]["open"], 2)
        self.assertEqual(families["FM007"]["exceptions"], 1)
        self.assertEqual(families["FM007"]["files"], 1)
        self.assertEqual(families["FM006"]["warnings"], 1)
        self.assertEqual(families["FM006"]["errors"], 0)

    def test_a_family_summary_comes_from_the_reports_own_guidance(self):
        data = report(self.rows())
        data["remediation_guidance"]["rules"]["FM007"] = {"summary": "hex"}
        families = {row["rule"]: row for row in lint_debt.family_rows(data)}
        self.assertEqual(families["FM007"]["summary"], "hex")
        self.assertEqual(families["FM001"]["summary"], "")

    def test_an_empty_report_is_an_empty_ledger_not_an_error(self):
        self.assertEqual(lint_debt.tu_rows(report([])), [])
        self.assertEqual(lint_debt.family_rows(report([])), [])

    def test_the_table_says_how_many_rows_it_did_not_print(self):
        rows = lint_debt.tu_rows(report(self.rows()))
        text = "\n".join(lint_debt.format_tu_table(rows, top=1))
        self.assertIn("... 1 more TU(s), 1 open finding(s)", text)


class CrossCheck(unittest.TestCase):
    """Our totals and the report's own counts must be reconciled, not assumed."""

    def test_a_consistent_report_produces_no_note(self):
        self.assertEqual(lint_debt.cross_check(report([finding()])), [])

    def test_a_disagreeing_by_rule_is_reported_not_hidden(self):
        data = report([finding()])
        data["by_rule"]["FM007"] = 99
        notes = lint_debt.cross_check(data)
        self.assertTrue(any("by_rule" in note for note in notes), notes)

    def test_a_disagreeing_unsuppressed_total_is_reported(self):
        data = report([finding()])
        data["unsuppressed"] = 7
        self.assertTrue(any("unsuppressed" in note
                            for note in lint_debt.cross_check(data)))


class Freshness(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "src/game/a").mkdir(parents=True)

    def source(self, name, text):
        path = self.root / name
        path.write_text(text, encoding="utf-8")
        import hashlib
        return hashlib.sha256(path.read_bytes()).hexdigest()

    def test_an_unchanged_tree_reads_current(self):
        digest = self.source("src/game/a/a.c", "int a;\n")
        fresh = lint_debt.freshness_rows(
            report([], source_sha256={"src/game/a/a.c": digest}), self.root)
        self.assertEqual((fresh["changed"], fresh["missing"],
                          fresh["unscanned"]), ([], [], []))

    def test_an_edited_source_is_named_changed(self):
        digest = self.source("src/game/a/a.c", "int a;\n")
        self.source("src/game/a/a.c", "int a; int b;\n")
        fresh = lint_debt.freshness_rows(
            report([], source_sha256={"src/game/a/a.c": digest}), self.root)
        self.assertEqual(fresh["changed"], ["src/game/a/a.c"])

    def test_a_deleted_source_is_missing_not_unchanged(self):
        fresh = lint_debt.freshness_rows(
            report([], source_sha256={"src/game/a/gone.c": "0" * 64}),
            self.root)
        self.assertEqual(fresh["missing"], ["src/game/a/gone.c"])

    def test_a_scoped_file_the_scan_never_saw_is_named(self):
        digest = self.source("src/game/a/a.c", "int a;\n")
        self.source("src/game/a/new.c", "int n;\n")
        fresh = lint_debt.freshness_rows(
            report([], source_sha256={"src/game/a/a.c": digest}), self.root)
        self.assertEqual(fresh["unscanned"], ["src/game/a/new.c"])

    def test_the_rendered_header_says_STALE_when_a_file_moved(self):
        digest = self.source("src/game/a/a.c", "int a;\n")
        self.source("src/game/a/a.c", "int a; int b;\n")
        data = report([finding()], source_sha256={"src/game/a/a.c": digest})
        text = "\n".join(lint_debt.render(data, "r.json", root=self.root))
        self.assertIn("freshness   STALE", text)
        self.assertIn("re-scan before quoting these numbers", text)


class Movement(unittest.TestCase):
    def test_counts_move_per_tu_and_rule_bucket(self):
        before = report([finding(line=1), finding(line=2),
                         finding(rule="FM001", line=3)])
        after = report([finding(line=1),
                        finding(rule="FM001", line=3),
                        finding(rule="FM001", line=4)])
        moved = {(row["path"], row["rule"]): row
                 for row in lint_debt.compare(before, after)["counts"]}
        self.assertEqual(moved[("src/game/a/a.c", "FM007")]["open_delta"], -1)
        self.assertEqual(moved[("src/game/a/a.c", "FM001")]["open_delta"], 1)

    def test_an_unchanged_bucket_is_not_listed(self):
        same = report([finding(line=1)])
        self.assertEqual(lint_debt.compare(same, same)["counts"], [])
        self.assertEqual(lint_debt.compare(same, same)["new"], [])

    def test_new_findings_carry_the_function_they_sit_in(self):
        before = report([finding(line=1)])
        after = report([finding(line=1),
                        finding(line=9, scope="CritterDoKnockback",
                                fingerprint="new-one")])
        movement = lint_debt.compare(before, after)
        self.assertEqual([row["scope"] for row in movement["new"]],
                         ["CritterDoKnockback"])
        text = "\n".join(lint_debt.format_movement(movement))
        self.assertIn("CritterDoKnockback (1)", text)

    def test_a_waived_finding_reads_as_an_exception_not_a_resolution(self):
        # Suppressing a row must not look like the debt disappeared.
        before = report([finding(line=1, fingerprint="f1")])
        after = report([finding(line=1, fingerprint="f1", suppressed=True,
                                review_reason="verified")])
        movement = lint_debt.compare(before, after)
        self.assertEqual(movement["new"], [])
        self.assertEqual(movement["resolved"], [])
        self.assertEqual(movement["open_after"], 0)
        self.assertEqual(movement["exceptions_after"], 1)
        row = movement["counts"][0]
        self.assertEqual((row["open_delta"], row["exceptions_delta"]), (-1, 1))

    def test_duplicate_fingerprints_are_multiset_arithmetic_not_a_set(self):
        # Fingerprints were all distinct in the live report, but a set
        # difference would report NOTHING for a real second occurrence.
        before = report([finding(line=1, fingerprint="dup")])
        after = report([finding(line=1, fingerprint="dup"),
                        finding(line=2, fingerprint="dup")])
        movement = lint_debt.compare(before, after)
        self.assertEqual(len(movement["new"]), 1)
        self.assertEqual(movement["resolved"], [])

    def test_the_movement_text_states_which_measure_is_conservative(self):
        before = report([finding(line=1)])
        after = report([finding(line=1), finding(line=2, fingerprint="x")])
        text = "\n".join(lint_debt.format_movement(
            lint_debt.compare(before, after)))
        self.assertIn("conservative measure", text)
        self.assertIn("FINGERPRINT movement", text)


class ReadOnly(unittest.TestCase):
    """The ledger may not touch the linter or write anywhere but --out."""

    def test_the_module_never_imports_or_shells_out_to_the_linter(self):
        source = (TOOLS / "lint_debt.py").read_text(encoding="utf-8")
        for banned in ("import fakematch_lint", "from fakematch_lint",
                       "subprocess", "ast_grep", "ast-grep"):
            with self.subTest(banned=banned):
                self.assertNotIn(banned, source)
        # Measured in a FRESH interpreter: another test in this suite may
        # legitimately have the linter loaded already, so asserting on this
        # process's sys.modules would test the suite, not the module.
        done = subprocess.run(
            [sys.executable, "-c",
             "import sys; sys.path.insert(0, 'tools/gdl');"
             " import lint_debt;"
             " print(sorted(n for n in sys.modules if 'fakematch' in n))"],
            cwd=str(ROOT), capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertEqual(done.stdout.strip(), "[]")

    def test_the_module_names_no_writable_path_under_vscode_lint(self):
        source = (TOOLS / "lint_debt.py").read_text(encoding="utf-8")
        for line in source.splitlines():
            if ".vscode/lint" in line:
                with self.subTest(line=line.strip()[:60]):
                    self.assertNotIn("write", line)
                    self.assertNotIn("open(", line)

    def test_a_default_run_writes_nothing_and_out_writes_exactly_one_file(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        root = Path(temp.name)
        source = write(root, "report.json", report([finding()]))
        before = sorted(path.name for path in root.iterdir())
        done = subprocess.run(
            [sys.executable, "tools/gdl/lint_debt.py", str(source),
             "--no-freshness"],
            cwd=str(ROOT), capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertEqual(sorted(path.name for path in root.iterdir()), before)
        out = root / "ledger.json"
        done = subprocess.run(
            [sys.executable, "tools/gdl/lint_debt.py", str(source),
             "--no-freshness", "--out", str(out)],
            cwd=str(ROOT), capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        payload = json.loads(out.read_text(encoding="utf-8"))
        self.assertEqual(payload["totals"]["open"], 1)
        self.assertEqual(sorted(path.name for path in root.iterdir()),
                         sorted(before + ["ledger.json"]))


LIVE = ROOT / "build" / "fakematch_lint.json"


@unittest.skipUnless(LIVE.is_file(),
                     "needs build/fakematch_lint.json from a lint scan")
class LiveReport(unittest.TestCase):
    """The live report is the calibration population for the tables."""

    @classmethod
    def setUpClass(cls):
        cls.data = lint_debt.load_report(LIVE)

    def test_the_derived_totals_reconcile_with_the_reports_own(self):
        self.assertEqual(lint_debt.cross_check(self.data), [])

    def test_every_scanned_file_with_findings_appears_once(self):
        rows = lint_debt.tu_rows(self.data)
        paths = [row["path"] for row in rows]
        self.assertEqual(len(paths), len(set(paths)))
        self.assertEqual(sum(row["open"] for row in rows),
                         self.data["unsuppressed"])

    def test_family_open_totals_equal_the_reports_by_rule(self):
        families = {row["rule"]: row["open"]
                    for row in lint_debt.family_rows(self.data)
                    if row["open"]}
        self.assertEqual(families, self.data["by_rule"])

    def test_comparing_a_report_with_itself_reports_no_movement(self):
        movement = lint_debt.compare(self.data, self.data)
        self.assertEqual((movement["counts"], movement["new"],
                          movement["resolved"]), ([], [], []))


if __name__ == "__main__":
    unittest.main()
