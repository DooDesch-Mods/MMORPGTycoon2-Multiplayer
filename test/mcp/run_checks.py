"""Run bounded local checks and retain the exact evidence."""
import json
import argparse
import hashlib
from datetime import datetime, timezone
from pathlib import Path
import subprocess
import sys

test_root = Path(__file__).resolve().parent
workspace = test_root.parents[1]
evidence = test_root / "evidence"
evidence.mkdir(exist_ok=True)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--pattern", default="test*.py")
parser.add_argument("--timeout", type=float, default=45)
options = parser.parse_args()
if not 1 <= options.timeout <= 300:
    parser.error("timeout must be 1 through 300 seconds")
previous_log = evidence / "checks.log"
if previous_log.exists():
    previous_stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    (evidence / f"checks_previous_{previous_stamp}.log").write_bytes(previous_log.read_bytes())
command = [sys.executable, "-X", "utf8", "-m", "unittest", "discover", "-s", str(test_root), "-p", options.pattern, "-v"]
try:
    completed = subprocess.run(command, cwd=workspace, capture_output=True, text=True, encoding="utf-8", timeout=options.timeout)
    log = completed.stdout + completed.stderr
    exit_code = completed.returncode
except subprocess.TimeoutExpired as exc:
    stdout = exc.stdout.decode("utf-8", errors="replace") if isinstance(exc.stdout, bytes) else (exc.stdout or "")
    stderr = exc.stderr.decode("utf-8", errors="replace") if isinstance(exc.stderr, bytes) else (exc.stderr or "")
    log = stdout + stderr + f"\nerror: MCP test subprocess exceeded {options.timeout:g} seconds\n"
    exit_code = 1
log_path = evidence / "checks.log"
log_path.write_text(log, encoding="utf-8")
result = {"exit_code": exit_code, "passed": exit_code == 0, "log": str(log_path), "command": command,
          "scope": "MCP stdio protocol, Windows process identity, isolated mailbox fixtures; no live game acceptance"}
result["source_sha256"] = {str(path.relative_to(workspace)): hashlib.sha256(path.read_bytes()).hexdigest()
                           for path in sorted((workspace / "tools" / "mcp").glob("*.py"))}
(evidence / "checks.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
print(json.dumps(result))
raise SystemExit(exit_code)
