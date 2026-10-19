"""Exercise the real saved-world setup boundary without game or MCP calls."""
import ast
import hashlib
from pathlib import Path
from types import SimpleNamespace
import unittest


class SavedFixtureSetup(unittest.TestCase):
    def run_setup(self, *, pending=2, discovery_ok=True, worlds=None, client_snapshot='shared', never_ready=False):
        root = Path(__file__).resolve().parents[2]
        tree = ast.parse((root / 'tools/lobby_ui/creation_acceptance.py').read_text(encoding='utf-8'))
        function = next(node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name == 'load_saved_fixture')
        events = []
        worlds = ['fixture'] if worlds is None else worlds
        ready = {'discovery_callback_completed': False, 'discovery_pending': False,
                 'discovery_callback_ok': False, 'discovery_state': 'Saved worlds' if discovery_ok else 'original discovery failure',
                 'saved_worlds': worlds, 'ui_page_size': 2}

        def require(value, cause):
            if not value:
                raise RuntimeError(cause)

        class UI:
            def __init__(self, peer):
                self.peer = peer
                self.page = 0

            def wait(self, predicate, label):
                if label == 'saved-fixture-discovery-complete':
                    waiting = {**ready, 'discovery_callback_completed': False, 'discovery_pending': True, 'saved_worlds': []}
                    for _ in range(pending):
                        events.append('pending')
                        require(not predicate(waiting), 'pending discovery was accepted')
                    require(not never_ready, 'discovery never completed')
                    events.append('discovered')
                    require(predicate(ready), 'completed discovery was rejected')
                    return ready
                value = {'prepare': {'playing': True, 'snapshot_id': 'shared' if self.peer == 'host' else client_snapshot}}
                require(predicate(value), 'playing predicate rejected actual state')
                return value

            def click(self, target, predicate):
                require('discovered' in events, 'world selection preceded discovery')
                events.append(target)
                if target == 'list_next':
                    self.page += 1
                    value = {'discovery_page': self.page}
                else:
                    value = {'selected_world': 'fixture', 'prepare': {'start_requested': True, 'playing': False}}
                require(predicate(value), 'native click result rejected')
                return value

        binary = SimpleNamespace(read_bytes=lambda: b'fixture candidate')
        runner = SimpleNamespace(summary={}, summary_path='retained.json', persist=lambda: None)
        namespace = {'args': SimpleNamespace(saved_world_setup='fixture', candidate_binary=binary),
                     'runner': runner, 'require': require, 'utc': lambda: 'now', 'hashlib': hashlib, 'print': lambda *args: None}
        exec(compile(ast.Module(body=[function], type_ignores=[]), 'actual_saved_fixture_setup', 'exec'), namespace)
        try:
            namespace['load_saved_fixture'](UI('host'), UI('client'))
        except RuntimeError as error:
            return events, runner.summary, str(error)
        return events, runner.summary, None

    def test_pending_discovery_precedes_selection(self):
        events, result, error = self.run_setup()
        self.assertIsNone(error)
        self.assertEqual(events, ['pending', 'pending', 'discovered', 'list0'])
        self.assertTrue(result['passed'])

    def test_discovery_failure_preserves_cause_and_prevents_selection(self):
        events, result, error = self.run_setup(discovery_ok=False)
        self.assertIn('original discovery failure', error)
        self.assertNotIn('list0', events)
        self.assertFalse(result.get('passed', False))

    def test_missing_or_duplicate_world_never_selects(self):
        for worlds in [[], ['fixture', 'fixture']]:
            with self.subTest(worlds=worlds):
                events, _, error = self.run_setup(worlds=worlds)
                self.assertIn('absent or duplicated', error)
                self.assertNotIn('list0', events)

    def test_pagination_uses_completed_list(self):
        events, result, error = self.run_setup(worlds=['a', 'b', 'c', 'd', 'fixture'])
        self.assertIsNone(error)
        self.assertEqual(events[-3:], ['list_next', 'list_next', 'list0'])
        self.assertTrue(result['passed'])

    def test_snapshot_mismatch_is_not_success(self):
        _, result, error = self.run_setup(client_snapshot='other')
        self.assertIn('same snapshot', error)
        self.assertFalse(result.get('passed', False))

    def test_no_completed_callback_never_selects(self):
        events, _, error = self.run_setup(never_ready=True)
        self.assertIn('never completed', error)
        self.assertNotIn('list0', events)


if __name__ == '__main__':
    unittest.main()
