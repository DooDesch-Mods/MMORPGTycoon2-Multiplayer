import ast
from pathlib import Path
import re
import sys

server = Path(sys.argv[1])
tree = ast.parse(server.read_text(encoding='utf-8'))
function = [node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name in {'validate_character_rename_ui', 'validate_character_rename_read'}]
assert len(function) == 2

class RuntimeErrorDetail(RuntimeError):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code

scope = {'re': re, 'RuntimeErrorDetail': RuntimeErrorDetail,
         'TIME_TOKEN_PATTERN': r'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}'}
exec(compile(ast.Module(body=function, type_ignores=[]), str(server), 'exec'), scope)
validate = scope['validate_character_rename_ui']
base = {'token': '11111111-1111-4111-8111-111111111111', 'kind': 0, 'id': 1, 'name': 'New name', 'read_only': False}
checks = 0
for value in [base, dict(base, open=True), dict(base, open=False), dict(base, name=''), dict(base, name='ä' * 20), dict(base, kind=-1, id=-1, name='', read_only=True)]:
    validate(value)
    checks += 1
for key, value in [('token', 'used'), ('kind', True), ('kind', -2), ('kind', 5), ('id', True), ('id', -2),
                   ('id', 1000000), ('name', 'x' * 21), ('name', 'x\0y'), ('name', None), ('read_only', 1),
                   ('kind', -1), ('id', -1), ('read_only', True), ('open', 1)]:
    try:
        validate(dict(base, **{key: value}))
    except RuntimeErrorDetail as error:
        assert error.code == 'invalid_args'
    else:
        raise AssertionError('Invalid Character Rename UI field accepted: ' + key)
    checks += 1
for value in [dict(base, pointer=1), {key: value for key, value in base.items() if key != 'name'}, []]:
    try:
        validate(value)
    except RuntimeErrorDetail as error:
        assert error.code == 'invalid_args'
    else:
        raise AssertionError('Wrong Character Rename UI shape accepted')
    checks += 1
read = scope['validate_character_rename_read']
for value in [{}, {'action_uuid': base['token']}]:
    read(value)
    checks += 1
for value in [[], {'action_uuid': 'bad'}, {'action_uuid': 1}, {'action_uuid': base['token'], 'kind': 0}]:
    try:
        read(value)
    except RuntimeErrorDetail as error:
        assert error.code == 'invalid_args'
    else:
        raise AssertionError('Invalid cached Character read accepted')
    checks += 1
print(f'{checks} actual Character Rename MCP validator checks passed')
