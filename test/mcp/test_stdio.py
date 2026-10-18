"""Protocol handshake and tool calls against the real standard-I/O server."""
import asyncio
from pathlib import Path
import sys
import tempfile
import unittest
from datetime import timedelta

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client


class StdioTests(unittest.TestCase):
    def test_initialize_list_call_and_errors(self):
        async def run():
            with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent) as directory:
                root = Path(directory) / "_MPTest"
                server = Path(__file__).resolve().parents[2] / "tools" / "mcp" / "server.py"
                parameters = StdioServerParameters(command=sys.executable, args=["-X", "utf8", str(server), "--instances-root", str(root)])
                async with stdio_client(parameters) as (read, write):
                    async with ClientSession(read, write, read_timeout_seconds=timedelta(seconds=10)) as client:
                        result = await client.initialize()
                        self.assertEqual(result.serverInfo.name, "mmorpg-tycoon2-local")
                        self.assertTrue(result.capabilities.tools.listChanged)
                        listed = await client.list_tools()
                        names = {tool.name for tool in listed.tools}
                        self.assertEqual(names, {"list_instances", "bind_runtime", "runtime_status", "read_logs", "observe_pending", "recover_pending"})
                        instances = await client.call_tool("list_instances", {})
                        self.assertFalse(instances.isError)
                        self.assertEqual(instances.structuredContent["instances"], [])
                        unbound = await client.call_tool("runtime_status", {})
                        self.assertTrue(unbound.isError)
                        self.assertIn("not_bound", unbound.content[0].text)
                        unknown = await client.call_tool("execute_python", {})
                        self.assertTrue(unknown.isError)
                        self.assertIn("unknown_tool", unknown.content[0].text)
                        validation = await client.call_tool("read_logs", {"filename": "../../private"})
                        self.assertTrue(validation.isError)
                        self.assertIn("Input validation error", validation.content[0].text)
        asyncio.run(run())


if __name__ == "__main__":
    unittest.main()
