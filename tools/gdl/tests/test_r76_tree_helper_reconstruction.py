import copy
import unittest

from tools.gdl.composed_census import r76_tree_helper_reconstruction as tool


SOURCE = '''MBTreeNode* MBNewNode(MBTreeNode* parent, const f32* matrix, s32 type)
{
    u8 unused[8];
    MBTreeNode* node;
    if (matrix == 0)
        matrix = gIdentityMatrix;
    if (type == 0)
        type = 1;
    if ((node = lbl_80344EE0) != 0) {
        lbl_80344EE0 = node->next;
    } else {
        node = AllocMem(0x80);
    }

    if (node != 0) {
        MBNodeInit(node, type);
        CopyMat4(matrix, (f32*)node);
        node->parent = parent;
        if (parent == 0)
            lbl_80344ECC = node;
        else
            parent->child = node;
    }
    return node;
}

MBTreeNode* MBCreateNode(void)
{
    MBTreeNode* node;
    if (lbl_80344EE0 != 0) {
        node = lbl_80344EE0;
        lbl_80344EE0 = node->next;
    } else {
        node = AllocMem(0x80);
    }
    return node;
}

void MBNodeInsert(MBTreeNode* node, MBTreeNode* parent)
{
    node->parent = parent;
    if (parent == 0) {
        MBTreeNode* head;
        if ((head = lbl_80344ECC) == 0) {
            lbl_80344ECC = node;
            return;
        }
        MBNodeAppend(node, head);
        return;
    }
    {
        MBTreeNode* head = parent->child;
        if (head == 0) {
            parent->child = node;
            return;
        }
        MBNodeAppend(node, head);
    }
}
'''


def fixture():
    before_body = bytes(272)
    target_body = bytearray(before_body)
    for n, off in enumerate(tool.SITES, 1):
        target_body[off:off+4] = n.to_bytes(4, 'big')
    before = dict(
        functions={tool.FN: dict(body=before_body.hex(), size=272, offset=4,
                                relocations=[[0x54, 10, 'AllocMem', 0]]),
                   'sibling': dict(body='12345678', size=4, offset=0, relocations=[])},
        sections={'.text': dict(bytes=(bytes.fromhex('12345678')+before_body).hex(), relocations=[]),
                  '.sdata2': dict(bytes='3f800000', relocations=[]),
                  '.extab': dict(bytes='01020304', relocations=[])},
        all_symbols=[['MBNewNode', '.text', 4, 272]], metadata=['baseline'])
    target = copy.deepcopy(before)
    target['functions'][tool.FN]['body'] = target_body.hex()
    target['sections']['.text']['bytes'] = (bytes.fromhex('12345678')+target_body).hex()
    return before, copy.deepcopy(target), target, copy.deepcopy(target)


class TreeHelperReconstructionTests(unittest.TestCase):
    def test_two_forms_keep_public_abi_and_only_shared_form_factors_public_bodies(self):
        duplicate = tool.reconstruct(SOURCE, 'duplicate')
        shared = tool.reconstruct(SOURCE, 'shared')
        for output in (duplicate, shared):
            self.assertIn('MBTreeNode* MBNewNode(MBTreeNode* parent, const f32* matrix, s32 type)', output)
            self.assertIn('    u8 unused[8];', output)
            self.assertIn('    node = MBCreateNodeInline();', output)
            self.assertIn('        MBNodeInsertInline(node, parent);', output)
            self.assertNotIn('#pragma', output)
        self.assertEqual(duplicate.count('node = AllocMem(0x80);'), 2)
        self.assertEqual(shared.count('node = AllocMem(0x80);'), 1)
        self.assertEqual(shared.count('#undef MB_NODE_INSERT_BODY'), 1)
        self.assertEqual(shared.count('MB_NODE_INSERT_BODY(MBNodeAppend(node, head))'), 1)

    def test_refuse_unknown_or_changed_source_shapes(self):
        for source, form in ((SOURCE, 'unknown'), (SOURCE+SOURCE, 'shared'),
                             (SOURCE.replace('MBNodeAppend(node, head);', 'other(node, head);', 1), 'shared'),
                             (tool.reconstruct(SOURCE, 'duplicate'), 'shared')):
            with self.subTest(form=form), self.assertRaises(ValueError):
                tool.reconstruct(source, form)

    def test_exact_pair_is_experiment_and_does_not_mutate_inputs(self):
        inputs = fixture()
        saved = copy.deepcopy(inputs)
        result = tool.audit(*inputs)
        self.assertEqual(result['differing_words_before'], 23)
        self.assertEqual(result['differing_words_after'], 0)
        self.assertEqual(result['rules_retired'], 0)
        self.assertEqual(result['status'], 'RAW_EXACT_EXPERIMENT')
        self.assertEqual(result['unchanged_siblings'], 1)
        self.assertEqual(inputs, saved)

    def test_refuse_sibling_data_metadata_symbol_and_relocation_drift(self):
        for defect in ('sibling', 'data', 'eh', 'symbol', 'relocation'):
            values = fixture()
            after = values[1]
            if defect == 'sibling':
                after['functions']['sibling']['body'] = 'ffffffff'
            elif defect == 'data':
                after['sections']['.sdata2']['bytes'] = '00000000'
            elif defect == 'eh':
                after['sections']['.extab']['bytes'] = '00000000'
            elif defect == 'symbol':
                after['all_symbols'][0][2] = 8
            else:
                after['functions'][tool.FN]['relocations'][0][2] = 'wrong_allocator'
            with self.subTest(defect=defect), self.assertRaises(ValueError):
                tool.audit(*values)

    def test_refuse_wrong_count_or_residual_sites(self):
        for defect in ('size', 'sites'):
            values = fixture()
            if defect == 'size':
                values[0]['functions'][tool.FN]['size'] = 268
            else:
                values[2]['functions'][tool.FN]['body'] = 'ffffffff' + values[2]['functions'][tool.FN]['body'][8:]
            with self.subTest(defect=defect), self.assertRaises(ValueError):
                tool.audit(*values)

    def test_refuse_different_processed_baseline(self):
        values = fixture()
        values[3]['metadata'] = ['different']
        with self.assertRaisesRegex(ValueError, 'processed allocated object'):
            tool.audit(*values)


if __name__ == '__main__':
    unittest.main()
