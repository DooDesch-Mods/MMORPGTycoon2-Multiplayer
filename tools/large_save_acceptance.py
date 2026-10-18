"""MCP acceptance for one imported owned large save, without placement or replay."""
from __future__ import annotations

import argparse
import ctypes
import html
import json
import math
import os
from pathlib import Path
import re
import sys
import time
import uuid

from local_acceptance import (AcceptanceError, AcceptanceRunner, ROOT, compare_ledgers,
                              extract_result, hash_file, ledger_state, log, read_json,
                              ready_reason, reject_crash_dialog, utc, validate_snapshot,
                              within, write_json)

INPUT_ID = "106937b1-b5c8-4348-8939-b526cef4ea04"
INPUT_BYTES = 122740859
INPUT_SHA = "176fdd3c48cee809cb57da8db0a187bb6af4c46b21c9666bd22f19d5cdd56689"
SUBSCRIBER_COUNT = 129178
SUBSCRIBER_IDS = (0, 1, 2, 3)
BUILDING_COUNT = 625
PHASES = ("idle", "awaiting_receivers", "transferring", "awaiting_verification", "awaiting_load", "active", "failed")
COUNTERS = ("bytes_total", "chunks_total", "bytes_queued", "chunks_queued", "acknowledged_min",
            "bytes_acknowledged_min", "peers_expected", "peers_ready", "peers_verified", "peers_loaded",
            "bytes_written", "chunks_received", "discarded_after_failure")
ALLOWED_TOOLS = {"game_load_snapshot", "game_capture_snapshot", "game_set_time_scale",
                 "game_inspect_engine", "game_read_subscriber_needs", "game_read_time_ui", "game_read_building_catalog",
                 "runtime_status", "observe_pending", "read_logs"}

def confirm_time_ui(presentation, engine):
    items = presentation.get('items')
    if not isinstance(items, list) or [item.get('scale') for item in items] != [0, 1, 3, 9]:
        raise AcceptanceError('native time-item scales differ')
    if 'shared_time' not in engine:
        if presentation.get('capture_enabled') is not False or presentation.get('host_state_available') is not False:
            raise AcceptanceError('legacy time reader unexpectedly captures or applies state')
        return presentation
    shared = engine['shared_time']
    runtime = shared.get('runtime', {})
    controller = runtime.get('controller', {})
    host = engine.get('peer_id') == 1
    state = controller.get('observed' if host else 'applied', {})
    if (shared.get('initialized') is not True or shared.get('hooks_ready') is not True or
        shared.get('status_read_confirmed') is not True or runtime.get('phase') != 3 or
        runtime.get('hook_mask') != 3 or controller.get('started') is not True or
        controller.get('write_locked') is not False or runtime.get('presentation_uncertain') is not False or
        state.get('present') is not True or state.get('uncertain') is not False):
        raise AcceptanceError('shared time runtime is not active and confirmed')
    if (type(runtime.get('epoch')) is not int or runtime['epoch'] <= 0 or
        type(state.get('revision')) is not int or state['revision'] <= 0 or
        type(presentation.get('epoch')) is not int or type(presentation.get('revision')) is not int or
        type(state.get('selected')) not in (int, float) or state['selected'] not in (0, 1, 3, 9) or
        type(state.get('pause')) not in (int, float) or not math.isfinite(state['pause']) or state['pause'] < 0 or
        presentation.get('capture_enabled') is not True or presentation.get('host_state_available') is not True or
        presentation.get('local_host') is not host or presentation.get('epoch') != runtime['epoch'] or
        presentation.get('revision') != state['revision'] or
        presentation.get('selected_scale') != state.get('selected') or presentation.get('pause_scale') != state.get('pause') or
        presentation.get('effective_scale') != state.get('selected', 0) * state.get('pause', 0)):
        raise AcceptanceError('shared time presentation differs from authoritative receipt')
    for item in items:
        native_state = item.get('native_state')
        if type(native_state) is not int or not -(2**31) <= native_state < 2**31:
            raise AcceptanceError('native time-button state is not int32')
        active = native_state == 0 if host else item['scale'] == presentation['effective_scale']
        if item.get('active') is not active or (not host and (native_state == 0) is not active):
            raise AcceptanceError('native time-button state differs from authoritative selection')
    return presentation


