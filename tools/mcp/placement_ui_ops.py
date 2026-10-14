"""Two bounded native normal-building diagnostic operations."""
import math
import re
from runtime import RuntimeErrorDetail

UUID_PATTERN = r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"
UUID = {"type": "string", "pattern": "^" + UUID_PATTERN + "$"}
UINT64 = {"type": "integer", "minimum": 1, "maximum": 18446744073709551615}
FLOAT32_MAX = 3.4028234663852886e38
COORDINATE = {"type": "number", "minimum": -FLOAT32_MAX, "maximum": FLOAT32_MAX}
PROPERTIES = {
    "acceptance_token": UUID, "snapshot_uuid": UUID,
    "expected_world_epoch": UINT64, "expected_generation": UINT64,
    "type": {"type": "string", "minLength": 1, "maxLength": 256},
    "variant": {"type": "string", "minLength": 1, "maxLength": 256},
    "x": COORDINATE, "y": COORDINATE, "z": COORDINATE,
}
SCHEMAS = {
    "native_build_button": {"type": "object", "properties": PROPERTIES,
                            "required": list(PROPERTIES), "additionalProperties": False},
    "read_build_ui": {"type": "object", "properties": {"acceptance_token": UUID},
                      "required": ["acceptance_token"], "additionalProperties": False},
}
DESCRIPTIONS = {
    "native_build_button": "Activate the existing normal-building ItemButton and its normal native cursor callbacks once. Keep the token and read its receipt. Native return is not capture or Host confirmation; never replay Unknown.",
    "read_build_ui": "Read one native normal-building diagnostic by token, with distinct callback, actual GameUpdate capture and complete Host/native receipt states.",
}


def validate(op, args):
    expected = set(PROPERTIES) if op == "native_build_button" else {"acceptance_token"}
    if not isinstance(args, dict) or set(args) != expected:
        raise RuntimeErrorDetail("invalid_args", "building UI requires exact scalar fields")
    for name in ("acceptance_token", "snapshot_uuid") if op == "native_build_button" else ("acceptance_token",):
        if not isinstance(args[name], str) or re.fullmatch(UUID_PATTERN, args[name]) is None:
            raise RuntimeErrorDetail("invalid_args", "building UI requires canonical UUIDs")
    if op == "read_build_ui":
        return
    for name in ("expected_world_epoch", "expected_generation"):
        value = args[name]
        if type(value) is not int or not 0 < value <= 18446744073709551615:
            raise RuntimeErrorDetail("invalid_args", "building UI epoch/generation requires positive uint64")
    for name in ("type", "variant"):
        value = args[name]
        if not isinstance(value, str) or not value or "\0" in value:
            raise RuntimeErrorDetail("invalid_args", "building UI requires nonempty type/variant identifiers")
        try:
            valid_bytes = len(value.encode("utf-8")) <= 256
        except UnicodeEncodeError as cause:
            raise RuntimeErrorDetail("invalid_args", "building UI " + name + " is not UTF-8") from cause
        if not valid_bytes:
            raise RuntimeErrorDetail("invalid_args", "building UI " + name + " exceeds 256 native bytes")
    for name in ("x", "y", "z"):
        value = args[name]
        if type(value) not in (int, float) or abs(value) > FLOAT32_MAX or not math.isfinite(value):
            raise RuntimeErrorDetail("invalid_args", "building UI coordinate requires finite float32")
