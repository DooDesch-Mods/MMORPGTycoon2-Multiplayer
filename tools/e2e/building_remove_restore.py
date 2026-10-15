"""Prove one Client Undo of Remove, then one Host Redo, without replaying Remove."""
from __future__ import annotations

import argparse
import copy
import math
from pathlib import Path
import sys
import struct
import time
import uuid
import zlib

ROOT = next(path for path in Path(__file__).resolve().parents if (path / "tools/local_acceptance.py").is_file())
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools/mcp"))
from tools.e2e.building_history import PEERS, full_pose, pair, redo_ledger, shared_ready, undo_ledger
from local_acceptance import (AcceptanceError, AcceptanceRunner, compare_ledgers, extract_result,
                              hash_file, log, read_json, utc, within)
from runtime import Runtime



class RecordReader:
    def __init__(self, raw):
        self.raw = raw
        self.offset = 0
        self.nodes = []

    def value(self, format):
        size = struct.calcsize(format)
        if self.offset + size > len(self.raw):
            raise ValueError(f"Truncated record at offset {self.offset}")
        value = struct.unpack_from(format, self.raw, self.offset)[0]
        self.offset += size
        return value

    def string(self):
        size = self.value(">H")
        if self.offset + size > len(self.raw):
            raise ValueError(f"Truncated string offset={self.offset} size={size}")
        text = self.raw[self.offset:self.offset + size].decode("utf-8")
        self.offset += size
        return text

    def token(self):
        kind = self.value(">B")
        if kind in (1, 2):
            return self.string()
        if kind == 3:
            return self.value("<f")
        if kind == 4:
            return self.value(">i")
        if kind == 0:
            return None
        raise ValueError(f"Unexpected token kind={kind} offset={self.offset - 1}")

    def record(self, parent="$", index=0, depth=0, owner=None):
        if depth > 256 or len(self.nodes) >= 200000:
            raise ValueError(f"Record structure limit depth={depth} nodes={len(self.nodes)}")
        offset = self.offset
        label = self.token()
        token_count = self.value(">I")
        if token_count > 65536:
            raise ValueError(f"Record token count exceeds 65536: {token_count}")
        tokens = [self.token() for _ in range(token_count)]
        child_count = self.value(">I")
        if child_count > 200000:
            raise ValueError(f"Record child count exceeds 200000: {child_count}")
        path = f"{parent}/{label}[{index}]"
        ordinal = len(self.nodes)
        if isinstance(label, str) and label.startswith("mmo"):
            owner = ordinal
        node = {"label": label, "offset": offset, "path": path, "ordinal": ordinal,
                "owner_ordinal": owner, "tokens": tokens, "children": []}
        self.nodes.append(node)
        node["children"] = [self.record(path, number, depth + 1, owner) for number in range(child_count)]
        return node


def canonical_uuid(value, field):
    try:
        parsed = str(uuid.UUID(value))
    except (TypeError, ValueError, AttributeError) as error:
        raise AcceptanceError(f"{field} is not a canonical UUID") from error
    if parsed != value:
        raise AcceptanceError(f"{field} is not a canonical lowercase UUID")
    return value


def integer(value, field, minimum=0, maximum=2**63 - 1):
    if type(value) is not int or not minimum <= value <= maximum:
        raise AcceptanceError(f"{field} requires an exact integer in {minimum}..{maximum}; actual={value!r}")
    return value


def native_ledger(value):
    if not isinstance(value, dict) or type(value.get("record_present")) is not int or value["record_present"] != 1:
        raise AcceptanceError("native Developer ledger requires the actual current record")
    for field in ("cash", "flat_day", "record_day", "record_cash", "record_income", "record_expense"):
        integer(value.get(field), "native ledger " + field, -(2**31), 2**31 - 1)
    for field in ("line_counts", "line_income"):
        group = value.get(field)
        if not isinstance(group, list) or len(group) != 20:
            raise AcceptanceError("native ledger " + field + " requires all 20 groups")
        for item in group:
            integer(item, "native ledger " + field, -(2**31), 2**31 - 1)
    if value["flat_day"] != value["record_day"] or value["cash"] != value["record_cash"]:
        raise AcceptanceError("native current record day/cash differs from the actual ledger")
    return {"clock_day": value["flat_day"], "record_present": True,
            **{name: copy.deepcopy(value[name]) for name in ("cash", "record_day", "record_cash", "record_income", "record_expense", "line_counts", "line_income")}}


