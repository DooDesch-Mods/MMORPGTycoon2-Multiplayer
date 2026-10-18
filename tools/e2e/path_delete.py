"""Prove one Client PathInfoWindow Destroy on an existing native Road."""
from __future__ import annotations

import argparse
import math
from pathlib import Path
import re
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools"))

from local_acceptance import AcceptanceError, AcceptanceRunner, compare_ledgers, extract_result, log, utc
from tools.e2e.run import PEERS, preflight, shared_ready
from tools.e2e.subscriber_additions import check_executable_identity

REGION_CONTENT = ("id", "preset", "styles", "scenery_count", "scenery_sha256", "grid_sha256")
NATIVE_STATE = REGION_CONTENT + ("random", "release_edits", "bugs_sha256", "bug_count")
RECEIPT_SHARED = ("cost", "cash_before", "cash_after", "graph_before", "graph_after", "paths_before", "paths_after",
                  "nodes_before", "nodes_after", "global_random_before", "global_random_after", "region_random_before",
                  "region_random_after", "release_before", "release_after", "bugs_before", "bugs_after",
                  "bug_count_before", "bug_count_after")


def digest(value):
    return isinstance(value, str) and re.fullmatch(r"[0-9a-f]{64}", value) is not None


def rng_state(value):
    return (isinstance(value, list) and len(value) == 4
            and all(type(word) is int and 0 <= word <= 18446744073709551615 for word in value))


def graph_state(value):
    graph = value.get("graph", {})
    roads, nodes = graph.get("paths"), graph.get("nodes")
    if (not isinstance(roads, list) or not isinstance(nodes, list) or not digest(value.get("graph_sha256"))
            or type(value.get("paths")) is not int or value["paths"] != len(roads)
            or type(value.get("nodes")) is not int or value["nodes"] != len(nodes)):
        raise AcceptanceError("native Road graph/count/digest readback incomplete")
    ids = []
    for name, rows in (("local_path_uids", roads), ("local_node_uids", nodes)):
        uids = value.get(name)
        if (not isinstance(uids, list) or len(uids) != len(rows)
                or any(type(uid) is not int or uid <= 0 for uid in uids) or len(set(uids)) != len(uids)):
            raise AcceptanceError(f"native {name} readback incomplete/duplicate")
        ids.extend(uids)
    if len(set(ids)) != len(ids):
        raise AcceptanceError("native Road/node UIDs overlap")
    adjacency = [[] for _ in nodes]
    for index, row in enumerate(roads):
        if (not isinstance(row, list) or len(row) != 6 or any(type(end) is not int or not 0 <= end < len(nodes) for end in row[:2])
                or row[0] == row[1] or not isinstance(row[2], list) or len(row[2]) < 2):
            raise AcceptanceError("native Road row/endpoints/waypoints invalid")
        for points in (row[2], row[4]):
            if not isinstance(points, list) or any(not valid_point(point) for point in points):
                raise AcceptanceError("native Road waypoint data invalid")
        for distances in (row[3], row[5]):
            if not isinstance(distances, list) or any(type(v) not in (int, float) or not math.isfinite(v) for v in distances):
                raise AcceptanceError("native Road distances invalid")
        for end in row[:2]:
            adjacency[end].append(index)
    for index, row in enumerate(nodes):
        if (not isinstance(row, list) or len(row) != 2 or not valid_point(row[0]) or not isinstance(row[1], list)
                or any(type(edge) is not int for edge in row[1]) or sorted(row[1]) != adjacency[index]):
            raise AcceptanceError("native node position/reciprocal adjacency invalid")
    if not rng_state(value.get("global_random")):
        raise AcceptanceError("native observation global RNG requires four uint64 words")
    if any(key not in value for key in NATIVE_STATE):
        raise AcceptanceError("native Region/RNG/release/bugs readback absent")
    return graph


def valid_point(point):
    return (isinstance(point, list) and len(point) == 3
            and all(type(v) in (int, float) and math.isfinite(v) for v in point))


