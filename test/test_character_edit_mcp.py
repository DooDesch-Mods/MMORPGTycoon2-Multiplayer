import ast,re,sys
from pathlib import Path
server=Path(sys.argv[1]);tree=ast.parse(server.read_text(encoding='utf-8'))
functions=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in {'validate_character_edit_ui','validate_character_edit_read'}]
assert len(functions)==2
class RuntimeErrorDetail(RuntimeError):
    def __init__(self,code,message):super().__init__(message);self.code=code
scope={'RuntimeErrorDetail':RuntimeErrorDetail,'re':re,'TIME_TOKEN_PATTERN':r'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}'}
exec(compile(ast.Module(body=functions,type_ignores=[]),str(server),'exec'),scope)
base={'token':'11111111-1111-4111-8111-111111111111','kind':0,'id':1}
valid=[dict(base,operation='ShowCostume'),dict(base,operation='CostumeState'),dict(base,operation='ToggleWeapon',category='Staff',expected_enabled=False)]
for operation in ['ShowAbility','AbilityState','ApplyAbility']:valid.append(dict(base,operation=operation,slot=1))
valid += [dict(base,operation='ShowAbilityPage',slot=1,page='{ability_edit_tab_flavor}'),
    dict(base,operation='EditAbilityText',slot=1,control='flavor_description',value='Actual Ä description\nline two'),
    dict(base,operation='EditAbilityChoice',slot=1,control='ability_cooldown',value='1second'),
    dict(base,operation='ToggleAbilityControl',slot=1,control='flavor_custom',expected_enabled=False),
    dict(base,operation='ActivateAbilityControl',slot=1,control='original_button')]
checks=0
valid.extend([dict(valid[7],commit=False),dict(valid[7],commit=True)])
valid.append(dict(base,operation='ShowAbility'))
for value in valid:scope['validate_character_edit_ui'](value);checks+=1
bad=[]
for value in valid:
    bad += [dict(value,pointer=1),{k:v for k,v in value.items() if k!='token'},dict(value,token='used'),dict(value,kind=True),dict(value,id=1000000)]
for key,value in [('operation',[]),('operation','HiddenBuff'),('slot',True),('slot',-1),('slot',10000),('control',''),('value','x\0y'),('value','ä'*2049),('value','\ud800')]:
    bad.append(dict(valid[7],**{key:value}))
bad += [dict(valid[2],expected_enabled=1),dict(valid[2],category='Staff\nHidden'),dict(valid[2],category='ä'*65),
    dict(valid[7],commit=1),dict(valid[8],commit=False),[],{}]
for value in bad:
    try:scope['validate_character_edit_ui'](value)
    except RuntimeErrorDetail as error:assert error.code=='invalid_args'
    else:raise AssertionError('Malformed native Character input admitted: '+repr(value))
    checks+=1
scope['validate_character_edit_read']({'token':base['token']});checks+=1
for value in [{},{'token':'used'},{'token':base['token'],'kind':0},[]]:
    try:scope['validate_character_edit_read'](value)
    except RuntimeErrorDetail as error:assert error.code=='invalid_args'
    else:raise AssertionError('Invalid retained read admitted')
    checks+=1
readonly=[n for n in ast.walk(tree) if isinstance(n,ast.keyword) and n.arg=='readOnlyHint']
assert sum(any(isinstance(n,ast.Constant) and n.value=='read_character_edit_ui' for n in ast.walk(node)) for node in readonly)==1
print(f'{checks} actual offered Character MCP validator assertions passed')
