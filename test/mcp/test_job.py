import asyncio
from contextlib import asynccontextmanager
from pathlib import Path
import sys
import tempfile
import unittest

from mcp import types

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "mcp"))
from run_job import execute_job, load_spec
from runtime import RuntimeErrorDetail, atomic_json
from error_details import error_details, original_causes


class JobTests(unittest.TestCase):
    def test_nested_exception_group_preserves_original_timeout(self):
        failure = ExceptionGroup("stdio teardown", [ExceptionGroup("request", [TimeoutError("No response after 80 seconds")])])
        details = error_details(failure)
        self.assertEqual(original_causes(details), ["TimeoutError: No response after 80 seconds"])
        self.assertEqual(len(details["exceptions"]), 1)
    def test_unknown_code_tool_rejected_before_execution(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent) as directory:
            spec = Path(directory) / "spec.json"
            atomic_json(spec, {"calls": [{"tool": "execute_script", "arguments": {"code": "arbitrary"}}]})
            with self.assertRaises(RuntimeErrorDetail) as caught:
                load_spec(spec)
            self.assertEqual(caught.exception.code, "invalid_job")

    def test_bounded_timeout_and_owned_operations(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent) as directory:
            spec = Path(directory) / "spec.json"
            atomic_json(spec, {"timeout_seconds": 65, "calls": [{"tool": "game_create_test_world"}, {"tool": "game_capture_frame"}]})
            self.assertEqual(load_spec(spec)["timeout_seconds"], 65)
            atomic_json(spec, {"timeout_seconds": 66, "calls": [{"tool": "runtime_status"}]})
            with self.assertRaises(RuntimeErrorDetail):
                load_spec(spec)

    def test_first_failed_response_stops_job_without_replay(self):
        requested = []
        class Client:
            async def initialize(self):
                return types.InitializeResult(protocolVersion="2025-11-25", capabilities=types.ServerCapabilities(),
                                              serverInfo=types.Implementation(name="fixture", version="1"))
            async def call_tool(self, name, arguments):
                requested.append(name)
                return types.CallToolResult(content=[types.TextContent(type="text", text="fixture error" if name == "game_capture_snapshot" else "ack")],
                                            isError=name == "game_capture_snapshot")
        @asynccontextmanager
        async def session():
            yield Client()
        evidence = {"calls": []}
        persisted = []
        spec = {"timeout_seconds": 60, "calls": [{"tool": "game_capture_snapshot"}, {"tool": "game_load_snapshot", "arguments": {"snapshot_id": "saved"}}]}
        with self.assertRaises(RuntimeErrorDetail):
            asyncio.run(execute_job(Path("_MPTest/host/instance.json"), spec, evidence,
                                    lambda: persisted.append(evidence["step"]), None, session_factory=session))
        self.assertEqual(requested, ["bind_runtime", "game_capture_snapshot"])
        self.assertEqual(evidence["calls"][-1]["state"], "failed")
        self.assertEqual(evidence["calls"][0]["state"], "confirmed")
        self.assertGreater(len(persisted), 2)


if __name__ == "__main__":
    unittest.main()
