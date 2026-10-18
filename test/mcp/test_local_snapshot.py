"""Local snapshot SDK guards and acknowledgements without game execution."""
import unittest
from unittest.mock import Mock

import test_stdio_region as region_tests
from server import dispatch, tools_for
from runtime import MUTATIONS


class LocalSnapshotTests(unittest.TestCase):
    def test_local_capture_rejects_arguments_before_mailbox(self):
        region_tests.RegionStdioTests().check_dispatch('capture_local_snapshot', (None, [], {'distribute': True}, {'path': 'user/private.vrz'}))
        runtime = Mock()
        dispatch(runtime, 'game_capture_local_snapshot', {})
        runtime.request.assert_called_once_with('capture_local_snapshot', {})

    def test_local_capture_is_not_advertised_as_replayable_read(self):
        runtime = Mock()
        runtime.available.return_value = {'capture_local_snapshot'}
        tool = next(tool for tool in tools_for(runtime) if tool.name == 'game_capture_local_snapshot')
        self.assertIn('capture_local_snapshot', MUTATIONS)
        self.assertFalse(tool.annotations.readOnlyHint)
        self.assertFalse(tool.annotations.idempotentHint)
        self.assertFalse(tool.inputSchema['additionalProperties'])

    def test_local_capture_stdio_ack_keeps_distribution_false(self):
        region_tests.RegionStdioTests().check_stdio('capture_local_snapshot', {}, ({'distribute': True},),
                                                  {'state': 'snapshot_file_complete', 'distribution_requested': False,
                                                   'snapshot_id': '11111111-1111-4111-8111-111111111111', 'bytes': 3})


if __name__ == '__main__':
    unittest.main()
