"""Read or prepare one original InitialRelease Host fixture before shared gameplay."""
import re
from runtime import RuntimeErrorDetail
UUID=r'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}'
I32={'type':'integer','minimum':-2147483648,'maximum':2147483647}
ARRAY={'type':'array','items':I32,'minItems':3,'maxItems':3}
FIELDS=('release_type','changes','changelogs','last_changelog_type','server_current','server_target','map_state')
STATE={'type':'object','properties':{**{k:I32 for k in FIELDS},**{k:ARRAY for k in ('version','planned','last_changelog')}},'required':list(FIELDS)+['version','planned','last_changelog'],'additionalProperties':False}
SCHEMAS={'read_native_release':{'type':'object','properties':{},'additionalProperties':False},
         'native_initial_release_fixture':{'type':'object','properties':{'token':{'type':'string','pattern':'^'+UUID+'$'},'native_origin_uuid':{'type':'string','pattern':'^'+UUID+'$'},'expected':STATE},'required':['token','native_origin_uuid','expected'],'additionalProperties':False}}
DESCRIPTIONS={'read_native_release':'Read actual current ReleaseHUD/native Origin/version/changelog/Clock0 fixture state and retained receipts on owned GameUpdate. Read does not call RNG-consuming CanReleaseNow.',
 'native_initial_release_fixture':'Invoke original ReleaseHUD.DoCelebration once on actual idle Host/Clock0 before Controller initialization. Genuine native UI-command/engine fixture route, not a mouse-click or peer ACK proof. Inspect actual effect/settled state and retain token; no replay.'}
def validate(op,args):
    def bad(cause):raise RuntimeErrorDetail('invalid_args',cause)
    if not isinstance(args,dict):bad('InitialRelease requires exact argument object')
    if op=='read_native_release':
        if args:bad('InitialRelease read requires empty arguments')
        return
    if op!='native_initial_release_fixture' or set(args)!={'token','native_origin_uuid','expected'}:bad('InitialRelease fixture fields differ')
    for k in ('token','native_origin_uuid'):
        if not isinstance(args[k],str) or re.fullmatch(UUID,args[k]) is None:bad('InitialRelease requires canonical token/Origin UUIDs')
    state=args['expected']
    if not isinstance(state,dict) or set(state)!=set(FIELDS)|{'version','planned','last_changelog'}:bad('InitialRelease expected state fields differ')
    for k in FIELDS:
        if type(state[k]) is not int or not -2147483648<=state[k]<=2147483647:bad('InitialRelease expected state requires exact int32')
    for k in ('version','planned','last_changelog'):
        if not isinstance(state[k],list) or len(state[k])!=3:bad('InitialRelease native version requires three int32 fields')
        for n in state[k]:
            if type(n) is not int or not -2147483648<=n<=2147483647:bad('InitialRelease native version requires exact int32')
