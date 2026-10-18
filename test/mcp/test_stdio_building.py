"""Placement protocol checks through real stdio and isolated runtime fixtures."""
import asyncio
from datetime import timedelta
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock
import uuid

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "mcp"))
from runtime import RuntimeErrorDetail, atomic_json, read_json
from server import dispatch

OPERATIONS = ("probe_building_placement", "place_test_building")
VALID = {"type": "Testhütte", "variant": "default", "x": 12.5, "z": -9}


class BuildingStdioTests(unittest.TestCase):
    def test_strict_dispatch_rejects_invalid_args_before_runtime(self):
        invalid = [dict(VALID, uid=42), {key: value for key, value in VALID.items() if key != "variant"},
                   dict(VALID, type=""), dict(VALID, variant="x" * 257), dict(VALID, x=True),
                   dict(VALID, z="1"), dict(VALID, x=float("nan")), dict(VALID, z=float("inf")),
                   dict(VALID, x=10 ** 1000)]
        for operation in OPERATIONS:
            for arguments in invalid:
                with self.subTest(operation=operation, arguments_type=type(arguments)):
                    runtime = Mock()
                    with self.assertRaises(RuntimeErrorDetail) as caught:
                        dispatch(runtime, f"game_{operation}", arguments)
                    self.assertEqual(caught.exception.code, "invalid_args")
                    runtime.request.assert_not_called()

    def test_stdio_capability_schema_and_matching_acknowledgements(self):
        async def run():
            with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent) as directory:
                root = Path(directory) / "_MPTest"
                host = root / "host"
                diagnostics = host / "diagnostics"
                diagnostics.mkdir(parents=True)
                manifest = {"schema_version": 1, "run_id": str(uuid.uuid4()), "pid": 222,
                            "process_start_time": "133400000000000000", "runtime_root": str(host),
                            "executable_path": str(host / "game" / "MT2.exe")}
                manifest["owner"] = {"kind": "MPTest", "run_id": manifest["run_id"]}
                atomic_json(host / "instance.json", manifest)
                status = {"schema_version": 1, "run_id": manifest["run_id"], "pid": manifest["pid"],
                          "game_thread_ready": True, "capabilities": {operation: False for operation in OPERATIONS}}
                atomic_json(diagnostics / "status.json", status)
                server = Path(__file__).with_name("fixture_stdio_server.py")
                parameters = StdioServerParameters(command=sys.executable,
                                                   args=["-X", "utf8", str(server), "--instances-root", str(root)])
                async with stdio_client(parameters) as (read, write):
                    async with ClientSession(read, write, read_timeout_seconds=timedelta(seconds=10)) as client:
                        await client.initialize()
                        bound = await client.call_tool("bind_runtime", {"manifest_path": str(host / "instance.json")})
                        self.assertFalse(bound.isError)
                        listed = await client.list_tools()
                        names = {tool.name for tool in listed.tools}
                        for operation in OPERATIONS:
                            self.assertNotIn(f"game_{operation}", names)
                            blocked = await client.call_tool(f"game_{operation}", VALID)
                            self.assertTrue(blocked.isError)
                            self.assertIn("capability_unavailable", blocked.content[0].text)
                            self.assertFalse((diagnostics / "request.json").exists())

                        status["capabilities"] = {operation: True for operation in OPERATIONS}
                        atomic_json(diagnostics / "status.json", status)
                        listed = {tool.name: tool for tool in (await client.list_tools()).tools}
                        self.assertTrue(listed["game_probe_building_placement"].annotations.readOnlyHint)
                        self.assertFalse(listed["game_probe_building_placement"].annotations.destructiveHint)
                        self.assertFalse(listed["game_place_test_building"].annotations.readOnlyHint)
                        for operation in OPERATIONS:
                            definition = listed[f"game_{operation}"]
                            self.assertEqual(set(definition.inputSchema["required"]), {"type", "variant", "x", "z"})
                            self.assertFalse(definition.inputSchema["additionalProperties"])
                            self.assertFalse(definition.annotations.openWorldHint)
                            for arguments in (dict(VALID, uid=42), {"type": "hut", "x": 1, "z": 2},
                                              dict(VALID, type=""), dict(VALID, variant="x" * 257),
                                              dict(VALID, x=True), dict(VALID, z="1"),
                                              dict(VALID, x=float("nan")), dict(VALID, z=float("inf"))):
                                rejected = await client.call_tool(f"game_{operation}", arguments)
                                self.assertTrue(rejected.isError)
                                self.assertIn("Input validation error", rejected.content[0].text)
                                self.assertFalse((diagnostics / "request.json").exists())

                        for operation in OPERATIONS:
                            async def acknowledge_fixture(expected_operation=operation):
                                deadline = asyncio.get_running_loop().time() + 5
                                previous_response = read_json(diagnostics / "response.json") if (diagnostics / "response.json").exists() else None
                                while True:
                                    path = diagnostics / "request.json"
                                    request = read_json(path) if path.exists() else None
                                    if request and (previous_response is None or request["id"] != previous_response["id"]):
                                        break
                                    if asyncio.get_running_loop().time() >= deadline:
                                        raise AssertionError("No fresh building mailbox request observed within 5s")
                                    await asyncio.sleep(0.005)
                                self.assertEqual(request["op"], expected_operation)
                                self.assertEqual(request["args"], VALID)
                                result = {"diagnostic": True, "valid": True, "purchase_performed": False, "edit_performed": False}
                                if expected_operation == "place_test_building":
                                    result = {"diagnostic": True, "host_result": {"placed": True}, "local_replica": {"applied": True}}
                                atomic_json(diagnostics / "response.json", {"schema_version": 1, "run_id": request["run_id"],
                                                                            "id": request["id"], "ok": True, "result": result})
                                return request["id"]
                            acknowledgement = asyncio.create_task(acknowledge_fixture())
                            try:
                                confirmed = await client.call_tool(f"game_{operation}", VALID)
                            finally:
                                request_id = await acknowledgement
                            self.assertFalse(confirmed.isError)
                            self.assertEqual(confirmed.structuredContent["id"], request_id)
                            self.assertTrue(confirmed.structuredContent["result"]["diagnostic"])
                            self.assertFalse((diagnostics / "request.lock").exists())
        asyncio.run(run())


if __name__ == "__main__":
    unittest.main()
