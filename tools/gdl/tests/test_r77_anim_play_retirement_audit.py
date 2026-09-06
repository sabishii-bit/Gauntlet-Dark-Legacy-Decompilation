import copy
import unittest

from tools.gdl.composed_census.r77_anim_play_retirement_audit import audit, bind, FN, LITERALS, MAGIC, POOL


def fixture():
    body = '00'*4188
    relocs = [[0,109,'lbl_803457F0',0], [4,109,['.sdata2',0,8,1,0],0]]
    a = dict(functions={FN:dict(body=body,size=4188,offset=0,relocations=relocs),
                           'sibling':dict(body='12345678',size=4,offset=4188,relocations=[])},
             sections={'.text':dict(bytes=body,relocations=copy.deepcopy(relocs)),
                       '.sdata2':dict(bytes=MAGIC,size=8,alignment=8,flags=3,type=1,relocations=[])},
             exception_records={}, all_symbols=[[[ '.sdata2',0,8,1,0],'.sdata2',0,8,1,0]]+
             [[name,'',0,0,16,0] for name in LITERALS])
    b = copy.deepcopy(a)
    b['functions'][FN]['relocations'] = [[0,109,['.sdata2',0,4,1,0],0], [4,109,['.sdata2',72,8,1,0],0]]
    b['sections']['.text']['relocations'] = copy.deepcopy(b['functions'][FN]['relocations'])
    b['sections']['.sdata2'].update(bytes=POOL,size=88)
    b['all_symbols'] = [[[ '.sdata2',off,size,1,0],'.sdata2',off,size,1,0]
                        for off,size in {0:4,8:8,16:8,24:8,32:4,40:8,48:8,56:8,64:8,72:8,80:8}.items()]
    return a,b,copy.deepcopy(b)


class AnimPlayRetirementTests(unittest.TestCase):
    def test_reviewed_ownership_move(self):
        result = audit(*fixture())
        self.assertEqual(result['status'],'PASS')
        self.assertEqual(result['new_source_owned_bytes'],80)
        self.assertEqual(result['raw_exact_instructions'],1047)

    def test_pool_binding_preserves_type_and_address(self):
        self.assertEqual(bind(['.sdata2',32,4,1,0],0x803457F0),bind('lbl_80345810',0))
        self.assertNotEqual(bind(['.sdata2',32,8,1,0],0x803457F0),bind('lbl_80345810',0))

    def test_each_unapproved_delta_fails(self):
        for change in ('sibling','target','extent','pool','binding','eh','section','symbol','old_symbol','flags','count'):
            with self.subTest(change=change):
                a,b,t = fixture()
                if change=='sibling': b['functions']['sibling']['body']='87654321'
                elif change=='target': t['functions'][FN]['body']='11'*4188
                elif change=='extent': b['functions'][FN]['offset']=4
                elif change=='pool': b['sections']['.sdata2']['bytes']='11'+POOL[2:]
                elif change=='binding': b['functions'][FN]['relocations'][0][2][1]=4
                elif change=='eh': b['exception_records'][FN]={'bad':1}
                elif change=='section': b['sections']['.data']={}
                elif change=='symbol': b['all_symbols'][0][3]=8
                elif change=='old_symbol': a['all_symbols'].pop()
                elif change=='flags': b['sections']['.sdata2']['flags']=7
                elif change=='count': t['functions'][FN]['body']='00'*4
                with self.assertRaises(ValueError): audit(a,b,t)


if __name__=='__main__':
    unittest.main()
