"""pdb_types: TYPE-stream aggregate layouts, and every way the walk refuses.

WHY. `describe_types` models LF_MODIFIER/LF_POINTER/LF_ARRAY only, so every
struct came back `{"unmodelled": true}`, and `r75_pdb_signatures.py` expands
LF_STRUCTURE for three hard-coded audio tags and refuses the rest. Lane I's
run-62 player work needed `Hidden` (36 x 27) and `Cheats` (20 x 18), neither
of which is in `research/xbox_symbols/xbox_structs.tsv` at all, and had to
hand-roll the expansion in lane scratch.

TWO SIDES. The synthetic half builds CodeView type records by hand and drives
`TypeTable` end to end, so the header shapes, the numeric leaves, the
forward-reference resolution, the LF_INDEX continuation and EVERY refusal are
covered in any checkout. The live half needs shell3D.pdb -- a gitignored
private input that is NOT provisioned into every worktree -- and skips when it
is absent, never passing vacuously.

THE TRAP THIS FILE EXISTS FOR is the silent short layout. A fieldlist walker
that skips a subleaf it does not know, or that consumes LF_ONEMETHOD's
optional vbaseoff word unconditionally, still prints a struct: one that is
missing members, or whose later members are invented out of misaligned bytes,
and nothing in the output says so. Both are asserted to REFUSE below, and the
count reconciliation that catches them is asserted directly.

CALIBRATION (measured 2026-09-08 on shell3D.pdb, 5243 aggregate records):
5183 expand; the 60 refusals are all genuine forward references whose tag has
no defining record in this PDB. Against the 1760 tags `xbox_structs.tsv` also
carries, 1666 agree exactly, 91 differ ONLY by members the TSV drops (84
function pointers, 7 trailing `[0]` flexible arrays) with every size equal,
and 3 are tag collisions where the TSV and `by_name` chose different records
of the same name -- `--index` is the escape for those.
"""
import struct
import subprocess
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
for _path in (str(TOOLS), str(ROOT)):
    if _path not in sys.path:
        sys.path.insert(0, _path)

import pdb_types as pt  # noqa: E402

PDB = ROOT / "research" / "xbox_symbols" / "shell3D.pdb"
TSV = ROOT / "research" / "xbox_symbols" / "xbox_structs.tsv"

T_CHAR, T_INT, T_FLOAT, T_CHARPTR = 0x70, 0x74, 0x40, 0x470
FIRST = 0x1000


def record(leaf, body):
    """One TPI record: [u16 length][u16 leaf][body]; length counts the leaf."""
    return struct.pack("<HH", 2 + len(body), leaf) + body


def name(text):
    return bytes([len(text)]) + text.encode("latin1")


def numeric(value):
    """The inline form below 0x8000, LF_ULONG above it."""
    if 0 <= value < 0x8000:
        return struct.pack("<H", value)
    return struct.pack("<HI", 0x8004, value)


def member(type_index, offset, member_name):
    return (struct.pack("<HHI", 0x1405, 3, type_index) + numeric(offset)
            + name(member_name))


def structure(count, fieldlist, size, tag, property_bits=0):
    return record(pt.LF_STRUCTURE,
                  struct.pack("<HHIII", count, property_bits, fieldlist, 0, 0)
                  + numeric(size) + name(tag))


def tpi(records):
    """A PDB 2.0 TPI stream holding `records` starting at index 0x1000."""
    payload = b"".join(records)
    return struct.pack("<5I", 19961031, 20, FIRST, FIRST + len(records),
                       len(payload)) + payload


def table_of(records):
    types, _header = pt.load_types(tpi(records))
    return pt.TypeTable(types)


class NumericAndStrings(unittest.TestCase):
    def test_inline_and_widened_values_round_trip(self):
        self.assertEqual(pt.numeric_leaf(numeric(36), 0), (36, 2))
        self.assertEqual(pt.numeric_leaf(numeric(0x12345), 0), (0x12345, 6))
        self.assertEqual(pt.numeric_leaf(struct.pack("<Hh", 0x8001, -8), 0),
                         (-8, 4))

    def test_an_unmodelled_numeric_leaf_refuses_rather_than_returning_its_id(self):
        # 0x8005 is LF_REAL32. Returning 32773 as a size would look plausible.
        with self.assertRaises(pt.Unavailable) as caught:
            pt.numeric_leaf(struct.pack("<HI", 0x8005, 0x40000000), 0)
        self.assertIn("unmodelled numeric leaf 0x8005", str(caught.exception))

    def test_a_numeric_or_name_running_past_the_record_refuses(self):
        with self.assertRaises(pt.Unavailable):
            pt.numeric_leaf(struct.pack("<H", 0x8004), 0)
        with self.assertRaises(pt.Unavailable):
            pt.prefixed_string(b"\x08ab", 0)
        with self.assertRaises(pt.Unavailable):
            pt.prefixed_string(b"", 0)

    def test_load_types_refuses_an_impossible_header_or_record(self):
        with self.assertRaises(pt.Unavailable):
            pt.load_types(b"\x00" * 8)
        bad = struct.pack("<5I", 19961031, 20, FIRST, FIRST + 1, 4) \
            + struct.pack("<HH", 0, 0x1005)
        with self.assertRaises(pt.Unavailable):
            pt.load_types(bad)