def cash_inverse(before, refund, restoring):
    integer(refund, "actual Remove refund", 0, 2**31 - 1)
    expected = (redo_ledger(before, refund) if restoring else undo_ledger(before, refund)) if refund else copy.deepcopy(before)
    for field in ("cash", "record_cash", "record_income", "record_expense"):
        integer(expected[field], "expected inverse " + field, -(2**31), 2**31 - 1)
    for field in ("line_counts", "line_income"):
        if len(expected[field]) != 20:
            raise AcceptanceError("inverse ledger requires all 20 groups")
        for number in expected[field]:
            integer(number, "expected inverse " + field, -(2**31), 2**31 - 1)
    return expected


def binding(value, key):
    rows = value.get("bindings")
    if not isinstance(rows, list):
        raise AcceptanceError("actual Root binding list absent")
    matches = [row for row in rows if row.get("record_key") == key]
    if len(matches) != 1:
        raise AcceptanceError(f"exact Building key requires one Root binding: key={key} actual={len(matches)}")
    result = matches[0]
    for field in ("local_uid", "entry_id", "registry_generation"):
        integer(result.get(field), "binding " + field, 1)
    canonical_uuid(result.get("snapshot_id"), "binding snapshot")
    if type(result.get("revoked")) is not bool or result.get("unknown") is not False:
        raise AcceptanceError("Root binding revocation missing or Unknown")
    for field, length in (("rotation", 4), ("scale", 3)):
        if not isinstance(result.get(field), list) or len(result[field]) != length:
            raise AcceptanceError("Root full pose missing " + field)
    for number in [result.get("x"), result.get("y"), result.get("z"), *result["rotation"], *result["scale"]]:
        if type(number) not in (int, float) or not math.isfinite(number):
            raise AcceptanceError("Root full pose is not finite")
    return result


def same_binding(actual, original, peer, phase, alive):
    full_pose(actual, original, peer, phase)
    for field in ("snapshot_id", "record_key", "entry_id", "registry_generation"):
        if actual.get(field) != original.get(field):
            raise AcceptanceError(f"{peer} {phase} retained binding identity differs field={field}")
    if actual.get("revoked") is not (not alive):
        raise AcceptanceError(f"{peer} {phase} current native binding/tombstone differs")


def holder_receipt(engine, original, key, row, peer):
    status = engine.get("native_remove", {})
    if status.get("initialized") is not True or status.get("unknown_locked") is not False:
        raise AcceptanceError(peer + " original Remove runtime is not observed or is Unknown")
    removal = status.get("receipts", {}).get(original)
    retired = status.get("retirements", {}).get(original)
    if not isinstance(removal, dict) or not isinstance(retired, dict):
        raise AcceptanceError(peer + " actual original Remove/holder receipt missing")
    if (removal.get("token") != original or removal.get("record_key") != key
            or removal.get("snapshot_id") != row["snapshot_id"] or removal.get("confirmed") is not True
            or removal.get("binding_revoked") is not True or removal.get("error") != ""
            or retired.get("token") != original or retired.get("confirmed") is not True or retired.get("error") != ""):
        raise AcceptanceError(peer + " original Remove/retirement identity or confirmation differs")
    sequence = integer(removal.get("sequence"), peer + " original Remove sequence", 1)
    origin = engine.get("native_open_origin", {})
    for receipt_field, origin_field in (("world_epoch", "world_epoch"), ("world_generation", "generation")):
        integer(removal.get(receipt_field), peer + " Remove " + receipt_field, 1)
        if removal[receipt_field] != origin.get(origin_field):
            raise AcceptanceError(peer + " original Remove/actual NativeOpen origin differs field=" + receipt_field)
    if retired.get("sequence") != sequence or type(retired.get("sequence")) is not int:
        raise AcceptanceError(peer + " original holder sequence differs")
    for label, receipt in (("Remove", removal), ("retirement", retired)):
        result = receipt.get("result", {})
        if any(result.get(flag) is not True for flag in ("attempted", "returned", "confirmed", "target_retired", "undo_confirmed", "selection_confirmed")):
            raise AcceptanceError(peer + " original " + label + " native execution unconfirmed")
        if (result.get("unknown_locked") is not False or result.get("error") != ""
                or type(result.get("seh_code")) is not int or result["seh_code"] != 0
                or type(result.get("sequence")) is not int or result["sequence"] != sequence):
            raise AcceptanceError(peer + " original " + label + " native cause/sequence differs")
    effects = retired.get("effects", {})
    if (retired["result"].get("retirement_confirmed") is not True or effects.get("attempted") is not True
            or effects.get("confirmed") is not True or effects.get("unknown") is not False):
        raise AcceptanceError(peer + " original held resources are not actually retired")
    integer(effects.get("releases"), peer + " original holder releases", 1)
    frees = effects.get("frees")
    if not isinstance(frees, list) or not frees:
        raise AcceptanceError(peer + " original allocation FreeACKs missing")
    for free in frees:
        for field in ("delete_attempted", "delete_returned", "sized_attempted", "sized_returned", "free_attempted", "free_returned"):
            if type(free.get(field)) is not int or free[field] != 1:
                raise AcceptanceError(peer + " original FreeACK differs field=" + field)
        if type(free.get("seh_code")) is not int or free["seh_code"] != 0:
            raise AcceptanceError(peer + " original FreeACK SEH cause=" + str(free.get("seh_code")))
    return {"removal": removal, "retirement": retired}


