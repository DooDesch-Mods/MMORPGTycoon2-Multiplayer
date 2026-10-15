"""Fixed native Developer reader and pre-Controller Host UI-command fixture."""
import re
from runtime import RuntimeErrorDetail

UUID = r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"
SCHEMAS = {
    "read_native_developers": {"type": "object", "properties": {}, "additionalProperties": False},
    "native_developer_fixture": {
        "type": "object",
        "properties": {
            "command": {"type": "string", "enum": ["Show", "Hire", "Fire"]},
            "token": {"type": "string", "pattern": "^" + UUID + "$"},
            "native_origin_uuid": {"type": "string", "pattern": "^" + UUID + "$"},
            "epoch": {"type": "integer", "minimum": 1, "maximum": 18446744073709551615},
            "generation": {"type": "integer", "minimum": 1, "maximum": 18446744073709551615},
            "expected_count": {"type": "integer", "minimum": 0, "maximum": 50},
        },
        "required": ["command", "token", "native_origin_uuid", "epoch", "generation", "expected_count"],
        "additionalProperties": False,
    },
}
DESCRIPTIONS = {
    "read_native_developers": "Read the actual fixed 50-Developer catalogue, persisted IDs, employment/active/map/job flags, manager membership counts, full ledger and retained fixture receipts on owned GameUpdate. Does not advance Developer simulation.",
    "native_developer_fixture": "Run one fixed original DevelopmentWindow Show or genuine Hire/Fire UICommand on isolated Host/Clock0 before Controller initialization. Requires exact native Origin and settled expected count. Acknowledges only the immediate original count/ledger effect, not final firing retirement or peer completion. Token consumed before native call; no replay.",
}


def validate(op, args):
    def bad(cause):
        raise RuntimeErrorDetail("invalid_args", cause)

    if not isinstance(args, dict):
        bad("Developer operation requires an exact argument object")
    if op == "read_native_developers":
        if args:
            bad("Developer read requires empty arguments")
        return
    required = {"command", "token", "native_origin_uuid", "epoch", "generation", "expected_count"}
    if op != "native_developer_fixture" or set(args) != required:
        bad("Developer fixture requires exact six fields")
    if not isinstance(args["command"], str) or args["command"] not in {"Show", "Hire", "Fire"}:
        bad("Developer fixture command must be Show/Hire/Fire")
    for field in ("token", "native_origin_uuid"):
        if not isinstance(args[field], str) or re.fullmatch(UUID, args[field]) is None:
            bad("Developer fixture requires canonical token/Origin UUID")
    for field in ("epoch", "generation"):
        if type(args[field]) is not int or not 1 <= args[field] <= 18446744073709551615:
            bad("Developer fixture Origin epoch/generation must be positive uint64")
    if type(args["expected_count"]) is not int or not 0 <= args["expected_count"] <= 50:
        bad("Developer fixture expected count must be exact integer0..50")
