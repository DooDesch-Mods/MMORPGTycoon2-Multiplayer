"""Exercise the actual Config UI MCP validator and narrow native routing contract."""
import ast
from pathlib import Path
import re
import sys

root=Path(__file__).resolve().parents[1]
server=root/'tools/mcp/server.py'
tree=ast.parse(server.read_text(encoding='utf-8'))
function=next(node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name=='validate_dungeon_config_ui')
sys.path.insert(0,str(server.parent))
from building_keys import KEY_PATTERN
class RuntimeErrorDetail(RuntimeError):
    def __init__(self,code,message):super().__init__(message);self.code=code
scope={'re':re,'RuntimeErrorDetail':RuntimeErrorDetail,'SHARED_BUILDING_KEY_PATTERN':'^'+KEY_PATTERN+'$',
       'TIME_TOKEN_PATTERN':r'^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'}
exec(compile(ast.Module(body=[function],type_ignores=[]),str(server),'exec'),scope)
validate=scope['validate_dungeon_config_ui']
value=dict(token='11111111-1111-4111-8111-111111111111',key='action:22222222-2222-4222-8222-222222222222',desired=4,read_only=False)
checks=0
for count in range(1,11):
    for read_only in [False,True]:validate(dict(value,desired=count,read_only=read_only));checks+=1
validate({'token':value['token']},read=True);checks+=1
for field,bad in [('token',False),('token','invalid'),('key',''),('key','bad\0key'),('key',False),('desired',True),('desired',0),('desired',11),('desired',1.0),('read_only',0)]:
    try:validate(dict(value,**{field:bad}))
    except RuntimeErrorDetail as error:assert error.code=='invalid_args'
    else:raise AssertionError('Invalid Config UI field accepted: '+field)
    checks+=1
for bad in [dict(value,pointer=123),{key:val for key,val in value.items() if key!='read_only'},None]:
    try:validate(bad)
    except RuntimeErrorDetail as error:assert error.code=='invalid_args'
    else:raise AssertionError('Foreign Config UI shape accepted')
    checks+=1
try:validate(value,read=True)
except RuntimeErrorDetail as error:assert error.code=='invalid_args'
else:raise AssertionError('Mutation-shaped read accepted')
runtime=server.with_name('runtime.py').read_text(encoding='utf-8')
assert 'OPS.update({"native_dungeon_config_ui", "read_dungeon_config_ui"})' in runtime
assert 'MUTATIONS.add("native_dungeon_config_ui")' in runtime
assert 'MUTATIONS.add("read_dungeon_config_ui")' not in runtime
native=(root/'src/game/dungeon_config_ui.inc').read_text(encoding='utf-8')
assert 'SetupFor' not in native and '*reinterpret_cast' not in native
for required in ['0x66d990','OpenInstancesConfig','SetPage','tab_enabled','shown.page!=tab.page','waiting_for_tab','GetTickCount64()+5000','ui_preparation->context!=context','dungeon_config_ui::prepare','SpinnerIncrement','SpinnerDecrement','api.destroy_label="Accept"','steps>9','read_live_window','dungeon_config_ui::offer','record_cleanup_confirmed']:
    assert required in native,required
tools_for=next(node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name=='tools_for')
read_only_sets=[node.value.comparators[0] for node in ast.walk(tools_for)
                if isinstance(node,ast.keyword) and node.arg=='readOnlyHint' and isinstance(node.value,ast.Compare)]
assert any(isinstance(node,ast.Set) and any(isinstance(item,ast.Constant) and item.value=='read_dungeon_config_ui' for item in node.elts)
           for node in read_only_sets)
print(f'PASS {checks+1} actual Config MCP input/read-only/original-offer route checks')