def weak_cleanup(value, expected, peer):
    before, after = expected.get("jobs"), value.get("jobs")
    if not isinstance(before, dict) or not isinstance(after, dict):
        raise AcceptanceError(peer + " original Job census missing")
    old_jobs, new_jobs = before.get("jobs"), after.get("jobs")
    if not isinstance(old_jobs, list) or not isinstance(new_jobs, list) or len(old_jobs) != len(new_jobs):
        raise AcceptanceError(peer + " original queue membership changed")
    normalized = copy.deepcopy(after)
    cleared = set()
    for index, (old, new) in enumerate(zip(old_jobs, new_jobs)):
        pair_before = (old.get("building_present"), old.get("building_proxy_present"))
        pair_after = (new.get("building_present"), new.get("building_proxy_present"))
        if any(type(flag) is not bool for flag in pair_before + pair_after):
            raise AcceptanceError(peer + " Job Building/proxy presence flags missing")
        if pair_before == (True, True) and pair_after == (False, False):
            if (type(old.get("queue_index")) is not int or old["queue_index"] != index
                    or old.get("type") != 1 or old.get("valid") is not True or old.get("region_present") is not True
                    or old.get("developer_present") is not False):
                raise AcceptanceError(peer + " cleared reference is not the retained unassigned type1 Job")
            cleared.add(index)
            normalized["jobs"][index]["building_present"] = True
            normalized["jobs"][index]["building_proxy_present"] = True
        elif pair_before != pair_after or pair_after not in ((True, True), (False, False)):
            raise AcceptanceError(peer + " original Job reference partially cleared, replaced or rebound")
    if normalized != before:
        raise AcceptanceError(peer + " original queue or non-reference Job fields changed")
    old_targets, targets = expected.get("job_building_targets"), value.get("job_building_targets")
    if not isinstance(old_targets, dict) or not isinstance(targets, dict):
        raise AcceptanceError(peer + " original weak-target observation absent")
    projected = copy.deepcopy(old_targets)
    old_expired = projected.get("expired_targets")
    if not isinstance(old_expired, list):
        raise AcceptanceError(peer + " original expired-target list absent")
    for index in cleared:
        matches = [entry for entry in old_expired if entry.get("queue_index_diagnostic") == index]
        if len(matches) != 1 or any(matches[0].get(field) is not required for field, required in (
                ("cached_receiver_present", True), ("weak_proxy_present", True),
                ("weak_target_alive", False), ("building_binding_observed", False))):
            raise AcceptanceError(peer + " cleared pair has no prior measured expired weak target")
    projected["expired_targets"] = [entry for entry in old_expired if entry.get("queue_index_diagnostic") not in cleared]
    if cleared and not projected["expired_targets"]:
        projected["building_binding_cause"] = "Queue contains no Building target; no shared Building binding was observed"
    if projected != targets:
        raise AcceptanceError(peer + " original weak-target diagnostics changed beyond the measured complete clear")
    return cleared


