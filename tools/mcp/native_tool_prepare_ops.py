import re
from runtime import RuntimeErrorDetail
UUID_PATTERN=r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"
UUID={"type":"string","pattern":"^"+UUID_PATTERN+"$"}
UINT64={"type":"integer","minimum":1,"maximum":18446744073709551615}
FIELDS={"acceptance_token":UUID,"snapshot_uuid":UUID,"expected_world_epoch":UINT64,"expected_generation":UINT64}
SCHEMAS={"native_cancel_tool":{"type":"object","properties":FIELDS,"required":list(FIELDS),"additionalProperties":False},"read_cancel_tool":{"type":"object","properties":{"acceptance_token":UUID},"required":["acceptance_token"],"additionalProperties":False}}
DESCRIPTIONS={"native_cancel_tool":"Cancel the current native Build/Move preview once and switch through the original Cursor default behaviour. Keep its token; observe actual preview cleanup and idle selection/world. Pending commits are rejected.","read_cancel_tool":"Read the retained native Cancel/default-switch receipt without repeating input."}
SCHEMAS["native_buildings_tab"]=SCHEMAS["native_cancel_tool"]
SCHEMAS["read_buildings_tab"]=SCHEMAS["read_cancel_tool"]
DESCRIPTIONS["native_buildings_tab"]="Select only the original owned Edit/Buildings tab once from idle Default. Observe the retained token until tab_ready before normal placement; never replay Unknown."
DESCRIPTIONS["read_buildings_tab"]="Observe the retained Buildings tab request and native layout without repeating Press."

def validate(op,args):
    if not isinstance(args,dict) or set(args)!=(set(FIELDS) if op in ('native_cancel_tool','native_buildings_tab') else {'acceptance_token'}):raise RuntimeErrorDetail('invalid_args','Cancel tool requires exact scalar fields')
    for name in ('acceptance_token','snapshot_uuid') if op in ('native_cancel_tool','native_buildings_tab') else ('acceptance_token',):
        if not isinstance(args[name],str) or re.fullmatch(UUID_PATTERN,args[name]) is None:raise RuntimeErrorDetail('invalid_args','Cancel tool requires canonical UUIDs')
    if op in ('native_cancel_tool','native_buildings_tab'):
        for name in ('expected_world_epoch','expected_generation'):
            if type(args[name]) is not int or not 0<args[name]<2**64:raise RuntimeErrorDetail('invalid_args','Cancel tool requires positive uint64 epoch/generation')
