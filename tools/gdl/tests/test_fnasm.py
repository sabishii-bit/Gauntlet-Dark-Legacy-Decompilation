"""Tests for fnasm's postprocessor-awareness (run-31 tool queue item 1).

The trap these cover: `build/GUNE5D/src/<unit>.o` is the POSTPROCESSED
object, so for a WebFrank/P6Frank-pinned function `--ours`/`--diff` compares
the target against a byte-identical copy of the target and reports a perfect
match with no warning.  `--raw` reads the pre-postprocess compiler output
instead, and the pin screen warns whenever the postprocessed view is used on
a pinned function.
"""

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import fnasm  # noqa: E402


class PinScreenTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        cfg = root / "config" / fnasm.VERSION
        cfg.mkdir(parents=True)
        (cfg / "webfrank.json").write_text(json.dumps({
            "version": 1,
            "units": {
                "game/sys/sysservice": [
                    {"function": "sysPollResetButton"},
                    {"function": "sysClearFlags"},
                ],
            },
        }), encoding="utf-8")
        (cfg / "p6frank.json").write_text(json.dumps({
            "version": 1,
            "units": {"game/sys/registry": {"function": "regFind"}},
        }), encoding="utf-8")
        self.root = root

    def tearDown(self):
        self.tmp.cleanup()

    def test_webfrank_list_form_is_read(self):
        pins = fnasm.pinned_functions("game/sys/sysservice", root=self.root)
        self.assertEqual(pins,
                         {"sysPollResetButton": "webfrank",
                          "sysClearFlags": "webfrank"})

    def test_p6frank_single_dict_form_is_read(self):
        pins = fnasm.pinned_functions("game/sys/registry", root=self.root)
        self.assertEqual(pins, {"regFind": "p6frank"})

    def test_unpinned_unit_is_empty(self):
        self.assertEqual(fnasm.pinned_functions("game/ui/select",
                                                root=self.root), {})

    def test_missing_config_is_not_fatal(self):
        empty = Path(self.tmp.name) / "nowhere"
        self.assertEqual(fnasm.pinned_functions("game/sys/sysservice",
                                                root=empty), {})

    def test_warning_only_for_pinned_function_in_ours_view(self):
        self.assertIsNotNone(fnasm.pin_warning(
            "game/sys/sysservice", "sysPollResetButton", "ours",
            root=self.root))
        self.assertIsNone(fnasm.pin_warning(
            "game/sys/sysservice", "sysUnpinnedFn", "ours", root=self.root))

    def test_no_warning_for_target_or_raw_views(self):
        for kind in ("target", "raw"):
            self.assertIsNone(fnasm.pin_warning(
                "game/sys/sysservice", "sysPollResetButton", kind,
                root=self.root))

    def test_warning_names_the_rule_and_the_remedy(self):
        msg = fnasm.pin_warning("game/sys/registry", "regFind", "ours",
                                root=self.root)
        self.assertIn("p6frank", msg)
        self.assertIn("--raw", msg)


class RawPathTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def _touch(self, rel):
        p = self.root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(b"")
        return p

    def test_raw_prefers_frank_stage_over_body(self):
        base = f"build/{fnasm.VERSION}/src/game/sys/.postprocess"
        self._touch(f"{base}/body/sysservice.o")
        frank = self._touch(f"{base}/frank/sysservice.o")
        got = fnasm.raw_obj_path("game/sys/sysservice", root=self.root)
        self.assertEqual(got, frank)

    def test_raw_falls_back_to_body(self):
        body = self._touch(
            f"build/{fnasm.VERSION}/src/game/sys/.postprocess/body/"
            "sysservice.o")
        got = fnasm.raw_obj_path("game/sys/sysservice", root=self.root)
        self.assertEqual(got, body)

    def test_raw_returns_none_when_unit_is_not_postprocessed(self):
        self._touch(f"build/{fnasm.VERSION}/src/game/ui/select.o")
        self.assertIsNone(fnasm.raw_obj_path("game/ui/select",
                                             root=self.root))


if __name__ == "__main__":
    unittest.main()
