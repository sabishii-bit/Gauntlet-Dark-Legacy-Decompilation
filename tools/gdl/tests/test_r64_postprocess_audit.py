"""Safety gates for the finite compiler-necessity investigation."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.gdl.composed_census import r64_postprocess_audit as audit


class AuditSafetyTests(unittest.TestCase):
    def test_fidelity_drift_refuses_before_variants(self):
        with tempfile.TemporaryDirectory(prefix='r64_test_') as td:
            root = Path(td)
            (root/'body.o').write_bytes(b'actual shipped object')
            edge = dict(body_o='body.o')
            with patch.object(audit, 'ROOT', root), \
                 patch.object(audit, 'OUT', root/'outputs'), \
                 patch.object(audit.cv, 'read_edges', return_value={'game/sys/sysservice':edge}), \
                 patch.object(audit, 'compile_row', return_value=dict(error=None,object_sha256='wrong')) as compile_one, \
                 patch.object(audit.cv, 'variants', side_effect=AssertionError('variants reached')), \
                 patch.object(audit.sys, 'argv', ['audit']):
                with self.assertRaisesRegex(SystemExit, 'baseline object differs'):
                    audit.main()
                self.assertEqual(compile_one.call_count, 1)

    def test_baseline_compile_failure_refuses(self):
        with tempfile.TemporaryDirectory(prefix='r64_test_') as td:
            with patch.object(audit, 'OUT', Path(td)), \
                 patch.object(audit.cv, 'read_edges', return_value={'game/sys/sysservice':{}}), \
                 patch.object(audit, 'compile_row', return_value=dict(error='intentional failure')) as compile_one, \
                 patch.object(audit.sys, 'argv', ['audit']):
                with self.assertRaisesRegex(SystemExit, 'REFUSED baseline compile'):
                    audit.main()
                self.assertEqual(compile_one.call_count, 1)

    def test_mocked_negative_control_does_not_launch_a_compiler(self):
        with patch.object(audit.cv.subprocess, 'run', side_effect=AssertionError('real process launched')):
            result = audit.fidelity_negative_control()
        self.assertIn('synthetic', result['kind'])
        self.assertIn('DIFFERS', result['output'])
        self.assertGreaterEqual(result['compile_calls'], 1)
        # Characterize, do not require, the upstream bug: a repaired cv_probe
        # may legitimately refuse here, and this audit should report that.
        if result['returncode'] == 0:
            self.assertGreater(result['compile_calls'], 1)


if __name__ == '__main__':
    unittest.main()
