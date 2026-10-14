"""Bounded original Client append probe for an owned Host capture artifact."""
import re
from runtime import RuntimeErrorDetail

UUID = r'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}'
SCHEMAS = {'append_subscriber_record_probe': {
    'type':'object','properties': {
        'artifact_basename':{'type':'string','pattern':r'^[a-z0-9-]+\.json$','maxLength':160},
        'artifact_sha256':{'type':'string','pattern':'^[0-9a-f]{64}$'},
        'native_origin_uuid':{'type':'string','pattern':'^'+UUID+'$'}},
    'required':['artifact_basename','artifact_sha256','native_origin_uuid'],'additionalProperties':False}}
DESCRIPTIONS = {'append_subscriber_record_probe':
    'Append one genuinely captured Host member through original Client C1/load/resolve. Mutates native membership under the original save scope. Read back exact cohort, Toon registry, needs and finance. Never replay a timed-out or unknown append.'}

def validate(op,args):
    if op not in SCHEMAS or not isinstance(args,dict) or set(args)!=set(SCHEMAS[op]['required']):
        raise RuntimeErrorDetail('invalid_args','Subscriber append requires exact artifact_basename/artifact_sha256/native_origin_uuid')
    patterns = {'artifact_basename':r'[a-z0-9-]+\.json','artifact_sha256':r'[0-9a-f]{64}','native_origin_uuid':UUID}
    for field,pattern in patterns.items():
        if not isinstance(args[field],str) or re.fullmatch(pattern,args[field]) is None:
            raise RuntimeErrorDetail('invalid_args','Subscriber append field invalid: '+field)
    if not 6<=len(args['artifact_basename'])<=160:
        raise RuntimeErrorDetail('invalid_args','Subscriber append artifact name length must be 6..160')
