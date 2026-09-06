import copy
import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from tools.gdl.composed_census.r78_blit_retirement_audit import audit, checked_output, verify_archive


class BlitRetirementAuditTests(unittest.TestCase):
    def setUp(self):
        pool = bytes(144)
        rows = [[170,109,'lbl_80348AD4',0],[310,109,'lbl_80348AE0',0]]
        fn = dict(offset=16,size=1500,body='00'*1500,relocations=rows,binding=1)
        section = dict(size=144,bytes=pool.hex(),alignment=8,type=1,flags=2,relocations=[])
        self.before = dict(functions={'DrawBlit':fn},sections={'.sdata2':section},
                           exception_records=[],all_symbols=[['lbl_80348AE0','',0,0,16,0]])
        self.after = copy.deepcopy(self.before)
        self.after['functions']['DrawBlit']['relocations'] = [[170,109,['.sdata2',68,4,1,0],0],[310,109,['.sdata2',80,4,1,0],0]]
        self.after['sections']['.sdata2'].update(size=152,bytes=(pool[:80]+bytes.fromhex('3d80000000000000')+pool[80:]).hex())
        self.after['all_symbols'] = [[['.sdata2',80,4,1,0],'.sdata2',80,4,1,0]]
        self.target = copy.deepcopy(self.before)
        self.target['functions']['DrawBlit']['relocations'] = [[168,109,'lbl_80348AD4',0],[308,109,'lbl_80348AE0',0]]
        self.reader = patch('tools.gdl.composed_census.r78_blit_retirement_audit.dol_read',
                            side_effect=lambda addr,size: bytes.fromhex('3d800000') if addr==0x80348AE0 else bytes(size))
        self.reader.start()
        self.addCleanup(self.reader.stop)

    def run_audit(self):
        return audit(self.before,self.after,self.target)

    def test_positive(self):
        self.assertEqual(self.run_audit()['instruction_words'],375)
        self.assertEqual(self.run_audit()['sibling_bodies_preserved'],0)

    def test_body_refused(self):
        self.after['functions']['DrawBlit']['body']='ff'*1500
        with self.assertRaisesRegex(ValueError,'instruction bytes'): self.run_audit()

    def test_pool_datum_refused(self):
        self.after['sections']['.sdata2']['bytes']='ff'+self.after['sections']['.sdata2']['bytes'][2:]
        with self.assertRaisesRegex(ValueError,'UV insertion'): self.run_audit()

    def test_call_or_position_refused(self):
        self.target['functions']['DrawBlit']['relocations'][0][0]+=4
        with self.assertRaisesRegex(ValueError,'positional'): self.run_audit()

    def test_addend_refused(self):
        self.after['functions']['DrawBlit']['relocations'][0][3]=4
        with self.assertRaisesRegex(ValueError,'relocation identities'): self.run_audit()

    def test_symbol_extra_refused(self):
        self.after['all_symbols'].append(['surprise','',0,0,16,0])
        with self.assertRaisesRegex(ValueError,'symbol identities'): self.run_audit()

    def test_exception_refused(self):
        self.after['exception_records']=[1]
        with self.assertRaisesRegex(ValueError,'exception records'): self.run_audit()

    def test_uv_outside_drawblit_refused(self):
        self.before['functions']['other']=dict(offset=1600,size=4,body='00000000',binding=1,relocations=[[0,109,'lbl_80348AE0',0]])
        self.after['functions']['other']=copy.deepcopy(self.before['functions']['other'])
        self.after['functions']['other']['relocations'][0][2]=['.sdata2',80,4,1,0]
        with self.assertRaisesRegex(ValueError,'outside DrawBlit'): self.run_audit()

    def test_sibling_refused(self):
        self.before['functions']['other']=dict(offset=1600,size=4,body='00000000',binding=1,relocations=[])
        self.after['functions']['other']=copy.deepcopy(self.before['functions']['other'])
        self.after['functions']['other']['body']='ffffffff'
        with self.assertRaisesRegex(ValueError,'function bytes/layout other'): self.run_audit()

    def test_retail_datum_refused(self):
        with patch('tools.gdl.composed_census.r78_blit_retirement_audit.dol_read',return_value=b'\xff'*4):
            with self.assertRaisesRegex(ValueError,'retail datum'): self.run_audit()

    def test_output_guard(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            self.assertEqual(checked_output(root/'build/r78_blit_test.json',root),root/'build/r78_blit_test.json')
            for path in (root/'src/r78_blit_test.json',root/'build/plain.json',root/'build/../r78_blit_test.json'):
                with self.assertRaisesRegex(ValueError,'output must'): checked_output(path,root)

    def test_archive_source_drift_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            (root/'r78_blit_baseline.c').write_bytes(b'changed')
            manifest=dict(edge={'body_o':'raw.o'},input_hashes={'src/game/mb/mb_blit.c':hashlib.sha256(b'original').hexdigest()})
            with self.assertRaisesRegex(ValueError,'archive hash source'): verify_archive(root,manifest)


if __name__ == '__main__':
    unittest.main()
