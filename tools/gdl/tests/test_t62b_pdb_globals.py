"""pdb_globals: the PDB's shared global-symbol stream, with sizes.

WHY. `research/xbox_symbols/functions_by_module.txt` is generated from the
PDB's PER-MODULE symbol streams and is not a global-data census. Three
objects the GameCube target plainly uses appear nowhere in it and do appear
in the DBI's SHARED symbol-record stream (Codex, r158):

    SandglassBlit   16 bytes,    segment 9 offset 0xAD4880
    soft_reset      int[4],      segment 9 offset 0xAD4810
    restore_pos     float[4][3], segment 9 offset 0xAD4840

An empty module search is not an absence certificate.

TWO SIDES. The synthetic half builds CodeView records by hand and runs
end-to-end through `read_globals`, so the walk, the REFSYM skip and every
refusal are covered in any checkout. The live half needs the PDB, which is a
gitignored private input that is NOT provisioned into every worktree, and
skips when it is absent -- never passes vacuously.

THE REFSYM TRAP is the one that decides whether the walk works at all:
`S_PROCREF`/`S_DATAREF`/`S_LPROCREF` carry a length-prefixed name AFTER their
declared record length and the next record begins at the following 4-byte
boundary. A walker that trusts `reclen` desynchronizes at the first one, and
then either crashes or manufactures symbols out of misaligned bytes -- which
in a dump of 3,629 rows is indistinguishable from real ones. Both behaviours
are asserted below.

THE ARRAY-ORDER TRAP. CodeView nests array dimensions outermost-first, so the
naive recursive spelling prints `float[3][4]` for what the source declared
`float[4][3]`. A reversed shape reads as a different object; the source order
is asserted here against the live record.
"""
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT))

import pdb_globals as pg  # noqa: E402

PDB = ROOT / "research" / "xbox_symbols" / "shell3D.pdb"


def record(kind, body):
    """One CodeView record: [u16 len][u16 kind][body], len counts kind+body.

    Padded to a 4-byte multiple, as a real stream is: the REFSYM realignment
    below is absolute `& ~3` arithmetic, so an unaligned synthetic record
    makes the walker land mid-record and the fixture tests a stream shape
    that does not exist.
    """
    body = body + b"\x00" * ((-(4 + len(body))) % 4)
    return struct.pack("<HH", 2 + len(body), kind) + body


def data_symbol(kind, type_index, offset, segment, name):
    encoded = name.encode("latin1")
    return record(kind, struct.pack("<IIH", type_index, offset, segment)
                  + bytes([len(encoded)]) + encoded)


def refsym(kind, hidden_name):
    """A REFSYM whose name sits OUTSIDE reclen, then 4-byte realignment."""
    encoded = hidden_name.encode("latin1")
    head = record(kind, struct.pack("<IIH", 0xDEAD, 0, 1))
    tail = bytes([len(encoded)]) + encoded
    pad = (-(len(head) + len(tail))) % 4
    return head + tail + b"\x00" * pad


def empty_tpi():
    """A TPI with no records: describe() then answers primitives only."""
    return struct.pack("<5I", 19960307, 20, 0x1000, 0x1000, 0)


def dbi_naming(stream_index):
    return b"\x00" * 20 + struct.pack("<H", stream_index)


class Primitives(unittest.TestCase):
    def test_known_primitives_resolve(self):
        self.assertEqual(pg.primitive_type(0x74), ("int", 4))
        self.assertEqual(pg.primitive_type(0x40), ("float", 4))
        self.assertEqual(pg.primitive_type(0x41), ("double", 8))
        self.assertEqual(pg.primitive_type(0x70), ("char", 1))

    def test_a_near32_pointer_is_four_bytes(self):
        self.assertEqual(pg.primitive_type(0x470), ("char *", 4))
        self.assertEqual(pg.primitive_type(0x474), ("int *", 4))

    def test_type_stream_indices_are_not_primitives(self):
        self.assertIsNone(pg.primitive_type(0x1000))
        self.assertIsNone(pg.primitive_type(0x3ecd))

    def test_unknown_and_invalid_indices_return_none_not_a_guess(self):
        self.assertIsNone(pg.primitive_type(0x0FF))
        self.assertIsNone(pg.primitive_type(-1))
        self.assertIsNone(pg.primitive_type("0x74"))


