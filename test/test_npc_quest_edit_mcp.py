"""Run the staged normal Quest editor MCP validator without native calls."""
import ast
from pathlib import Path
import re
import sys
HERE=Path(__file__).resolve().parent
SERVER=Path(sys.argv[1]) if len(sys.argv)>1 else HERE/'candidate/tools/mcp/server.py'
tree=ast.parse(SERVER.read_text(encoding='utf-8'))
functions=[node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name in ('validate_npc_quest_list','validate_npc_quest_edit')]
assert len(functions)==2
class RuntimeErrorDetail(RuntimeError):
    def __init__(self,code,message):super().__init__(message);self.code=code
scope={'re':re,'RuntimeErrorDetail':RuntimeErrorDetail,'TIME_TOKEN_PATTERN':r'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}'}
exec(compile(ast.Module(body=functions,type_ignores=[]),str(SERVER),'exec'),scope)
validate=scope['validate_npc_quest_edit'];checks=0
base={'token':'11111111-1111-4111-8111-111111111111','owner_key':'action:22222222-2222-4222-8222-222222222222','region_id':67,'remove_slot':1}
values=[{'kind':1,'title':'New title','kills':0,'xp':0,'gold':0,'defaults':False},
        {'kind':2,'title':'','kills':7,'xp':0,'gold':0,'defaults':False},
        {'kind':3,'title':'','kills':0,'xp':400,'gold':7,'defaults':False},
        {'kind':3,'title':'','kills':0,'xp':0,'gold':0,'defaults':True}]
for value in values:validate(dict(base,edit=value));checks+=1
for field,value in [('kind',True),('kind',0),('kind',4),('title','x\0y'),('title','ä'*513),('defaults',1),
                    ('kills',True),('kills',-1),('kills',1000001),('xp',False),('xp',2147483648),('gold',-1),('gold',2.5)]:
    bad=dict(base,edit=dict(values[2],**{field:value}))
    try:validate(bad)
    except RuntimeErrorDetail as error:assert error.code=='invalid_args'
    else:raise AssertionError('invalid editor field accepted: '+field)
    checks+=1
for value in [dict(values[0],title=''),dict(values[0],xp=1),dict(values[1],title='Wrong'),dict(values[1],gold=2),
              dict(values[2],kills=1),dict(values[2],defaults=True),dict(values[3],xp=1),dict(values[3],gold=1)]:
    try:validate(dict(base,edit=value))
    except RuntimeErrorDetail as error:assert error.code=='invalid_args'
    else:raise AssertionError('unrelated command value accepted')
    checks+=1
for change in ['remove_slot','extra','missing']:
    bad=dict(base,edit=values[0])
    if change=='remove_slot':bad['remove_slot']=-1
    elif change=='extra':bad['pointer']=123
    else:bad.pop('edit')
    try:validate(bad)
    except RuntimeErrorDetail as error:assert error.code=='invalid_args'
    else:raise AssertionError('wrong editor shape/slot accepted')
    checks+=1
print(f'PASS {checks} actual Quest editor MCP command/domain checks')
