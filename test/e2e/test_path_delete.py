"""Offline Path-delete DTO and single-mutation regressions; no game calls."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.e2e import path_delete as delete
from tools.e2e.run import FRAME_ID

ACTION = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"
TOKEN = "11111111-2222-3333-4444-555555555555"


def args(index=1):
    return argparse.Namespace(setup=Path("setup.json"), freeze=Path("freeze.json"), region_id=67, path_index=index)


def graph(peer, edges=((0, 1), (1, 2), (1, 3)), target=None):
    count = max(max(edge) for edge in edges) + 1
    road_indices = list(range(len(edges)))
    node_indices = list(range(count))
    if target is not None:
        ends = edges[target]
        removed = [end for end in ends if sum(end in edge for edge in edges) == 1]
        road_indices[target] = road_indices[-1]
        road_indices.pop()
        for end in removed:
            index = node_indices.index(end)
            node_indices[index] = node_indices[-1]
            node_indices.pop()
    rows = {"paths": [], "nodes": []}
    for index in road_indices:
        start, end = edges[index]
        rows["paths"].append([node_indices.index(start), node_indices.index(end),
                              [[float(start), 0.0, 0.0], [float(end), 0.0, 0.0]], [0.0, 1.0], [], []])
    for index in node_indices:
        rows["nodes"].append([[float(index), 0.0, 0.0],
                              [i for i, road in enumerate(road_indices) if index in edges[road]]])
    base = 1000 if peer == "host" else 2000
    return {"id": 67, "preset": "Forest", "styles": ["Forest"] * 4, "random": [1, 2, 3, 4],
            "scenery_count": 2, "scenery_sha256": "a" * 64, "grid_sha256": "b" * 64,
            "paths": len(rows["paths"]), "nodes": len(rows["nodes"]), "graph": rows,
            "graph_sha256": hashlib.sha256(json.dumps(rows).encode()).hexdigest(),
            "local_path_uids": [base + i for i in road_indices], "local_node_uids": [base + 100 + i for i in node_indices],
            "release_edits": [0] * 8, "global_random": [5, 6, 7, 8], "bugs_sha256": "c" * 64, "bug_count": 2}


def cash(refund=0):
    counts, income = [0] * 20, [0] * 20
    counts[6], income[6] = int(refund != 0), refund
    return {"clock_day": 8, "cash": 500 + refund, "record_present": True, "record_day": 8,
            "record_cash": 500 + refund, "record_income": 20 + refund, "record_expense": 0,
            "line_counts": counts, "line_income": income}


def admitted(fields):
    return {**fields, "attempted": True, "captured": True, "submitted": False, "confirmed": False,
            "ui_command_attempted": True, "ui_command_returned": True, "ui_command_accepted": True,
            "record_cleanup_confirmed": True, "exception": 0, "cleanup_exception": 0}


def engine(peer, done=False, before_graph=None, after_graph=None, fields=None, refund=10):
    frame = {key: 1 for key in FRAME_ID}
    frame["snapshot_uuid"] = "one-snapshot"
    controller = {"initialized": True, "started": True, "pending": False, "locked": False, "failed": 0,
                  "attempted": 3 + int(done), "confirmed": 3 + int(done), "host_revision": 7 + int(done),
                  "action_uuid": ACTION if done else "", "outcome": int(done), "ack_mask": 3 if done and peer == "host" else 0,
                  "live_state": {"active": True, "locked": False, "confirmed": peer == "client-1", "cause": ""},
                  "network_frame_revision": 1, "native_economy": {"day": {"frame": frame}}}
    ui = {"enabled": True, "pending": False, "captured": 2 + int(done and peer == "client-1"),
          "submitted": 2 + int(done and peer == "client-1"), "rejected": 0, "cause": "", "operations": [], "ui_jobs": {}}
    if done:
        removed = delete.survivor_topology(before_graph, after_graph, fields["path_index"])
        row = {"action": 13, "action_uuid": ACTION, "canonical_record_key": "action:" + ACTION,
               "attempted": True, "confirmed": True, "retired": True, "undo_confirmed": True, "cause": "", "purchases": 0,
               "cost": refund, "cash_before": 500, "cash_after": 500 + refund,
               "local_uids": [removed["road_uid"], *removed["node_uids"]]}
        for suffix, value in (("before", before_graph), ("after", after_graph)):
            for output, key in (("graph", "graph_sha256"), ("paths", "paths"), ("nodes", "nodes"),
                                ("global_random", "global_random"), ("region_random", "random"),
                                ("release", "release_edits"), ("bugs", "bugs_sha256"), ("bug_count", "bug_count")):
                row[output + "_" + suffix] = value[key]
        ui["operations"] = [row]
        if peer == "client-1":
            ui["ui_jobs"][fields["token"]] = {**admitted(fields), "submitted": True, "confirmed": True, "action_uuid": ACTION}
    money = cash(refund if done else 0)
    return {"game_thread_ready": True, "session_connected": True, "in_game": True,
            "native_world": {"world_present": True, "mode_state": 4, "quitting": False, "title_pending": False,
                             "edits": 0, "saves": 0, "map_task_pending": False},
            "capabilities": {"native_path_delete": True}, "ui_path": ui, "typed_actions": controller,
            "clock": {"scale": 0, "paused": True, "day": 8},
            "simulation": {key: value for key, value in money.items() if key != "clock_day"}}


def fixture(edges=((0, 1), (1, 2), (1, 3)), target=1, refund=10):
    fields = {"token": TOKEN, "id": 67, "path_index": target}
    old_graph = {peer: graph(peer, edges) for peer in delete.PEERS}
    new_graph = {peer: graph(peer, edges, target) for peer in delete.PEERS}
    old = {peer: engine(peer) for peer in delete.PEERS}
    new = {peer: engine(peer, True, old_graph[peer], new_graph[peer], fields, refund) for peer in delete.PEERS}
    for peer in delete.PEERS:new_graph[peer]["ui_path"] = copy.deepcopy(new[peer]["ui_path"])
    return args(target), old, old_graph, new, new_graph, fields


def verify(value):
    options, old, old_graph, new, new_graph, fields = value
    return delete.confirmed(options, old, old_graph, cash(), new, new_graph, fields, ACTION)


class ProofTests(unittest.TestCase):
    def test_actual_dtos_allow_distinct_local_uids_and_zero_one_two_orphans(self):
        for edges, target, count in [(((0, 1),), 0, 2), (((0, 1), (1, 2)), 0, 1),
                                     (((0, 1), (1, 2), (2, 0)), 1, 0)]:
            with self.subTest(count=count):
                value = fixture(edges, target)
                delete.baseline(value[0], value[1], value[2])
                result = verify(value)
                self.assertEqual(len(result["removed"]["host"]["node_uids"]), count)
                self.assertNotEqual(result["removed"]["host"]["road_uid"], result["removed"]["client-1"]["road_uid"])

    def test_later_global_samples_can_differ_with_equal_original_rng_and_unchanged_receipts(self):
        value = fixture()
        for index,peer in enumerate(delete.PEERS):
            value[2][peer]["global_random"] = [30+index,40,50,60]
            value[4][peer]["global_random"] = [70+index,80,90,100]
        result = verify(value)
        self.assertEqual(result["global_rng"]["original_before"], [5,6,7,8])
        self.assertTrue(result["global_rng"]["original_peer_states_equal"])
        self.assertNotEqual(result["global_rng"]["observation_after"]["host"], result["global_rng"]["observation_after"]["client-1"])

    def test_different_original_global_rng_is_rejected_even_with_unchanged_readback(self):
        value = fixture()
        value[3]["client-1"]["ui_path"]["operations"][0]["global_random_after"][0] += 1
        value[4]["client-1"]["ui_path"]["operations"] = copy.deepcopy(value[3]["client-1"]["ui_path"]["operations"])
        with self.assertRaisesRegex(delete.AcceptanceError, "Host/Client original receipt differs"):verify(value)

    def test_missing_original_rng_and_changed_retained_receipt_rejected(self):
        value = fixture()
        for peer in delete.PEERS:
            del value[3][peer]["ui_path"]["operations"][0]["global_random_before"]
            value[4][peer]["ui_path"]["operations"] = copy.deepcopy(value[3][peer]["ui_path"]["operations"])
        with self.assertRaisesRegex(delete.AcceptanceError, "global RNG receipt incomplete"):verify(value)
        value = fixture();value[4]["host"]["ui_path"]["operations"][0]["global_random_after"][0] += 1
        with self.assertRaisesRegex(delete.AcceptanceError, "receipt changed during later readback"):verify(value)

    def test_zero_cost_requires_unchanged_full_ledger(self):
        self.assertEqual(verify(fixture(refund=0))["refund"]["line_count_delta"], 0)

    def test_baseline_requires_existing_shared_graph_ledger_capability_and_revision(self):
        mutations = [lambda v: v[2]["client-1"].update(graph_sha256="e" * 64),
                     lambda v: setattr(v[0], "path_index", 99),
                     lambda v: v[1]["client-1"]["simulation"].update(record_income=99),
                     lambda v: v[1]["client-1"]["capabilities"].update(native_path_delete=False),
                     lambda v: v[1]["client-1"]["typed_actions"].update(host_revision=6)]
        for mutate in mutations:
            with self.subTest(mutate=mutate):
                value = fixture();mutate(value)
                with self.assertRaises(delete.AcceptanceError):
                    delete.baseline(value[0], value[1], value[2])

    def test_wrong_controller_ack_capture_retirement_undo_or_uid_receipt_rejected(self):
        mutations = [lambda v: v[3]["host"]["typed_actions"].update(ack_mask=1),
                     lambda v: v[3]["client-1"]["typed_actions"].update(host_revision=9),
                     lambda v: v[3]["host"]["typed_actions"].update(attempted=5),
                     lambda v: v[3]["host"]["ui_path"].update(captured=3),
                     lambda v: v[3]["client-1"]["ui_path"]["operations"][0].update(retired=False),
                     lambda v: v[3]["client-1"]["ui_path"]["operations"][0].update(undo_confirmed=False),
                     lambda v: v[3]["client-1"]["ui_path"]["operations"][0].update(local_uids=[2001, 2001]),
                     lambda v: v[3]["client-1"]["ui_path"]["operations"].append(copy.deepcopy(v[3]["client-1"]["ui_path"]["operations"][0])),
                     lambda v: v[3]["client-1"]["ui_path"]["operations"][0].update(action=7)]
        for mutate in mutations:
            with self.subTest(mutate=mutate):
                value = fixture();mutate(value)
                with self.assertRaises(delete.AcceptanceError):verify(value)

    def test_survivor_geometry_position_adjacency_and_uid_changes_rejected(self):
        mutations = [lambda g: g["graph"]["paths"][0][2][0].__setitem__(0, 99),
                     lambda g: g["graph"]["nodes"][0][0].__setitem__(0, 99),
                     lambda g: g["graph"]["nodes"][0].__setitem__(1, []),
                     lambda g: g["local_path_uids"].__setitem__(0, 9999),
                     lambda g: g["local_node_uids"].__setitem__(0, 9998)]
        for mutate in mutations:
            with self.subTest(mutate=mutate):
                value = fixture();mutate(value[4]["client-1"])
                with self.assertRaises(delete.AcceptanceError):verify(value)

    def test_all_twenty_refund_categories_and_unchanged_expense_day_are_checked(self):
        for category in range(20):
            value = fixture()
            for peer in delete.PEERS:value[3][peer]["simulation"]["line_counts"][category] += 1
            with self.subTest(category=category), self.assertRaisesRegex(delete.AcceptanceError, "20-category"):
                verify(value)
        for field in ("record_expense", "record_income"):
            value = fixture()
            for peer in delete.PEERS:value[3][peer]["simulation"][field] += 1
            with self.subTest(field=field), self.assertRaisesRegex(delete.AcceptanceError, "20-category"):
                verify(value)

    def test_native_rng_release_bugs_and_graph_receipts_bind_actual_readback(self):
        for field in ("global_random_after", "region_random_before", "release_after", "bugs_after", "bug_count_after", "graph_before", "cash_after"):
            value = fixture();row=value[3]["client-1"]["ui_path"]["operations"][0]
            row[field] = "mismatch"
            with self.subTest(field=field), self.assertRaises(delete.AcceptanceError):verify(value)

    def test_ui_capture_and_cleanup_acknowledgements_are_required(self):
        fields = fixture()[-1]
        for key in ("captured", "ui_command_returned", "ui_command_accepted", "record_cleanup_confirmed"):
            value=admitted(fields);value[key]=False
            with self.subTest(key=key), self.assertRaisesRegex(delete.AcceptanceError, "no replay"):
                delete.admission(value, fields)


class FakeRunner:
    def __init__(self, behavior="confirmed"):
        self.behavior=behavior;self.calls=[];self.checked_values=[];self.fields=None
        self.summary={"step":"fixture","passed":False};self.summary_path=Path("offline-path-delete-summary.json")
        self.value=fixture()
    def persist(self):pass
    def checked(self,name,value):self.checked_values.append((name,value))
    def inspect(self,name,peer):return copy.deepcopy(self.value[1][peer])
    def single(self,name,peer,tool,fields=None):
        self.calls.append((peer,tool,copy.deepcopy(fields)))
        if tool=="game_read_path":return copy.deepcopy(self.value[4 if self.fields else 2][peer])
        if tool!="game_native_path_delete":raise AssertionError(tool)
        self.fields=fields
        if self.behavior=="mutation-timeout":raise delete.AcceptanceError("MCP observation timed out; no replay")
        return admitted(fields)
    def job(self,name,peer,calls,remaining=None):
        current=copy.deepcopy(self.value[3][peer])
        if peer=="client-1":
            job=current["ui_path"]["ui_jobs"].pop(TOKEN);job.update(self.fields);current["ui_path"]["ui_jobs"][self.fields["token"]]=job
        if self.behavior=="timeout":
            current["typed_actions"].update(pending=True,outcome=0)
            for job in current["ui_path"]["ui_jobs"].values():job["confirmed"]=False
        if self.behavior=="unknown":current["typed_actions"].update(locked=True,outcome=3,cause="original inverse mismatch")
        return {"calls":[{"tool":"game_inspect_engine","state":"confirmed",
                          "result":{"structuredContent":{"ok":True,"result":current}}}]}


class RunnerTests(unittest.TestCase):
    def run_fixture(self, behavior, moments):
        runner=FakeRunner(behavior)
        with patch.object(delete,"preflight"), patch.object(delete,"check_executable_identity"), patch.object(delete,"log"), patch("builtins.print"):
            status=delete.execute(args(),runner_factory=lambda _:runner,now=lambda:next(moments))
        return runner,status
    def test_success_has_exactly_one_client_destroy_and_no_placement(self):
        runner,status=self.run_fixture("confirmed",iter([0,1,2,3]))
        self.assertEqual(status,0)
        mutations=[call for call in runner.calls if call[1]=="game_native_path_delete"]
        self.assertEqual(len(mutations),1);self.assertEqual(mutations[0][0],"client-1")
        self.assertTrue(all(call[1] in ("game_read_path","game_native_path_delete") for call in runner.calls))
        self.assertEqual(runner.summary["result"]["host_ack_mask"],3)
    def test_timeout_unknown_and_mutation_timeout_never_retry(self):
        for behavior,moments in [("timeout",[0,1,2,3,46]),("unknown",[0,1,2,3]),("mutation-timeout",[])]:
            with self.subTest(behavior=behavior):
                runner,status=self.run_fixture(behavior,iter(moments))
                self.assertEqual(status,1)
                self.assertEqual(sum(call[1]=="game_native_path_delete" for call in runner.calls),1)
                self.assertIn("no replay",runner.summary["error"]["cause"])


if __name__=="__main__":unittest.main()