def baseline(args, engines, graphs):
    shared_ready(engines)
    cash = compare_ledgers(engines["host"], engines["client-1"])
    for peer in PEERS:
        graph_state(graphs[peer])
        ui = engines[peer].get("ui_path", {})
        if (engines[peer].get("capabilities", {}).get("native_path_delete") is not True
                or ui.get("enabled") is not True or ui.get("pending") is not False
                or any(type(ui.get(key)) is not int or ui[key] < 0 for key in ("captured", "submitted", "rejected"))):
            raise AcceptanceError(f"{peer} native Path UI unavailable/pending")
        if graphs[peer]["id"] != args.region_id or not 0 <= args.path_index < graphs[peer]["paths"]:
            raise AcceptanceError(f"{peer} existing Road prerequisite/index differs")
    if (graphs["host"]["graph"] != graphs["client-1"]["graph"]
            or graphs["host"]["graph_sha256"] != graphs["client-1"]["graph_sha256"]
            or any(graphs["host"][key] != graphs["client-1"][key] for key in REGION_CONTENT)):
        raise AcceptanceError("Host/Client canonical graph/Region baseline differs")
    if engines["host"]["typed_actions"]["host_revision"] != engines["client-1"]["typed_actions"]["host_revision"]:
        raise AcceptanceError("Host/Client baseline action revision differs")
    return cash


def survivor_topology(before, after, target):
    old, new = graph_state(before), graph_state(after)
    roads, nodes = before["local_path_uids"], before["local_node_uids"]
    final_roads, final_nodes = after["local_path_uids"], after["local_node_uids"]
    removed_road = roads[target]
    orphan_nodes = {nodes[end] for end in old["paths"][target][:2] if len(old["nodes"][end][1]) == 1}
    if (set(final_roads) != set(roads) - {removed_road}
            or set(final_nodes) != set(nodes) - orphan_nodes):
        raise AcceptanceError("Road delete removed UID set or orphan node consequence differs")
    old_road_rows = {uid: [*[nodes[end] for end in row[:2]], *row[2:]] for uid, row in zip(roads, old["paths"])}
    new_road_rows = {uid: [*[final_nodes[end] for end in row[:2]], *row[2:]] for uid, row in zip(final_roads, new["paths"])}
    if any(new_road_rows[uid] != old_road_rows[uid] for uid in final_roads):
        raise AcceptanceError("Road delete changed survivor UID/geometry/endpoint identity")
    old_node_rows = {uid: [row[0], sorted(roads[edge] for edge in row[1] if roads[edge] != removed_road)]
                     for uid, row in zip(nodes, old["nodes"])}
    new_node_rows = {uid: [row[0], sorted(final_roads[edge] for edge in row[1])]
                     for uid, row in zip(final_nodes, new["nodes"])}
    if any(new_node_rows[uid] != old_node_rows[uid] for uid in final_nodes):
        raise AcceptanceError("Road delete changed survivor node position/adjacency")
    return {"road_uid": removed_road, "node_uids": sorted(orphan_nodes),
            "surviving_road_uids": final_roads, "surviving_node_uids": final_nodes}


def refund_delta(before, after, refund):
    if type(refund) is not int or refund < 0:
        raise AcceptanceError("Road delete original refund is invalid")
    expected = {**before, "cash": before["cash"] + refund, "record_cash": before["record_cash"] + refund,
                "record_income": before["record_income"] + refund,
                "line_counts": list(before["line_counts"]), "line_income": list(before["line_income"])}
    expected["line_counts"][6] += int(refund != 0)
    expected["line_income"][6] += refund
    if after != expected:
        raise AcceptanceError("Road delete full 20-category refund Ledger differs")
    return {"refund": refund, "category": 6, "line_count_delta": int(refund != 0)}


def admission(value, fields):
    if (any(value.get(key) is not True for key in ("attempted", "captured", "ui_command_attempted",
                                                  "ui_command_returned", "ui_command_accepted", "record_cleanup_confirmed"))
            or value.get("exception") != 0 or value.get("cleanup_exception") != 0
            or any(value.get(key) != wanted for key, wanted in fields.items()) or value.get("cause")):
        raise AcceptanceError("Road delete original UI capture/cleanup incomplete; no replay")


def healthy(before, after):
    for peer in PEERS:
        old, new = before[peer], after[peer]
        status, ui = new.get("typed_actions", {}), new.get("ui_path", {})
        if (status.get("locked") is True or status.get("outcome") == 3
                or status.get("failed") != old["typed_actions"]["failed"]
                or ui.get("rejected") != old["ui_path"]["rejected"]):
            raise AcceptanceError(f"{peer} Road delete failed/Unknown: Controller={status.get('cause')} UI={ui.get('cause')}; no replay")


