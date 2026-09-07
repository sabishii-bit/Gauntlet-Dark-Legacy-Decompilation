"""Two-sided tests for tools/gdl/retire_audit.py.

The classifier is calibrated BOTH ways on hand-encoded PowerPC words, so each
of the five classes has at least one instruction that must land in it and at
least one neighbouring instruction that must NOT. Sitting next to those are
the three real retirements in this checkout's history, which must PASS at
their own commits, and two deliberately broken inputs, which must FAIL and
name the check that failed.

Live-population calibration, re-measurable with
`python tools/gdl/composed_census/ta_class_census.py`, over all 1461
functions present in both a raw body object and the dtk target at 50e8c254e
(85 of them count-asymmetric, so 1376 are word-comparable):

    register-assignment  6875 words in 222 functions
    scheduling           4683 words in 141 functions
    memory-access         389 words in  55 functions
    immediate             399 words in  87 functions
    relocation             14 words in   9 functions
    unmodelled relocation types: NONE

so no class is empty in practice and RELOC_TYPE_NAMES covers the live image.
"""

import copy
import json
from pathlib import Path
import subprocess
import sys
import unittest

GDL = Path(__file__).resolve().parents[1]
ROOT = GDL.parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(GDL))

from tools.gdl import retire_audit as ra  # noqa: E402

# Hand-encoded PowerPC words. Each is spelled out so a reader can check the
# encoding rather than trust the label.
ADDI_R3_R4_8 = 0x38640008    # addi r3,r4,8
ADDI_R5_R4_8 = 0x38A40008    # addi r5,r4,8      (register field only)
ADDI_R3_R4_12 = 0x3864000C   # addi r3,r4,12     (immediate field only)
LI_R3_8 = 0x38600008         # li   r3,8         (addi with rA = literal 0)
LWZ_R3_8_R4 = 0x80640008     # lwz  r3,8(r4)
LWZ_R3_8_R5 = 0x80650008     # lwz  r3,8(r5)     (base register only)
LWZ_R3_12_R4 = 0x8064000C    # lwz  r3,12(r4)    (displacement only)
LWZ_R3_8_R0 = 0x80600008     # lwz  r3,8(0)      (absolute: rA is literal 0)
B_PLUS_16 = 0x48000010       # b    +0x10
B_PLUS_20 = 0x48000014       # b    +0x14
LIS_R3_0 = 0x3C600000        # lis  r3,0         (unrelocated)
LIS_R3_8029 = 0x3C608029     # lis  r3,0x8029    (the linker's bits)
NOP = 0x60000000             # ori r0,r0,0


