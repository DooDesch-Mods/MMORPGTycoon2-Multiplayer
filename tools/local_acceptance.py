"""Run saved MCP acceptance jobs for one bounded local placement candidate."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent / "mcp"))
from runtime import Runtime, FRAMEWORK_PLUGIN_KIND, manifest_runtime_kind, runtime_diagnostic_directory
LEDGER_FIELDS = ("cash", "record_present", "record_day", "record_cash", "record_income", "record_expense", "line_counts", "line_income")


class AcceptanceError(RuntimeError):
    pass


def utc():
    return datetime.now(timezone.utc).isoformat()


def log(level, text):
    print(f"{level}: {text}", flush=True)


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for attempt in range(5):
        try:
            temporary.replace(path)
        except PermissionError as error:
            if attempt == 4:
                log("error", f"acceptance summary replace failed target={path}: {error}")
                raise
            if attempt == 0:
                log("warn", f"acceptance summary replace temporarily blocked target={path}: {error}")
            time.sleep(0.025 * 2 ** attempt)
        else:
            if attempt:
                log("info", f"acceptance summary replace recovered target={path} retries={attempt}")
            return


def hash_file(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def within(path, root):
    path, root = Path(path).resolve(), Path(root).resolve()
    if not path.is_relative_to(root):
        raise AcceptanceError(f"owned artifact path outside root: actual={path} required=inside {root}")
    return path


def validate_snapshot(record, snapshot_id, host_root):
    if record.get("snapshot_id") != snapshot_id:
        raise AcceptanceError("input snapshot identifier does not match its diagnostic record")
    path = within(record["path"], host_root / "data")
    length = record.get("bytes")
    digest = record.get("sha256")
    if type(length) is not int or not 0 < length <= 1024 * 1024 * 1024:
        raise AcceptanceError(f"input snapshot size invalid: {length}")
    if not isinstance(digest, str) or len(digest) != 64:
        raise AcceptanceError("input snapshot SHA256 is missing or invalid")
    if path.stat().st_size != length:
        raise AcceptanceError(f"input snapshot size differs actual={path.stat().st_size} required={length}")
    actual = hash_file(path)
    if actual != digest:
        raise AcceptanceError(f"input snapshot SHA256 differs actual={actual} required={digest}")
    return {**record, "path": str(path), "file_verified": True}


def ledger_state(engine):
    clock = engine.get("clock")
    ledger = engine.get("simulation")
    if not isinstance(clock, dict) or not isinstance(ledger, dict):
        raise AcceptanceError(f"native clock/ledger unavailable: clock={engine.get('clock_error')} simulation={engine.get('simulation_error')}")
    if clock.get("scale") != 0 or clock.get("paused") is not True:
        raise AcceptanceError(f"native calendar must be paused: scale={clock.get('scale')} paused={clock.get('paused')}")
    if type(clock.get("day")) is not int:
        raise AcceptanceError("native clock day is missing")
    if ledger.get("record_present") is not True:
        raise AcceptanceError("native current-day record is absent")
    for field in ("cash", "record_day", "record_cash", "record_income", "record_expense"):
        if type(ledger.get(field)) is not int:
            raise AcceptanceError(f"native ledger field missing/invalid: {field}")
    for field in ("line_counts", "line_income"):
        value = ledger.get(field)
        if not isinstance(value, list) or len(value) != 20 or any(type(item) is not int for item in value):
            raise AcceptanceError(f"native ledger {field} requires exactly20 integer categories; actual={value}")
    if ledger["record_day"] != clock["day"]:
        raise AcceptanceError(f"native record day differs from clock: record={ledger['record_day']} clock={clock['day']}")
    if ledger["record_cash"] != ledger["cash"]:
        raise AcceptanceError(f"native cash differs from current record: cash={ledger['cash']} record={ledger['record_cash']}")
    return {"clock_day": clock["day"], **{key: ledger[key] for key in LEDGER_FIELDS}}


def compare_ledgers(host, client):
    left, right = ledger_state(host), ledger_state(client)
    differences = {field: {"host": left[field], "client": right[field]} for field in left if left[field] != right[field]}
    if differences:
        raise AcceptanceError("host/client ledger mismatch: " + json.dumps(differences, ensure_ascii=False))
    return left


def placement_expense(before, after):
    amount = before["cash"] - after["cash"]
    if amount <= 0 or before["clock_day"] != after["clock_day"] or before["record_day"] != after["record_day"] or before["record_income"] != after["record_income"] or after["record_expense"] - before["record_expense"] != amount:
        raise AcceptanceError(f"placement ledger delta differs from one expense: cash_delta={amount}")
    for category in range(20):
        expected_count = before["line_counts"][category] + (1 if category == 6 else 0)
        expected_income = before["line_income"][category] - (amount if category == 6 else 0)
        if after["line_counts"][category] != expected_count or after["line_income"][category] != expected_income:
            raise AcceptanceError(f"placement ledger delta category={category} count={after['line_counts'][category]} required={expected_count} income={after['line_income'][category]} required={expected_income}")
    return {"cash_delta": amount, "expense_category": 6, "line_count_delta": 1}


def ready_reason(engine, require_placement=True):
    if engine.get("game_thread_ready") is not True:
        return "game thread is not ready"
    if engine.get("session_connected") is not True:
        return "session is not connected"
    if engine.get("in_game") is not True:
        return "native world is not active"
    world = engine.get("native_world", {})
    if world.get("world_present") is not True or world.get("mode_state") != 4:
        return "native loaded world has not reached state4"
    if world.get("quitting") is not False or world.get("title_pending") is not False:
        return "native title/quit transition is pending"
    if world.get("edits") != 0 or world.get("saves") != 0 or world.get("map_task_pending") is not False:
        return f"native world busy edits={world.get('edits')} saves={world.get('saves')} map={world.get('map_task_pending')}"
    if require_placement and engine.get("capabilities", {}).get("place_test_building") is not True:
        return "placement capability is not available"
    return None


def reject_crash_dialog(engine, peer):
    world = engine.get("native_world", {})
    if world.get("crash_report_active") is True or world.get("active_mode") == 26:
        raise AcceptanceError(f"native crash dialog active peer={peer}; no dismiss operation requested; remaining mutations skipped")


def title_reason(engine):
    if engine.get("game_thread_ready") is not True:
        return "game thread is not ready"
    if engine.get("session_connected") is not True:
        return "session is not connected"
    world = engine.get("native_world", {})
    if world.get("active_mode") != 2:
        return f"native title mode actual={world.get('active_mode')} required=2"
    if world.get("title_pending") is not False or world.get("quitting") is not False:
        return "native title/quit transition is pending"
    if engine.get("capabilities", {}).get("load_snapshot") is not True:
        return "native load_snapshot capability actual=false required=true"
    return None


def extract_result(receipt, tool):
    matches = [call for call in receipt.get("calls", []) if call.get("tool") == tool]
    if len(matches) != 1:
        raise AcceptanceError(f"MCP receipt requires exactly1 {tool} call; actual={len(matches)}")
    call = matches[0]
    result = call.get("result", {}).get("structuredContent")
    if call.get("state") != "confirmed" or not isinstance(result, dict) or call.get("result", {}).get("isError") is True:
        raise AcceptanceError(f"MCP call {tool} was not confirmed: {call.get('cause', result)}")
    if tool.startswith("game_"):
        if result.get("ok") is not True:
            raise AcceptanceError(f"MCP game acknowledgement failed tool={tool}: {result.get('error')}")
        return result["result"]
    return result


def validate_job_results(receipt, calls):
    observed = receipt.get("calls", [])
    if len(observed) != len(calls) + 1:
        raise AcceptanceError(f"MCP job call count actual={len(observed)} required={len(calls) + 1}")
    for index, planned in enumerate(calls, 1):
        actual = observed[index]
        if actual.get("index") != index or actual.get("tool") != planned["tool"]:
            raise AcceptanceError(f"MCP job call order differs at index={index} required={planned['tool']}")
        # A batch can intentionally use the same tool at different positions,
        # such as Start/read/Pause. Validate each original acknowledgement.
        extract_result({"calls": [actual]}, planned["tool"])


class AcceptanceRunner:
    def __init__(self, args, workspace=ROOT):
        self.args, self.workspace = args, Path(workspace).resolve()
        self.root = Path(args.test_root).resolve()
        self.id = str(uuid.uuid4())
        self.directory = self.workspace / "artifacts/acceptance" / self.id
        self.directory.mkdir(parents=True, exist_ok=False)
        self.summary_path = self.directory / "summary.json"
        self.summary = {"schema_version": 1, "acceptance_id": self.id, "started_utc": utc(), "state": "running",
                        "passed": False, "full_coop_proven": False, "scope": "one diagnostic placement with native ledger and object readback",
                        "parameters": {key: str(value) if isinstance(value, Path) else value for key, value in vars(args).items()},
                        "step": "preflight", "jobs": [], "checks": [], "commands": [], "cleanup": {"requested": args.cleanup, "state": "pending" if args.cleanup else "deliberately-skipped"},
                        "mutation_retries": 0}
        self.manifests = {}
        self.persist()

    def persist(self):
        write_json(self.summary_path, self.summary)

    def checkpoint(self, step):
        self.summary["step"] = step
        self.persist()
        log("info", f"acceptance={self.id} step={step}")

    def checked(self, name, value):
        self.summary["checks"].append({"check": name, "confirmed_utc": utc(), "result": value})
        self.persist()

    def command(self, label, command):
        self.checkpoint(label)
        path = self.directory / f"{label}.log"
        record = {"step": label, "command": command, "log_path": str(path), "state": "attempted", "started_utc": utc()}
        self.summary["commands"].append(record)
        self.persist()
        try:
            with path.open("w", encoding="utf-8") as stream:
                result = subprocess.run(command, cwd=self.workspace, stdout=stream, stderr=subprocess.STDOUT,
                                        timeout=60, check=False)
            record["exit_code"], record["finished_utc"] = result.returncode, utc()
            record["state"] = "confirmed" if result.returncode == 0 else "failed"
            self.persist()
            if result.returncode:
                raise AcceptanceError(f"command step={label} exit={result.returncode} log={path}\n{path.read_text(encoding='utf-8', errors='replace')[-6000:]}")
        except subprocess.TimeoutExpired as error:
            record["state"], record["cause"] = "observation-timeout", str(error)
            self.persist()
            raise AcceptanceError(f"command observation timed out step={label} required=60s log={path}; effects unconfirmed, no replay") from error
        except OSError as error:
            record["state"], record["cause"] = "failed-to-start", f"{type(error).__name__}: {error}"
            self.persist()
            raise AcceptanceError(f"command failed to start step={label} target={command[3]} cause={error} log={path}") from error

    def load_manifests(self):
        identities = {}
        players = getattr(self.args, 'players', 2)
        for peer in range(1, players + 1):
            name = 'host' if peer == 1 else f'client-{peer - 1}'
            path = within(self.root / name / "instance.json", self.root)
            identity = read_json(path)
            run_id = identity.get("run_id")
            uuid.UUID(run_id)
            if identity.get("schema_version") == 2:
                identity = Runtime(self.root)._manifest_value(identity, expected_root=path.parent)
            if identity.get("schema_version") not in (1, 2) or identity.get("owner") != {"kind": "MPTest", "run_id": run_id}:
                raise AcceptanceError(f"manifest ownership invalid: {path}")
            if identity.get("peer_id") != peer or Path(identity.get("runtime_root", "")).resolve() != (self.root / name).resolve():
                raise AcceptanceError(f"manifest peer/runtime differs: {path}")
            if type(identity.get("pid")) is not int or not str(identity.get("process_start_time", "")).isdigit():
                raise AcceptanceError(f"manifest process identity is incomplete: {path}")
            identities[name] = identity
            self.manifests[name] = path
        if any(identity['run_id'] != identities['host']['run_id'] for identity in identities.values()):
            raise AcceptanceError("host/client instance manifests refer to different runs")
        kinds = {manifest_runtime_kind(identity) for identity in identities.values()}
        if FRAMEWORK_PLUGIN_KIND in kinds and len(kinds) != 1:
            raise AcceptanceError("Framework and standalone peers cannot share this acceptance candidate")
        self.summary["runtime_run_id"] = identities["host"]["run_id"]
        self.checked("instance-manifest-identity", identities)

    def job(self, name, peer, calls, remaining=None):
        self.checkpoint(name)
        index = len(self.summary["jobs"])
        label = f"a{self.id.replace('-', '')[:12]}-{index:02d}-{name[:12]}"
        spec_path = self.directory / "jobs" / f"{index:03d}-{name}.json"
        timeout = min(self.args.step_timeout, max(0.05, remaining - 1)) if remaining is not None else self.args.step_timeout
        wait = min(60, timeout + 15) if remaining is None else min(60, max(0.05, remaining))
        write_json(spec_path, {"timeout_seconds": timeout, "calls": calls})
        stdout_path = spec_path.with_suffix(".runner.log")
        command = [sys.executable, "-X", "utf8", str(self.workspace / "tools/mcp/run_job.py"), "--manifest", str(self.manifests[peer]), "--spec", str(spec_path), "--label", label]
        record = {"step": name, "peer": peer, "spec_path": str(spec_path), "runner_log": str(stdout_path),
                  "state": "attempted", "started_utc": utc(), "tools": [call["tool"] for call in calls], "wait_bound_seconds": wait}
        self.summary["jobs"].append(record)
        self.persist()
        try:
            with stdout_path.open("w", encoding="utf-8") as stream:
                result = subprocess.run(command, cwd=self.workspace, stdout=stream, stderr=subprocess.STDOUT,
                                        timeout=wait, check=False)
            record["exit_code"] = result.returncode
        except subprocess.TimeoutExpired as error:
            record["state"], record["cause"] = "observation-timeout", str(error)
            try:
                self.index_receipt(record, label, spec_path)
            except (AcceptanceError, OSError, ValueError) as index_error:
                record["receipt_index_error"] = f"{type(index_error).__name__}: {index_error}"
                log("warn", f"timed-out MCP job receipt unavailable step={name}: {index_error}; original={error}")
            record["state"] = "observation-timeout"
            self.persist()
            raise AcceptanceError(f"MCP job observation timed out step={name} bound={wait}s; original receipt={record.get('evidence_path')}; effects unconfirmed, no replay") from error
        except OSError as error:
            record["state"], record["cause"] = "failed-to-start", f"{type(error).__name__}: {error}"
            self.persist()
            raise AcceptanceError(f"MCP job failed to start step={name} target={command[3]} cause={error} runner_log={stdout_path}") from error
        receipt = self.index_receipt(record, label, spec_path)
        record["finished_utc"] = utc()
        if result.returncode != 0 or receipt.get("passed") is not True:
            record["state"], record["error"] = "failed", receipt.get("error")
            self.persist()
            raise AcceptanceError(f"MCP job failed step={name} exit={result.returncode} cause={json.dumps(receipt.get('error'), ensure_ascii=False)} evidence={record['evidence_path']}; remaining mutations skipped")
        bound = extract_result(receipt, "bind_runtime")
        expected = read_json(self.manifests[peer])
        for field in ("runtime_root", "executable_path"):
            expected[field] = str(Path(expected[field]).resolve())
        if bound.get("process_identity_verified") is not True or bound.get("bound") != expected:
            record["state"] = "failed-identity-guard"
            self.persist()
            raise AcceptanceError(f"MCP bound identity differs step={name}; original evidence={record['evidence_path']}")
        if expected["run_id"] != self.summary["runtime_run_id"]:
            record["state"] = "failed-identity-guard"
            self.persist()
            raise AcceptanceError(f"runtime identity changed during acceptance step={name}")
        try:
            validate_job_results(receipt, calls)
        except AcceptanceError as error:
            record["state"], record["cause"] = "failed-native-acknowledgement", str(error)
            self.persist()
            raise
        record["state"] = "confirmed"
        self.persist()
        return receipt

    def index_receipt(self, record, label, spec_path):
        directory = self.workspace / "test/mcp/evidence"
        paths = sorted(directory.glob(label + "_*.json"))
        if len(paths) != 1:
            record["receipt_discovery"] = {"expected": 1, "actual": len(paths), "label": label}
            record["state"] = "failed-evidence-guard"
            self.persist()
            raise AcceptanceError(f"original MCP receipt count actual={len(paths)} required=1 label={label} runner_log={record['runner_log']}")
        path = within(paths[0], directory)
        receipt = read_json(path)
        record["evidence_path"], record["mcp_log_path"] = str(path), str(path.with_suffix(".log"))
        record["evidence_sha256"] = hash_file(path)
        record["calls"] = [{"tool": call.get("tool"), "state": call.get("state"), "index": call.get("index"),
                            "request_id": call.get("result", {}).get("structuredContent", {}).get("id")} for call in receipt.get("calls", [])]
        self.persist()
        if Path(receipt.get("spec_path", "")).resolve() != spec_path.resolve() or receipt.get("spec_sha256") != hash_file(spec_path):
            record["state"] = "failed-evidence-guard"
            self.persist()
            raise AcceptanceError(f"original MCP receipt spec identity differs: {path}")
        return receipt

    def single(self, name, peer, tool, arguments=None):
        receipt = self.job(name, peer, [{"tool": tool, "arguments": arguments or {}}])
        return extract_result(receipt, tool)

    def inspect(self, name, peer):
        return self.single(name, peer, "game_inspect_engine")

    def poll_title(self, peer):
        deadline, count, previous = time.monotonic() + self.args.ready_timeout, 0, None
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise AcceptanceError(f"native title/load readiness timed out peer={peer} required={self.args.ready_timeout}s last_condition={previous}; no load/capture mutation attempted after this guard")
            count += 1
            receipt = self.job(f"title-{peer}-{count:02d}", peer, [{"tool": "game_inspect_engine", "arguments": {}}], remaining=remaining)
            engine = extract_result(receipt, "game_inspect_engine")
            reject_crash_dialog(engine, peer)
            reason = title_reason(engine)
            if reason is None:
                self.checked(f"{peer}-native-title-load-ready", {"poll_count": count, "native_world": engine["native_world"],
                                                               "load_snapshot": True, "window": engine.get("window")})
                log("info", f"native title/load ready peer={peer} polls={count}")
                return engine
            if reason != previous:
                log("info", f"native title/load pending peer={peer} condition={reason} polls={count}")
                previous = reason
            self.summary.setdefault("title_waits", {})[peer] = {"condition": reason, "poll_count": count, "required_seconds": self.args.ready_timeout}
            self.persist()
            time.sleep(min(0.25, max(0, deadline - time.monotonic())))

    def poll_client(self, snapshot_id, peer='client-1'):
        deadline, count, previous = time.monotonic() + self.args.ready_timeout, 0, None
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise AcceptanceError(f"client readiness observation timed out required={self.args.ready_timeout}s last_condition={previous}; no placement attempted")
            count += 1
            receipt = self.job(f"{peer}-ready-{count:02d}", peer, [
                {"tool": "game_inspect_engine", "arguments": {}},
                {"tool": "read_logs", "arguments": {"filename": "mt2mp.log", "max_bytes": 65536}},
            ], remaining=remaining)
            engine = extract_result(receipt, "game_inspect_engine")
            reject_crash_dialog(engine, peer)
            text = extract_result(receipt, "read_logs").get("text", "")
            reason = ready_reason(engine)
            if reason is None and f"WORLD_ACTIVE after native snapshot load id={snapshot_id}" not in text:
                reason = f"native WORLD_ACTIVE acknowledgement missing for captured snapshot={snapshot_id}"
            if reason is None:
                self.checked("client-new-snapshot-world-active", {"peer": peer, "snapshot_id": snapshot_id, "poll_count": count})
                return engine
            if reason != previous:
                log("info", f"client readiness pending condition={reason} polls={count}")
                previous = reason
            self.summary["client_wait"] = {"condition": reason, "poll_count": count, "required_seconds": self.args.ready_timeout}
            self.persist()
            time.sleep(min(0.25, max(0, deadline - time.monotonic())))

    def object_readback(self, host, client, placement):
        if placement.get("state") != "host_result_applied":
            raise AcceptanceError(f"client placement state actual={placement.get('state')} required=host_result_applied")
        mapping = placement.get("placement", {})
        uuid.UUID(mapping["operation_id"])
        matches = {}
        for name, engine, uid_key in (("host", host, "host_uid"), ("client-1", client, "local_uid")):
            uid = mapping.get(uid_key)
            if type(uid) is not int or uid <= 0:
                raise AcceptanceError(f"placement has invalid {uid_key}: {uid}")
            props = [prop for prop in engine.get("props", []) if prop.get("uid") == uid]
            if len(props) != 1:
                raise AcceptanceError(f"placed object readback peer={name} uid={uid} count={len(props)} required=1")
            prop = props[0]
            if prop.get("building") is not True or prop.get("region_owned") is not True or prop.get("type") != self.args.place_type or prop.get("variant") != self.args.place_variant:
                raise AcceptanceError(f"placed object type/region readback differs peer={name}: {prop}")
            for coordinate in ("x", "z"):
                if not math.isclose(prop.get(coordinate, math.inf), getattr(self.args, coordinate), rel_tol=0, abs_tol=0.001):
                    raise AcceptanceError(f"placed object {coordinate} differs peer={name}: actual={prop.get(coordinate)} required={getattr(self.args, coordinate)}")
            matches[name] = prop
        if not math.isclose(matches["host"]["y"], matches["client-1"]["y"], rel_tol=0, abs_tol=0.001):
            raise AcceptanceError("placed object grounded height differs between peers")
        self.checked("placed-object-native-readback", {"operation_id": mapping["operation_id"], "revision": mapping.get("revision"), "objects": matches})
        return matches

    def capture(self, peer):
        frame = self.single(f"{peer}-frame", peer, "game_capture_frame")
        identity = read_json(self.manifests[peer])
        boundary = runtime_diagnostic_directory(identity) if identity.get("schema_version") == 2 else self.root / peer / "diagnostics"
        path = within(frame["path"], boundary)
        if path.stat().st_size != frame.get("bytes") or frame.get("width", 0) <= 0 or frame.get("height", 0) <= 0:
            raise AcceptanceError(f"native frame file/readback differs peer={peer} path={path}")
        self.checked("native-frame-artifact", {"peer": peer, **frame, "file_sha256": hash_file(path),
                                                "visual_acceptance": "requires inspection of the native frame"})

    def cleanup(self):
        if not self.args.cleanup:
            log("info", "cleanup deliberately skipped; owned windows remain available")
            return
        try:
            current = read_json(self.root / "run.json")
            expected = self.summary.get("runtime_run_id")
            if expected is not None and current.get("run_id") != expected:
                raise AcceptanceError(f"cleanup refused: runtime run changed actual={current.get('run_id')} required={expected}")
            self.command("cleanup-owned", [sys.executable, "-X", "utf8", str(self.workspace / "tools/stop_local_test.py"), "--test-root", str(self.root)])
            self.summary["cleanup"] = {"requested": True, "state": "confirmed", "run_id": current.get("run_id")}
        except Exception as error:
            self.summary["cleanup"] = {"requested": True, "state": "failed", "cause": f"{type(error).__name__}: {error}"}
            self.summary["passed"] = False
            log("error", f"acceptance cleanup failed: {error}")
        self.persist()

    def run(self):
        try:
            input_record = read_json(self.root / "host/diagnostics" / f"snapshot-{self.args.snapshot_id}.json")
            verified = validate_snapshot(input_record, self.args.snapshot_id, self.root / "host")
            self.checked("input-snapshot-file", verified)
            if self.args.restart:
                self.command("restart-stop-owned", [sys.executable, "-X", "utf8", str(self.workspace / "tools/stop_local_test.py"), "--test-root", str(self.root)])
                self.command("restart-start-owned", [sys.executable, "-X", "utf8", str(self.workspace / "tools/local_test.py"), "--test-root", str(self.root), "--ready-timeout", str(self.args.ready_timeout)])
            self.load_manifests()
            self.poll_title("host")
            loaded = self.single("host-load", "host", "game_load_snapshot", {"snapshot_id": self.args.snapshot_id})
            if loaded.get("state") != "snapshot_world_active" or loaded.get("snapshot_id") != self.args.snapshot_id:
                raise AcceptanceError(f"host native snapshot load was not acknowledged: {loaded}")
            self.single("host-pause", "host", "game_set_time_scale", {"scale": 0})
            host = self.inspect("host-inspect-loaded", "host")
            if ready_reason(host, require_placement=False):
                raise AcceptanceError(f"host loaded-world guard failed: {ready_reason(host, require_placement=False)}")
            self.checked("host-paused-native-ledger", ledger_state(host))
            self.poll_title("client-1")
            captured = self.single("host-capture-snapshot", "host", "game_capture_snapshot")
            uuid.UUID(captured["snapshot_id"])
            self.checked("host-new-snapshot-file", validate_snapshot(captured, captured["snapshot_id"], self.root / "host"))
            client = self.poll_client(captured["snapshot_id"])
            host = self.inspect("host-before-placement", "host")
            self.checkpoint("guard-ledger-before-placement")
            before = compare_ledgers(host, client)
            self.checked("host-client-ledger-before-placement", before)
            parameters = {"type": self.args.place_type, "variant": self.args.place_variant, "x": self.args.x, "z": self.args.z}
            self.single("client-placement-probe", "client-1", "game_probe_building_placement", parameters)
            placement = self.single("client-place-once", "client-1", "game_place_test_building", parameters)
            host, client = self.inspect("host-after-placement", "host"), self.inspect("client-after-placement", "client-1")
            self.checkpoint("guard-ledger-after-placement")
            after = compare_ledgers(host, client)
            self.checked("host-client-ledger-after-placement", after)
            self.checked("one-native-placement-expense", placement_expense(before, after))
            objects = self.object_readback(host, client, placement)
            if self.args.camera_distance is not None:
                for peer in ("host", "client-1"):
                    prop = objects[peer]
                    self.single(f"{peer}-focus-once", peer, "game_focus_test_camera",
                                {"x": prop["x"], "y": prop["y"], "z": prop["z"], "distance": self.args.camera_distance})
            for peer in ("host", "client-1"):
                self.capture(peer)
            self.summary["passed"], self.summary["state"] = True, "confirmed-bounded-placement"
        except Exception as error:
            self.summary["state"] = "failed"
            self.summary["error"] = {"step": self.summary["step"], "cause": f"{type(error).__name__}: {error}",
                                     "mutation_replayed": False, "action": "remaining mutations skipped"}
            log("error", f"acceptance failed step={self.summary['step']} cause={error}")
            self.persist()
        finally:
            self.cleanup()
            self.summary["finished_utc"] = utc()
            if self.summary["cleanup"]["state"] == "failed":
                self.summary["state"] = "cleanup-failed"
            self.persist()
        log("info" if self.summary["passed"] else "error", f"acceptance passed={self.summary['passed']} full_coop_proven=false summary={self.summary_path}")
        return 0 if self.summary["passed"] else 1


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--snapshot-id", type=lambda value: str(uuid.UUID(value)), required=True)
    result.add_argument("--test-root", type=Path, default=ROOT / "_MPTest")
    result.add_argument("--restart", action="store_true")
    result.add_argument("--cleanup", action="store_true")
    result.add_argument("--place-type", default="StartingPoint")
    result.add_argument("--place-variant", default="base")
    result.add_argument("--x", type=float, default=11047.5625)
    result.add_argument("--z", type=float, default=6811.82861328125)
    result.add_argument("--camera-distance", type=float)
    result.add_argument("--step-timeout", type=float, default=45)
    result.add_argument("--ready-timeout", type=float, default=60)
    return result


def main(argv=None):
    arguments = parser().parse_args(argv)
    if not (0.05 <= arguments.step_timeout <= 45) or not (0.05 <= arguments.ready_timeout <= 60):
        parser().error("step-timeout must be0.05..45 and ready-timeout0.05..60 seconds")
    for name in ("x", "z", "camera_distance"):
        value = getattr(arguments, name)
        if value is not None:
            try:
                converted = struct.unpack("f", struct.pack("f", value))[0]
            except (OverflowError, struct.error):
                parser().error(f"{name} exceeds Float32")
            if not math.isfinite(value) or (name == "camera_distance" and converted <= 0):
                parser().error(f"{name} must be finite; camera distance must be positive")
    if any(not value or len(value) > 256 or "\x00" in value for value in (arguments.place_type, arguments.place_variant)):
        parser().error("placement type and variant require1..256 characters without NUL")
    return AcceptanceRunner(arguments).run()


if __name__ == "__main__":
    sys.exit(main())
