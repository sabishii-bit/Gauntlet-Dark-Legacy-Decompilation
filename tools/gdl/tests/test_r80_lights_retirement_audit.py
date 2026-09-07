import copy
import unittest

from tools.gdl.composed_census.r80_lights_retirement_audit import (
    BASES, FUNCTIONS, SIZES, check_inventories,
)


class LightsRetirementTests(unittest.TestCase):
    def setUp(self):
        functions = {name: dict(offset=offset, size=size, binding=1, body='60000000')
                     for name, (offset, size) in FUNCTIONS.items()}
        sections = {name: dict(size=size, alignment=4, type=1, bytes='00' * size)
                    for name, size in SIZES.items()}
        lights = dict(functions=functions, sections=sections,
                      exception_records={name: {'size': size} for name, (_, size) in FUNCTIONS.items()})
        items = dict(functions={str(i): dict(body='60000000', size=4, relocations=[], binding=1)
                                for i in range(51)}, sections={'.data': {'bytes': '11223344'}},
                     exception_records={'0': {'frame': 16}})
        before = copy.deepcopy(items)
        before['functions'].update(copy.deepcopy(functions))
        relocs = {name: [] for name in SIZES}
        relocs['.text'] = [[100, 109, BASES['.sdata2']]]
        self.data = dict(before=before, items=items, lights=lights, target=copy.deepcopy(lights),
                         source_relocs=relocs, target_relocs=copy.deepcopy(relocs))

    def test_exact_partition_and_bindings(self):
        self.assertEqual(check_inventories(**self.data)['unchanged_items_functions'], 51)

    def test_wrong_zero_address_is_not_equal_datum(self):
        self.data['source_relocs']['.text'][0][2] += 4
        with self.assertRaisesRegex(ValueError, 'binding'):
            check_inventories(**self.data)

    def test_positional_relocation_matters(self):
        self.data['source_relocs']['.text'][0][0] += 4
        with self.assertRaisesRegex(ValueError, 'binding'):
            check_inventories(**self.data)

    def test_parent_sibling_body(self):
        self.data['items']['functions']['0']['body'] = '4e800020'
        with self.assertRaisesRegex(ValueError, 'function changed'):
            check_inventories(**self.data)

    def test_parent_nontext(self):
        self.data['items']['sections']['.data']['bytes'] = '00000000'
        with self.assertRaisesRegex(ValueError, 'nontext'):
            check_inventories(**self.data)

    def test_parent_exception_record(self):
        self.data['items']['exception_records']['0']['frame'] = 24
        with self.assertRaisesRegex(ValueError, 'exception record'):
            check_inventories(**self.data)

    def test_lights_native_body(self):
        self.data['lights']['functions']['InitLighting']['body'] = '4e800020'
        with self.assertRaisesRegex(ValueError, 'body differs'):
            check_inventories(**self.data)

    def test_lights_function_order(self):
        self.data['lights']['functions']['InitLighting']['offset'] = 4
        with self.assertRaisesRegex(ValueError, 'layout'):
            check_inventories(**self.data)

    def test_lights_exception_bytes(self):
        self.data['lights']['sections']['extab']['bytes'] = '01' * 16
        with self.assertRaisesRegex(ValueError, 'section bytes'):
            check_inventories(**self.data)

    def test_wrong_allocation_extent(self):
        self.data['lights']['sections']['.sdata2']['size'] = 8
        with self.assertRaisesRegex(ValueError, 'extent'):
            check_inventories(**self.data)

    def test_incompatible_alignment(self):
        self.data['lights']['sections']['.sdata2']['alignment'] = 16
        with self.assertRaisesRegex(ValueError, 'alignment'):
            check_inventories(**self.data)

    def test_additional_allocated_data(self):
        self.data['lights']['sections']['.data'] = self.data['lights']['sections']['.sdata2']
        with self.assertRaisesRegex(ValueError, 'allocated sections'):
            check_inventories(**self.data)


if __name__ == '__main__':
    unittest.main()
