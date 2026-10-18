"""Prove one Client Undo and one Host Redo of a confirmed native building placement."""
from __future__ import annotations

import argparse
import copy
from pathlib import Path
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools" / "mcp"))

from local_acceptance import (AcceptanceError, AcceptanceRunner, compare_ledgers, extract_result,
                              hash_file, log, read_json, ready_reason, utc, within)
from runtime import Runtime, FRAMEWORK_PLUGIN_KIND, manifest_runtime_kind, candidate_binary_receipt

PEERS = ("host", "client-1")
POSE = ("type", "variant", "x", "y", "z", "rotation", "scale")


FRAME_ID = ("session", "snapshot_uuid", "epoch", "revision", "base_action_revision", "time_revision")


def shared_ready(engines):
    frames = {}
    for peer in PEERS:
        engine = engines[peer]
        reason = ready_reason(engine, require_placement=False)
        if reason:
            raise AcceptanceError(f"{peer} native world is not ready: {reason}")
        controller = engine.get("typed_actions", {})
        if not isinstance(controller, dict):
            raise AcceptanceError(f"{peer} native Controller readback absent")
        live = controller.get("live_state", {})
        if not isinstance(live, dict):
            raise AcceptanceError(f"{peer} native Controller/LiveBridge readback absent")
        if (controller.get("initialized") is not True or controller.get("started") is not True
                or controller.get("pending") is not False or controller.get("locked") is not False
                or type(controller.get("failed")) is not int or controller["failed"] != 0
                or live.get("active") is not True or live.get("locked") is not False
                or (peer == "client-1" and live.get("confirmed") is not True) or live.get("cause")):
            raise AcceptanceError(f"{peer} Controller/LiveBridge unhealthy: failed={controller.get('failed')} "
                                  f"pending={controller.get('pending')} locked={controller.get('locked')} "
                                  f"live_confirmed={live.get('confirmed')} live_cause={live.get('cause')}")
        revision = controller.get("network_frame_revision")
        economy = controller.get("native_economy")
        day = economy.get("day") if isinstance(economy, dict) else None
        frame = day.get("frame", {}) if isinstance(day, dict) else {}
        if (type(revision) is not int or revision <= 0 or not isinstance(frame, dict)
                or type(frame.get("revision")) is not int or not 0 < frame["revision"] <= revision
                or any(key not in frame for key in FRAME_ID)):
            raise AcceptanceError(f"{peer} native gameplay Frame not adopted: network_revision={revision} "
                                  f"economy_frame_revision={frame.get('revision')}")
        frames[peer] = {key: frame[key] for key in FRAME_ID}
    if frames["host"] != frames["client-1"]:
        raise AcceptanceError(f"Host/Client adopted gameplay Frames differ: {frames}")
    return frames


def placement_evidence(path):
    path = within(path, ROOT / "artifacts/acceptance")
    summary = read_json(path)
    if summary.get("passed") is not True or summary.get("state") != "confirmed-normal-building-UI-two":
        raise AcceptanceError(f"placement summary is not confirmed: {path}")
    try:
        action = str(uuid.UUID(summary["action_uuid"]))
    except (KeyError, ValueError, TypeError) as error:
        raise AcceptanceError("placement original ActionUUID missing") from error
    matches = [check.get("result") for check in summary.get("checks", [])
               if check.get("check") == "normal-UI-building-confirmed"]
    if len(matches) != 1 or not isinstance(matches[0], dict):
        raise AcceptanceError("placement summary lacks one normal native building proof")
    proof = matches[0]
    if any(peer not in proof.get("objects", {}) for peer in PEERS):
        raise AcceptanceError("placement native object readback missing a peer")
    if proof.get("receipt", {}).get("action_uuid") != action:
        raise AcceptanceError("placement UI receipt ActionUUID differs")
    return summary, action, proof


