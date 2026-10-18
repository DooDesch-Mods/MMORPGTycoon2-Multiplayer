"""Offline Region property proof checks; no game process or MCP mutation."""
from __future__ import annotations

import argparse
import copy
import hashlib
from pathlib import Path
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.e2e import region_properties as region


def args(operation="name"):
    return argparse.Namespace(region_id=67, operation=operation, name="Neuwald" if operation == "name" else None,
                              region_type="ocean" if operation == "type" else None)


def state():
    return {"id": 67, "preset": "Forest", "styles": ["Forest"] * 4,
            "random": [1, 2, 3, 4], "type_random": [5, 6, 7, 8],
            "scenery_count": 2, "scenery_sha256": "a" * 64, "grid_sha256": "b" * 64,
            "name": "Altwald", "type": 1, "owned": True, "status": 1, "level": 1, "max_level": 3}


def cash():
    return {"clock_day": 8, "cash": 500, "record_present": True, "record_day": 8,
            "record_cash": 500, "record_income": 20, "record_expense": 0,
            "line_counts": [0] * 20, "line_income": [0] * 20}


def engine(peer, confirmed=False, operation="name"):
    frame = {key: 1 for key in region.shared_ready.__globals__["FRAME_ID"]}
    frame["snapshot_uuid"] = "one-snapshot"
    ui = {"enabled": True, "pending_ui": False, "property_cause": "", "property_captured": int(confirmed and peer == "client-1"),
          "property_submitted": int(confirmed and peer == "client-1"), "property_rejected": 0,
          "property_last_action_uuid": "action" if confirmed and peer == "client-1" else "", "property_operations": []}
    controller = {"initialized": True, "started": True, "pending": False, "locked": False, "failed": 0,
                  "confirmed": int(confirmed), "host_revision": 2 if confirmed else 1,
                  "action_uuid": "action" if confirmed else "", "outcome": 1 if confirmed else 0,
                  "ack_mask": 3 if confirmed and peer == "host" else 0,
                  "live_state": {"active": True, "locked": False, "confirmed": peer == "client-1", "cause": ""},
                  "network_frame_revision": 1, "native_economy": {"day": {"frame": frame}}}
    if confirmed:
        ui["property_operations"] = [{"action_uuid": "action", "kind": region.KINDS[operation],
            "attempted": True, "returned": True, "confirmed": True, "retired": True,
            "before_name": "Altwald", "after_name": "Neuwald" if operation == "name" else ("Ocean" if operation == "type" else "Altwald"),
            "before_type": 1, "after_type": 0 if operation == "type" else 1, "before_owned": operation == "name",
            "after_owned": True, "before_cash": 500, "after_cash": 490 if operation == "purchase" else 500}]
        if operation == "type":
            row = ui["property_operations"][0]
            counts = {"building": 1, "scenery": 2, "path": 1, "path_node": 1,
                      "network_node": 1, "network_pipe": 1}
            for family, count in counts.items():
                row[family + "_count_before"] = count
                row[family + "_count_after"] = 0
            row.update(building_uids_sha256_before=hashlib.sha256(b"[1]").hexdigest(),
                       building_uids_sha256_after=hashlib.sha256(b"[]").hexdigest(),
                       removed_uids=list(range(1, 8)), removed_lookup_absent=True,
                       retired_shared_keys=["region:67:a"],
                       retired_keys_sha256=hashlib.sha256(b'["region:67:a"]').hexdigest(),
                       after_owned=False)
    money = cash()
    if confirmed and operation == "purchase":
        money.update(cash=490, record_cash=490, record_expense=10)
        money["line_counts"][7] = 1
        money["line_income"][7] = -10
    return {"game_thread_ready": True, "session_connected": True, "in_game": True,
            "native_world": {"world_present": True, "mode_state": 4, "quitting": False,
                             "title_pending": False, "edits": 0, "saves": 0, "map_task_pending": False},
            "capabilities": {"native_region_property_ui": True}, "ui_region": ui, "typed_actions": controller,
            "clock": {"scale": 0, "paused": True, "day": 8}, "simulation": {key: value for key, value in money.items() if key != "clock_day"}}


def engines(confirmed=False, operation="name"):
    return {peer: engine(peer, confirmed, operation) for peer in region.PEERS}


