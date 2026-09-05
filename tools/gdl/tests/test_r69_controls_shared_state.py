import unittest

from tools.gdl.composed_census import r69_controls_shared_state as audit


class SharedStateTests(unittest.TestCase):
    def homes(self):
        return (dict(section=".bss", offset=1656, size=240, binding=0, bytes=None),
                [dict(object="current.o", section=".bss", offset=5704, size=240)],
                {audit.CTL: (".bss", 0x80240E30, 240)})

    def test_exact_home_evidence(self):
        audit.check_homes(*self.homes())

    def test_no_fake_initialized_or_exported_private_home(self):
        for key, value in (("section", ".sbss"), ("offset", 0), ("size", 244),
                           ("binding", 1), ("bytes", "00"*240)):
            local, public, table = self.homes()
            local[key] = value
            with self.assertRaises(ValueError):
                audit.check_homes(local, public, table)

    def test_public_uniqueness_extent_and_target_are_guarded(self):
        for change in ("absent", "duplicate", "extent", "target"):
            local, public, table = self.homes()
            if change == "absent":
                public = []
            elif change == "duplicate":
                public += [dict(public[0])]
            elif change == "extent":
                public[0]["size"] = 244
            else:
                table[audit.CTL] = (".bss", 0x802407B8, 240)
            with self.assertRaises(ValueError):
                audit.check_homes(local, public, table)

    def test_instruction_count_and_nonreloc_delta_are_separate(self):
        target = ["mulli   r5,r3,60", "stw     r4,1656(r3)", "blr"]
        same = audit.score(target, target)
        moved = audit.score(target, ["mulli   r5,r3,60", "stw     r4,1832(r3)", "blr"])
        self.assertEqual(same["nonreloc_diff_rows"], 0)
        self.assertEqual(moved["target_instructions"], moved["ours_instructions"])
        self.assertEqual(moved["nonreloc_diff_rows"], 2)
        self.assertEqual(moved["classification"], "OPERAND_DIFF")

    def test_score_does_not_claim_datum_equivalence(self):
        target = ["lis     r3,0", "    R_PPC_ADDR16_HA\tlbl_802407B8", "blr"]
        ours = ["lis     r3,0", "    R_PPC_ADDR16_HA\tlbl_80240E30", "blr"]
        self.assertEqual(audit.score(target, ours)["nonreloc_diff_rows"], 0)
        # This zero is the deliberately reported metric's blind spot, not a
        # statement that the two different named addresses denote one object.

    def test_unverified_reports_are_refused_before_io(self):
        for report in ({}, {"status": "MEASURED", "fidelity": False},
                       {"status": "PASS", "fidelity": True}):
            with self.assertRaises(ValueError):
                audit.measure(report)


if __name__ == "__main__":
    unittest.main()
