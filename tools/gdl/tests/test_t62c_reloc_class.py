"""data_credit RELOC: the class that stops the two tools contradicting.

THE CONTRADICTION (run 62, reproduced at fe3e4e736 on one tree):

    data_credit.py --boundaries : game/game/controls .data   BYTE
    datadiff.py --sections      : game/game/controls .data   100.0% bytes equal

`BYTE` means "the bytes differ; symbols.txt cannot help" -- source work. Lane
H measured the bytes directly (`objcopy -O binary --only-section .data` on
both objects: 3252 == 3252, zero differing bytes) and the 99.0715% is 11
`R_PPC_ADDR32` rows at `.data` +0xC48..+0xC68 and +0xC80..+0xC88 whose target
side names `lbl_80111F70..lbl_801120F8` in an UNCLAIMED `.rodata` while ours
names `...rodata.0 + off`. objdiff cannot pair a symbol that exists in only
one object, so it scores those words unmatched however identical the bytes.
That is `.rodata`-CLAIM work, and no source edit pays it.

The mechanism is proved by the two neighbours that DO pair, +0xC44 and
+0xC50: their symbol names also differ (`@1` against `lbl_803463F8`), but
they point into `.sdata2`, which controls DOES claim -- so the split has a
symbol there and objdiff resolves them. Address-inside-a-claimed-run is
therefore the discriminant, not name equality.

CALIBRATION over the live 208 uncredited section rows: SOURCELESS 111,
ANONYMOUS 72, BOUNDARY 23, RELOC 1, BYTE 1. Exactly one row moves, and the
surviving BYTE row (`game/mb/mb_blit .ctors`) is one where byte equality
CANNOT be measured -- the section is absent from one object -- which is the
negative half: an unmeasured byte comparison never promotes a row to RELOC.

TWO SIDES. Synthetic maps and relocation tables drive the classifier and
every refusal; the live half re-measures controls and asserts the two tools
now say compatible things about the same tree.
"""
import unittest
from pathlib import Path

from tools.gdl import data_credit as dc

ROOT = Path(__file__).resolve().parents[3]
CONTROLS = "main/game/game/controls"
# controls.c's runs, as splits.txt has them.
RUNS = [("extab", 0x80005C18, 0x80005CE0), (".text", 0x8003104C, 0x80034CFC),
        (".data", 0x8011A220, 0x8011AED4), (".bss", 0x802407B8, 0x80240FD0),
        (".sdata", 0x80343BE0, 0x80343BE8),
        (".sbss", 0x803445D8, 0x80344624),
        (".sdata2", 0x803463F8, 0x80346470)]
EQUAL_MAPS = ({".data": {"gCtrl": 16}}, {".data": {"gCtrl": 16}})


def relocs(rows):
    return {".data": {offset: ("R_PPC_ADDR32", symbol, addend)
                      for offset, symbol, addend in rows}}


class UnpairableRelocations(unittest.TestCase):
    UNCLAIMED = [(0xC48, "lbl_80111F70", 0), (0xC4C, "lbl_80111F7C", 0)]
    CLAIMED = [(0xC44, "lbl_803463F8", 0)]

    def ours(self, rows):
        return relocs([(offset, "...rodata.0", n * 12)
                       for n, (offset, _s, _a) in enumerate(rows)])

    def test_a_pointer_into_an_unclaimed_address_is_unpairable(self):
        found = dc.unpairable_relocations(
            ".data", relocs(self.UNCLAIMED), self.ours(self.UNCLAIMED), RUNS)
        self.assertEqual([row["offset"] for row in found["rows"]],
                         [0xC48, 0xC4C])
        self.assertEqual(found["pairable"], [])
        self.assertEqual(found["unexplained"], [])

    def test_a_pointer_into_a_claimed_run_is_pairable_despite_the_name(self):
        found = dc.unpairable_relocations(
            ".data", relocs(self.CLAIMED),
            relocs([(0xC44, "@1", 0)]), RUNS)
        self.assertEqual(found["rows"], [])
        self.assertEqual([row["offset"] for row in found["pairable"]],
                         [0xC44])

    def test_identical_rows_are_not_differences_at_all(self):
        same = relocs([(0xC48, "gGameOptions", 4)])
        found = dc.unpairable_relocations(".data", same, same, RUNS)
        self.assertEqual((found["rows"], found["pairable"],
                          found["unexplained"]), ([], [], []))

    def test_an_unresolvable_target_symbol_is_unexplained_not_evidence(self):
        found = dc.unpairable_relocations(
            ".data", relocs([(0xC48, "someGlobal", 0)]),
            relocs([(0xC48, "otherGlobal", 0)]), RUNS)
        self.assertEqual(found["rows"], [])
        self.assertEqual(len(found["unexplained"]), 1)

    def test_an_offset_only_one_object_relocates_is_not_compared(self):
        found = dc.unpairable_relocations(
            ".data", relocs([(0xC48, "lbl_80111F70", 0)]), relocs([]), RUNS)
        self.assertEqual((found["rows"], found["unexplained"]), ([], []))


