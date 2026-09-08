"""The retired atree symbol-export stage must not return in either mode."""
from pathlib import Path
import runpy
import sys
import unittest
from unittest.mock import patch
from tools import project

ROOT=Path(__file__).resolve().parents[3]

class AtreeNativeBuildTests(unittest.TestCase):
    def test_both_modes_have_no_exporter_or_objcopy_dependency(self):
        for args in ([],['--non-matching'],['--binutils','custom-tools']):
            with self.subTest(args=args),patch.object(sys,'argv',['configure.py',*args]), patch.object(project,'generate_build') as generate:
                runpy.run_path(str(ROOT/'configure.py'),run_name='__main__')
                config=generate.call_args.args[0]
                self.assertTrue(config.native_only)
                self.assertEqual(config.object_postprocesses,{})
                self.assertEqual(config.custom_build_rules,[])
                self.assertEqual(config.custom_build_steps,{})

if __name__=='__main__': unittest.main()
