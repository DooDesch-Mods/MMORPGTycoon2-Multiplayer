"""Fixed Duplicate/Group/Ungroup native UI acceptance operations."""
import math
import re
from runtime import RuntimeErrorDetail

UUID_PATTERN = r'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}'
UUID = {'type': 'string', 'pattern': '^' + UUID_PATTERN + '$'}
U64 = {'type': 'integer', 'minimum': 1, 'maximum': 2**64 - 1}
F32 = 3.4028234663852886e38
FIELDS = {'acceptance_token': UUID, 'snapshot_uuid': UUID,
          'command': {'type': 'string', 'enum': ['Duplicate', 'Group', 'Ungroup']},
          'record_keys': {'type': 'array', 'minItems': 1, 'maxItems': 32, 'uniqueItems': True,
                          'items': {'type': 'string', 'minLength': 1, 'maxLength': 256}},
          'expected_world_epoch': U64, 'expected_generation': U64,
          'position': {'type': 'array', 'minItems': 3, 'maxItems': 3,
                       'items': {'type': 'number', 'minimum': -F32, 'maximum': F32}}}
SCHEMAS = {
    'native_object_ui': {'type': 'object', 'properties': FIELDS, 'required': list(FIELDS)[:-1],
                         'additionalProperties': False},
    'read_object_ui': {'type': 'object', 'properties': {'acceptance_token': UUID},
                       'required': ['acceptance_token'], 'additionalProperties': False}}
DESCRIPTIONS = {
    'native_object_ui': 'Select exact current canonical objects and invoke genuine Duplicate/Group/Ungroup InfoWindow command once. Duplicate also runs native cursor input, then ordinary GameUpdate Purchase capture. Retain token; native return is not Host completion. Never replay Unknown.',
    'read_object_ui': 'Read one retained object UI token, exact captured source keys, native operation, local retirement and Controller revision. Full paired Host ACK is checked separately.'}

def validate(op, args):
    def reject(cause):
        raise RuntimeErrorDetail('invalid_args', 'Object UI: ' + cause)
    if not isinstance(args, dict):
        reject('object arguments required')
    if op not in SCHEMAS:
        reject('unsupported operation')
    fields = {'acceptance_token'} if op == 'read_object_ui' else set(FIELDS) - {'position'}
    if op == 'native_object_ui' and args.get('command') == 'Duplicate':
        fields.add('position')
    if set(args) != fields:
        reject('exact command fields required')
    for name in ('acceptance_token',) if op == 'read_object_ui' else ('acceptance_token', 'snapshot_uuid'):
        if not isinstance(args[name], str) or re.fullmatch(UUID_PATTERN, args[name]) is None:
            reject('canonical UUID required')
    if op == 'read_object_ui':
        return
    if args['command'] not in ('Duplicate', 'Group', 'Ungroup'):
        reject('unsupported command')
    for name in ('expected_world_epoch', 'expected_generation'):
        if type(args[name]) is not int or not 0 < args[name] < 2**64:
            reject('positive uint64 native lifetime required')
    keys = args['record_keys']
    if not isinstance(keys, list) or not 1 <= len(keys) <= 32 or any(not isinstance(key, str) or not 1 <= len(key) <= 256 or '\0' in key for key in keys) or len(set(keys)) != len(keys):
        reject('unique bounded current canonical keys required')
    if args['command'] == 'Duplicate':
        values = args['position']
        if len(keys) != 1 or not isinstance(values, list) or len(values) != 3 or any(type(value) not in (int, float) or not math.isfinite(value) or abs(value) > F32 for value in values):
            reject('one Duplicate target and three finite float32 coordinates required')
