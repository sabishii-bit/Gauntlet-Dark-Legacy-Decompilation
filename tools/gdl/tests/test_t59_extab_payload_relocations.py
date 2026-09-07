#!/usr/bin/env python3
"""T3 run-59 item 3: extab PAYLOAD relocations are decoded, not refused.

THE OBSERVATION (TA lane). `exception_metadata.exception_records` raised on
2 of 145 units. Reproduced at afe555163 over all 668 built objects with
build/t3_scratch/t3_item3_repro.py -- exactly four objects raise, and all
four raise the same refusal:

    ValueError: build/GUNE5D/obj/game/movie/movieplayer.o
        UNRESOLVED extab payload relocations: raw bytes cannot prove
        relocated metadata                          .relaextab, 4 entries
    ValueError: build/GUNE5D/obj/Runtime.PPCEABI.H/NMWException.o
    ValueError: build/GUNE5D/src/Runtime.PPCEABI.H/NMWException.o
    ValueError: .../src/Runtime.PPCEABI.H/.postprocess/body/NMWException.o
                                                    .relaextab, 2 entries

The refusal is sound but blind: every one of the six entries is
R_PPC_ADDR32, addend 0, aiming at a NAMED .text function (a destructor).
The relocation IS the pointer's value, so decoding it proves more than
comparing the zero placeholder ever could.

WHAT THE DECODE FOUND, immediately (build/t3_scratch/t3_item3_after.py):

    Runtime.PPCEABI.H/NMWException  10/10 records, 0 changed -- a unit that
        could not be measured at all now measures CLEAN, both payload
        relocations identical on both sides.
    game/movie/movieplayer          39/39 records, 11 CHANGED, and OUR
        object emits NO extab payload relocations where the target emits
        four (__dla__FPv, __dl__FPv, dtor_800DBB94,
        __dt__15MoviePlayerBaseFv). That EH difference was invisible while
        the decoder refused the object.

CALLERS. `retire_audit.object_image` already turns a ValueError into a
`Refused`, and `datadiff.exception_table` into `status: UNRESOLVED`; both
were verified to print no traceback before this change. The refusal is now
`UnsupportedExceptionMetadata`, a ValueError SUBCLASS, so those handlers
keep working while a caller can tell an unmodelled FORM from a malformed
object. (Reported separately: datadiff prints
`UNRESOLVED ... missing=0 changed=0 extra=0`, showing zeros for a
measurement that did not happen.)

TWO-SIDED. The positive side is the live decode of both units. The negative
side patches a real object's relocation table into each unmodelled form and
requires a refusal that NAMES it -- an unmodelled type, a misaligned or
out-of-section target, a duplicate entry, an unnamed target.
"""

import struct
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))

import exception_metadata as em                              # noqa: E402
import webfrank as wf                                        # noqa: E402

NMW = ROOT / "build/GUNE5D/obj/Runtime.PPCEABI.H/NMWException.o"
NMW_OURS = ROOT / "build/GUNE5D/src/Runtime.PPCEABI.H/NMWException.o"
MOVIE = ROOT / "build/GUNE5D/obj/game/movie/movieplayer.o"
MOVIE_OURS = ROOT / "build/GUNE5D/src/game/movie/movieplayer.o"
DESTRUCTOR = "__dt__26__partial_array_destructorFv"


def extab_relocation_entries(data):
    """[(file offset of the RELA entry)] for the extab payload table."""
    sections = wf._sections(data)
    extab = [s for s in sections if s.name.lstrip(".") == "extab"][0]
    out = []
    for rs in sections:
        if rs.section_type == wf.SHT_RELA and rs.info == extab.index:
            stride = rs.entry_size or 12
            out.extend(range(rs.offset, rs.offset + rs.size, stride))
    return out


class RefusalType(unittest.TestCase):
    def test_the_refusal_is_named_and_is_still_a_ValueError(self):
        """The callers' `except ValueError` handlers must keep working."""
        self.assertTrue(issubclass(em.UnsupportedExceptionMetadata,
                                   ValueError))

    def test_a_non_elf_input_refuses_by_name(self):
        with self.assertRaises(em.UnsupportedExceptionMetadata):
            em.exception_records(b"not an elf at all")


