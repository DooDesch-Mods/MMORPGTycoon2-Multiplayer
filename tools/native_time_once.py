"""Invoke one original Host time button and confirm both peers without replay."""
import time
import uuid

from local_acceptance import AcceptanceError


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
