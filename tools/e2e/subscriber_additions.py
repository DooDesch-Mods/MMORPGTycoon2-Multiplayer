"""Prove two natural Host subscriber additions on one already prepared native world."""
from __future__ import annotations

import argparse
import math
from pathlib import Path
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools"))

from tools.e2e.run import FRAME_ID, PEERS, preflight, shared_ready
from local_acceptance import AcceptanceError, AcceptanceRunner, hash_file, log, read_json, ready_reason, utc


def actual_ids(engine):
    economy = engine.get("typed_actions", {}).get("native_economy", {})
    rows = economy.get("subscribers") if isinstance(economy, dict) else None
    if not isinstance(rows, list):
        raise AcceptanceError("native Economy subscriber array is absent")
    ids = [row.get("id") if isinstance(row, dict) else None for row in rows]
    if any(type(item) is not int or item < 0 for item in ids) or len(set(ids)) != len(ids):
        raise AcceptanceError(f"native Economy subscriber IDs invalid: {ids}")
    return ids


def addition_status(engine, peer):
    value = engine.get("typed_actions", {}).get("subscriber_additions")
    if not isinstance(value, dict) or not isinstance(value.get("cache"), dict):
        raise AcceptanceError(f"{peer} native subscriber cache readback absent")
    cache = value["cache"]
    if cache.get("bound") is not True or cache.get("locked") is not False:
        raise AcceptanceError(f"{peer} native subscriber cache unbound/locked: {cache}")
    return value


def running_frames(engines):
    frames = {}
    for peer in PEERS:
        engine = engines[peer]
        reason = ready_reason(engine, require_placement=False)
        if reason:
            raise AcceptanceError(f"{peer} native world degraded during subscriber observation: {reason}")
        controller = engine.get("typed_actions", {})
        live = controller.get("live_state", {})
        if (controller.get("initialized") is not True or controller.get("started") is not True
                or controller.get("pending") is not False or controller.get("locked") is not False
                or controller.get("failed") != 0 or live.get("active") is not True
                or live.get("locked") is not False or (peer == "client-1" and live.get("confirmed") is not True)
                or live.get("cause")):
            raise AcceptanceError(f"{peer} subscriber Controller/Bridge unhealthy failed={controller.get('failed')} "
                                  f"pending={controller.get('pending')} locked={controller.get('locked')} live={live}")
        revision = controller.get("network_frame_revision")
        economy = controller.get("native_economy", {})
        frame = economy.get("day", {}).get("frame", {}) if isinstance(economy, dict) else {}
        if (type(revision) is not int or revision <= 0 or not isinstance(frame, dict)
                or type(frame.get("revision")) is not int or not 0 < frame["revision"] <= revision
                or any(key not in frame for key in FRAME_ID)):
            raise AcceptanceError(f"{peer} subscriber gameplay Frame not adopted: network={revision} day={frame}")
        frames[peer] = {key: frame[key] for key in FRAME_ID}
    lineage = ("session", "snapshot_uuid", "epoch")
    if any(frames["host"][key] != frames["client-1"][key] for key in lineage):
        raise AcceptanceError(f"Host/Client subscriber Frame lineage differs: {frames}")
    return frames


def check_executable_identity(runner):
    receipts = {}
    for peer in PEERS:
        manifest = read_json(runner.manifests[peer])
        expected = manifest.get("installed_binary_sha256", {}).get("MT2.exe")
        actual = hash_file(Path(manifest["executable_path"]))
        if not isinstance(expected, str) or expected != actual:
            raise AcceptanceError(f"{peer} owned MT2.exe hash differs: manifest={expected} actual={actual}")
        receipts[peer] = actual
    if receipts["host"] != receipts["client-1"]:
        raise AcceptanceError(f"Host/Client executable hashes differ: {receipts}")
    runner.checked("exact-executable-binary", receipts)


