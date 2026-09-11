"""Qualifiers are not pointers: exercise both public PDB type views.

The authentic Time witness is LF_MODIFIER 0x3e21, body
400000000200f2f1: volatile float, not float * or const float. Synthetic
records cover every modifier bit, placement around pointers, non-pointer
sizes and refusal of malformed records without needing the private PDB.
"""
import struct
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

import pdb_globals as pg  # noqa: E402
import pdb_types as pt  # noqa: E402

PDB = TOOLS.parents[1] / "research/xbox_symbols/shell3D.pdb"


def record(leaf, body):
    return struct.pack("<HH", len(body) + 2, leaf) + body


def modifier(target, flags):
    return record(pt.LF_MODIFIER, struct.pack("<IH", target, flags))


def pointer(target, attributes=0x0A):
    # CV_PTR_NEAR32, ordinary pointer mode; not LF_MODIFIER's flag layout.
    return record(pt.LF_POINTER, struct.pack("<II", target, attributes))


def tpi(records):
    payload = b"".join(records)
    return struct.pack("<5I", 19961031, 20, 0x1000,
                       0x1000 + len(records), len(payload)) + payload


def table(records):
    return pt.TypeTable(pt.load_types(tpi(records))[0])


def global_streams(records, index=0x1000):
    # A genuine shared-stream GDATA32 shape, separate from the TPI leaves.
    symbol = record(pg.S_GDATA32, struct.pack("<IIHB", index, 0x1234, 9, 1)
                    + b"g")
    return [b"", b"", tpi(records), bytes(20) + struct.pack("<H", 4), symbol]


class QualifierTypes(unittest.TestCase):
    def assert_type(self, records, index, spelling, size):
        with self.subTest(view="pdb_globals", spelling=spelling):
            row = pg.read_globals(global_streams(records, index))["rows"][0]
            self.assertEqual((row["type"], row["size"]), (spelling, size))
        with self.subTest(view="pdb_types", spelling=spelling):
            types = table(records)
            self.assertEqual((types.spell(index), types.size(index)),
                             (spelling, size))

    def test_each_modifier_combination_preserves_the_base_type(self):
        for flags, spelling in (
                (0, "float"), (1, "const float"), (2, "volatile float"),
                (3, "const volatile float"), (4, "__unaligned float"),
                (5, "const __unaligned float"),
                (6, "volatile __unaligned float"),
                (7, "const volatile __unaligned float")):
            with self.subTest(flags=flags):
                records = [modifier(0x40, flags)]
                self.assert_type(records, 0x1000, spelling, 4)
                description = pg.describe_types(tpi(records))(0x1000)
                self.assertEqual(description["modifiers"], flags)
                self.assertEqual(description["target"]["type_index"], "0x40")

    def test_qualifiers_do_not_force_a_four_byte_size(self):
        for base, name, size in ((0x70, "char", 1), (0x11, "short", 2),
                                 (0x41, "double", 8),
                                 (0x42, "long double", 10)):
            with self.subTest(base=base):
                self.assert_type([modifier(base, 2)], 0x1000,
                                 "volatile " + name, size)

    def test_real_pointer_is_not_a_modifier_and_stays_four_bytes(self):
        self.assert_type([pointer(0x41)], 0x1000, "double *", 4)
        self.assert_type([modifier(0x41, 1)], 0x1000, "const double", 8)

    def test_modified_pointer_differs_from_pointer_to_modified_type(self):
        for flags, qualifier in ((1, "const"), (2, "volatile"),
                                 (3, "const volatile"), (4, "__unaligned")):
            with self.subTest(flags=flags):
                self.assert_type([pointer(0x41), modifier(0x1000, flags)],
                                 0x1001, "double * " + qualifier, 4)
                self.assert_type([modifier(0x41, flags), pointer(0x1000)],
                                 0x1001, qualifier + " double *", 4)
                self.assert_type([modifier(0x441, flags)], 0x1000,
                                 "double * " + qualifier, 4)

    def test_pointer_attribute_bits_qualify_the_pointer_not_its_target(self):
        for attributes, qualifier in ((0x400, "const"), (0x200, "volatile"),
                                      (0x600, "const volatile"),
                                      (0x800, "__unaligned"),
                                      (0xE00, "const volatile __unaligned")):
            with self.subTest(attributes=attributes):
                self.assert_type([pointer(0x41, 0xA | attributes)],
                                 0x1000, "double * " + qualifier, 4)

    def test_nested_pointer_levels_keep_separate_qualifiers(self):
        records = [modifier(0x41, 1), pointer(0x1000), modifier(0x1001, 2),
                   pointer(0x1002), modifier(0x1003, 1)]
        self.assert_type(records, 0x1004, "const double * volatile * const", 4)

    def test_arrays_use_qualified_element_sizes(self):
        array = record(pt.LF_ARRAY, struct.pack("<IIH", 0x1000, 0x74, 24)
                       + b"\0")
        self.assert_type([modifier(0x41, 2), array], 0x1001,
                         "volatile double[3]", 24)
        array = record(pt.LF_ARRAY, struct.pack("<IIH", 0x1000, 0x74, 24)
                       + b"\0")
        self.assert_type([pointer(0x41), array, modifier(0x1001, 1)],
                         0x1002, "double * const[6]", 24)

    def test_modified_aggregate_retains_layout_size_and_field_spelling(self):
        fields = record(pt.LF_FIELDLIST,
                        struct.pack("<HHIHB", 0x1405, 3, 0x1000, 0, 1) + b"x")
        aggregate = record(pt.LF_STRUCTURE,
                           struct.pack("<HHIIIHB", 1, 0, 0x1001, 0, 0, 8, 1)
                           + b"S")
        types = table([modifier(0x41, 2), fields, aggregate,
                       modifier(0x1002, 1)])
        self.assertEqual(types.spell(0x1003), "const struct S")
        self.assertEqual(types.size(0x1003), 8)
        layout = types.expand(0x1002)
        self.assertEqual(layout["members"][0]["type"], "volatile double")
        self.assertEqual(layout["members"][0]["size"], 8)
        self.assertIn("volatile double x;", pt.render(layout))

    def test_unknown_modifier_base_does_not_invent_a_pointer_size(self):
        records = [modifier(0x9999, 2)]
        description = pg.describe_types(tpi(records))(0x1000)
        self.assertIsNone(pg.type_size(description))
        self.assertEqual(pg.type_name(description), "volatile type0x9999")
        self.assertIsNone(table(records).size(0x1000))
        self.assertEqual(table(records).spell(0x1000), "volatile type0x9999?")


