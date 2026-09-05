"""Adversarial algebra, CFG/ELF integration and independent concrete checks."""
import random
import struct
import unittest

from tools.gdl import ppc_address_fold as proof
from tools.gdl import webfrank as wf
from tools.gdl.tests.test_webfrank import _elf_object, _words


FOLDED = (0x7c7f0214, 0x3bc30e18, 0x80631024)
UNFOLDED = (0x7fdf0214, 0x3bde0e18, 0x807e020c)
BLR = 0x4e800020
NOP = 0x60000000


def encode(temp, pointer, a, b, delta, displacement):
    def add(d):
        return 31 << 26 | d << 21 | a << 16 | b << 11 | 532

    def addi(d, base):
        return 14 << 26 | d << 21 | base << 16 | delta & 0xffff

    def lwz(base, imm):
        return 32 << 26 | temp << 21 | base << 16 | imm & 0xffff

    return ((add(temp), addi(pointer, temp), lwz(temp, delta + displacement)),
            (add(pointer), addi(pointer, pointer), lwz(pointer, displacement)))


def concrete(words, initial, loaded):
    """Independent small interpreter; no symbolic helpers from the prover."""
    regs = list(initial)
    accesses = []
    for w in words:
        op, d, a, b = w >> 26, (w >> 21) % 32, (w >> 16) % 32, (w >> 11) % 32
        if op == 31:
            regs[d] = (regs[a] + regs[b]) % 4294967296
        else:
            imm = int.from_bytes((w % 65536).to_bytes(2, "big"), "big", signed=True)
            value = (regs[a] + imm) % 4294967296
            if op == 14:
                regs[d] = value
            else:
                accesses.append(value)
                regs[d] = loaded
    return accesses, regs


class AddressFoldAlgebraTests(unittest.TestCase):
    def test_measured_pair_both_directions(self):
        self.assertEqual(proof.prove_address_fold(FOLDED, UNFOLDED), "ours_folded")
        self.assertEqual(proof.prove_address_fold(UNFOLDED, FOLDED), "ours_unfolded")

    def test_every_single_bit_mutation_refuses(self):
        for stream in (0, 1):
            for word in range(3):
                for bit in range(32):
                    pair = [list(FOLDED), list(UNFOLDED)]
                    pair[stream][word] ^= 1 << bit
                    with self.subTest(stream=stream, word=word, bit=bit):
                        with self.assertRaises(ValueError):
                            proof.prove_address_fold(*map(tuple, pair))

    def test_positive_generalizations_against_independent_interpreter(self):
        rng = random.Random(6305)
        available = [r for r in range(3, 32) if r != 13]
        for _ in range(500):
            temp, pointer = rng.sample(available, 2)
            a, b = rng.randrange(32), rng.randrange(32)
            delta = rng.choice([-32768, -3608, -1, 1, 3608, 32767])
            low, high = max(-32768, -32768 - delta), min(32767, 32767 - delta)
            pair = encode(temp, pointer, a, b, delta, rng.randint(low, high))
            proof.prove_address_fold(*pair)
            for seed in (0, 0xffffffff, None):
                initial = [seed if seed is not None else rng.getrandbits(32) for r in range(32)]
                loaded = rng.getrandbits(32)
                self.assertEqual(concrete(pair[0], initial, loaded),
                                 concrete(pair[1], initial, loaded))

    def test_coincident_add_inputs_keep_coefficient_two(self):
        pair = encode(3, 30, 31, 31, -3608, 524)
        proof.prove_address_fold(*pair)
        address, _ = proof.state(pair[0])
        self.assertIn(("r31", 2), address)

    def test_unnecessary_or_out_of_shape_modes_refuse(self):
        for pair in ((FOLDED, FOLDED), (UNFOLDED, UNFOLDED),
                     encode(3, 30, 31, 0, 0, 524),
                     encode(3, 30, 31, 0, 32767, 1)):
            with self.assertRaises(ValueError):
                proof.prove_address_fold(*pair)
        for reg in (0, 1, 2, 13):
            with self.assertRaises(ValueError):
                proof.prove_address_fold(*encode(reg, 30, 31, 0, 3608, 524))