class ClassifierTests(unittest.TestCase):
    def klass(self, ours, target, types=()):
        return ra.classify_word(ours, target, types)[0]

    # -- positives ---------------------------------------------------------
    def test_register_field_difference_is_register_assignment(self):
        self.assertEqual(self.klass(ADDI_R3_R4_8, ADDI_R5_R4_8), "register-assignment")

    def test_base_register_of_a_load_is_still_register_assignment(self):
        """A renaming reaches a base register; that is a recolor, not an
        access change, and calling it memory-access would hide recolors."""
        self.assertEqual(self.klass(LWZ_R3_8_R4, LWZ_R3_8_R5), "register-assignment")

    def test_displacement_of_a_load_is_memory_access(self):
        self.assertEqual(self.klass(LWZ_R3_8_R4, LWZ_R3_12_R4), "memory-access")

    def test_absolute_versus_based_load_is_memory_access(self):
        self.assertEqual(self.klass(LWZ_R3_8_R0, LWZ_R3_8_R4), "memory-access")

    def test_plain_literal_difference_is_immediate(self):
        self.assertEqual(self.klass(ADDI_R3_R4_8, ADDI_R3_R4_12), "immediate")

    def test_li_versus_copy_is_immediate_not_register_assignment(self):
        """rA == 0 means the VALUE zero in addi, so no renaming reaches it."""
        self.assertEqual(self.klass(LI_R3_8, ADDI_R3_R4_8), "immediate")
        self.assertEqual(ra.classify_word(LI_R3_8, ADDI_R3_R4_8)[1], "RA-ZERO")

    def test_different_instruction_at_one_index_is_scheduling(self):
        self.assertEqual(self.klass(ADDI_R3_R4_8, LWZ_R3_8_R4), "scheduling")

    def test_branch_displacement_is_scheduling(self):
        self.assertEqual(self.klass(B_PLUS_16, B_PLUS_20), "scheduling")

    def test_relocated_field_is_the_linkers(self):
        self.assertEqual(self.klass(LIS_R3_0, LIS_R3_8029, ("R_PPC_ADDR16_HA",)),
                         "relocation")

    # -- negatives ---------------------------------------------------------
    def test_the_same_bits_without_a_relocation_are_not_the_linkers(self):
        """Identical word pair, relocation withdrawn: the class must move."""
        self.assertNotEqual(self.klass(LIS_R3_0, LIS_R3_8029), "relocation")
        self.assertEqual(self.klass(LIS_R3_0, LIS_R3_8029), "immediate")

    def test_a_relocation_that_does_not_own_the_bits_does_not_absorb_them(self):
        """ADDR16_LO patches the low halfword; the rD field is not its bits.

        Measured while writing this: R_PPC_REL24's mask (0x03FFFFFC) DOES
        cover the rD field, so a hypothetical addi carrying a REL24 would
        read `relocation`. That pairing cannot occur -- REL24 only ever sits
        on a branch -- but the distinction is why the negative control uses
        a relocation whose field really excludes the register slots.
        """
        self.assertEqual(self.klass(ADDI_R3_R4_8, ADDI_R5_R4_8, ("R_PPC_ADDR16_LO",)),
                         "register-assignment")

    def test_a_register_difference_is_never_immediate(self):
        self.assertNotEqual(self.klass(ADDI_R3_R4_8, ADDI_R5_R4_8), "immediate")

    def test_a_non_memory_immediate_is_never_memory_access(self):
        self.assertNotEqual(self.klass(ADDI_R3_R4_8, ADDI_R3_R4_12), "memory-access")

    def test_equal_words_are_never_reported(self):
        stream = ra.classify_stream(ADDI_R3_R4_8.to_bytes(4, "big") * 3,
                                    ADDI_R3_R4_8.to_bytes(4, "big") * 3)
        self.assertEqual(stream["differing_words"], 0)
        self.assertEqual(stream["rows"], [])
        self.assertEqual(set(stream["counts"].values()), {0})

    def test_every_class_name_is_one_of_the_five(self):
        for pair in ((ADDI_R3_R4_8, ADDI_R5_R4_8), (LWZ_R3_8_R4, LWZ_R3_12_R4),
                     (ADDI_R3_R4_8, ADDI_R3_R4_12), (B_PLUS_16, B_PLUS_20),
                     (ADDI_R3_R4_8, LWZ_R3_8_R4), (LI_R3_8, ADDI_R3_R4_8)):
            self.assertIn(ra.classify_word(*pair)[0], ra.CLASSES)


class StreamTests(unittest.TestCase):
    def test_count_asymmetry_is_a_determinate_scheduling_answer(self):
        ours = ADDI_R3_R4_8.to_bytes(4, "big") * 3
        target = ADDI_R3_R4_8.to_bytes(4, "big") * 2
        result = ra.classify_stream(ours, target)
        self.assertFalse(result["aligned"])
        self.assertTrue(result["count_asymmetric"])
        self.assertIsNone(result["differing_words"])
        self.assertIn("target 2, ours 3", result["summary"])

    def test_rows_carry_offset_words_class_and_decode(self):
        ours = ADDI_R3_R4_8.to_bytes(4, "big") + NOP.to_bytes(4, "big")
        target = ADDI_R5_R4_8.to_bytes(4, "big") + NOP.to_bytes(4, "big")
        rows = ra.classify_stream(ours, target)["rows"]
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["offset"], 0)
        self.assertEqual(rows[0]["ours"], "38640008")
        self.assertEqual(rows[0]["target"], "38a40008")
        self.assertEqual((rows[0]["class"], rows[0]["decode"]),
                         ("register-assignment", "REGFIELD-ONLY"))

    def test_relocation_types_are_indexed_by_instruction_not_by_byte(self):
        table = ra._reloc_types_by_index([[6, 6, "sym", 0]], [[4, 10, "fn", 0]])
        self.assertEqual(table, {1: ("R_PPC_ADDR16_HA", "R_PPC_REL24")})