class GuardTests(unittest.TestCase):
    def test_host_seed_adoption_admits_only_rng_difference_before_and_none_after(self):
        for kind in ('name', 'purchase', 'type'):
            with self.subTest(kind=kind):
                regions = {peer: {**state(), 'owned': kind == 'name',
                    'position_resolves_region': True, 'region_position': [4, 0, 5]} for peer in region.PEERS}
                regions['client-1']['random'] = [9, 10, 11, 12]
                regions['client-1']['type_random'] = [13, 14, 15, 16]
                region.baseline(args(kind), engines(operation=kind), regions)
                regions['client-1']['grid_sha256'] = 'c' * 64
                with self.assertRaisesRegex(region.AcceptanceError, 'content Host-Client state differs'):
                    region.baseline(args(kind), engines(operation=kind), regions)
        after = {peer: {**state(), 'name': 'Neuwald'} for peer in region.PEERS}
        after['client-1']['random'] = [9, 10, 11, 12]
        with self.assertRaisesRegex(region.AcceptanceError, 'full state/RNG differs after authority'):
            region.confirmed(args(), engines(), state(), cash(), engines(True), after, 'action')

    def test_observation_reads_ui_sibling_and_reports_native_rejection(self):
        before, after = engines(), engines(True)
        region.require_healthy_observation(before, after, 'client-1')
        after['client-1']['ui_region'].update(property_rejected=1, property_cause='native owner differs')
        with self.assertRaisesRegex(region.AcceptanceError, 'native owner differs.*no retry'):
            region.require_healthy_observation(before, after, 'client-1')

    def test_name_requires_owned_and_exact_region_on_both_peers(self):
        before = engines()
        regions = {peer: state() for peer in region.PEERS}
        self.assertEqual(region.baseline(args(), before, regions)[0]["name"], "Altwald")
        regions["client-1"]["owned"] = False
        with self.assertRaisesRegex(region.AcceptanceError, "content Host-Client state differs before authority"):
            region.baseline(args(), before, regions)
        regions = {peer: state() for peer in region.PEERS}
        for value in regions.values():
            value["owned"] = False
        with self.assertRaisesRegex(region.AcceptanceError, "requires owned"):
            region.baseline(args(), before, regions)

    def test_unowned_land_purchase_and_native_hit_guards(self):
        before = engines()
        regions = {peer: {**state(), "owned": False, "status": 0,
                          "position_resolves_region": True, "region_position": [4.0, 0.0, 5.0]}
                   for peer in region.PEERS}
        region.baseline(args("purchase"), before, regions)
        self.assertEqual(region.intent(args("purchase"), regions["client-1"])["hit"], [4.0, 0.0, 5.0])
        regions["client-1"]["position_resolves_region"] = False
        with self.assertRaisesRegex(region.AcceptanceError, "native center hit"):
            region.baseline(args("purchase"), before, regions)
        regions["client-1"]["position_resolves_region"] = True
        for value in regions.values():
            value["type"] = 0
        with self.assertRaisesRegex(region.AcceptanceError, "Land type 1"):
            region.baseline(args("purchase"), before, regions)

    def test_type_requires_unowned_native_region_hit(self):
        before = engines()
        regions = {peer: {**state(), "owned": False, "status": 0,
                          "position_resolves_region": True, "region_position": [4.0, 0.0, 5.0]}
                   for peer in region.PEERS}
        region.baseline(args("type"), before, regions)
        regions["client-1"]["position_resolves_region"] = False
        with self.assertRaisesRegex(region.AcceptanceError, "native center hit"):
            region.baseline(args("type"), before, regions)

    def test_purchase_full_twenty_category_expense(self):
        before, after = cash(), cash()
        after.update(cash=490, record_cash=490, record_expense=10)
        after["line_counts"][7] = 1
        after["line_income"][7] = -10
        self.assertEqual(region.purchase_delta(before, after)["cost"], 10)
        changed = copy.deepcopy(after)
        changed["line_counts"][19] = 1
        with self.assertRaisesRegex(region.AcceptanceError, "category=19"):
            region.purchase_delta(before, changed)


