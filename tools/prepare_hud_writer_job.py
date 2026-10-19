"""Save the build checks for the UI-only HUD writer integration."""
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
prior = json.loads((ROOT / 'artifacts/jobs/native_open_path.json').read_text(encoding='utf-8'))
prior['candidate'] = 'HUD-TIME-2-UI-ONLY-CORE'
path = ROOT / 'artifacts/jobs/hud_writer_core.json'
if path.exists():
    raise RuntimeError('HUD writer build job already exists')
path.write_text(json.dumps(prior, indent=2) + '\n', encoding='utf-8')
print('info: HUD writer build job=' + str(path))
