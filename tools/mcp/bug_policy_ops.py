"""Fixed original Host Bug-C policy checkbox UI, not a Bug or job setter."""
import re
from runtime import RuntimeErrorDetail

UUID=r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"
SCHEMAS={
    "read_bug_c_policy":{"type":"object","properties":{},"additionalProperties":False},
    "native_bug_c_policy":{"type":"object","properties":{
        "command":{"type":"string","enum":["Show","Toggle"]},
        "scope":{"type":"string","enum":["online","offline"]},
        "token":{"type":"string","pattern":"^"+UUID+"$"},
        "native_origin_uuid":{"type":"string","pattern":"^"+UUID+"$"},
        "epoch":{"type":"integer","minimum":1,"maximum":18446744073709551615},
        "generation":{"type":"integer","minimum":1,"maximum":18446744073709551615},
        "expected":{"type":"boolean"}},
        "required":["command","scope","token","native_origin_uuid","epoch","generation","expected"],"additionalProperties":False},
}
DESCRIPTIONS={
    "read_bug_c_policy":"Read actual owned paused Host Bug policies/server state, policy-control readiness and retained UI receipts. Does not discover Bugs, create jobs or change policy. UI ready=false retains its exact cause.",
    "native_bug_c_policy":"On an owned live local Host/Clock0, Show the genuine Development Policies page or Toggle the uniquely bound Bug-C online/offline checkbox via original Hover/Press/Release once. Requires exact Origin/expected bool and idle Controller. Inspect attempted/returned/confirmed/unknown and cause; never replay an attempted token. No Bug-known setter, MakeJob or peer ACK claim.",
}
def validate(op,args):
    def bad(cause):raise RuntimeErrorDetail("invalid_args",cause)
    if not isinstance(args,dict):bad("Bug-C policy requires an exact argument object")
    if op=="read_bug_c_policy":
        if args:bad("Bug-C policy read requires empty arguments")
        return
    if op!="native_bug_c_policy" or set(args)!={"command","scope","token","native_origin_uuid","epoch","generation","expected"}:bad("Bug-C policy requires exact seven fields")
    if args["command"] not in ("Show","Toggle"):bad("Bug-C policy command must be Show or Toggle")
    if args["scope"] not in ("online","offline"):bad("Bug-C policy scope must be online or offline")
    for name in ("token","native_origin_uuid"):
        if not isinstance(args[name],str) or re.fullmatch(UUID,args[name]) is None:bad("Bug-C policy requires canonical token/Origin UUID")
    for name in ("epoch","generation"):
        if type(args[name]) is not int or not 1<=args[name]<=18446744073709551615:bad("Bug-C policy requires positive exact uint64 epoch/generation")
    if type(args["expected"]) is not bool:bad("Bug-C policy expected requires exact boolean")