def clock_fields(runner, scale):
    engine = runner.inspect(f"clock-{scale}-guard-host", "host")
    runtime = engine["shared_time"]["runtime"]
    controller = runtime["controller"]
    origin = engine["native_open_origin"]
    current = controller["observed"]
    if runtime.get("phase") != 3 or controller.get("write_locked") is not False:
        raise AcceptanceError(f"Host clock authority unavailable before scale={scale}")
    if current.get("selected") == scale:
        raise AcceptanceError(f"Host clock already has requested scale={scale}; no duplicate button")
    return {"acceptance_token": str(uuid.uuid4()), "snapshot_uuid": origin["uuid"], "scale": scale,
            "expected_epoch": runtime["epoch"], "expected_world_epoch": origin["world_epoch"],
            "expected_generation": origin["generation"], "expected_revision": current["revision"],
            "expected_request_sequence": controller["processed_sequences"][0] + 1}


def set_clock_once(runner, scale, wait_seconds, now=time.monotonic):
    fields = clock_fields(runner, scale)
    before = runner.inspect(f"clock-{scale}-setter-before", "host")["shared_time"]["runtime"]["native_set_confirmed"]
    runner.summary.setdefault("clock_intents", []).append({"fields": fields, "state": "attempted"})
    runner.persist()
    admitted = runner.single(f"clock-{scale}-button-once", "host", "game_native_time_button", fields)
    if admitted.get("state") != "waiting_for_host" or admitted.get("native", {}).get("attempted") is not True or admitted["native"].get("returned") is not True:
        raise AcceptanceError(f"Host native clock button admission incomplete scale={scale}: {admitted}")
    deadline, poll = now() + wait_seconds, 0
    while now() < deadline:
        poll += 1
        receipt = runner.single(f"clock-{scale}-receipt-{poll}", "host", "game_read_time_mouse",
                                {"acceptance_token": fields["acceptance_token"]})
        if receipt.get("state") == "confirmed":
            runner.summary["clock_intents"][-1].update(state="receipt-confirmed", receipt=receipt)
            runner.persist()
            while now() < deadline:
                host = runner.inspect(f"clock-{scale}-confirmed-host-{poll}", "host")
                client = runner.inspect(f"clock-{scale}-confirmed-client-{poll}", "client-1")
                if host["shared_time"]["runtime"]["native_set_confirmed"] != before + 1:
                    raise AcceptanceError(f"Host native clock setter count differs scale={scale}")
                if (host["shared_time"]["runtime"]["controller"]["observed"]["selected"] == scale
                        and client["shared_time"]["runtime"]["controller"]["applied"]["selected"] == scale
                        and host["clock"]["scale"] == scale and client["clock"]["scale"] == 0):
                    runner.summary["clock_intents"][-1].update(state="confirmed", receipt=receipt,
                                                                host_clock=host["clock"], client_clock=client["clock"])
                    runner.persist()
                    return receipt
                poll += 1
            raise AcceptanceError(f"Host/Client clock readback timeout scale={scale}; button not replayed")
        if receipt.get("state") != "waiting_for_host":
            raise AcceptanceError(f"Host native clock button failed scale={scale}: {receipt}")
    raise AcceptanceError(f"Host clock scale={scale} confirmation timeout; token={fields['acceptance_token']}; no replay")


