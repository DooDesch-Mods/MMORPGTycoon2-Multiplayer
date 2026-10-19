"""Create one damaged owned test copy while retaining its expected digest."""
import json
import uuid
from pathlib import Path

root = Path(__file__).resolve().parents[1]
runtime = root / "_MPTest/host"
identifier = str(uuid.uuid4())
source = json.loads((runtime / "diagnostics/snapshot-8e8d4803-cd03-4abf-881b-0005ed49f9b6.json").read_text(encoding="utf-8"))
owned = runtime / "data"
path = Path(source["path"]).resolve()
if not path.is_relative_to(owned.resolve()):
    raise RuntimeError("source snapshot is outside the owned test root")
data = bytearray(path.read_bytes())
if not data:
    raise RuntimeError("source snapshot is empty")
data[len(data)//2] ^= 1
target = path.parent / f"snapshot-{identifier}.vrz"
with target.open("xb") as stream:
    stream.write(data)
record = dict(source, snapshot_id=identifier, path=str(target))
record["run_id"] = json.loads((runtime / "instance.json").read_text(encoding="utf-8"))["run_id"]
(runtime / f"diagnostics/snapshot-{identifier}.json").write_text(json.dumps(record,indent=2),encoding="utf-8")
spec = {"timeout_seconds":15,"calls":[{"tool":"game_load_snapshot","arguments":{"snapshot_id":identifier}}]}
spec_path = root / "artifacts/mcp_corrupt_snapshot.json"
spec_path.write_text(json.dumps(spec,indent=2),encoding="utf-8")
print(f"info: corrupt copy prepared id={identifier} bytes={len(data)} spec={spec_path}")
