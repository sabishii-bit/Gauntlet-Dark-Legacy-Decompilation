import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.gdl.composed_census import r69_controls_ownership_audit as audit
from tools.gdl.composed_census import r69_controls_layout_probe as probe


class ControlsAllocationTests(unittest.TestCase):
    def fixture(self, extracted=False, corrected=False):
        tail_size = 4 if corrected else 8
        symbols = [dict(name=n, value=a-audit.BASE, size=tail_size if extracted and n == "lbl_80344620" else size,
                        section=".sbss", type=1, binding=1, other=0) for n, a, size in audit.OBJECTS]
        table = {n: (".sbss", a, tail_size if n == "lbl_80344620" else size) for n, a, size in audit.OBJECTS}
        snap = dict(sections={".sbss": dict(type=8, flags=3, size=72+tail_size if extracted else 76,
                    alignment=8, bytes=None)}, relocations={}, symbols=symbols)
        return snap, table

    def test_complete_source_and_target_extents(self):
        for extracted in (False, True):
            for corrected in (False, True):
                with self.subTest(extracted=extracted, corrected=corrected):
                    snap, table = self.fixture(extracted, corrected)
                    row = audit.allocation(snap, table, extracted)
                    self.assertEqual(row["status"], "EXACT" if corrected else "NAMED_LAYOUT_EXACT_WITH_TERMINAL_EXTENT")
                    self.assertEqual(row["terminal_alignment_extent"], 0 if corrected else 4)
                    self.assertEqual(row["target_extent"], 76 if corrected else 80)
                    self.assertEqual(row["full_section_exact"], corrected)
                    self.assertIsNone(row["initialized_bytes"])

    def test_claim_and_symbol_metadata_must_describe_same_state(self):
        for corrected in (False, True):
            _, table = self.fixture(corrected=corrected)
            end = audit.SCALAR_END if corrected else audit.LEGACY_END
            wrong_end = audit.LEGACY_END if corrected else audit.SCALAR_END
            self.assertEqual(audit.check_claim((audit.BASE, end), table), end-audit.BASE)
            with self.assertRaises(ValueError):
                audit.check_claim((audit.BASE, wrong_end), table)
            with self.assertRaises(ValueError):
                audit.check_claim((audit.BASE+4, end), table)

    def test_corrected_extraction_cannot_smuggle_old_slack(self):
        snap, table = self.fixture(extracted=True, corrected=True)
        snap["sections"][".sbss"]["size"] = 80
        with self.assertRaises(ValueError):
            audit.allocation(snap, table, extracted=True)
        snap, table = self.fixture(extracted=True, corrected=True)
        snap["symbols"][-1]["size"] = 8
        with self.assertRaises(ValueError):
            audit.allocation(snap, table, extracted=True)

    def test_unsupported_target_tail_or_identity_refused(self):
        for target in ((".sbss", 0x80344620, 12), (".sbss", 0x80344620, 0),
                       (".bss", 0x80344620, 4), (".sbss", 0x80344624, 4)):
            snap, table = self.fixture()
            table["lbl_80344620"] = target
            with self.assertRaises(ValueError):
                audit.allocation(snap, table)

    def test_cli_strict_gate_requires_no_slack(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "build").mkdir()
            out = root / "build/r69_controls_gate.json"
            for corrected in (False, True):
                snap, table = self.fixture(corrected=corrected)
                result = dict(status="MEASURED", small_bss_allocation=audit.allocation(snap, table))
                with patch.object(audit, "ROOT", root), patch.object(audit, "measure", return_value=result):
                    self.assertEqual(audit.main(["--require-exact", "--out", str(out)]), 0 if corrected else 2)
                    written = json.loads(out.read_text())
                    self.assertEqual(written["status"], "MEASURED" if corrected else "UNRESOLVED")

    def test_wrong_section_properties_rejected(self):
        for key, value in (("type", 1), ("flags", 0), ("size", 80),
                           ("alignment", 4), ("bytes", "00"*76)):
            snap, table = self.fixture()
            snap["sections"][".sbss"][key] = value
            with self.assertRaises(ValueError):
                audit.allocation(snap, table)

    def test_wrong_offset_size_scope_target_rejected(self):
        for key, value in (("value", 4), ("size", 8), ("binding", 0), ("other", 1)):
            snap, table = self.fixture()
            snap["symbols"][0][key] = value
            with self.assertRaises(ValueError):
                audit.allocation(snap, table)
        snap, table = self.fixture()
        table["ctrls_initialized"] = (".sbss", audit.BASE, 4)
        with self.assertRaises(ValueError):
            audit.allocation(snap, table)

    def test_no_extra_pad_or_missing_name(self):
        for action in ("pad", "missing", "duplicate"):
            snap, table = self.fixture()
            if action == "pad":
                snap["symbols"].append(dict(snap["symbols"][0], name="lbl_80344624", value=76))
            elif action == "missing":
                snap["symbols"].pop()
            else:
                snap["symbols"].append(dict(snap["symbols"][0]))
            with self.assertRaises(ValueError):
                audit.allocation(snap, table)

    def test_relocations_fail_closed(self):
        snap, table = self.fixture()
        snap["relocations"][".sbss"] = [[0, 1, "foreign", 0]]
        with self.assertRaises(ValueError):
            audit.allocation(snap, table)

    def pair(self):
        snap, _ = self.fixture()
        snap.update(fidelity=True, unit=audit.UNIT, compiler="actual", flags="actual",
                    functions={"keep": dict(body="60000000", relocations=[[0, 109, "ctrls_initialized", 0]])})
        post = dict(functions=copy.deepcopy(snap["functions"]), sections={".sbss": dict(snap["sections"][".sbss"], relocations=[])},
                    exception_records={"keep": "payload"}, symbols={"other": dict(offset=0, size=16)})
        after = dict(snapshot=snap, postprocessed=post, raw_eh={"keep": "payload"})
        before = copy.deepcopy(after)
        before["snapshot"]["sections"][".sbss"]["size"] = 80
        before["snapshot"]["symbols"].append(dict(snap["symbols"][0], name="lbl_80344624", value=60, binding=0))
        before["postprocessed"]["sections"][".sbss"]["size"] = 80
        return before, after

    def test_preservation_and_body_relocation_guards(self):
        before, after = self.pair()
        self.assertEqual(audit.preservation(before, after)["status"], "PASS")
        for key in ("unit", "compiler", "flags", "functions", "relocations"):
            before, after = self.pair()
            after["snapshot"][key] = "changed"
            with self.assertRaises(ValueError):
                audit.preservation(before, after)

    def test_other_sections_symbols_and_eh_guarded(self):
        for where, key in (("postprocessed", "functions"), ("postprocessed", "exception_records"),
                           ("postprocessed", "symbols")):
            before, after = self.pair()
            after[where][key] = {}
            with self.assertRaises(ValueError):
                audit.preservation(before, after)
        before, after = self.pair()
        after["raw_eh"] = {}
        with self.assertRaises(ValueError):
            audit.preservation(before, after)
        before, after = self.pair()
        after["snapshot"]["symbols"].append(dict(after["snapshot"]["symbols"][0], section=".bss", name="other"))
        with self.assertRaises(ValueError):
            audit.preservation(before, after)

    def source(self, newline=b"\n"):
        declarations = []
        for name, _, size in audit.OBJECTS:
            prefix = b"" if name in probe.GLOBAL_NAMES else b"static "
            declarations.append(prefix+b"u32 "+name.encode()+(b"[2]" if size == 8 else b"")+b";")
        declarations.append(b"static u32 lbl_80344624;")
        return newline.join([b"before", b"/* --- .sbss --- */"]+declarations+[b"/* --- .other --- */", b"after"])

    def test_diagnostic_reproduces_negative_controls_after_repair(self):
        forms = probe.small_forms(self.source())
        again = probe.small_forms(forms["global_reverse"])
        for name in forms.keys()-{"baseline"}:
            self.assertEqual(forms[name], again[name])
        self.assertIn(b"static u32 lbl_803445D8;", again["reverse_only"])
        self.assertNotIn(b"static", again["global_reverse"])

    def test_diagnostic_preserves_line_endings_and_untouched_regions(self):
        forms = probe.small_forms(self.source(b"\r\n"))
        for text in forms.values():
            self.assertTrue(text.startswith(b"before\r\n/* --- .sbss --- */"))
            self.assertTrue(text.endswith(b"/* --- .other --- */\r\nafter"))
            self.assertNotIn(b"\n", text.replace(b"\r\n", b""))

    def test_referenced_nominal_pad_is_not_removed(self):
        with self.assertRaises(ValueError):
            probe.small_forms(self.source()+b"\nlbl_80344624 = 1;\n")


if __name__ == "__main__":
    unittest.main()
