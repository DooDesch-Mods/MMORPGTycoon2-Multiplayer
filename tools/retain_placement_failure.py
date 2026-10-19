"""Preserve an unconfirmed owned placement without replaying a native operation."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[1]
stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
target = ROOT / 'artifacts/placement_failure' / stamp
target.mkdir(parents=True, exist_ok=False)
sources = ['_MPTest/run.json', '_MPTest/host/instance.json', '_MPTest/client-1/instance.json',
           '_MPTest/host/game/crash.rpt', '_MPTest/host/game/mt2mp.log', '_MPTest/client-1/game/mt2mp.log',
           '_MPTest/host/diagnostics/status.json', '_MPTest/client-1/diagnostics/status.json',
           'artifacts/acceptance/d1169a9c-a10c-47ea-8dda-8aa93dbbb947/summary.json',
           'artifacts/acceptance/d1169a9c-a10c-47ea-8dda-8aa93dbbb947/crash_offsets.json']
receipt = {'state': 'native outcome unconfirmed; no replay', 'cause': 'native private constructor identifier index assertion',
           'observed_frame': 'MT2.exe+E438CA within identifier-array Get called by private construction', 'files': {}}
for name in sources:
    source = ROOT / name
    path = target / name
    path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, path)
    receipt['files'][name] = hashlib.sha256(path.read_bytes()).hexdigest()
(target / 'checkpoint.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
print(str(target / 'checkpoint.json'))
