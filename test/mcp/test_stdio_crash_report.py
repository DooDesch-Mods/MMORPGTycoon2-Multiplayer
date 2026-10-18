"""Actual SDK stdio calls with isolated process and native-acknowledgement fixtures."""
import asyncio
from datetime import timedelta
from pathlib import Path
import sys
import tempfile
import unittest
import uuid

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "mcp"))
from runtime import atomic_json, read_json


class CrashReportStdioTests(unittest.TestCase):
    def test_capability_block_schema_and_matching_acknowledgement(self):
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
                          "game_thread_ready": True, "capabilities": {"dismiss_crash_report": False}, "crash_mode_active": False}
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
                        self.assertNotIn("game_dismiss_crash_report", {tool.name for tool in listed.tools})
                        blocked = await client.call_tool("game_dismiss_crash_report", {})
                        self.assertTrue(blocked.isError)
                        self.assertIn("capability_unavailable", blocked.content[0].text)
                        self.assertFalse((diagnostics / "request.json").exists())

                        status["crash_mode_active"] = True
                        status["capabilities"]["dismiss_crash_report"] = True
                        atomic_json(diagnostics / "status.json", status)
                        listed = await client.list_tools()
                        definition = next(tool for tool in listed.tools if tool.name == "game_dismiss_crash_report")
                        self.assertFalse(definition.annotations.readOnlyHint)
                        self.assertFalse(definition.annotations.openWorldHint)
                        rejected = await client.call_tool("game_dismiss_crash_report", {"upload": True})
                        self.assertTrue(rejected.isError)
                        self.assertIn("Input validation error", rejected.content[0].text)
                        self.assertFalse((diagnostics / "request.json").exists())

                        async def acknowledge_fixture():
                            deadline = asyncio.get_running_loop().time() + 5
                            while not (diagnostics / "request.json").exists():
                                if asyncio.get_running_loop().time() >= deadline:
                                    raise AssertionError("No crash-report mailbox request observed within 5s")
                                await asyncio.sleep(0.005)
                            request = read_json(diagnostics / "request.json")
                            self.assertEqual(request["op"], "dismiss_crash_report")
                            self.assertEqual(request["args"], {})
                            self.assertEqual(request["run_id"], manifest["run_id"])
                            status["crash_mode_active"] = False
                            status["capabilities"]["dismiss_crash_report"] = False
                            atomic_json(diagnostics / "status.json", status)
                            atomic_json(diagnostics / "response.json", {"schema_version": 1, "run_id": request["run_id"],
                                                                        "id": request["id"], "ok": True,
                                                                        "result": {"dismissed": True, "route": "CrashReport::Command/Cancel", "crash_mode_active": False}})
                            return request["id"]

                        acknowledgement = asyncio.create_task(acknowledge_fixture())
                        try:
                            confirmed = await client.call_tool("game_dismiss_crash_report", {})
                        finally:
                            request_id = await acknowledgement
                        self.assertFalse(confirmed.isError)
                        self.assertEqual(confirmed.structuredContent["id"], request_id)
                        self.assertTrue(confirmed.structuredContent["result"]["dismissed"])
                        self.assertFalse(confirmed.structuredContent["result"]["crash_mode_active"])
                        self.assertFalse((diagnostics / "request.lock").exists())
                        listed = await client.list_tools()
                        self.assertNotIn("game_dismiss_crash_report", {tool.name for tool in listed.tools})
        asyncio.run(run())


if __name__ == "__main__":
    unittest.main()
