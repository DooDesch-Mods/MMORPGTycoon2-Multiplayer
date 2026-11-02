"""Exercise the actual NPC property MCP validator without a runtime."""
import ast
from pathlib import Path
import re

server = Path(__file__).resolve().parents[1] / 'tools/mcp/server.py'
tree = ast.parse(server.read_text(encoding='utf-8'))
function = next(node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name == 'validate_npc_property')
class RuntimeErrorDetail(RuntimeError):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code
scope = {'re': re, 'RuntimeErrorDetail': RuntimeErrorDetail,
         'TIME_TOKEN_PATTERN': r'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}'}
exec(compile(ast.Module(body=[function], type_ignores=[]), str(server), 'exec'), scope)
validate = scope['validate_npc_property']
valid = dict(token='11111111-1111-4111-8111-111111111111',
             key='action:22222222-2222-4222-8222-222222222222', kind=1, name='New NPC', online=0, read_only=False)
checks = 0
for kind in (1, 2, 3, 4):
    validate(dict(valid, kind=kind, name=valid['name'] if kind == 1 else ''))
    checks += 1
for field, value in [('token', False), ('token', 'invalid'), ('key', ''), ('key', 'bad\0key'), ('key', False),
                     ('kind', True), ('kind', 0), ('kind', 5), ('kind', 1.0), ('name', ''), ('name', 'x' * 257),
                     ('name', 'bad\0name'), ('name', False), ('online', True), ('online', 3), ('online', -1),
                     ('online', 1), ('read_only', 0)]:
    try:
        validate(dict(valid, **{field: value}))
    except RuntimeErrorDetail as error:
        assert error.code == 'invalid_args'
    else:
        raise AssertionError('Invalid property input accepted: ' + field)
    checks += 1
for bad in (dict(valid, pointer=123), {key: value for key, value in valid.items() if key != 'read_only'},
            dict(valid, kind=2), dict(valid, kind=3, name='', online=1)):
    try:
        validate(bad)
    except RuntimeErrorDetail as error:
        assert error.code == 'invalid_args'
    else:
        raise AssertionError('Unrelated property fields accepted')
    checks += 1
assert 'native_npc_property_ui' in server.with_name('runtime.py').read_text(encoding='utf-8')
print(f'PASS {checks} actual NPC property MCP cases')
