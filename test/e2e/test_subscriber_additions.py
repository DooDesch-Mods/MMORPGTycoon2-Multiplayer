"""Offline two-birth oracle and once-only clock intent checks; no native process."""
from __future__ import annotations

import copy
import argparse
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.e2e import subscriber_additions as additions


def engine(peer, ids, revision=8):
    frame = {key: 1 for key in additions.FRAME_ID}
    frame.update(snapshot_uuid="one-snapshot", revision=revision)
    cache = {"bound": True, "locked": False, "known_additions": len(ids) - 3,
             "pending_graphs": 0, "pending_graph_bytes": 0}
    creation = {"attempted": True, "confirmed": True, "unknown": False,
                "created_ids": ids[3:], "native_receipts": [{"state": "confirmed", "serialized_subscriber_id": item,
                    "subscriber_creation_attempted": True, "closed_world_readback_confirmed": True,
                    "shared_frame_binding": True} for item in ids[3:]],
                "frame_revision": revision, "frame_committed": True}
    return {"game_thread_ready": True, "session_connected": True, "in_game": True,
            "native_world": {"world_present": True, "mode_state": 4, "quitting": False,
                             "title_pending": False, "edits": 0, "saves": 0, "map_task_pending": False},
            "typed_actions": {"initialized": True, "started": True, "pending": False, "locked": False, "failed": 0,
                              "live_state": {"active": True, "locked": False, "confirmed": peer == "client-1", "cause": ""},
                              "network_frame_revision": revision,
                              "native_economy": {"day": {"frame": frame}, "subscribers": [{"id": item} for item in ids]},
                              "subscriber_additions": {"last_creation": creation if peer == "client-1" else None,
                                                       "cache": cache}}}


def pair(ids, revision=8):
    return {peer: engine(peer, ids, revision) for peer in additions.PEERS}


class BirthOracleTests(unittest.TestCase):
    baseline = [0, 1, 2]

    def test_two_native_ids_require_each_c1_receipt_and_host_ack(self):
        first = additions.classify_birth(pair([0, 1, 2, 3]), self.baseline, [])
        self.assertEqual([row["id"] for row in first], [3])
        self.assertEqual(first[0]["creation_frame_revision"], 8)
        self.assertEqual(first[0]["host_ack_cache"]["pending_graph_bytes"], 0)
        second = additions.classify_birth(pair([0, 1, 2, 3, 4], 9), self.baseline, [3])
        self.assertEqual([row["id"] for row in second], [4])
        self.assertEqual(second[0]["client_c1"]["serialized_subscriber_id"], 4)

    def test_host_lead_or_ack_pending_waits_without_claiming_success(self):
        values = pair([0, 1, 2, 3])
        values["client-1"] = engine("client-1", self.baseline)
        self.assertEqual(additions.classify_birth(values, self.baseline, []), [])
        values = pair([0, 1, 2, 3])
        values["host"]["typed_actions"]["subscriber_additions"]["cache"].update(pending_graphs=1, pending_graph_bytes=500)
        self.assertEqual(additions.classify_birth(values, self.baseline, []), [])
        values = pair([0, 1, 2, 3])
        values["client-1"]["typed_actions"]["subscriber_additions"]["last_creation"]["frame_committed"] = False
        self.assertEqual(additions.classify_birth(values, self.baseline, []), [])
        values = pair([0, 1, 2, 3])
        values["client-1"]["typed_actions"]["subscriber_additions"]["last_creation"]["native_receipts"][0]["shared_frame_binding"] = False
        with self.assertRaisesRegex(additions.AcceptanceError, "native receipts incomplete"):
            additions.classify_birth(values, self.baseline, [])

    def test_unknown_or_wrong_actual_ids_rejects(self):
        values = pair([0, 1, 2, 3])
        values["client-1"]["typed_actions"]["subscriber_additions"]["last_creation"]["unknown"] = True
        with self.assertRaisesRegex(additions.AcceptanceError, "Unknown"):
            additions.classify_birth(values, self.baseline, [])
        values = pair([0, 1, 2, 3])
        values["client-1"]["typed_actions"]["native_economy"]["subscribers"][-1]["id"] = 99
        with self.assertRaisesRegex(additions.AcceptanceError, "diverged"):
            additions.classify_birth(values, self.baseline, [])

    def test_frame_lag_waits_but_failed_controller_rejects(self):
        values = pair([0, 1, 2, 3])
        values["client-1"]["typed_actions"]["native_economy"]["day"]["frame"]["revision"] = 7
        self.assertEqual(additions.classify_birth(values, self.baseline, []), [])
        values = pair([0, 1, 2, 3])
        values["client-1"]["typed_actions"]["failed"] = 1
        with self.assertRaisesRegex(additions.AcceptanceError, "failed=1"):
            additions.classify_birth(values, self.baseline, [])


