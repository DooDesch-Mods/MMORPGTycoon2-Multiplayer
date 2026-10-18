"""Run one selected native two-peer acceptance scenario on an already prepared world."""
from __future__ import annotations

import argparse
import math
from pathlib import Path
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools" / "mcp"))
from local_acceptance import AcceptanceError, AcceptanceRunner, hash_file, log, read_json, ready_reason, utc
from runtime import Runtime, FRAMEWORK_PLUGIN_KIND, manifest_runtime_kind, candidate_binary_receipt
from tools.e2e.chat import run as run_chat

PEERS = ("host", "client-1")
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


def preflight(runner, freeze, setup, runtime_factory=Runtime, require_world=True, candidate_binary=None):
    frozen, prepared = read_json(freeze), read_json(setup)
    if prepared.get("passed") is not True:
        raise AcceptanceError(f"fixture setup is not confirmed: {setup}")
    expected_run = prepared.get("new_run_id", prepared.get("runtime_run_id"))
    expected_dll = frozen.get("dll_sha256")
    if not isinstance(expected_dll, str) or len(expected_dll) != 64:
        raise AcceptanceError(f"freeze DLL SHA256 missing or invalid: {freeze}")
    runner.load_manifests()
    if runner.summary["runtime_run_id"] != expected_run:
        raise AcceptanceError(f"owned runtime differs from fixture: actual={runner.summary['runtime_run_id']} required={expected_run}")
    identities = {}
    for peer in PEERS:
        manifest = runner.manifests[peer]
        bound = runtime_factory(runner.root).bind(str(manifest))
        if bound.get("process_identity_verified") is not True:
            raise AcceptanceError(f"{peer} exact PID/creation identity is not verified")
        identity = read_json(manifest)
        if manifest_runtime_kind(identity) == FRAMEWORK_PLUGIN_KIND or candidate_binary is not None:
            receipt = candidate_binary_receipt(identity, candidate_binary)
            actual = receipt["sha256"]
            if actual != expected_dll:
                raise AcceptanceError(f"{peer} selected candidate differs from freeze: actual={actual} required={expected_dll}")
            if manifest_runtime_kind(identity) != FRAMEWORK_PLUGIN_KIND:
                installed_path = Path(identity["runtime_root"]) / "game" / "MT2_Multiplayer.dll"
                if hash_file(installed_path) != expected_dll:
                    raise AcceptanceError(f"{peer} installed DLL differs from explicit candidate: {installed_path}")
            runner.checked("selected-candidate-" + peer, receipt)
        else:
            installed = identity.get("installed_binary_sha256", {}).get("MT2_Multiplayer.dll")
            dll = Path(identity["runtime_root"]) / "game" / "MT2_Multiplayer.dll"
            actual = hash_file(dll)
            if installed != expected_dll or actual != expected_dll:
                raise AcceptanceError(f"{peer} installed DLL differs: manifest={installed} file={actual} required={expected_dll}")
        identities[peer] = {"pid": identity["pid"], "process_start_time": identity["process_start_time"],
                            "dll_sha256": actual}
    runner.checked("exact-running-candidate", {"run_id": expected_run, "peers": identities})
    if require_world:
        engines = {peer: runner.inspect("preflight-" + peer, peer) for peer in PEERS}
        runner.checked("shared-gameplay-ready", shared_ready(engines))


def command(args):
    if args.scenario == "building-ui":
        if type(args.expected_cost) is not int or args.expected_cost <= 0:
            raise AcceptanceError("building-ui requires positive --expected-cost for exact native expense check")
        if args.x is None or args.z is None or not math.isfinite(args.x) or not math.isfinite(args.z):
            raise AcceptanceError("building-ui requires explicit finite --x and --z for a fresh placement site")
        selected = ["--test-root", str(getattr(args, "test_root", ROOT / "_MPTest"))]
        if getattr(args, "candidate_binary", None) is not None:
            selected += ["--candidate-binary", str(args.candidate_binary)]
        return [sys.executable, "-X", "utf8", str(ROOT / "tools" / "placement_ui_acceptance.py"),
                "--setup", str(args.setup), "--freeze", str(args.freeze), "--reuse-installed",
                "--type", args.type, "--variant", args.variant, "--expected-cost", str(args.expected_cost),
                "--x", str(args.x), "--z", str(args.z)] + selected
    raise AcceptanceError(f"unsupported native scenario: {args.scenario}")