class ClassifyGap(unittest.TestCase):
    def call(self, **kw):
        options = {"target_map": EQUAL_MAPS[0], "ours_map": EQUAL_MAPS[1],
                   "target_relocs": relocs([(0xC48, "lbl_80111F70", 0)]),
                   "our_relocs": relocs([(0xC48, "...rodata.0", 0)]),
                   "bytes_equal": True, "runs": RUNS}
        options.update(kw)
        return dc.classify_gap(CONTROLS, ".data", **options)

    def test_equal_bytes_plus_an_unpairable_relocation_is_reloc(self):
        row = self.call()
        self.assertEqual(row["verdict"], "RELOC")
        self.assertEqual(len(row["unpairable_relocs"]), 1)

    def test_differing_bytes_are_never_reloc(self):
        self.assertEqual(self.call(bytes_equal=False)["verdict"], "BYTE")

    def test_unmeasured_byte_equality_is_never_reloc(self):
        # game/mb/mb_blit .ctors: the section is absent from one object.
        self.assertEqual(self.call(bytes_equal=None)["verdict"], "BYTE")

    def test_no_unpairable_relocation_leaves_the_byte_verdict(self):
        self.assertEqual(
            self.call(target_relocs=relocs([(0xC44, "lbl_803463F8", 0)]),
                      our_relocs=relocs([(0xC44, "@1", 0)]))["verdict"],
            "BYTE")

    def test_a_relocation_the_mechanism_cannot_explain_blocks_reloc(self):
        row = self.call(
            target_relocs=relocs([(0xC48, "lbl_80111F70", 0),
                                  (0xC50, "someGlobal", 0)]),
            our_relocs=relocs([(0xC48, "...rodata.0", 0),
                               (0xC50, "otherGlobal", 0)]))
        self.assertEqual(row["verdict"], "BYTE")
        self.assertEqual(len(row["relocs_not_explained"]), 1)

    def test_a_symbol_map_difference_still_outranks_reloc(self):
        row = self.call(target_map={".data": {"gCtrl": 16}},
                        ours_map={".data": {"gControls": 16}})
        self.assertEqual(row["verdict"], "BOUNDARY")
        self.assertEqual(row["unpairable_relocs"], [])

    def test_anonymous_names_still_outrank_reloc(self):
        row = self.call(target_map={".data": {"@1": 4}},
                        ours_map={".data": {"@9": 4}})
        self.assertEqual(row["verdict"], "ANONYMOUS")


class RelocationRowParsing(unittest.TestCase):
    DUMP = "\r\n".join([
        "obj:     file format elf32-powerpc",
        "",
        "RELOCATION RECORDS FOR [.data]:",
        "OFFSET   TYPE              VALUE",
        "00000c48 R_PPC_ADDR32      ...rodata.0",
        "00000c4c R_PPC_ADDR32      ...rodata.0+0x0000000c",
        "00000c50 R_PPC_ADDR32      lbl_80346400-0x00000004",
        "",
        "RELOCATION RECORDS FOR [.text]:",
        "0000001e R_PPC_ADDR16_HA   gGameOptions",
    ])

    def parse(self):
        datadiff = dc._datadiff()
        original = datadiff.dump_object
        datadiff.dump_object = lambda obj, *flags: self.DUMP
        try:
            return dc.relocation_rows("ignored")
        finally:
            datadiff.dump_object = original

    def test_symbols_addends_and_sections_are_all_recovered(self):
        table = self.parse()
        self.assertEqual(table[".data"][0xC48], ("R_PPC_ADDR32",
                                                 "...rodata.0", 0))
        self.assertEqual(table[".data"][0xC4C][2], 0xC)
        self.assertEqual(table[".data"][0xC50], ("R_PPC_ADDR32",
                                                 "lbl_80346400", -4))
        self.assertEqual(table[".text"][0x1E][1], "gGameOptions")

    def test_the_header_lines_are_not_read_as_relocations(self):
        self.assertEqual(len(self.parse()[".data"]), 3)


def live_objects(base):
    return ((ROOT / "build/GUNE5D/obj" / (base + ".o")).is_file()
            and (ROOT / "build/GUNE5D/src" / (base + ".o")).is_file())


@unittest.skipUnless(live_objects("game/game/controls")
                     and (ROOT / "build/GUNE5D/report.json").is_file(),
                     "controls objects or report.json are not built here")
class LiveControls(unittest.TestCase):
    """The regression fixture: lane H's eleven rows, and the agreement."""

    @classmethod
    def setUpClass(cls):
        cls.row = dc.classify_gap(CONTROLS, ".data")

    def test_controls_data_is_reloc_not_byte(self):
        self.assertEqual(self.row["verdict"], "RELOC")

    def test_the_eleven_rows_are_the_ones_lane_h_measured(self):
        offsets = [row["offset"] for row in self.row["unpairable_relocs"]]
        self.assertEqual(len(offsets), 11)
        self.assertEqual(offsets[:8],
                         [0xC48, 0xC4C, 0xC54, 0xC58, 0xC5C, 0xC60, 0xC64,
                          0xC68])
        self.assertEqual(offsets[8:], [0xC80, 0xC84, 0xC88])
        self.assertTrue(all(row["our_symbol"] == "...rodata.0"
                            for row in self.row["unpairable_relocs"]))

    def test_the_two_pairable_neighbours_are_the_sdata2_ones(self):
        self.assertEqual([row["offset"] for row in self.row["pairable_relocs"]],
                         [0xC44, 0xC50])

    def test_datadiff_and_data_credit_now_say_compatible_things(self):
        import datadiff
        note = datadiff.credit_note("game/game/controls.c",
                                    "game/game/controls", ".data")
        self.assertIsNotNone(note)
        self.assertIn("11 relocation(s)", note)
        self.assertIn("CLAIM work, not source work", note)

    def test_a_fully_paid_section_gets_no_credit_note(self):
        import datadiff
        self.assertIsNone(datadiff.credit_note("game/game/controls.c",
                                               "game/game/controls",
                                               ".sdata2"))


if __name__ == "__main__":
    unittest.main()
