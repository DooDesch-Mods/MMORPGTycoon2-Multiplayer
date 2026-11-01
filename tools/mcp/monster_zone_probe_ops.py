import math,re
from runtime import RuntimeErrorDetail
UUID_PATTERN=r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"
UUID={"type":"string","pattern":"^"+UUID_PATTERN+"$"}
FIELDS={"token":UUID,"snapshot_uuid":UUID,"expected_world_epoch":{"type":"integer","minimum":1,"maximum":2**64-1},"expected_generation":{"type":"integer","minimum":1,"maximum":2**64-1},"region_id":{"type":"integer","minimum":0,"maximum":2**31-1},"phase":{"type":"string","enum":["open","select","state","stroke","hover","prepare_tab"]},"character_type":{"type":"integer","minimum":-1,"maximum":2**31-1},"points":{"type":"array","maxItems":8,"items":{"type":"array","minItems":3,"maxItems":3,"items":{"type":"number"}}}}
SCHEMAS={"native_monster_zone_probe":{"type":"object","properties":FIELDS,"required":list(FIELDS),"additionalProperties":False}}
DESCRIPTIONS={"native_monster_zone_probe":"Fixed original MonsterZone UI probe in an isolated local-test Host before Controller initialization. prepare_tab presses the fixed owned Edit/NPCs tab once; state observes pending view settlement without replay. Open/select/state/stroke use the actual owned item and catalogue; stroke calls original MouseOver/Press/Drag/Release once. Full native Zone/Monster/RNG/Ledger/Undo/Release receipts are local diagnostic evidence, not an ordered action or peer ACK. Never replay an attempted or unknown token."}
def validate(op,a):
    def fail(cause):raise RuntimeErrorDetail('invalid_args',cause)
    if op not in SCHEMAS or not isinstance(a,dict) or set(a)!=set(FIELDS):fail('zone probe requires exact fixed fields')
    for field in ('token','snapshot_uuid'):
        if not isinstance(a[field],str) or re.fullmatch(UUID_PATTERN,a[field]) is None:fail('zone probe requires canonical UUIDs')
    for field,low,high in [('expected_world_epoch',1,2**64-1),('expected_generation',1,2**64-1),('region_id',0,2**31-1),('character_type',-1,2**31-1)]:
        if type(a[field]) is not int or not low<=a[field]<=high:fail('zone probe integer out of range: '+field)
    phase=a['phase'];points=a['points'];kind=a['character_type']
    if phase not in (('open','select','state','stroke','hover','prepare_tab','receipt') if op=='native_monster_zone_ui' else ('open','select','state','stroke','hover','prepare_tab')) or not isinstance(points,list) or len(points)>8:fail('zone probe phase/point count invalid')
    if (phase=='stroke' and (len(points)<2 or kind<0)) or (phase=='hover' and (len(points)!=1 or kind!=-1)) or (phase not in ('stroke','hover') and points) or (phase in ('open','state','prepare_tab','receipt') and kind!=-1) or (phase=='select' and kind<0):fail('zone probe phase payload differs')
    for point in points:
        if not isinstance(point,list) or len(point)!=3:fail('zone probe requires XYZ points')
        for v in point:
            if type(v) not in (int,float) or not math.isfinite(v) or abs(v)>3.4028234663852886e38:fail('zone probe point outside finite float32')

SCHEMAS["native_monster_zone_ui"] = {**SCHEMAS["native_monster_zone_probe"],"properties":{**FIELDS,"phase":{"type":"string","enum":["open","select","state","stroke","hover","prepare_tab","receipt"]}}}
DESCRIPTIONS["native_monster_zone_ui"] = "Drive the fixed owned MonsterZone UI. Ordered input is queued once for actual OriginalGo using the retained exact Session and Binding. Read receipt with the same token and owner, character_type=-1 and points=[]; never replay a queued, attempted or uncertain operation. State is read-only. Hover calls original MouseOver once at one owned Region point without pressing or changing camera; its full native state must remain equal. Original tab, item and catalogue selection remain native. Creation currently supports catalogue IDs 0..3; higher IDs require the unadopted achievement effect. A returned input is not a retired ordered command; inspect ui_monster_zone.ui_jobs and both peers' command receipts."
