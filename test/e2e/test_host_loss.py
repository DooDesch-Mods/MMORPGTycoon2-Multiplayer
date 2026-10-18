"""Host-loss admission and native visibility without terminating any process."""
import argparse
import copy
from pathlib import Path
import unittest
from unittest.mock import patch

from tools.e2e import host_loss as subject


def disconnected():
    return {'session_connected': False, 'peer_count': 0, 'game_thread_ready': True,
            'session_status': 'Sitzung beendet: Host nicht erreichbar', 'overlay_visible': False}


def visible_lobby():
    return {'entry_owner': 'framework_native_app', 'panel_registered': True, 'panel_visible': True,
            'framework_ui': {'ready': True, 'open': True, 'visibility_known': True,
                             'visible': True, 'suspended': False}}


class FrameworkHostLossTests(unittest.TestCase):
    def test_framework_control_manifest_and_exact_host_identity(self):
        run_id = 'c8939df3-368c-4868-8aff-652fce18aab6'
        control = subject.ROOT / 'artifacts/host-loss-fixture'
        root = control / 'runs' / run_id
        owner = {'kind': 'MPTest', 'run_id': run_id}
        identity = {'schema_version': 2, 'runtime_kind': 'framework-plugin', 'run_id': run_id,
                    'owner': owner, 'role': 'game', 'peer_id': 1, 'pid': 101,
                    'process_start_time': '123456', 'runtime_root': str(root / 'host'),
                    'executable_path': str(root / 'host/game/MT2.exe')}
        run = {'schema_version': 1, 'run_id': run_id, 'owner': owner,
               'test_root': str(control), 'processes': [identity]}
        self.assertEqual(subject.manifest_root(root, identity), control)
        self.assertEqual(subject.owned_host(run, root, run_id, identity, control), identity)
        for field, wrong in (('pid', 102), ('process_start_time', '999')):
            with self.assertRaises(subject.AcceptanceError):
                subject.owned_host(run, root, run_id, {**identity, field: wrong}, control)
        with self.assertRaises(subject.AcceptanceError):
            subject.manifest_root(control, identity)
        with self.assertRaises(subject.AcceptanceError):
            subject.owned_host(run, root, 'other-run', identity, control)

    def test_legacy_manifest_remains_at_selected_root(self):
        root = subject.ROOT / '_MPTest'
        self.assertEqual(subject.manifest_root(root, {'schema_version': 1}), root)

    def test_framework_requires_confirmed_visible_native_notice(self):
        engine, lobby = disconnected(), visible_lobby()
        self.assertTrue(subject.ended(engine, lobby))
        for key in ('ready', 'open', 'visibility_known', 'visible'):
            changed = copy.deepcopy(lobby)
            changed['framework_ui'][key] = False
            self.assertFalse(subject.ended(engine, changed), key)
        for key in ('panel_registered', 'panel_visible'):
            self.assertFalse(subject.ended(engine, {**lobby, key: False}), key)
        changed = copy.deepcopy(lobby)
        changed['framework_ui']['suspended'] = True
        self.assertFalse(subject.ended(engine, changed))
        self.assertFalse(subject.ended({**engine, 'session_connected': True}, lobby))
        self.assertFalse(subject.ended({**engine, 'session_status': 'Verbunden'}, lobby))

    def test_legacy_notice_still_requires_overlay(self):
        self.assertFalse(subject.ended(disconnected()))
        self.assertTrue(subject.ended({**disconnected(), 'overlay_visible': True}))

    def test_cli_forwards_explicit_root_and_candidate(self):
        with patch.object(subject, 'execute', return_value=0) as execute:
            subject.main(['--setup', 'setup.json', '--freeze', 'freeze.json',
                          '--test-root', 'owned-run', '--candidate-binary', 'MT2MultiplayerPlugin.dll'])
        args = execute.call_args.args[0]
        self.assertEqual(args.test_root, Path('owned-run'))
        self.assertEqual(args.candidate_binary, Path('MT2MultiplayerPlugin.dll'))

    def test_candidate_rejection_precedes_engine_and_process_access(self):
        root, candidate = Path('explicit-owned-run'), Path('MT2MultiplayerPlugin.dll')
        args = argparse.Namespace(test_root=root, candidate_binary=candidate, freeze='freeze', setup='setup')
        class Runner:
            def __init__(self, received):
                self.args, self.summary, self.summary_path = received, {'step': 'preflight'}, Path('no-files-written')
            def persist(self):
                pass
            def inspect(self, *args):
                raise AssertionError('Engine must not be accessed after rejected candidate')
        with patch.object(subject, 'preflight', side_effect=subject.AcceptanceError('wrong candidate')) as admission:
            processes = unittest.mock.Mock(side_effect=AssertionError('No process access'))
            self.assertEqual(subject.execute(args, Runner, processes), 1)
        self.assertEqual(args.test_root, root)
        self.assertEqual(admission.call_args.kwargs['candidate_binary'], candidate)
        processes.assert_not_called()


if __name__ == '__main__':
    unittest.main()
