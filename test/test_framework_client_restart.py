"""Exercise the real restart implementation with existing owned-launcher fixtures."""
import importlib.util
from pathlib import Path
import sys
from types import SimpleNamespace
import unittest
from unittest import mock
import uuid

HERE = Path(__file__).resolve().parent
TOOLS = HERE / 'overlay/tools' if (HERE / 'overlay/tools').is_dir() else HERE.parent / 'tools'
SOURCE = HERE.parents[1] / 'release-next/multiplayer-source' if (HERE / 'overlay/tools').is_dir() else HERE.parent
sys.path.insert(0, str(TOOLS))


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    sys.modules[name] = result
    spec.loader.exec_module(result)
    return result


launcher = module('restart_launcher', TOOLS / 'framework_local_test.py')
launcher.ROOT = SOURCE
base = module('existing_launcher_fixture', SOURCE / 'test/test_framework_local_launcher.py')
base.launcher = launcher
base.__file__ = str(TOOLS.parent / 'test/test_framework_local_launcher.py')
(TOOLS.parent / 'test').mkdir(parents=True, exist_ok=True)
from framework_client_restart import restart_client


class RestartTests(unittest.TestCase):
    def setUp(self):
        def retained_directory(**kwargs):
            path = HERE / 'test-runs' / uuid.uuid4().hex
            path.mkdir(parents=True)
            # Keep private fixture artifacts; default inherited ACLs work in the
            # Windows sandbox whereas tempfile's restricted ACL does not.
            return SimpleNamespace(name=str(path), cleanup=lambda: None)
        with mock.patch.object(base.tempfile, 'TemporaryDirectory', side_effect=retained_directory):
            base.FrameworkLauncherTests.setUp(self)
    fake_popen = base.FrameworkLauncherTests.fake_popen
    launch = base.FrameworkLauncherTests.launch

    def fixture(self):
        _, self.before = self.launch()
        self.args.expected_run_id = self.before['run_id']
        self.client = {row['role']: row for row in self.before['processes'] if row['peer_id'] == 2}
        self.runtime = Path(self.client['game']['runtime_root'])
        self.inspections = []
        self.states = {row['pid']: 'alive' for row in self.before['processes']}
        self.processes = mock.Mock()
        def inspect(record, terminate=False):
            self.inspections.append((record['pid'], terminate))
            state = self.states.get(record['pid'], 'alive')
            if terminate and state == 'alive':
                self.states[record['pid']] = 'exited:1'
                return 'stopped:1'
            return state
        self.processes.inspect.side_effect = inspect
        self.processes.identity.side_effect = lambda process: {
            'pid': process.pid, 'process_start_time': str(process.pid + 5000),
            'executable_path': next(command[0] for p, command, _ in self.started if p is process)}
        self.save = self.runtime / 'profile/game-data/existing-save.vrz'
        self.save.write_bytes(b'preserve client data')
        return self.before

    def restart(self, fake=None, ports=None):
        with mock.patch.object(launcher.subprocess, 'Popen', side_effect=fake or self.fake_popen), \
                mock.patch.object(launcher, 'reserve_ports', side_effect=ports or (lambda values: [mock.Mock()])):
            return restart_client(self.args, self.processes, launcher)

    def assert_retained(self):
        current = launcher.read_json(self.root / 'run.json')
        self.assertEqual([row for row in current['processes'] if row['peer_id'] != 2],
                         [row for row in self.before['processes'] if row['peer_id'] != 2])
        self.assertTrue(all(pid in {row['pid'] for row in self.client.values()} for pid, terminate in self.inspections if terminate))
        self.assertEqual(self.save.read_bytes(), b'preserve client data')
        return current

    def test_restart_only_client_pair_preserves_host_relay_profile_and_config(self):
        self.fixture()
        config = (self.runtime / 'profile/data/doodesch.multiplayer/local-test.json').read_bytes()
        self.assertEqual(self.restart(), 0)
        current = self.assert_retained()
        self.assertEqual(current['state'], 'transport-ready')
        self.assertEqual(len(self.started), 7)
        self.assertEqual([row['role'] for row in current['client_restarts'][0]['new_processes']], ['bridge', 'game'])
        self.assertEqual(current['client_restarts'][0]['state'], 'confirmed-new-client-transport')
        for record in current['client_restarts'][0]['new_processes']:
            self.assertNotEqual(record['pid'], self.client[record['role']]['pid'])
            self.assertNotEqual(record['process_start_time'], self.client[record['role']]['process_start_time'])
        self.assertEqual(launcher.read_json(self.runtime / 'instance.json')['pid'], self.started[-1][0].pid)
        self.assertEqual((self.runtime / 'profile/data/doodesch.multiplayer/local-test.json').read_bytes(), config)
        archive = self.runtime / 'restarts' / current['client_restarts'][0]['id']
        self.assertEqual(launcher.read_json(archive / 'before.json')['processes'], list(self.client.values()))
        launcher.validate_manifest(current, self.root)

    def test_restart_already_exited_client_is_not_treated_as_live(self):
        self.fixture()
        for row in self.client.values(): self.states[row['pid']] = 'exited:0'
        self.assertEqual(self.restart(), 0)
        self.assert_retained()

    def test_restart_mismatched_client_identity_refuses_before_any_termination(self):
        self.fixture(); self.states[self.client['game']['pid']] = 'identity-mismatch'
        with self.assertRaisesRegex(launcher.LaunchError, 'unknown game identity'):
            self.restart()
        self.assertFalse(any(stop for _, stop in self.inspections))
        self.assertEqual(len(self.started), 5)
        self.assertEqual(launcher.read_json(self.root / 'run.json'), self.before)

    def test_restart_requires_live_unchanged_host_and_exact_run(self):
        self.fixture(); self.args.expected_run_id = 'other'
        with self.assertRaisesRegex(launcher.LaunchError, 'exact transport-ready'):
            self.restart()
        self.assertEqual(self.inspections, [])
        self.args.expected_run_id = self.before['run_id']
        host = next(row for row in self.before['processes'] if row['role'] == 'game' and row['peer_id'] == 1)
        self.states[host['pid']] = 'identity-mismatch'
        with self.assertRaisesRegex(launcher.LaunchError, 'unchanged live game'):
            self.restart()
        self.assertFalse(any(stop for _, stop in self.inspections))

    def test_restart_pending_requests_remain_and_prevent_process_mutation(self):
        self.fixture()
        request = self.runtime / 'profile/diagnostics/request.json'
        launcher.atomic_json(request, {'id': 'unacknowledged', 'pid': self.client['game']['pid'], 'process_start_time': self.client['game']['process_start_time']})
        with self.assertRaisesRegex(launcher.LaunchError, 'unacknowledged Framework'):
            self.restart()
        self.assertTrue(request.exists())
        self.assertFalse(any(stop for _, stop in self.inspections))

    def test_restart_changed_binary_is_rejected_before_stop(self):
        self.fixture()
        (self.runtime / launcher.MODULES['MT2MultiplayerPlugin.dll']).write_bytes(b'changed')
        with self.assertRaisesRegex(launcher.LaunchError, 'installed binary changed'):
            self.restart()
        self.assertFalse(any(stop for _, stop in self.inspections))

    def test_restart_occupied_control_port_retains_partial_failure_without_launch(self):
        self.fixture()
        with self.assertRaisesRegex(launcher.LaunchError, 'occupied fixture control port'):
            self.restart(ports=mock.Mock(side_effect=launcher.LaunchError('occupied fixture control port')))
        result = self.assert_retained()
        self.assertEqual(result['state'], 'client-restart-failed')
        self.assertIn('occupied fixture control port', result['failure'])
        self.assertEqual(len(self.started), 5)

    def test_restart_failed_new_game_cleans_only_new_handles_and_retains_cause(self):
        self.fixture()
        def fail_game(command, **kwargs):
            if Path(command[0]).name == 'MT2.exe': raise OSError('restart game launch denied')
            return self.fake_popen(command, **kwargs)
        with self.assertRaisesRegex(OSError, 'restart game launch denied'):
            self.restart(fake=fail_game)
        result = self.assert_retained()
        self.assertEqual(len(self.started), 6)
        self.started[-1][0].kill.assert_called_once()
        self.assertTrue(all(not process.kill.called for process, _, _ in self.started[:5]))
        self.assertEqual(result['client_restarts'][0]['error'], 'restart game launch denied')
        launcher.validate_manifest(result, self.root)

    def test_restart_stop_identity_race_does_not_start_or_kill_foreign_process(self):
        self.fixture()
        original = self.processes.inspect.side_effect
        self.processes.inspect.side_effect = lambda row, terminate=False: 'identity-mismatch' if terminate else original(row)
        with self.assertRaisesRegex(launcher.LaunchError, 'stop refused role=game'):
            self.restart()
        result = self.assert_retained()
        self.assertEqual(result['state'], 'client-restart-failed')
        self.assertEqual(len(self.started), 5)

    def test_restart_new_identity_failure_cleans_exact_new_handle_and_records_attempt(self):
        self.fixture()
        self.processes.identity.side_effect = OSError('new bridge identity unavailable')
        with self.assertRaisesRegex(OSError, 'new bridge identity unavailable'):
            self.restart()
        result = self.assert_retained()
        self.assertEqual(len(self.started), 6)
        self.started[-1][0].kill.assert_called_once()
        receipt = result['client_restarts'][0]
        self.assertEqual(receipt['new_processes'], [])
        self.assertEqual(receipt['process_attempts'][0]['pid'], self.started[-1][0].pid)
        self.assertEqual(receipt['process_attempts'][0]['exit_code'], -9)

    def test_restart_persistence_failure_keeps_original_error_and_cleans_new_handle(self):
        self.fixture()
        write = launcher.atomic_json
        persistence_broken = False
        def fail_game(command, **kwargs):
            nonlocal persistence_broken
            if Path(command[0]).name == 'MT2.exe':
                persistence_broken = True
                raise OSError('original game start failure')
            return self.fake_popen(command, **kwargs)
        def fail_receipt(path, value):
            if persistence_broken:
                raise OSError('receipt disk full')
            return write(path, value)
        with mock.patch.object(launcher, 'atomic_json', side_effect=fail_receipt), \
                mock.patch.object(launcher, 'log') as log:
            with self.assertRaisesRegex(OSError, 'original game start failure') as caught:
                self.restart(fake=fail_game)
        self.assertIn('receipt disk full', caught.exception.__notes__[0])
        self.assertIn('original game start failure', log.call_args.args[1])
        self.started[-1][0].kill.assert_called_once()
        self.assertTrue(all(not process.kill.called for process, _, _ in self.started[:5]))
        self.assert_retained()


if __name__ == '__main__':
    unittest.main()