def validate_developers(value, expected, ledger, peer):
    if (value.get("observed") is not True or type(value.get("time_scale")) not in (float, int)
            or value["time_scale"] != 0 or value.get("fixture_locked") is not False):
        raise AcceptanceError(peer + " actual paused Developer/Job read is not confirmed")
    if native_ledger(value.get("ledger")) != ledger:
        raise AcceptanceError(peer + " Developer full ledger differs from confirmed history effect")
    rows = value.get("developers")
    if not isinstance(rows, list) or len(rows) != 50:
        raise AcceptanceError(peer + " fixed full50 Developer read absent")
    ids = set()
    for slot, row in enumerate(rows):
        if type(row.get("slot")) is not int or row["slot"] != slot:
            raise AcceptanceError(peer + " native Developer slot order differs")
        identity = integer(row.get("id"), peer + " Developer id", 0, 49)
        if identity in ids:
            raise AcceptanceError(peer + " duplicate native Developer id")
        ids.add(identity)
        integer(row.get("state"), peer + " Developer state", 0, 7)
        if any(type(row.get(field)) is not bool for field in ("employed", "active", "on_map", "job_link")):
            raise AcceptanceError(peer + " native Developer flags are not exact booleans")
        position = row.get("position")
        if not isinstance(position, list) or len(position) != 3 or any(type(n) not in (int, float) or not math.isfinite(n) for n in position):
            raise AcceptanceError(peer + " native Developer pose is not finite")
    for field in ("snapshot_uuid", "world_epoch", "generation", "origin_sha256", "origin_bytes", "business_count", "lounge_count", "count_settled", "indoor_count", "outdoor_count", "developers"):
        if field not in expected or value.get(field) != expected[field]:
            raise AcceptanceError(f"{peer} original expired Job/pool/Origin changed field={field}; no repair is allowed")
    weak_cleanup(value, expected, peer)
    targets = value["job_building_targets"]
    expired = targets.get("expired_targets")
    if targets.get("observed") is not True or targets.get("targets") != [] or not isinstance(expired, list):
        raise AcceptanceError(peer + " original expired Job proxy observation missing")
    if not expired and not any(job.get("type") == 1 and job.get("valid") is True
                              and job.get("region_present") is True and job.get("building_present") is False
                              and job.get("building_proxy_present") is False for job in value["jobs"]["jobs"]):
        raise AcceptanceError(peer + " neither retained expired nor fully cleared original reference was observed")
    for row in expired:
        if (row.get("cached_receiver_present") is not True or row.get("weak_proxy_present") is not True
                or row.get("weak_target_alive") is not False or row.get("building_binding_observed") is not False):
            raise AcceptanceError(peer + " original expired Job weak target was rebound or not measured")


def validate_baseline(value, expected_run):
    if (value.get("passed") is not True or value.get("state") != "original-remove-construction-job-weak-links-observed"
            or value.get("runtime_run_id") != expected_run or value.get("no_clock_advance") is not True):
        raise AcceptanceError("baseline is not the exact completed normal Remove in this owned run")
    original = canonical_uuid(value.get("remove_action_uuid"), "Remove source action")
    key = value.get("building_key")
    if not isinstance(key, str) or not key or key == "action:" + original:
        raise AcceptanceError("baseline must retain the Building key separately from Remove ActionUUID")
    digest = value.get("dll_sha256")
    if not isinstance(digest, str) or len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
        raise AcceptanceError("baseline exact DLL hash absent")
    ledgers = []
    for peer in PEERS:
        before = native_ledger(value.get("before", {}).get(peer, {}).get("ledger"))
        after = native_ledger(value.get("after", {}).get(peer, {}).get("ledger"))
        refund = integer(after["cash"] - before["cash"], peer + " measured native refund", 0, 2**31 - 1)
        if cash_inverse(before, refund, False) != after:
            raise AcceptanceError(peer + " baseline is not one exact native category6 refund")
        validate_developers(value["after"][peer], value["after"][peer], after, peer)
        ledgers.append((before, after, refund))
        if not isinstance(value.get("final_status", {}).get(peer), dict):
            raise AcceptanceError(peer + " baseline confirmed Controller observation missing")
    if ledgers[0] != ledgers[1]:
        raise AcceptanceError("baseline before/after full20 ledgers or refund differ between peers")
    return original, key, ledgers[0][1], ledgers[0][2]


