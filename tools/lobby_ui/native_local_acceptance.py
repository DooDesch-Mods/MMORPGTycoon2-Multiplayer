"""Check native two-peer lobby start, leave and rehost without mutation retries."""
import argparse
import sys
import time
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from local_acceptance import AcceptanceError, AcceptanceRunner, ROOT, compare_ledgers, log, read_json, utc

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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--freeze", type=Path, required=True)
    parser.add_argument("--expected-run-id", required=True)
    parser.add_argument("--world", default="MPTestSession")
    parser.add_argument("--current-world", action="store_true", help="Create a Host world after lobby admission and start from the open world")
    parser.add_argument("--wait-seconds", type=int, choices=range(5, 61), default=45)
    args = parser.parse_args()
    args.test_root, args.players, args.cleanup, args.step_timeout = ROOT / "_MPTest", 2, False, 45
    args.ready_timeout = args.wait_seconds
    runner = AcceptanceRunner(args)
    runner.summary["scope"] = ("native local Host/Join, create current Host world, automatic selection and Start, both Ready; no closed Leave/Rehost or action replay"
                               if args.current_world else "native local Host/Join/Pick/Start, both Ready, Leave and rehost with a new snapshot; no world-action replay")
    try:
        frozen = read_json(args.freeze)
        runner.load_manifests()
        if runner.summary["runtime_run_id"] != args.expected_run_id:
            raise AcceptanceError("owned run differs; no native button attempted")
        for peer in PEERS:
            manifest = read_json(runner.manifests[peer])
            if manifest["installed_binary_sha256"]["MT2_Multiplayer.dll"] != frozen["dll_sha256"]:
                raise AcceptanceError("candidate DLL differs on " + peer + "; no native button attempted")
            runner.poll_title(peer)
        snapshots = []
        for cycle in range(1 if args.current_world else 2):
            runner.checked("cycle", {"index": cycle + 1})
            for peer in PEERS:
                button(runner, peer, "open")
            button(runner, "host", "host")
            button(runner, "client-1", "join")
            if args.current_world:
                created = runner.single("create-current-host-world", "host", "game_create_test_world")
                name = created.get("name", "")
                if created.get("state") != "world_active" or not name.startswith("MPTest"):
                    raise AcceptanceError("native new Host world acknowledgement incomplete")
                selected = runner.single("current-host-world-selected", "host", "game_read_native_lobby")
                start_pane = next((pane for pane in selected.get("native_panes", []) if pane.get("id") == "start"), {})
                if (selected.get("current_host_world") is not True or selected.get("selected_world") != name
                        or selected.get("selected_file") != "" or selected.get("title_active") is not False
                        or selected.get("host_permission") is not True or start_pane.get("disabled") is not False):
                    raise AcceptanceError("new open Host world was not selected automatically with Start enabled")
                runner.checked("current-host-world-selected", {"world": name, "selection": selected})
            else:
                button(runner, "host", "pick", world=args.world)
            button(runner, "host", "start")
            values = observe(runner, "ready", lambda peer, value: ready(value))
            for index, peer in enumerate(PEERS, 1):
                value = values[peer]
                if value.get("local_peer") != index or value.get("local_role") != ("host" if index == 1 else "client"):
                    raise AcceptanceError("local admission role/peer differs: " + peer)
                if value.get("host_permission") is not (index == 1):
                    raise AcceptanceError("local Host permission differs: " + peer)
            snapshot = values["host"]["prepare"]["snapshot_id"]
            uuid.UUID(snapshot)
            if values["client-1"]["prepare"]["snapshot_id"] != snapshot or snapshot in snapshots:
                raise AcceptanceError("local peers lack one new shared snapshot identity")
            snapshots.append(snapshot)
            if args.current_world:
                if values["host"].get("selected_world") != name or values["host"]["prepare"].get("source") != "current_world":
                    raise AcceptanceError("common snapshot preparation replaced the open Host source")
                runner.summary["current_world"] = name
                engines = {peer: runner.inspect("current-world-native-values-" + peer, peer) for peer in PEERS}
                ledger = compare_ledgers(engines["host"], engines["client-1"])
                if ledger["clock_day"] <= 0 or engines["host"]["clock"] != engines["client-1"]["clock"]:
                    raise AcceptanceError("current Host world native calendar is not normalized and equal on both peers")
                histories, calendars = {}, {}
                for peer, engine in engines.items():
                    typed = engine.get("typed_actions", {})
                    if typed.get("native_economy_read_error"):
                        raise AcceptanceError("actual native History read failed on " + peer + ": " + typed["native_economy_read_error"])
                    day = typed.get("native_economy", {}).get("day", {})
                    histories[peer] = day.get("history")
                    calendars[peer] = day.get("frame", {}).get("values", {}).get("calendar")
                    if not isinstance(histories[peer], list) or not histories[peer] or not isinstance(calendars[peer], list) or len(calendars[peer]) != 6:
                        raise AcceptanceError("actual native calendar/History readback absent on " + peer)
                if histories["host"] != histories["client-1"] or calendars["host"] != calendars["client-1"]:
                    raise AcceptanceError("current Host world native full History/calendar differs between peers")
                runner.checked("current-world-native-calendar-ledger-history", {"calendar": calendars["host"], "ledger": ledger, "history": histories["host"]})
            for peer in PEERS:
                frame = runner.single("ready-frame-" + peer, peer, "game_capture_frame")
                runner.checked("ready-frame-" + peer, frame)
            if cycle == 0 and not args.current_world:
                previous = {peer: values[peer]["prepare"]["leave_confirmed"] for peer in PEERS}
                for peer in reversed(PEERS):
                    button(runner, peer, "leave")
                observe(runner, "left", lambda peer, value: left(value, previous[peer]))
        runner.summary.update(passed=True, state="native-current-world-session-confirmed" if args.current_world else "native-local-lobby-lifecycle-confirmed", snapshots=snapshots)
    except Exception as cause:
        runner.summary.update(passed=False, state="failed-native-local-lobby", error={
            "step": runner.summary["step"], "cause": str(cause), "behavior": "remaining mutations skipped; effects retained; no replay"})
        log("error", "native local lobby: " + str(cause))
    runner.summary["finished_utc"] = utc()
    runner.persist()
    print(runner.summary_path)
    return 0 if runner.summary["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
