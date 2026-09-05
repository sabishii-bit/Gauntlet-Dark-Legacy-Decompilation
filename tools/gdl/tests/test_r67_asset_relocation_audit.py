import unittest
from unittest.mock import patch

from tools.gdl.composed_census import r67_asset_relocation_audit as audit


class AssetAuditTests(unittest.TestCase):
    def symbols(self, start):
        return [audit.webfrank.Symbol(name, start + offset, 4, 1)
                for name, offset in audit.ANCHORS.items()]

    def test_relocated_whole_payload_anchors(self):
        self.assertEqual(audit.anchor_start(self.symbols(audit.START - 32)), (audit.START - 32, 1))

    def test_missing_duplicate_or_displaced_anchor_refuses(self):
        symbols = self.symbols(audit.START)
        bad = symbols[1]
        for candidate in [symbols[:1], symbols + symbols[:1],
                          [symbols[0], audit.webfrank.Symbol(bad.name, bad.value + 4, 4, 1)],
                          [symbols[0], audit.webfrank.Symbol(bad.name, bad.value, 4, 2)]]:
            with self.assertRaises(ValueError):
                audit.anchor_start(candidate)

    def test_byte_comparison_preserves_interior_differences(self):
        expected = bytes(audit.SIZE)
        self.assertEqual(audit.compare_payload(expected, expected, audit.START)['status'], 'PASS')
        actual = bytearray(expected)
        actual[12345] = 1
        actual[-1] = 1
        result = audit.compare_payload(expected, actual, audit.START - 32)
        self.assertEqual((result['status'], result['changed_bytes'], result['changed_words']), ('FAIL', 2, 2))
        self.assertEqual(result['changes'][0]['linked_address'], hex(audit.START - 32 + 12344))

    def test_truncation_is_never_equality(self):
        for expected, actual in [(b'', b''), (bytes(audit.SIZE), None),
                                  (bytes(audit.SIZE), bytes(audit.SIZE - 1))]:
            with self.assertRaises(ValueError):
                audit.compare_payload(expected, actual, audit.START)

    def test_arbitrary_same_size_data_cannot_inherit_range_classification(self):
        with self.assertRaises(ValueError):
            audit.checked_payload(bytes(audit.SIZE))

    def object_fixture(self, rels, count):
        sections = [audit.webfrank.Section(0, '', 0, 0, 0, 0, 0, 0),
                    audit.webfrank.Section(1, '.data', 1, 0, audit.SIZE, 0, 0, 0),
                    audit.webfrank.Section(2, '.rela.data', 4, 0, count * 12, 0, 1, 12)]
        return patch.object(audit, 'read_elf', return_value=(bytes(audit.SIZE), sections, self.symbols(0))), patch.object(
            audit.webfrank, '_function_text_relocations_full', return_value=rels)

    def test_whole_object_bytes_outside_relocations_are_checked(self):
        source, reloc = self.object_fixture({}, 0)
        with source, reloc:
            self.assertEqual(audit.object_relocations(None, bytes(audit.SIZE)), ([], True))
            expected = bytearray(audit.SIZE)
            expected[9876] = 1
            self.assertEqual(audit.object_relocations(None, expected), ([], False))

    def test_unknown_or_unaligned_relocation_refuses(self):
        for rels in [{4: (2, 'fake', 0)}, {3: (1, 'fake', 0)}]:
            source, reloc = self.object_fixture(rels, 1)
            with source, reloc, self.assertRaises(ValueError):
                audit.object_relocations(None, bytes(audit.SIZE))

    def test_duplicate_or_out_of_range_entries_cannot_be_silently_dropped(self):
        source, reloc = self.object_fixture({4: (1, 'fake', 0)}, 2)
        with source, reloc, self.assertRaises(ValueError):
            audit.object_relocations(None, bytes(audit.SIZE))


if __name__ == '__main__':
    unittest.main()