class TypeSizeAndName(unittest.TestCase):
    ARRAY = {"type_index": "0x3ecd", "leaf": "0x1003", "size": 48, "count": 4,
             "element": {"type_index": "0x2c9d", "leaf": "0x1003",
                         "size": 12,
                         "element": {"type_index": "0x40",
                                     "unmodelled": True}}}
    POINTER_ARRAY = {"type_index": "0x3e46", "leaf": "0x1003", "size": 16,
                     "element": {"type_index": "0x3950", "leaf": "0x1002",
                                 "target": {"type_index": "0x394f",
                                            "unmodelled": True}}}
    INCOMPLETE = dict(ARRAY, size=0, count=0)

    def test_an_array_reports_its_own_total_size(self):
        self.assertEqual(pg.type_size(self.ARRAY), 48)

    def test_a_pointer_is_four_bytes(self):
        self.assertEqual(
            pg.type_size({"type_index": "0x3950", "leaf": "0x1002"}), 4)

    def test_a_primitive_without_a_leaf_still_has_a_size(self):
        self.assertEqual(pg.type_size({"type_index": "0x40",
                                       "unmodelled": True}), 4)

    def test_an_unknown_type_is_none_not_zero(self):
        # 0 would read as "an empty object", a different claim entirely.
        self.assertIsNone(pg.type_size({"type_index": "0x394f",
                                        "unmodelled": True}))
        self.assertIsNone(pg.type_size(None))

    def test_array_dimensions_come_out_in_source_order(self):
        self.assertEqual(pg.type_name(self.ARRAY), "float[4][3]")

    def test_an_incomplete_extern_array_prints_empty_brackets(self):
        self.assertEqual(pg.type_name(self.INCOMPLETE), "float[][3]")

    def test_a_pointer_element_counts_and_spells(self):
        self.assertEqual(pg.type_name(self.POINTER_ARRAY), "type0x394f *[4]")

    def test_an_unmodelled_type_names_its_index_rather_than_guessing(self):
        self.assertEqual(pg.type_name({"type_index": "0x394f",
                                       "unmodelled": True}), "type0x394f")


class StreamWalk(unittest.TestCase):
    def test_it_yields_each_record_with_its_offset_and_kind(self):
        buf = (data_symbol(pg.S_GDATA32, 0x74, 0x10, 9, "a")
               + data_symbol(pg.S_LDATA32, 0x74, 0x20, 9, "bb"))
        rows = list(pg.iterate_symbols(buf))
        self.assertEqual([row[1] for row in rows],
                         [pg.S_GDATA32, pg.S_LDATA32])
        self.assertEqual(rows[0][0], 0)

    def test_a_refsym_hidden_name_does_not_desynchronize_the_walk(self):
        buf = (refsym(0x400, "SomeProcedureName")
               + data_symbol(pg.S_GDATA32, 0x74, 0x30, 9, "after_refsym"))
        rows = list(pg.iterate_symbols(buf))
        self.assertEqual([row[1] for row in rows], [0x400, pg.S_GDATA32])
        self.assertEqual(pg.parse_data_symbol(rows[1][2], pg.S_GDATA32)["name"],
                         "after_refsym")

    def test_every_refsym_kind_is_handled(self):
        for kind in pg.REFSYM_KINDS:
            with self.subTest(kind=hex(kind)):
                buf = (refsym(kind, "Hidden")
                       + data_symbol(pg.S_GDATA32, 0x74, 0, 9, "tail"))
                rows = list(pg.iterate_symbols(buf))
                self.assertEqual(len(rows), 2)

    def test_trailing_zero_padding_ends_the_walk_quietly(self):
        buf = data_symbol(pg.S_GDATA32, 0x74, 0, 9, "a") + b"\x00" * 64
        self.assertEqual(len(list(pg.iterate_symbols(buf))), 1)

    def test_a_length_that_runs_past_the_buffer_refuses(self):
        buf = struct.pack("<HH", 0x7FFF, pg.S_GDATA32) + b"\x00" * 8
        with self.assertRaisesRegex(pg.Unavailable, "malformed symbol record"):
            list(pg.iterate_symbols(buf))

    def test_an_impossible_length_refuses_rather_than_resynchronizing(self):
        buf = struct.pack("<HH", 1, pg.S_GDATA32) + b"\x01" * 16
        with self.assertRaises(pg.Unavailable):
            list(pg.iterate_symbols(buf))


