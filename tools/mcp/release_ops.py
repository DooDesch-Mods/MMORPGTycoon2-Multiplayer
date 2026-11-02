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
SCHEMAS['read_release_planner_ui']={'type':'object','properties':{},'additionalProperties':False}
SCHEMAS['read_native_gm_ui']={'type':'object','properties':{},'additionalProperties':False}
SCHEMAS['read_native_loan_ui']={'type':'object','properties':{},'additionalProperties':False}
SCHEMAS['read_native_campaign_ui']={'type':'object','properties':{},'additionalProperties':False}
SCHEMAS['read_native_service_sync_ui']={'type':'object','properties':{'key':{'type':'string','minLength':1,'maxLength':512},'ordinal':{'type':'integer','minimum':0,'maximum':1023}},'required':['key','ordinal'],'additionalProperties':False}
SCHEMAS['native_service_sync_ui']={'type':'object','properties':{'key':{'type':'string','minLength':1,'maxLength':512},'ordinal':{'type':'integer','minimum':0,'maximum':1023},'token':{'type':'string','pattern':'^'+UUID+'$'},'snapshot_uuid':{'type':'string','pattern':'^'+UUID+'$'},'epoch':{'type':'integer','minimum':1,'maximum':18446744073709551615},'generation':{'type':'integer','minimum':1,'maximum':18446744073709551615},'expected_sync':{'type':'boolean'}},'required':['key','ordinal','token','snapshot_uuid','epoch','generation','expected_sync'],'additionalProperties':False}
SCHEMAS['native_service_select_ui']={'type':'object','properties':{k:v for k,v in SCHEMAS['native_service_sync_ui']['properties'].items() if k!='expected_sync'},'required':['key','ordinal','token','snapshot_uuid','epoch','generation'],'additionalProperties':False}
DESCRIPTIONS['read_native_service_sync_ui']='Read a selected Building service and its actual visible sync control without mutation.'
DESCRIPTIONS['native_service_sync_ui']='Invoke one guarded original selected Building service sync button callback. Consumed tokens never replay.'
DESCRIPTIONS['native_service_select_ui']='Select one actual Building and open its original InfoWindow once. Consumed tokens never replay.'
SCHEMAS['native_service_price_ui']={'type':'object','properties':{**SCHEMAS['native_service_select_ui']['properties'],'command':{'type':'string','enum':['Increment','Decrement']}},'required':['key','ordinal','token','snapshot_uuid','epoch','generation','command'],'additionalProperties':False}
DESCRIPTIONS['native_service_price_ui']='Invoke one guarded original visible selected Building price spinner arrow. Consumed tokens never replay.'
SCHEMAS['native_campaign_ui']={'type':'object','properties':{
    'command':{'type':'string','enum':['Show','SelectAdvertise','SelectCampaign','Start']},'token':{'type':'string','pattern':'^'+UUID+'$'},
    'snapshot_uuid':{'type':'string','pattern':'^'+UUID+'$'},'epoch':{'type':'integer','minimum':1,'maximum':18446744073709551615},
    'generation':{'type':'integer','minimum':1,'maximum':18446744073709551615},'expected_type':{'type':'integer','minimum':-1,'maximum':8}},
    'required':['command','token','snapshot_uuid','epoch','generation','expected_type'],'additionalProperties':False}
DESCRIPTIONS['read_native_campaign_ui']='Read the actual Overview advertising selection, full Management, Ledger and RNG without mutation.'
DESCRIPTIONS['native_campaign_ui']='Invoke one guarded original Overview Show, advertising page selection or StartAdCampaign callback. Consumed tokens never replay.'
SCHEMAS['native_loan_ui']={'type':'object','properties':{
    'command':{'type':'string','enum':['Show','SelectLoans','Take','Repay']},'token':{'type':'string','pattern':'^'+UUID+'$'},
    'snapshot_uuid':{'type':'string','pattern':'^'+UUID+'$'},'epoch':{'type':'integer','minimum':1,'maximum':18446744073709551615},
    'generation':{'type':'integer','minimum':1,'maximum':18446744073709551615},'key':{'type':'string','maxLength':256}},
    'required':['command','token','snapshot_uuid','epoch','generation','key'],'additionalProperties':False}
