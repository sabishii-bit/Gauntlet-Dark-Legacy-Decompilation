"""Diagnostic safety/effect classification; does not mandate a destructive fixup."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.gdl.composed_census import r66_exception_mod_probe as probe


class ExceptionProbeTests(unittest.TestCase):
    def test_only_one_literal_edit_is_authorized(self):
        before = "prefix\n" + probe.ORIGINAL + "\nsuffix\n"
        after = "prefix\n" + probe.MODIFIED + "\nsuffix\n"
        probe.validate_source(before, after)
        with self.assertRaisesRegex(ValueError, "beyond"):
            probe.validate_source(before, after + "extra edit")
        with self.assertRaisesRegex(ValueError, "exactly one"):
            probe.validate_source(before + probe.ORIGINAL, after)

    def test_non_elf_input_refuses(self):
        with tempfile.TemporaryDirectory(prefix="r66_exception_test_") as td:
            path = Path(td) / "not.o"
            path.write_bytes(b"not a compiler object")
            with self.assertRaisesRegex(ValueError, "ELF32"):
                probe.inspect_object(path)

    def test_effect_classifier_accepts_preservation_or_identifies_overwrite(self):
        before = {"what_strings": ["MODIFIED!"]}
        self.assertEqual(probe.classify_effect(before, before), "PRESERVED")
        self.assertEqual(probe.classify_effect(before, {"what_strings": ["exception"]}), "OVERWRITTEN_WITH_RETAIL_LITERAL")
        self.assertEqual(probe.classify_effect(before, {"what_strings": ["different"]}), "OTHER_DATUM_CHANGE")

    def test_uncompiled_modification_is_not_evidence(self):
        with self.assertRaisesRegex(ValueError, "did not reach"):
            probe.classify_effect({"what_strings": ["exception"]}, {"what_strings": ["exception"]})

    def test_outside_build_source_refuses_before_compiler_or_fixup(self):
        with tempfile.TemporaryDirectory(prefix="r66_exception_test_") as td:
            root = Path(td)
            with patch.object(probe, "ROOT", root), \
                 patch.object(probe.cv, "compile_with", side_effect=AssertionError("compiler launched")), \
                 patch.object(probe.fix, "fix_nmw", side_effect=AssertionError("fixup called")):
                rc = probe.main(["--modified-source", str(root / "src/NMWException.cpp"), "--out", str(root / "build/result.json")])
            self.assertEqual(rc, 2)
            result = json.loads((root / "build/result.json").read_text())
            self.assertEqual(result["status"], "UNRESOLVED")
            self.assertIn("isolated copy", result["error"])

    def test_output_cannot_overwrite_production_source(self):
        with tempfile.TemporaryDirectory(prefix="r66_exception_test_") as td:
            root = Path(td)
            source = root / "production.cpp"
            source.write_text("keep me")
            with patch.object(probe, "ROOT", root):
                rc = probe.main(["--modified-source", str(root / "build/modified.cpp"), "--out", str(source)])
            self.assertEqual(rc, 2)
            self.assertEqual(source.read_text(), "keep me")


if __name__ == "__main__":
    unittest.main()
