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
SCHEMAS["native_scenery_tab"]=SCHEMAS["native_cancel_tool"]
SCHEMAS["read_scenery_tab"]=SCHEMAS["read_cancel_tool"]
SCHEMAS["native_network_tab"]=SCHEMAS["native_cancel_tool"]
SCHEMAS["read_network_tab"]=SCHEMAS["read_cancel_tool"]
DESCRIPTIONS["native_buildings_tab"]="Select only the original owned Edit/Buildings tab once from idle Default. Observe the retained token until tab_ready before normal placement; never replay Unknown."
DESCRIPTIONS["read_buildings_tab"]="Observe the retained Buildings tab request and native layout without repeating Press."
DESCRIPTIONS["native_scenery_tab"]="Select the original Scenery tab once for the exact current world; retain the token and observe tab_ready before painting."
DESCRIPTIONS["read_scenery_tab"]="Read the retained Scenery tab and layout acknowledgement without repeating input."
DESCRIPTIONS["native_network_tab"]="Select the original Network tab once for the exact current world; observe the retained token until tab_ready before construction."
DESCRIPTIONS["read_network_tab"]="Read the retained Network tab and layout acknowledgement without repeating input."
SCHEMAS["native_interact_tab"]=SCHEMAS["native_cancel_tool"]
SCHEMAS["read_interact_tab"]=SCHEMAS["read_cancel_tool"]
DESCRIPTIONS["native_interact_tab"]="Select the original Interact tab once for the exact current world. Observe its original Map view request and retained token; never replay Unknown."
DESCRIPTIONS["read_interact_tab"]="Read the retained original Interact tab, Map request and layout without repeating input."
CONTEXT_OPS={'native_interact_tab','native_cancel_tool','native_buildings_tab','native_scenery_tab','native_network_tab'}

def validate(op,args):
    if not isinstance(args,dict) or set(args)!=(set(FIELDS) if op in CONTEXT_OPS else {'acceptance_token'}):raise RuntimeErrorDetail('invalid_args','Tool preparation requires exact scalar fields')
    for name in ('acceptance_token','snapshot_uuid') if op in CONTEXT_OPS else ('acceptance_token',):
        if not isinstance(args[name],str) or re.fullmatch(UUID_PATTERN,args[name]) is None:raise RuntimeErrorDetail('invalid_args','Cancel tool requires canonical UUIDs')
    if op in CONTEXT_OPS:
        for name in ('expected_world_epoch','expected_generation'):
            if type(args[name]) is not int or not 0<args[name]<2**64:raise RuntimeErrorDetail('invalid_args','Cancel tool requires positive uint64 epoch/generation')
