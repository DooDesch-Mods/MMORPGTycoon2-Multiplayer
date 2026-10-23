"""Bounded save-overlap acceptance: one mutation, exact file, retained failure."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import uuid
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from retain_live_worlds import AcceptanceError, verify_client_save_wait, observe_client_autosave_wait


def engine(observations=0, recoveries=0, ack=74, waiting=False):
    return {'clock': {'scale': 1}, 'native_world': {'saves': 0}, 'native_open_origin': {'uuid': 'world', 'generation': 4, 'world_epoch': 9, 'origin_world_epoch': 9},
            'typed_actions': {'started': True, 'pending': False, 'locked': False, 'live_failure_terminal': False,
                'live_state': {'active': True, 'locked': False, 'unknown': False, 'economic_fence_locked': False,
                    'application_wait_receipt': {'waiting': waiting, 'observations': observations, 'recoveries': recoveries,
                                                'first_cause': '', 'last_cause': ''}},
                'frame_transport': {'apply_ack': {'peers': [{'peer': 2, 'applied_revision': ack}]}}}}


class Runner:
    def __init__(self, root):
        root.mkdir(parents=True, exist_ok=True)
        self.root, self.manifests = root, {}
        self.args = SimpleNamespace(candidate_binary=root / 'MT2MultiplayerPlugin.dll', wait_seconds=30)
        self.args.candidate_binary.write_bytes(b'candidate')
        self.summary = {'runtime_run_id': '7f6d7c03-d2ab-4099-9fed-b240c5e9e91'}
        self.mutations, self.persisted, self.checks, self.jobs = [], [], [], []
        self.reads = {peer: [engine(), engine(1, 1, 75)] for peer in ('host', 'client-1')}
        self.save_error = None
        for peer in self.reads:
            location = root / peer
            data = location / 'profile/game-data/123/save/World'
            data.mkdir(parents=True)
            manifest = {'schema_version': 2, 'runtime_kind': 'framework-plugin', 'runtime_root': str(location),
                        'framework_profile': str(location / 'profile'), 'owner': {'kind': 'MPTest', 'run_id': self.summary['runtime_run_id']},
                        'installed_binary_sha256': {'MT2MultiplayerPlugin.dll': hashlib.sha256(b'candidate').hexdigest()}}
            self.manifests[peer] = location / 'instance.json'
            self.manifests[peer].write_text(json.dumps(manifest))
        save = root / 'client-1/profile/game-data/123/save/World/test.vrz'
        save.write_bytes(b'exact native file')
        self.saved = {'state': 'snapshot_file_complete', 'distribution_requested': False,
                      'run_id': self.summary['runtime_run_id'], 'snapshot_id': '06704bc1-bf3d-4d30-b740-db927d6370c8',
                      'path': str(save), 'bytes': save.stat().st_size, 'sha256': hashlib.sha256(save.read_bytes()).hexdigest()}

    def inspect(self, name, peer):
        return self.reads[peer].pop(0)

    def single(self, name, peer, tool):
        self.mutations.append((peer, tool))
        if self.save_error:
            raise self.save_error
        return self.saved

    def persist(self):
        self.persisted.append(copy.deepcopy(self.summary))

    def checked(self, name, result):
        self.checks.append((name, copy.deepcopy(result)))
        self.persist()

    def job(self, name, peer, calls, remaining):
        self.jobs.append((name, peer, remaining))
        if calls != [{'tool': 'game_inspect_engine', 'arguments': {}}] or remaining <= 0:
            raise AssertionError('Observer may only inspect within its remaining bound')
        if hasattr(self, 'on_inspect'):
            self.on_inspect(name, peer)
        return {'calls': [{'tool': 'game_inspect_engine', 'state': 'confirmed',
                           'result': {'structuredContent': {'ok': True, 'result': self.inspect(name, peer)}}}]}


class SaveWaitTests(unittest.TestCase):
    def setUp(self):
        owned_tests = Path(__file__).resolve().parents[1] / 'artifacts/test-retain-save'
        owned_tests.mkdir(parents=True, exist_ok=True)
        self.directory = owned_tests / str(uuid.uuid4())
        self.runner = Runner(self.directory)

    def test_confirmed_exact_file_wait_recovery_ack(self):
        result = verify_client_save_wait(self.runner)
        self.assertEqual(result['state'], 'confirmed-client-save-wait')
        self.assertTrue(result['snapshot']['file_verified'])
        self.assertEqual(self.runner.mutations, [('client-1', 'game_capture_local_snapshot')])
        self.assertEqual(result['after_ack_revision'], 75)

    def test_no_overlap_inconclusive_and_never_replayed(self):
        self.runner.reads['client-1'][1] = engine()
        with self.assertRaisesRegex(AcceptanceError, 'without an observed'):
            verify_client_save_wait(self.runner)
        self.assertEqual(self.runner.persisted[-1]['client_save_overlap']['state'], 'inconclusive-no-overlap')
        self.assertEqual(len(self.runner.mutations), 1)

    def test_wait_then_ack_advanced_after_recovery(self):
        self.runner.reads = {peer: [engine(), engine(2, 0, 74, True), engine(3, 1, 75)] for peer in ('host', 'client-1')}
        result = verify_client_save_wait(self.runner)
        self.assertTrue(result['confirmed'])
        self.assertEqual(len(self.runner.mutations), 1)

    def test_missing_ack_does_not_pass(self):
        self.runner.reads['host'][1] = engine(1, 1, 74)
        with patch('retain_live_worlds.time.monotonic', side_effect=[0, 31]):
            with self.assertRaisesRegex(AcceptanceError, 'did not recover'):
                verify_client_save_wait(self.runner)
        self.assertEqual(self.runner.summary['client_save_overlap']['state'], 'failed-no-replay')

    def test_complete_file_hash_required(self):
        self.runner.saved['sha256'] = '0' * 64
        with self.assertRaisesRegex(AcceptanceError, 'size/hash'):
            verify_client_save_wait(self.runner)
        self.assertEqual(len(self.runner.mutations), 1)

    def test_save_failure_retains_one_attempt_without_replay(self):
        self.runner.save_error = AcceptanceError('native save timeout')
        with self.assertRaisesRegex(AcceptanceError, 'native save timeout'):
            verify_client_save_wait(self.runner)
        self.assertEqual(len(self.runner.mutations), 1)
        self.assertIn('native save timeout', self.runner.persisted[-1]['client_save_overlap']['cause'])

    def test_unknown_or_terminal_failure_not_hidden_by_recovery(self):
        for field in ('unknown', 'live_failure_terminal'):
            with self.subTest(field=field):
                runner = Runner(self.directory / field)
                live = runner.reads['client-1'][1]['typed_actions']
                (live['live_state'] if field == 'unknown' else live)[field] = True
                with self.assertRaisesRegex(AcceptanceError, 'healthy settled'):
                    verify_client_save_wait(runner)
                self.assertEqual(len(runner.mutations), 1)

    def test_world_rebind_rejected(self):
        self.runner.reads['client-1'][1]['native_open_origin']['generation'] = 5
        with self.assertRaisesRegex(AcceptanceError, 'identity changed'):
            verify_client_save_wait(self.runner)

    def test_foreign_profile_rejected_before_save(self):
        path = self.runner.manifests['client-1']
        manifest = json.loads(path.read_text())
        manifest['framework_profile'] = str(self.runner.root / 'host/profile')
        path.write_text(json.dumps(manifest))
        with self.assertRaises(AcceptanceError):
            verify_client_save_wait(self.runner)
        self.assertEqual(self.runner.mutations, [])

    def test_foreign_file_rejected(self):
        self.runner.saved['path'] = str(self.runner.root / 'host/profile/game-data/123/save/World/test.vrz')
        with self.assertRaisesRegex(AcceptanceError, 'outside root'):
            verify_client_save_wait(self.runner)

    def test_counter_regression_rejected(self):
        self.runner.reads['client-1'] = [engine(5, 3), engine(1, 1, 75)]
        with self.assertRaisesRegex(AcceptanceError, 'counters regressed'):
            verify_client_save_wait(self.runner)


class NaturalAutosaveTests(unittest.TestCase):
    def setUp(self):
        SaveWaitTests.setUp(self)
        self.path = Path(self.runner.saved['path']).with_name('periodic.vrz')
        self.runner.args.client_autosave_path = self.path
        self.runner.reads = {peer: [engine(), engine(1, 1, 75), engine(1, 1, 76)] for peer in ('host', 'client-1')}
        self.created = False

        def create_once(name, peer):
            if name == 'autosave-before-host' and not self.created:
                self.path.write_bytes(b'native periodic file')
                self.created = True
        self.runner.on_inspect = create_once

    def test_first_save_wait_recovery_and_later_ack(self):
        result = observe_client_autosave_wait(self.runner)
        self.assertTrue(result['confirmed'])
        self.assertEqual(result['save_requests'], 0)
        self.assertFalse(result['periodic_file']['snapshot_ack'])
        self.assertEqual(result['periodic_file']['sha256'], hashlib.sha256(b'native periodic file').hexdigest())
        self.assertEqual((result['first_complete_ack_revision'], result['after_ack_revision']), (75, 76))
        self.assertEqual(self.runner.mutations, [])

    def test_first_complete_without_overlap_stops(self):
        self.runner.reads['client-1'][1] = engine()
        with self.assertRaisesRegex(AcceptanceError, 'without an incoming'):
            observe_client_autosave_wait(self.runner)
        self.assertEqual(self.runner.summary['client_autosave_wait']['state'], 'inconclusive-no-overlap')
        self.assertEqual(len(self.runner.reads['client-1']), 1)
        self.assertEqual(self.runner.mutations, [])

    def test_no_periodic_file_timeout_is_inconclusive(self):
        with patch('retain_live_worlds.time.monotonic', side_effect=[0, 0, 0, 361]):
            with self.assertRaisesRegex(AcceptanceError, 'No complete new periodic'):
                observe_client_autosave_wait(self.runner)
        self.assertEqual(self.runner.summary['client_autosave_wait']['state'], 'inconclusive-no-periodic-save')
        self.assertEqual(self.runner.mutations, [])

    def test_existing_periodic_file_is_not_new_evidence(self):
        self.path.write_bytes(b'old periodic file')
        self.created = True
        with patch('retain_live_worlds.time.monotonic', side_effect=[0, 0, 0, 0, 0, 361]):
            with self.assertRaisesRegex(AcceptanceError, 'No complete new periodic'):
                observe_client_autosave_wait(self.runner)
        self.assertNotIn('periodic_file', self.runner.summary['client_autosave_wait'])

    def test_active_save_waits_for_idle_before_hash(self):
        pending = engine(1, 0, 74, True)
        pending['native_world']['saves'] = 1
        self.runner.reads = {peer: [engine(), copy.deepcopy(pending), engine(2, 1, 75), engine(2, 1, 76)] for peer in ('host', 'client-1')}
        self.assertTrue(observe_client_autosave_wait(self.runner)['confirmed'])
        self.assertEqual(self.runner.mutations, [])

    def test_changed_file_after_first_completion_is_not_substituted(self):
        def change(name, peer):
            if name == 'autosave-before-host':
                self.path.write_bytes(b'first file')
            if name == 'autosave-observe-client-1':
                if self.created:
                    self.path.write_bytes(b'second periodic file')
                self.created = True
        self.runner.on_inspect = change
        with self.assertRaisesRegex(AcceptanceError, 'no later save substitution'):
            observe_client_autosave_wait(self.runner)

    def test_foreign_autosave_path_fails_before_inspection(self):
        self.runner.args.client_autosave_path = self.runner.root / 'host/profile/game-data/123/save/World/periodic.vrz'
        with self.assertRaisesRegex(AcceptanceError, 'outside root'):
            observe_client_autosave_wait(self.runner)
        self.assertEqual(len(self.runner.reads['host']), 3)

    def test_terminal_failure_is_not_an_inconclusive_save(self):
        self.runner.reads['client-1'][1]['typed_actions']['live_failure_terminal'] = True
        with self.assertRaisesRegex(AcceptanceError, 'healthy settled'):
            observe_client_autosave_wait(self.runner)
        self.assertEqual(self.runner.summary['client_autosave_wait']['state'], 'failed-no-replay')

    def test_short_final_budget_stops_without_launching_read(self):
        with patch('retain_live_worlds.time.monotonic', side_effect=[0, 0, 0, 356.49]):
            with self.assertRaisesRegex(AcceptanceError, 'next read requires 60s'):
                observe_client_autosave_wait(self.runner)
        self.assertEqual(len(self.runner.jobs), 2)
        receipt = self.runner.persisted[-1]['client_autosave_wait']
        self.assertEqual(receipt['state'], 'inconclusive-no-periodic-save')
        self.assertAlmostEqual(receipt['stopped_remaining_seconds'], 3.51)
        self.assertEqual(self.runner.mutations, [])

    def test_configured_read_budget_is_preserved_between_peers(self):
        self.runner.args.step_timeout = 10
        with patch('retain_live_worlds.time.monotonic', side_effect=[0, 0, 0, 335, 336]):
            with self.assertRaisesRegex(AcceptanceError, 'next read requires 25s'):
                observe_client_autosave_wait(self.runner)
        self.assertEqual(len(self.runner.jobs), 3)
        self.assertEqual(self.runner.jobs[-1][2], 25)
        self.assertEqual(self.runner.summary['client_autosave_wait']['state'], 'inconclusive-no-periodic-save')

    def test_started_job_failure_stays_failed(self):
        job = self.runner.job
        def failing(name, peer, calls, remaining):
            if name.startswith('autosave-observe'):
                raise AcceptanceError('actual launched MCP job timed out')
            return job(name, peer, calls, remaining)
        self.runner.job = failing
        with self.assertRaisesRegex(AcceptanceError, 'actual launched MCP job timed out'):
            observe_client_autosave_wait(self.runner)
        self.assertEqual(self.runner.summary['client_autosave_wait']['state'], 'failed-no-replay')


if __name__ == '__main__':
    unittest.main()
