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
SCHEMAS["native_developer_ui"] = SCHEMAS["native_developer_fixture"]
DESCRIPTIONS = {
    "read_native_developers": "Read the actual fixed 50-Developer catalogue, persisted IDs, employment/active/map/job flags, manager membership counts, full ledger and retained fixture receipts on owned GameUpdate. Does not advance Developer simulation.",
    "native_developer_ui": "Run one fixed original DevelopmentWindow Show or HOOKED Hire/Fire UICommand on an owned live local-test peer with the actual idle Controller and exact Origin/count. Hire/Fire returns exactly one captured action UUID before native gameplay effects. This is admission only, never execution or peer ACK. Token consumed before the UI entry; no replay.",
    "native_developer_fixture": "Run one fixed original DevelopmentWindow Show or genuine Hire/Fire UICommand on isolated Host/Clock0 before Controller initialization. Requires exact native Origin and settled expected count. Acknowledges only the immediate original count/ledger effect, not final firing retirement or peer completion. Token consumed before native call; no replay.",
}


SCHEMAS["read_native_developer_retirement"] = SCHEMAS["read_native_developers"]
SCHEMAS["native_developer_retirement_fixture"] = {
    "type": "object", "properties": {
        **{k: v for k, v in SCHEMAS["native_developer_fixture"]["properties"].items() if k != "command"},
        "slot": {"type": "integer", "minimum": 0, "maximum": 49},
        "id": {"type": "integer", "minimum": 0, "maximum": 49},
    },
    "required": ["token", "native_origin_uuid", "epoch", "generation", "expected_count", "slot", "id"],
    "additionalProperties": False,
}
DESCRIPTIONS["read_native_developer_retirement"] = "Read actual fixed50 Developer closure details and retained retirement receipts on isolated Host/Clock0 before Controller initialization. No mutation or lock reset."
DESCRIPTIONS["native_developer_retirement_fixture"] = "Run one fixed original active-property/arrival closure for an actual fired-active no-job/no-carried Developer on isolated Host/Clock0 before Controller initialization. Requires exact actual Origin/count/slot/persisted ID. Diagnostic fixture only; no peer ACK. Token consumed before native call; no replay."


def validate(op, args):
    def bad(cause):
        raise RuntimeErrorDetail("invalid_args", cause)

    if not isinstance(args, dict):
        bad("Developer operation requires an exact argument object")
    if op in {"read_native_developers", "read_native_developer_retirement"}:
        if args:
            bad("Developer read requires empty arguments")
        return
    if op == "native_developer_retirement_fixture":
        required = {"token", "native_origin_uuid", "epoch", "generation", "expected_count", "slot", "id"}
        if set(args) != required:
            bad("Developer retirement fixture requires exact seven fields")
        validate("native_developer_fixture", {**{k: args[k] for k in required - {"slot", "id"}}, "command": "Fire"})
        for key in ("slot", "id"):
            if type(args[key]) is not int or not 0 <= args[key] <= 49:
                bad("Developer retirement slot/persisted ID must be exact int0..49")
        return
    required = {"command", "token", "native_origin_uuid", "epoch", "generation", "expected_count"}
    if op not in {"native_developer_fixture", "native_developer_ui"} or set(args) != required:
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
