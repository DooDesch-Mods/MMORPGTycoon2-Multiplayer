import math
import re
from runtime import RuntimeErrorDetail
UUID_PATTERN=r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"
FIELDS={"token":{"type":"string","pattern":"^"+UUID_PATTERN+"$"},"id":{"type":"integer","minimum":0,"maximum":2147483647},"cell":{"type":"integer","minimum":0,"maximum":999999},"up":{"type":"boolean"},"radius":{"type":"number","minimum":1,"maximum":1024}}
SCHEMAS={"native_terrain_height":{"type":"object","properties":FIELDS,"required":list(FIELDS),"additionalProperties":False}}
DESCRIPTIONS={"native_terrain_height":"Select the actual Height ItemButton for up/down, then original Press (which invokes virtual Drag once) and Release for one native cell. Keep the attempted token, observe ui_terrain.ui_jobs and action_uuid until the existing Host controller confirms. Never replay after timeout or Unknown. Cell and Region are validated against the native grid; radius is 1..1024."}
def validate(op,args):
    if op!='native_terrain_height' or not isinstance(args,dict) or set(args)!=set(FIELDS):raise RuntimeErrorDetail('invalid_args','Height UI requires exact token/id/cell/up/radius')
    if not isinstance(args['token'],str) or re.fullmatch(UUID_PATTERN,args['token']) is None:raise RuntimeErrorDetail('invalid_args','Height UI requires canonical UUID token')
    if type(args['id']) is not int or not 0<=args['id']<=2147483647 or type(args['cell']) is not int or not 0<=args['cell']<1000000:raise RuntimeErrorDetail('invalid_args','Height UI requires nonnegative int32 Region and bounded cell')
    if type(args['up']) is not bool or type(args['radius']) not in (int,float) or not math.isfinite(args['radius']) or not 1<=args['radius']<=1024:raise RuntimeErrorDetail('invalid_args','Height UI requires boolean up and finite radius 1..1024')