DESCRIPTIONS['read_native_loan_ui']='Read the current bound Loan/Offer state, full Ledger, RNG and actual visible LoanView rows without mutation.'
DESCRIPTIONS['native_loan_ui']='Invoke one guarded original Overview Show, Loans page selection or actual LoanView Take/Repay callback. Consumed tokens never replay.'
SCHEMAS['native_gm_ui']={'type':'object','properties':{
    'command':{'type':'string','enum':['Show','Hire','Fire']},'token':{'type':'string','pattern':'^'+UUID+'$'},
    'snapshot_uuid':{'type':'string','pattern':'^'+UUID+'$'},'epoch':{'type':'integer','minimum':1,'maximum':18446744073709551615},
    'generation':{'type':'integer','minimum':1,'maximum':18446744073709551615},'expected_count':{'type':'integer','minimum':0,'maximum':50}},
    'required':['command','token','snapshot_uuid','epoch','generation','expected_count'],'additionalProperties':False}
DESCRIPTIONS['read_native_gm_ui']='Read the current bound GameMaster Management state, full Ledger, owner identities and RNG without mutation.'
DESCRIPTIONS['native_gm_ui']='Invoke one guarded original GameMaster Show, Hire or Fire UI callback on an owned ordered peer. Capture is admission only; consumed tokens never replay.'
SCHEMAS['native_release_planner_ui']={'type':'object','properties':{
    'command':{'type':'string','enum':['Show','SetMinor','SetHotfix','AllocatePayment']},
    'token':{'type':'string','pattern':'^'+UUID+'$'},'snapshot_uuid':{'type':'string','pattern':'^'+UUID+'$'},
    'epoch':{'type':'integer','minimum':1,'maximum':18446744073709551615},
    'generation':{'type':'integer','minimum':1,'maximum':18446744073709551615},
    'expected_type':{'type':'integer','minimum':0,'maximum':4},
    'expected_funds':{'type':'integer','minimum':0,'maximum':2147483647}},
    'required':['command','token','snapshot_uuid','epoch','generation','expected_type','expected_funds'],'additionalProperties':False}
