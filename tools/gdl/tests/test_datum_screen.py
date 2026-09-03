"""fndiff's merged DATUM screen (run 43 item 2).

Two shipped composed_census tools answered "does this function read a
different pool datum than retail?" and each carried half the answer:
cq_raw_pool_screen read the RAW compiler object (so a webfrank RULE artifact
could not read as a source defect) and cr_datum_screen carried the
calibrated false-positive suppressions. The screen now lives in fndiff and
both delegate to it; these tests pin the suppressions, because every one of
them was forced by a measured false positive and a regression in any of them
turns the screen back into a list of candidates advertised as a list of bugs.
"""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import fndiff  # noqa: E402

TARGET_OBJ = "build/GUNE5D/obj/game/test/foo.o"
OURS_OBJ = "build/GUNE5D/src/game/test/foo.o"

BIAS = bytes.fromhex("4330000080000000")
PI = bytes.fromhex("400921fb54442d18")
TWO_PI = bytes.fromhex("401921fb54442d18")
STRING_RUN = b"COIN_BRONZE\x00COIN_SILVER\x00"
STRING_ONE = b"COIN_BRONZE\x00"


def lines(*symbols):
    """A function body: one load per symbol, each with its relocation row."""
    out = []
    for symbol in symbols:
        out.append("lfd     f1,0(0)")
        out.append(f"    R_PPC_EMB_SDA21 {symbol}")
    return out


