"""Run the build driver with real cache/receipt files and intercepted CMake calls."""
import contextlib
import io
import json
from pathlib import Path
import runpy
import shutil
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import uuid

ROOT = Path(__file__).resolve().parents[1]


class BuildCommandTests(unittest.TestCase):
    def setUp(self):
        self.root = ROOT / 'artifacts/build-command-tests' / uuid.uuid4().hex
        self.root.mkdir(parents=True)
        (self.root / 'tools').mkdir()
        for name in ('build.py', 'build_input_guard.py'):
            shutil.copy2(ROOT / 'tools' / name, self.root / 'tools' / name)
        (self.root / 'mod.json').write_text('{"version":"0.1.0-beta.23"}', encoding='utf-8')
        self.sdk = self.root / 'sdk'
        for relative in ('include/mt2fw/plugin.h', 'adapters/mt2/sdk/native_services.h',
                         'adapters/mt2/sdk/native_routes.h', 'adapters/mt2/sdk/native_ownership.h'):
            path = self.sdk / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('actual-header-file-placeholder', encoding='utf-8')
        self.build = self.root / 'build/framework'
        self.calls = []

    def cache(self, tests):
        self.build.mkdir(parents=True, exist_ok=True)
        values = {'CMAKE_GENERATOR': 'Visual Studio 17 2022', 'CMAKE_GENERATOR_PLATFORM': 'x64',
                  'MT2_FRAMEWORK_SDK': str(self.sdk), 'MT2MP_DIAGNOSTICS': 'ON',
                  'CMAKE_HOME_DIRECTORY': str(self.root)}
        if tests is not None:
            values['MT2MP_BUILD_TESTS'] = tests
        (self.build / 'CMakeCache.txt').write_text('\n'.join(key + ':STRING=' + value for key, value in values.items()), encoding='utf-8')
        (self.build / 'CMakeFiles').mkdir(exist_ok=True)
        (self.build / 'CMakeFiles/generate.stamp').write_text('generated', encoding='utf-8')
        (self.build / 'MT2_Multiplayer.sln').write_text('generated', encoding='utf-8')

    def invoke(self, extra=(), configure_failure=False):
        argv = [str(self.root / 'tools/build.py'), '--framework-sdk', str(self.sdk), *extra]

        def cmake(command, **kwargs):
            self.calls.append(command)
            if '-S' in command:
                if configure_failure:
                    kwargs['stdout'].write('retained configure error')
                    return SimpleNamespace(returncode=9)
                tests = next(arg.split('=', 1)[1] for arg in command if arg.startswith('-DMT2MP_BUILD_TESTS='))
                self.cache(tests)
            else:
                binary = self.build / 'bin/Release/MT2MultiplayerPlugin.dll'
                binary.parent.mkdir(parents=True, exist_ok=True)
                binary.write_bytes(b'intercepted-link-output')
            return SimpleNamespace(returncode=0)

        with patch.object(sys, 'argv', argv), patch.object(sys, 'path', [str(self.root / 'tools'), *sys.path]), \
                patch('shutil.which', return_value='cmake'), patch('subprocess.run', side_effect=cmake), contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            runpy.run_path(argv[0], run_name='__main__')

    def receipt(self):
        return json.loads((self.build / 'build-receipt.json').read_text(encoding='utf-8'))

    def test_default_configures_tests_on(self):
        self.invoke()
        self.assertIn('-DMT2MP_BUILD_TESTS=ON', self.calls[0])
        self.assertNotIn('--target', self.calls[1])
        self.assertTrue(self.receipt()['tests_configured'])
        self.assertFalse(self.receipt()['native_accepted'])

    def test_native_only_selects_dll_and_records_no_test_configuration(self):
        self.invoke(['--native-only'])
        self.assertIn('-DMT2MP_BUILD_TESTS=OFF', self.calls[0])
        self.assertEqual(self.calls[1][-2:], ['--target', 'MT2MultiplayerPlugin'])
        self.assertFalse(self.receipt()['tests_configured'])
        self.assertFalse(self.receipt()['native_accepted'])

    def test_native_only_accepts_explicit_matching_target_and_existing_jobs(self):
        self.invoke(['--native-only', '--target', 'MT2MultiplayerPlugin', '--jobs', '2', '--no-node-reuse'])
        self.assertIn('-DMT2MP_BUILD_TESTS=OFF', self.calls[0])
        self.assertEqual(self.calls[1][self.calls[1].index('--parallel') + 1], '2')
        self.assertEqual(self.calls[1][-2:], ['--', '/nr:false'])

    def test_matching_off_cache_skips_configure_for_native_only(self):
        self.cache('OFF')
        self.invoke(['--native-only'])
        self.assertEqual(len(self.calls), 1)
        self.assertIn('--build', self.calls[0])

    def test_matching_on_cache_skips_configure_for_default(self):
        self.cache('ON')
        self.invoke()
        self.assertEqual(len(self.calls), 1)
        self.assertTrue(self.receipt()['tests_configured'])

    def test_returning_to_default_reconfigures_off_cache_to_on(self):
        self.cache('OFF')
        self.invoke()
        self.assertEqual(len(self.calls), 2)
        self.assertIn('-DMT2MP_BUILD_TESTS=ON', self.calls[0])

    def test_native_only_reconfigures_on_cache_to_off(self):
        self.cache('ON')
        self.invoke(['--native-only'])
        self.assertEqual(len(self.calls), 2)
        self.assertIn('-DMT2MP_BUILD_TESTS=OFF', self.calls[0])

    def test_missing_test_cache_flag_requires_configure(self):
        self.cache(None)
        self.invoke()
        self.assertEqual(len(self.calls), 2)

    def test_reconfigure_flag_still_forces_matching_cache(self):
        self.cache('OFF')
        self.invoke(['--native-only', '--reconfigure'])
        self.assertEqual(len(self.calls), 2)

    def test_other_target_is_rejected_before_any_cmake_call(self):
        with self.assertRaises(SystemExit) as failure:
            self.invoke(['--native-only', '--target', 'test_optional_contract'])
        self.assertEqual(failure.exception.code, 2)
        self.assertEqual(self.calls, [])

    def test_configure_failure_preserves_exit_and_never_links_or_writes_receipt(self):
        with self.assertRaises(SystemExit) as failure:
            self.invoke(['--native-only'], configure_failure=True)
        self.assertEqual(failure.exception.code, 9)
        self.assertEqual(len(self.calls), 1)
        self.assertFalse((self.build / 'build-receipt.json').exists())
        self.assertIn('retained configure error', (self.root / 'artifacts/build-0.log').read_text())


if __name__ == '__main__':
    unittest.main()