class RelocationNormalizationTests(unittest.TestCase):
    def test_sda21_halfword_convention_is_normalized_on_our_side_only(self):
        ours = [[6, ra.SDA21, "lbl_1", 0]]
        target = [[4, ra.SDA21, "lbl_1", 0]]
        self.assertEqual(ra.normalize_relocations(ours, "ours"),
                         ra.normalize_relocations(target, "target"))

    def test_addr16_lo_is_not_normalized_because_both_sides_use_plus_two(self):
        ours = [[6, 4, "lbl_1", 0]]
        self.assertEqual(ra.normalize_relocations(ours, "ours"), [(6, 4, "lbl_1", 0)])

    def test_a_misplaced_sda21_refuses_rather_than_being_shifted(self):
        with self.assertRaises(ra.Refused) as caught:
            ra.normalize_relocations([[4, ra.SDA21, "lbl_1", 0]], "ours")
        self.assertIn("instruction+2", str(caught.exception))

    def test_an_unmodelled_relocation_type_refuses(self):
        with self.assertRaises(ra.Refused) as caught:
            ra.normalize_relocations([[4, 77, "lbl_1", 0]], "ours")
        self.assertIn("unmodelled relocation type 77", str(caught.exception))

    def test_a_different_symbol_at_one_position_is_not_equal(self):
        a = ra.normalize_relocations([[4, 10, "left", 0]], "ours")
        b = ra.normalize_relocations([[4, 10, "right", 0]], "target")
        self.assertNotEqual(a, b)

    def test_a_different_addend_at_one_position_is_not_equal(self):
        a = ra.normalize_relocations([[4, 10, "same", 0]], "ours")
        b = ra.normalize_relocations([[4, 10, "same", 4]], "target")
        self.assertNotEqual(a, b)


class MonotonicTests(unittest.TestCase):
    def test_a_word_that_becomes_the_target_is_monotonic(self):
        result = ra.monotonic_words(ADDI_R5_R4_8.to_bytes(4, "big"),
                                    ADDI_R3_R4_8.to_bytes(4, "big"),
                                    ADDI_R3_R4_8.to_bytes(4, "big"))
        self.assertEqual(result, {"changed_words": 1, "new_exact_words": 1})

    def test_a_word_that_moves_away_from_the_target_refuses(self):
        with self.assertRaises(ra.Refused) as caught:
            ra.monotonic_words(ADDI_R3_R4_8.to_bytes(4, "big"),
                               ADDI_R5_R4_8.to_bytes(4, "big"),
                               ADDI_R3_R4_8.to_bytes(4, "big"))
        self.assertIn("non-target bits", str(caught.exception))

    def test_a_different_length_refuses(self):
        with self.assertRaises(ra.Refused):
            ra.monotonic_words(b"\0" * 8, b"\0" * 4, b"\0" * 8)


def image(bodies, section=".text"):
    """A minimal object image with the shape object_image produces."""
    blob = bytearray()
    functions = {}
    for name, body in bodies.items():
        functions[name] = {"section": section, "offset": len(blob),
                           "size": len(body) // 2, "binding": 1,
                           "body": body, "relocations": []}
        blob += bytes.fromhex(body)
    return {"functions": functions,
            "sections": {section: {"type": 1, "flags": 6, "size": len(blob),
                                   "alignment": 4, "bytes": blob.hex(),
                                   "relocations": []}},
            "symbols": {}, "common": {}, "exception_records": []}


