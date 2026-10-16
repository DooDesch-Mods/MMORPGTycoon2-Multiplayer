"""Scoped native preparation checks with real SDK stdio and mailbox fixtures."""
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
from server import FLOAT32_MAX, dispatch

VALID = {"x": 12.5, "z": -9}
INVALID = ({}, {"x": 1}, dict(VALID, replay=True), dict(VALID, x=True),
           dict(VALID, z="1"), dict(VALID, x=float("nan")), dict(VALID, z=float("inf")),
           dict(VALID, x=1e100), dict(VALID, z=-1e100), dict(VALID, x=10 ** 1000))
CAMERA_VALID = {"x": 12.5, "y": 3, "z": -9, "distance": 20}
CAMERA_INVALID = ({}, {"x": 1, "y": 2, "z": 3}, dict(CAMERA_VALID, extra=1),
                  dict(CAMERA_VALID, y=True), dict(CAMERA_VALID, z="1"),
                  dict(CAMERA_VALID, x=float("nan")), dict(CAMERA_VALID, y=float("inf")),
                  dict(CAMERA_VALID, z=1e100), dict(CAMERA_VALID, x=10 ** 1000),
                  dict(CAMERA_VALID, distance=0), dict(CAMERA_VALID, distance=-1),
                  dict(CAMERA_VALID, distance=1e-100), dict(CAMERA_VALID, distance=float("inf")))