class ClockOnceTests(unittest.TestCase):
    def test_one_host_button_and_token_reads_until_confirmation(self):
        class Runner:
            def __init__(self):
                self.calls = []
                self.summary = {}

            def persist(self):
                pass

            def inspect(self, name, peer):
                selected = 1 if "confirmed" in name else 0
                return {"native_open_origin": {"uuid": "one-snapshot", "world_epoch": 4, "generation": 5},
                        "shared_time": {"runtime": {"phase": 3, "epoch": 2, "native_set_confirmed": selected,
                                                    "controller": {"write_locked": False,
                                                                   "observed": {"selected": selected, "revision": 6},
                                                                   "applied": {"selected": selected, "revision": 6},
                                                                   "processed_sequences": [7, 3]}}},
                        "clock": {"scale": selected if peer == "host" else 0}}

            def single(self, name, peer, tool, fields):
                self.calls.append((peer, tool, copy.deepcopy(fields)))
                if tool == "game_native_time_button":
                    return {"state": "waiting_for_host", "native": {"attempted": True, "returned": True}}
                return {"state": "waiting_for_host" if len(self.calls) == 2 else "confirmed",
                        "host_receipt": {"revision": 6}}

        runner = Runner()
        receipt = additions.set_clock_once(runner, 1, 5, now=lambda: 0)
        self.assertEqual(receipt["state"], "confirmed")
        self.assertEqual([tool for _, tool, _ in runner.calls].count("game_native_time_button"), 1)
        self.assertEqual(len({fields["acceptance_token"] for _, tool, fields in runner.calls if tool == "game_read_time_mouse"}), 1)

    def test_failed_pause_intent_is_never_replayed(self):
        class Runner:
            def __init__(self, args):
                self.summary = {"step": "preflight", "clock_intents": []}
                self.summary_path = Path("offline-summary.json")

            def persist(self):
                pass

            def inspect(self, name, peer):
                return {"shared_time": {"runtime": {"controller": {"observed": {"selected": 0}}}},
                        "clock": {"scale": 0}}

            def single(self, name, peer, tool):
                return {"existing_count": 3, "scan_complete": True}

            def checked(self, *args):
                pass

        calls = []

        def clock(runner, scale, wait_seconds):
            calls.append(scale)
            runner.summary["clock_intents"].append({"fields": {"scale": scale}, "state": "attempted"})
            if scale == 0:
                raise additions.AcceptanceError("pause effect unknown")
            runner.summary["clock_intents"][-1]["state"] = "confirmed"

        args = argparse.Namespace(setup=Path("fixture.json"), freeze=Path("freeze.json"),
                                  clock_wait_seconds=5, wait_seconds=10)
        cache = {"cache": {"known_additions": 0, "pending_graphs": 0}}
        with (patch.object(additions, "preflight"), patch.object(additions, "check_executable_identity"),
              patch.object(additions, "shared_ready", return_value={}),
              patch.object(additions, "actual_ids", return_value=[0, 1, 2]),
              patch.object(additions, "addition_status", return_value=cache),
              patch.object(additions, "observe_two", return_value=[{"id": 3}, {"id": 4}]),
              patch.object(additions, "set_clock_once", side_effect=clock)):
            self.assertEqual(additions.execute(args, runner_factory=Runner), 1)
        self.assertEqual(calls, [1, 0])


if __name__ == "__main__":
    unittest.main()