class DatumScreenTests(unittest.TestCase):
    # symbols.txt view: name -> (section, address, size)
    SYMBOLS = {
        "sBias": (".sdata2", 0x80348370, 8),
        "sPi": (".sdata2", 0x80348380, 8),
        "coin_names": (".rodata", 0x80113AE0, len(STRING_RUN)),
        "atree_scroll": (".bss", 0x8023D000, 384),
        "gFlags": (".sdata", 0x803445C8, 8),
    }
    RETAIL = {
        "sBias": BIAS, "sPi": PI, "coin_names": STRING_RUN,
        "atree_scroll": None, "gFlags": None,
    }
    # our object's local data objects: name -> (section, size, bytes|None)
    OURS = {
        "@193": (".sdata2", 8, BIAS),
        "@33": (".rodata", len(STRING_ONE), STRING_ONE),
        "@50": (".sdata2", 8, TWO_PI),
        "jumptable_1": (".rodata", 8, bytes(8)),
    }
    TARGET_LOCALS = {
        "jumptable_80120B4C": (".rodata", 8,
                               bytes.fromhex("801000001080100004")[:8]),
    }

    def setUp(self):
        self._symbol_table = fndiff.symbol_table
        self._datum_entry = fndiff.target_datum_entry
        self._datum_table = fndiff.object_datum_table
        fndiff.symbol_table = lambda: self.SYMBOLS
        fndiff.target_datum_entry = self._entry
        fndiff.object_datum_table = self._table

    def tearDown(self):
        fndiff.symbol_table = self._symbol_table
        fndiff.target_datum_entry = self._datum_entry
        fndiff.object_datum_table = self._datum_table

    def _entry(self, name, cap=fndiff.DATUM_PREFIX_BYTES):
        known = self.SYMBOLS.get(name)
        if known is None:
            return None
        blob = self.RETAIL.get(name)
        return known[0], known[2], (blob[:cap] if blob else None)

    def _table(self, objfile, readable=None):
        return dict(self.TARGET_LOCALS) if str(objfile) == TARGET_OBJ \
            else dict(self.OURS)

    def screen(self, target_symbols, ours_symbols, **kwargs):
        return fndiff.datum_screen_from_lines(
            lines(*target_symbols), lines(*ours_symbols),
            TARGET_OBJ, OURS_OBJ, **kwargs)

    def test_named_and_anonymous_spellings_of_one_constant_are_equal(self):
        """show_gold's shape: retail names the bias, we pool it as @193."""
        result = self.screen(["sBias"], ["@193"])
        self.assertEqual(result["verdict"], "VALUE-EQUAL")

    def test_a_different_constant_is_a_delta_named_by_value(self):
        result = self.screen(["sPi"], ["@50"])
        self.assertEqual(result["verdict"], "VALUE-DELTA")
        self.assertIn("B:" + PI.hex(), result["target_only"])
        self.assertIn("B:" + TWO_PI.hex(), result["ours_only"])

    def test_prefix_granularity_is_not_a_defect(self):
        """dtk names a whole .rodata run with ONE symbol; MWCC emits one @N
        per literal, so the shorter entry is a PREFIX of the longer."""
        result = self.screen(["coin_names"], ["@33"])
        self.assertEqual(result["verdict"], "VALUE-EQUAL")

    def test_a_relocation_filled_table_is_a_representation_difference(self):
        """Resolved addresses in the retail image, zeros in our object."""
        result = self.screen(["jumptable_80120B4C"], ["jumptable_1"])
        self.assertEqual(result["verdict"], "VALUE-EQUAL")

    def test_an_addend_keys_to_the_same_datum_as_the_named_symbol(self):
        result = self.screen(["gFlags+0x4"], ["gFlags+0x4"])
        self.assertEqual(result["verdict"], "VALUE-EQUAL")
        self.assertEqual(list(self.screen(["gFlags"], ["gFlags+0x4"])
                              ["target_only"]), ["A:0x803445C8"])

    def test_a_transposition_is_invisible_by_construction(self):
        """The KNOWN LIMIT: a multiset cannot see a swap. Recorded, not fixed
        — move_logic00's swapped pi/2pi reads VALUE-EQUAL here, and the
        wrong-OPERAND census is the complement that sees it."""
        result = self.screen(["sPi", "sBias"], ["sBias", "sPi"])
        self.assertEqual(result["verdict"], "VALUE-EQUAL")

    def test_a_placeholder_name_is_a_delta_by_default(self):
        """Our `extern u8 lbl_8023D000[]` vs the splitter's `atree_scroll`.

        Measured image-wide (1,702 functions): resolving the placeholder
        removes one whole false-positive function (pb_diag::pbDiagDrawInfo,
        4 rows) and re-keys 3 further rows, and adds no new delta. It stays
        OFF by default so a lane mid-run keeps the verdicts it started with.
        """
        result = self.screen(["atree_scroll"], ["lbl_8023D000"])
        self.assertEqual(result["verdict"], "VALUE-DELTA")
        self.assertIn("N:lbl_8023D000", result["ours_only"])

    def test_resolving_the_placeholder_cancels_it(self):
        result = self.screen(["atree_scroll"], ["lbl_8023D000"],
                             placeholder_addresses=True)
        self.assertEqual(result["verdict"], "VALUE-EQUAL")

    def test_counts_are_reported_for_both_sides(self):
        result = self.screen(["sBias", "sPi"], ["@193"])
        self.assertEqual((result["target_relocs"], result["ours_relocs"]),
                         (2, 1))


class PlaceholderAddressTests(unittest.TestCase):
    def test_dtk_spellings_carry_their_address(self):
        for name, value in (("lbl_8023D000", 0x8023D000),
                            ("jumptable_80120B4C", 0x80120B4C),
                            ("jtbl_800A1B2C", 0x800A1B2C)):
            self.assertEqual(fndiff.placeholder_address(name), value)

    def test_a_real_name_carries_none(self):
        for name in ("atree_scroll", "@193", "sBias", "lbl_", "lbl_zz"):
            self.assertIsNone(fndiff.placeholder_address(name))


class OursObjectPathTests(unittest.TestCase):
    def test_raw_prefers_the_pre_postprocess_body_when_it_exists(self):
        path, used = fndiff.ours_object_path("game/enemy/enemy", raw=True)
        body = Path("build/GUNE5D/src/game/enemy/.postprocess/body/enemy.o")
        if body.is_file():
            self.assertEqual((Path(path), used), (body, True))
        else:                      # unpinned TU: the plain object IS raw
            self.assertFalse(used)

    def test_without_raw_the_plain_object_is_used(self):
        path, used = fndiff.ours_object_path("game/enemy/enemy")
        self.assertFalse(used)
        self.assertEqual(Path(path),
                         Path("build/GUNE5D/src/game/enemy/enemy.o"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