class AddressFoldApplyTests(unittest.TestCase):
    def pair(self, ours=FOLDED, target=UNFOLDED, prefix=(NOP,), suffix=(BLR,),
             our_kwargs=None, target_kwargs=None):
        our_body, target_body = _words(*prefix, *ours, *suffix), _words(*prefix, *target, *suffix)
        patch = {"function": "fn", "before_sha256": wf._sha256(our_body),
                 "after_sha256": wf._sha256(target_body),
                 "address_fold": {"at": len(prefix) * 4,
                                  "proof": "add-addi-lwz-affine-v1"}}
        return (_elf_object(our_body, **(our_kwargs or {})),
                _elf_object(target_body, **(target_kwargs or {})), patch)

    def refuse(self, pattern, **kwargs):
        data, target, patch = self.pair(**kwargs)
        with self.assertRaisesRegex(ValueError, pattern):
            wf.apply_patch(data, patch, bytes(target))

    def test_real_apply_patch_closes_both_directions_at_nonzero_symbol_value(self):
        for ours, target in ((FOLDED, UNFOLDED), (UNFOLDED, FOLDED)):
            data, target_data, patch = self.pair(ours, target, our_kwargs={"value": 64},
                                               target_kwargs={"value": 128})
            before, after, changed = wf.apply_patch(data, patch, bytes(target_data))
            self.assertEqual((before, after, changed),
                             (patch["before_sha256"], patch["after_sha256"], 3))

    def test_hash_target_size_and_outside_window_guards(self):
        for change, pattern in (("input", "input hash"), ("target", "target function hash"),
                                ("size", "size mismatch"), ("outside", "identity outside")):
            data, target, patch = self.pair()
            if change == "input":
                patch["before_sha256"] = "0" * 64
            elif change == "target":
                patch["after_sha256"] = "0" * 64
            else:
                body = _words(NOP if change == "size" else 0x38600000,
                              *UNFOLDED, BLR, *([NOP] if change == "size" else []))
                target = _elf_object(body)
                patch["after_sha256"] = wf._sha256(body)
            with self.subTest(change=change), self.assertRaisesRegex(ValueError, pattern):
                wf.apply_patch(data, patch, bytes(target))

    def test_requires_target_and_strict_declaration(self):
        data, target, patch = self.pair()
        with self.assertRaisesRegex(ValueError, "target object is required"):
            wf.apply_patch(data, patch)
        for declaration in (None, [], {}, {"at": 4, "proof": "trust-me"},
                            {"at": True, "proof": "add-addi-lwz-affine-v1"},
                            {"at": 5, "proof": "add-addi-lwz-affine-v1"},
                            {"at": -4, "proof": "add-addi-lwz-affine-v1"},
                            {"at": 500, "proof": "add-addi-lwz-affine-v1"}):
            data, target, patch = self.pair()
            patch["address_fold"] = declaration
            with self.assertRaises(ValueError):
                wf.apply_patch(data, patch, bytes(target))

    def test_no_composition_or_escape(self):
        for key in ("copy_register_fields", "recolors", "register_fields",
                    "unproven_recolor_audit", "equivalent_copy_form",
                    "equivalent_zero_form", "equivalent_mask_form",
                    "instruction_permutation", "post_recolor_permutation",
                    "value_equality_recolor", "unknown_option"):
            data, target, patch = self.pair()
            patch[key] = True
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, "does not compose"):
                wf.apply_patch(data, patch, bytes(target))

    def test_relocations_in_either_window_and_relocation_shape_drift(self):
        for offset in (4, 6, 8, 10, 12, 14):
            kwargs = {"relocations": [(offset, "global", 4, 0)]}
            self.refuse("relocation inside", our_kwargs=kwargs, target_kwargs=kwargs)
            self.refuse("offsets/types differ", our_kwargs=kwargs)
            self.refuse("offsets/types differ", target_kwargs=kwargs)
        self.refuse("offsets/types differ",
                    our_kwargs={"relocations": [(0, "global", 4, 0)]},
                    target_kwargs={"relocations": [(0, "global", 6, 0)]})

    def test_external_branches_and_calls_cannot_enter_interior(self):
        for branch in (0x48000008, 0x4800000c, 0x48000009, 0x41820008):
            self.refuse("enters window interior", prefix=(branch,))
        # Entry at the START is legal: the complete proof is executed.
        data, target, patch = self.pair(prefix=(0x48000004,))
        self.assertEqual(wf.apply_patch(data, patch, bytes(target))[2], 3)

    def test_only_sda21_halfword_convention_is_normalized(self):
        data, target, patch = self.pair(
            our_kwargs={"relocations": [(2, "global", 109, 0)]},
            target_kwargs={"relocations": [(0, "global", 109, 0)]})
        self.assertEqual(wf.apply_patch(data, patch, bytes(target))[2], 3)
        self.refuse("offsets/types differ",
                    our_kwargs={"relocations": [(2, "global", 4, 0)]},
                    target_kwargs={"relocations": [(0, "global", 4, 0)]})
        for relocs in ([(0, "global", 109, 0), (2, "global", 109, 0)],
                       [(0, "global", 109, 0), (0, "global", 109, 0)]):
            self.refuse("duplicate relocation", our_kwargs={"relocations": relocs},
                        target_kwargs={"relocations": relocs})

    def test_unknown_computed_branch_is_fail_closed(self):
        self.refuse("bctr without", suffix=(0x4e800420,))

    def test_symbols_and_data_or_text_address_taking_refuse(self):
        for side in ("our_kwargs", "target_kwargs"):
            self.refuse("symbol in window interior",
                        **{side: {"extra_symbols": [("interior", 8, 0, 1)]}})
            self.refuse("address-taken window interior", **{side: {
                "data": bytes(4), "data_relocations": [(0, "fn", 1, 8)]}})
        kwargs = {"relocations": [(0, "fn", 10, 8)]}
        self.refuse("address-taken window interior", our_kwargs=kwargs, target_kwargs=kwargs)

    def test_final_datum_guard_still_refuses_wrong_symbols(self):
        data, target, patch = self.pair(
            our_kwargs={"relocations": [(0, "original_global", 4, 0)]},
            target_kwargs={"relocations": [(0, "wrong_global", 4, 0)]})
        with self.assertRaises(ValueError):
            wf.apply_patch(data, patch, bytes(target), {
                "original_global": (".data", 0x80001000),
                "wrong_global": (".data", 0x80002000)})


if __name__ == "__main__":
    unittest.main()