def saved_binding(summary, peer, key, identity):
    jobs = [job for job in summary.get("jobs", []) if job.get("step") == "existing-remove-binding-" + peer]
    if len(jobs) != 1 or jobs[0].get("state") != "confirmed" or jobs[0].get("peer") != peer:
        raise AcceptanceError(peer + " baseline original binding evidence is not uniquely confirmed")
    job = jobs[0]
    path = within(job["evidence_path"], ROOT / "test/mcp/evidence")
    if hash_file(path) != job.get("evidence_sha256"):
        raise AcceptanceError(peer + " original binding evidence hash differs")
    receipt = read_json(path)
    bound = extract_result(receipt, "bind_runtime")
    expected = copy.deepcopy(identity)
    for field in ("runtime_root", "executable_path"):
        expected[field] = str(Path(expected[field]).resolve())
    if bound.get("process_identity_verified") is not True or bound.get("bound") != expected:
        raise AcceptanceError(peer + " baseline binding receipt belongs to another runtime identity")
    return binding(extract_result(receipt, "game_read_remove_bindings"), key)


def preflight(runner, args, runtime_factory):
    canonical_uuid(args.expected_run_id, "expected owned run")
    path = within(args.baseline, ROOT / "artifacts/acceptance")
    base = read_json(path)
    original, key, ledger, refund = validate_baseline(base, args.expected_run_id)
    runner.load_manifests()
    if runner.summary["runtime_run_id"] != args.expected_run_id:
        raise AcceptanceError("actual owned runtime differs from explicit expected run")
    for peer in PEERS:
        identity = read_json(runner.manifests[peer])
        bound = runtime_factory(runner.root).bind(str(runner.manifests[peer]))
        expected = copy.deepcopy(identity)
        for field in ("runtime_root", "executable_path"):
            expected[field] = str(Path(expected[field]).resolve())
        if bound.get("process_identity_verified") is not True or bound.get("bound") != expected:
            raise AcceptanceError(peer + " actual PID/start/owner does not match the retained manifest")
        actual = hash_file(Path(identity["runtime_root"]) / "game/MT2_Multiplayer.dll")
        if actual != base["dll_sha256"] or identity.get("installed_binary_sha256", {}).get("MT2_Multiplayer.dll") != actual:
            raise AcceptanceError(peer + " actual loaded candidate/manifest/baseline DLL differs")
    runner.checked("original-remove-runtime-bound", {"baseline": str(path), "sha256": hash_file(path), "remove_action_uuid": original, "building_key": key, "dll_sha256": base["dll_sha256"], "refund": refund})
    return base, original, key, ledger, refund


def require_initial(current, base, original, key, ledger, originals):
    shared_ready({peer: current[peer]["engine"] for peer in PEERS})
    if compare_ledgers(current["host"]["engine"], current["client-1"]["engine"]) != ledger:
        raise AcceptanceError("ledger changed after the original Remove; no inverse attempted")
    for peer in PEERS:
        state = current[peer]
        controller, earlier = state["engine"]["typed_actions"], base["final_status"][peer]["typed_actions"]
        for field in ("action_uuid", "host_revision", "attempted", "confirmed", "failed", "outcome"):
            if field not in earlier or controller.get(field) != earlier[field]:
                raise AcceptanceError(peer + " confirmed Remove Controller changed field=" + field)
        for field in ("host_revision", "attempted", "confirmed"):
            integer(controller.get(field), peer + " initial " + field, 1)
        if (controller.get("action_uuid") != original or type(controller.get("outcome")) is not int or controller["outcome"] != 1
                or type(controller.get("failed")) is not int or controller["failed"] != 0):
            raise AcceptanceError(peer + " last confirmed action is not the baseline Remove")
        if peer == "host" and controller.get("ack_mask") != 3:
            raise AcceptanceError("baseline Remove lacks both native ACKs")
        ui = state["ui"]
        row = binding(state["bindings"], key)
        if (state["engine"].get("capabilities", {}).get("native_building_history_ui") is not True
                or ui.get("undo_target_action_uuid") != original or ui.get("snapshot_uuid") != row["snapshot_id"]
                or ui.get("active_graph") is not False):
            raise AcceptanceError(peer + " original Remove is not the exact owned native Undo top")
        for field in ("ui_hook_calls", "ui_intents_queued", "undo_depth", "redo_depth"):
            integer(ui.get(field), peer + " history UI " + field)
        same_binding(row, originals[peer], peer, "after Remove", False)
        state["original_holder"] = holder_receipt(state["engine"], original, key, row, peer)
    if current["host"]["engine"]["typed_actions"]["host_revision"] != current["client-1"]["engine"]["typed_actions"]["host_revision"]:
        raise AcceptanceError("initial Host/Client action revisions differ")


