"""Retiring a postprocessor must not leave datadiff reading its old body."""
import contextlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.gdl import datadiff
from tools.gdl.tests.test_raw_object import UNIT, graph_fixture


class DatadiffSelectionTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        mocked = patch.object(datadiff, 'REPO', self.root)
        mocked.start()
        self.addCleanup(mocked.stop)

    def test_retired_rule_ignores_stale_body_even_when_newer(self):
        _, _, plain = graph_fixture(self.root)
        obsolete = self.root / 'build/GUNE5D/src/game/example/.postprocess/body/example.o'
        obsolete.parent.mkdir(parents=True)
        obsolete.write_bytes(b'old eight byte constant pool')
        self.assertEqual(datadiff.ours_object(UNIT), self.root / plain)

    def test_active_transform_uses_compiler_ancestor(self):
        for chain in [('webfrank',), ('p6frank',), ('frank', 'webfrank')]:
            with self.subTest(chain=chain):
                _, body, _ = graph_fixture(self.root, chain=chain)
                self.assertEqual(datadiff.ours_object(UNIT), self.root / body)

    def test_missing_active_body_does_not_fall_back_to_processed_object(self):
        _, body, _ = graph_fixture(self.root, chain=('webfrank',))
        (self.root / body).unlink()
        with self.assertRaisesRegex(datadiff.MeasurementUnavailable, 'active object is missing'):
            datadiff.ours_object(UNIT)

    def test_stale_graph_refuses_in_both_cli_modes(self):
        graph_fixture(self.root)
        (self.root / 'build.ninja').write_bytes(b'changed graph')
        for mode in ('--sections', '--deadstrip'):
            with self.subTest(mode=mode), contextlib.redirect_stdout(io.StringIO()) as output:
                with patch.object(datadiff, 'parse_splits', return_value={UNIT + '.c': {}}):
                    self.assertEqual(datadiff.main([mode, UNIT]), 2)
                self.assertIn('UNRESOLVED', output.getvalue())
                self.assertIn('build.ninja differs', output.getvalue())


if __name__ == '__main__':
    unittest.main()
