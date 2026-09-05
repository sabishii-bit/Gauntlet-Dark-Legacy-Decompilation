"""Two-sided calibration for the narrowly scoped R67 audio investigations."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "tools/gdl/composed_census" / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


audit = load("r67_audio_effective_string_audit")
probe = load("r67_audio_context_probe")


def lines(base="pool", displacement=220):
    result = ["lis r6,0", "    R_PPC_ADDR16_HA\t" + base,
              "addi r31,r6,0", "    R_PPC_ADDR16_LO\t" + base]
    for delta in (displacement, displacement + 28, displacement, displacement + 64):
        result += [f"addi r3,r31,{delta}", "crclr 4*cr1+eq", "addi r4,r30,1048",
                   "bl <reloc>", "    R_PPC_REL24\tErrorPrintf"]
    result += ["blr"]
    return result


class EffectiveStringAuditTests(unittest.TestCase):
    def test_offsets_are_taken_from_consumers_not_pool_symbol(self):
        sites = audit.argument_sites(lines())
        self.assertEqual([s["displacement"] for s in sites], [220, 248, 220, 284])
        self.assertEqual([s["call_offset"] for s in sites], [20, 36, 52, 68])

    def test_different_bases_can_pass_when_consumed_strings_equal(self):
        target = [dict(string=s, base="timeout", displacement=d) for s, d in zip(["bad", "memory", "bad", "error"], [220, 248, 220, 284])]
        ours = [dict(string=s, base="bad", displacement=d) for s, d in zip(["bad", "memory", "bad", "error"], [0, 28, 0, 64])]
        self.assertTrue(all(r["equal"] for r in audit.compare_messages(target, ours)))

    def test_actual_wrong_string_does_not_pass(self):
        target = [dict(string=s) for s in ["bad", "memory", "bad", "error"]]
        ours = [dict(string=s) for s in ["timeout", "memory", "bad", "error"]]
        self.assertEqual([r["equal"] for r in audit.compare_messages(target, ours)], [False, True, True, True])

    def test_empty_coverage_refuses(self):
        with self.assertRaises(ValueError):
            audit.compare_messages([], [])
        with self.assertRaises(ValueError):
            audit.argument_sites([])

    def test_changed_base_refuses(self):
        code = lines()
        code.insert(9, "li r31,0")
        with self.assertRaisesRegex(ValueError, "invariant"):
            audit.argument_sites(code)

    def test_updating_store_clobbers_address_register_not_store_source(self):
        code = lines()
        code.insert(9, "stwu r0,-16(r31)")
        with self.assertRaisesRegex(ValueError, "invariant"):
            audit.argument_sites(code)
        self.assertFalse(audit.writes(dict(op="stwu", args="r31,-16(r1)"), 31))
        self.assertTrue(audit.writes(dict(op="stwu", args="r31,-16(r1)"), 1))

    def test_non_dominating_base_refuses(self):
        with self.assertRaisesRegex(ValueError, "dominate"):
            audit.argument_sites(["beq <fn+0xc>"] + lines())

    def test_interior_branch_refuses(self):
        code = lines()
        code.insert(4, "beq <fn+0x10>")
        with self.assertRaisesRegex(ValueError, "branch enters"):
            audit.argument_sites(code)

    def test_unknown_instruction_and_wrong_ha_refuse(self):
        with self.assertRaisesRegex(ValueError, "unmodelled"):
            audit.argument_sites(lines() + ["bctrl"])
        code = lines()
        code[1] = "    R_PPC_ADDR16_HA\tother"
        with self.assertRaisesRegex(ValueError, "HA/LO"):
            audit.argument_sites(code)

    def test_unreadable_or_unterminated_strings_refuse(self):
        for value in (None, b"unterminated", b"\xff\0", b"\0"):
            with self.assertRaises(ValueError):
                audit.cstring(value)
        self.assertEqual(audit.cstring(b"message\0neighbour"), "message")


class ContextProbeTests(unittest.TestCase):
    def test_variants_preserve_production_and_expose_diagnostic_scope(self):
        source = (ROOT / "src/game/audio/audio.c").read_text(encoding="utf-8")
        variants = probe.variants(source)
        self.assertEqual(variants["scratch_mirror"], source)
        original_body = probe.stream_block(source)[2]
        self.assertEqual(probe.stream_block(variants["own_rodata_prefix_control"])[2], original_body)
        self.assertIn("const char sAudioTimeoutMsg[] =", variants["own_rodata_prefix_control"])
        no_prior = variants["own_rodata_prefix_no_prior_pragmas_control"]
        boundary = probe.stream_block(no_prior)[0]
        self.assertNotIn("#pragma", no_prior[:boundary])
        self.assertIn("#pragma", no_prior[boundary:])
        self.assertIn("R67AudioStateProbe* state", variants["typed_local_record_control"])
        balanced = variants["local_wrapper_owned_rodata_pad248_diagnostic_control"]
        a, b, body = probe.stream_block(balanced)
        self.assertIn("volatile u8 unused[248];", body)
        self.assertIn("volatile u8 unused[256];", balanced[:a])
        self.assertEqual(balanced[b:], variants["own_rodata_prefix_control"][probe.stream_block(variants["own_rodata_prefix_control"])[1]:])
        self.assertNotEqual(variants["direct_state_uses"], source)
        self.assertEqual((ROOT / "src/game/audio/audio.c").read_text(encoding="utf-8"), source)

    def test_changed_state_declaration_refuses(self):
        source = (ROOT / "src/game/audio/audio.c").read_text(encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "state declaration changed"):
            probe.variants(source.replace("extern u8 sAudioState[];", "extern u8 sAudioState[24];"))


if __name__ == "__main__":
    unittest.main()
