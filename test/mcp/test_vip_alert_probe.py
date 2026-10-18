"""Read-only VIP owner probe adapter; no native execution."""
import unittest
from unittest.mock import Mock
from runtime import OPS, MUTATIONS, RuntimeErrorDetail
from server import dispatch, tools_for


class VipAlertProbeTests(unittest.TestCase):
    def test_readonly_capability(self):
        self.assertIn('read_vip_alert_owners', OPS)
        self.assertNotIn('read_vip_alert_owners', MUTATIONS)
        runtime = Mock()
        runtime.available.return_value = {'read_vip_alert_owners'}
        tool = next(t for t in tools_for(runtime) if t.name == 'game_read_vip_alert_owners')
        self.assertTrue(tool.annotations.readOnlyHint)
        self.assertFalse(tool.annotations.destructiveHint)
        self.assertFalse(tool.inputSchema['additionalProperties'])

    def test_empty_request_forwards_exactly_once(self):
        runtime = Mock()
        receipt = {'status': 'observed', 'observed': True, 'read_only': True, 'vip_arrivals': []}
        runtime.request.return_value = receipt
        self.assertIs(dispatch(runtime, 'game_read_vip_alert_owners', {}), receipt)
        runtime.request.assert_called_once_with('read_vip_alert_owners', {})

    def test_pointer_or_effect_arguments_never_reach_runtime(self):
        for args in ({'window': 1}, {'show': True}, {'create': True}, [], None):
            runtime = Mock()
            with self.subTest(args=args), self.assertRaises(RuntimeErrorDetail):
                dispatch(runtime, 'game_read_vip_alert_owners', args)
            runtime.request.assert_not_called()

    def test_absent_and_error_causes_are_not_changed(self):
        for code in ('VIP_ALERT_OWNERS_ABSENT', 'VIP_ALERT_OWNERS_REJECTED'):
            runtime = Mock()
            failure = RuntimeErrorDetail(code, 'original native owner cause')
            runtime.request.side_effect = failure
            with self.subTest(code=code), self.assertRaises(RuntimeErrorDetail) as caught:
                dispatch(runtime, 'game_read_vip_alert_owners', {})
            self.assertIs(caught.exception, failure)


if __name__ == '__main__':
    unittest.main()
