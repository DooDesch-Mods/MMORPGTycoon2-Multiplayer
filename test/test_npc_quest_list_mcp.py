"""Execute the actual staged MCP validator without starting a server or game."""
import ast
from pathlib import Path
import re
import sys

HERE=Path(__file__).resolve().parent
SERVER=Path(sys.argv[1]) if len(sys.argv)>1 else HERE/'candidate/tools/mcp/server.py'
source=SERVER.read_text(encoding='utf-8')
tree=ast.parse(source)
function=next(node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name=='validate_npc_quest_list')
class RuntimeErrorDetail(RuntimeError):
    def __init__(self,code,message):super().__init__(message);self.code=code
scope={'re':re,'RuntimeErrorDetail':RuntimeErrorDetail,'TIME_TOKEN_PATTERN':r'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}'}
exec(compile(ast.Module(body=[function],type_ignores=[]),str(SERVER),'exec'),scope)
validate=scope['validate_npc_quest_list']
valid={'token':'11111111-1111-4111-8111-111111111111','owner_key':'action:22222222-2222-4222-8222-222222222222','region_id':67,'remove_slot':-1}
checks=0
for slot in [-1,0,1,2,3,4]:
    validate(dict(valid,remove_slot=slot));checks+=1
for field,value in [('token','not-uuid'),('token',False),('owner_key',''),('owner_key','bad\0key'),('owner_key',False),
                    ('region_id',True),('region_id',-1),('region_id',2147483648),('region_id',67.0),
                    ('remove_slot',True),('remove_slot',-2),('remove_slot',5),('remove_slot',0.0)]:
    try:validate(dict(valid,**{field:value}))
    except RuntimeErrorDetail as error:assert error.code=='invalid_args'
    else:raise AssertionError('invalid actual MCP field accepted: '+field)
    checks+=1
for change in ['missing','extra']:
    bad=valid.copy()
    if change=='missing':bad.pop('owner_key')
    else:bad['pointer']=123
    try:validate(bad)
    except RuntimeErrorDetail as error:assert error.code=='invalid_args'
    else:raise AssertionError('invalid actual MCP shape accepted: '+change)
    checks+=1
for path in [SERVER,SERVER.with_name('runtime.py')]:
    ast.parse(path.read_text(encoding='utf-8'));checks+=1
print(f'PASS {checks} actual Quest list MCP argument and module checks')
detail=next(node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name=='validate_npc_region')
exec(compile(ast.Module(body=[detail],type_ignores=[]),str(SERVER),'exec'),scope)
read=scope['validate_npc_region'];read({'region_id':67});read({'region_id':67,'quest_action_uuid':valid['token']})
for bad in [{'region_id':True},{'region_id':67,'quest_action_uuid':False},{'region_id':67,'quest_action_uuid':'bad'},
            {'region_id':67,'quest_action_uuid':valid['token'],'extra':0},{'quest_action_uuid':valid['token']}]:
    try:read(bad)
    except RuntimeErrorDetail as error:assert error.code=='invalid_args'
    else:raise AssertionError('invalid cached Quest detail accepted')
print('PASS exact cached Quest operation detail MCP arguments')
root=SERVER.parents[2]
native=(root/'src/game/npc_quest_list_runtime.inc').read_text(encoding='utf-8')
detail_body=native[native.index('bool read_operation('):native.index('json status(){')]
assert all(call not in detail_body for call in ('state_read(', 'd::sample(', 'Memory ', 'network_binding(', 'query(')), 'Cached Quest detail must not scan native state'
assert 'canonical_diagnostic(op.before)' in detail_body and 'canonical_diagnostic(op.after)' in detail_body
compact=native[native.index('json status(){'):native.index('bool native_ui(')]
assert 'canonical_diagnostic(' not in compact, 'Ordinary status must stay compact'
core=(root/'src/core/dll_main.cpp').read_text(encoding='utf-8')
route=core[core.index('else if(operation=="read_npc_region")'):core.index('else if(operation=="native_npc_tool_ui")')]
assert 'detail?game::npc_probe::monster_action_runtime::quest_list::read_operation(' in route, 'Explicit cached Quest route absent'
print('PASS cached operation-only projection and compact ordinary status boundaries')
