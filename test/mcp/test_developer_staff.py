"""Exercise the real staged server dispatch without a native runtime."""
from pathlib import Path
import copy
import importlib.util
import sys
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(ROOT / "tools/mcp"))
if (HERE / "candidate/tools/mcp").is_dir():
    sys.path.insert(0, str(HERE / "candidate/tools/mcp"))


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


SDK = HERE / "integration/after/tools/mcp"
if not SDK.is_dir():
    SDK = ROOT / "tools/mcp"
runtime = load("runtime", SDK / "runtime.py")
server = load("developer_fixture_test_server", SDK / "server.py")


class NoNativeRuntime:
    def __init__(self):
        self.calls = []

    def available(self):
        return {"read_native_developers", "native_developer_fixture"}

    def request(self, operation, args):
        self.calls.append((operation, args))
        return {"operation": operation, "args": args}


def args():
    return {"command": "Hire", "token": "11111111-1111-4111-8111-111111111111",
            "native_origin_uuid": "22222222-2222-4222-8222-222222222222", "epoch": 7, "generation": 9, "expected_count": 6}


class DeveloperSDK(unittest.TestCase):
    def test_three_fixed_commands_dispatch_once(self):
        for command in ("Show", "Hire", "Fire"):
            with self.subTest(command=command):
                fake = NoNativeRuntime()
                request = args()
                request["command"] = command
                result = server.dispatch(fake, "game_native_developer_fixture", request)
                self.assertEqual(fake.calls, [("native_developer_fixture", request)])
                self.assertEqual(result["args"], request)

    def test_read_empty_only(self):
        fake = NoNativeRuntime()
        server.dispatch(fake, "game_read_native_developers", {})
        self.assertEqual(fake.calls, [("read_native_developers", {})])
        with self.assertRaises(runtime.RuntimeErrorDetail):
            server.dispatch(fake, "game_read_native_developers", {"count": 0})
        self.assertEqual(len(fake.calls), 1)

    def test_invalid_request_never_reaches_native(self):
        mutations = [
            ("command", "HireOther"), ("command", []), ("token", "bad"), ("token", args()["token"].upper()),
            ("native_origin_uuid", "bad"), ("epoch", True), ("epoch", 0), ("epoch", -1), ("epoch", 2**64),
            ("generation", 1.0), ("expected_count", True), ("expected_count", -1), ("expected_count", 51),
        ]
        # The numeric-only UUID above has no uppercase letters; exercise a real uppercase hex value.
        mutations[3] = ("token", "AAAAAAAA-1111-4111-8111-111111111111")
        for key, value in mutations:
            with self.subTest(field=key, value=value):
                fake = NoNativeRuntime()
                request = args()
                request[key] = value
                with self.assertRaises(runtime.RuntimeErrorDetail):
                    server.dispatch(fake, "game_native_developer_fixture", request)
                self.assertEqual(fake.calls, [])
        for request in ({}, {**args(), "extra": 1}, {k: v for k, v in args().items() if k != "token"}):
            fake = NoNativeRuntime()
            with self.assertRaises(runtime.RuntimeErrorDetail):
                server.dispatch(fake, "game_native_developer_fixture", request)
            self.assertEqual(fake.calls, [])

    def test_origin_uint64_limit_and_zero_pool(self):
        request = args()
        request.update(epoch=2**64-1, generation=2**64-1, expected_count=0, command="Show")
        fake = NoNativeRuntime()
        server.dispatch(fake, "game_native_developer_fixture", request)
        self.assertEqual(len(fake.calls), 1)

    def test_real_descriptors_distinguish_reader_and_mutation(self):
        tools = {tool.name: tool for tool in server.tools_for(NoNativeRuntime())}
        read = tools["game_read_native_developers"]
        mutate = tools["game_native_developer_fixture"]
        self.assertTrue(read.annotations.readOnlyHint)
        self.assertFalse(read.annotations.destructiveHint)
        self.assertFalse(mutate.annotations.readOnlyHint)
        self.assertTrue(mutate.annotations.destructiveHint)
        self.assertFalse(mutate.annotations.idempotentHint)
        self.assertEqual(mutate.inputSchema["properties"]["command"]["enum"], ["Show", "Hire", "Fire"])

    def test_original_native_unknown_cause_survives_dispatch(self):
        class Failed(NoNativeRuntime):
            def request(self, operation, arguments):
                raise runtime.RuntimeErrorDetail("native_error", "original native UICommand SEH=123")
        with self.assertRaisesRegex(runtime.RuntimeErrorDetail, "original native UICommand SEH=123"):
            server.dispatch(Failed(), "game_native_developer_fixture", copy.deepcopy(args()))


if __name__ == "__main__":
    unittest.main(verbosity=2)