class CompareImagesTests(unittest.TestCase):
    def setUp(self):
        self.target = image({"a": "38640008", "b": "38640008"})
        self.before = image({"a": "38a40008", "b": "38640008"})
        self.after = image({"a": "38640008", "b": "38640008"})

    def test_identical_images_pass_with_nothing_permitted(self):
        result = ra.compare_images(self.after, self.after, set(), self.target)
        self.assertTrue(result["ok"])
        self.assertEqual(result["permitted"], {})

    def test_a_permitted_body_moving_toward_target_passes(self):
        result = ra.compare_images(self.before, self.after, {"a"}, self.target)
        self.assertTrue(result["ok"], result["differences"])
        self.assertEqual(result["permitted"]["a"],
                         {"changed_words": 1, "new_exact_words": 1})

    def test_the_same_change_unpermitted_fails_by_name(self):
        result = ra.compare_images(self.before, self.after, set(), self.target)
        self.assertFalse(result["ok"])
        self.assertIn("unpermitted function change: a", result["differences"])

    def test_a_permitted_body_moving_away_from_target_fails(self):
        away = image({"a": "38a40008", "b": "38640008"})
        result = ra.compare_images(self.after, away, {"a"}, self.target)
        self.assertFalse(result["ok"])
        self.assertTrue(any("non-target bits" in d for d in result["differences"]))

    def test_a_permitted_function_may_not_change_its_relocations(self):
        moved = copy.deepcopy(self.after)
        moved["functions"]["a"]["relocations"] = [[0, 10, "somewhere", 0]]
        result = ra.compare_images(self.before, moved, {"a"}, self.target)
        self.assertFalse(result["ok"])
        self.assertIn("permitted function a changed relocations", result["differences"])

    def test_a_data_section_change_fails_even_when_a_body_is_permitted(self):
        moved = copy.deepcopy(self.after)
        moved["sections"][".sdata2"] = {"type": 1, "flags": 3, "size": 4,
                                        "alignment": 4, "bytes": "00000001",
                                        "relocations": []}
        result = ra.compare_images(self.before, moved, {"a"}, self.target)
        self.assertFalse(result["ok"])
        self.assertIn("changed outside the permitted bodies: sections",
                      result["differences"])

    def test_a_common_allocation_change_fails(self):
        moved = copy.deepcopy(self.after)
        moved["common"]["gThing"] = {"size": 4, "alignment": 4, "binding": 1}
        result = ra.compare_images(self.after, moved, set(), self.target)
        self.assertFalse(result["ok"])
        self.assertIn("changed outside the permitted bodies: common",
                      result["differences"])

    def test_an_exception_record_change_fails(self):
        moved = copy.deepcopy(self.after)
        moved["exception_records"] = [{"function": "a"}]
        result = ra.compare_images(self.after, moved, set(), self.target)
        self.assertFalse(result["ok"])
        self.assertIn("changed outside the permitted bodies: exception_records",
                      result["differences"])

    def test_a_lost_function_fails_as_a_roster_change(self):
        fewer = image({"a": "38640008"})
        result = ra.compare_images(self.after, fewer, {"a", "b"}, self.target)
        self.assertFalse(result["ok"])
        self.assertEqual(result["removed"], ["b"])


class RederivedRuleTests(unittest.TestCase):
    RULE = {"function": "do_exit", "before_sha256": "aa", "after_sha256": "bb",
            "instruction_permutation": [{"before_relocations_sha256": "1",
                                         "after_relocations_sha256": "2"}]}

    def test_only_the_relocation_hashes_are_erasable(self):
        other = copy.deepcopy(self.RULE)
        other["instruction_permutation"][0]["before_relocations_sha256"] = "9"
        self.assertEqual(ra._erase_rederivable(self.RULE),
                         ra._erase_rederivable(other))

    def test_a_moved_body_hash_is_never_erased(self):
        other = copy.deepcopy(self.RULE)
        other["before_sha256"] = "cc"
        self.assertNotEqual(ra._erase_rederivable(self.RULE),
                            ra._erase_rederivable(other))

    def test_a_moved_window_is_never_erased(self):
        other = copy.deepcopy(self.RULE)
        other["instruction_permutation"][0]["order"] = [1, 0]
        self.assertNotEqual(ra._erase_rederivable(self.RULE),
                            ra._erase_rederivable(other))


class ObjectImageTests(unittest.TestCase):
    """The image, read off a real object in this checkout."""

    UNIT = "game/sys/memcard"

    def setUp(self):
        self.path = ROOT / "build/GUNE5D/obj" / (self.UNIT + ".o")
        if not self.path.exists():
            self.skipTest("target object not split in this checkout")
        self.image = ra.object_image(self.path)

    def test_the_image_has_functions_sections_and_exception_records(self):
        self.assertGreater(len(self.image["functions"]), 20)
        self.assertIn(".text", self.image["sections"])
        self.assertIn("memCardErrorPrompt", self.image["functions"])

    def test_bodies_agree_with_the_shipped_inventory_helper(self):
        """Two readers of one object must not disagree about its bytes."""
        from tools.gdl.composed_census.r67_runtime_visibility_probe import inventory
        functions, _sections = inventory(self.path)
        self.assertEqual(sorted(functions), sorted(self.image["functions"]))
        for name, row in functions.items():
            self.assertEqual(row["body"], self.image["functions"][name]["body"], name)
            self.assertEqual(row["size"], self.image["functions"][name]["size"], name)

    def test_the_image_is_json_round_trippable(self):
        self.assertEqual(json.loads(json.dumps(self.image)), self.image)

    def test_a_flipped_sibling_byte_is_detected(self):
        """The deliberately broken input, built from a real object."""
        broken = copy.deepcopy(self.image)
        name = next(n for n in broken["functions"] if n != "memCardErrorPrompt")
        body = bytearray.fromhex(broken["functions"][name]["body"])
        body[0] ^= 0x01
        broken["functions"][name]["body"] = body.hex()
        result = ra.compare_images(self.image, broken, set(), self.image)
        self.assertFalse(result["ok"])
        self.assertIn("unpermitted function change: " + name, result["differences"])


