"""Bound native multiplayer menu observation and one native activation."""
import re
from runtime import RuntimeErrorDetail

BUTTONS = ("open", "host", "host_public", "search", "list_refresh", "list_previous", "list_next", "list_back", "list0", "list1", "list2", "list3", "join", "pick", "prepare", "start", "leave")
UUID_PATTERN = r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"
SCHEMAS = {
    "read_native_lobby": {"type": "object", "properties": {}, "additionalProperties": False},
    "native_lobby_button": {
        "type": "object", "properties": {
            "acceptance_token": {"type": "string", "pattern": "^" + UUID_PATTERN + "$"},
            "button": {"type": "string", "enum": list(BUTTONS)},
            "lobby_id": {"type": "string", "pattern": "^[1-9][0-9]{0,19}$"},
            "world": {"type": "string", "minLength": 1, "maxLength": 200}},
        "required": ["acceptance_token", "button"], "additionalProperties": False},
}
DESCRIPTIONS = {
    "read_native_lobby": "Read actual registered native desktop/menu panes, disabled flags, command strings, callbacks, physical lobby membership and preparation receipts. Membership is not Ready.",
    "native_lobby_button": "Activate one actual native multiplayer ClickView/ButtonPane on the bound active-mode thread once. Keep its token and observe callbacks with read_native_lobby. Native return is not Steam admission or world readiness. Invitation operations are excluded.",
}


def validate(op, args):
    if not isinstance(args, dict):
        raise RuntimeErrorDetail("invalid_args", "native lobby requires scalar argument fields")
    if op == "read_native_lobby":
        if args:
            raise RuntimeErrorDetail("invalid_args", "native lobby read requires empty arguments")
        return
    if set(args) not in ({"acceptance_token", "button"}, {"acceptance_token", "button", "lobby_id"}, {"acceptance_token", "button", "world"}):
        raise RuntimeErrorDetail("invalid_args", "native lobby requires exact token/button and optional Join lobby_id")
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