DESCRIPTIONS['read_release_planner_ui']='Read the current ordered planner Release state, RNG and full Ledger from the actual bound world without changing it.'
DESCRIPTIONS['native_release_planner_ui']='Invoke one verified original planner Show or SetMinor, SetHotfix or AllocatePayment callback on the owned ordered peer. Capture is admission only, not execution or peer ACK. Consumed tokens never replay.'
def validate(op,args):
    def bad(cause):raise RuntimeErrorDetail('invalid_args',cause)
    if not isinstance(args,dict):bad('InitialRelease requires exact argument object')
    if op in ('read_native_service_sync_ui','native_service_sync_ui','native_service_select_ui','native_service_price_ui'):
        fields={'key','ordinal'}
        if op!='read_native_service_sync_ui':fields|={'token','snapshot_uuid','epoch','generation'}
        if op=='native_service_sync_ui':fields.add('expected_sync')
        if op=='native_service_price_ui':fields.add('command')
        if set(args)!=fields or not isinstance(args['key'],str) or not 1<=len(args['key'].encode('utf-8'))<=512 or '\0' in args['key'] or type(args['ordinal']) is not int or not 0<=args['ordinal']<=1023:bad('Service UI requires exact Building key and ordinal')
        if op!='read_native_service_sync_ui':
            for k in ('token','snapshot_uuid'):
                if not isinstance(args[k],str) or re.fullmatch(UUID,args[k]) is None:bad('Service UI requires canonical UUIDs')
            for k in ('epoch','generation'):
                if type(args[k]) is not int or not 1<=args[k]<=18446744073709551615:bad('Service UI requires positive uint64 binding')
            if op=='native_service_sync_ui' and type(args['expected_sync']) is not bool:bad('Service UI requires actual sync flag')
            if op=='native_service_price_ui' and args['command'] not in ('Increment','Decrement'):bad('Service price UI requires an original spinner direction')
        return
    if op=='read_native_campaign_ui':
        if args:bad('Campaign UI read requires empty arguments')
        return
    if op=='native_campaign_ui':
        if set(args)!={'command','token','snapshot_uuid','epoch','generation','expected_type'} or args['command'] not in ('Show','SelectAdvertise','SelectCampaign','Start'):bad('Campaign UI requires exact command and fields')
        for k in ('token','snapshot_uuid'):
            if not isinstance(args[k],str) or re.fullmatch(UUID,args[k]) is None:bad('Campaign UI requires canonical UUIDs')
        for k in ('epoch','generation'):
            if type(args[k]) is not int or not 1<=args[k]<=18446744073709551615:bad('Campaign UI requires positive uint64 binding')
        value=args['expected_type']
        if type(value) is not int or (not 0<=value<=8 if args['command'] in ('SelectCampaign','Start') else value!=-1):bad('Campaign UI type differs from command')
        return
    if op=='read_native_loan_ui':
        if args:bad('Loan UI read requires empty arguments')
        return
    if op=='native_loan_ui':
        if set(args)!={'command','token','snapshot_uuid','epoch','generation','key'} or args['command'] not in ('Show','SelectLoans','Take','Repay'):bad('Loan UI requires exact command and fields')
        for k in ('token','snapshot_uuid'):
            if not isinstance(args[k],str) or re.fullmatch(UUID,args[k]) is None:bad('Loan UI requires canonical UUIDs')
        for k in ('epoch','generation'):
            if type(args[k]) is not int or not 1<=args[k]<=18446744073709551615:bad('Loan UI requires positive uint64 binding')
        key=args['key']
        if not isinstance(key,str) or len(key.encode('utf-8'))>256 or '\0' in key or (bool(key)!=(args['command'] in ('Take','Repay'))):bad('Loan UI key differs from command')
        return
    if op=='read_native_gm_ui':
        if args:bad('GM UI read requires empty arguments')
        return
    if op=='native_gm_ui':
        if set(args)!={'command','token','snapshot_uuid','epoch','generation','expected_count'} or args['command'] not in ('Show','Hire','Fire'):bad('GM UI requires exact command and fields')
        for k in ('token','snapshot_uuid'):
            if not isinstance(args[k],str) or re.fullmatch(UUID,args[k]) is None:bad('GM UI requires canonical UUIDs')
        for k in ('epoch','generation'):
            if type(args[k]) is not int or not 1<=args[k]<=18446744073709551615:bad('GM UI requires positive uint64 binding')
        if type(args['expected_count']) is not int or not 0<=args['expected_count']<=50:bad('GM UI expected count is outside range')
        return
    if op=='read_release_planner_ui':
        if args:bad('Release planner read requires empty arguments')
        return
    if op=='native_release_planner_ui':
        fields={'command','token','snapshot_uuid','epoch','generation','expected_type','expected_funds'}
        if set(args)!=fields or args['command'] not in ('Show','SetMinor','SetHotfix','AllocatePayment'):bad('Release planner requires exact command and fields')
        for k in ('token','snapshot_uuid'):
            if not isinstance(args[k],str) or re.fullmatch(UUID,args[k]) is None:bad('Release planner requires canonical UUIDs')
        for k in ('epoch','generation'):
            if type(args[k]) is not int or not 1<=args[k]<=18446744073709551615:bad('Release planner requires positive uint64 binding')
        for k,maximum in (('expected_type',4),('expected_funds',2147483647)):
            if type(args[k]) is not int or not 0<=args[k]<=maximum:bad('Release planner expected state is outside range')
        return
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
