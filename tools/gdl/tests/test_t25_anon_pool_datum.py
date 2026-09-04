"""T25 run-55 item 2: the anonymous-pool datum screen in wf_word_diff.

REPORTED (WV, run 54): "wf_word_diff printed RELOC-SYMBOL MISMATCH = 0 and
DECODE: REGFIELD-ONLY 53 for move_logic00 both before and after — it cannot
name-compare our anonymous @688/@689 against the target's lbl_ symbols."
Reproduced at 84f85a96a, where +0x1e4 relocates lbl_80346840 (pi) in the
target and @689 (2*pi) in ours while the headline reads 0.

`reloc_symbol_mismatches` drops such a row with a bare `continue`, so the
zero is reached by NOT LOOKING. `anonymous_datum_rows` decides them by DATUM
VALUE instead, with three exclusion classes each measured over the whole
image (see the function's docstring). These tests pin the exclusions, since
every one of them was found by READING the would-be-flagged set rather than
by counting it.

Monkeypatched over the two object readers, so no built object is required.
"""

import struct
import sys
import unittest
from pathlib import Path
from unittest import mock

TOOLS = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS / "composed_census"))

import fndiff                       # noqa: E402
import wf_word_diff as wd           # noqa: E402

LWZ = 0x80640000
PI = bytes.fromhex("400921fb54524550")
TWO_PI = bytes.fromhex("401921fb54524550")


def words(*values):
    return b"".join(struct.pack(">I", value) for value in values)


class AnonymousDatumRowTests(unittest.TestCase):
    def run_screen(self, t_sym, o_sym, t_val, o_val, positional=None):
        """One offset, identical words, differing relocation symbols."""
        stream = words(LWZ, LWZ)
        positional = positional or {}
        with mock.patch.object(wd, "_reloc_map", side_effect=[
                {0: ("R_PPC_EMB_SDA21", t_sym)},
                {0: ("R_PPC_EMB_SDA21", o_sym)}]), \
            mock.patch.object(wd, "target_object", return_value="t.o"), \
            mock.patch.object(wd, "our_object", return_value=("o.o", "k")), \
            mock.patch.object(fndiff, "resolve_reloc_symbol_positional",
                              side_effect=lambda s: positional.get(s)), \
            mock.patch.object(fndiff, "target_datum_bytes",
                              return_value=t_val), \
            mock.patch.object(fndiff, "ours_datum_bytes",
                              return_value=o_val):
            return wd.anonymous_datum_rows("game/x/y", "fn", stream, stream)

    def test_a_differing_datum_is_reported(self):
        """The move_logic00 shape: pi in the target, 2*pi in ours."""
        rows, tally = self.run_screen("lbl_80346840", "@689", PI, TWO_PI)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0][3], "VALUE-DIFFERS")
        self.assertEqual(rows[0][1:3], ("lbl_80346840", "@689"))
        self.assertEqual(tally["differs"], 1)

    def test_an_equal_datum_is_silent(self):
        """2,561 of the 2,741 dropped rows image-wide are this — the
        negative side, and it must stay out of the headline."""
        rows, tally = self.run_screen("lbl_80346840", "@689", PI, PI)
        self.assertEqual(rows, [])
        self.assertEqual(tally["value_equal"], 1)

    def test_differing_granularity_cancels(self):
        """dtk names a whole .rodata run with one symbol; the shorter entry
        being a PREFIX of the longer is fndiff's own rule, reused here."""
        rows, _tally = self.run_screen(
            "lbl_80115840", "@1274", PI + b"\x00\x11\x22", PI)
        self.assertEqual(rows, [])

    def test_a_jumptable_is_excluded(self):
        """122 of 145 differing rows image-wide, including byte-exact
        functions inside the 100%-matched zlib and MSL regions: the payload
        is linked branch addresses, zeros in our object until link."""
        rows, tally = self.run_screen(
            "jumptable_8011CCF4", "@981", b"\x80\x11\x00\x00", b"\0\0\0\0")
        self.assertEqual(rows, [])
        self.assertEqual(tally["jumptable"], 1)

    def test_a_named_jumptable_is_excluded_too(self):
        rows, tally = self.run_screen(
            "cardCmdJumptable", "@66", b"\x80\xdc\x48\x8c", b"\0\0\0\0")
        self.assertEqual(rows, [])
        self.assertEqual(tally["jumptable"], 1)

    def test_an_all_zero_datum_of_ours_is_an_unlinked_table(self):
        """13 rows image-wide (MSL/printf::longlong2str 132B, dolphin/dvd::
        stateReady 32B): the content is the LINKER's, not the compiler's."""
        rows, tally = self.run_screen(
            "lbl_80127BA8", "@170", b"\x80\xb6\x01\x14", b"\0\0\0\0")
        self.assertEqual(rows, [])
        self.assertEqual(tally["unlinked_table"], 1)

    def test_a_target_side_anonymous_name_is_excluded(self):
        """`@40` is defined TWICE in config/GUNE5D/symbols.txt at two
        addresses and sizes, so a name->address map answers with whichever
        came last — that is what made TRKNubWelcome (fndiff --clean MATCH,
        0 real diff lines) read as "OFF" vs "MetroTRK"."""
        rows, tally = self.run_screen("@40", "@35", b"OFF\0", b"MetroTRK\0")
        self.assertEqual(rows, [])
        self.assertEqual(tally["target_anonymous"], 1)

    def test_an_unreadable_datum_is_reported_not_dropped(self):
        """"Not decided" and "decided equal" are different findings; the
        whole defect being fixed here is the second standing in for the
        first."""
        rows, tally = self.run_screen("lbl_80346840", "@689", None, TWO_PI)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0][3], "NOT-DECIDABLE")
        self.assertEqual(tally["not_decidable"], 1)

    def test_a_name_comparable_row_stays_with_the_name_screen(self):
        """Both symbols resolve positionally: reloc_symbol_mismatches owns
        that row and this screen must not double-report it."""
        rows, tally = self.run_screen(
            "gGameplayPauseTimer", "lbl_803447B8", PI, TWO_PI,
            positional={"gGameplayPauseTimer": 0x80344770,
                        "lbl_803447B8": 0x803447B8})
        self.assertEqual(rows, [])
        self.assertEqual(sum(tally.values()), 0)

    def test_an_uncomparable_function_returns_none(self):
        with mock.patch.object(wd, "_reloc_map", return_value=None), \
            mock.patch.object(wd, "target_object", return_value="t.o"), \
                mock.patch.object(wd, "our_object", return_value=("o.o", "k")):
            rows, tally = wd.anonymous_datum_rows(
                "game/x/y", "fn", words(LWZ), words(LWZ))
        self.assertIsNone(rows)
        self.assertEqual(tally, {})


if __name__ == "__main__":
    unittest.main()