def execute(args, runtime_factory=Runtime, process=subprocess.run):
    args.test_root = getattr(args, "test_root", ROOT / "_MPTest")
    args.players, args.cleanup, args.step_timeout = 2, False, 45
    runner = AcceptanceRunner(args)
    runner.summary["scope"] = ("two-peer native chat diagnostic send, recipient display and ACK; keyboard UI is separate"
                                if args.scenario == "chat-two" else
                                f"selected native two-peer E2E scenario: {args.scenario}; existing loaded world only")
    try:
        preflight(runner, args.freeze, args.setup, runtime_factory, require_world=args.scenario != "chat-two", candidate_binary=getattr(args, "candidate_binary", None))
        if args.scenario == "world-ready":
            runner.summary.update(passed=True, state="confirmed-shared-gameplay-ready")
            runner.summary["finished_utc"] = utc()
            runner.persist()
            print(runner.summary_path)
            return 0
        if args.scenario == "chat-two":
            if not math.isfinite(args.chat_wait_seconds) or not 2 < args.chat_wait_seconds <= 60:
                raise AcceptanceError("chat wait requires finite seconds >2 and <=60")
            runner.checkpoint("chat-two-directions")
            runner.summary["chat_messages"] = run_chat(runner, args.chat_wait_seconds)
            runner.summary.update(passed=True, state="confirmed-native-chat-two-directions")
            runner.summary["finished_utc"] = utc()
            runner.persist()
            print(runner.summary_path)
            return 0
        cmd = command(args)
        runner.checkpoint("dispatch-" + args.scenario)
        log_path = runner.directory / "scenario.log"
        record = {"scenario": args.scenario, "state": "attempted", "log_path": str(log_path), "started_utc": utc()}
        runner.summary["dispatch"] = record
        runner.persist()
        try:
            with log_path.open("w", encoding="utf-8") as stream:
                result = process(cmd, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, timeout=600, check=False)
        except subprocess.TimeoutExpired as error:
            record.update(state="observation-timeout", cause=str(error))
            runner.persist()
            raise AcceptanceError(f"scenario observation timed out after 600s; effects unconfirmed; no replay; log={log_path}") from error
        record.update(state="confirmed" if result.returncode == 0 else "failed", exit_code=result.returncode,
                      finished_utc=utc())
        output = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
        candidates = [Path(line.strip()) for line in output if line.strip().endswith("summary.json")]
        if len(candidates) != 1 or not candidates[0].resolve().is_relative_to((ROOT / "artifacts" / "acceptance").resolve()):
            raise AcceptanceError(f"scenario summary path missing or ambiguous: exit={result.returncode} log={log_path}")
        child_path = candidates[0].resolve()
        child = read_json(child_path)
        record["summary_path"] = str(child_path)
        runner.persist()
        if result.returncode != 0 or child.get("passed") is not True or child.get("runtime_run_id") != runner.summary["runtime_run_id"]:
            raise AcceptanceError(f"scenario failed or identity changed: exit={result.returncode} "
                                  f"child_passed={child.get('passed')} child_run={child.get('runtime_run_id')} "
                                  f"evidence={child_path} log={log_path}")
        runner.summary.update(passed=True, state="confirmed-selected-native-scenario")
    except Exception as error:
        runner.summary.update(passed=False, state="failed-selected-native-scenario",
                              error={"step": runner.summary["step"], "cause": f"{type(error).__name__}: {error}",
                                     "behavior": "remaining native mutations skipped; no retry"})
        log("error", str(error))
    runner.summary["finished_utc"] = utc()
    runner.persist()
    print(runner.summary_path)
    return 0 if runner.summary["passed"] else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", choices=("world-ready", "building-ui", "chat-two"), required=True)
    parser.add_argument("--setup", type=Path, required=True, help="passed native fixture summary for the current run")
    parser.add_argument("--freeze", type=Path, required=True, help="candidate freeze containing dll_sha256")
    parser.add_argument("--test-root", type=Path, default=ROOT / "_MPTest")
    parser.add_argument("--candidate-binary", type=Path, help="Exact selected mod artifact; required for Framework plugin mode")
    parser.add_argument("--type", default="Respawn")
    parser.add_argument("--variant", default="base")
    parser.add_argument("--expected-cost", type=int)
    parser.add_argument("--x", type=float, help="required for building-ui; select an unoccupied native site")
    parser.add_argument("--z", type=float, help="required for building-ui; select an unoccupied native site")
    parser.add_argument("--chat-wait-seconds", type=float, default=45, help="chat-two delivery bound for each direction")
    return execute(parser.parse_args(argv))


if __name__ == "__main__":
    raise SystemExit(main())
