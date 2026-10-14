"""Capture one genuine Host Subscriber into a private native Record artifact."""
import re
from runtime import RuntimeErrorDetail

UUID = r'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}'
SCHEMAS = {'capture_subscriber_record': {
    'type': 'object', 'properties': {
        'serialized_id': {'type': 'integer', 'minimum': 0, 'maximum': 2147483647},
        'native_origin_uuid': {'type': 'string', 'pattern': '^' + UUID + '$'}},
    'required': ['serialized_id', 'native_origin_uuid'], 'additionalProperties': False}}
DESCRIPTIONS = {'capture_subscriber_record':
    'Capture one existing natural Host Subscriber on the game thread. Original PrepareSave and SaveToRecord, native temporary lifetime, exact owner census and private artifact readback. Does not create a Client Subscriber. Do not replay a timed-out capture.'}

def validate(op, args):
    if op not in SCHEMAS or not isinstance(args, dict) or set(args) != {'serialized_id', 'native_origin_uuid'}:
        raise RuntimeErrorDetail('invalid_args', 'Subscriber capture requires exact serialized_id/native_origin_uuid fields')
    number = args['serialized_id']
    if type(number) is not int or not 0 <= number <= 2147483647:
        raise RuntimeErrorDetail('invalid_args', 'Subscriber capture ID requires int32 range 0..2147483647')
    origin = args['native_origin_uuid']
    if not isinstance(origin, str) or re.fullmatch(UUID, origin) is None:
        raise RuntimeErrorDetail('invalid_args', 'Subscriber capture requires canonical native origin UUID')