def confirmed(args, before, graphs_before, cash_before, after, graphs_after, fields, action):
    shared_ready(after)
    healthy(before, after)
    cash_after = compare_ledgers(after["host"], after["client-1"])
    receipts, removed = {}, {}
    for peer in PEERS:
        old, new = before[peer], after[peer]
        status, previous = new["typed_actions"], old["typed_actions"]
        if (status.get("action_uuid") != action or status.get("outcome") != 1 or status.get("pending") is not False
                or any(status.get(key) != previous[key] + 1 for key in ("attempted", "confirmed", "host_revision"))):
            raise AcceptanceError(f"{peer} Road delete exact-once Controller/revision differs")
        ui, prior_ui = new["ui_path"], old["ui_path"]
        origin = int(peer == "client-1")
        if (ui.get("captured") != prior_ui["captured"] + origin or ui.get("submitted") != prior_ui["submitted"] + origin):
            raise AcceptanceError(f"{peer} Road delete original capture/submission count differs")
        rows = [row for row in ui.get("operations", []) if row.get("action_uuid") == action]
        if (len(rows) != 1 or rows[0].get("action") != 13 or rows[0].get("canonical_record_key") != "action:" + action
                or any(rows[0].get(key) is not True for key in ("attempted", "confirmed", "retired", "undo_confirmed"))
                or rows[0].get("purchases") != 0 or rows[0].get("cause")):
            raise AcceptanceError(f"{peer} native Road delete confirmation/retirement/Undo receipt incomplete")
        row = rows[0]
        removed[peer] = survivor_topology(graphs_before[peer], graphs_after[peer], args.path_index)
        deleted = [removed[peer]["road_uid"], *removed[peer]["node_uids"]]
        if (not isinstance(row.get("local_uids"), list) or len(row["local_uids"]) != len(deleted)
                or set(row["local_uids"]) != set(deleted) or row["local_uids"][0] != deleted[0]):
            raise AcceptanceError(f"{peer} native inverse removed UID receipt differs")
        for suffix, graph in (("before", graphs_before[peer]), ("after", graphs_after[peer])):
            if (row.get("graph_" + suffix) != graph["graph_sha256"]
                    or row.get("paths_" + suffix) != graph["paths"] or row.get("nodes_" + suffix) != graph["nodes"]):
                raise AcceptanceError(f"{peer} native Road delete graph/count receipt differs")
        if row["graph_before"] == row["graph_after"] or row.get("cash_before") != cash_before["cash"] or row.get("cash_after") != cash_after["cash"]:
            raise AcceptanceError(f"{peer} Road delete graph/refund receipt differs from readback")
        for keys, prefix in (("random", "region_random"), ("release_edits", "release"), ("bugs_sha256", "bugs"), ("bug_count", "bug_count")):
            if row.get(prefix + "_before") != graphs_before["host"][keys] or row.get(prefix + "_after") != graphs_after[peer][keys]:
                raise AcceptanceError(f"{peer} Road delete original native consequence differs: {keys}")
        # These states were read synchronously around Original and seeded from Host authority.
        # Separate graph queries sample the live global RNG at different times.
        if any(not rng_state(row.get("global_random_" + suffix)) for suffix in ("before", "after")):
            raise AcceptanceError(f"{peer} original operation global RNG receipt incomplete")
        retained = [item for item in graphs_after[peer].get("ui_path", {}).get("operations", [])
                    if item.get("action_uuid") == action]
        if len(retained) != 1 or retained[0] != row:
            raise AcceptanceError(f"{peer} original Road operation receipt changed during later readback")
        if any(graphs_before[peer][key] != graphs_after[peer][key] for key in REGION_CONTENT):
            raise AcceptanceError(f"{peer} Road delete changed unrelated Region/scenery/grid content")
        receipts[peer] = row
    if after["host"]["typed_actions"].get("ack_mask") != 3:
        raise AcceptanceError("Road delete Host lacks ACK mask 3")
    job = after["client-1"]["ui_path"].get("ui_jobs", {}).get(fields["token"], {})
    admission(job, fields)
    if job.get("submitted") is not True or job.get("confirmed") is not True or job.get("action_uuid") != action:
        raise AcceptanceError("Road delete UI token lacks native submission/completion")
    if (graphs_after["host"]["graph"] != graphs_after["client-1"]["graph"]
            or any(graphs_after["host"][key] != graphs_after["client-1"][key] for key in NATIVE_STATE)):
        raise AcceptanceError("Road delete final canonical topology/native consequences differ")
    if any(receipts["host"].get(key) != receipts["client-1"].get(key) for key in RECEIPT_SHARED):
        raise AcceptanceError("Road delete Host/Client original receipt differs")
    return {"action_uuid": action, "host_ack_mask": 3, "operations": receipts, "removed": removed,
            "ledger_before": cash_before, "ledger_after": cash_after,
            "refund": refund_delta(cash_before, cash_after, receipts["host"]["cost"]),
            "global_rng": {"original_before": receipts["host"]["global_random_before"],
                           "original_after": receipts["host"]["global_random_after"],
                           "observation_before": {peer: graphs_before[peer]["global_random"] for peer in PEERS},
                           "observation_after": {peer: graphs_after[peer]["global_random"] for peer in PEERS},
                           "original_peer_states_equal": True, "retained_original_receipts_unchanged": True}}