class DataSymbols(unittest.TestCase):
    def test_it_reads_the_type_segment_offset_and_name(self):
        body = data_symbol(pg.S_GDATA32, 0x23E1, 0xAD4810, 9, "soft_reset")[4:]
        row = pg.parse_data_symbol(body, pg.S_GDATA32)
        self.assertEqual(row["name"], "soft_reset")
        self.assertEqual(row["segment"], 9)
        self.assertEqual(row["offset"], 0xAD4810)
        self.assertEqual(row["type_index"], 0x23E1)

    def test_a_public_symbol_has_flags_where_a_type_index_would_be(self):
        body = data_symbol(pg.S_PUB32, 0x2, 0x100, 1, "pub")[4:]
        row = pg.parse_data_symbol(body, pg.S_PUB32)
        self.assertIsNone(row["type_index"])
        self.assertEqual(row["public_flags"], 2)

    def test_a_truncated_record_refuses(self):
        with self.assertRaises(pg.Unavailable):
            pg.parse_data_symbol(b"\x00" * 6, pg.S_GDATA32)

    def test_a_name_running_past_its_record_refuses(self):
        body = struct.pack("<IIH", 0x74, 0, 9) + bytes([40]) + b"short"
        with self.assertRaisesRegex(pg.Unavailable, "runs past"):
            pg.parse_data_symbol(body, pg.S_GDATA32)


class SyntheticEndToEnd(unittest.TestCase):
    def streams(self, symbols, index=4):
        rows = [b"", b"", empty_tpi(), dbi_naming(index)]
        while len(rows) < index:
            rows.append(b"")
        rows.append(symbols)
        return rows

    def test_a_whole_stream_becomes_rows_with_sizes(self):
        symbols = (data_symbol(pg.S_GDATA32, 0x74, 0xAD4810, 9, "soft_reset")
                   + refsym(0x400, "IgnoredProc")
                   + data_symbol(pg.S_LDATA32, 0x40, 0x20, 2, "a_float"))
        result = pg.read_globals(self.streams(symbols))
        self.assertEqual([row["name"] for row in result["rows"]],
                         ["soft_reset", "a_float"])
        self.assertEqual(result["rows"][0]["size"], 4)
        self.assertEqual(result["rows"][0]["type"], "int")
        self.assertEqual(result["rows"][1]["type"], "float")
        self.assertEqual(result["rows"][0]["segment"], 9)

    def test_publics_are_excluded_unless_asked_for(self):
        symbols = (data_symbol(pg.S_GDATA32, 0x74, 0, 9, "g")
                   + data_symbol(pg.S_PUB32, 0x2, 0, 9, "p"))
        self.assertEqual(
            [row["name"] for row in pg.read_globals(self.streams(symbols))["rows"]],
            ["g"])
        self.assertEqual(
            [row["name"] for row in
             pg.read_globals(self.streams(symbols), publics=True)["rows"]],
            ["g", "p"])

    def test_a_public_row_carries_no_invented_type_or_size(self):
        symbols = data_symbol(pg.S_PUB32, 0x2, 0, 9, "p")
        row = pg.read_globals(self.streams(symbols), publics=True)["rows"][0]
        self.assertIsNone(row["type"])
        self.assertIsNone(row["size"])

    def test_a_pdb_without_a_dbi_stream_refuses(self):
        with self.assertRaisesRegex(pg.Unavailable, "no DBI stream"):
            pg.read_globals([b"", b"", empty_tpi()])

    def test_a_dbi_naming_a_missing_stream_refuses(self):
        with self.assertRaisesRegex(pg.Unavailable, "does not exist"):
            pg.read_globals([b"", b"", empty_tpi(), dbi_naming(99)])

    def test_a_short_dbi_refuses(self):
        with self.assertRaisesRegex(pg.Unavailable, "too short"):
            pg.symbol_stream_index(b"\x00" * 10)

    def test_the_report_always_carries_the_address_limitation(self):
        result = pg.read_globals(self.streams(
            data_symbol(pg.S_GDATA32, 0x74, 0, 9, "g")))
        self.assertTrue(any("OMAP" in line for line in result["limits"]))
        self.assertIn("OMAP", pg.format_rows(result))


class CommandLine(unittest.TestCase):
    def run_tool(self, *args):
        return subprocess.run(
            [sys.executable, "tools/gdl/pdb_globals.py", *args],
            cwd=str(ROOT), capture_output=True, text=True)

    def test_a_missing_pdb_refuses_and_names_the_path(self):
        with tempfile.TemporaryDirectory() as temp:
            absent = Path(temp) / "nope.pdb"
            done = self.run_tool("--pdb", str(absent), "--no-out")
            self.assertEqual(done.returncode, 2, done.stdout + done.stderr)
            self.assertIn("PDB_GLOBALS REFUSED", done.stdout)
            self.assertIn("nope.pdb", done.stdout)

    def test_a_non_pdb_file_refuses(self):
        with tempfile.TemporaryDirectory() as temp:
            junk = Path(temp) / "junk.pdb"
            junk.write_bytes(b"not a pdb at all" * 8)
            done = self.run_tool("--pdb", str(junk), "--no-out")
            self.assertEqual(done.returncode, 2, done.stdout)
            self.assertIn("REFUSED", done.stdout)

    def test_an_out_path_outside_build_is_refused(self):
        done = self.run_tool("--out", "src/leak.txt")
        self.assertEqual(done.returncode, 2)

    def test_help_exits_zero(self):
        done = self.run_tool("--help")
        self.assertEqual(done.returncode, 0)
        self.assertIn("--grep", done.stdout)


