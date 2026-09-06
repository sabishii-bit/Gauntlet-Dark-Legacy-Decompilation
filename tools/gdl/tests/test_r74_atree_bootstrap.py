"""Exercise the real atree configuration block without configuring the repo."""
import ast
from pathlib import Path
import shutil
import subprocess
import tempfile
from types import SimpleNamespace
import unittest


ROOT = Path(__file__).resolve().parents[3]


def export_config(editable, custom=None, windows=False):
    tree = ast.parse((ROOT / 'configure.py').read_text())
    def assigns(node, name):
        return isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == name for t in node.targets)
    start = next(i for i, node in enumerate(tree.body) if assigns(node, 'atree_export_unit'))
    end = next(i for i, node in enumerate(tree.body) if assigns(node, 'p6frank_config'))
    config = SimpleNamespace(binutils_path=custom, build_dir=Path('build'), non_matching=editable,
                             object_postprocesses={'game/anim/atree': {'implicit': [], 'variables': {}}})
    namespace = {'config': config, 'is_windows': lambda: windows}
    exec(compile(ast.Module(body=tree.body[start:end], type_ignores=[]), 'configure.py', 'exec'), namespace)
    return config.object_postprocesses['game/anim/atree']


class AtreeBootstrapTests(unittest.TestCase):
    def test_both_modes_wait_on_managed_download_directory(self):
        for editable in (False, True):
            for windows in (False, True):
                with self.subTest(editable=editable, windows=windows):
                    row = export_config(editable, windows=windows)
                    executable = Path('build/binutils') / ('powerpc-eabi-objcopy.exe' if windows else 'powerpc-eabi-objcopy')
                    self.assertIn(Path('build/binutils'), row['implicit'])
                    self.assertNotIn(executable, row['implicit'])
                    self.assertEqual(row['variables']['atree_objcopy'], executable)

    def test_custom_binutils_wait_on_the_actual_executable(self):
        for editable in (False, True):
            row = export_config(editable, custom=Path('custom-tools'))
            self.assertIn(Path('custom-tools/powerpc-eabi-objcopy'), row['implicit'])
            self.assertNotIn(Path('build/binutils'), row['implicit'])

    @unittest.skipUnless(shutil.which('ninja'), 'ninja is needed for the missing-tool negative control')
    def test_missing_managed_executable_is_not_an_unbuildable_input(self):
        with tempfile.TemporaryDirectory(prefix='r74_atree_bootstrap_') as directory:
            root = Path(directory)
            for editable in (False, True):
                row = export_config(editable)
                dependency = next(p for p in row['implicit'] if isinstance(p, Path))
                for old in (True, False):
                    dep = row['variables']['atree_objcopy'] if old else dependency
                    (root / 'build.ninja').write_text(
                        'rule tool\n  command = echo download\n'
                        'rule export\n  command = echo export\n'
                        'build build/binutils: tool\n'
                        'build atree.o: export | ' + dep.as_posix() + '\n', encoding='utf-8')
                    result = subprocess.run(['ninja', '-n', 'atree.o'], cwd=root,
                                            capture_output=True, text=True, timeout=15)
                    if old:
                        self.assertNotEqual(result.returncode, 0)
                        self.assertIn('missing and no known rule', result.stderr)
                    else:
                        self.assertEqual(result.returncode, 0, result.stderr)
                        self.assertIn('download', result.stdout)
                        self.assertIn('export', result.stdout)


if __name__ == '__main__':
    unittest.main()
