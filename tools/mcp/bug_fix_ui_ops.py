"""Fixed original Bug detail controls for owned local acceptance runs."""
import re
from runtime import RuntimeErrorDetail

UUID = r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"
UINT64 = {"type": "integer", "minimum": 1, "maximum": 18446744073709551615}
SCHEMAS = {
    "read_native_bug_fix_ui": {"type": "object", "properties": {}, "additionalProperties": False},
    "native_bug_fix_ui": {
        "type": "object",
        "properties": {
            "command": {"type": "string", "enum": ["Show", "Select", "Click"]},
            "token": {"type": "string", "pattern": "^" + UUID + "$"},
            "native_origin_uuid": {"type": "string", "pattern": "^" + UUID + "$"},
            "epoch": UINT64, "generation": UINT64,
            "bug": {"type": "array", "minItems": 6, "maxItems": 6,
                    "items": {"type": ["integer", "string"]}},
            "schedule": {"type": "boolean"},
        },
        "required": ["command", "token", "native_origin_uuid", "epoch", "generation", "bug", "schedule"],
        "additionalProperties": False,
    },
}
DESCRIPTIONS = {
    "read_native_bug_fix_ui": "Read actual known witnessed Bugs, original detail-control readiness and retained UI receipts in an owned local world. Does not change Bug discovery or jobs.",
    "native_bug_fix_ui": "On an owned local paused world, Show the original Bugs page, Select a known witnessed Bug, or Click its verified selected BugButton through original Hover/Press/Release. Requires exact Origin, stable Bug identity and expected Schedule/Cancel state. Use one token per attempt; inspect retained capture UUID and Controller ACK separately. Never replay an attempted token. No direct job or Bug-known setter.",
}


def validate(op, args):
    def bad(cause):
        raise RuntimeErrorDetail("invalid_args", cause)
    if not isinstance(args, dict):
        bad("Bug Fix UI requires an exact argument object")
    if op == "read_native_bug_fix_ui":
        if args:
            bad("Bug Fix UI read requires empty arguments")
        return
    if op != "native_bug_fix_ui" or set(args) != set(SCHEMAS[op]["required"]):
        bad("Bug Fix UI requires exact seven fields")
    if args["command"] not in ("Show", "Select", "Click"):
        bad("Bug Fix UI command must be Show, Select or Click")
    for name in ("token", "native_origin_uuid"):
        if not isinstance(args[name], str) or re.fullmatch(UUID, args[name]) is None:
            bad("Bug Fix UI requires canonical token/Origin UUID")
    for name in ("epoch", "generation"):
        if type(args[name]) is not int or not 1 <= args[name] <= 18446744073709551615:
            bad("Bug Fix UI requires positive exact uint64 epoch/generation")
    bug = args["bug"]
    if not isinstance(bug, list) or len(bug) != 6:
        bad("Bug Fix UI requires exact six-field Bug identity")
    if type(bug[0]) is not int or bug[0] not in (0, 1) or type(bug[2]) is not int or bug[2] not in (0, 1, 2):
        bad("Bug Fix UI parent/birth kind is invalid")
    if not isinstance(bug[1], str) or not 1 <= len(bug[1]) <= 512:
        bad("Bug Fix UI requires bounded parent key; native code validates its canonical identity")
    pattern = r"[0-9a-f]{64}" if bug[2] == 0 else UUID
    if not isinstance(bug[3], str) or re.fullmatch(pattern, bug[3]) is None:
        bad("Bug Fix UI birth source differs from its kind")
    if any(type(value) is not int or not 0 <= value <= 18446744073709551615 for value in bug[4:]):
        bad("Bug Fix UI sequence/occurrence requires exact uint64")
    if (bug[2] in (0, 1) and bug[4] != 0) or (bug[2] == 2 and bug[4] == 0):
        bad("Bug Fix UI birth sequence differs from its kind")
    if type(args["schedule"]) is not bool:
        bad("Bug Fix UI schedule requires exact boolean")
