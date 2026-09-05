"""Negative controls for the bounded auxscreen ownership evidence."""
import copy
import json
import unittest

from tools.gdl.composed_census import r68_aux_ownership_audit as audit


class InitializedObligationTests(unittest.TestCase):
    def setUp(self):
        self.data = b"".join(i.to_bytes(4, "big") for i in range(7))
        self.source = {"sections": {".sdata": {"type": 1, "alignment": 8,
                        "size": 28, "bytes": self.data.hex(), "relocations": []}},
                       "symbols": {n: {"section": ".sdata", "offset": i*4,
                        "size": 4, "binding": 1, "bytes": self.data[i*4:i*4+4].hex()}
                        for i, n in enumerate(audit.INITIALIZED)}}
        self.offsets = {n: i*4 for i, n in enumerate(audit.INITIALIZED)}

    def check(self, source=None, target=None):
        return audit.initialized_obligation(source or self.source,
                    target if target is not None else self.data + b"\0"*4, self.offsets)

    def test_complete_run(self):
        self.assertEqual(self.check()["status"], "PASS")

    def test_wrong_value_or_nonzero_slack(self):
        for index in (3, 31):
            target = bytearray(self.data + b"\0"*4)
            target[index] ^= 1
            with self.assertRaises(ValueError):
                self.check(target=target)

    def test_short_extent_and_hidden_relocation(self):
        for key, value in (("size", 24), ("alignment", 4), ("type", 8),
                           ("relocations", [(0, 1, "other", 0)])):
            source = copy.deepcopy(self.source)
            source["sections"][".sdata"][key] = value
            with self.assertRaises(ValueError):
                self.check(source=source)

    def test_missing_or_moved_definition(self):
        for key, value in (("offset", 4), ("size", 8), ("binding", 0)):
            source = copy.deepcopy(self.source)
            source["symbols"][audit.INITIALIZED[0]][key] = value
            with self.assertRaises(ValueError):
                self.check(source=source)
        del self.source["symbols"][audit.INITIALIZED[0]]
        with self.assertRaises(ValueError):
            self.check()

    def test_comparison_survives_json_roundtrip(self):
        source = {"f": dict(offset=0, size=4, body="12345678",
                            relocations=[[0, 1, "data", 0]], binding=1)}
        self.assertTrue(all(not rows for rows in audit.changed_functions(
            json.loads(json.dumps(source)), source).values()))
        source["f"]["relocations"][0][2] = "wrong"
        before = {"f": dict(source["f"], relocations=[[0, 1, "data", 0]])}
        self.assertEqual(audit.changed_functions(before, source)["relocations"], ["f"])


if __name__ == "__main__":
    unittest.main()
