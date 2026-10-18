"""Bounded raw standard-I/O probe independent of SDK client Job Objects."""
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
messages = [
    {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"protocolVersion": "2025-11-25", "capabilities": {}, "clientInfo": {"name": "bounded-probe", "version": "1"}}},
    {"jsonrpc": "2.0", "method": "notifications/initialized"},
    {"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}},
    {"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": "list_instances", "arguments": {}}},
]
process = subprocess.Popen([sys.executable, "-X", "utf8", str(root / "tools/mcp/server.py"), "--instances-root", str(root / "test/mcp/_MPTest")],
                           stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8")
try:
    stdout, stderr = process.communicate("".join(json.dumps(message) + "\n" for message in messages), timeout=15)
except subprocess.TimeoutExpired:
    process.kill()
    stdout, stderr = process.communicate(timeout=5)
    print(json.dumps({"error": "stdio observation timeout", "stdout": stdout, "stderr": stderr}))
    raise SystemExit(1)
print(json.dumps({"exit_code": process.returncode, "messages": [json.loads(line) for line in stdout.splitlines()], "stderr": stderr}, ensure_ascii=False))
raise SystemExit(process.returncode)