def pending_id(receipt, tool):
    matches = [call for call in receipt.get("calls", []) if call.get("tool") == tool]
    if len(matches) != 1:
        raise AcceptanceError(f"pending observation requires one original call tool={tool} actual={len(matches)}")
    result = matches[0].get("result", {})
    if matches[0].get("state") != "failed" or result.get("isError") is not True:
        raise AcceptanceError(f"pending observation lacks explicit original MCP timeout tool={tool}")
    texts = [part.get("text", "") for part in result.get("content", []) if part.get("type") == "text"]
    pattern = r"^observation_timeout: Request id=([0-9a-f-]{36}) op=([a-z_]+): no matching response within "
    found = [re.match(pattern, text) for text in texts]
    found = [match for match in found if match]
    if len(found) != 1 or found[0][2] != tool.removeprefix("game_"):
        raise AcceptanceError(f"failed call is not a matching native observation timeout tool={tool}; no replay")
    return str(uuid.UUID(found[0][1]))


def matching_pending(value, request_id, run_id):
    if not isinstance(value, dict) or type(value.get("pending")) is not bool:
        raise AcceptanceError("observe_pending response schema invalid")
    if value["pending"]:
        if value.get("request_id") != request_id:
            raise AcceptanceError(f"pending UUID differs actual={value.get('request_id')} required={request_id}; no replay")
        return None
    response = value.get("response")
    if not isinstance(response, dict):
        raise AcceptanceError(f"pending request ended without acknowledgement id={request_id} observation={value}; no replay")
    if response.get("schema_version") != 1 or response.get("id") != request_id or response.get("run_id") != run_id:
        raise AcceptanceError(f"pending acknowledgement identity differs id={request_id}: {response}")
    if response.get("ok") is not True:
        raise AcceptanceError(f"native pending action failed id={request_id} original={response.get('error')}; no replay")
    if not isinstance(response.get("result"), dict):
        raise AcceptanceError(f"native pending acknowledgement has no result id={request_id}")
    return response["result"]


def progress_schema(value):
    if not isinstance(value, dict):
        raise AcceptanceError("snapshot_progress unavailable; native structured progress is required")
    if value.get("direction") not in {"send", "receive"} or value.get("phase") not in PHASES:
        raise AcceptanceError(f"snapshot_progress direction/phase invalid: {value}")
    if not isinstance(value.get("id"), str) or not isinstance(value.get("first_failure"), str):
        raise AcceptanceError("snapshot_progress id/first_failure missing")
    if value["id"]:
        uuid.UUID(value["id"])
    for name in COUNTERS:
        if type(value.get(name)) is not int or value[name] < 0:
            raise AcceptanceError(f"snapshot_progress counter invalid field={name} actual={value.get(name)} required=nonnegative integer")
    for name in ("pending_load", "transfer_active"):
        if type(value.get(name)) is not bool:
            raise AcceptanceError(f"snapshot_progress boolean missing field={name}")
    if not (value["bytes_acknowledged_min"] <= value["bytes_queued"] <= value["bytes_total"] and
            value["acknowledged_min"] <= value["chunks_queued"] <= value["chunks_total"] and
            value["bytes_written"] <= value["bytes_total"] and value["chunks_received"] <= value["chunks_total"]):
        raise AcceptanceError(f"snapshot_progress byte/chunk bounds invalid: {value}")
    if not (value["peers_loaded"] <= value["peers_verified"] <= value["peers_ready"] <= value["peers_expected"] <= 3):
        raise AcceptanceError(f"snapshot_progress peer bounds invalid: {value}")
    return value


class ProgressWatch:
    def __init__(self, snapshot_id, direction, idle_seconds, load_seconds, total_seconds, now):
        self.id, self.direction = snapshot_id, direction
        self.idle_seconds, self.load_seconds, self.total_seconds = idle_seconds, load_seconds, total_seconds
        self.started = self.changed = now
        self.previous = None
        self.condition = "matching snapshot manifest not observed"

    def observe(self, value, now):
        value = progress_schema(value)
        if value["direction"] != self.direction:
            raise AcceptanceError(f"snapshot direction actual={value['direction']} required={self.direction}")
        if value["first_failure"] or value["phase"] == "failed":
            raise AcceptanceError(f"native snapshot failed id={value['id']} first_cause={value['first_failure']!r} counters={value}; no replay")
        matched = value["id"] == self.id
        if matched:
            if self.direction == "send":
                confirmed = (value["bytes_acknowledged_min"], value["acknowledged_min"],
                             value["peers_ready"], value["peers_verified"], value["peers_loaded"])
            else:
                confirmed = (value["bytes_written"], value["chunks_received"])
            state = (PHASES.index(value["phase"]), *confirmed)
            if self.previous is not None and any(current < old for current, old in zip(state, self.previous)):
                raise AcceptanceError(f"snapshot confirmed progress regressed id={self.id} previous={self.previous} actual={state}")
            if state != self.previous:
                self.changed, self.previous = now, state
            self.condition = f"phase={value['phase']} confirmed={confirmed}"
        elapsed, idle = now - self.started, now - self.changed
        bound = self.load_seconds if matched and value["phase"] == "awaiting_load" else self.idle_seconds
        if elapsed > self.total_seconds or idle > bound:
            raise AcceptanceError(f"snapshot observation stalled id={self.id} direction={self.direction} elapsed={elapsed:.3f}s required<={self.total_seconds}s idle={idle:.3f}s required<={bound}s last={self.condition}; no replay")
        if not matched or value["phase"] != "active" or value["pending_load"] or value["transfer_active"]:
            return False
        if self.direction == "send":
            return (0 < value["peers_expected"] == value["peers_loaded"] and
                    value["bytes_acknowledged_min"] == value["bytes_total"] > 0 and
                    value["acknowledged_min"] == value["chunks_total"] > 0)
        return value["bytes_written"] == value["bytes_total"] > 0 and value["chunks_received"] == value["chunks_total"] > 0


