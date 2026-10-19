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
