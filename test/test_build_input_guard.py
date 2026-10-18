"""Regression for a source edit that predates its compiler output timestamp."""
from pathlib import Path
import os
import tempfile
import unittest
from tools.build_input_guard import inputs, require_unchanged


class BuildInputGuardTests(unittest.TestCase):
    def test_framework_recipe_sdk_and_assembly_edits_invalidate_candidate(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / 'mod'
            sdk = Path(directory) / 'sdk'
            relative = ('include/mt2fw/plugin.h', 'adapters/mt2/sdk/native_services.h',
                        'adapters/mt2/sdk/native_routes.h', 'adapters/mt2/sdk/native_ownership.h')
            files = [root / 'cmake/framework_plugin.cmake', root / 'src/native.asm']
            files.extend(sdk / name for name in relative)
            for path in files:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('original', encoding='utf-8')
            for path in files:
                with self.subTest(path=path):
                    before = inputs(root, sdk)
                    path.write_text('changed', encoding='utf-8')
                    os.utime(path, (1, 1))
                    with self.assertRaisesRegex(RuntimeError, 'candidate invalid:'):
                        require_unchanged(root, before, sdk)
                    self.assertGreater(path.stat().st_mtime, 1)
                    stable = inputs(root, sdk)
                    self.assertEqual(require_unchanged(root, stable, sdk), stable)

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
