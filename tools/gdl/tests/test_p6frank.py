import copy
import hashlib
import struct
import unittest

from tools.gdl.p6frank import (
    NamedRelocation,
    encode_direct_branch,
    load_rule,
    transform_fixed_carrier,
    validate_siblings,
)


def words(*values):
    return b"".join(struct.pack(">I", value) for value in values)


class FixedCarrierTests(unittest.TestCase):
    def setUp(self):
        # The existing branch at +0x14 rotates to +0x08 across two data atoms
        # and the one audited loop branch at +0x10.  No atom is inserted.
        addi = (14 << 26) | (3 << 21) | (3 << 16) | 1
        lwz = (32 << 26) | (3 << 21) | (3 << 16)
        cmplwi = (10 << 26) | (3 << 16)
        li_zero = (14 << 26) | (3 << 21)
        self.current = words(
            addi,
            encode_direct_branch("b", 0x04, 0x10),
            lwz,
            cmplwi,
            encode_direct_branch("bne", 0x10, 0x00),
            encode_direct_branch("b", 0x14, 0x1C),
            li_zero,
        )
        self.target = words(
            addi,
            encode_direct_branch("b", 0x04, 0x14),
            encode_direct_branch("b", 0x08, 0x1C),
            lwz,
            cmplwi,
            encode_direct_branch("bne", 0x14, 0x00),
            li_zero,
        )
        self.relocations = [NamedRelocation(0, 10, "callee", 0)]
        self.rule = {
            "function_size": len(self.current),
            "before_sha256": hashlib.sha256(self.current).hexdigest(),
            "after_sha256": hashlib.sha256(self.target).hexdigest(),
            "move": {"from": 0x14, "to": 0x08, "crosses": [0x10]},
            "changed_offsets": [0x04, 0x08, 0x14],
            "reencode": [
                {"at": 0x04, "kind": "b", "destination": 0x14},
                {"at": 0x08, "kind": "b", "destination": 0x1C},
                {"at": 0x14, "kind": "bne", "destination": 0x00},
            ],
            "before_edges": [
                {"at": 0x04, "kind": "b", "destination": 0x10},
                {"at": 0x10, "kind": "bne", "destination": 0x00},
                {"at": 0x14, "kind": "b", "destination": 0x1C},
            ],
            "after_edges": [
                {"at": 0x04, "kind": "b", "destination": 0x14},
                {"at": 0x08, "kind": "b", "destination": 0x1C},
                {"at": 0x14, "kind": "bne", "destination": 0x00},
            ],
            "function_relocations": [
                {"at": 0, "type": 10, "symbol": "callee", "addend": 0}
            ],
        }

    def transform(self, **changes):
        rule = copy.deepcopy(self.rule)
        rule.update(changes.pop("rule", {}))
        return transform_fixed_carrier(
            changes.pop("current", self.current),
            changes.pop("target", self.target),
            changes.pop("relocations", self.relocations),
            rule,
        )

    def test_success_is_fixed_size_and_target_exact(self):
        output = self.transform()
        self.assertEqual(len(output), len(self.current))
        self.assertEqual(output, self.target)

    def test_bad_input_and_target_hashes_fail_closed(self):
        with self.assertRaisesRegex(ValueError, "input hash changed"):
            self.transform(rule={"before_sha256": "0" * 64})
        with self.assertRaisesRegex(ValueError, "target function hash changed"):
            self.transform(rule={"after_sha256": "0" * 64})

    def test_wrong_control_opcode_or_edge_is_rejected(self):
        changed = bytearray(self.current)
        struct.pack_into(">I", changed, 0x10, encode_direct_branch("b", 0x10, 0))
        with self.assertRaisesRegex(ValueError, "carrier control edge mismatch"):
            self.transform(
                current=bytes(changed),
                rule={"before_sha256": hashlib.sha256(changed).hexdigest()},
            )

    def test_relocation_at_moved_atom_is_rejected(self):
        relocations = self.relocations + [NamedRelocation(0x14, 10, "label", 0)]
        expected = self.rule["function_relocations"] + [
            {"at": 0x14, "type": 10, "symbol": "label", "addend": 0}
        ]
        with self.assertRaisesRegex(ValueError, "relocation overlaps"):
            self.transform(
                relocations=relocations,
                rule={"function_relocations": expected},
            )

    def test_crossing_branch_ambiguity_is_rejected(self):
        changed = bytearray(self.current)
        struct.pack_into(">I", changed, 0x0C, encode_direct_branch("b", 0x0C, 0x18))
        before_edges = self.rule["before_edges"][:1] + [
            {"at": 0x0C, "kind": "b", "destination": 0x18}
        ] + self.rule["before_edges"][1:]
        with self.assertRaisesRegex(ValueError, "crossing branch ambiguity"):
            self.transform(
                current=bytes(changed),
                rule={
                    "before_sha256": hashlib.sha256(changed).hexdigest(),
                    "before_edges": before_edges,
                },
            )

    def test_nonbranch_target_mismatch_is_rejected(self):
        changed = bytearray(self.target)
        changed[3] ^= 1
        with self.assertRaisesRegex(ValueError, "non-branch target mismatch"):
            self.transform(
                target=bytes(changed),
                rule={"after_sha256": hashlib.sha256(changed).hexdigest()},
            )

    def test_sibling_mutation_is_rejected(self):
        exact = b"exact sibling"
        wanted = hashlib.sha256(exact).hexdigest()
        with self.assertRaisesRegex(ValueError, "sibling mutation"):
            validate_siblings(
                {"sibling": exact + b"!"},
                {"sibling": exact},
                {"sibling": wanted},
            )

    def test_config_absence_is_rejected(self):
        with self.assertRaisesRegex(KeyError, "no p6frank configuration"):
            load_rule({"units": {}}, "game/sys/registry")


if __name__ == "__main__":
    unittest.main()
