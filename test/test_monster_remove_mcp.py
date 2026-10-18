"""Exercise the real existing MCP schema and validator without a game MCP."""
import ast
import math
from pathlib import Path
import re
import sys

source=Path(sys.argv[1]).read_text(encoding='utf-8')
tree=ast.parse(source)
names={'validate_npc_target','npc_target_schema'}
functions=[node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name in names]
class RuntimeErrorDetail(Exception):pass
env={'RuntimeErrorDetail':RuntimeErrorDetail,'re':re,'math':math,
     'TIME_TOKEN_PATTERN':r'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}',
     'TIME_TOKEN':{},'object_schema':lambda properties,required:{'properties':properties,'required':required}}
exec(compile(ast.Module(body=functions,type_ignores=[]),'actual_remove_mcp','exec'),env)
validate=env['validate_npc_target'];base={'token':'11111111-1111-4111-8111-111111111111','target_action_uuid':'33333333-3333-4333-8333-333333333333','family':'monster'}
n=0
validate(base,monster=True);n+=1
for value in [{k:v for k,v in base.items() if k!='family'}, {'token':base['token'],'record_key':'saved:npc:63:0'}]:validate(value,monster=True);n+=1
def reject(value,**options):
    global n
    try:validate(value,**options)
    except RuntimeErrorDetail:n+=1
    else:raise AssertionError('Invalid target accepted: '+repr(value))
reject(base)
for key in base:
    if key!='family':reject({k:v for k,v in base.items() if k!=key},monster=True)
reject({**base,'record_key':'saved:npc:63:0'},monster=True)
reject({**base,'family':'npc'},monster=True)
reject({**base,'target_action_uuid':'bad'},monster=True)
reject({**base,'token':True},monster=True)
reject({**base,'position':[0,0,0]},monster=True)
reject(base,move=True)
schema=env['npc_target_schema'](monster=True)
assert schema['properties']['family']=={'type':'string','const':'monster'}
assert 'family' not in env['npc_target_schema'](move=True)['properties']
print(f'{n} actual Monster Remove MCP schema/validation cases passed')
