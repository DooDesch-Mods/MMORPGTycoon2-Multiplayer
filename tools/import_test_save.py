"""Copy an authorized save into the isolated test profile with source evidence."""
from pathlib import Path
from datetime import datetime, timezone
import argparse
import hashlib
import json
import os
import uuid

ROOT=Path(__file__).resolve().parents[1]
SAVE_ROOT=Path.home()/"AppData/Roaming/VectorStorm/MMORPG Tycoon 2/76561198013473990/save"
MAX_BYTES=1024*1024*1024


def hash_file(path):
    digest=hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda:stream.read(1024*1024),b""):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source",type=Path)
    args=parser.parse_args()
    candidates=[path for path in SAVE_ROOT.rglob("*.vrz") if not path.name.endswith(".meta.vrz")]
    candidates.sort(key=lambda path:(path.stat().st_size,path.stat().st_mtime_ns),reverse=True)
    if args.source is None:
        print(json.dumps({"save_count":len(candidates),"largest":[{"path":str(path),"bytes":path.stat().st_size,"modified_utc":datetime.fromtimestamp(path.stat().st_mtime,timezone.utc).isoformat()} for path in candidates[:8]]},ensure_ascii=False,indent=2))
        return
    source=args.source.resolve(strict=True)
    if not source.is_relative_to(SAVE_ROOT.resolve()) or source not in candidates:
        raise ValueError("source must be one complete save in the authorized game's private save directory")
    before=source.stat()
    if not 0<before.st_size<=MAX_BYTES:
        raise ValueError(f"source size actual={before.st_size} required=1..{MAX_BYTES} bytes")
    identifier=str(uuid.uuid4())
    runtime=ROOT/"_MPTest/host"
    status=json.loads((runtime/"diagnostics/status.json").read_bytes())
    destination_root=Path(status["write_dir"]).resolve()
    if not destination_root.is_relative_to((runtime/"data").resolve()):
        raise ValueError("test PhysFS write directory is outside the isolated host profile")
    target=destination_root/"save/MPTestSession"/f"snapshot-{identifier}.vrz"
    target.parent.mkdir(parents=True,exist_ok=True)
    staging=target.with_suffix(".importing")
    digest_state=hashlib.sha256()
    copied=0
    with source.open("rb") as original, staging.open("xb") as stream:
        for chunk in iter(lambda:original.read(1024*1024),b""):
            copied+=len(chunk)
            if copied>before.st_size:
                raise ValueError("source grew during copy; original preserved, partial staging retained")
            if stream.write(chunk)!=len(chunk):
                raise ValueError("test staging write incomplete; original preserved")
            digest_state.update(chunk)
        stream.flush()
        os.fsync(stream.fileno())
    after=source.stat()
    if (before.st_size,before.st_mtime_ns)!=(after.st_size,after.st_mtime_ns) or copied!=before.st_size:
        raise ValueError("source changed during copy; original preserved, partial staging retained")
    digest=digest_state.hexdigest()
    if hash_file(staging)!=digest:
        raise ValueError("copied test staging digest differs; original preserved, staging retained")
    staging.rename(target)
    source_check=source.stat()
    if (source_check.st_size,source_check.st_mtime_ns)!=(after.st_size,after.st_mtime_ns):
        raise ValueError("source changed after copy; copied snapshot retained, import not confirmed")
    record={"snapshot_id":identifier,"path":str(target),"bytes":copied,"sha256":digest,"run_id":status["run_id"],
        "source_path":str(source),"source_size":after.st_size,"source_mtime_ns":after.st_mtime_ns,"original_preserved":True,"imported_utc":datetime.now(timezone.utc).isoformat()}
    receipt=runtime/"diagnostics"/f"snapshot-{identifier}.json"
    with receipt.open("x",encoding="utf-8") as stream:
        json.dump(record,stream,indent=2,ensure_ascii=False)
    print(json.dumps({"state":"copied-and-hash-checked","snapshot_id":identifier,"bytes":copied,"sha256":digest,"receipt":str(receipt),"original_preserved":True},ensure_ascii=False))


if __name__=="__main__":
    main()