def phase_complete(current, before, action, original, key, direction, source, expected_ledger):
    shared_ready({peer: current[peer]["engine"] for peer in PEERS})
    if compare_ledgers(current["host"]["engine"], current["client-1"]["engine"]) != expected_ledger:
        raise AcceptanceError(direction + " exact signed category6 full20 ledger differs")
    for peer in PEERS:
        status, earlier = current[peer]["engine"]["typed_actions"], before[peer]["engine"]["typed_actions"]
        if (status.get("action_uuid") != action or type(status.get("outcome")) is not int or status["outcome"] != 1
                or status.get("pending") is not False or status.get("locked") is not False
                or type(status.get("failed")) is not int or status["failed"] != 0):
            raise AcceptanceError(peer + " " + direction + " is not an exact confirmed idle action: " + str(status.get("cause")))
        for field in ("host_revision", "attempted", "confirmed"):
            if type(status.get(field)) is not int or status[field] != earlier[field] + 1:
                raise AcceptanceError(peer + " " + direction + " exact once/revision differs field=" + field)
        if peer == "host" and status.get("ack_mask") != 3:
            raise AcceptanceError("Host " + direction + " lacks native ACK3")
        ui, old_ui = current[peer]["ui"], before[peer]["ui"]
        for field in ("ui_hook_calls", "ui_intents_queued"):
            if type(ui.get(field)) is not int or ui[field] != old_ui[field] + int(peer == source):
                raise AcceptanceError(peer + " " + direction + " source/UI original-once differs field=" + field)
        if peer == source and (ui.get("last_action_uuid") != action or ui.get("last_target_action_uuid") != original or ui.get("last_direction") != direction):
            raise AcceptanceError(peer + " " + direction + " captured mutation source UUID differs")
        if ui.get("snapshot_uuid") != old_ui["snapshot_uuid"] or ui.get("active_graph") is not False:
            raise AcceptanceError(peer + " " + direction + " native stack owner differs")
        decrease, increase = ("undo_depth", "redo_depth") if direction == "undo" else ("redo_depth", "undo_depth")
        if ui.get(decrease) != old_ui[decrease] - 1 or ui.get(increase) != min(20, old_ui[increase] + 1):
            raise AcceptanceError(peer + " " + direction + " native stack transition differs")
        top = "redo_target_action_uuid" if direction == "undo" else "undo_target_action_uuid"
        if ui.get(top) != original:
            raise AcceptanceError(peer + " " + direction + " inverse graph points at wrong mutation UUID")
        row = binding(current[peer]["bindings"], key)
        same_binding(row, binding(before[peer]["bindings"], key), peer, direction, direction == "undo")
        holder = holder_receipt(current[peer]["engine"], original, key, row, peer)
        if holder != before[peer]["original_holder"]:
            raise AcceptanceError(peer + " original Remove/holder receipt changed across inverse")
        current[peer]["original_holder"] = holder
    return expected_ledger


def wait_complete(runner, before, action, original, key, direction, source, ledger, wait_seconds, now=time.monotonic):
    deadline, poll = now() + wait_seconds, 0
    while True:
        remaining = deadline - now()
        if remaining <= 2:
            raise AcceptanceError(f"{direction} observation timeout action={action}; original already attempted, no replay")
        poll += 1
        current = pair(runner, f"remove-{direction}-observe-{poll}", remaining, include_bindings=False)
        for peer in PEERS:
            status = current[peer]["engine"].get("typed_actions", {})
            if type(status.get("failed")) is not int or status["failed"] != 0 or status.get("locked") is True or status.get("outcome") == 3:
                raise AcceptanceError(peer + " " + direction + " native failure/missing status/Unknown: failed=" + str(status.get("failed")) + " cause=" + str(status.get("cause")))
        if all(current[peer]["engine"]["typed_actions"].get("action_uuid") == action
               and current[peer]["engine"]["typed_actions"].get("pending") is False for peer in PEERS):
            for peer in PEERS:
                current[peer]["bindings"] = runner.single(direction + "-completed-binding-" + peer, peer, "game_read_remove_bindings", {})
            phase_complete(current, before, action, original, key, direction, source, ledger)
            runner.checked(direction + "-remove-native-two-peer-confirmed", {"action_uuid": action, "target_action_uuid": original, "building_key": key, "host_ack_mask": 3, "ledger": ledger, "bindings": {peer: binding(current[peer]["bindings"], key) for peer in PEERS}, "ui": {peer: current[peer]["ui"] for peer in PEERS}})
            return current


