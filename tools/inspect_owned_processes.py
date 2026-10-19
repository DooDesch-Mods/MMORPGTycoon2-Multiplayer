"""Read exact process identities from the current owned test manifest."""
import json
from local_test import ROOT, RuntimeLock, WindowsProcesses

root=ROOT / "_MPTest"
with RuntimeLock(root):
    manifest=json.loads((root / "run.json").read_text(encoding="utf-8"))
    processes=WindowsProcesses()
    for record in manifest["processes"]:
        state=processes.inspect(record)
        print(f"info: run={manifest['run_id']} role={record['role']} peer={record['peer_id']} pid={record['pid']} state={state}")
