"""Pure positive/negative checks for the finite CLOCK section certificate."""
import copy
import unittest

from tools.gdl.composed_census import r82_clock_partition_audit as audit


def fixture():
    sections={n:dict(size=size,type=1,alignment=4,bytes='00'*size)
              for n,size in audit.SIZES.items()}
    sections['.sdata2']['bytes']=audit.POOL
    functions={n:dict(offset=off,size=size,binding=1)
               for n,(off,size) in audit.CLOCK_FUNCTIONS.items()}
    return dict(functions=functions,sections=sections,
                exception_records={'ClockOncePerFrame':'1008000000000000'})


class ClockCertificateTests(unittest.TestCase):
    def test_positive_whole_allocated_extent(self):
        inv=fixture()
        relocs={n:[] for n in audit.SIZES}
        result=audit.check_clock(inv,copy.deepcopy(inv),relocs,copy.deepcopy(relocs))
        self.assertEqual(result['allocated_bytes'],808)
        self.assertEqual(sum(result['raw_instructions'].values()),187)

    def test_wrong_pool_word_even_equal_size_refused(self):
        inv=fixture()
        bad=copy.deepcopy(inv)
        bad['sections']['.sdata2']['bytes']='3f800000'+audit.POOL[8:]
        with self.assertRaisesRegex(ValueError,'pool bytes'):
            audit.check_clock(bad,inv,{}, {})

    def test_extra_allocation_refused(self):
        inv=fixture()
        bad=copy.deepcopy(inv)
        bad['sections']['.data']=dict(size=4)
        with self.assertRaisesRegex(ValueError,'section roster'):
            audit.check_clock(bad,inv,{}, {})

    def test_wrong_function_extent_refused(self):
        inv=fixture()
        bad=copy.deepcopy(inv)
        bad['functions']['ClockOncePerFrame']['size']+=4
        with self.assertRaisesRegex(ValueError,'function layout'):
            audit.check_clock(bad,inv,{}, {})

    def test_final_binding_identity_not_just_bytes(self):
        inv=fixture()
        a={n:[] for n in audit.SIZES}
        b=copy.deepcopy(a)
        a['.text']=[[4,109,0x803462E8]]
        b['.text']=[[4,109,0x803462EC]]
        with self.assertRaisesRegex(ValueError,'final-address bindings'):
            audit.check_clock(inv,inv,a,b)

    def test_wrong_eh_bytes_refused(self):
        inv=fixture()
        bad=copy.deepcopy(inv)
        bad['sections']['extab']['bytes']='01'+'00'*7
        with self.assertRaisesRegex(ValueError,'section bytes'):
            audit.check_clock(bad,inv,{}, {})

    def test_missing_eh_record_refused(self):
        inv=fixture()
        bad=copy.deepcopy(inv)
        bad['exception_records']={}
        relocs={n:[] for n in audit.SIZES}
        with self.assertRaisesRegex(ValueError,'EH coverage'):
            audit.check_clock(bad,inv,relocs,relocs)

    def test_named_bindings_never_value_normalized(self):
        self.assertNotEqual(audit.reference({},'named_a',0),audit.reference({},'named_b',0))

    def test_anonymous_code_interior_keeps_function_identity(self):
        inv={'functions':{'first':{'offset':0,'size':8},'second':{'offset':8,'size':8}}}
        self.assertEqual(audit.reference(inv,['.text',12,0,0,0],0),['code','second',4,0,0])
        with self.assertRaisesRegex(ValueError,'unresolved anonymous code'):
            audit.reference(inv,['.text',16,0,0,0],0)

    def test_partition_refuses_absent_boundary(self):
        with self.assertRaises(ValueError):
            audit.partition_sources('void unrelated(void) {}')


if __name__=='__main__':
    unittest.main()