def preflight(runner, placement_path, freeze, runtime_factory=Runtime, candidate_binary=None):
    placed, action, proof = placement_evidence(placement_path)
    frozen = read_json(freeze)
    expected_dll = frozen.get("dll_sha256")
    if not isinstance(expected_dll, str) or len(expected_dll) != 64:
        raise AcceptanceError("candidate freeze DLL SHA256 missing")
    runner.load_manifests()
    if runner.summary["runtime_run_id"] != placed.get("runtime_run_id"):
        raise AcceptanceError("placement and current owned runtime differ")
    identities = {}
    for peer in PEERS:
        manifest = runner.manifests[peer]
        bound = runtime_factory(runner.root).bind(str(manifest))
        if bound.get("process_identity_verified") is not True:
            raise AcceptanceError(f"{peer} PID/creation identity unverified")
        identity = read_json(manifest)
        if manifest_runtime_kind(identity) == FRAMEWORK_PLUGIN_KIND or candidate_binary is not None:
            receipt = candidate_binary_receipt(identity, candidate_binary)
            actual = receipt['sha256']
            if actual != expected_dll:
                raise AcceptanceError(f"{peer} selected DLL differs from candidate freeze")
            if manifest_runtime_kind(identity) != FRAMEWORK_PLUGIN_KIND:
                if hash_file(Path(identity['runtime_root']) / 'game' / 'MT2_Multiplayer.dll') != actual:
                    raise AcceptanceError(f"{peer} installed DLL differs from explicit candidate")
            runner.checked('selected-candidate-' + peer, receipt)
        else:
            installed = identity.get("installed_binary_sha256", {}).get("MT2_Multiplayer.dll")
            actual = hash_file(Path(identity["runtime_root"]) / "game" / "MT2_Multiplayer.dll")
            if installed != expected_dll or actual != expected_dll:
                raise AcceptanceError(f"{peer} exact installed DLL differs from candidate freeze")
        identities[peer] = {"pid": identity["pid"], "process_start_time": identity["process_start_time"],
                            "dll_sha256": actual}
    runner.checked("placement-and-exact-runtime", {"placement": str(placement_path),
                   "placement_sha256": hash_file(placement_path), "action_uuid": action, "peers": identities})
    return action, proof


def snapshot(runner, label, peer, remaining=None, include_bindings=True):
    tools = ["game_inspect_engine", "game_read_building_history_ui"]
    if include_bindings:
        tools.append("game_read_remove_bindings")
    calls = [{"tool": tool, "arguments": {}} for tool in
             tools]
    receipt = runner.job(label + "-" + peer, peer, calls, remaining=remaining)
    result = {"engine": extract_result(receipt, "game_inspect_engine"),
              "ui": extract_result(receipt, "game_read_building_history_ui")}
    if include_bindings:
        result["bindings"] = extract_result(receipt, "game_read_remove_bindings")
    return result


def pair(runner, label, remaining=None, include_bindings=True):
    return {peer: snapshot(runner, label, peer, remaining, include_bindings) for peer in PEERS}


def row(snapshot_value, action):
    key = "action:" + action
    matches = [entry for entry in snapshot_value["bindings"].get("bindings", [])
               if entry.get("record_key") == key]
    if len(matches) != 1:
        raise AcceptanceError(f"native building binding requires exactly one {key}, actual={len(matches)}")
    return matches[0]


def full_pose(actual, original, peer, phase):
    for name in ("local_uid",) + POSE:
        if name not in original or actual.get(name) != original[name]:
            raise AcceptanceError(f"{peer} {phase} original UID/full pose differs field={name}")
    if actual.get("unknown") is not False:
        raise AcceptanceError(f"{peer} {phase} binding Unknown")


def undo_ledger(placed, cost):
    if type(cost) is not int or cost <= 0:
        raise AcceptanceError("placement cost missing or invalid")
    expected = copy.deepcopy(placed)
    if len(expected.get("line_counts", [])) != 20 or len(expected.get("line_income", [])) != 20:
        raise AcceptanceError("placement ledger lacks full 20 categories")
    expected["cash"] += cost
    expected["record_cash"] += cost
    # Native DoCash adds a compensating income entry; it preserves the purchase expense.
    expected["record_income"] += cost
    expected["line_counts"][6] += 1
    expected["line_income"][6] += cost
    return expected


def redo_ledger(undone, cost):
    expected = copy.deepcopy(undone)
    expected["cash"] -= cost
    expected["record_cash"] -= cost
    expected["record_income"] -= cost
    expected["line_counts"][6] += 1
    expected["line_income"][6] -= cost
    return expected


