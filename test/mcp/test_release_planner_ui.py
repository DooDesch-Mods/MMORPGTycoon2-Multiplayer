"""Exercise fixed planner SDK dispatch without invoking the game."""
from pathlib import Path
import importlib.util
import sys
import unittest

HERE = Path(__file__).resolve().parent
ROOT = next(p for p in [HERE, *HERE.parents] if (p / 'tools/mcp/runtime.py').is_file())
sys.path.insert(0, str(ROOT / 'tools/mcp'))

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module

runtime = load('runtime', ROOT / 'tools/mcp/runtime.py')
server = load('release_planner_test_server', ROOT / 'tools/mcp/server.py')

class Fake:
    def __init__(self): self.calls = []
    def available(self): return {'native_release_planner_ui', 'read_release_planner_ui'}
    def request(self, op, args):
        self.calls.append((op, args))
        return {'read_only': op.startswith('read_'), 'execution_ack': False}

def args():
    return {'command': 'SetMinor', 'token': '11111111-1111-4111-8111-111111111111',
            'snapshot_uuid': '22222222-2222-4222-8222-222222222222', 'epoch': 7,
            'generation': 9, 'expected_type': 1, 'expected_funds': 0}

class PlannerUI(unittest.TestCase):
    def test_fixed_commands_and_read_dispatch_once(self):
        fake = Fake()
        self.assertTrue(server.dispatch(fake, 'game_read_release_planner_ui', {})['read_only'])
        for command in ('Show', 'SetMinor', 'SetHotfix', 'AllocatePayment'):
            value = {**args(), 'command': command}
            result = server.dispatch(fake, 'game_native_release_planner_ui', value)
            self.assertFalse(result['execution_ack'])
            self.assertEqual(fake.calls[-1], ('native_release_planner_ui', value))
        self.assertEqual(len(fake.calls), 5)

    def test_invalid_arguments_never_dispatch(self):
        variants = [{}, {**args(), 'extra': 1}]
        for key, value in [('command', 'Read'), ('command', []), ('token', 'bad'), ('snapshot_uuid', 'A' * 36),
                           ('epoch', True), ('epoch', 0), ('generation', 2**64),
                           ('expected_type', True), ('expected_type', 5), ('expected_funds', -1)]:
            variants.append({**args(), key: value})
        for value in variants:
            fake = Fake()
            with self.assertRaises(runtime.RuntimeErrorDetail):
                server.dispatch(fake, 'game_native_release_planner_ui', value)
            self.assertEqual(fake.calls, [])
        fake = Fake()
        with self.assertRaises(runtime.RuntimeErrorDetail):
            server.dispatch(fake, 'game_read_release_planner_ui', {'extra': 1})
        self.assertEqual(fake.calls, [])

    def test_descriptor_and_failure_cause(self):
        tools = {tool.name: tool for tool in server.tools_for(Fake())}
        self.assertTrue(tools['game_read_release_planner_ui'].annotations.readOnlyHint)
        self.assertTrue(tools['game_native_release_planner_ui'].annotations.destructiveHint)
        self.assertIn('native_release_planner_ui', runtime.MUTATIONS)
        class Failed(Fake):
            def request(self, op, value): raise runtime.RuntimeErrorDetail('native_error', 'original callback Unknown')
        with self.assertRaisesRegex(runtime.RuntimeErrorDetail, 'original callback Unknown'):
            server.dispatch(Failed(), 'game_native_release_planner_ui', args())

if __name__ == '__main__':
    unittest.main(verbosity=2)