def classify_birth(engines, baseline, confirmed):
    frames = running_frames(engines)
    host_ids, client_ids = (actual_ids(engines[peer]) for peer in PEERS)
    if host_ids[:len(baseline)] != baseline or client_ids[:len(baseline)] != baseline:
        raise AcceptanceError(f"native subscriber cohorts differ from baseline: host={host_ids} client={client_ids}")
    if client_ids != host_ids:
        if host_ids[:len(client_ids)] == client_ids:
            return []
        raise AcceptanceError(f"native subscriber cohorts diverged: host={host_ids} client={client_ids}")
    new_ids = host_ids[len(baseline):]
    if len(new_ids) > 2 or len(set(new_ids)) != len(new_ids):
        raise AcceptanceError(f"native subscriber additions moved outside two-birth scope: {new_ids}")
    host = addition_status(engines["host"], "host")
    client = addition_status(engines["client-1"], "client-1")
    receipt = client.get("last_creation")
    if not isinstance(receipt, dict):
        if new_ids:
            raise AcceptanceError(f"new native IDs lack Client C1 receipt: {new_ids}")
        return []
    if receipt.get("unknown") is True:
        raise AcceptanceError(f"Client native subscriber append is Unknown: {receipt}")
    created = receipt.get("created_ids", [])
    if not isinstance(created, list) or any(type(item) is not int for item in created):
        raise AcceptanceError(f"Client native created IDs invalid: {created}")
    fresh = [item for item in new_ids if item not in confirmed]
    if not fresh:
        return []
    if (receipt.get("attempted") is not True or receipt.get("confirmed") is not True
            or receipt.get("frame_committed") is not True or not all(item in created for item in fresh)):
        return []
    native = receipt.get("native_receipts")
    if (not isinstance(native, list) or len(native) != len(created)
            or any(not isinstance(row, dict) or row.get("state") != "confirmed"
                   or row.get("serialized_subscriber_id") != created[index]
                   or row.get("subscriber_creation_attempted") is not True
                   or row.get("closed_world_readback_confirmed") is not True
                   or row.get("shared_frame_binding") is not True for index, row in enumerate(native))):
        raise AcceptanceError(f"Client C1 native receipts incomplete for IDs={fresh}: {native}")
    revision = receipt.get("frame_revision")
    if type(revision) is not int or revision <= 0 or any(frames[peer]["revision"] < revision for peer in PEERS):
        return []
    cache = host["cache"]
    if cache.get("known_additions") != len(new_ids) or cache.get("pending_graphs") != 0 or cache.get("pending_graph_bytes") != 0:
        return []
    return [{"id": item, "client_c1": native[created.index(item)], "creation_frame_revision": revision,
             "adopted_frames": frames, "host_ack_cache": cache, "client_creation": receipt} for item in fresh]


def observe_two(runner, baseline, wait_seconds, now=time.monotonic):
    deadline, poll, confirmed = now() + wait_seconds, 0, []
    while now() < deadline:
        poll += 1
        engines = {peer: runner.inspect(f"subscriber-{poll}-{peer}", peer) for peer in PEERS}
        for birth in classify_birth(engines, baseline, [row["id"] for row in confirmed]):
            confirmed.append(birth)
            runner.checked(f"native-subscriber-birth-{len(confirmed)}", birth)
        if len(confirmed) == 2:
            return confirmed
    raise AcceptanceError(f"natural subscriber observation timed out after {wait_seconds}s; confirmed={len(confirmed)} required=2; no creation replay")