class SyntheticLayouts(unittest.TestCase):
    """A `Hidden`-shaped record built by hand, plus its refusal siblings."""

    def setUp(self):
        # 0x1000 char[8], 0x1001 char[16], 0x1002 fieldlist, 0x1003 struct
        self.records = [
            record(pt.LF_ARRAY, struct.pack("<II", T_CHAR, T_INT)
                   + numeric(8) + name("")),
            record(pt.LF_ARRAY, struct.pack("<II", T_CHAR, T_INT)
                   + numeric(16) + name("")),
            record(pt.LF_FIELDLIST,
                   member(T_INT, 0, "color") + member(T_INT, 4, "type")
                   + member(0x1000, 8, "mname") + member(0x1001, 16, "dir")
                   + member(T_INT, 32, "disable")),
            structure(5, 0x1002, 36, "Hidden"),
        ]

    def test_the_layout_reconciles_and_carries_offsets_sizes_and_names(self):
        layout = table_of(self.records).expand(0x1003)
        self.assertEqual(layout["name"], "Hidden")
        self.assertEqual(layout["size"], 36)
        self.assertEqual([(row["offset"], row["size"], row["type"],
                           row["name"]) for row in layout["members"]],
                         [(0, 4, "int", "color"), (4, 4, "int", "type"),
                          (8, 8, "char[8]", "mname"),
                          (16, 16, "char[16]", "dir"),
                          (32, 4, "int", "disable")])

    def test_render_emits_a_real_c_declarator_not_type_then_name(self):
        text = pt.render(table_of(self.records).expand(0x1003))
        self.assertIn("char mname[8];", text)
        self.assertIn("char dir[16];", text)
        self.assertNotIn("char[8] mname", text)

    def test_declarator_places_the_star_and_the_dimensions(self):
        self.assertEqual(pt.declarator("char *", "name"), "char *name")
        self.assertEqual(pt.declarator("int[4][3]", "m"), "int m[4][3]")
        self.assertEqual(pt.declarator("int", "x"), "int x")

    def test_a_fieldlist_short_of_the_declared_count_refuses(self):
        broken = list(self.records)
        broken[3] = structure(6, 0x1002, 36, "Hidden")
        with self.assertRaises(pt.Unavailable) as caught:
            table_of(broken).expand(0x1003)
        self.assertIn("NOT reconciled", str(caught.exception))
        self.assertIn("declares 6", str(caught.exception))

    def test_an_unmodelled_subleaf_refuses_instead_of_being_skipped(self):
        broken = list(self.records)
        broken[2] = record(pt.LF_FIELDLIST,
                           member(T_INT, 0, "color")
                           + struct.pack("<HHI", 0x1499, 0, T_INT)
                           + member(T_INT, 4, "type"))
        broken[3] = structure(3, 0x1002, 36, "Hidden")
        with self.assertRaises(pt.Unavailable) as caught:
            table_of(broken).expand(0x1003)
        self.assertIn("unmodelled subleaf 0x1499", str(caught.exception))

    def test_a_fieldlist_index_that_is_not_a_fieldlist_refuses(self):
        with self.assertRaises(pt.Unavailable) as caught:
            table_of(self.records).fieldlist(0x1003)
        self.assertIn("not LF_FIELDLIST", str(caught.exception))

    def test_a_non_aggregate_index_refuses(self):
        with self.assertRaises(pt.Unavailable) as caught:
            table_of(self.records).expand(0x1000)
        self.assertIn("not an aggregate", str(caught.exception))
        with self.assertRaises(pt.Unavailable):
            table_of(self.records).expand(0x9999)


