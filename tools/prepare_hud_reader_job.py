"""Save the checks for the read-only HUD Core candidate."""
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
prior = json.loads((ROOT / 'artifacts/jobs/native_open_path.json').read_text(encoding='utf-8'))
prior['candidate'] = 'HUD-TIME-1-READONLY-CORE'
path = ROOT / 'artifacts/jobs/hud_reader_core.json'
if path.exists():
    raise RuntimeError('HUD reader build job already exists')
path.write_text(json.dumps(prior, indent=2) + '\n', encoding='utf-8')
print('info: HUD reader build job=' + str(path))
