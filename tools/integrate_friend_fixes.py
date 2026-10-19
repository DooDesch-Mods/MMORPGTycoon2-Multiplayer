"""Apply exact staged candidates while preserving unrelated canonical changes."""
from pathlib import Path
import hashlib
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    staging = (ROOT / sys.argv[1]).resolve()
    staging.relative_to(ROOT / "tools")
    manifest = json.loads((staging / "manifest.json").read_text(encoding="utf-8"))
    prepared = []
    for entry in manifest.get("files", manifest.get("changes", [])):
        target = (ROOT / entry.get("path", entry.get("target"))).resolve()
        target.relative_to(ROOT)
        source = staging / entry["stage"] if "stage" in entry else staging / "tree" / entry["path"]
        if entry.get("base") == "absent":
            if target.exists():
                raise RuntimeError(f"New canonical target exists: {target}")
        elif digest(target) != entry["base_sha256"]:
            raise RuntimeError(f"Canonical input changed: {target}")
        if digest(source) != entry.get("staged_sha256", entry.get("after_sha256")):
            raise RuntimeError(f"Staged input changed: {source}")
        prepared.append((target, source.read_bytes()))
    for target, payload in prepared:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(payload)
    print(json.dumps({"applied": [str(path.relative_to(ROOT)) for path, _ in prepared]}))

if __name__ == "__main__":
    main()
