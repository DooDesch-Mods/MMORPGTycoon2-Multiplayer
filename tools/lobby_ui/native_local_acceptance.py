"""Native lobby readiness, button and observation helpers for two-peer acceptance tools."""
import sys
import time
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from local_acceptance import AcceptanceError, read_json

PEERS = ("host", "client-1")


def ready(value):
    prepare, controller = value["prepare"], value["controller"]
    if prepare.get("error") or controller.get("locked") or controller.get("cause"):
        raise AcceptanceError("native preparation failed: " + str(prepare.get("error") or controller.get("cause")))
    captures = prepare.get("captures", {})
    captures_ready = all(captures.get(name, {}).get("enabled") is True
                         for name in ("build", "remove", "move", "region", "terrain"))
    live = controller.get("live_state", {})
    live_ready = (live.get("active") is True and live.get("requested") is True
                  and live.get("locked") is False and not live.get("cause")
                  and (value.get("local_role") != "client" or live.get("confirmed") is True))
    return (value.get("transport") == "local" and value.get("admission_source") == "launcher_loopback"
            and value.get("admitted") is True and value.get("local_admitted") is True
            and value.get("local_peer_mask") == 3
            and prepare.get("ready") is True and prepare.get("playing") is True
            and prepare.get("captures_ready") is True and captures_ready and live_ready
            and controller.get("initialized") is True and controller.get("started") is True
            and value["snapshot"].get("world_authority_valid") is True
            and value["snapshot"].get("phase") == "active")


def left(value, previous):
    if value["prepare"].get("error"):
        raise AcceptanceError("native Leave failed: " + value["prepare"]["error"])
    confirmed = value["prepare"].get("leave_confirmed")
    if type(previous) is not int or type(confirmed) is not int:
        raise AcceptanceError("native Leave counter requires integers")
    return (value.get("title_active") is True and value.get("ui_mode") == 2
            and value.get("admitted") is False and value.get("local_admitted") is False
            and confirmed == previous + 1
            and value["prepare"].get("leaving") is False
            and value["controller"].get("initialized") is False)


def button(runner, peer, name, **extra):
    from runtime import FRAMEWORK_PLUGIN_KIND, manifest_runtime_kind
    if manifest_runtime_kind(read_json(runner.manifests[peer])) == FRAMEWORK_PLUGIN_KIND:
        from lobby_ui.framework_acceptance import button as framework_button
        return framework_button(runner, peer, name, **extra)
    token = str(uuid.uuid4())
    result = runner.single(name + "-" + peer, peer, "game_native_lobby_button",
                           {"acceptance_token": token, "button": name, **extra})
    for field in ("native_attempted", "native_returned", "handler_reached"):
        if result.get(field) is not True:
            raise AcceptanceError("native button receipt incomplete: " + peer + "/" + name + "/" + field)
    if (result.get("handler_failed") is not False or result.get("exception_code") != 0
            or result.get("acceptance_token") != token):
        raise AcceptanceError("native button handler/token differs: " + peer + "/" + name)
    runner.checked("native-button-" + name + "-" + peer, result)
    return result


def observe(runner, phase, predicate):
    deadline = time.monotonic() + runner.args.wait_seconds
    pending = set(PEERS)
    values = {}
    count = 0
    while pending:
        count += 1
        for peer in PEERS:
            if peer not in pending:
                continue
            if time.monotonic() >= deadline:
                raise AcceptanceError(phase + " timed out pending=" + ",".join(sorted(pending))
                                      + " required<" + str(runner.args.wait_seconds) + "s; no mutation replay")
            value = runner.single(phase + "-" + peer + "-" + str(count), peer, "game_read_native_lobby")
            values[peer] = value
            if predicate(peer, value):
                pending.remove(peer)
                runner.checked(phase + "-" + peer, value)
    return values
