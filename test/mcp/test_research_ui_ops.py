import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('research_ui_ops', Path(__file__).resolve().parents[2] / 'tools/mcp/research_ui_ops.py')
ops = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ops)

class ResearchUiGuards(unittest.TestCase):
    def arguments(self, key='parties', command='Start'):
        return {'token': '10000000-0000-4000-8000-000000000001', 'native_origin_uuid': '20000000-0000-4000-8000-000000000002',
                'operation': command, 'tech_key': key, 'world_epoch': 1, 'world_generation': 2, 'expected_development': {'actual': True}}

    def test_catalogue_keys_and_original_controls_are_generic(self):
        for key in ('parties', 'dungeons', 'future_real_catalogue_key'):
            for command in ops.COMMANDS:
                ops.validate(self.arguments(key, command))

    def test_no_state_setter_or_unguarded_call(self):
        for command in ('Complete', 'SetCash', 'Unlock', 'DeployForFree'):
            with self.assertRaises(ValueError):
                ops.validate(self.arguments(command=command))
        for field, value in (('token', 'stale'), ('native_origin_uuid', ''), ('world_epoch', True), ('world_generation', 0),
                             ('tech_key', ''), ('tech_key', 'x\0y'), ('tech_key', 'ü' * 101), ('expected_development', {})):
            args = self.arguments()
            args[field] = value
            with self.assertRaises(ValueError):
                ops.validate(args)
        args = self.arguments()
        args['cash'] = 90000
        with self.assertRaises(ValueError):
            ops.validate(args)

if __name__ == '__main__':
    unittest.main()
