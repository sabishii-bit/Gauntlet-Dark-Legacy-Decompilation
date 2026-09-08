"""Two-sided graph selection tests; phantom artifacts must never decide."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.gdl import raw_object as raw

UNIT = "game/example/example"


def graph_fixture(root, *, unit=UNIT, chain=(), editable=False, outputs=True,
                  native_only=False, policy=b"native policy"):
    """Write a standalone hash-bound graph, independent of the real build.

    `native_only` writes the post-retirement shape: the policy fingerprint is
    `tools/gdl/native_build.py` instead of the two retired rule JSON files,
    and the graph carries the `rules` table `native_build.check_snapshot`
    reads. It is incompatible with `chain`, which is a postprocessing pipeline.
    """
    root = Path(root)
    if native_only and chain:
        raise ValueError("a native-only graph has no postprocessing chain")
    files = {"configure.py": b"generator", "tools/project.py": b"project",
             "tools/gdl/build_provenance.py": b"recorder"}
    if native_only:
        files["tools/gdl/native_build.py"] = policy
    else:
        files.update({"config/GUNE5D/" + name: b"config"
                      for name in ("webfrank.json", "p6frank.json")})
    files.update({"config/GUNE5D/" + name: b"config" for name in
                  ("config.yml", "splits.txt", "symbols.txt")})
    for name, content in files.items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
    (root / "build.ninja").write_bytes(b"recorded test graph\n")
    plain = f"build/GUNE5D/src/{unit}.o"
    directory, stem = unit.rsplit("/", 1)
    body = f"build/GUNE5D/src/{directory}/.postprocess/body/{stem}.o" if chain else plain
    edges = [dict(rule="mwcc_sjis", outputs=[body], inputs=[f"src/{unit}.c"])]
    current = body
    for index, rule in enumerate(chain):
        output = plain if index == len(chain)-1 else f"build/GUNE5D/src/{directory}/.postprocess/{rule}/{stem}.o"
        inputs = [current]
        if rule == "frank":
            profile = f"build/GUNE5D/src/{directory}/.postprocess/profile/{stem}.o"
            edges.append(dict(rule="mwcc_sjis", outputs=[profile], inputs=[f"src/{unit}.c"]))
            inputs.append(profile)
        edges.append(dict(rule=rule, outputs=[output], inputs=inputs))
        current = output
    result = dict(schema_version=1, version="GUNE5D", non_matching=editable,
        ninja_sha256=hashlib.sha256((root/'build.ninja').read_bytes()).hexdigest(),
        generator_inputs={name:hashlib.sha256(value).hexdigest() for name,value in files.items()},
        edges=edges, units=[dict(name=unit+'.c', source_object=plain)])
    if native_only:
        result["native_only"] = True
        result["rules"] = {"mwcc_sjis": dict(command_template="mwcc -c $in -o $out")}
    save_graph(root, result)
    if outputs:
        for edge in edges:
            for name in edge["outputs"]:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(edge["rule"].encode())
    return result, body, plain


def save_graph(root, graph):
    path = Path(root) / 'build/GUNE5D/build_edges.json'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(graph), encoding='utf-8')


class RawObjectTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def test_plain_and_retired_last_pin_ignore_phantom_body_and_frank(self):
        _, body, plain = graph_fixture(self.root)
        for stage in ('body','frank'):
            stale = self.root / f'build/GUNE5D/src/game/example/.postprocess/{stage}/example.o'
            stale.parent.mkdir(parents=True, exist_ok=True)
            stale.write_bytes(b'wrong stale object')
        for view in ('compiler','pre_postprocessor'):
            selected = raw.resolve_object(UNIT, root=self.root, view=view)
            self.assertEqual(selected.relative, plain)
            self.assertIn('(raw)', selected.description)

    def test_active_webfrank_p6_and_runtime_fixup_select_compile_input(self):
        for rule in ('webfrank','p6frank','fix_exception_object','webfrank_globalize_atree'):
            with self.subTest(rule=rule):
                _, body, _ = graph_fixture(self.root, chain=(rule,))
                self.assertEqual(raw.resolve_object(UNIT, root=self.root).relative, body)

    def test_frank_compiler_raw_and_pre_webfrank_are_distinct(self):
        _, body, _ = graph_fixture(self.root, chain=('frank','webfrank'))
        compiler = raw.resolve_object(UNIT, root=self.root)
        pin_input = raw.resolve_object(UNIT, root=self.root, view='pre_postprocessor')
        self.assertEqual(compiler.relative, body)
        self.assertIn('/frank/', pin_input.relative)
        self.assertIn('NOT compiler raw', pin_input.description)
        self.assertEqual(compiler.pipeline, ('mwcc_sjis','frank','webfrank'))

    def test_missing_active_output_buildable_but_not_readable(self):
        _, body, plain = graph_fixture(self.root, chain=('webfrank',), outputs=False)
        path = self.root / plain
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b'processed target exact stale output')
        with self.assertRaisesRegex(raw.RawObjectError, 'active object is missing'):
            raw.resolve_object(UNIT, root=self.root)
        self.assertEqual(raw.resolve_object(UNIT, root=self.root, require_exists=False).relative, body)

    def test_missing_graph_no_fallback_even_when_object_exists(self):
        self.root.joinpath('build.ninja').write_bytes(b'unknown')
        with self.assertRaisesRegex(raw.RawObjectError, 'missing or malformed'):
            raw.resolve_object(UNIT, root=self.root, require_exists=False)

    def test_ninja_hash_or_each_generator_hash_drift_refuses(self):
        graph, _, _ = graph_fixture(self.root)
        for name in ('build.ninja', *graph['generator_inputs']):
            with self.subTest(path=name):
                graph_fixture(self.root)
                (self.root / name).write_bytes(b'changed')
                with self.assertRaises(raw.RawObjectError):
                    raw.resolve_object(UNIT, root=self.root)

    def test_incomplete_manifest_unknown_rule_cycle_duplicate_and_foreign_source(self):
        mutations = [lambda g: g['generator_inputs'].pop('configure.py'),
                     lambda g: g.update(schema_version=999),
                     lambda g: g['edges'][0].update(rule='mystery_compiler'),
                     lambda g: g['edges'].append(g['edges'][0]),
                     lambda g: g['units'].append(g['units'][0]),
                     lambda g: g['edges'][0].update(inputs=['src/foreign.c']),
                     lambda g: g['edges'][0].update(rule='webfrank', inputs=g['edges'][0]['outputs']),
                     lambda g: g['units'][0].update(source_object='../escape.o')]
        for mutate in mutations:
            graph_fixture(self.root)
            graph = json.loads((self.root/'build/GUNE5D/build_edges.json').read_text())
            mutate(graph)
            save_graph(self.root, graph)
            with self.assertRaises(raw.RawObjectError):
                raw.resolve_object(UNIT, root=self.root)

    def test_editable_mode_plain_is_raw_but_target_bound_chain_refused(self):
        _, _, plain = graph_fixture(self.root, editable=True)
        self.assertEqual(raw.resolve_object(UNIT, root=self.root).relative, plain)
        graph_fixture(self.root, editable=True, chain=('webfrank',))
        with self.assertRaisesRegex(raw.RawObjectError, 'editable graph'):
            raw.resolve_object(UNIT, root=self.root)

    def test_legacy_inplace_fixup_cannot_be_called_raw(self):
        graph, body, _ = graph_fixture(self.root)
        graph['edges'].append(dict(rule='fix_exception_objects',
                                   outputs=['build/exception_stamp'], inputs=[body]))
        save_graph(self.root, graph)
        with self.assertRaisesRegex(raw.RawObjectError, 'overwritten in place'):
            raw.resolve_object(UNIT, root=self.root)

    def test_unit_spellings_share_fndiff_normalizer(self):
        _, body, _ = graph_fixture(self.root, chain=('webfrank',))
        for spelling in (UNIT, UNIT+'.c', 'src/'+UNIT+'.c', ('src/'+UNIT+'.c').replace('/','\\')):
            self.assertEqual(raw.resolve_object(spelling, root=self.root).relative, body)

    def test_native_only_graph_selects_the_direct_compiler_output(self):
        _, _, plain = graph_fixture(self.root, native_only=True)
        selected = raw.resolve_object(UNIT, root=self.root)
        self.assertEqual(selected.relative, plain)
        self.assertIn('(raw)', selected.description)
        self.assertEqual(selected.pipeline, ('mwcc_sjis',))

    def test_native_only_graph_without_its_policy_fingerprint_refuses(self):
        # The memo keys on generator_inputs, so a native-only graph that does
        # not inventory tools/gdl/native_build.py must never be accepted: it
        # could otherwise be cached under a fingerprint that omits the policy.
        graph_fixture(self.root, native_only=True)
        graph = json.loads((self.root/'build/GUNE5D/build_edges.json').read_text())
        graph['generator_inputs'].pop('tools/gdl/native_build.py')
        save_graph(self.root, graph)
        with self.assertRaisesRegex(raw.RawObjectError, 'incomplete generator-input'):
            raw.resolve_object(UNIT, root=self.root)

    def test_native_only_graph_refuses_a_rewrite_edge_or_postprocessed_output(self):
        mutations = [
            lambda g: g['edges'].append(dict(rule='webfrank', outputs=['build/x.o'],
                                             inputs=['build/y.o'])),
            lambda g: g['edges'].append(dict(rule='cleanup', outputs=['build/x.o'],
                                             inputs=['build/y.o'])),
            lambda g: g['edges'][0].update(
                outputs=['build/GUNE5D/src/game/example/.postprocess/body/example.o']),
            lambda g: g['edges'][0].update(rule='mystery_compiler'),
        ]
        for index, mutate in enumerate(mutations):
            with self.subTest(mutation=index):
                graph_fixture(self.root, native_only=True)
                graph = json.loads((self.root/'build/GUNE5D/build_edges.json').read_text())
                graph['rules']['cleanup'] = dict(command_template='python tools/gdl/webfrank.py $in')
                mutate(graph)
                save_graph(self.root, graph)
                with self.assertRaises(raw.RawObjectError):
                    raw.resolve_object(UNIT, root=self.root)

    def test_cn_input_and_fnasm_compiler_use_distinct_views(self):
        from tools.gdl import fnasm
        from tools.gdl.composed_census import cn_analyze
        _, body, _ = graph_fixture(self.root, chain=('frank','webfrank'))
        self.assertEqual(fnasm.raw_obj_path(UNIT, root=self.root), self.root/body)
        with patch.object(cn_analyze, 'ROOT', str(self.root)):
            path, label = cn_analyze.our_object(UNIT)
        self.assertIn('/frank/', Path(path).as_posix())
        self.assertIn('NOT compiler raw', label)


if __name__ == '__main__':
    unittest.main()
