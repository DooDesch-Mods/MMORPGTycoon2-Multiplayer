"""Guards for the original ordered Research UI fixture."""
import re

COMMANDS = ('Read', 'Show', 'State', 'Start', 'StaffShow', 'StaffState', 'StaffOne', 'Commit')
FIELDS = {'token', 'native_origin_uuid', 'operation', 'world_epoch', 'world_generation', 'expected_development', 'tech_key'}
UUID = r'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}'

def validate(arguments):
    if not isinstance(arguments, dict) or set(arguments) != FIELDS:
        raise ValueError('Research UI requires exact token, Origin, development, Tech key and operation')
    if arguments['operation'] not in COMMANDS:
        raise ValueError('Research UI requires an original offered control operation')
    if any(not isinstance(arguments[key], str) or re.fullmatch(UUID, arguments[key]) is None for key in ('token', 'native_origin_uuid')):
        raise ValueError('Research UI requires canonical UUID guards')
    key = arguments['tech_key']
    if not isinstance(key, str) or not 1 <= len(key.encode('utf-8')) <= 200 or '\0' in key:
        raise ValueError('Research UI requires an actual catalogue Tech key')
    if any(type(arguments[key]) is not int or not 1 <= arguments[key] <= 18446744073709551615 for key in ('world_epoch', 'world_generation')):
        raise ValueError('Research UI requires positive native Origin epoch and generation')
    if not isinstance(arguments['expected_development'], dict) or not arguments['expected_development']:
        raise ValueError('Research UI requires current development readback')