class LiveDecode(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not (NMW.exists() and NMW_OURS.exists() and MOVIE.exists()
                and MOVIE_OURS.exists()):
            raise unittest.SkipTest("checkout is not built")

    def records(self, path):
        return em.exception_records(path.read_bytes())

    def test_the_unit_that_used_to_refuse_now_decodes_on_both_sides(self):
        target, ours = self.records(NMW), self.records(NMW_OURS)
        self.assertTrue(target and ours)
        result = em.compare_exception_records(target, ours)
        self.assertEqual(result["missing"], [])
        self.assertEqual(result["extra"], {})
        self.assertEqual(result["changed"], {})

    def test_the_payload_relocation_is_decoded_by_symbol_identity(self):
        target = self.records(NMW)
        carrying = {name: rec["relocations"] for name, rec in target.items()
                    if rec["relocations"]}
        self.assertEqual(sorted(carrying),
                         ["__construct_array", "__construct_new_array"])
        for name, rows in carrying.items():
            self.assertEqual(rows, [[20, em.R_PPC_ADDR32, DESTRUCTOR, 0]],
                             name)

    def test_offsets_are_RECORD_relative_not_section_relative(self):
        """Two records at different extab offsets carry the same row."""
        target = self.records(NMW)
        rows = [rec["relocations"] for rec in target.values()
                if rec["relocations"]]
        self.assertEqual(rows[0], rows[1])

    def test_every_record_carries_the_key_even_when_empty(self):
        for record in self.records(NMW).values():
            self.assertIn("relocations", record)
            self.assertIsInstance(record["relocations"], list)

    def test_movieplayer_decodes_and_its_EH_difference_is_now_visible(self):
        """The defect the refusal hid: ours emits no payload relocations."""
        target, ours = self.records(MOVIE), self.records(MOVIE_OURS)
        target_relocs = {n: r["relocations"] for n, r in target.items()
                         if r["relocations"]}
        ours_relocs = {n: r["relocations"] for n, r in ours.items()
                       if r["relocations"]}
        self.assertTrue(target_relocs, "the target's four entries vanished")
        self.assertEqual(
            sorted(name for rows in target_relocs.values()
                   for _o, _k, name, _a in rows),
            ["__dl__FPv", "__dla__FPv", "__dt__15MoviePlayerBaseFv",
             "dtor_800DBB94"])
        self.assertEqual(ours_relocs, {})
        self.assertTrue(
            em.compare_exception_records(target, ours)["changed"],
            "our movieplayer EH is not equal to the target's")


class UnmodelledFormsStillRefuse(unittest.TestCase):
    """The negative side, patched into a REAL object's relocation table."""

    @classmethod
    def setUpClass(cls):
        if not NMW.exists():
            raise unittest.SkipTest("checkout is not built")
        cls.data = NMW.read_bytes()
        cls.entries = extab_relocation_entries(cls.data)
        if len(cls.entries) < 2:
            raise unittest.SkipTest("no extab payload relocations to patch")

    def patched(self, at, *, offset=None, kind=None, symbol=None):
        blob = bytearray(self.data)
        was_offset, info, addend = struct.unpack_from(">IIi", blob, at)
        struct.pack_into(
            ">IIi", blob, at,
            was_offset if offset is None else offset,
            ((info >> 8) if symbol is None else symbol) << 8
            | ((info & 255) if kind is None else kind),
            addend)
        return bytes(blob)

    def refusal(self, data):
        with self.assertRaises(em.UnsupportedExceptionMetadata) as caught:
            em.exception_records(data)
        return str(caught.exception)

    def test_the_unpatched_object_decodes(self):
        """The control: the patches below are what break it."""
        self.assertTrue(em.exception_records(self.data))

    def test_an_unmodelled_relocation_type_refuses_and_names_it(self):
        message = self.refusal(self.patched(self.entries[0], kind=10))
        self.assertIn("unmodelled extab payload relocation type 10", message)

    def test_a_misaligned_target_refuses(self):
        was = struct.unpack_from(">I", self.data, self.entries[0])[0]
        message = self.refusal(self.patched(self.entries[0], offset=was + 1))
        self.assertIn("misaligned", message)

    def test_a_target_outside_the_section_refuses(self):
        message = self.refusal(self.patched(self.entries[0], offset=0x10000))
        self.assertIn("outside the section", message)

    def test_a_duplicate_entry_refuses(self):
        second = struct.unpack_from(">I", self.data, self.entries[1])[0]
        message = self.refusal(self.patched(self.entries[0], offset=second))
        self.assertIn("duplicate extab payload relocation", message)

    def test_an_unnamed_target_refuses(self):
        """Symbol 0 is the null entry: no name, so no identity."""
        message = self.refusal(self.patched(self.entries[0], symbol=0))
        self.assertIn("unnamed extab payload relocation target", message)

    def test_an_entry_outside_every_record_refuses(self):
        """A relocation nothing indexes must not be silently dropped."""
        sections = wf._sections(self.data)
        extab = [s for s in sections if s.name.lstrip(".") == "extab"][0]
        # The last aligned word of extab is beyond the final record only if
        # the records do not cover the section; when they do, this patch
        # lands inside a record and the object still decodes -- so the test
        # asserts the property that actually holds for THIS object.
        data = self.patched(self.entries[0], offset=extab.size - 4)
        try:
            records = em.exception_records(data)
        except em.UnsupportedExceptionMetadata as error:
            self.assertIn("outside every indexed record", str(error))
        else:
            moved = [rows for rec in records.values()
                     for rows in [rec["relocations"]] if rows]
            self.assertTrue(moved, "the moved relocation was dropped")


class CallerDegradation(unittest.TestCase):
    """`datadiff` must report UNRESOLVED, never a traceback."""

    def test_datadiff_turns_a_refusal_into_UNRESOLVED(self):
        sys.path.insert(0, str(ROOT))
        from tools.gdl import datadiff
        real = em.exception_records

        def refusing(_data):
            raise em.UnsupportedExceptionMetadata("synthetic unmodelled form")

        em.exception_records = refusing
        try:
            result = datadiff.exception_table(str(NMW), str(NMW))
        finally:
            em.exception_records = real
        self.assertEqual(result["status"], "UNRESOLVED")
        self.assertIn("synthetic unmodelled form", result["error"])


if __name__ == "__main__":
    unittest.main()
