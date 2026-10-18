"""Execute the real MCP target schema and validator without loading a game."""
import ast,math,re,sys
from pathlib import Path
tree=ast.parse(Path(sys.argv[1]).read_text(encoding='utf-8'))
nodes=[node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name in {'npc_target_schema','validate_npc_target'}]
class RuntimeErrorDetail(Exception):pass
env={'RuntimeErrorDetail':RuntimeErrorDetail,'math':math,'re':re,'TIME_TOKEN_PATTERN':r'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}',
     'TIME_TOKEN':{},'object_schema':lambda properties,required:{'properties':properties,'required':required}}
exec(compile(ast.Module(body=nodes,type_ignores=[]),'actual_move_mcp','exec'),env)
base={'token':'11111111-1111-4111-8111-111111111111','target_action_uuid':'33333333-3333-4333-8333-333333333333','family':'monster','position':[8021,70,8550]}
validate=env['validate_npc_target'];validate(base,move=True,monster=True);n=1
normal=dict(base);normal.pop('family');validate(normal,move=True,monster=True);n+=1
saved={'token':base['token'],'record_key':'saved:npc:63:0','position':[1,2,3]};validate(saved,move=True,monster=True);n+=1
def reject(value,**opts):
 global n
 try:validate(value,**opts)
 except RuntimeErrorDetail:n+=1
 else:raise AssertionError('invalid Move MCP argument accepted '+repr(value))
reject(base,move=True)
for k in ['token','target_action_uuid','position']:
 value=dict(base);value.pop(k);reject(value,move=True,monster=True)
for point in [[True,0,0],[float('nan'),0,0],[1000001,0,0],[1,2]]:reject({**base,'position':point},move=True,monster=True)
reject({**base,'family':'npc'},move=True,monster=True);reject({**base,'record_key':'saved:npc:63:0'},move=True,monster=True)
reject({**base,'target_action_uuid':'bad'},move=True,monster=True)
schema=env['npc_target_schema'](move=True,monster=True)
assert schema['properties']['family']=={'type':'string','const':'monster'} and 'family' not in schema['required']
assert 'family' not in env['npc_target_schema']()['properties']
print(f'{n} actual Move MCP target schema/validation cases passed')
