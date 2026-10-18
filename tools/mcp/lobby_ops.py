"""Bound native multiplayer menu observation and one native activation."""
import re
from runtime import RuntimeErrorDetail

BUTTONS = ("icon_single", "icon_drag", "open", "start_menu", "host", "host_public", "search", "list_refresh", "list_previous", "list_next", "list_back", "list0", "list1", "list2", "list3", "join", "pick", "prepare", "start", "leave", "chat_send")
UUID_PATTERN = r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"
SCHEMAS = {
    "read_native_lobby": {"type": "object", "properties": {}, "additionalProperties": False},
    "native_lobby_button": {
        "type": "object", "properties": {
            "acceptance_token": {"type": "string", "pattern": "^" + UUID_PATTERN + "$"},
            "button": {"type": "string", "enum": list(BUTTONS)},
            "lobby_id": {"type": "string", "pattern": "^[1-9][0-9]{0,19}$"},
            "world": {"type": "string", "minLength": 1, "maxLength": 200},
            "text": {"type": "string", "minLength": 1, "maxLength": 240}},
        "required": ["acceptance_token", "button"], "additionalProperties": False},
}
DESCRIPTIONS = {
    "read_native_lobby": "Read actual registered native desktop/menu panes, disabled flags, command strings, callbacks, physical lobby membership and preparation receipts. Membership is not Ready.",
    "native_lobby_button": "Activate one actual native multiplayer DesktopIcon/ButtonPane on the bound active-mode thread once. icon_single probes native Press/Release without opening; icon_drag probes Press/Drag/Release with one bounded 20-pixel move and readback; both require the visible Title Desktop and an idle real cursor. No cursor warp or direct position write. start_menu opens the original in-game StartMenu through native Show, requires its original build callback and actual copied key/caption plus native TextPane item readback. open in-game then selects its existing Multiplayer entry once; Title open retains DesktopIcon double-click. chat_send optionally sets bounded text through the native input setter, checks exact readback, then presses Send; without text it uses the existing draft. Keep its token and observe callbacks with read_native_lobby; Chat delivery requires recipient acknowledgement from read_chat. Native return is not Steam admission or world readiness. Invitation operations are excluded.",
}


def validate(op, args):
    if not isinstance(args, dict):
        raise RuntimeErrorDetail("invalid_args", "native lobby requires scalar argument fields")
    if op == "read_native_lobby":
        if args:
            raise RuntimeErrorDetail("invalid_args", "native lobby read requires empty arguments")
        return
    if set(args) not in ({"acceptance_token", "button"}, {"acceptance_token", "button", "lobby_id"}, {"acceptance_token", "button", "world"}, {"acceptance_token", "button", "text"}):
        raise RuntimeErrorDetail("invalid_args", "native lobby requires exact token/button and one optional Join lobby_id, Pick world or Chat text")
    if not isinstance(args["acceptance_token"], str) or re.fullmatch(UUID_PATTERN, args["acceptance_token"]) is None:
        raise RuntimeErrorDetail("invalid_args", "native lobby token requires a canonical UUID")
    if not isinstance(args["button"], str) or args["button"] not in BUTTONS:
        raise RuntimeErrorDetail("invalid_args", "native lobby button is not admitted")
    if "lobby_id" in args:
        value = args["lobby_id"]
        if args["button"] != "join" or not isinstance(value, str) or re.fullmatch(r"[1-9][0-9]{0,19}", value) is None or int(value) > 18446744073709551615:
            raise RuntimeErrorDetail("invalid_args", "native Join lobby_id requires a positive uint64 decimal string")
    if "world" in args:
        value=args["world"]
        if args["button"]!="pick" or not isinstance(value,str) or not value or value==".." or any(c in value for c in '/\\"\r\n\t\0'):
            raise RuntimeErrorDetail("invalid_args", "native Pick world requires an existing native save name")
        try:
            size=len(value.encode('utf-8'))
        except UnicodeEncodeError as cause:
            raise RuntimeErrorDetail("invalid_args", "native Pick world requires UTF-8") from cause
        if size>200:
            raise RuntimeErrorDetail("invalid_args", "native Pick world exceeds 200 bytes")
    if "text" in args:
        value = args['text']
        if args['button'] != 'chat_send' or not isinstance(value, str) or not value or any(ord(c) < 32 or 127 <= ord(c) <= 159 for c in value):
            raise RuntimeErrorDetail('invalid_args', 'native Chat text requires 1 to 240 UTF-8 bytes without controls')
        try:
            size = len(value.encode('utf-8'))
        except UnicodeEncodeError as cause:
            raise RuntimeErrorDetail('invalid_args', 'native Chat text requires valid UTF-8') from cause
        if size > 240:
            raise RuntimeErrorDetail('invalid_args', 'native Chat text exceeds 240 bytes')
