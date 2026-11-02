"""All registered operations must expose exact native argument contracts."""
from pathlib import Path
import sys
import unittest

from jsonschema import Draft202012Validator, ValidationError

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mcp'))
import server


class Available:
    def __init__(self, operations):
        self.operations = operations

    def available(self):
        return self.operations


class OperationMetadataTests(unittest.TestCase):
    def test_all_registered_operations_have_metadata(self):
        tools = {tool.name: tool for tool in server.tools_for(Available(server.OPS))}
        for op in server.OPS:
            self.assertTrue(tools['game_' + op].description, op)
            Draft202012Validator.check_schema(tools['game_' + op].inputSchema)

    def test_native_terrain_tab_contract(self):
        values = {'acceptance_token': '12345678-1234-1234-1234-123456789abc',
                  'snapshot_uuid': '23456789-1234-1234-1234-123456789abc',
                  'expected_world_epoch': 1, 'expected_generation': 2}
        validator = Draft202012Validator(server.OP_SCHEMAS['native_terrain_tab'])
        validator.validate(values)
        invalid = [dict(values, extra=True), dict(values, expected_world_epoch=0),
                   dict(values, expected_generation=2 ** 64), dict(values, acceptance_token='invalid')]
        for key in values:
            invalid.append({name: value for name, value in values.items() if name != key})
        for value in invalid:
            with self.assertRaises(ValidationError):
                validator.validate(value)
        reader = Draft202012Validator(server.OP_SCHEMAS['read_terrain_tab'])
        reader.validate({'acceptance_token': values['acceptance_token']})
        with self.assertRaises(ValidationError):
            reader.validate(values)

    def test_empty_native_contracts_and_annotations(self):
        tools = {tool.name: tool for tool in server.tools_for(Available(server.OPS))}
        for op in ('native_periodic_countdown', 'read_toon_motion_trace', 'read_ordered_world'):
            self.assertIn(op, server.OPS)
            validator = Draft202012Validator(server.OP_SCHEMAS[op])
            validator.validate({})
            with self.assertRaises(ValidationError):
                validator.validate({'extra': 1})
        for op in ('read_terrain_tab', 'read_toon_motion_trace', 'read_ordered_world'):
            self.assertTrue(tools['game_' + op].annotations.readOnlyHint)
        for op in ('native_terrain_tab', 'native_periodic_countdown'):
            self.assertTrue(tools['game_' + op].annotations.destructiveHint)

    def test_unknown_operation_is_not_silently_advertised(self):
        with self.assertRaises(KeyError):
            server.tools_for(Available({'unregistered_operation'}))


if __name__ == '__main__':
    unittest.main()