def execute(args, runner_factory=AcceptanceRunner):
    args.test_root, args.players, args.cleanup, args.step_timeout = ROOT / "_MPTest", 2, False, 45
    runner = runner_factory(args)
    runner.summary["scope"] = "two natural Host subscriber births, native Client C1, committed Frame and Host ACK cache"
    started = False
    try:
        preflight(runner, args.freeze, args.setup)
        check_executable_identity(runner)
        engines = {peer: runner.inspect(f"subscriber-baseline-{peer}", peer) for peer in PEERS}
        frames = shared_ready(engines)
        baseline = actual_ids(engines["host"])
        if not baseline or actual_ids(engines["client-1"]) != baseline:
            raise AcceptanceError(f"native baseline subscriber cohorts are empty or differ: {baseline}")
        for peer in PEERS:
            status = addition_status(engines[peer], peer)
            if status["cache"].get("known_additions") != 0 or status["cache"].get("pending_graphs") != 0:
                raise AcceptanceError(f"{peer} subscriber baseline cache not empty: {status['cache']}")
        census = runner.single("subscriber-baseline-census", "host", "game_read_subscriber_candidates")
        if census.get("existing_count") != len(baseline) or census.get("scan_complete") is not True:
            raise AcceptanceError(f"Host native subscriber census differs from baseline count={len(baseline)}: {census}")
        runner.checked("native-subscriber-baseline", {"ids": baseline, "frames": frames, "census": census})
        host_clock = engines["host"]["shared_time"]["runtime"]["controller"]["observed"]["selected"]
        if host_clock != 0 or engines["client-1"]["clock"]["scale"] != 0:
            raise AcceptanceError(f"native clocks must begin paused Host={host_clock}")
        set_clock_once(runner, 1, args.clock_wait_seconds)
        started = True
        runner.summary["births"] = observe_two(runner, baseline, args.wait_seconds)
        set_clock_once(runner, 0, args.clock_wait_seconds)
        started = False
        final = {peer: runner.inspect(f"subscriber-final-{peer}", peer) for peer in PEERS}
        frames = running_frames(final)
        ids = actual_ids(final["host"])
        if ids != baseline + [row["id"] for row in runner.summary["births"]] or actual_ids(final["client-1"]) != ids:
            raise AcceptanceError(f"final native subscriber IDs changed: {ids}")
        if final["client-1"]["clock"]["scale"] != 0:
            raise AcceptanceError("Client native clock is not zero at final readback")
        if addition_status(final["host"], "host")["cache"]["pending_graphs"] != 0:
            raise AcceptanceError("Host subscriber ACK cache regained pending graphs")
        runner.checked("two-native-subscribers-final", {"ids": ids, "frames": frames,
                                                        "host_ack_cache": final["host"]["typed_actions"]["subscriber_additions"]["cache"],
                                                        "client_clock": final["client-1"]["clock"]})
        runner.summary.update(passed=True, state="confirmed-two-natural-native-subscribers")
    except Exception as error:
        runner.summary.update(passed=False, state="failed-two-natural-native-subscribers",
                              error={"step": runner.summary["step"], "cause": f"{type(error).__name__}: {error}",
                                     "behavior": "no subscriber mutation or replay; clock compensation only after confirmed start"})
        log("error", str(error))
        start_intents = [item for item in runner.summary.get("clock_intents", []) if item["fields"]["scale"] == 1]
        stop_intents = [item for item in runner.summary.get("clock_intents", []) if item["fields"]["scale"] == 0]
        if not stop_intents and (started or (start_intents and start_intents[-1]["state"] in ("receipt-confirmed", "confirmed"))):
            try:
                set_clock_once(runner, 0, args.clock_wait_seconds)
                runner.summary["clock_compensation"] = "confirmed-paused-once"
            except Exception as pause_error:
                runner.summary["clock_compensation"] = {"state": "failed-or-unknown", "cause": str(pause_error),
                                                         "behavior": "no pause retry"}
                log("error", f"Host clock pause failed after subscriber observation: {pause_error}")
        elif stop_intents:
            runner.summary["clock_compensation"] = {"state": stop_intents[-1]["state"],
                                                     "behavior": "pause already attempted; no retry"}
    runner.summary["finished_utc"] = utc()
    runner.persist()
    print(runner.summary_path)
    return 0 if runner.summary["passed"] else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--setup", type=Path, required=True, help="passed normal two-peer fixture summary")
    parser.add_argument("--freeze", type=Path, required=True, help="current candidate freeze with DLL SHA256")
    parser.add_argument("--wait-seconds", type=float, default=180, help="bound for two natural births")
    parser.add_argument("--clock-wait-seconds", type=float, default=45)
    args = parser.parse_args(argv)
    if any(not math.isfinite(value) or value <= 0 or value > limit for value, limit in
           ((args.wait_seconds, 600), (args.clock_wait_seconds, 65))):
        parser.error("wait bounds must be finite and positive (births <=600s, clock <=65s)")
    return execute(args)


if __name__ == "__main__":
    raise SystemExit(main())