def run_cli(*args):
    proc = subprocess.run([sys.executable, str(GDL / "retire_audit.py"), *args],
                          capture_output=True, text=True, cwd=str(ROOT))
    return proc


class LiveRetirementTests(unittest.TestCase):
    """The three retirements in this branch's history, and two broken inputs.

    Each PASS is an end-to-end run: three real compiles through the Ninja
    edge, two real postprocessor replays and the DOL hash check.
    """

    @classmethod
    def setUpClass(cls):
        if not (ROOT / "build/GUNE5D/build_edges.json").exists():
            raise unittest.SkipTest("checkout is not configured/built")
        if subprocess.run(["git", "cat-file", "-e", "e7f9d740b~1"],
                          cwd=str(ROOT), capture_output=True).returncode:
            raise unittest.SkipTest("retirement history is not present")

    def test_memcard_error_prompt_passes_at_its_commit(self):
        proc = run_cli("game/sys/memcard", "memCardErrorPrompt",
                       "--before-ref", "e7f9d740b~1")
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertTrue(proc.stdout.startswith("PASS:"), proc.stdout[:400])

    def test_mb_camera_update_passes_at_its_commit(self):
        proc = run_cli("game/mb/mb_camera", "MBCameraUpdate",
                       "--before-ref", "30582a770~1")
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertTrue(proc.stdout.startswith("PASS:"), proc.stdout[:400])

    def test_exp_to_level_passes_with_its_declared_caller_and_rederive(self):
        proc = run_cli("game/game/player", "ExpToLevel",
                       "--before-ref", "37f9daac5~1",
                       "--allow-changed-sibling", "player_store_in_save",
                       "--allow-rederived-rule", "do_exit")
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertTrue(proc.stdout.startswith("PASS:"), proc.stdout[:400])

    def test_exp_to_level_fails_without_declaring_the_changed_caller(self):
        """The permissions are not decoration: withdraw them and it fails."""
        proc = run_cli("game/game/player", "ExpToLevel", "--before-ref", "37f9daac5~1")
        self.assertEqual(proc.returncode, 1)
        self.assertTrue(proc.stdout.startswith("FAIL:"))
        self.assertIn("sibling_bodies", proc.stdout)
        self.assertIn("player_store_in_save", proc.stdout)

    def test_a_still_rule_served_function_fails_and_classifies_its_words(self):
        # `--before-ref HEAD` until run-58 item 5; a before-ref holding the
        # current source is now REFUSED, so this uses a revision at which
        # memcard.c genuinely differs. The classification under test does
        # not depend on the ref at all.
        proc = run_cli("game/sys/memcard", "add_vmu_file",
                       "--before-ref", "e7f9d740b~1")
        self.assertEqual(proc.returncode, 1)
        self.assertTrue(proc.stdout.startswith("FAIL:"))
        # Membership, not the exact list: against a genuine prior revision
        # memCardErrorPrompt is also an undeclared changed sibling (that IS
        # the retirement at e7f9d740b), so sibling_bodies joins the two
        # checks this test is about.
        failing = proc.stdout.split("FAILING CHECK(S): ")[1].splitlines()[0]
        self.assertIn("native_body", failing)
        self.assertIn("rule_delta", failing)
        # The five words this rule's own recorded mechanism names.
        self.assertIn("register-assignment 5", proc.stdout)
        for offset in ("+0x0014", "+0x0020", "+0x0024", "+0x0028", "+0x002c"):
            self.assertIn(offset, proc.stdout)
        self.assertIn("STILL rule-served", proc.stdout)

    def test_an_unknown_function_refuses_rather_than_failing(self):
        proc = run_cli("game/sys/memcard", "no_such_function_at_all",
                       "--before-ref", "e7f9d740b~1")
        self.assertEqual(proc.returncode, 2)
        self.assertTrue(proc.stdout.startswith("REFUSED:"))
        self.assertIn("no_such_function_at_all", proc.stdout)

    # --- run-58 item 5: the before-ref is an INPUT, and it is checked ----

    def test_a_before_ref_holding_the_current_source_refuses(self):
        """The eaten-caret form, end to end.

        `--before-ref e7f9d740b^` arrives as `e7f9d740b`, which is the
        retirement commit itself; memcard.c is byte-identical there to the
        working tree. Before this check the run reported
        `[PASS] before_image / source_changed: false` and then
        `FAIL: rule_delta` at exit 1 -- a named check blamed for a typed
        argument, after three compiles and a link.
        """
        proc = run_cli("game/sys/memcard", "memCardErrorPrompt",
                       "--before-ref", "e7f9d740b")
        self.assertEqual(proc.returncode, 2, proc.stdout[:400])
        self.assertTrue(proc.stdout.startswith("REFUSED:"), proc.stdout[:400])
        self.assertIn("BYTE-IDENTICAL", proc.stdout)
        self.assertIn("e7f9d740b~1", proc.stdout)
        self.assertNotIn("rule_delta", proc.stdout)

    def test_head_as_a_before_ref_refuses_and_says_so(self):
        proc = run_cli("game/sys/memcard", "memCardErrorPrompt",
                       "--before-ref", "HEAD")
        self.assertEqual(proc.returncode, 2, proc.stdout[:400])
        self.assertIn("resolves to HEAD itself", proc.stdout)

    def test_a_ref_naming_a_tree_refuses_as_a_tree(self):
        tree = subprocess.run(["git", "rev-parse", "HEAD^{tree}"],
                              cwd=str(ROOT), capture_output=True, text=True)
        if tree.returncode:
            # `^{}` peeling is broken in this git; read the tree id from the
            # commit object instead, which is the point of the item.
            body = subprocess.run(["git", "cat-file", "commit", "HEAD"],
                                  cwd=str(ROOT), capture_output=True,
                                  text=True).stdout
            tree_id = body.split("\n", 1)[0].split()[1]
        else:
            tree_id = tree.stdout.strip()
        proc = run_cli("game/sys/memcard", "memCardErrorPrompt",
                       "--before-ref", tree_id)
        self.assertEqual(proc.returncode, 2, proc.stdout[:400])
        self.assertIn("names a tree", proc.stdout)

    def test_a_genuine_parent_ref_is_not_refused(self):
        """THE NEGATIVE SIDE: `~1` still audits, and still PASSes.

        A guard that refused everything would satisfy the three tests above
        and destroy the tool; this is the same retirement the caret form
        breaks, spelled correctly.
        """
        proc = run_cli("game/sys/memcard", "memCardErrorPrompt",
                       "--before-ref", "e7f9d740b~1")
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertTrue(proc.stdout.startswith("PASS:"), proc.stdout[:400])

    def test_cat_file_resolves_the_revisions_rev_parse_peeling_rejects(self):
        """The first half of item 5, as a live measurement of this git.

        `rev-parse --verify <rev>^{commit}` fails here for EVERY revision
        spelling -- so a ref check written on it would reject valid input --
        while `cat-file` resolves the same revision.
        """
        head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=str(ROOT),
                              capture_output=True, text=True).stdout.strip()
        peeled = subprocess.run(["git", "rev-parse", "--verify",
                                 head + "^{commit}"],
                                cwd=str(ROOT), capture_output=True)
        oid, kind = ra.resolve_commit(head)
        self.assertEqual((oid, kind), (head, "commit"))
        if peeled.returncode:
            self.assertIn(b"Needed a single revision", peeled.stderr)

    def test_an_unresolvable_ref_refuses_by_name(self):
        with self.assertRaises(ra.Refused) as caught:
            ra.resolve_commit("definitely-not-a-ref")
        self.assertIn("does not resolve", str(caught.exception))

    def test_an_unknown_unit_refuses(self):
        proc = run_cli("game/nope/nope", "whatever", "--before-ref", "HEAD")
        self.assertEqual(proc.returncode, 2)
        self.assertIn("no Ninja compile edge", proc.stdout)

    def test_an_unresolvable_before_ref_refuses(self):
        proc = run_cli("game/sys/memcard", "memCardErrorPrompt",
                       "--before-ref", "definitely-not-a-ref")
        self.assertEqual(proc.returncode, 2)
        self.assertIn("does not resolve", proc.stdout)

    def test_before_ref_is_mandatory(self):
        proc = run_cli("game/sys/memcard", "memCardErrorPrompt")
        self.assertEqual(proc.returncode, 2)
        self.assertIn("--before-ref", proc.stderr)

    def test_out_must_stay_under_build(self):
        proc = run_cli("game/sys/memcard", "memCardErrorPrompt",
                       "--before-ref", "HEAD", "--out", "certificate.json")
        self.assertEqual(proc.returncode, 2)
        self.assertIn("build/", proc.stderr)


if __name__ == "__main__":
    unittest.main()
