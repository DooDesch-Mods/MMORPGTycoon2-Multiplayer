"""A native Buildings-tab request is observed without repeating its input."""
from pathlib import Path
import sys
import unittest
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from tools import placement_ui_acceptance as subject


class Runner:
    def __init__(self, responses):
        self.responses, self.calls, self.summary, self.checks = iter(responses), [], {}, []
    def persist(self):
        pass
    def single(self, label, peer, operation, arguments):
        self.calls.append((operation, arguments))
        value = next(self.responses)
        if isinstance(value, Exception):
            raise value
        return value
    def checked(self, label, value):
        self.checks.append((label, value))


class TabPreparationTests(unittest.TestCase):
    def setUp(self):
        self.origin = {'uuid': '11111111-1111-4111-8111-111111111111', 'world_epoch': 1, 'generation': 2}
    def test_pending_native_layout_is_only_observed(self):
        runner = Runner([{'confirmed': True, 'unknown': False, 'tab_ready': False},
                         {'confirmed': True, 'unknown': False, 'tab_ready': True}])
        self.assertTrue(subject.prepare_buildings_tab(runner, self.origin)['tab_ready'])
        self.assertEqual([call[0] for call in runner.calls], ['game_native_buildings_tab', 'game_read_buildings_tab'])
        request, read = [call[1] for call in runner.calls]
        self.assertEqual(str(uuid.UUID(request['acceptance_token'])), request['acceptance_token'])
        self.assertEqual(read, {'acceptance_token': request['acceptance_token']})
        self.assertEqual(request['snapshot_uuid'], self.origin['uuid'])
        self.assertEqual(request['expected_generation'], 2)
    def test_unknown_effect_blocks_following_input(self):
        runner = Runner([{'confirmed': False, 'unknown': True, 'tab_ready': False}])
        with self.assertRaises(subject.AcceptanceError):
            subject.prepare_buildings_tab(runner, self.origin)
        self.assertEqual(len(runner.calls), 1)
        self.assertFalse(runner.checks)
    def test_read_timeout_does_not_replay_press(self):
        runner = Runner([{'confirmed': True, 'unknown': False, 'tab_ready': False},
                         subject.AcceptanceError('exact request observation timed out')])
        with self.assertRaisesRegex(subject.AcceptanceError, 'exact request observation timed out'):
            subject.prepare_buildings_tab(runner, self.origin)
        self.assertEqual([call[0] for call in runner.calls].count('game_native_buildings_tab'), 1)


if __name__ == '__main__':
    unittest.main()