@unittest.skipUnless(PDB.is_file(),
                     "needs research/xbox_symbols/shell3D.pdb (gitignored)")
class LivePdb(unittest.TestCase):
    """The r158 names, at their recorded sizes and PDB addresses."""

    @classmethod
    def setUpClass(cls):
        cls.result = pg.read_globals(pg.pdb_streams(PDB.read_bytes()))
        cls.by_name = {}
        for row in cls.result["rows"]:
            cls.by_name.setdefault(row["name"], []).append(row)

    def test_the_shared_stream_parses_to_its_end(self):
        self.assertEqual(self.result["symbol_stream"], 511)
        self.assertEqual(self.result["symbol_stream_bytes"], 1070080)
        self.assertGreater(len(self.result["rows"]), 3000)

    def test_sandglass_blit_is_sixteen_bytes_of_four_pointers(self):
        rows = [row for row in self.by_name["SandglassBlit"]
                if row["size"] == 16]
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["segment"], 9)
        self.assertEqual(rows[0]["offset"], 0xAD4880)
        self.assertTrue(rows[0]["type"].endswith("*[4]"), rows[0]["type"])

    def test_soft_reset_is_int_four(self):
        rows = self.by_name["soft_reset"]
        self.assertEqual([row["type"] for row in rows], ["int[4]"])
        self.assertEqual(rows[0]["size"], 16)
        self.assertEqual((rows[0]["segment"], rows[0]["offset"]),
                         (9, 0xAD4810))

    def test_restore_pos_is_float_four_by_three_in_source_order(self):
        rows = [row for row in self.by_name["restore_pos"] if row["size"]]
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["type"], "float[4][3]")
        self.assertEqual(rows[0]["size"], 48)
        self.assertEqual((rows[0]["segment"], rows[0]["offset"]),
                         (9, 0xAD4840))

    def test_an_incomplete_extern_declaration_is_reported_separately(self):
        # The PDB carries BOTH an `extern float restore_pos[][3];` record
        # (size 0) and the definition (48). Collapsing them would hide which
        # one the size came from.
        sizes = sorted(row["size"] for row in self.by_name["restore_pos"])
        self.assertEqual(sizes, [0, 48])

    def test_the_names_the_module_roster_omits_are_here(self):
        roster = (ROOT / "research/xbox_symbols/functions_by_module.txt")
        if not roster.is_file():
            self.skipTest("no functions_by_module.txt")
        text = roster.read_text(encoding="utf-8", errors="replace")
        for name in ("SandglassBlit", "soft_reset", "restore_pos"):
            self.assertIn(name, self.by_name)
            self.assertNotIn(name, text,
                             "%s is in the module roster after all; the"
                             " premise for this tool needs remeasuring" % name)

    def test_grep_narrows_the_report(self):
        done = subprocess.run(
            [sys.executable, "tools/gdl/pdb_globals.py", "--grep",
             "restore_pos", "--no-out"],
            cwd=str(ROOT), capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("float[4][3]", done.stdout)
        self.assertNotIn("SandglassBlit", done.stdout)

    def test_a_bad_grep_pattern_refuses(self):
        done = subprocess.run(
            [sys.executable, "tools/gdl/pdb_globals.py", "--grep", "(",
             "--no-out"],
            cwd=str(ROOT), capture_output=True, text=True)
        self.assertEqual(done.returncode, 2, done.stdout)
        self.assertIn("bad --grep", done.stdout)

    def test_the_full_dump_is_written_under_build(self):
        out = ROOT / "build" / pg.VERSION / "t62b_pdb_globals_test.txt"
        done = subprocess.run(
            [sys.executable, "tools/gdl/pdb_globals.py", "--grep",
             "soft_reset", "--out", str(out)],
            cwd=str(ROOT), capture_output=True, text=True)
        self.addCleanup(lambda: out.exists() and out.unlink())
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        text = out.read_text(encoding="utf-8")
        self.assertIn("SandglassBlit", text, "the FILE holds the FULL dump")
        self.assertNotIn("SandglassBlit", done.stdout, "stdout is filtered")


if __name__ == "__main__":
    unittest.main()
