"""Offline false-success regressions for the actual Remove history acceptance predicates."""
from pathlib import Path
import copy
import hashlib
import importlib.util
import struct
import tempfile
import unittest
import zlib

HERE = Path(__file__).resolve().parent
ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("remove_restore_acceptance", ROOT / "tools/e2e/building_remove_restore.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)

RUN = "11111111-1111-4111-8111-111111111111"
PLACED = "22222222-2222-4222-8222-222222222222"
REMOVED = "33333333-3333-4333-8333-333333333333"
UNDONE = "44444444-4444-4444-8444-444444444444"
SNAPSHOT = "55555555-5555-4555-8555-555555555555"
KEY = "action:" + PLACED


def ledger():
    return {"clock_day": 2, "cash": 88400, "record_present": True, "record_day": 2,
            "record_cash": 88400, "record_income": 400, "record_expense": 1000,
            "line_counts": [2 if i == 6 else 0 for i in range(20)],
            "line_income": [-600 if i == 6 else 0 for i in range(20)]}


def native_ledger(value):
    return {"flat_day": value["clock_day"], "record_present": 1,
            **{name: copy.deepcopy(value[name]) for name in ("cash", "record_day", "record_cash", "record_income", "record_expense", "line_counts", "line_income")}}


def row(revoked):
    return {"record_key": KEY, "snapshot_id": SNAPSHOT, "local_uid": 3176, "entry_id": 1,
            "registry_generation": 1, "revoked": revoked, "unknown": False, "type": "SmallInn", "variant": "base",
            "x": 8110.0, "y": 79.79767608642578, "z": 8610.0, "rotation": [0, 0, 0, 1], "scale": [1, 1, 1]}


def remove_status():
    native = {"sequence": 1, "attempted": True, "returned": True, "confirmed": True, "target_retired": True,
              "undo_confirmed": True, "selection_confirmed": True, "unknown_locked": False, "error": "", "seh_code": 0}
    receipt = {"token": REMOVED, "record_key": KEY, "snapshot_id": SNAPSHOT, "world_epoch": 2, "world_generation": 3,
               "sequence": 1, "confirmed": True, "binding_revoked": True, "error": "", "result": native}
    retired = {"token": REMOVED, "sequence": 1, "confirmed": True, "error": "", "result": {**native, "retirement_confirmed": True},
               "effects": {"attempted": True, "confirmed": True, "unknown": False, "releases": 1, "cause": "info: own allocations released",
                           "frees": [{"delete_attempted": 1, "delete_returned": 1, "sized_attempted": 1, "sized_returned": 1, "free_attempted": 1, "free_returned": 1, "seh_code": 0}]}}
    return {"initialized": True, "unknown_locked": False, "receipts": {REMOVED: receipt}, "retirements": {REMOVED: retired}}


def engine(money, action, revision, count):
    frame = {"session": 1, "snapshot_uuid": SNAPSHOT, "epoch": 2, "revision": 5, "base_action_revision": revision, "time_revision": 1}
    return {"game_thread_ready": True, "session_connected": True, "in_game": True,
            "native_world": {"world_present": True, "mode_state": 4, "quitting": False, "title_pending": False, "edits": 0, "saves": 0, "map_task_pending": False},
            "clock": {"scale": 0, "paused": True, "day": 2}, "simulation": {k: copy.deepcopy(v) for k, v in money.items() if k != "clock_day"},
            "capabilities": {"native_building_history_ui": True}, "native_open_origin": {"world_epoch": 2, "generation": 3},
            "native_remove": remove_status(), "typed_actions": {"initialized": True, "started": True, "pending": False, "locked": False,
            "failed": 0, "cause": "", "action_uuid": action, "outcome": 1, "host_revision": revision, "attempted": count, "confirmed": count, "ack_mask": 3,
            "live_state": {"active": True, "locked": False, "confirmed": True, "cause": ""}, "network_frame_revision": 5,
            "native_economy": {"day": {"frame": frame}}}}


def snapshots():
    before, after = {}, {}
    initial = ledger()
    expected = runner.cash_inverse(initial, 400, True)
    for peer in runner.PEERS:
        ui = {"snapshot_uuid": SNAPSHOT, "undo_target_action_uuid": REMOVED, "active_graph": False, "ui_hook_calls": 3,
              "ui_intents_queued": 2, "undo_depth": 2, "redo_depth": 0}
        before[peer] = {"engine": engine(initial, REMOVED, 4, 2), "ui": ui, "bindings": {"bindings": [row(True)]}}
        before[peer]["original_holder"] = runner.holder_receipt(before[peer]["engine"], REMOVED, KEY, row(True), peer)
        after_ui = {**ui, "undo_depth": 1, "redo_depth": 1, "redo_target_action_uuid": REMOVED}
        if peer == "client-1":
            after_ui.update(ui_hook_calls=4, ui_intents_queued=3, last_action_uuid=UNDONE, last_target_action_uuid=REMOVED, last_direction="undo")
        after[peer] = {"engine": engine(expected, UNDONE, 5, 3), "ui": after_ui, "bindings": {"bindings": [row(False)]}}
    return before, after, expected


def developers(money):
    return {"observed": True, "fixture_locked": False, "time_scale": 0, "snapshot_uuid": SNAPSHOT,
            "world_epoch": 2, "generation": 3, "origin_sha256": "a" * 64, "origin_bytes": 234000,
            "ledger": native_ledger(money), "business_count": 1, "lounge_count": 1, "count_settled": True,
            "indoor_count": 50, "outdoor_count": 0,
            "developers": [{"id": n, "slot": n, "state": 0, "active": n == 0, "employed": n == 0,
                            "on_map": False, "job_link": False, "position": [0, 0, 0]} for n in range(50)],
            "jobs": {"queue_count": 1, "jobs": [{"queue_index": 0, "type": 1, "valid": True, "region_present": True,
                        "developer_present": False, "building_present": True, "building_proxy_present": True}]},
            "job_building_targets": {"observed": True, "targets": [],
                "building_binding_cause": "Queue contains expired Building weak targets; no live shared Building binding was observed",
                "expired_targets": [{"queue_index_diagnostic": 0,
                "cached_receiver_present": True, "weak_proxy_present": True, "weak_target_alive": False, "building_binding_observed": False}]}}


def set_path(value, path, replacement):
    target = value
    for key in path[:-1]:
        target = target[key]
    target[path[-1]] = replacement


class RemoveRestoreOracleTests(unittest.TestCase):
    def test_real_phase_semantics_accept_restore_not_placement_inverse(self):
        before, after, expected = snapshots()
        self.assertEqual(runner.phase_complete(after, before, UNDONE, REMOVED, KEY, "undo", "client-1", expected), expected)
        self.assertNotEqual(REMOVED, PLACED)
        self.assertEqual(after["host"]["bindings"]["bindings"][0]["revoked"], False)

    def test_phase_rejects_false_success(self):
        changes = [
            (("host", "engine", "typed_actions", "ack_mask"), 1),
            (("client-1", "engine", "typed_actions", "attempted"), 4),
            (("client-1", "engine", "typed_actions", "confirmed"), 2),
            (("host", "engine", "typed_actions", "host_revision"), 6),
            (("host", "engine", "typed_actions", "outcome"), True),
            (("host", "engine", "typed_actions", "locked"), True),
            (("host", "engine", "typed_actions", "action_uuid"), REMOVED),
            (("host", "engine", "simulation", "line_income"), [0] * 20),
            (("host", "ui", "undo_depth"), 2),
            (("client-1", "ui", "ui_hook_calls"), 5),
            (("client-1", "ui", "last_target_action_uuid"), PLACED),
            (("host", "ui", "redo_target_action_uuid"), PLACED),
            (("client-1", "bindings", "bindings", 0, "revoked"), True),
            (("client-1", "bindings", "bindings", 0, "local_uid"), 999),
            (("client-1", "bindings", "bindings", 0, "entry_id"), 999),
            (("client-1", "bindings", "bindings", 0, "y"), float("nan")),
            (("client-1", "bindings", "bindings", 0, "record_key"), "action:" + REMOVED),
        ]
        for path, value in changes:
            with self.subTest(path=path):
                before, after, expected = snapshots()
                set_path(after, path, value)
                with self.assertRaises(runner.AcceptanceError):
                    runner.phase_complete(after, before, UNDONE, REMOVED, KEY, "undo", "client-1", expected)

    def test_holder_unknown_missing_origin_and_unfreed_are_rejected(self):
        changes = [(("native_remove", "unknown_locked"), True),
                   (("native_remove", "receipts", REMOVED, "world_epoch"), 4),
                   (("native_remove", "retirements", REMOVED, "sequence"), 2),
                   (("native_remove", "retirements", REMOVED, "effects", "confirmed"), False),
                   (("native_remove", "retirements", REMOVED, "effects", "frees", 0, "free_returned"), 0),
                   (("native_remove", "receipts", REMOVED, "result", "seh_code"), True)]
        for path, value in changes:
            with self.subTest(path=path):
                engine_value = engine(ledger(), REMOVED, 4, 2)
                set_path(engine_value, path, value)
                with self.assertRaises(runner.AcceptanceError):
                    runner.holder_receipt(engine_value, REMOVED, KEY, row(True), "host")

    def test_expired_job_is_measured_not_repaired_or_omitted(self):
        original = developers(ledger())
        runner.validate_developers(original, original, ledger(), "host")
        changes = [(("job_building_targets", "expired_targets", 0, "weak_target_alive"), True),
                   (("jobs", "queue_count"), 0), (("developers",), []),
                   (("developers", 1, "id"), 0), (("time_scale",), False)]
        for path, value in changes:
            with self.subTest(path=path):
                actual = copy.deepcopy(original)
                set_path(actual, path, value)
                with self.assertRaises(runner.AcceptanceError):
                    runner.validate_developers(actual, original, ledger(), "host")

    def test_cash_inverse_signed_category6_and_zero_no_event(self):
        initial = ledger()
        restored = runner.cash_inverse(initial, 400, True)
        self.assertEqual(restored["cash"], 88000)
        self.assertEqual(restored["record_income"], 0)
        self.assertEqual(restored["record_expense"], 1000)
        self.assertEqual(restored["line_counts"][6], 3)
        redone = runner.cash_inverse(restored, 400, False)
        self.assertEqual(redone["cash"], initial["cash"])
        self.assertEqual(redone["record_income"], initial["record_income"])
        self.assertEqual(redone["line_counts"][6], 4)
        self.assertEqual(runner.cash_inverse(initial, 0, True), initial)
        for bad in (True, -1, 2**31):
            with self.subTest(refund=bad), self.assertRaises(runner.AcceptanceError):
                runner.cash_inverse(initial, bad, True)

    def test_missing_client_never_counts_as_pair(self):
        before, after, expected = snapshots()
        after.pop("client-1")
        with self.assertRaises((runner.AcceptanceError, KeyError)):
            runner.phase_complete(after, before, UNDONE, REMOVED, KEY, "undo", "client-1", expected)

    def test_stale_baseline_must_not_readmit_completed_inverse(self):
        before, after, _ = snapshots()
        base = {"final_status": {peer: before[peer]["engine"] for peer in runner.PEERS}}
        originals = {peer: row(False) for peer in runner.PEERS}
        with self.assertRaisesRegex(runner.AcceptanceError, "ledger changed|Controller changed"):
            runner.require_initial(after, base, REMOVED, KEY, ledger(), originals)

    def test_complete_original_record_reader_reports_native_flags(self):
        def token(value):
            if isinstance(value, str):
                encoded = value.encode("utf-8")
                return b"\x01" + struct.pack(">H", len(encoded)) + encoded
            return b"\x03" + struct.pack("<f", value)
        def record(label, values=(), children=()):
            return token(label) + struct.pack(">I", len(values)) + b"".join(token(value) for value in values) + struct.pack(">I", len(children)) + b"".join(children)
        native = record("mmoBuilding", children=(record("type", ("SmallInn",)), record("variant", ("base",)),
                        record("active", ("false",)), record("powered", ("true",)), record("state", ("online",)),
                        record("transform", children=(record("translation", (8110.0, 78.82322692871094, 8610.0)),))))
        header = struct.pack(">H", len(b"RecordV2")) + b"RecordV2"
        with tempfile.TemporaryDirectory(prefix="remove-restore-record-", dir=HERE) as directory:
            root = Path(directory)
            path = root / "data" / ("snapshot-" + SNAPSHOT + ".vrz")
            path.parent.mkdir()
            def write(payload):
                data = zlib.compress(payload)
                path.write_bytes(data)
                return {"path": str(path), "snapshot_id": SNAPSHOT, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                        "state": "snapshot_file_complete", "distribution_requested": False, "building_bindings_confirmed": True,
                        "building_identity_error": "", "run_id": RUN}
            receipt = write(header + record("root", children=(native,)))
            result = runner.saved_building(receipt, {"runtime_root": str(root)}, RUN, row(False))
            self.assertTrue(result["fully_consumed"])
            self.assertEqual(result["fields"]["active"], ["false"])
            self.assertEqual(result["fields"]["powered"], ["true"])
            # Native saved height is observed independently of the live pose.
            self.assertNotEqual(result["transform"]["translation"][1], row(False)["y"])
            receipt = write(header + record("root", children=(native,)) + b"extra")
            with self.assertRaisesRegex(runner.AcceptanceError, "not fully consumed"):
                runner.saved_building(receipt, {"runtime_root": str(root)}, RUN, row(False))

    def test_exact_original_weak_pair_can_clear_without_queue_retirement(self):
        original = developers(ledger())
        cleared = copy.deepcopy(original)
        cleared["jobs"]["jobs"][0].update(building_present=False, building_proxy_present=False)
        cleared["job_building_targets"].update(expired_targets=[], building_binding_cause="Queue contains no Building target; no shared Building binding was observed")
        runner.validate_developers(cleared, original, ledger(), "host")
        runner.validate_developers(cleared, cleared, ledger(), "host")
        self.assertEqual(runner.weak_cleanup(cleared, original, "host"), {0})
        self.assertEqual(cleared["jobs"]["queue_count"], 1)
        for field, value in (("building_present", True), ("type", 2), ("valid", False), ("region_present", False), ("developer_present", True)):
            with self.subTest(field=field):
                broken = copy.deepcopy(cleared)
                broken["jobs"]["jobs"][0][field] = value
                with self.assertRaises(runner.AcceptanceError):
                    runner.validate_developers(broken, original, ledger(), "host")
        with self.assertRaises(runner.AcceptanceError):
            runner.validate_developers(original, cleared, ledger(), "host")


if __name__ == "__main__":
    unittest.main(verbosity=2)
