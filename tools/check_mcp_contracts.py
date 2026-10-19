"""Retain one complete SDK contract-suite result with its original output."""
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
output = root / 'artifacts/mcp_checks' / stamp
output.mkdir(parents=True, exist_ok=False)
log = output / 'suite.log'
command = [sys.executable, '-X', 'utf8', '-m', 'unittest', 'discover', '-s', 'test/mcp', '-p', 'test_*.py']
with log.open('w', encoding='utf-8') as stream:
    result = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT)
receipt = {'passed': result.returncode == 0, 'exit_code': result.returncode, 'log': str(log),
           'scope': 'SDK and isolated owned mailbox contracts; no game execution'}
(output / 'result.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
print(json.dumps(receipt), flush=True)
lines = log.read_text(encoding='utf-8', errors='replace').splitlines()
print('\n'.join(lines[-8:] if result.returncode == 0 else lines[-35:]))
sys.exit(result.returncode)
