"""Regression checks for visible disconnection evidence."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.e2e.host_loss import ended


class HostLossChecks(unittest.TestCase):
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
