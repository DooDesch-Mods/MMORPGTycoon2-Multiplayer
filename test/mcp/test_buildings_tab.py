from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mcp'))
import native_tool_prepare_ops as ops
import runtime
from runtime import RuntimeErrorDetail
import server

class BuildingsTabTests(unittest.TestCase):
    def test_fixed_schemas_and_dispatch_registration(self):
        for name in ('native_buildings_tab', 'read_buildings_tab'):
            self.assertIn(name, runtime.OPS)
            self.assertEqual(server.OP_SCHEMAS[name], ops.SCHEMAS[name])
        self.assertIn('native_buildings_tab', runtime.MUTATIONS)
        self.assertNotIn('read_buildings_tab', runtime.MUTATIONS)

    def test_prepare_requires_exact_origin_and_no_arbitrary_tab(self):
        token='11111111-2222-4333-8444-555555555555'
        args=dict(acceptance_token=token,snapshot_uuid=token,expected_world_epoch=1,expected_generation=2)
        ops.validate('native_buildings_tab',args)
        for invalid in ({},dict(args,tab='NPCs'),dict(args,expected_generation=True),
                        dict(args,expected_world_epoch=0),dict(args,snapshot_uuid='foreign')):
            with self.subTest(invalid=invalid), self.assertRaises(RuntimeErrorDetail) as caught:
                ops.validate('native_buildings_tab',invalid)
            self.assertEqual(caught.exception.code,'invalid_args')

    def test_read_requires_only_retained_token(self):
        token='11111111-2222-4333-8444-555555555555'
        ops.validate('read_buildings_tab',dict(acceptance_token=token))
        for args in ({},dict(acceptance_token=token,tab='Buildings'),dict(acceptance_token='invalid')):
            with self.assertRaises(RuntimeErrorDetail):ops.validate('read_buildings_tab',args)

if __name__=='__main__':unittest.main()