def mutate_once(runner, before, original, key, direction, source, ledger, wait_seconds):
    revision = before[source]["engine"]["typed_actions"]["host_revision"]
    request = {"direction": direction, "snapshot_uuid": before[source]["ui"]["snapshot_uuid"], "expected_host_revision": revision, "target_action_uuid": original}
    runner.checked(direction + "-remove-immutable-UI-request", {**request, "building_key": key})
    admitted = runner.single(direction + "-remove-native-entry-once", source, "game_native_building_history_ui", request)
    action = canonical_uuid(admitted.get("action_uuid"), direction + " captured action")
    ui = before[source]["ui"]
    if (action == original or admitted.get("state") != "queued" or admitted.get("target_action_uuid") != original
            or admitted.get("direction") != direction or type(admitted.get("ui_hook_calls")) is not int
            or admitted["ui_hook_calls"] != ui["ui_hook_calls"] + 1
            or type(admitted.get("ui_intents_queued")) is not int or admitted["ui_intents_queued"] != ui["ui_intents_queued"] + 1):
        raise AcceptanceError(direction + " original UI did not capture exactly one request; no replay")
    runner.checked(direction + "-remove-UI-request-queued", admitted)
    runner.summary[direction + "_action_uuid"] = action
    runner.persist()
    return action, wait_complete(runner, before, action, original, key, direction, source, ledger, wait_seconds)


def saved_building(save, identity, run_id, row):
    if (save.get("state") != "snapshot_file_complete" or save.get("distribution_requested") is not False
            or save.get("building_bindings_confirmed") is not True or save.get("building_identity_error") != ""
            or save.get("run_id") != run_id):
        raise AcceptanceError("actual local undistributed Building save is not confirmed")
    path = within(save["path"], Path(identity["runtime_root"]) / "data")
    snapshot_id = canonical_uuid(save.get("snapshot_id"), "local native save snapshot")
    if path.name != "snapshot-" + snapshot_id + ".vrz":
        raise AcceptanceError("local native saved filename differs from its capture UUID")
    size = integer(save.get("bytes"), "local native snapshot bytes", 1, 32 * 1024 * 1024)
    if path.stat().st_size != size or hash_file(path) != save.get("sha256"):
        raise AcceptanceError("local native snapshot bytes/SHA differs from its actual receipt")
    decoder = zlib.decompressobj()
    decoded = decoder.decompress(path.read_bytes(), 32 * 1024 * 1024 + 1)
    if len(decoded) > 32 * 1024 * 1024 or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise AcceptanceError("local native snapshot decode incomplete, oversized or has trailing data")
    reader = RecordReader(decoded)
    if reader.string() != "RecordV2":
        raise AcceptanceError("local native save is not RecordV2")
    reader.record()
    if reader.offset != len(decoded):
        raise AcceptanceError("local native record stream not fully consumed")
    selected = []
    for node in reader.nodes:
        if node["label"] != "mmoBuilding":
            continue
        fields = {child["label"]: child["tokens"] for child in node["children"]}
        transforms = [child for child in node["children"] if child["label"] == "transform"]
        transform = {child["label"]: child["tokens"] for child in transforms[0]["children"]} if len(transforms) == 1 else {}
        xyz = transform.get("translation", [])
        if fields.get("type") == [row["type"]] and fields.get("variant") == [row["variant"]] and len(xyz) == 3 and xyz[0] == row["x"] and xyz[2] == row["z"]:
            selected.append({"record_path": node["path"], "fields": {name: fields.get(name) for name in ("type", "variant", "active", "powered", "state", "linkId")}, "transform": transform})
    if len(selected) != 1:
        raise AcceptanceError(f"restored Building is not unique in complete native save; actual={len(selected)}")
    result = selected[0]
    if result["fields"]["active"] not in (["false"], ["true"]) or result["fields"]["powered"] not in (["false"], ["true"]) or not isinstance(result["fields"]["state"], list) or len(result["fields"]["state"]) != 1:
        raise AcceptanceError("restored native saved active/powered/state fields absent")
    return {"source": str(path), "sha256": save["sha256"], "bytes": size, "fully_consumed": True, **result}


def observe_jobs(runner, label, baseline, expected_ledger):
    observed = {}
    for peer in PEERS:
        observed[peer] = runner.single(label + "-expired-job-" + peer, peer, "game_read_native_developers", {})
        validate_developers(observed[peer], baseline[peer], expected_ledger, peer)
    runner.checked(label + "-original-expired-Job-retained", {peer: {"queue_count": observed[peer]["jobs"]["queue_count"], "expired_targets": observed[peer]["job_building_targets"]["expired_targets"], "cleared_pairs": sorted(weak_cleanup(observed[peer], baseline[peer], peer)), "full50_unchanged": True} for peer in PEERS})
    return observed


