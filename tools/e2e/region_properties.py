"""Prove one original Client Region property handler through Host authority."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools"))

from local_acceptance import AcceptanceError, AcceptanceRunner, compare_ledgers, ledger_state, log, utc
from tools.e2e.run import PEERS, preflight, shared_ready
from tools.e2e.subscriber_additions import check_executable_identity

REGION_FIELDS = ("id", "preset", "styles", "random", "scenery_count", "scenery_sha256", "grid_sha256",
                 "name", "type", "owned", "status", "level", "max_level", "type_random")
KINDS = {"name": 1, "type": 2, "purchase": 3}
CLEAR_FAMILIES = ("building", "scenery", "path", "path_node", "network_node", "network_pipe")


def region_state(value):
    if not isinstance(value, dict) or any(key not in value for key in REGION_FIELDS):
        raise AcceptanceError("native Region full property/RNG readback absent")
    return {key: value[key] for key in REGION_FIELDS}


def baseline(args, engines, regions):
    shared_ready(engines)
    cash = compare_ledgers(engines["host"], engines["client-1"])
    states = {peer: region_state(regions[peer]) for peer in PEERS}
    comparable = (lambda state: {key: value for key, value in state.items()
                             if key not in ("random", "type_random")})
    if comparable(states["host"]) != comparable(states["client-1"]) or states["host"]["id"] != args.region_id:
        raise AcceptanceError(f"Region ID/content Host-Client state differs before authority: {states}")
    before = states["host"]
    for peer in PEERS:
        ui = engines[peer].get("ui_region", {})
        if ui.get("enabled") is not True or ui.get("pending_ui") is not False or ui.get("property_cause"):
            raise AcceptanceError(f"{peer} Region property UI not idle/healthy: {ui}")
        if engines[peer].get("capabilities", {}).get("native_region_property_ui") is not True:
            raise AcceptanceError(f"{peer} original Region property handler capability absent")
    if args.operation == "name":
        if before["owned"] is not True or args.name == before["name"]:
            raise AcceptanceError("Region rename requires owned region and a different name")
    else:
        if before["owned"] is not False or regions["client-1"].get("position_resolves_region") is not True:
            raise AcceptanceError(f"Region {args.operation} requires unowned region and native center hit identity")
        if args.operation == "purchase" and before["type"] != 1:
            raise AcceptanceError("Region purchase requires native Land type 1")
        if args.operation == "type" and before["type"] == (1 if args.region_type == "land" else 0):
            raise AcceptanceError("Region type must differ from the current native Land/Ocean type")
    return before, cash


def intent(args, region):
    hit = [0, 0, 0] if args.operation == "name" else region.get("region_position")
    if not isinstance(hit, list) or len(hit) != 3 or any(type(v) not in (int, float) or not math.isfinite(v) or abs(v) > 1000000 for v in hit):
        raise AcceptanceError(f"native Region center hit unavailable or out of bounds: {hit}")
    return {"diagnostic_uuid": str(uuid.uuid4()), "region_id": args.region_id, "kind": args.operation,
            "name": args.name if args.operation == "name" else "",
            "type": (1 if args.region_type == "land" else 0) if args.operation == "type" else -1,
            "hit": hit}


def purchase_delta(before, after):
    cost = before["cash"] - after["cash"]
    if (cost < 0 or before["clock_day"] != after["clock_day"] or before["record_day"] != after["record_day"]
            or before["record_income"] != after["record_income"] or after["record_expense"] != before["record_expense"] + cost
            or after["record_cash"] != before["record_cash"] - cost):
        raise AcceptanceError(f"Region purchase native cash/record delta differs: cost={cost}")
    for category in range(20):
        count = before["line_counts"][category] + (1 if category == 7 else 0)
        income = before["line_income"][category] - (cost if category == 7 else 0)
        if after["line_counts"][category] != count or after["line_income"][category] != income:
            raise AcceptanceError(f"Region purchase native ledger category={category} differs")
    return {"cost": cost, "expense_category": 7, "line_count_delta": 1}


def type_clear_receipt(receipts):
    checked = {}
    empty_buildings = hashlib.sha256(b"[]").hexdigest()
    for peer in PEERS:
        row = receipts[peer]
        before = {name: row.get(name + "_count_before") for name in CLEAR_FAMILIES}
        after = {name: row.get(name + "_count_after") for name in CLEAR_FAMILIES}
        if (any(type(value) is not int or value < 0 for value in before.values())
                or any(type(value) is not int or value != 0 for value in after.values())):
            raise AcceptanceError(f"{peer} native Region ClearContents manager counts incomplete: before={before} after={after}")
        removed = row.get("removed_uids")
        if (not isinstance(removed, list) or any(type(uid) is not int or uid <= 0 for uid in removed)
                or removed != sorted(set(removed)) or len(removed) != sum(before.values())
                or row.get("removed_lookup_absent") is not True):
            raise AcceptanceError(f"{peer} native Region removed-UID census/Lookup proof incomplete: {removed}")
        before_sha, after_sha = (row.get("building_uids_sha256_" + suffix) for suffix in ("before", "after"))
        if (not isinstance(before_sha, str) or len(before_sha) != 64
                or after_sha != empty_buildings):
            raise AcceptanceError(f"{peer} native BuildingManager UID digest after ClearContents differs")
        keys = row.get("retired_shared_keys")
        digest = row.get("retired_keys_sha256")
        if (not isinstance(keys, list) or any(not isinstance(key, str) for key in keys)
                or keys != sorted(set(keys))
                or digest != hashlib.sha256(json.dumps(keys, ensure_ascii=False, separators=(",", ":")).encode("utf-8")).hexdigest()):
            raise AcceptanceError(f"{peer} Region shared-key retirement receipt differs")
        checked[peer] = {"manager_counts_before": before, "manager_counts_after": after,
                         "removed_uids": removed, "removed_lookup_absent": True,
                         "building_uids_sha256_before": before_sha, "building_uids_sha256_after": after_sha,
                         "retired_shared_keys": keys, "retired_keys_sha256": digest}
    if checked["host"]["manager_counts_before"] != checked["client-1"]["manager_counts_before"]:
        raise AcceptanceError("Host/Client Region ClearContents pre-counts differ")
    if checked["host"]["retired_keys_sha256"] != checked["client-1"]["retired_keys_sha256"]:
        raise AcceptanceError("Host/Client Region shared-key retirement differs")
    return checked


def confirmed(args, before_engines, before_state, before_cash, after_engines, after_regions, action):
    shared_ready(after_engines)
    after_cash = compare_ledgers(after_engines["host"], after_engines["client-1"])
    states = {peer: region_state(after_regions[peer]) for peer in PEERS}
    if states["host"] != states["client-1"] or states["host"]["id"] != args.region_id:
        raise AcceptanceError(f"Region full state/RNG differs after authority: {states}")
    after = states["host"]
    receipts = {}
    for peer in PEERS:
        old, new = before_engines[peer], after_engines[peer]
        controller = new["typed_actions"]
        prior = old["typed_actions"]
        if (controller.get("action_uuid") != action or controller.get("pending") is not False
                or controller.get("outcome") != 1 or controller.get("confirmed") != prior["confirmed"] + 1
                or controller.get("host_revision") != prior["host_revision"] + 1):
            raise AcceptanceError(f"{peer} Region Host order/once confirmation differs: {controller}")
        ui, previous = new["ui_region"], old["ui_region"]
        if (ui.get("property_captured") != previous["property_captured"] + (peer == "client-1")
                or ui.get("property_submitted") != previous["property_submitted"] + (peer == "client-1")
                or ui.get("property_rejected") != previous["property_rejected"]):
            raise AcceptanceError(f"{peer} Region original handler capture/submission counters differ: {ui}")
        rows = [row for row in ui.get("property_operations", []) if row.get("action_uuid") == action]
        if len(rows) != 1 or rows[0].get("kind") != KINDS[args.operation] or any(rows[0].get(key) is not True for key in
                                                                                 ("attempted", "returned", "confirmed", "retired")):
            raise AcceptanceError(f"{peer} original Region handler execution/retirement not confirmed: {rows}")
        receipts[peer] = rows[0]
    if after_engines["host"]["typed_actions"].get("ack_mask") != 3:
        raise AcceptanceError("Host Region action lacks ACK mask 3")
    if args.operation == "name":
        expected = {**before_state, "name": args.name}
        if after != expected or after_cash != before_cash:
            raise AcceptanceError("Region name changed unrelated native property/RNG or money")
    elif args.operation == "purchase":
        expected = {**before_state, "owned": True, "status": 1}
        # Native SetOwned initializes both levels to one if either level is unset.
        if before_state["level"] == 0 or before_state["max_level"] == 0:
            expected.update(level=1, max_level=1)
        if after != expected:
            raise AcceptanceError("Region purchase changed unrelated native property/RNG")
        purchase_delta(before_cash, after_cash)
    else:
        if after["type"] != (1 if args.region_type == "land" else 0) or after["owned"] is not False or after_cash != before_cash:
            raise AcceptanceError("Region Land/Ocean type or unchanged money differs")
    clear = type_clear_receipt(receipts) if args.operation == "type" else None
    for key in ("before_name", "after_name", "before_type", "after_type", "before_owned", "after_owned", "before_cash", "after_cash"):
        if receipts["host"].get(key) != receipts["client-1"].get(key):
            raise AcceptanceError(f"Host/Client Region authority receipt differs: {key}")
    if (receipts["host"]["before_name"] != before_state["name"] or receipts["host"]["after_name"] != after["name"]
            or receipts["host"]["before_type"] != before_state["type"] or receipts["host"]["after_type"] != after["type"]
            or receipts["host"]["before_owned"] != before_state["owned"] or receipts["host"]["after_owned"] != after["owned"]
            or receipts["host"]["before_cash"] != before_cash["cash"] or receipts["host"]["after_cash"] != after_cash["cash"]):
        raise AcceptanceError("Region original handler receipt differs from native before/after readback")
    return {"action_uuid": action, "region_before": before_state, "region_after": after, "ledger_before": before_cash,
            "ledger_after": after_cash, "purchase": purchase_delta(before_cash, after_cash) if args.operation == "purchase" else None,
            "type_clear_contents": clear, "operations": receipts, "host_ack_mask": 3}


def require_healthy_observation(before, after, peer):
    controller, prior = after[peer]["typed_actions"], before[peer]["typed_actions"]
    ui, previous_ui = after[peer]["ui_region"], before[peer]["ui_region"]
    if (controller.get("locked") is True or controller.get("failed", 0) > prior["failed"]
            or ui.get("property_rejected", 0) > previous_ui["property_rejected"]):
        raise AcceptanceError(f"{peer} Region property action failed/locked: {controller.get('cause')} "
                              f"UI={ui.get('property_cause')}; no retry")


def execute(args, runner_factory=AcceptanceRunner, now=time.monotonic):
    args.test_root = getattr(args, "test_root", ROOT / "_MPTest")
    args.players, args.cleanup, args.step_timeout = 2, False, 45
    runner = runner_factory(args)
    runner.summary["scope"] = "one original Region property handler, Host order, both native outcomes and full Region/cash readback"
    try:
        preflight(runner, args.freeze, args.setup, candidate_binary=getattr(args, "candidate_binary", None))
        check_executable_identity(runner)
        before = {peer: runner.inspect("region-property-before-" + peer, peer) for peer in PEERS}
        regions = {peer: runner.single("region-property-read-before-" + peer, peer, "game_read_region_preset",
                                       {"region_id": args.region_id}) for peer in PEERS}
        state, cash = baseline(args, before, regions)
        fields = intent(args, regions["client-1"])
        runner.checked("region-property-before", {"state": state, "cash": cash, "fields": fields})
        runner.summary["single_intent"] = {"state": "attempted", "fields": fields}
        runner.persist()
        admitted = runner.single("region-property-original-handler-once", "client-1", "game_native_region_property_ui", fields)
        if any(admitted.get(key) is not True for key in ("attempted", "handler_returned", "captured_once", "local_state_unchanged", "pending_intent")):
            raise AcceptanceError(f"original Region handler intent admission incomplete: {admitted}")
        if args.operation != "name" and any(admitted.get(key) is not True for key in ("press_admitted", "cleanup_returned")):
            raise AcceptanceError(f"original Region cursor Press/Release admission incomplete: {admitted}")
        runner.summary["single_intent"].update(state="captured-before-host", admission=admitted)
        runner.persist()
        deadline, poll, action = now() + args.wait_seconds, 0, None
        while now() < deadline:
            poll += 1
            after = {peer: runner.inspect(f"region-property-observe-{poll}-{peer}", peer) for peer in PEERS}
            for peer in PEERS:
                require_healthy_observation(before, after, peer)
            action = after["client-1"]["ui_region"].get("property_last_action_uuid")
            if action and action != before["client-1"]["ui_region"].get("property_last_action_uuid") and all(
                    after[peer]["typed_actions"].get("action_uuid") == action and
                    after[peer]["typed_actions"].get("pending") is False and
                    after[peer]["typed_actions"].get("outcome") == 1 for peer in PEERS):
                break
        else:
            raise AcceptanceError(f"Region property Host confirmation timeout after {args.wait_seconds}s; action={action}; no replay")
        runner.summary["action_uuid"] = action
        final_regions = {peer: runner.single("region-property-read-after-" + peer, peer, "game_read_region_preset",
                                             {"region_id": args.region_id}) for peer in PEERS}
        result = confirmed(args, before, state, cash, after, final_regions, action)
        runner.checked("region-property-two-peer-native-confirmed", result)
        runner.summary.update(passed=True, state="confirmed-native-region-property-two-peer", result=result)
    except Exception as error:
        runner.summary.update(passed=False, state="failed-native-region-property-two-peer",
                              error={"step": runner.summary["step"], "cause": f"{type(error).__name__}: {error}",
                                     "behavior": "one Region handler intent maximum; no mutation retry after failure or Unknown"})
        log("error", str(error))
    runner.summary["finished_utc"] = utc()
    runner.persist()
    print(runner.summary_path)
    return 0 if runner.summary["passed"] else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--setup", type=Path, required=True)
    parser.add_argument("--freeze", type=Path, required=True)
    parser.add_argument("--test-root", type=Path, default=ROOT / "_MPTest")
    parser.add_argument("--candidate-binary", type=Path, help="Exact selected mod artifact; required for Framework plugin mode")
    parser.add_argument("--region-id", type=int, required=True)
    parser.add_argument("--operation", choices=KINDS, required=True)
    parser.add_argument("--name")
    parser.add_argument("--type", dest="region_type", choices=("land", "ocean"))
    parser.add_argument("--wait-seconds", type=float, default=45)
    args = parser.parse_args(argv)
    if not 0 <= args.region_id <= 2147483647:
        parser.error("--region-id requires nonnegative int32")
    if not math.isfinite(args.wait_seconds) or not 0 < args.wait_seconds <= 120:
        parser.error("--wait-seconds requires finite value >0 and <=120")
    if args.operation == "name":
        if args.region_type is not None or not isinstance(args.name, str) or not 1 <= len(args.name.encode("utf-8")) <= 256 or "\0" in args.name:
            parser.error("name requires only --name with 1..256 UTF-8 bytes and no NUL")
    elif args.operation == "type":
        if args.name is not None or args.region_type is None:
            parser.error("type requires only --type land|ocean")
    elif args.name is not None or args.region_type is not None:
        parser.error("purchase takes neither --name nor --type")
    return execute(args)


if __name__ == "__main__":
    raise SystemExit(main())
