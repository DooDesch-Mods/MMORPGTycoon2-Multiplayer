"""Stop the exact owned host process and observe the client's native status."""
import json
import time
import argparse
import sys
from pathlib import Path
from local_test import ROOT, RuntimeLock, WindowsProcesses

root = ROOT / "_MPTest"
sys.path.insert(0,str(ROOT / "tools/mcp"))
from runtime import read_json
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--observe-only",action="store_true")
args = parser.parse_args()
with RuntimeLock(root):
    manifest = json.loads((root / "run.json").read_text(encoding="utf-8"))
    host = [record for record in manifest["processes"] if record["role"] == "game" and record["peer_id"] == 1]
    if len(host) != 1:
        raise RuntimeError(f"host identity count={len(host)} required=1")
    state = WindowsProcesses().inspect(host[0], terminate=not args.observe_only)
    print(f"info: exact owned host pid={host[0]['pid']} state={state}", flush=True)
    if state != "stopped:1" and not (args.observe_only and state.startswith("exited")):
        raise RuntimeError(f"host outage requires confirmed termination; actual={state}")
    started = time.monotonic()
    status_path = root / "client-1/diagnostics/status.json"
    while time.monotonic() - started <= 25:
        status = read_json(status_path)
        if status.get("run_id") != manifest["run_id"]:
            raise RuntimeError("client status belongs to a different run")
        if not status.get("session_connected") and status.get("overlay_visible"):
            receipt = {"run_id": manifest["run_id"], "host": host[0], "terminated": state,
                       "elapsed_seconds": time.monotonic() - started, "client": status}
            path = ROOT / "artifacts" / f"host-loss-{manifest['run_id']}.json"
            path.write_text(json.dumps(receipt, ensure_ascii=False, indent=2), encoding="utf-8")
            print(f"info: session ended visibly cause={status['session_status']} evidence={path}", flush=True)
            break
        time.sleep(0.2)
    else:
        raise RuntimeError(f"host loss observation timed out measured={time.monotonic()-started:.3f}s required<=25s")