class InvalidQualifierRecords(unittest.TestCase):
    def assert_refused(self, records, message):
        with self.assertRaisesRegex(ValueError, message):
            pg.describe_types(tpi(records))(0x1000)
        with self.assertRaisesRegex(ValueError, message):
            pg.read_globals(global_streams(records))
        types = table(records)
        for method in (types.spell, types.size):
            with self.subTest(method=method.__name__):
                with self.assertRaisesRegex(pt.Unavailable, message):
                    method(0x1000)

    def test_each_truncated_modifier_body_refuses_instead_of_guessing(self):
        body = struct.pack("<IH", 0x40, 2)
        for length in range(6):
            with self.subTest(length=length):
                self.assert_refused([record(pt.LF_MODIFIER, body[:length])],
                                    "truncated LF_MODIFIER")

    def test_each_truncated_pointer_header_refuses_even_for_size(self):
        body = struct.pack("<II", 0x41, 0xA)
        for length in range(8):
            with self.subTest(length=length):
                self.assert_refused([record(pt.LF_POINTER, body[:length])],
                                    "truncated LF_POINTER")

    def test_reserved_modifier_bits_refuse_instead_of_silently_disappearing(self):
        for flags in (8, 0x8000, 0xFFFF):
            with self.subTest(flags=flags):
                self.assert_refused([modifier(0x40, flags)],
                                    "unsupported LF_MODIFIER flags")

    def test_incomplete_descriptions_do_not_turn_into_pointers(self):
        description = {"leaf": "0x1001", "type_index": "0x1000",
                       "target": {"type_index": "0x40"}}
        with self.assertRaisesRegex(ValueError, "LF_MODIFIER flags"):
            pg.type_name(description)
        self.assertIsNone(pg.type_size({"leaf": "0x1001", "modifiers": 2}))

    def test_recursive_modifier_chain_is_bounded(self):
        records = [modifier(0x1000, 2)]
        with self.assertRaisesRegex(ValueError, "recursion"):
            pg.describe_types(tpi(records))(0x1000)
        self.assertIsNone(table(records).size(0x1000))
        self.assertIn("?deep", table(records).spell(0x1000))


@unittest.skipUnless(PDB.is_file(), "needs private shell3D.pdb (gitignored)")
class LiveTimeWitness(unittest.TestCase):
    def test_time_record_is_volatile_float_in_both_public_views(self):
        streams = pg.pdb_streams(PDB.read_bytes())
        types = pt.TypeTable(pt.load_types(streams[2])[0])
        self.assertEqual(types.types[0x3E21],
                         (pt.LF_MODIFIER, bytes.fromhex("400000000200f2f1")))
        rows = [row for row in pg.read_globals(streams)["rows"]
                if row["name"] == "Time"]
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["type_index"], 0x3E21)
        self.assertEqual((rows[0]["type"], rows[0]["size"]),
                         ("volatile float", 4))
        self.assertEqual((types.spell(0x3E21), types.size(0x3E21)),
                         ("volatile float", 4))


if __name__ == "__main__":
    unittest.main()
