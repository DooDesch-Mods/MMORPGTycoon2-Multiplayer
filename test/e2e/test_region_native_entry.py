"""Exact Framework entry guards for the existing native Region styles runner."""
from pathlib import Path
import unittest
from unittest import mock

from tools.astra_edit_actions import native_region_acceptance as region


class EntryTests(unittest.TestCase):
    def test_explicit_identity_and_legacy_default_reach_shared_preflight_before_native_actions(self):
        command = ['--setup', 'setup.json', '--freeze', 'freeze.json', '--region-id', '67', '--styles-json', 'styles.json']
        for extra, expected_root, expected_candidate in [([], region.ROOT / '_MPTest', None),
                (['--test-root', 'owned/runs/test', '--candidate-binary', 'frozen/MT2MultiplayerPlugin.dll'],
                 Path('owned/runs/test'), Path('frozen/MT2MultiplayerPlugin.dll'))]:
            calls = []
            class Runner:
                def __init__(self, options):
                    self.root = options.test_root
                    self.summary = {'step': 'preflight'}
                    self.summary_path = 'offline-region-styles-summary'
                def inspect(self, *values):
                    calls.append(values)
                    raise AssertionError('native access before selected candidate admission')
                def persist(self):
                    pass
            def reject(runner, freeze, setup, *, candidate_binary):
                self.assertEqual(runner.root, expected_root)
                self.assertEqual(candidate_binary, expected_candidate)
                self.assertEqual((freeze, setup), (Path('freeze.json'), Path('setup.json')))
                raise region.AcceptanceError('exact selected candidate differs')
            with self.subTest(candidate=expected_candidate), mock.patch.object(region, 'AcceptanceRunner', Runner), \
                    mock.patch.object(region, 'preflight', side_effect=reject) as preflight, \
                    mock.patch.object(region, 'read_json', return_value=['Field'] * 4):
                self.assertEqual(region.main(command + extra), 1)
                self.assertEqual(preflight.call_count, 1)
            self.assertEqual(calls, [])


if __name__ == '__main__':
    unittest.main()
