"""Bounded diagnostic reads do not replay a game or publication operation."""
import io
from pathlib import Path
import sys
import time
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "mcp"))
from runtime import JSON_SHARING_WAIT_SECONDS, Runtime, RuntimeErrorDetail, read_json


def windows_error(code):
    error = OSError(f"Windows sharing probe error {code}")
    error.winerror = code
    return error


class JsonReadRetryTests(unittest.TestCase):
    def mailbox(self):
        mailbox = Runtime(Path('_MPTest'))
        mailbox.bound = {'run_id': 'owned-run'}
        mailbox._diagnostics = lambda: Path('_MPTest/host/diagnostics').resolve()
        return mailbox

    def test_response_disappears_after_exists_retains_wait_then_matches(self):
        mailbox = self.mailbox()
        response = b'{"schema_version":1,"id":"request","run_id":"owned-run","ok":true,"result":{}}'
        with patch('runtime.Path.exists', return_value=True), patch('runtime.shared_open', side_effect=[windows_error(2), io.BytesIO(response)]), patch('runtime.atomic_json') as publish:
            with self.assertLogs('mt2_mcp', level='INFO') as logs:
                self.assertIsNone(mailbox._matching_response('request'))
                self.assertEqual(mailbox._matching_response('request')['id'], 'request')
            publish.assert_not_called()
        self.assertIn('lock retained, no replay', logs.output[0])
        self.assertIn('publication recovered', logs.output[1])

    def test_response_other_io_and_json_failures_remain_errors(self):
        for failure in (windows_error(3), windows_error(5), io.BytesIO(b'{bad-json')):
            with self.subTest(failure=failure), patch('runtime.Path.exists', return_value=True), patch('runtime.shared_open', side_effect=[failure]):
                with self.assertRaises(RuntimeErrorDetail) as caught:
                    self.mailbox()._matching_response('request')
                self.assertEqual(caught.exception.code, 'read_failed')

    def test_missing_response_wait_is_counted_bounded_and_never_confirms_foreign_id(self):
        mailbox = self.mailbox()
        foreign = b'{"schema_version":1,"id":"foreign","run_id":"owned-run","ok":true,"result":{}}'
        with patch('runtime.Path.exists', return_value=True), patch('runtime.shared_open', side_effect=[windows_error(2) for _ in range(9)] + [io.BytesIO(foreign)]):
            with self.assertLogs('mt2_mcp', level='INFO') as logs:
                for _ in range(10):
                    self.assertIsNone(mailbox._matching_response('request'))
        self.assertEqual(len(logs.output), 5)
        self.assertIn('count=9', logs.output[-1])

    def test_temporary_sharing_errors_recover_with_original_cause_and_count(self):
        failures = [windows_error(32), windows_error(33)]
        with patch("runtime.shared_open", side_effect=failures + [io.BytesIO(b'{"ok":true}')]) as opened:
            with self.assertLogs("mt2_mcp", level="INFO") as logs:
                self.assertEqual(read_json(Path("response.json")), {"ok": True})
        self.assertEqual(opened.call_count, 3)
        self.assertEqual(sum("sharing wait target=" in line for line in logs.output), 2)
        self.assertIn("failures=2", logs.output[-1])
        self.assertIn("original_cause=Windows sharing probe error 32", logs.output[-1])
        self.assertIn("sharing recovered target=response.json", logs.output[-1])

    def test_persistent_sharing_failure_is_bounded_and_preserves_cause(self):
        failure = windows_error(32)
        started = time.monotonic()
        with patch("runtime.shared_open", side_effect=failure) as opened:
            with self.assertLogs("mt2_mcp", level="INFO") as logs:
                with self.assertRaises(RuntimeErrorDetail) as caught:
                    read_json(Path("response.json"))
        elapsed = time.monotonic() - started
        self.assertGreaterEqual(elapsed, JSON_SHARING_WAIT_SECONDS)
        self.assertLess(elapsed, JSON_SHARING_WAIT_SECONDS + 0.5)
        self.assertGreater(opened.call_count, 1)
        self.assertEqual(caught.exception.code, "read_failed")
        self.assertIs(caught.exception.__cause__, failure)
        self.assertEqual(len(logs.output), 2)
        self.assertIn("ERROR", logs.output[-1])
        self.assertIn("sharing wait exhausted target=response.json", logs.output[-1])
        self.assertIn("required_limit_ms=250.0", logs.output[-1])

    def test_nonsharing_failure_is_immediate_without_wait_or_retry(self):
        failure = windows_error(5)
        with patch("runtime.shared_open", side_effect=failure) as opened:
            with patch("runtime.time.sleep") as waited:
                with self.assertRaises(RuntimeErrorDetail) as caught:
                    read_json(Path("response.json"))
        opened.assert_called_once()
        waited.assert_not_called()
        self.assertEqual(caught.exception.code, "read_failed")
        self.assertIs(caught.exception.__cause__, failure)

    def test_missing_generic_json_is_not_status_publication_wait(self):
        failure=windows_error(2)
        with patch('runtime.shared_open',side_effect=failure) as opened,patch('runtime.time.sleep') as waited:
            with self.assertRaises(RuntimeErrorDetail) as caught:read_json(Path('instance.json'))
        opened.assert_called_once();waited.assert_not_called()
        self.assertEqual(caught.exception.code,'read_failed');self.assertIs(caught.exception.__cause__,failure)


if __name__ == "__main__":
    unittest.main()
