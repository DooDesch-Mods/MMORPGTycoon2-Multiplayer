"""Narrow existing native Move UI operations."""
import math,re
from runtime import RuntimeErrorDetail
UUID_PATTERN=r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"
from building_keys import KEY_PATTERN, valid_building_key
UUID={"type":"string","pattern":"^"+UUID_PATTERN+"$"}
UINT64={"type":"integer","minimum":1,"maximum":18446744073709551615}
F32=3.4028234663852886e38
POSITION={"type":"number","minimum":-F32,"maximum":F32}
FIELDS={"acceptance_token":UUID,"snapshot_uuid":UUID,"record_key":{"type":"string","pattern":"^"+KEY_PATTERN+"$"},"expected_world_epoch":UINT64,"expected_generation":UINT64,"x":POSITION,"y":POSITION,"z":POSITION}
SCHEMAS={"native_move_ui":{"type":"object","properties":FIELDS,"required":list(FIELDS),"additionalProperties":False},"read_move_ui":{"type":"object","properties":{"acceptance_token":UUID},"required":["acceptance_token"],"additionalProperties":False}}
DESCRIPTIONS={"native_move_ui":"Select one actual snapshot-bound SmallInn, invoke its real BuildingInfoWindow Move command and normal native cursor callbacks once. Preserve the token. Native return is not capture or Host completion; never replay Unknown.","read_move_ui":"Read the retained normal native Move token with exact capture UUID, cleanup/submission and authoritative local native completion."}
def validate(op,args):
    fields=set(FIELDS) if op=='native_move_ui' else {'acceptance_token'}
    if not isinstance(args,dict) or set(args)!=fields:raise RuntimeErrorDetail('invalid_args','Move UI requires exact scalar fields')
    for name in ('acceptance_token','snapshot_uuid') if op=='native_move_ui' else ('acceptance_token',):
        if not isinstance(args[name],str) or re.fullmatch(UUID_PATTERN,args[name]) is None:raise RuntimeErrorDetail('invalid_args','Move UI requires canonical UUIDs')
    if op=='read_move_ui':return
    key=args['record_key']
    if not valid_building_key(key):raise RuntimeErrorDetail('invalid_args','Move UI requires canonical existing building key')
    for name in ('expected_world_epoch','expected_generation'):
        if type(args[name]) is not int or not 0<args[name]<2**64:raise RuntimeErrorDetail('invalid_args','Move UI requires positive uint64 epoch/generation')
    for name in ('x','y','z'):
        value=args[name]
        if type(value) not in (int,float) or abs(value)>F32 or not math.isfinite(value):raise RuntimeErrorDetail('invalid_args','Move UI requires finite float32 position')
