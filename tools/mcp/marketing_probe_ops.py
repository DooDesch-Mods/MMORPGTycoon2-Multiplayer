"""One owned Client attempt to insert the two retained Marketing records."""
import re
from runtime import RuntimeErrorDetail

UUID = r'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}'
SCHEMAS = {'insert_marketing_record_probe': {'type': 'object', 'properties': {
    'artifact_basename': {'type': 'string', 'pattern': r'^[a-z0-9-]{1,155}\.json$'},
    'artifact_sha256': {'type': 'string', 'pattern': r'^[0-9a-f]{64}$'},
    'native_origin_uuid': {'type': 'string', 'pattern': '^' + UUID + '$'},
    'native_origin_sha256': {'type': 'string', 'pattern': r'^[0-9a-f]{64}$'},
    'token': {'type': 'string', 'pattern': '^' + UUID + '$'},
    'native_epoch': {'type': 'integer', 'minimum': 1, 'maximum': 18446744073709551615},
    'native_generation': {'type': 'integer', 'minimum': 1, 'maximum': 18446744073709551615}},
    'required': ['artifact_basename', 'artifact_sha256', 'native_origin_uuid', 'native_origin_sha256', 'token', 'native_epoch', 'native_generation'],
    'additionalProperties': False}}
DESCRIPTIONS = {'insert_marketing_record_probe':
    'One owned Client attempt to add the retained Most Stable Award and PositiveStreamer campaign to the exact paused day 2 fixture. Uses original typed Record Load, native insertion and Boost rebuild. Replays are rejected and uncertain owners retained. No grant, payout, toast or Frame transport.'}

def validate(op, args):
    if op not in SCHEMAS or not isinstance(args, dict) or set(args) != set(SCHEMAS[op]['required']):
        raise RuntimeErrorDetail('invalid_args', 'Marketing probe requires seven exact arguments')
    patterns = {'artifact_basename': r'[a-z0-9-]{1,155}\.json', 'artifact_sha256': r'[0-9a-f]{64}', 'native_origin_sha256': r'[0-9a-f]{64}', 'native_origin_uuid': UUID, 'token': UUID}
    for key, pattern in patterns.items():
        if not isinstance(args[key], str) or re.fullmatch(pattern, args[key]) is None:
            raise RuntimeErrorDetail('invalid_args', 'Marketing probe field invalid: ' + key)
    for key in ('native_epoch', 'native_generation'):
        if type(args[key]) is not int or not 1 <= args[key] <= 18446744073709551615:
            raise RuntimeErrorDetail('invalid_args', 'Marketing probe world identity invalid: ' + key)
