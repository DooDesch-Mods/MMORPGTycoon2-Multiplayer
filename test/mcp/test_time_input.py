"""Time-input SDK contracts with synthetic native mailbox acknowledgements."""
from pathlib import Path
import sys
import unittest
from unittest.mock import Mock

import test_stdio_region as region_tests
from runtime import RuntimeErrorDetail
from server import dispatch, OP_SCHEMAS, tools_for

TOKEN = "aabbccdd-1234-4321-9876-0123456789ab"
MAXIMUM = 18446744073709551615
VALID = {"key": "SPACE", "expected_epoch": MAXIMUM, "expected_revision": MAXIMUM,
         "expected_request_sequence": MAXIMUM, "acceptance_token": TOKEN}


class TimeInputContract(unittest.TestCase):
    def test_strict_dispatch_before_mailbox(self):
        invalid = [None, [], {}, dict(VALID, extra=True)]
        for field in VALID:
            missing = dict(VALID); del missing[field]; invalid.append(missing)
        for field in ("expected_epoch", "expected_revision", "expected_request_sequence"):
            for value in (True, False, 0, -1, MAXIMUM+1, 1.0, "1", None, float("inf"), float("nan")):
                invalid.append(dict(VALID, **{field: value}))
        for value in ("space", "F5", "", 1, True, [], None):
            invalid.append(dict(VALID, key=value))
        for value in (TOKEN.upper(), TOKEN.replace("-", ""), "{"+TOKEN+"}", TOKEN+"\n", " "+TOKEN, "x"*36, None, 1):
            invalid.append(dict(VALID, acceptance_token=value))
        helper = region_tests.RegionStdioTests()
        helper.check_dispatch("native_time_key", invalid)
        helper.check_dispatch("read_time_acceptance", (None, [], {"extra": 1}))
        helper.check_dispatch("read_time_input", (None, [], {}, {"acceptance_token": TOKEN, "extra": 1},
                                                  {"acceptance_token": TOKEN.upper()}))

    def test_all_five_literals_and_uint64_boundaries_forward_unchanged(self):
        for key in ("SPACE", "F1", "F2", "F3", "F4"):
            for value in (1, MAXIMUM):
                runtime = Mock()
                args = dict(VALID, key=key, expected_epoch=value, expected_revision=value, expected_request_sequence=value)
                dispatch(runtime, "game_native_time_key", args)
                runtime.request.assert_called_once_with("native_time_key", args)

    def test_schema_capability_and_readonly_annotations(self):
        runtime = Mock(); runtime.available.return_value = set()
        self.assertFalse(any(t.name.startswith("game_") for t in tools_for(runtime)))
        runtime.available.return_value = {"read_time_acceptance", "native_time_key", "read_time_input"}
        definitions = {t.name: t for t in tools_for(runtime)}
        for op in runtime.available.return_value:
            definition = definitions["game_"+op]
            self.assertEqual(definition.annotations.readOnlyHint, op != "native_time_key")
            self.assertEqual(definition.annotations.destructiveHint, op == "native_time_key")
            self.assertFalse(definition.annotations.idempotentHint)
            self.assertFalse(definition.inputSchema["additionalProperties"])
        self.assertEqual(OP_SCHEMAS["native_time_key"]["properties"]["expected_epoch"]["maximum"], MAXIMUM)

    def test_readers_stdio_matching_receipts(self):
        helper = region_tests.RegionStdioTests()
        helper.check_stdio("read_time_acceptance", {}, ({"extra": 0},),
                           {"time": {"epoch": MAXIMUM, "phase": "Active"}, "native_calendar_authority_proven": False})
        helper.check_stdio("read_time_input", {"acceptance_token": TOKEN}, ({}, {"acceptance_token": TOKEN.upper()}),
                           {"acceptance_token": TOKEN, "state": "pending", "down": {"queued": True, "consumed": False}})

    def test_native_queue_stdio_does_not_promote_to_confirmed(self):
        region_tests.RegionStdioTests().check_stdio("native_time_key", VALID,
            (dict(VALID, expected_epoch=True), dict(VALID, key="F5"), dict(VALID, acceptance_token=TOKEN.upper())),
            {"acceptance_token": TOKEN, "state": "queued"})

    def test_partial_native_response_keeps_original_cause_and_effect(self):
        region_tests.RegionStdioTests().check_stdio("native_time_key", VALID, (),
            {"acceptance_token": TOKEN, "state": "partial", "down": {"queued": True}, "up": {"queued": False}},
            native_error={"code": "TIME_INPUT_PARTIAL", "message": "SDL key up queue full"})

    def test_runner_ast_endpoint_check(self):
        directory = Path(__file__).resolve().parents[2] / "tools/engine_analysis/calendar_authority/time_acceptance"
        sys.path.insert(0, str(directory))
        try:
            from run_acceptance import sdk_guard
            sdk_guard(Path(__file__).resolve().parents[2] / "tools/mcp/runtime.py")
        finally:
            sys.path.remove(str(directory))


if __name__ == "__main__":
    unittest.main()
