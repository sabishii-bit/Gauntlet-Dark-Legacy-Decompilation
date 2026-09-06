"""Private-binary-independent retirement guards, including stale raw selection."""
import copy
import struct
import unittest

from tools.gdl.composed_census import r70_sndvoice_retirement_audit as audit


def active_edge():
    return dict(src=audit.SOURCE, body_o=audit.OUTPUT, rule="mwcc_sjis", mw="GC/1.2.5n", raw=False,
                command_template="build/tools/sjiswrap.exe build/compilers/$mw_version/mwcceppc.exe "
                                 "$cflags -MMD -c $in -o $basedir")


def sample_functions():
    return dict(functions={audit.FUNCTION: dict(offset=0, size=4, body="4e800020", relocations=[]),
                           "Sibling": dict(offset=4, size=4, body="48000001", relocations=[[0, 10, "callee", 0]])})


class RetirementTests(unittest.TestCase):
    def edge(self, edge=None, config=None, ninja=None):
        return audit.retired_edge({audit.UNIT: edge or active_edge()}, config or {"units": {}}, ninja or
                                  f"build {audit.OUTPUT}: mwcc_sjis {audit.SOURCE} | compiler\n")

    def test_plain_edge_selected_without_filesystem_fallback(self):
        self.assertEqual(self.edge()["body_o"], audit.OUTPUT)

    def test_existing_rule_or_missing_inventory_refused(self):
        for config in ({"units": {audit.UNIT: [{"function": audit.FUNCTION}]}}, {"other": {}}):
            with self.assertRaises(ValueError):
                self.edge(config=config)

    def test_stale_raw_edge_and_compiler_or_transform_changes_refused(self):
        for field, value in (("body_o", "build/GUNE5D/src/game/g3d/.postprocess/body/sndvoice.o"),
                             ("raw", True), ("rule", "mwcc_sjis_extab"), ("mw", "GC/1.2.5s"),
                             ("command_template", active_edge()["command_template"] + " && patch object")):
            with self.subTest(field=field), self.assertRaises(ValueError):
                edge = active_edge()
                edge[field] = value
                self.edge(edge=edge)

    def test_transformed_or_duplicate_actual_producer_refused(self):
        normal = f"build {audit.OUTPUT}: mwcc_sjis {audit.SOURCE}\n"
        for ninja in (normal.replace(": mwcc_sjis ", ": webfrank "), normal + normal,
                      normal + "build build/GUNE5D/src/game/g3d/.postprocess/body/sndvoice.o: mwcc_sjis source.c\n"):
            with self.assertRaises(ValueError):
                self.edge(ninja=ninja)

    def test_complete_object_comparison_includes_data_and_metadata(self):
        data = b"object-body-data-relocations-eh"
        self.assertTrue(audit.require_same_object(data, data, "control")["byte_identical"])
        for wrong in (b"", data[:-1] + b"!", data + b"alignment"):
            with self.assertRaises(ValueError):
                audit.require_same_object(data, wrong, "control")
        with self.assertRaises(ValueError):
            audit.require_same_object(b"", b"", "control")

    def test_whole_roster_body_and_positional_relocation_checks(self):
        before = sample_functions()
        self.assertEqual(audit.exact_functions(before, before)["function_count"], 2)
        for key, value in (("body", "48000000"), ("offset", 8), ("size", 8),
                           ("relocations", [[0, 10, "other_callee", 0]]),
                           ("relocations", [[0, 10, "callee", 4]]),
                           ("relocations", [[2, 10, "callee", 0]]),
                           ("relocations", [[0, 1, "callee", 0]])):
            after = copy.deepcopy(before)
            after["functions"]["Sibling"][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                audit.exact_functions(before, after)

    def test_empty_or_missing_function_roster_refused(self):
        for after in ({"functions": {}}, {"functions": {"Other": {}}}):
            with self.assertRaises(ValueError):
                audit.exact_functions(after, after)

    def test_full_text_coverage_rejects_gaps_extra_bytes_and_bad_alignment(self):
        obj = sample_functions()
        obj["sections"] = {".text": dict(size=8, bytes="4e80002048000001", type=1, flags=6, alignment=4)}
        self.assertEqual(audit.text_coverage(obj), 8)
        for mutate in (lambda x: x["functions"]["Sibling"].update(offset=8),
                       lambda x: x["sections"][".text"].update(size=12),
                       lambda x: x["sections"][".text"].update(alignment=8)):
            bad = copy.deepcopy(obj)
            mutate(bad)
            with self.assertRaises(ValueError):
                audit.text_coverage(bad)

    def test_only_sda21_subword_convention_is_normalized(self):
        a = [[0, 109, "g", 0]]
        self.assertEqual(audit.relocation_rows(a), audit.relocation_rows([[2, 109, "g", 0]]))
        self.assertNotEqual(audit.relocation_rows(a), audit.relocation_rows([[1, 109, "g", 0]]))
        self.assertNotEqual(audit.relocation_rows([[0, 10, "g", 0]]), audit.relocation_rows([[2, 10, "g", 0]]))
        with self.assertRaises(ValueError):
            audit.relocation_rows([[0, 109, "g", 0], [2, 109, "g", 0]])

    def test_data_base_alias_requires_exact_entire_context(self):
        symbol = dict(section=".data", offset=0, size=1932, binding=1, bytes="00" * 1932)
        base = dict(section=".data", offset=0, size=0, kind=0, binding=0)
        obj = dict(symbols={"sndDbTable": symbol}, sections={".data": dict(size=3006, bytes="00" * 3006)},
                   exception_records={})
        self.assertEqual(audit.verify_data_base(obj, obj, base), {"...data.0": "sndDbTable"})
        for mutate in (lambda x: x["symbols"]["sndDbTable"].update(offset=4),
                       lambda x: x["symbols"]["sndDbTable"].update(bytes="01" * 1932),
                       lambda x: x["sections"][".data"].update(size=3008),
                       lambda x: x.update(exception_records={"unexpected": {}})):
            bad = copy.deepcopy(obj)
            mutate(bad)
            with self.assertRaises(ValueError):
                audit.verify_data_base(bad, obj, base)
        with self.assertRaises(ValueError):
            audit.verify_data_base(obj, obj, dict(base, offset=4))

    def test_layout_requires_exact_fields_extent_and_unrelocated_values(self):
        values = [2, 36, 88, 48, 40] + list(range(0, 36, 2))
        symbol = dict(section=".data", size=len(values) * 4,
                      bytes=struct.pack(">" + "I" * len(values), *values).hex())
        inventory = dict(symbols={"r70_sndvoice_layout": symbol}, sections={".data": dict(relocations=[])})
        result = audit.check_layout(inventory)
        self.assertEqual(result["mixer_output_halfwords"], 18)
        self.assertEqual(result["input_mix_halfwords"], 20)
        for mutate in (lambda x: x["symbols"]["r70_sndvoice_layout"].update(size=88),
                       lambda x: x["symbols"]["r70_sndvoice_layout"].update(bytes="00" * 92),
                       lambda x: x["sections"][".data"].update(relocations=[[0, 1, "symbol", 0]])):
            bad = copy.deepcopy(inventory)
            mutate(bad)
            with self.assertRaises(ValueError):
                audit.check_layout(bad)


if __name__ == "__main__":
    unittest.main()
