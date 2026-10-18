import copy
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from large_group_readback import confirm_group_maps
from local_acceptance import AcceptanceError

class GroupMapProof(unittest.TestCase):
    def setUp(self):
        self.peers = ['host', 'client-1', 'client-2', 'client-3']
        self.capture = {'snapshot_id': '55e543c0-d6bd-4ac7-8672-3ef5491e1b84', 'sha256': 'a' * 64}
        identity = {'ready': True, 'staged_nodes': 240, 'uuid': self.capture['snapshot_id'],
                    'snapshot_sha256': 'a' * 64, 'keys_sha256': 'b' * 64}
        self.engines = {peer: {'network_identity': copy.deepcopy(identity)} for peer in self.peers}

    def test_complete_group(self):
        self.assertEqual(set(confirm_group_maps(self.engines, self.capture, self.peers)), set(self.peers))

    def test_missing_third_client(self):
        del self.engines['client-3']
        with self.assertRaisesRegex(AcceptanceError, 'missing an expected peer'):
            confirm_group_maps(self.engines, self.capture, self.peers)

    def test_third_client_source_file_differs(self):
        self.engines['client-3']['network_identity']['snapshot_sha256'] = 'c' * 64
        with self.assertRaisesRegex(AcceptanceError, 'client-3'):
            confirm_group_maps(self.engines, self.capture, self.peers)

    def test_second_client_map_differs(self):
        self.engines['client-2']['network_identity']['keys_sha256'] = 'd' * 64
        with self.assertRaisesRegex(AcceptanceError, 'client-2'):
            confirm_group_maps(self.engines, self.capture, self.peers)

    def test_equal_empty_digests_do_not_prove_identity(self):
        for engine in self.engines.values():
            engine['network_identity']['keys_sha256'] = ''
        with self.assertRaisesRegex(AcceptanceError, 'host'):
            confirm_group_maps(self.engines, self.capture, self.peers)

    def test_partial_third_client_is_not_ready(self):
        self.engines['client-3']['network_identity'].update(ready=False, staged_nodes=239)
        with self.assertRaisesRegex(AcceptanceError, 'client-3'):
            confirm_group_maps(self.engines, self.capture, self.peers)

if __name__ == '__main__':
    unittest.main()