def execute(args, runner_factory=AcceptanceRunner, runtime_factory=Runtime):
    args.players, args.cleanup, args.step_timeout = 2, False, 45
    runner = runner_factory(args)
    runner.summary["scope"] = "One Client Undo of a confirmed normal Remove, local saved native state readback, then one Host Redo; no placement/Remove replay, clock change or Job repair"
    try:
        if not math.isfinite(args.wait_seconds) or not 2 < args.wait_seconds <= 60:
            raise AcceptanceError("--wait-seconds must be finite and >2..60")
        base, original, key, removed_ledger, refund = preflight(runner, args, runtime_factory)
        runner.summary.update(remove_action_uuid=original, building_key=key, refund=refund, dll_sha256=base["dll_sha256"])
        runner.persist()
        originals = {peer: saved_binding(base, peer, key, read_json(runner.manifests[peer])) for peer in PEERS}
        initial = pair(runner, "remove-history-before")
        require_initial(initial, base, original, key, removed_ledger, originals)
        before_jobs = observe_jobs(runner, "before-inverse", base["after"], removed_ledger)
        restored_ledger = cash_inverse(removed_ledger, refund, True)
        undo, restored = mutate_once(runner, initial, original, key, "undo", "client-1", restored_ledger, args.wait_seconds)
        restored_jobs = observe_jobs(runner, "after-restore", before_jobs, restored_ledger)
        saves, decoded = {}, {}
        for peer in PEERS:
            saves[peer] = runner.single("restored-local-save-once-" + peer, peer, "game_capture_local_snapshot", {})
            runner.summary.setdefault("local_saves", {})[peer] = saves[peer]
            runner.persist()
            decoded[peer] = saved_building(saves[peer], read_json(runner.manifests[peer]), args.expected_run_id, binding(restored[peer]["bindings"], key))
        for field in ("type", "variant", "active", "powered", "state"):
            if decoded["host"]["fields"][field] != decoded["client-1"]["fields"][field]:
                raise AcceptanceError("restored Host/Client native saved state differs field=" + field)
        runner.checked("restored-original-native-saves-state", decoded)
        # CaptureLocalSnapshot has original save effects. Read again before Redo.
        after_save = pair(runner, "after-local-saves")
        phase_complete(after_save, initial, undo, original, key, "undo", "client-1", restored_ledger)
        observe_jobs(runner, "after-local-saves", restored_jobs, restored_ledger)
        redone_ledger = cash_inverse(restored_ledger, refund, False)
        redo, redone = mutate_once(runner, after_save, original, key, "redo", "host", redone_ledger, args.wait_seconds)
        if redo == undo:
            raise AcceptanceError("Undo and Redo reused the same action UUID")
        final_jobs = observe_jobs(runner, "after-redo-remove", before_jobs, redone_ledger)
        runner.summary.update(passed=True, state="confirmed-native-building-remove-undo-redo-two", dll_sha256=base["dll_sha256"], building_key=key,
                              remove_action_uuid=original, undo_action_uuid=undo, redo_action_uuid=redo, refund=refund,
                              restored_saved_buildings=decoded, final_status={peer: redone[peer]["engine"] for peer in PEERS}, after=final_jobs,
                              no_clock_advance=True, no_job_repair=True, full_coop_proven=False)
    except Exception as error:
        runner.summary.update(passed=False, state="failed-native-building-remove-undo-redo-two", error={"step": runner.summary["step"], "cause": f"{type(error).__name__}: {error}", "behavior": "remaining mutations skipped; inspect retained receipts before any recovery; no native UI/save replay"})
        log("error", str(error))
    runner.summary["finished_utc"] = utc()
    runner.persist()
    print(runner.summary_path)
    return 0 if runner.summary["passed"] else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--expected-run-id", required=True)
    parser.add_argument("--baseline", type=Path, required=True, help="successful native_removed_construction_job.py summary.json from this exact owned run")
    parser.add_argument("--test-root", type=Path, default=ROOT / "_MPTest/tutorial-two/_MPTest")
    parser.add_argument("--wait-seconds", type=float, default=45)
    return execute(parser.parse_args(argv))


if __name__ == "__main__":
    raise SystemExit(main())
