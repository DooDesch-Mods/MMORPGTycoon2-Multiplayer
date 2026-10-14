"""Bounded live state admission and retained actual receipt operations."""
from runtime import RuntimeErrorDetail
SCHEMAS={name:{'type':'object','properties':{},'required':[],'additionalProperties':False} for name in ('enable_live_host_state','read_live_host_state')}
DESCRIPTIONS={'enable_live_host_state':'Arm actual native Client economic fences and Host Clock/Ledger replication in the existing action session once. Retain Unknown; never replay activation.', 'read_live_host_state':'Read attempted, confirmed and Unknown live native state stores from the actual bound Mainthread runtime.'}
def validate(op,args):
    if op not in SCHEMAS or not isinstance(args,dict) or args:raise RuntimeErrorDetail('invalid_args','Live state operation requires exact empty arguments')
def receipt(status):
    if not isinstance(status,dict) or not isinstance(status.get('live_state'),dict):raise RuntimeErrorDetail('missing_native_receipt','Actual Root live state receipt is missing')
    data=status['live_state']
    for name in ('requested','active','locked','attempted','confirmed','unknown'):
        if type(data.get(name)) is not bool:raise RuntimeErrorDetail('invalid_native_receipt','Live native receipt boolean missing: '+name)
    for name in ('attempted_stores','completed_stores'):
        if type(data.get(name)) is not int or not 0<=data[name]<=52:raise RuntimeErrorDetail('invalid_native_receipt','Live native fixed-slot count invalid: '+name)
    if data['completed_stores']>data['attempted_stores'] or data['unknown'] and data['confirmed'] or data['unknown'] and not data['attempted'] or data['unknown'] and not data['locked']:raise RuntimeErrorDetail('invalid_native_receipt','Live native Unknown/count/confirmation differs')
    if not isinstance(data.get('cause'),str) or data['unknown'] and not data['cause']:raise RuntimeErrorDetail('invalid_native_receipt','Live native Unknown original cause missing')
    return data.copy()