def require_initial(snapshots, action, proof):
    engines = {peer: snapshots[peer]["engine"] for peer in PEERS}
    frames = shared_ready(engines)
    ledger = compare_ledgers(engines["host"], engines["client-1"])
    if ledger != proof["ledger"]:
        raise AcceptanceError("native ledger changed after placement; no history mutation attempted")
    revisions = set()
    confirms = set()
    for peer in PEERS:
        engine, ui = snapshots[peer]["engine"], snapshots[peer]["ui"]
        controller = engine["typed_actions"]
        if engine.get("capabilities", {}).get("native_building_history_ui") is not True:
            raise AcceptanceError(f"{peer} building history native UI capability absent")
        if ui.get("snapshot_uuid") != frames[peer]["snapshot_uuid"]:
            raise AcceptanceError(f"{peer} history UI/shared world snapshot differs")
        if ui.get("undo_target_action_uuid") != action or ui.get("active_graph") is not False:
            raise AcceptanceError(f"{peer} original building graph is not native Undo top")
        if (controller.get("action_uuid") != action or controller.get("outcome") != 1
                or controller.get("pending") is not False or controller.get("locked") is not False
                or controller.get("failed") != 0):
            raise AcceptanceError(f"{peer} placement Controller not confirmed and idle")
        native = row(snapshots[peer], action)
        full_pose(native, proof["objects"][peer], peer, "placed")
        if native.get("revoked") is not False:
            raise AcceptanceError(f"{peer} original building is not live")
        revisions.add(controller.get("host_revision"))
        confirms.add(controller.get("confirmed"))
    if len(revisions) != 1 or len(confirms) != 1:
        raise AcceptanceError("Host/Client starting Controller revision or confirmation differs")
    return ledger, revisions.pop(), confirms.pop()


def phase_complete(snapshots, before, action, original, direction, expected_ledger,
                   revision, confirmed, source):
    engines = {peer: snapshots[peer]["engine"] for peer in PEERS}
    shared_ready(engines)
    ledger = compare_ledgers(engines["host"], engines["client-1"])
    if ledger != expected_ledger:
        raise AcceptanceError(f"{direction} full 20-category ledger differs from expected effect")
    for peer in PEERS:
        controller = engines[peer]["typed_actions"]
        ui = snapshots[peer]["ui"]
        earlier = before[peer]["ui"]
        if (controller.get("action_uuid") != action or controller.get("outcome") != 1
                or controller.get("host_revision") != revision + 1
                or controller.get("confirmed") != confirmed + 1 or controller.get("failed") != 0
                or controller.get("attempted") != before[peer]["engine"]["typed_actions"].get("attempted", -1) + 1
                or controller.get("pending") is not False or controller.get("locked") is not False):
            raise AcceptanceError(f"{peer} {direction} exact Controller completion missing")
        if peer == "host" and controller.get("ack_mask") != 3:
            raise AcceptanceError(f"Host {direction} lacks both native ACKs")
        increment = 1 if peer == source else 0
        if (ui.get("ui_hook_calls") != earlier.get("ui_hook_calls", -1) + increment
                or ui.get("ui_intents_queued") != earlier.get("ui_intents_queued", -1) + increment):
            raise AcceptanceError(f"{peer} {direction} UI hook/source count differs")
        if peer == source and (ui.get("last_action_uuid") != action or
                               ui.get("last_target_action_uuid") != original or
                               ui.get("last_direction") != direction):
            raise AcceptanceError(f"{peer} {direction} exact UI intent receipt differs")
        if ui.get("snapshot_uuid") != earlier.get("snapshot_uuid") or ui.get("active_graph") is not False:
            raise AcceptanceError(f"{peer} {direction} native stack owner/active graph differs")
        if (ui.get("undo_depth") != earlier.get("undo_depth") + (-1 if direction == "undo" else 1)
                or ui.get("redo_depth") != earlier.get("redo_depth") + (1 if direction == "undo" else -1)):
            raise AcceptanceError(f"{peer} {direction} native Undo/Redo stack transition differs")
        top = "redo_target_action_uuid" if direction == "undo" else "undo_target_action_uuid"
        if ui.get(top) != original:
            raise AcceptanceError(f"{peer} {direction} native inverse graph target differs")
        native = row(snapshots[peer], original)
        full_pose(native, before[peer]["binding"], peer, direction)
        if native.get("revoked") is not (direction == "undo"):
            raise AcceptanceError(f"{peer} {direction} native binding tombstone differs")
    return ledger