def building_readbacks(engine):
    value = engine.get('load_identity')
    if not isinstance(value, dict) or value.get('state') != 'confirmed' or value.get('error'):
        raise AcceptanceError(f'native building readbacks incomplete: {value}')
    for key, expected in (('building_count', BUILDING_COUNT), ('rejected_buildings', 0), ('references', BUILDING_COUNT)):
        if type(value.get(key)) is not int or value[key] != expected:
            raise AcceptanceError(f'native building counter differs field={key} actual={value.get(key)} required={expected}')
    return {key: value[key] for key in ('state', 'snapshot_sha256', 'snapshot_id', 'building_count', 'rejected_buildings', 'references')}


def subscriber_state(value, expected_id):
    if not isinstance(value, dict) or value.get("serialized_id") != expected_id:
        raise AcceptanceError(f"subscriber identity differs actual={value} required={expected_id}")
    if value.get("subscriber_count") != SUBSCRIBER_COUNT:
        raise AcceptanceError(f"native subscriber count actual={value.get('subscriber_count')} required={SUBSCRIBER_COUNT}")
    needs = value.get("needs")
    if not isinstance(needs, list) or len(needs) != 7 or any(type(item) not in (int, float) or not math.isfinite(item) or not -100 <= item <= 100 for item in needs):
        raise AcceptanceError(f"subscriber needs invalid id={expected_id} actual={needs} required=seven finite values -100..100")
    return {key: value[key] for key in ("serialized_id", "subscriber_count", "needs")}


def software_state(engine, client=False):
    value = engine.get("software")
    if not isinstance(value, dict):
        raise AcceptanceError(f"native software readback unavailable cause={engine.get('software_error')}")
    for key in ("rating", "fun"):
        if type(value.get(key)) not in (int, float) or not math.isfinite(value[key]):
            raise AcceptanceError(f"native software {key} missing/nonfinite: {value.get(key)}")
    if client and (value.get("replica_armed") is not True or value.get("replica_bound") is not True or value.get("applied_revision", 0) < 1 or value.get("replica_error")):
        raise AcceptanceError(f"client software replica acknowledgement absent: {value}")
    return {key: value[key] for key in ("rating", "fun")}


def owned_peak_memory(manifest):
    if os.name != "nt":
        raise OSError("owned native process memory counters require Windows")
    # The existing inspector checks PID creation time, image and current-user ownership.
    if str(ROOT / "tools/mcp") not in sys.path:
        sys.path.insert(0, str(ROOT / "tools/mcp"))
    from runtime import windows_process
    def check():
        actual = windows_process(manifest["pid"])
        if actual is None or actual.get("owned_by_current_user") is not True or any(
                actual.get(key) != manifest[key] for key in ("pid", "process_start_time", "executable_path")):
            raise AcceptanceError(f"memory measurement process identity changed pid={manifest['pid']}")
    check()
    from ctypes import wintypes
    class Counters(ctypes.Structure):
        _fields_ = [("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD)] + [(name, ctypes.c_size_t) for name in (
            "PeakWorkingSetSize", "WorkingSetSize", "QuotaPeakPagedPoolUsage", "QuotaPagedPoolUsage",
            "QuotaPeakNonPagedPoolUsage", "QuotaNonPagedPoolUsage", "PagefileUsage", "PeakPagefileUsage")]
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    counter = kernel.K32GetProcessMemoryInfo
    counter.argtypes = [wintypes.HANDLE, ctypes.POINTER(Counters), wintypes.DWORD]
    handle = kernel.OpenProcess(0x1000, False, manifest["pid"])
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    measurement_error = None
    try:
        value = Counters()
        value.cb = ctypes.sizeof(value)
        if not counter(handle, ctypes.byref(value), value.cb):
            raise ctypes.WinError(ctypes.get_last_error())
    except OSError as error:
        measurement_error = error
        raise
    finally:
        if not kernel.CloseHandle(handle):
            close_error = ctypes.WinError(ctypes.get_last_error())
            if measurement_error is None:
                raise close_error
            log("warn", f"memory measurement handle close failed pid={manifest['pid']} original={close_error}; measurement failure retained={measurement_error}")
    check()
    return {"pid": manifest["pid"], "process_start_time": manifest["process_start_time"],
            "working_set_bytes": value.WorkingSetSize, "peak_working_set_bytes": value.PeakWorkingSetSize,
            "peak_private_commit_bytes": value.PeakPagefileUsage}


