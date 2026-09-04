"""T25 run-55 item 3: an empty SELECTION is not a verdict.

REPORTED (CR, run 54): "webfrank_audit --grep CritterCollideItems printed
0 register-only; 0 eligible ... for a function that was in fact eligible".
Reproduced at 84f85a96a — `--grep` filters UNIT PATHS, so a function name
selects nothing and the tool answers a question nobody asked, in the same
words it uses for "scanned and found nothing". With `--function` the same
function reports `1 register-only; 1 eligible in 1 TUs; 592 code bytes`.

CALIBRATED at e8e3959d0 over 256 units and 3,024 function names: 2,981
names (98.6%) match zero unit paths, so the refusal covers the class; the
43 that don't are SDK functions whose name IS their unit path (CARDCheck ->
dolphin/card/CARDCheck), where the grep selects the right TU anyway.

Driven through main() over a synthetic REPO, so no build is required.
"""

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

import webfrank_audit  # noqa: E402


class AuditScopeTests(unittest.TestCase):
    def setUp(self):
        self.repo = Path(tempfile.mkdtemp(prefix="t25audit-"))
        src = self.repo / "src" / "game" / "enemy"
        src.mkdir(parents=True)
        (src / "enemy.c").write_text("/* x */\n", encoding="utf-8")
        (self.repo / "compile_commands.json").write_text(json.dumps([{
            "file": str(src / "enemy.c"),
            "output": str(self.repo / "build" / "GUNE5D" / "src" / "game"
                          / "enemy" / "enemy.o"),
        }]), encoding="utf-8")

    def run_audit(self, *argv):
        out = []
        with mock.patch.object(webfrank_audit, "REPO", self.repo), \
                mock.patch.object(sys, "argv", ["webfrank_audit.py", *argv]), \
                mock.patch("builtins.print", side_effect=out.append):
            code = webfrank_audit.main()
        return code, "\n".join(str(line) for line in out)

    def test_a_function_name_passed_to_grep_is_refused(self):
        code, text = self.run_audit("--grep", "CritterCollideItems")
        self.assertEqual(code, 2)
        self.assertIn("REFUSED", text)
        self.assertIn("matched 0 of 1 unit paths", text)
        self.assertIn("--function CritterCollideItems", text)

    def test_the_refusal_never_reads_as_a_verdict(self):
        _code, text = self.run_audit("--grep", "nosuchunit")
        self.assertIn("never a verdict of ineligibility", text)
        self.assertNotIn("eligible in 0 TUs", text)

    def test_an_unbuilt_tree_is_refused_rather_than_counted_as_zero(self):
        """The objects do not exist in this synthetic repo, so the old code
        printed `0 register-only; 0 eligible` — a zero reached by not
        looking, the same shape as the reported defect."""
        code, text = self.run_audit("--grep", "game/enemy")
        self.assertEqual(code, 2)
        self.assertIn("NONE had both a split target object", text)

    def test_a_path_grep_that_matches_is_not_refused_for_the_grep(self):
        _code, text = self.run_audit("--grep", "game/enemy")
        self.assertNotIn("matched 0 of", text)

    def test_the_grep_help_says_it_is_not_a_function_name(self):
        parser_help = self._help()
        self.assertIn("NOT a function name", parser_help)
        self.assertIn("--function", parser_help)

    def _help(self):
        with mock.patch.object(sys, "argv", ["webfrank_audit.py", "--help"]):
            try:
                with mock.patch("sys.stdout") as out:
                    webfrank_audit.main()
            except SystemExit:
                pass
            return "".join(str(call[0][0])
                           for call in out.write.call_args_list)


if __name__ == "__main__":
    unittest.main()
