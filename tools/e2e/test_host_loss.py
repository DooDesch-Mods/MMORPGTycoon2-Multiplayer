"""Regression checks for Host identity guards and visible disconnection evidence."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.e2e.host_loss import AcceptanceError, ended, owned_host
from local_test import LaunchError


class HostLossChecks(unittest.TestCase):
    def setUp(self):
        self.root = ROOT / '_MPTest'
        self.run_id = 'aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee'
        owner = {'kind': 'MPTest', 'run_id': self.run_id}
        self.host = {'pid': 123, 'process_start_time': '456', 'role': 'game', 'peer_id': 1,
                     'owner': owner, 'run_id': self.run_id, 'runtime_root': str(self.root / 'host'),
                     'executable_path': str(self.root / 'host/game/MT2.exe')}
        self.run = {'schema_version': 1, 'owner': owner, 'run_id': self.run_id,
                    'test_root': str(self.root), 'processes': [self.host]}
        self.identity = {'pid': 123, 'process_start_time': '456'}

    def test_exact_owned_host(self):
        self.assertIs(owned_host(self.run, self.root, self.run_id, self.identity), self.host)

    def test_reused_pid_or_creation_refused(self):
        for identity in ({'pid': 124, 'process_start_time': '456'}, {'pid': 123, 'process_start_time': '457'}):
            with self.assertRaises(AcceptanceError):
                owned_host(self.run, self.root, self.run_id, identity)

    def test_run_change_or_duplicate_refused(self):
        with self.assertRaises(AcceptanceError):
            owned_host(self.run, self.root, 'ffffffff-bbbb-4ccc-8ddd-eeeeeeeeeeee', self.identity)
        run = deepcopy(self.run)
        run['processes'].append(deepcopy(self.host))
        with self.assertRaises(AcceptanceError):
            owned_host(run, self.root, self.run_id, self.identity)

    def test_foreign_folder_and_owner_refused(self):
        for field, value in (('owner', {'kind': 'Other'}), ('executable_path', str(ROOT / 'MT2.exe'))):
            run = deepcopy(self.run)
            run['processes'][0][field] = value
            with self.assertRaises((LaunchError, AcceptanceError)):
                owned_host(run, self.root, self.run_id, self.identity)

    def test_disconnection_requires_visible_notice_and_live_game(self):
        engine = {'session_connected': False, 'peer_count': 0, 'overlay_visible': True,
                  'game_thread_ready': True, 'session_status': 'Sitzung beendet: Host nicht erreichbar'}
        self.assertTrue(ended(engine))
        for field, value in (('session_connected', True), ('peer_count', 1), ('overlay_visible', False),
                             ('game_thread_ready', False), ('session_status', 'Lokale Sitzung verbunden')):
            changed = {**engine, field: value}
            self.assertFalse(ended(changed), field)


if __name__ == '__main__':
    unittest.main()
