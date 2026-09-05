import copy
import unittest

from tools.gdl.composed_census import r69_btext_small_globals as audit


class SmallGlobalsTests(unittest.TestCase):
    def fixture(self, section=".sdata"):
        base, names = audit.RUNS[section]
        retail = bytes.fromhex("ffffffff3f8000003f800000")
        initialized = section == ".sdata"
        snapshot = dict(sections={section: dict(type=1 if initialized else 8, flags=3,
                        size=12, alignment=8, bytes=retail.hex() if initialized else None)},
                        relocations={}, symbols=[dict(name=n, section=section, type=1,
                        binding=1, other=0, value=i*4, size=4) for i, n in enumerate(names)])
        table = {n: (section, base+i*4, 4) for i, n in enumerate(names)}
        return snapshot, table, retail

    def test_complete_initialized_run(self):
        s, t, r = self.fixture()
        self.assertEqual(audit.section_obligation(s, ".sdata", t, r)["status"], "EXACT")

    def test_bss_is_allocation_not_zero_payload(self):
        s, t, r = self.fixture(".sbss")
        result = audit.section_obligation(s, ".sbss", t, r)
        self.assertEqual(result["status"], "EXACT")
        self.assertIsNone(result["bytes"])
        s["sections"][".sbss"]["bytes"] = "00"*12
        with self.assertRaises(ValueError):
            audit.section_obligation(s, ".sbss", t, r)

    def test_reversed_bss_is_measured_mismatch(self):
        s, t, r = self.fixture(".sbss")
        s["symbols"][0]["value"], s["symbols"][2]["value"] = 8, 0
        result = audit.section_obligation(s, ".sbss", t, r)
        self.assertEqual(result["status"], "LAYOUT_MISMATCH")
        self.assertEqual(result["offset_mismatches"], ["shadow_color", "gLineSpacing"])

    def test_shape_and_datum_corruption_refused(self):
        for key, value in (("type", 8), ("flags", 0), ("alignment", 4),
                           ("size", 16), ("bytes", "00"*12)):
            with self.subTest(key=key):
                s, t, r = self.fixture()
                s["sections"][".sdata"][key] = value
                with self.assertRaises(ValueError):
                    audit.section_obligation(s, ".sdata", t, r)

    def test_relocation_cannot_hide_datum(self):
        s, t, r = self.fixture()
        s["relocations"][".sdata"] = [[0, 1, "foreign", 0]]
        with self.assertRaises(ValueError):
            audit.section_obligation(s, ".sdata", t, r)

    def test_no_missing_extra_duplicate_or_overlapping_object(self):
        for mutation in ("missing", "extra", "duplicate", "overlap"):
            with self.subTest(mutation=mutation):
                s, t, r = self.fixture()
                if mutation == "missing":
                    s["symbols"].pop()
                elif mutation == "extra":
                    s["symbols"].append(dict(s["symbols"][0], name="foreign"))
                elif mutation == "duplicate":
                    s["symbols"].append(dict(s["symbols"][0]))
                else:
                    s["symbols"][1]["value"] = 0
                with self.assertRaises(ValueError):
                    audit.section_obligation(s, ".sdata", t, r)

    def test_wrong_target_identity_or_extent_refused(self):
        for row in ((".bss", 0x80343BB8, 4), (".sdata", 0x80343BBC, 4),
                    (".sdata", 0x80343BB8, 8)):
            s, t, r = self.fixture()
            t["scroll_level_msg"] = row
            with self.assertRaises(ValueError):
                audit.section_obligation(s, ".sdata", t, r)

    def test_wrong_symbol_binding_or_type_metadata_refused(self):
        for key, value in (("binding", 0), ("type", 2), ("other", 1), ("size", 8)):
            s, t, r = self.fixture()
            s["symbols"][0][key] = value
            with self.assertRaises(ValueError):
                audit.section_obligation(s, ".sdata", t, r)

    def pair(self):
        s, _, _ = self.fixture(".sbss")
        s.update(fidelity=True, unit=audit.UNIT, compiler="actual", flags="actual",
                 functions={"kept": dict(body="60000000", offset=0, size=4,
                                         binding=1, relocations=[[0, 109, "shadow_color", 0]])})
        after = dict(snapshot=s, postprocessed=dict(functions=copy.deepcopy(s["functions"]),
                     sections=copy.deepcopy(s["sections"]), exception_records={"kept": "payload"}),
                     raw_eh={"kept": "payload"})
        after["postprocessed"]["symbols"] = {
            sym["name"]: dict(section=sym["section"], offset=sym["value"], size=sym["size"], binding=1)
            for sym in s["symbols"]}
        before = copy.deepcopy(after)
        before["snapshot"]["symbols"][0]["value"] = 8
        before["snapshot"]["symbols"][2]["value"] = 0
        before["postprocessed"]["symbols"]["shadow_color"]["offset"] = 8
        before["postprocessed"]["symbols"]["gLineSpacing"]["offset"] = 0
        return before, after

    def test_only_expected_endpoint_swap_passes(self):
        before, after = self.pair()
        self.assertEqual(audit.preservation(before, after)["status"], "PASS")

    def test_all_raw_identity_dimensions_guarded(self):
        for key in ("unit", "compiler", "flags", "sections", "relocations", "functions"):
            with self.subTest(key=key):
                before, after = self.pair()
                after["snapshot"][key] = "changed"
                with self.assertRaises(ValueError):
                    audit.preservation(before, after)

    def test_no_other_symbol_change(self):
        before, after = self.pair()
        after["snapshot"]["symbols"][1]["binding"] = 0
        with self.assertRaises(ValueError):
            audit.preservation(before, after)
        before, after = self.pair()
        after["postprocessed"]["symbols"]["gDrawTextY"]["size"] = 8
        with self.assertRaises(ValueError):
            audit.preservation(before, after)

    def test_postprocess_and_eh_preservation_guarded(self):
        for key in ("functions", "sections", "exception_records"):
            before, after = self.pair()
            after["postprocessed"][key] = {}
            with self.assertRaises(ValueError):
                audit.preservation(before, after)
        before, after = self.pair()
        after["raw_eh"] = {}
        with self.assertRaises(ValueError):
            audit.preservation(before, after)

    def test_fidelity_and_nonempty_roster_required(self):
        for key, value in (("fidelity", False), ("functions", {})):
            before, after = self.pair()
            before["snapshot"][key] = value
            with self.assertRaises(ValueError):
                audit.preservation(before, after)


if __name__ == "__main__":
    unittest.main()
