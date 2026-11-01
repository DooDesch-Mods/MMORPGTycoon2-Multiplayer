"""One bounded original Scenery gesture in an owned paused test world."""
import math
import re
from runtime import RuntimeErrorDetail
UUID_PATTERN=r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"
UUID={"type":"string","pattern":"^"+UUID_PATTERN+"$"}
FIELDS={"token":UUID,"snapshot_uuid":UUID,
    "expected_world_epoch":{"type":"integer","minimum":1,"maximum":2**64-1},
    "expected_generation":{"type":"integer","minimum":1,"maximum":2**64-1},
    "kind":{"type":"string","enum":["paint","erase"]},
    "region_id":{"type":"integer","minimum":0,"maximum":2**31-1},
    "points":{"type":"array","minItems":2,"maxItems":32,"items":{"type":"array","minItems":3,"maxItems":3,"items":{"type":"number"}}}}
SCHEMAS={"native_scenery_gesture":{"type":"object","properties":{**FIELDS,"selection":{"type":"object","properties":{"type":{"type":"string","maxLength":256},"variant":{"type":"string","maxLength":256}},"required":["type","variant"],"additionalProperties":False}},"required":list(FIELDS),"additionalProperties":False},
    "read_scenery_gesture":{"type":"object","properties":{"token":UUID},"required":["token"],"additionalProperties":False}}
DESCRIPTIONS={"native_scenery_gesture":"In ordered mode, queue semantic selection and one original Scenery gesture for the next actual OriginalGo (paint selection type/variant, erase empty strings). In legacy mode, invoke the currently selected original Scenery paint/eraser Hover, Press, Drag and Release once in an owned paused local-test world. Requires exact Snapshot/World/Region and 2..32 finite points in that Region. The returned captured receipt is queued input, not Host/Client action confirmation. Observe its action UUID and both Controller/Frame acknowledgements. Retain any attempted or uncertain token and never replay input.",
    "read_scenery_gesture":"Read the retained Scenery gesture token without executing input again. Captured does not mean the shared world effect is confirmed."}
def validate(op,args):
    expected=set(FIELDS) if op=='native_scenery_gesture' else {'token'}
    if op not in SCHEMAS or not isinstance(args,dict) or (set(args)!=expected and not (op=='native_scenery_gesture' and set(args)==expected|{'selection'})):raise RuntimeErrorDetail('invalid_args','Scenery gesture requires exact fields')
    for name in ('token','snapshot_uuid') if op=='native_scenery_gesture' else ('token',):
        if not isinstance(args[name],str) or re.fullmatch(UUID_PATTERN,args[name]) is None:raise RuntimeErrorDetail('invalid_args','Scenery gesture requires canonical UUIDs')
    if op=='read_scenery_gesture':return
    for name in ('expected_world_epoch','expected_generation'):
        if type(args[name]) is not int or not 0<args[name]<2**64:raise RuntimeErrorDetail('invalid_args','Scenery gesture requires positive uint64 World binding')
    if args['kind'] not in ('paint','erase') or type(args['region_id']) is not int or not 0<=args['region_id']<2**31:raise RuntimeErrorDetail('invalid_args','Scenery gesture kind/Region invalid')
    if 'selection' in args:
        selected=args['selection']
        if not isinstance(selected,dict) or set(selected)!={'type','variant'} or any(not isinstance(v,str) or len(v)>256 or '\0' in v or (not v if args['kind']=='paint' else bool(v)) for v in selected.values()):raise RuntimeErrorDetail('invalid_args','Scenery semantic selection differs')
    points=args['points']
    if not isinstance(points,list) or not 2<=len(points)<=32:raise RuntimeErrorDetail('invalid_args','Scenery gesture requires 2..32 points')
    for point in points:
        if not isinstance(point,list) or len(point)!=3 or any(type(v) not in (int,float) or not math.isfinite(v) or abs(v)>3.4028234663852886e38 for v in point):raise RuntimeErrorDetail('invalid_args','Scenery gesture requires finite float32 points')