class LargeSaveRunner(AcceptanceRunner):
    def __init__(self, args, workspace=ROOT):
        if Path(args.test_root).resolve() != (Path(workspace).resolve() / "_MPTest"):
            raise AcceptanceError("large-save acceptance requires this workspace's exact owned _MPTest root")
        super().__init__(args, workspace)
        self.summary["scope"] = "one imported large snapshot with native paused subscriber/needs/software/ledger readbacks; no placement"
        self.summary["mutation_attempts"] = {}
        self.summary["readback_failures"] = []
        self.summary["proof"] = {"host_native_load": False, "host_native_capture": False, "transfer_and_client_load": False,
                                 "subscriber_count": False, "seven_needs_equal": False, "software_equal": False, "ledger_equal": False}
        self.memory_errors = {}
        self.persist()

    def persist(self):
        super().persist()
        if hasattr(self, "summary"):
            short = {key: self.summary.get(key) for key in ("state", "step", "passed", "full_coop_proven", "proof", "pending_observation", "transfer_wait", "memory")}
            (self.directory / "progress.html").write_text("<!doctype html><meta charset='utf-8'><meta http-equiv='refresh' content='5'><title>Großer Save: Abnahme</title><h1>Großer Save: Abnahme</h1><pre>" + html.escape(json.dumps(short, indent=2, ensure_ascii=False)) + "</pre><a href='summary.json'>Belege und Fehler</a>", encoding="utf-8")

    def validate_bound(self, receipt, peer):
        result = extract_result(receipt, "bind_runtime")
        expected = read_json(self.manifests[peer])
        for key in ("runtime_root", "executable_path"):
            expected[key] = str(Path(expected[key]).resolve())
        if result.get("process_identity_verified") is not True or result.get("bound") != expected or expected["run_id"] != self.summary["runtime_run_id"]:
            raise AcceptanceError(f"pending job bound identity differs peer={peer}; no observation of a foreign request")

    def memory(self, peer):
        if self.args.no_memory_counters:
            self.summary.setdefault("memory", {})[peer] = {"state": "deliberately-skipped", "reason": "--no-memory-counters"}
            return
        try:
            value = owned_peak_memory(read_json(self.manifests[peer]))
        except AcceptanceError:
            raise
        except (OSError, AttributeError) as error:
            cause = f"{type(error).__name__}: {error}"
            previous = self.memory_errors.get(peer)
            self.memory_errors[peer] = cause
            if previous != cause:
                log("warn", f"owned process memory counters unavailable peer={peer} cause={cause}; native acceptance continues without this metric")
            self.summary.setdefault("memory", {})[peer] = {"state": "unavailable", "cause": cause}
            return
        if peer in self.memory_errors:
            log("info", f"owned process memory counters recovered peer={peer} previous={self.memory_errors.pop(peer)}")
        self.summary.setdefault("memory", {})[peer] = {"state": "observed", "utc": utc(), **value}

    def single(self, name, peer, tool, arguments=None):
        if tool not in ALLOWED_TOOLS:
            raise AcceptanceError(f"tool excluded from large-save acceptance: {tool}")
        try:
            return super().single(name, peer, tool, arguments)
        except AcceptanceError as original:
            record = self.summary["jobs"][-1]
            if "evidence_path" not in record:
                raise
            receipt = read_json(record["evidence_path"])
            try:
                request_id = pending_id(receipt, tool)
            except (AcceptanceError, ValueError):
                raise original
            self.validate_bound(receipt, peer)
            record["state"] = "awaiting-native-ack"
            record["original_observation_failure"] = str(original)
            log("info", f"MCP observation window ended peer={peer} tool={tool} id={request_id} cause={original}; observing matching ACK without replay")
            result = self.await_pending(peer, tool, request_id)
            record["state"] = "confirmed-after-pending-ack"
            record["native_ack_request_id"] = request_id
            self.persist()
            return result

    def await_pending(self, peer, tool, request_id):
        started, count, previous = time.monotonic(), 0, None
        while True:
            elapsed = time.monotonic() - started
            if elapsed > self.args.action_timeout:
                raise AcceptanceError(f"pending native ACK observation timed out peer={peer} tool={tool} id={request_id} measured={elapsed:.3f}s required<={self.args.action_timeout}s; lock retained; no replay")
            count += 1
            value = super().single(f"pending-{peer}-{count}", peer, "observe_pending")
            result = matching_pending(value, request_id, self.summary["runtime_run_id"])
            if result is not None:
                self.checked("matching-pending-native-ack", {"peer": peer, "tool": tool, "request_id": request_id, "polls": count, "elapsed_seconds": elapsed})
                self.summary["pending_observation"] = {"state": "confirmed", "peer": peer, "tool": tool, "request_id": request_id}
                self.persist()
                log("info", f"matching native ACK recovered peer={peer} tool={tool} id={request_id} polls={count}; mutation was not replayed")
                return result
            status = super().single(f"pending-status-{peer}-{count}", peer, "runtime_status")
            reject_crash_dialog(status, peer)
            self.memory(peer)
            condition = {"native_world": status.get("native_world"), "snapshot_progress": status.get("snapshot_progress")}
            if condition != previous:
                log("info", f"native ACK pending peer={peer} tool={tool} id={request_id} elapsed={elapsed:.3f}s condition={json.dumps(condition, ensure_ascii=False)}")
                previous = condition
            self.summary["pending_observation"] = {"state": "pending", "peer": peer, "tool": tool, "request_id": request_id,
                                                   "poll_count": count, "elapsed_seconds": elapsed, "required_seconds": self.args.action_timeout, "condition": condition}
            self.persist()
            time.sleep(self.args.poll_interval)

    def once(self, name, peer, tool, arguments=None):
        if tool not in {"game_load_snapshot", "game_capture_snapshot", "game_set_time_scale"}:
            raise AcceptanceError(f"unscoped mutation tool: {tool}")
        key = f"{peer}:{tool}"
        if self.summary["mutation_attempts"].get(key, 0):
            raise AcceptanceError(f"mutation replay blocked: {key}")
        self.summary["mutation_attempts"][key] = 1
        self.persist()
        return self.single(name, peer, tool, arguments)

    def input_snapshot(self):
        record = read_json(self.root / "host/diagnostics" / f"snapshot-{INPUT_ID}.json")
        if record.get("bytes") != INPUT_BYTES or record.get("sha256") != INPUT_SHA:
            raise AcceptanceError(f"authorized imported snapshot receipt differs bytes={record.get('bytes')}/{INPUT_BYTES} sha={record.get('sha256')}/{INPUT_SHA}")
        path = Path(record.get("path", "")).resolve()
        expected = self.root / "host/data/76561198013473990/save/MPTestSession" / f"snapshot-{INPUT_ID}.vrz"
        if path != expected.resolve():
            raise AcceptanceError(f"authorized imported snapshot path differs actual={path} required={expected}")
        safe_record = {key: record.get(key) for key in ("snapshot_id", "path", "bytes", "sha256")}
        return validate_snapshot(safe_record, INPUT_ID, self.root / "host")

    def samples(self, peer):
        rows = {}
        for number in SUBSCRIBER_IDS:
            try:
                value = self.single(f"needs-{peer}-{number}", peer, "game_read_subscriber_needs", {"serialized_id": number})
                rows[str(number)] = subscriber_state(value, number)
                self.checked("native-subscriber-read", {"peer": peer, **rows[str(number)]})
            except AcceptanceError as error:
                # Only an acknowledged native read rejection allows the independent snapshot gate to continue.
                record = self.summary["jobs"][-1] if self.summary["jobs"] else {}
                receipt = read_json(record["evidence_path"]) if record.get("evidence_path") else {}
                failed = [call for call in receipt.get("calls", []) if call.get("tool") == "game_read_subscriber_needs"]
                text = " ".join(part.get("text", "") for call in failed for part in call.get("result", {}).get("content", []))
                if "SUBSCRIBER_READ_REJECTED:" not in text:
                    raise
                self.validate_bound(receipt, peer)
                failure = {"peer": peer, "serialized_id": number, "cause": str(error), "evidence_path": record["evidence_path"],
                           "action": "readback unproven; independent owned snapshot persistence and transfer still checked"}
                self.summary["readback_failures"].append(failure)
                log("error", f"subscriber read rejected peer={peer} id={number} original={error}; native needs remain unproven")
                self.persist()
        return rows

    def transfer(self, captured):
        started = time.monotonic()
        watches = {peer: ProgressWatch(captured["snapshot_id"], direction, self.args.transfer_idle_timeout,
                    self.args.action_timeout, self.args.transfer_timeout, started)
                   for peer, direction in (("host", "send"), ("client-1", "receive"))}
        count, previous = 0, None
        while True:
            count += 1
            statuses = {peer: self.single(f"transfer-{peer}-{count}", peer, "runtime_status") for peer in watches}
            finished = {}
            for peer, status in statuses.items():
                reject_crash_dialog(status, peer)
                self.memory(peer)
                progress = progress_schema(status.get("snapshot_progress"))
                if progress["id"] == captured["snapshot_id"] and progress["bytes_total"] != captured["bytes"]:
                    raise AcceptanceError(f"snapshot transfer byte count differs peer={peer} actual={progress['bytes_total']} required={captured['bytes']}")
                finished[peer] = watches[peer].observe(progress, time.monotonic())
            confirmed = {peer: watch.previous for peer, watch in watches.items()}
            if confirmed != previous:
                log("info", f"snapshot confirmed transfer progress id={captured['snapshot_id']} polls={count} value={confirmed}")
                previous = confirmed
            self.summary["transfer_wait"] = {"snapshot_id": captured["snapshot_id"], "polls": count,
                                              "elapsed_seconds": time.monotonic() - started, "progress": {peer: value["snapshot_progress"] for peer, value in statuses.items()}}
            self.persist()
            if all(finished.values()):
                client_record = read_json(self.root / "client-1/diagnostics" / f"snapshot-{captured['snapshot_id']}.json")
                if client_record.get("sha256") != captured["sha256"] or client_record.get("bytes") != captured["bytes"]:
                    raise AcceptanceError("client snapshot receipt differs from captured host SHA/bytes")
                self.checked("client-transferred-file", validate_snapshot(client_record, captured["snapshot_id"], self.root / "client-1"))
                self.checked("native-transfer-and-client-load", self.summary["transfer_wait"])
                return
            time.sleep(self.args.poll_interval)

    def paused_engine(self, peer, name):
        engine = self.inspect(name, peer)
        reject_crash_dialog(engine, peer)
        reason = ready_reason(engine, require_placement=False)
        if reason:
            raise AcceptanceError(f"loaded native world guard peer={peer} condition={reason}")
        ledger_state(engine)
        if engine.get("capabilities", {}).get("read_subscriber_needs") is not True:
            raise AcceptanceError(f"native subscriber read capability unavailable peer={peer}")
        return engine

    def run(self):
        try:
            original = self.input_snapshot()
            self.checked("authorized-imported-copy", original)
            if self.args.restart:
                self.command("restart-stop-owned", [sys.executable, "-X", "utf8", str(self.workspace / "tools/stop_local_test.py"), "--test-root", str(self.root)])
                self.command("restart-start-owned", [sys.executable, "-X", "utf8", str(self.workspace / "tools/local_test.py"), "--test-root", str(self.root), "--players", str(self.args.players), "--ready-timeout", str(self.args.ready_timeout)])
            self.load_manifests()
            resume_id = getattr(self.args, 'resume_native_load', None)
            resumed = None
            if resume_id:
                resume_id = str(uuid.UUID(resume_id))
                resumed = read_json(self.workspace / 'artifacts/acceptance' / resume_id / 'summary.json')
                if resumed.get('runtime_run_id') != self.summary.get('runtime_run_id') or resumed.get('proof', {}).get('host_native_load') is not True or resumed.get('proof', {}).get('host_native_capture') is not False or resumed.get('step') != 'needs-host-0':
                    raise AcceptanceError('resume requires same owned runtime with confirmed input load and failed first read, before native capture')
                self.checked('resumed-confirmed-native-load', {'acceptance_id': resume_id, 'runtime_run_id': resumed['runtime_run_id'], 'input_id': INPUT_ID, 'input_sha256': INPUT_SHA, 'native_load_replayed': False})
            for peer in ("host", "client-1"):
                pending = self.single(f"preflight-pending-{peer}", peer, "observe_pending")
                if pending.get("pending") is not False or "response" in pending or pending.get("abandoned_after_process_exit"):
                    raise AcceptanceError(f"fresh run has unresolved/prior mailbox peer={peer} observed={pending}; no native mutation attempted")
                if not (resumed and peer == 'host'):
                    self.poll_title(peer)
                progress_schema(self.single(f"progress-schema-{peer}", peer, "runtime_status").get("snapshot_progress"))
            if not resumed:
                loaded = self.once("host-load-once", "host", "game_load_snapshot", {"snapshot_id": INPUT_ID})
                if loaded.get("state") != "snapshot_world_active" or loaded.get("snapshot_id") != INPUT_ID:
                    raise AcceptanceError(f"native host load acknowledgement invalid: {loaded}")
                self.once("host-pause-once", "host", "game_set_time_scale", {"scale": 0})
            self.summary["proof"]["host_native_load"] = True
            host = self.paused_engine("host", "host-loaded-readback")
            if resumed:
                identity = host.get('load_identity', {})
                if identity.get('snapshot_id') != INPUT_ID or identity.get('snapshot_sha256') != INPUT_SHA or identity.get('state') != 'confirmed':
                    raise AcceptanceError(f'resumed host no longer has confirmed selected native input: {identity}')
            if getattr(self.args, 'read_native_inventory', False):
                catalog = self.single('host-original-building-catalog', 'host', 'game_read_building_catalog')
                self.checked('host-original-building-catalog', catalog)
            host_samples = self.samples("host")
            self.summary["proof"]["subscriber_count"] = bool(host_samples)
            captured = self.once("host-capture-once", "host", "game_capture_snapshot")
            captured_id = str(uuid.UUID(captured["snapshot_id"]))
            if captured_id == INPUT_ID:
                raise AcceptanceError("native capture did not create a new snapshot UUID")
            captured = validate_snapshot(captured, captured_id, self.root / "host")
            self.checked("host-native-capture", captured)
            self.summary["proof"]["host_native_capture"] = True
            if getattr(self.args, 'read_building_capture', False):
                engine = self.single('host-building-capture-receipt', 'host', 'game_inspect_engine')
                capture = engine.get('building_capture')
                if not isinstance(capture, dict) or capture.get('uuid') != captured_id:
                    raise AcceptanceError(f'Building capture UUID differs: {capture}')
                if type(captured.get('building_bindings_confirmed')) is not bool or capture.get('bindings_confirmed') != captured['building_bindings_confirmed']:
                    raise AcceptanceError(f'Building capture confirmation differs from file receipt: {capture}')
                if capture.get('bindings_confirmed') is True:
                    if capture.get('sha256') != captured['sha256'] or capture.get('bytes') != captured['bytes'] or capture.get('state') != 'confirmed':
                        raise AcceptanceError(f'Confirmed building map lacks matching file identity: {capture}')
                elif not capture.get('error'):
                    raise AcceptanceError(f'Failed building map lacks original cause: {capture}')
                if capture.get('original_attempted', 0) <= 0 or capture.get('original_attempted') != capture.get('original_returned') or capture.get('original_failed') != 0:
                    raise AcceptanceError(f'Native save wrapper completion differs: {capture}')
                if capture.get('original_last_thread', 0) <= 0 or capture.get('game_thread_id', 0) <= 0:
                    raise AcceptanceError(f'Native save wrapper thread evidence missing: {capture}')
                self.checked('host-building-capture-receipt', capture)
                self.summary['proof']['building_bindings_confirmed'] = capture['bindings_confirmed']
            self.transfer(captured)
            self.summary["proof"]["transfer_and_client_load"] = True
            host = self.paused_engine("host", "host-final-readback")
            client = self.paused_engine("client-1", "client-final-readback")
            if getattr(self.args, 'read_network_identity', False):
                identities = {peer: engine.get('network_identity') for peer, engine in (('host', host), ('client-1', client))}
                for peer, identity in identities.items():
                    if not isinstance(identity, dict) or identity.get('ready') is not True or identity.get('uuid') != captured_id or identity.get('snapshot_sha256') != captured['sha256'] or identity.get('staged_nodes') != 240:
                        raise AcceptanceError(f'Native network map not confirmed peer={peer}: {identity}')
                    if not re.fullmatch('[0-9a-f]{64}', identity.get('keys_sha256', '')):
                        raise AcceptanceError(f'Native network key digest invalid peer={peer}: {identity}')
                if identities['host']['keys_sha256'] != identities['client-1']['keys_sha256']:
                    raise AcceptanceError(f'Native network key maps differ: {identities}')
                self.checked('native-network-identity', identities)
                self.summary['proof']['network_identity_confirmed'] = True
            if getattr(self.args, 'read_native_inventory', False):
                for peer in ('host', 'client-1'):
                    catalog = self.single(f'{peer}-final-building-catalog', peer, 'game_read_building_catalog')
                    self.checked(f'{peer}-final-building-catalog', catalog)
                time_ui = self.single('client-native-time-items', 'client-1', 'game_read_time_ui')
                confirm_time_ui(time_ui, client)
                self.checked('client-native-time-items', time_ui)
                self.summary['proof']['native_inventory_read'] = True
            self.summary["load_identity"] = {"host": host.get("load_identity"), "client-1": client.get("load_identity")}
            if getattr(self.args, 'require_building_readbacks', False):
                self.checked('all-native-building-readbacks', {peer: building_readbacks(engine) for peer, engine in (('host', host), ('client-1', client))})
                self.summary['proof']['all_native_building_readbacks'] = True
            self.summary["unproven"] = ["full simulation and mutation replication", "all-peer seven-needs readbacks", "complete scenery/character identity mapping", "visual gameplay acceptance"]
            self.checked("host-client-ledger", compare_ledgers(host, client))
            self.summary["proof"]["ledger_equal"] = True
            host_software, client_software = software_state(host), software_state(client, client=True)
            if host_software != client_software:
                raise AcceptanceError(f"native software differs host={host_software} client={client_software}")
            self.checked("host-client-software", host_software)
            self.summary["proof"]["software_equal"] = True
            client_samples = self.samples("client-1")
            if host_samples.keys() != {str(value) for value in SUBSCRIBER_IDS} or client_samples.keys() != host_samples.keys():
                raise AcceptanceError("seven-needs comparison incomplete; native readback failures retained")
            if host_samples != client_samples:
                raise AcceptanceError(f"native subscriber needs/count differ host={host_samples} client={client_samples}")
            self.checked("host-client-seven-needs", {"serialized_ids": list(SUBSCRIBER_IDS), "native_count": SUBSCRIBER_COUNT, "samples": host_samples})
            self.summary["proof"]["subscriber_count"] = self.summary["proof"]["seven_needs_equal"] = True
            for peer in ("host", "client-1"):
                self.memory(peer)
            self.summary["state"], self.summary["passed"] = "confirmed-bounded-large-save-readback", True
        except Exception as error:
            self.summary["state"], self.summary["passed"] = "failed", False
            self.summary["error"] = {"step": self.summary["step"], "cause": f"{type(error).__name__}: {error}",
                                     "mutation_replayed": False, "action": "remaining mutations skipped; pending locks retained"}
            log("error", f"large-save acceptance failed step={self.summary['step']} cause={error}")
        finally:
            try:
                self.checked("imported-copy-preserved", self.input_snapshot())
            except Exception as error:
                self.summary["passed"], self.summary["state"] = False, "failed"
                self.summary["preservation_error"] = f"{type(error).__name__}: {error}"
                log("error", f"imported-copy preservation check failed original={error}")
            self.summary["finished_utc"] = utc()
            self.summary["full_coop_proven"] = False
            self.persist()
        log("info" if self.summary["passed"] else "error", f"large-save acceptance passed={self.summary['passed']} full_coop_proven=false summary={self.summary_path}")
        return 0 if self.summary["passed"] else 1


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--test-root", type=Path, default=ROOT / "_MPTest")
    result.add_argument("--restart", action="store_true")
    result.add_argument("--players", type=int, choices=(2, 3, 4), default=2)
    result.add_argument("--step-timeout", type=float, default=20)
    result.add_argument("--ready-timeout", type=float, default=180)
    result.add_argument("--action-timeout", type=float, default=900)
    result.add_argument("--transfer-idle-timeout", type=float, default=90)
    result.add_argument("--transfer-timeout", type=float, default=3600)
    result.add_argument("--poll-interval", type=float, default=1)
    result.add_argument("--no-memory-counters", action="store_true")
    result.add_argument("--require-building-readbacks", action="store_true")
    result.add_argument("--read-native-inventory", action="store_true")
    result.add_argument("--read-building-capture", action="store_true")
    result.add_argument("--read-network-identity", action="store_true")
    result.add_argument("--resume-native-load")
    return result


def main(argv=None):
    arguments = parser().parse_args(argv)
    if arguments.resume_native_load and arguments.restart:
        parser().error('--resume-native-load cannot restart the owned runtime')
    arguments.cleanup = False
    arguments.snapshot_id = INPUT_ID
    for name in ("step_timeout", "ready_timeout", "action_timeout", "transfer_idle_timeout", "transfer_timeout", "poll_interval"):
        value = getattr(arguments, name)
        if not math.isfinite(value) or value <= 0:
            parser().error(f"{name} requires a finite positive number")
    if not 0.05 <= arguments.step_timeout <= 45 or arguments.poll_interval > 10:
        parser().error("step-timeout requires0.05..45 seconds; poll-interval requires<=10 seconds")
    return LargeSaveRunner(arguments).run()


if __name__ == "__main__":
    sys.exit(main())