def execute(args, runner_factory=AcceptanceRunner, now=time.monotonic):
    args.test_root, args.players, args.cleanup, args.step_timeout = ROOT / "_MPTest", 2, False, 45
    runner = runner_factory(args)
    runner.summary["scope"] = "one Client native existing Road Destroy, Host order, ACK3, native inverse UIDs and full graph/Ledger; no mutation replay"
    try:
        preflight(runner, args.freeze, args.setup)
        check_executable_identity(runner)
        before = {peer: runner.inspect("path-delete-before-" + peer, peer) for peer in PEERS}
        graphs = {peer: runner.single("path-delete-graph-before-" + peer, peer, "game_read_path", {"id": args.region_id}) for peer in PEERS}
        cash = baseline(args, before, graphs)
        runner.checked("path-delete-common-baseline", {"engines": before, "graphs": graphs, "ledger": cash})
        fields = {"token": str(uuid.uuid4()), "id": args.region_id, "path_index": args.path_index}
        runner.summary["single_intent"] = {"state": "attempted", "fields": fields}
        runner.persist()
        admitted = runner.single("client-path-destroy-once", "client-1", "game_native_path_delete", fields)
        admission(admitted, fields)
        runner.summary["single_intent"].update(state="captured-before-host", admission=admitted)
        runner.persist()
        deadline, poll, action = now() + 45, 0, None
        while True:
            remaining = deadline - now()
            if remaining <= 2:
                raise AcceptanceError(f"Road delete observation timeout required<45s action={action}; no replay")
            poll += 1
            after = {}
            for peer in PEERS:
                remaining = deadline - now()
                if remaining <= 2:
                    raise AcceptanceError(f"Road delete observation timeout required<45s action={action}; no replay")
                receipt = runner.job(f"path-delete-observe-{poll}-{peer}", peer,
                                     [{"tool": "game_inspect_engine", "arguments": {}}], remaining=remaining)
                after[peer] = extract_result(receipt, "game_inspect_engine")
            healthy(before, after)
            job = after["client-1"]["ui_path"].get("ui_jobs", {}).get(fields["token"], {})
            if job.get("cause"):
                raise AcceptanceError(f"Road delete UI failed: {job['cause']}; no replay")
            action = job.get("action_uuid")
            if action and job.get("confirmed") is True and all(after[peer]["typed_actions"].get("pending") is False for peer in PEERS):
                try:
                    if str(uuid.UUID(action)) != action:
                        raise ValueError("noncanonical UUID")
                except (ValueError, TypeError) as error:
                    raise AcceptanceError("Road delete completion ActionUUID invalid; no replay") from error
                break
        final = {peer: runner.single("path-delete-graph-after-" + peer, peer, "game_read_path", {"id": args.region_id}) for peer in PEERS}
        result = confirmed(args, before, graphs, cash, after, final, fields, action)
        runner.checked("native-path-delete-two-peer-confirmed", {"result": result, "engines": after, "graphs": final})
        runner.summary.update(passed=True, state="confirmed-native-path-delete-two-peer", result=result, action_uuid=action)
    except Exception as error:
        runner.summary.update(passed=False, state="failed-native-path-delete-two-peer",
                              error={"step": runner.summary["step"], "cause": f"{type(error).__name__}: {error}",
                                     "behavior": "one Client Destroy maximum; effects retained; no mutation retry after timeout/failure/Unknown"})
        log("error", str(error))
    runner.summary["finished_utc"] = utc()
    runner.persist()
    print(runner.summary_path)
    return 0 if runner.summary["passed"] else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--setup", type=Path, required=True)
    parser.add_argument("--freeze", type=Path, required=True)
    parser.add_argument("--region-id", type=int, required=True)
    parser.add_argument("--path-index", type=int, required=True)
    args = parser.parse_args(argv)
    if not 0 <= args.region_id <= 2147483647 or not 0 <= args.path_index < 1000000:
        parser.error("region-id requires nonnegative int32; path-index requires 0..999999")
    return execute(args)


if __name__ == "__main__":
    raise SystemExit(main())