class RegionStdioTests(unittest.TestCase):
    def test_questgiver_dispatch_uses_existing_npc_routes(self):
        token = "22222222-2222-4222-8222-222222222222"
        for operation in ("native_npc_tool_ui", "native_npc_place_probe", "native_npc_place_ui"):
            valid = {"token": token, "region_id": 67, "tool": "QuestGiver",
                     "character_slot": 0, "character_type": 0}
            if operation == "native_npc_tool_ui":
                valid.update(phase="select")
            else:
                valid.update(position=[12.5, 3, -9])
            invalid = [{}, dict(valid, tool="Unknown"), dict(valid, tool=[]),
                       dict(valid, region_id=True), dict(valid, character_slot=True),
                       dict(valid, token=""), dict(valid, replay=True)]
            if operation == "native_npc_tool_ui":
                invalid.extend((dict(valid, phase=[]), dict(valid, phase="open")))
            else:
                invalid.extend(dict(valid, position=point) for point in
                               ([True, 0, 0], [10 ** 1000, 0, 0], [float("nan"), 0, 0]))
            self.check_dispatch(operation, invalid)
            runtime = Mock()
            dispatch(runtime, f"game_{operation}", valid)
            runtime.request.assert_called_once_with(operation, valid)

    def test_questgiver_stdio_preserves_capability_and_native_acknowledgement(self):
        token = "22222222-2222-4222-8222-222222222222"
        for operation in ("native_npc_tool_ui", "native_npc_place_probe", "native_npc_place_ui"):
            valid = {"token": token, "region_id": 67, "tool": "QuestGiver",
                     "character_slot": 0, "character_type": 0}
            if operation == "native_npc_tool_ui":
                valid.update(phase="select")
            else:
                valid.update(position=[12.5, 3, -9])
            with self.subTest(operation=operation):
                self.check_stdio(operation, valid, (dict(valid, replay=True),),
                                 {"token": token, "attempted": True, "returned": True,
                                  "tool": "QuestGiver"})

    def test_npc_move_dispatch_requires_exact_reference_and_finite_position(self):
        valid = {"token": "22222222-2222-4222-8222-222222222222",
                 "target_action_uuid": "33333333-3333-4333-8333-333333333333",
                 "position": [12.5, 3, -9]}
        invalid = ({}, dict(valid, local_uid=1), dict(valid, token=True),
                   dict(valid, target_action_uuid=""), dict(valid, position=[1, 2]),
                   dict(valid, position=[True, 0, 0]), dict(valid, position=["1", 0, 0]),
                   dict(valid, position=[float("nan"), 0, 0]),
                   dict(valid, position=[float("inf"), 0, 0]),
                   dict(valid, position=[1000001, 0, 0]), dict(valid, position=[10 ** 1000, 0, 0]))
        self.check_dispatch("native_npc_move_ui", invalid)
        runtime = Mock()
        dispatch(runtime, "game_native_npc_move_ui", valid)
        runtime.request.assert_called_once_with("native_npc_move_ui", valid)

    def test_npc_move_stdio_capability_and_actual_acknowledgement(self):
        valid = {"token": "22222222-2222-4222-8222-222222222222",
                 "target_action_uuid": "33333333-3333-4333-8333-333333333333",
                 "position": [12.5, 3, -9]}
        self.check_stdio("native_npc_move_ui", valid, ({}, dict(valid, replay=True)),
                         {"token": valid["token"], "attempted": True, "returned": True,
                          "captured": True, "target_action_uuid": valid["target_action_uuid"]})

    def test_npc_move_stdio_preserves_native_failure_and_unknown(self):
        valid = {"token": "22222222-2222-4222-8222-222222222222",
                 "target_action_uuid": "33333333-3333-4333-8333-333333333333",
                 "position": [12.5, 3, -9]}
        self.check_stdio("native_npc_move_ui", valid, (),
                         {"token": valid["token"], "attempted": True, "unknown": True},
                         native_error={"code": "NPC_MOVE_UI_REJECTED",
                                       "message": "original NPC move returned without capture"})

    def test_npc_remove_dispatch_requires_exact_placement_reference(self):
        valid = {"token": "22222222-2222-4222-8222-222222222222",
                 "target_action_uuid": "33333333-3333-4333-8333-333333333333"}
        invalid = ({}, {"token": valid["token"]}, dict(valid, local_uid=1),
                   dict(valid, token=True), dict(valid, target_action_uuid=7),
                   dict(valid, token="not-a-token"), dict(valid, target_action_uuid=""))
        self.check_dispatch("native_npc_remove_ui", invalid)
        runtime = Mock()
        dispatch(runtime, "game_native_npc_remove_ui", valid)
        runtime.request.assert_called_once_with("native_npc_remove_ui", valid)

    def test_npc_remove_stdio_capability_and_actual_acknowledgement(self):
        valid = {"token": "22222222-2222-4222-8222-222222222222",
                 "target_action_uuid": "33333333-3333-4333-8333-333333333333"}
        self.check_stdio("native_npc_remove_ui", valid, ({}, dict(valid, replay=True)),
                         {"token": valid["token"], "attempted": True, "returned": True,
                          "captured": True, "target_action_uuid": valid["target_action_uuid"]})

    def test_npc_remove_stdio_preserves_native_failure_and_unknown(self):
        valid = {"token": "22222222-2222-4222-8222-222222222222",
                 "target_action_uuid": "33333333-3333-4333-8333-333333333333"}
        self.check_stdio("native_npc_remove_ui", valid, (),
                         {"token": valid["token"], "attempted": True, "unknown": True},
                         native_error={"code": "NPC_REMOVE_UI_REJECTED",
                                       "message": "original NPC command returned without capture"})

    def test_dispatch_rejects_invalid_args_before_runtime(self):
        self.check_dispatch("purchase_test_region", INVALID)
        runtime = Mock()
        dispatch(runtime, "game_purchase_test_region", {"x": FLOAT32_MAX, "z": -FLOAT32_MAX})
        runtime.request.assert_called_once_with("purchase_test_region", {"x": FLOAT32_MAX, "z": -FLOAT32_MAX})

    def test_camera_dispatch_rejects_invalid_args_before_runtime(self):
        self.check_dispatch("focus_test_camera", CAMERA_INVALID)
        runtime = Mock()
        dispatch(runtime, "game_focus_test_camera", CAMERA_VALID)
        runtime.request.assert_called_once_with("focus_test_camera", CAMERA_VALID)

    def check_dispatch(self, operation, invalid):
        for arguments in invalid:
            with self.subTest(arguments_type=type(arguments)):
                runtime = Mock()
                with self.assertRaises(RuntimeErrorDetail) as caught:
                    dispatch(runtime, f"game_{operation}", arguments)
                self.assertEqual(caught.exception.code, "invalid_args")
                runtime.request.assert_not_called()

    def test_stdio_capability_schema_and_matching_acknowledgement(self):
        self.check_stdio("purchase_test_region", VALID, INVALID,
                         {"diagnostic": True, "region_owned": True, "cash_before": 100,
                          "cash_after": 75, "purchase_cost": 25})

    def test_camera_stdio_capability_schema_and_matching_acknowledgement(self):
        self.check_stdio("focus_test_camera", CAMERA_VALID, CAMERA_INVALID,
                         {"diagnostic": True, "state": "camera_focus_observed", "camera": CAMERA_VALID})

    def check_stdio(self, operation, valid, invalid, native_result, native_error=None):
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
                          "game_thread_ready": True, "capabilities": {operation: False}}
                atomic_json(diagnostics / "status.json", status)
                parameters = StdioServerParameters(command=sys.executable,
                    args=["-X", "utf8", str(Path(__file__).with_name("fixture_stdio_server.py")),
                          "--instances-root", str(root)])
                async with stdio_client(parameters) as (read, write):
                    async with ClientSession(read, write, read_timeout_seconds=timedelta(seconds=10)) as client:
                        await client.initialize()
                        bound = await client.call_tool("bind_runtime", {"manifest_path": str(host / "instance.json")})
                        self.assertFalse(bound.isError)
                        self.assertNotIn(f"game_{operation}", {tool.name for tool in (await client.list_tools()).tools})
                        blocked = await client.call_tool(f"game_{operation}", valid)
                        self.assertTrue(blocked.isError)
                        self.assertIn("capability_unavailable", blocked.content[0].text)
                        self.assertFalse((diagnostics / "request.json").exists())
                        status["capabilities"][operation] = True
                        atomic_json(diagnostics / "status.json", status)
                        definition = {tool.name: tool for tool in (await client.list_tools()).tools}[f"game_{operation}"]
                        self.assertEqual(set(definition.inputSchema.get("required", [])), set(valid))
                        self.assertEqual(set(definition.inputSchema["properties"]), set(valid))
                        if operation == "focus_test_camera":
                            self.assertEqual(definition.inputSchema["properties"]["distance"]["exclusiveMinimum"], 0)
                        self.assertFalse(definition.inputSchema["additionalProperties"])
                        read_only=operation in {"read_native_bug_fix_ui", "read_subscriber_weapon", "read_subscriber_needs", "read_network_state", "read_subscriber_candidates", "read_time_ui", "read_building_catalog", "read_time_acceptance", "read_time_input"}
                        self.assertEqual(definition.annotations.readOnlyHint,read_only)
                        self.assertEqual(definition.annotations.destructiveHint,not read_only)
                        self.assertFalse(definition.annotations.openWorldHint)
                        for arguments in invalid:
                            rejected = await client.call_tool(f"game_{operation}", arguments)
                            self.assertTrue(rejected.isError)
                            self.assertFalse((diagnostics / "request.json").exists())
                            self.assertFalse((diagnostics / "request.lock").exists())

                        async def acknowledge_fixture():
                            deadline = asyncio.get_running_loop().time() + 5
                            while not (diagnostics / "request.json").exists():
                                if asyncio.get_running_loop().time() >= deadline:
                                    raise AssertionError(f"No {operation} mailbox request observed within 5s")
                                await asyncio.sleep(0.005)
                            request = read_json(diagnostics / "request.json")
                            self.assertEqual(request["op"], operation)
                            self.assertEqual(request["args"], valid)
                            response = {"schema_version": 1, "run_id": request["run_id"],
                                "id": request["id"], "ok": native_error is None, "result": native_result}
                            if native_error is not None:
                                response["error"] = native_error
                            atomic_json(diagnostics / "response.json", response)
                            return request["id"]
                        acknowledgement = asyncio.create_task(acknowledge_fixture())
                        try:
                            confirmed = await client.call_tool(f"game_{operation}", valid)
                        finally:
                            request_id = await acknowledgement
                        self.assertEqual(confirmed.isError, native_error is not None)
                        response = confirmed.structuredContent
                        if native_error is not None:
                            response = response["native_response"]
                            self.assertFalse(response["ok"])
                            self.assertEqual(response["error"], native_error)
                        self.assertEqual(response["id"], request_id)
                        self.assertEqual(response["result"], native_result)
                        self.assertFalse((diagnostics / "request.lock").exists())
        asyncio.run(run())


if __name__ == "__main__":
    unittest.main()