class ForwardReferences(unittest.TestCase):
    def records(self, with_definition=True):
        rows = [record(pt.LF_FIELDLIST,
                       member(T_INT, 0, "texidx") + member(T_CHARPTR, 4, "tname")),
                structure(2, 0x1000, 8, "_blit_setup") if with_definition
                else record(pt.LF_MODIFIER, struct.pack("<IH", T_INT, 0)),
                structure(0, 0, 0, "_blit_setup", property_bits=pt.FORWARD_REF)]
        return rows

    def test_a_forward_reference_resolves_to_the_defining_record(self):
        table = table_of(self.records())
        layout = table.expand(0x1002)
        self.assertEqual(layout["size"], 8)
        self.assertEqual(layout["forward_reference_from"], "0x1002")
        self.assertEqual([row["name"] for row in layout["members"]],
                         ["texidx", "tname"])
        self.assertEqual(table.size(0x1002), 8)

    def test_by_name_never_points_at_the_forward_reference(self):
        table = table_of(self.records())
        self.assertEqual(table.by_name["_blit_setup"], 0x1001)

    def test_an_unresolvable_forward_reference_refuses(self):
        with self.assertRaises(pt.Unavailable) as caught:
            table_of(self.records(with_definition=False)).expand(0x1002)
        self.assertIn("FORWARD REFERENCE", str(caught.exception))
        self.assertIn("no defining record", str(caught.exception))


class UnionsEnumsAndContinuations(unittest.TestCase):
    def test_a_union_expands_with_every_member_at_offset_zero(self):
        rows = [record(pt.LF_FIELDLIST,
                       member(T_INT, 0, "i") + member(T_FLOAT, 0, "f")),
                record(pt.LF_UNION, struct.pack("<HHI", 2, 0, 0x1000)
                       + numeric(4) + name("scalar"))]
        layout = table_of(rows).expand(0x1001)
        self.assertEqual((layout["kind"], layout["size"]), ("union", 4))
        self.assertEqual([row["offset"] for row in layout["members"]], [0, 0])

    def test_an_enum_expands_its_enumerators_and_sizes_by_its_base_type(self):
        enumerate_st = (struct.pack("<HH", 0x0403, 3) + numeric(7)
                        + name("SEVEN"))
        rows = [record(pt.LF_FIELDLIST, enumerate_st),
                record(pt.LF_ENUM, struct.pack("<HHII", 1, 0, T_INT, 0x1000)
                       + name("Mode"))]
        table = table_of(rows)
        layout = table.expand(0x1001)
        self.assertEqual(layout["kind"], "enum")
        self.assertEqual(layout["members"],
                         [{"kind": "enumerator", "name": "SEVEN", "value": 7}])
        self.assertEqual(table.size(0x1001), 4)

    def test_an_lf_index_continuation_is_followed_and_counted(self):
        rows = [record(pt.LF_FIELDLIST, member(T_INT, 4, "second")),
                record(pt.LF_FIELDLIST,
                       member(T_INT, 0, "first")
                       + struct.pack("<HHI", 0x1404, 0, 0x1000)),
                structure(2, 0x1001, 8, "Split")]
        layout = table_of(rows).expand(0x1002)
        self.assertEqual([row["name"] for row in layout["members"]],
                         ["first", "second"])

    def test_a_method_overload_group_counts_every_overload(self):
        # LF_METHOD_ST names a group of 3; the aggregate declares 4 members.
        rows = [record(pt.LF_FIELDLIST,
                       struct.pack("<HHI", 0x1407, 3, 0x1000) + name("op")
                       + member(T_INT, 0, "value")),
                structure(4, 0x1000, 4, "Overloaded")]
        layout = table_of(rows).expand(0x1001)
        self.assertEqual([row["name"] for row in layout["members"]],
                         ["op", "value"])

    def test_an_introducing_virtual_onemethod_consumes_its_vbaseoff(self):
        # Without the conditional u32 the following member is invented.
        intro = struct.pack("<HHII", 0x140B, 0x0113, 0x1000, 0) + name("dtor")
        plain = struct.pack("<HHI", 0x140B, 0x0003, 0x1000) + name("f")
        rows = [record(pt.LF_FIELDLIST,
                       intro + plain + member(T_INT, 0, "value")),
                structure(3, 0x1000, 4, "Virtualish")]
        layout = table_of(rows).expand(0x1001)
        self.assertEqual([row["name"] for row in layout["members"]],
                         ["dtor", "f", "value"])
        self.assertEqual(layout["members"][-1]["offset"], 0)


class CommandLine(unittest.TestCase):
    def run_tool(self, *args):
        return subprocess.run(
            [sys.executable, str(TOOLS / "pdb_types.py")] + list(args),
            capture_output=True, text=True, cwd=str(ROOT))

    def test_no_selector_is_a_usage_error(self):
        done = self.run_tool()
        self.assertEqual(done.returncode, 2)
        self.assertIn("nothing requested", done.stderr)

    def test_a_missing_pdb_refuses_with_the_path_it_looked_for(self):
        done = self.run_tool("--pdb", str(ROOT / "build" / "no-such.pdb"),
                             "--struct", "Hidden")
        self.assertEqual(done.returncode, 2)
        self.assertIn("PDB_TYPES REFUSED", done.stdout)
        self.assertIn("no-such.pdb", done.stdout)

    @unittest.skipUnless(PDB.is_file(), "shell3D.pdb is not in this worktree")
    def test_a_bad_grep_expression_refuses(self):
        done = self.run_tool("--grep", "(unclosed")
        self.assertEqual(done.returncode, 2)
        self.assertIn("bad --grep", done.stdout)

    @unittest.skipUnless(PDB.is_file(), "shell3D.pdb is not in this worktree")
    def test_an_unknown_name_refuses_and_a_known_one_prints_c(self):
        done = self.run_tool("--struct", "NoSuchStructAnywhere")
        self.assertEqual(done.returncode, 2)
        self.assertIn("no struct tag and no PDB global", done.stdout)
        done = self.run_tool("--struct", "_blit_setup")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("int texidx;", done.stdout)
        self.assertIn("char *name;", done.stdout)