def wait_complete(runner, before, action, original, direction, expected_ledger,
                  revision, confirmed, source, wait_seconds, now=time.monotonic):
    deadline, poll = now() + wait_seconds, 0
    while True:
        remaining = deadline - now()
        if remaining <= 2:
            raise AcceptanceError(f"{direction} observation timeout action={action}; mutation already attempted, no replay")
        poll += 1
        current = pair(runner, f"{direction}-observe-{poll}", remaining, include_bindings=False)
        for peer in PEERS:
            status = current[peer]["engine"].get("typed_actions", {})
            if status.get("failed") != 0 or status.get("locked") is True:
                raise AcceptanceError(f"{peer} {direction} Controller failed/Unknown: {status.get('cause')}")
        if all(current[peer]["engine"]["typed_actions"].get("action_uuid") == action
               and current[peer]["engine"]["typed_actions"].get("confirmed") == confirmed + 1
               and current[peer]["engine"]["typed_actions"].get("pending") is False for peer in PEERS):
            # Native deletion can precede registry retirement. Read bindings only after both ACKs settle.
            for peer in PEERS:
                current[peer]["bindings"] = runner.single(
                    f"{direction}-completed-bindings-{peer}", peer, "game_read_remove_bindings", {})
            ledger = phase_complete(current, before, action, original, direction, expected_ledger,
                                    revision, confirmed, source)
            runner.checked(direction + "-native-two-peer-confirmed", {
                "action_uuid": action, "target_action_uuid": original, "host_revision": revision + 1,
                "host_ack_mask": 3, "confirmed_delta_each": 1, "ledger": ledger,
                "bindings": {peer: row(current[peer], original) for peer in PEERS},
                "ui": {peer: current[peer]["ui"] for peer in PEERS}})
            return current


def mutate_once(runner, before, original, direction, source, expected_ledger,
                revision, confirmed, wait_seconds, now=time.monotonic):
    for peer in PEERS:
        before[peer]["binding"] = row(before[peer], original)
    args = {"direction": direction, "snapshot_uuid": before[source]["ui"]["snapshot_uuid"],
            "expected_host_revision": revision, "target_action_uuid": original}
    runner.checked(direction + "-immutable-native-UI-request", args)
    admitted = runner.single(direction + "-native-entry-once", source,
                             "game_native_building_history_ui", args)
    try:
        action = str(uuid.UUID(admitted["action_uuid"]))
    except (KeyError, ValueError, TypeError) as error:
        raise AcceptanceError(f"{direction} native UI ActionUUID absent; no replay") from error
    if (action == original or admitted.get("state") != "queued" or
            admitted.get("target_action_uuid") != original or admitted.get("direction") != direction or
            admitted.get("ui_hook_calls") != before[source]["ui"].get("ui_hook_calls", -1) + 1 or
            admitted.get("ui_intents_queued") != before[source]["ui"].get("ui_intents_queued", -1) + 1):
        raise AcceptanceError(f"{direction} native UI entry not exactly queued; no replay")
    runner.checked(direction + "-native-UI-intent-queued", admitted)
    return action, wait_complete(runner, before, action, original, direction, expected_ledger,
                                 revision, confirmed, source, wait_seconds, now)


def resume_initial(runner, path, placement):
    path = within(path, ROOT / "artifacts/acceptance")
    summary = read_json(path)
    if (summary.get("runtime_run_id") != runner.summary["runtime_run_id"]
            or summary.get("state") != "failed-native-building-undo-redo-two"
            or "undo full 20-category ledger differs" not in summary.get("error", {}).get("cause", "")
            or Path(summary.get("parameters", {}).get("placement", "")).resolve() != placement.resolve()):
        raise AcceptanceError("retained Undo is not the same runtime/placement ledger-oracle failure")
    jobs = {job["step"]: job for job in summary["jobs"]}
    initial = {}
    for peer in PEERS:
        job = jobs["history-before-" + peer]
        evidence = within(job["evidence_path"], ROOT / "test/mcp/evidence")
        if hash_file(evidence) != job["evidence_sha256"]:
            raise AcceptanceError("retained initial readback hash differs: " + peer)
        receipt = read_json(evidence)
        initial[peer] = {"engine": extract_result(receipt, "game_inspect_engine"),
                         "ui": extract_result(receipt, "game_read_building_history_ui"),
                         "bindings": extract_result(receipt, "game_read_remove_bindings")}
    runner.checked("resume-retained-undo-no-replay", {"path": str(path), "sha256": hash_file(path)})
    return initial


