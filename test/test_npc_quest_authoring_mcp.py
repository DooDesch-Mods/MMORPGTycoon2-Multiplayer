import ast,re,sys
from pathlib import Path
source=Path(sys.argv[1]).read_text(encoding='utf-8');tree=ast.parse(source)
nodes=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in {'saved_monster_key','validate_npc_quest_target'}]
class RuntimeErrorDetail(Exception):pass
token_pattern=next(n for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='TIME_TOKEN_PATTERN' for t in n.targets))
env={'re':re,'RuntimeErrorDetail':RuntimeErrorDetail}
exec(compile(ast.Module(body=[token_pattern],type_ignores=[]),'actual_token_pattern','exec'),env)
exec(compile(ast.Module(body=nodes,type_ignores=[]),'actual_quest_mcp','exec'),env)
check=env['validate_npc_quest_target'];n=0
base={'token':'11111111-1111-4111-8111-111111111111','owner_key':'action:33333333-3333-4333-8333-333333333333','region_id':67,'sequence_id':0,'destination_region':67,'destination_key':'action:44444444-4444-4444-8444-444444444444','family':'monster'}
for family in ['monster','building','zone','npc']:check({**base,'family':family});n+=1
saved_monster='a'*64+':world:monster:inline:region{67}/monsterManager/monster[1]'
check({**base,'destination_key':saved_monster});n+=1
check({**base,'family':'npc','destination_key':'a'*64+':world:npc:36'});n+=1
bad=[{**base,'extra':0},{**base,'family':'subscriber'},{**base,'destination_key':'local:123'},{**base,'token':True},{**base,'owner_key':'bad\0key'}]
bad += [{**base,'destination_key':saved_monster+'x'},{**base,'destination_key':'a'*64+':world:monster:2147483648'}]
bad += [{**base,'destination_key':value} for value in [base['destination_key']+'x','action:'+base['destination_key'],base['destination_key'].upper(),base['destination_key']+'\n']]
bad += [{k:v for k,v in base.items() if k!=field} for field in base]
bad += [{**base,field:value} for field in ['region_id','destination_region','sequence_id'] for value in [True,-1,1.0,'0',2147483648]]
for value in bad:
    try:check(value)
    except RuntimeErrorDetail:n+=1
    else:raise AssertionError(value)
assert '"native_npc_quest_target_ui": object_schema' in source


# Exercise the actual shared operation catalog and server dispatch boundary.
from types import SimpleNamespace
runtime_source=Path(sys.argv[2]).read_text(encoding='utf-8') if len(sys.argv)>2 else Path(sys.argv[1]).with_name('runtime.py').read_text(encoding='utf-8')
runtime_tree=ast.parse(runtime_source)
catalog=[]
for node in runtime_tree.body:
    if isinstance(node,ast.Assign) and len(node.targets)==1 and isinstance(node.targets[0],ast.Name) and node.targets[0].id in ('OPS','MUTATIONS'):catalog.append(node)
    if isinstance(node,ast.Expr) and isinstance(node.value,ast.Call) and isinstance(node.value.func,ast.Attribute) and isinstance(node.value.func.value,ast.Name) and node.value.func.value.id in ('OPS','MUTATIONS'):catalog.append(node)
exec(compile(ast.Module(body=catalog,type_ignores=[]),'actual_runtime_catalog','exec'),env)
op='native_npc_quest_target_ui'
assert op in env['OPS'] and op in env['MUTATIONS'];n+=1
def schema_node(name):
    values=next(node.value for node in tree.body if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id==name for t in node.targets))
    return next(value for key,value in zip(values.keys,values.values) if isinstance(key,ast.Constant) and key.value==op)
env.update(Runtime=object,BASE={},types=SimpleNamespace(Tool=lambda **kw:SimpleNamespace(**kw),ToolAnnotations=lambda **kw:SimpleNamespace(**kw)),
           TIME_TOKEN={'type':'string'},object_schema=lambda fields,required:{'type':'object','properties':fields,'required':list(required)})
env['SUBSCRIBER_ID']=ast.literal_eval(next(node.value for node in tree.body if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='SUBSCRIBER_ID' for t in node.targets)))
env['OP_SCHEMAS']={op:eval(compile(ast.Expression(schema_node('OP_SCHEMAS')),'actual_quest_schema','eval'),env)}
env['OP_DESCRIPTIONS']={op:ast.literal_eval(schema_node('OP_DESCRIPTIONS'))}
functions=[node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name in ('tools_for','dispatch')]
for node in ast.walk(next(node for node in functions if node.name=='dispatch')):
    if isinstance(node,ast.Attribute) and isinstance(node.value,ast.Name) and node.attr=='SCHEMAS':env[node.value.id]=SimpleNamespace(SCHEMAS={})
exec(compile(ast.Module(body=functions,type_ignores=[]),'actual_server_dispatch','exec'),env)
class RuntimePort:
    def __init__(self):self.calls=[]
    def available(self):return {op}&env['OPS']
    def request(self,name,arguments):self.calls.append((name,arguments));return {'captured':True}
port=RuntimePort();listed=env['tools_for'](port)
assert len(listed)==1 and listed[0].name=='game_'+op and listed[0].annotations.destructiveHint is True and listed[0].annotations.readOnlyHint is False;n+=1
assert env['dispatch'](port,'game_'+op,base)=={'captured':True} and port.calls==[(op,base)];n+=1
try:env['dispatch'](port,'game_'+op,{**base,'sequence_id':True})
except RuntimeErrorDetail:pass
else:raise AssertionError('actual dispatch skipped Quest validator')
assert len(port.calls)==1;n+=1
env['OPS'].remove(op)
try:env['dispatch'](port,'game_'+op,base)
except RuntimeErrorDetail:pass
else:raise AssertionError('unregistered operation bypassed shared runtime catalog')
assert len(port.calls)==1;n+=1
print(n,'actual Quest MCP validator/catalog/dispatch cases passed')
