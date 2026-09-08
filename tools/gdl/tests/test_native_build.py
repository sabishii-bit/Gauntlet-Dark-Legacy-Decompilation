"""Native producer policy: allow compiler output, reject rewrite regressions."""
from copy import deepcopy
from pathlib import Path
from types import SimpleNamespace
import unittest
from tools.gdl import native_build as native
from tools.gdl.raw_object import load_graph

ROOT=Path(__file__).resolve().parents[3]

def graph():
    return dict(native_only=True,rules={'mwcc':{'command_template':'mwcc $in -o $out'}},
                edges=[dict(rule='mwcc',outputs=['build/a.o'],inputs=['src/a.c'])],
                units=[dict(source_object='build/a.o')])

class NativePolicyTests(unittest.TestCase):
    def test_direct_compiler_output_passes(self):
        native.check_snapshot(graph())

    def test_every_known_rewriter_and_renamed_rewriter_refuses(self):
        for rule in native.FORBIDDEN_RULES:
            g=graph();g['edges'][0]['rule']=rule
            with self.subTest(rule=rule),self.assertRaises(ValueError): native.check_snapshot(g)
        for command in ('dtk extab clean a.o a.o','dtk elf fixup a.o a.o',
                        'python tools/gdl/webfrank.py a.o b.o'):
            g=graph();g['rules']['mwcc']['command_template']=command
            with self.subTest(command=command),self.assertRaises(ValueError): native.check_snapshot(g)

    def test_no_disguised_or_stale_postprocessed_source_objects(self):
        for mutate in (lambda g:g['edges'][0].update(rule='copy_patched_object'),
                       lambda g:g['edges'][0].update(outputs=['build/.postprocess/body/a.o']),
                       lambda g:g['units'][0].update(source_object='not-produced.o'),
                       lambda g:g.update(native_only=False)):
            g=graph();mutate(g)
            with self.assertRaises(ValueError): native.check_snapshot(g)

    def test_config_refuses_any_object_rewrite_or_custom_step(self):
        config=SimpleNamespace(native_only=True,object_postprocesses={},custom_build_rules=[],custom_build_steps={})
        native.check_config(config,{'a.c':SimpleNamespace(options={})})
        for option in ('postprocess','extab_padding','frank_profile_mw_version'):
            with self.subTest(option=option),self.assertRaises(ValueError):
                native.check_config(config,{'a.c':SimpleNamespace(options={option:'legacy'})})
        for option in ('object_postprocesses','custom_build_rules','custom_build_steps'):
            changed=deepcopy(config);setattr(changed,option,{'legacy':True})
            with self.subTest(option=option),self.assertRaises(ValueError): native.check_config(changed,{})

    def test_live_graph_has_no_rewrites_and_no_config_json(self):
        if not (ROOT/'build/GUNE5D/build_edges.json').exists(): self.skipTest('configure first')
        native.check_snapshot(load_graph(ROOT,'GUNE5D'))
        self.assertEqual(list((ROOT/'config/GUNE5D').glob('*.json')),[])

if __name__=='__main__': unittest.main()