def execute(args, runtime_factory=Runtime):
    args.test_root = getattr(args, "test_root", ROOT / "_MPTest")
    args.players, args.cleanup, args.step_timeout = 2, False, 45
    runner = AcceptanceRunner(args)
    runner.summary["scope"] = "one confirmed Client native Undo and Host native Redo of one existing building; no placement replay"
    try:
        if not 2 < args.wait_seconds <= 60:
            raise AcceptanceError("--wait-seconds must be >2 and <=60")
        original, proof = preflight(runner, args.placement, args.freeze, runtime_factory,
                                    candidate_binary=getattr(args, 'candidate_binary', None))
        resume = getattr(args, "resume_undo", None)
        initial = resume_initial(runner, resume, args.placement) if resume else pair(runner, "history-before")
        placed_ledger, revision, confirmed = require_initial(initial, original, proof)
        cost = initial["host"]["engine"]["ui_build"]["receipts"][original]["cost"]
        if cost != initial["client-1"]["engine"]["ui_build"]["receipts"][original]["cost"]:
            raise AcceptanceError("original building Host/Client cost differs")
        undone_ledger = undo_ledger(placed_ledger, cost)
        runner.checked("history-start", {"original_action_uuid": original, "host_revision": revision,
                                        "confirmed_each": confirmed, "cost": cost})
        if resume:
            for peer in PEERS:
                initial[peer]["binding"] = row(initial[peer], original)
            undone = pair(runner, "retained-undo-current")
            undo_action = undone["host"]["engine"]["typed_actions"]["action_uuid"]
            phase_complete(undone, initial, undo_action, original, "undo", undone_ledger,
                           revision, confirmed, "client-1")
            runner.checked("undo-native-two-peer-confirmed", {"action_uuid": undo_action,
                "target_action_uuid": original, "ledger": undone_ledger, "retained_mutation": True})
        else:
            undo_action, undone = mutate_once(runner, initial, original, "undo", "client-1",
                                             undone_ledger, revision, confirmed, args.wait_seconds)
        redo_action, restored = mutate_once(runner, undone, original, "redo", "host",
                                           redo_ledger(undone_ledger, cost), revision + 1, confirmed + 1, args.wait_seconds)
        if undo_action == redo_action:
            raise AcceptanceError("Undo and Redo ActionUUIDs are identical")
        runner.summary.update(passed=True, state="confirmed-native-building-undo-redo-two",
                              original_action_uuid=original, undo_action_uuid=undo_action,
                              redo_action_uuid=redo_action)
        runner.checked("history-final-restored", {"ledger": compare_ledgers(
            restored["host"]["engine"], restored["client-1"]["engine"]),
            "original_action_uuid": original, "undo_action_uuid": undo_action,
            "redo_action_uuid": redo_action})
    except Exception as error:
        runner.summary.update(passed=False, state="failed-native-building-undo-redo-two",
                              error={"step": runner.summary["step"], "cause": f"{type(error).__name__}: {error}",
                                     "behavior": "remaining mutations skipped; no native UI replay"})
        log("error", str(error))
    runner.summary["finished_utc"] = utc()
    runner.persist()
    print(runner.summary_path)
    return 0 if runner.summary["passed"] else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--placement", type=Path, required=True, help="passed placement_ui_acceptance summary.json")
    parser.add_argument("--test-root", type=Path, default=ROOT / "_MPTest")
    parser.add_argument("--freeze", type=Path, required=True, help="current candidate freeze containing dll_sha256")
    parser.add_argument("--candidate-binary", type=Path, help="Exact selected mod artifact; required for Framework plugin mode")
    parser.add_argument("--wait-seconds", type=float, default=45)
    parser.add_argument("--resume-undo", type=Path, help="retained confirmed Undo stopped by ledger-oracle error; never replay Undo")
    return execute(parser.parse_args(argv))


if __name__ == "__main__":
    raise SystemExit(main())
