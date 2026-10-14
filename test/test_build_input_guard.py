"""Regression for a source edit that predates its compiler output timestamp."""
from pathlib import Path
import os
import tempfile
import unittest
from tools.build_input_guard import inputs, require_unchanged


class BuildInputGuardTests(unittest.TestCase):
    def test_mid_build_edit_invalidates_and_forces_next_compile(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'src').mkdir()
            source = root / 'src/test.cpp'
            source.write_text('old', encoding='utf-8')
            before = inputs(root)
            source.write_text('new', encoding='utf-8')
            os.utime(source, (1, 1))
            with self.assertRaisesRegex(RuntimeError, 'candidate invalid: src'):
                require_unchanged(root, before)
            self.assertGreater(source.stat().st_mtime, 1)
            stable = inputs(root)
            self.assertEqual(require_unchanged(root, stable), stable)


if __name__ == '__main__':
    unittest.main()
