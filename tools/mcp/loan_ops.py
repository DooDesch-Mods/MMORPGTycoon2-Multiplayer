"""Read genuine native Loan/Offer rows or run one original engine operation."""
import re
from runtime import RuntimeErrorDetail

UUID=r'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}'
I32={'type':'integer','minimum':-2147483648,'maximum':2147483647}
def row(offer):
    result={'type':'object','properties':{'text':{'type':'array','items':{'type':'string','maxLength':2048},'minItems':3,'maxItems':3},'amounts':{'type':'array','items':I32,'minItems':3 if offer else 4,'maxItems':3 if offer else 4}},'required':['text','amounts'],'additionalProperties':False}
    if offer:
        result['properties'].update({'eligibility':I32,'available':{'type':'boolean'}})
        result['required']+=['eligibility','available']
    return result
SCHEMAS={'read_native_loans':{'type':'object','properties':{},'additionalProperties':False}}
for op,binding,offer in (('native_take_loan_fixture','native_origin_uuid',True),('native_loan_payment','snapshot_uuid',False)):
    SCHEMAS[op]={'type':'object','properties':{'token':{'type':'string','pattern':'^'+UUID+'$'},binding:{'type':'string','pattern':'^'+UUID+'$'},'expected':row(offer)},'required':['token',binding,'expected'],'additionalProperties':False}
DESCRIPTIONS={
    'read_native_loans':'Read actual native-owned Loan/Offer IDs, amounts, original IsAvailable eligibility, full Ledger and retained once-only diagnostic receipts. No pointer output.',
    'native_take_loan_fixture':'Invoke original Business.TakeLoan once for an exact existing available Offer. Host pre-controller fixture only: new Loan and consumed Offer membership cannot be copied by Economy1. Save/load through the existing shared snapshot afterward. Engine route, not a UI-click proof.',
    'native_loan_payment':'Invoke original Loan.NewDay once on an existing Loan in actual live Host GameUpdate. Requires Economy1 capture and retained Loan membership, sufficient cash and positive principal installment. Native confirmation is local; inspect actual Client Economy1 readback separately.',
}
def validate(op,args):
    def bad(cause):raise RuntimeErrorDetail('invalid_args',cause)
    if not isinstance(args,dict):bad('Native Loan requires exact argument object')
    if op=='read_native_loans':
        if args:bad('Native Loan read requires empty arguments')
        return
    if op not in SCHEMAS:bad('Native Loan operation is unknown')
    offer=op=='native_take_loan_fixture';binding='native_origin_uuid' if offer else 'snapshot_uuid'
    if set(args)!={'token',binding,'expected'}:bad('Native Loan requires exact token/binding/expected fields')
    for key in ('token',binding):
        if not isinstance(args[key],str) or re.fullmatch(UUID,args[key]) is None:bad('Native Loan '+key+' requires canonical UUID')
    value=args['expected'];fields={'text','amounts','eligibility','available'} if offer else {'text','amounts'}
    if not isinstance(value,dict) or set(value)!=fields:bad('Native Loan expected row fields differ')
    text=value['text'];amounts=value['amounts']
    if not isinstance(text,list) or len(text)!=3 or not isinstance(amounts,list) or len(amounts)!=(3 if offer else 4):bad('Native Loan native row array sizes differ')
    for i,s in enumerate(text):
        if not isinstance(s,str) or '\0' in s or (i==0 and not s):bad('Native Loan native string invalid')
        try:size=len(s.encode('utf-8'))
        except UnicodeEncodeError as cause:raise RuntimeErrorDetail('invalid_args','Native Loan string requires UTF-8') from cause
        if size>(2048 if i else 256):bad('Native Loan native string byte bound exceeded')
    for n in amounts+([value['eligibility']] if offer else []):
        if type(n) is not int or not -2147483648<=n<=2147483647:bad('Native Loan amounts/eligibility require exact int32')
    if offer and type(value['available']) is not bool:bad('Native Loan availability requires exact boolean')
