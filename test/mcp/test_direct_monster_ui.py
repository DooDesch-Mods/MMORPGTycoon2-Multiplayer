"""Execute the actual changed MCP validation branch without loading a game MCP."""
from pathlib import Path
import ast,math,re
import sys
SERVER_SOURCE=Path(sys.argv[1]) if __name__=='__main__' and len(sys.argv)==2 else Path(__file__).resolve().parents[2]/'tools/mcp/server.py'
tree=ast.parse(SERVER_SOURCE.read_text(encoding='utf-8'))
branch=next(node for node in ast.walk(tree) if isinstance(node,ast.If) and ast.unparse(node.test)=='op == \'native_npc_place_ui\'')
class RuntimeErrorDetail(Exception):pass
env={'math':math,'re':re,'RuntimeErrorDetail':RuntimeErrorDetail,'TIME_TOKEN_PATTERN':r'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}'}
function=ast.parse('def validate(arguments):\n op="native_npc_place_ui"\n return arguments').body[0]
function.body.insert(1,branch)
exec(compile(ast.fix_missing_locations(ast.Module(body=[function],type_ignores=[])),'actual_mcp_branch','exec'),env)
validate=env['validate'];count=0
base={'token':'11111111-1111-4111-8111-111111111111','region_id':63,'tool':'item_elitemonster','character_slot':1,'character_type':0,'level':1,'position':[8020,70,8550]}
validate(base);count+=1
for tool in ['Guard','ClassTrainer','QuestGiver']:
 value={**base,'tool':tool};value.pop('level');validate(value);count+=1
def reject(value):
 global count
 try:validate(value)
 except RuntimeErrorDetail:count+=1
 else:raise AssertionError('Invalid MCP arguments accepted: '+repr(value))
for field in base:
 value=dict(base);value.pop(field);reject(value)
for value in [0,-1,True,1.0,'1',2147483648]:reject({**base,'level':value})
for value in [0,True,-1,2]:reject({**base,'character_slot':value})
for value in [True,1.0,-1]:reject({**base,'character_type':value})
reject({**base,'position':[float('nan'),0,0]});reject({**base,'position':[1000001,0,0]})
reject({**base,'extra':1});reject({**base,'tool':'Guard'});reject({**base,'token':'reused-invalid'})
call=next(value for node in ast.walk(tree) if isinstance(node,ast.Dict) for key,value in zip(node.keys,node.values)
          if isinstance(key,ast.Constant) and key.value=='native_npc_place_ui' and isinstance(value,ast.Call))
schema_env={'object_schema':lambda properties,required=():{'properties':properties,'required':required},'TIME_TOKEN':{},'SUBSCRIBER_ID':{}}
schema=eval(compile(ast.Expression(call),'actual_mcp_schema','eval'),schema_env)
assert set(schema['properties']['tool']['enum'])=={'Guard','ClassTrainer','QuestGiver','item_elitemonster'}
assert schema['properties']['level']['minimum']==1 and 'level' not in schema['required']
print(f'{count} actual MCP argument validation cases passed; schema includes Elite conditional level')