class ConfirmationTests(unittest.TestCase):
    def test_name_requires_ack_once_and_full_region_rng(self):
        old, new = engines(), engines(True)
        before = state()
        after = {peer: {**state(), "name": "Neuwald"} for peer in region.PEERS}
        result = region.confirmed(args(), old, before, cash(), new, after, "action")
        self.assertEqual(result["host_ack_mask"], 3)
        after["client-1"]["type_random"] = [9, 6, 7, 8]
        with self.assertRaisesRegex(region.AcceptanceError, "full state/RNG differs"):
            region.confirmed(args(), old, before, cash(), new, after, "action")
        after["client-1"]["type_random"] = [5, 6, 7, 8]
        new["host"]["typed_actions"]["ack_mask"] = 1
        with self.assertRaisesRegex(region.AcceptanceError, "ACK mask 3"):
            region.confirmed(args(), old, before, cash(), new, after, "action")

    def test_purchase_requires_native_owned_state_and_cash_receipt(self):
        old, new = engines(), engines(True, "purchase")
        before = {**state(), "owned": False, "status": 0}
        after = {peer: {**before, "owned": True, "status": 1} for peer in region.PEERS}
        result = region.confirmed(args("purchase"), old, before, cash(), new, after, "action")
        self.assertEqual(result["purchase"]["expense_category"], 7)
        unset = {**before, 'level': 0, 'max_level': 0}
        initialized = {peer: {**unset, 'owned': True, 'status': 1, 'level': 1, 'max_level': 1}
                       for peer in region.PEERS}
        region.confirmed(args('purchase'), old, unset, cash(), new, initialized, 'action')
        new["client-1"]["ui_region"]["property_operations"][0]["attempted"] = False
        with self.assertRaisesRegex(region.AcceptanceError, "execution/retirement"):
            region.confirmed(args("purchase"), old, before, cash(), new, after, "action")


class TypeTests(unittest.TestCase):
    def test_all_six_manager_families_removed_and_lookup_null(self):
        old, new = engines(), engines(True, "type")
        before = {**state(), "owned": False, "status": 0}
        after = {peer: {**before, "type": 0, "name": "Ocean", "scenery_count": 0,
                        "random": [9, 8, 7, 6]} for peer in region.PEERS}
        result = region.confirmed(args("type"), old, before, cash(), new, after, "action")
        self.assertEqual(result["type_clear_contents"]["host"]["manager_counts_after"],
                         {family: 0 for family in region.CLEAR_FAMILIES})
        self.assertEqual(len(result["type_clear_contents"]["client-1"]["removed_uids"]), 7)

    def test_type_rejects_nonempty_manager_or_unproven_lookup(self):
        old, new = engines(), engines(True, "type")
        before = {**state(), "owned": False, "status": 0}
        after = {peer: {**before, "type": 0, "name": "Ocean", "scenery_count": 0} for peer in region.PEERS}
        row = new["client-1"]["ui_region"]["property_operations"][0]
        row["network_pipe_count_after"] = 1
        with self.assertRaisesRegex(region.AcceptanceError, "manager counts incomplete"):
            region.confirmed(args("type"), old, before, cash(), new, after, "action")
        row["network_pipe_count_after"] = 0
        row["removed_lookup_absent"] = False
        with self.assertRaisesRegex(region.AcceptanceError, "Lookup proof incomplete"):
            region.confirmed(args("type"), old, before, cash(), new, after, "action")


class FrameworkEntryTests(unittest.TestCase):
    def test_cli_keeps_selected_root_and_binary_and_legacy_defaults(self):
        command = ["--setup", "setup.json", "--freeze", "freeze.json", "--region-id", "68", "--operation", "purchase"]
        for extra, root, candidate in [([], region.ROOT / "_MPTest", None),
                (["--test-root", "owned/runs/test", "--candidate-binary", "frozen/MT2MultiplayerPlugin.dll"],
                 Path("owned/runs/test"), Path("frozen/MT2MultiplayerPlugin.dll"))]:
            with self.subTest(root=root), mock.patch.object(region, "execute", return_value=0) as execute:
                self.assertEqual(region.main(command + extra), 0)
                selected = execute.call_args.args[0]
                self.assertEqual((selected.test_root, selected.candidate_binary), (root, candidate))

    def test_exact_candidate_failure_precedes_any_engine_read_or_mutation(self):
        calls = []
        class Runner:
            def __init__(self, options):
                self.root = options.test_root
                self.summary = {"step": "preflight"}
                self.summary_path = "offline-region-property-summary"
            def inspect(self, *values):
                calls.append(values)
                raise AssertionError("engine access before candidate admission")
            def persist(self):
                pass
        options = args("purchase")
        options.setup, options.freeze = Path("setup.json"), Path("freeze.json")
        options.test_root = Path("owned/runs/test")
        options.candidate_binary = Path("frozen/MT2MultiplayerPlugin.dll")
        def reject(runner, freeze, setup, *, candidate_binary):
            self.assertEqual(runner.root, options.test_root)
            self.assertEqual(candidate_binary, options.candidate_binary)
            raise region.AcceptanceError("exact selected candidate differs")
        with mock.patch.object(region, "preflight", side_effect=reject):
            self.assertEqual(region.execute(options, Runner), 1)
        self.assertEqual(calls, [])


if __name__ == "__main__":
    unittest.main()