@unittest.skipUnless(PDB.is_file(), "shell3D.pdb is not in this worktree")
class LivePdb(unittest.TestCase):
    """The layouts lane I recovered by hand, re-derived by the shipped tool."""

    @classmethod
    def setUpClass(cls):
        from r68_sound_data_recovery import pdb_streams
        cls.streams = pdb_streams(PDB.read_bytes())
        types, cls.header = pt.load_types(cls.streams[2])
        cls.table = pt.TypeTable(types)

    def expand_named(self, wanted):
        index, note = pt.resolve_request(self.table, self.streams, wanted)
        return self.table.expand(index), note

    def test_hidden_is_27_records_of_36_bytes_with_lane_i_s_five_fields(self):
        layout, note = self.expand_named("Hidden")
        self.assertEqual(layout["size"], 36)
        self.assertIn("[27]", note)
        self.assertIn("972 bytes", note)
        self.assertEqual([(row["offset"], row["type"], row["name"])
                          for row in layout["members"]],
                         [(0, "int", "color"), (4, "int", "type"),
                          (8, "char[8]", "name"), (16, "char[16]", "dir"),
                          (32, "int", "disable")])

    def test_cheats_is_18_records_of_20_bytes(self):
        layout, note = self.expand_named("Cheats")
        self.assertEqual(layout["size"], 20)
        self.assertIn("[18]", note)
        self.assertIn("360 bytes", note)
        self.assertEqual([(row["offset"], row["type"], row["name"])
                          for row in layout["members"]],
                         [(0, "char[8]", "name"), (8, "int", "type"),
                          (12, "float", "add"), (16, "int", "flags")])

    def test_blit_setup_is_20_bytes_by_tag_and_through_its_forward_reference(self):
        by_tag, _note = self.expand_named("_blit_setup")
        self.assertEqual(by_tag["size"], 20)
        self.assertEqual([row["name"] for row in by_tag["members"]],
                         ["texidx", "name", "xp", "yp", "zp"])
        # tb_info's own type index is the FORWARD reference 0x3ff6.
        forward = self.table.expand(0x3FF6)
        self.assertEqual(forward["forward_reference_from"], "0x3ff6")
        self.assertEqual(forward["size"], 20)
        self.assertEqual(forward["members"], by_tag["members"])

    def test_three_tsv_structs_agree_member_for_member(self):
        tsv = {}
        current = None
        for line in TSV.read_text(encoding="latin1").splitlines():
            part = line.split("\t")
            if part[0] == "S":
                current = part[1]
                tsv[current] = {"size": int(part[2]), "fields": {}}
            elif part[0] == "F" and current:
                tsv[current]["fields"][int(part[1])] = part[3]
        for tag in ("mini_inv_item", "tPUPType", "tPUPDesc"):
            with self.subTest(tag=tag):
                layout, _note = self.expand_named(tag)
                self.assertEqual(layout["size"], tsv[tag]["size"])
                # The TSV inserts synthetic __alignN padding rows the TYPE
                # stream does not hold; every real member must agree.
                want = {off: field
                        for off, field in tsv[tag]["fields"].items()
                        if not field.startswith("__align")}
                self.assertEqual({row["offset"]: row["name"]
                                  for row in layout["members"]}, want)

    def test_the_walk_refuses_only_genuine_forward_references(self):
        """The calibration in this file's docstring, asserted as a floor."""
        aggregates = [index for index, (leaf, _body)
                      in self.table.types.items()
                      if leaf in (pt.LF_CLASS, pt.LF_STRUCTURE, pt.LF_UNION,
                                  pt.LF_ENUM)]
        expanded, other = 0, []
        for index in aggregates:
            try:
                self.table.expand(index)
                expanded += 1
            except pt.Unavailable as error:
                if "FORWARD REFERENCE" not in str(error):
                    other.append((index, str(error)))
        self.assertEqual(other[:3], [])
        self.assertGreater(expanded, 5000)
        self.assertGreater(len(aggregates), 5000)


if __name__ == "__main__":
    unittest.main()
