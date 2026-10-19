"""Retain the actual two-peer release receipt without further game calls."""
from pathlib import Path
import json
import hashlib
from native_remove_acceptance import retirement_confirmed

ROOT=Path(__file__).resolve().parents[1]
ID='19ddc160-18ed-46ce-994c-27fbcd51ef65'
DIRECTORY=ROOT/'artifacts/acceptance'/ID
path=DIRECTORY/'summary.json';summary=json.loads(path.read_text(encoding='utf-8'))
if summary.get('passed') is not True or summary.get('holder_retirement_proven') is not True:
    raise ValueError('actual native retirement scope absent')
rows=[entry['result'] for entry in summary['checks'] if entry['check']=='native-retirement-confirmed']
if {row['peer'] for row in rows}!={'host','client-1'} or len(rows)!=2:
    raise ValueError('native retirement peer coverage differs')
for row in rows:
    value=row['receipt'];retirement_confirmed(value,value['token'],value['sequence'])
frames=DIRECTORY/'frame_checkpoint_after.json'
if not frames.exists():
    raise ValueError('actually inspected frame checkpoint absent')
checkpoint=DIRECTORY/'native_retirement_checkpoint.json'
if checkpoint.exists():
    raise ValueError('native retirement checkpoint immutable')
checkpoint.write_text(json.dumps({'passed':True,'runtime_run_id':summary['runtime_run_id'],
    'dll_sha256':summary['parameters']['expected_dll_sha256'],'summary_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
    'frames_sha256':hashlib.sha256(frames.read_bytes()).hexdigest(),'retired_holders':2,
    'client_request_replication_proven':False},indent=2)+'\n',encoding='utf-8')
(ROOT/'_drafts/native_retirement_two.md').write_text(f'''# Native Referenzfreigabe auf zwei Instanzen

Die nach SmallInn-Entfernung gehaltene Mod-Referenz wurde auf Host und Client tatsächlich freigegeben. Je ein Release, D0, SizedDelete und CRT-Free sind bestätigt, alle Zähler1, SEH0. Beide Pools zeigen released1, held0, tombstones0 und kein Unknown. [Abnahme](../artifacts/acceptance/{ID}/summary.json), [unveränderlicher Beleg](../artifacts/acceptance/{ID}/native_retirement_checkpoint.json).

Beide [Abschlussbilder](../artifacts/acceptance/{ID}/frame_checkpoint_after.json) wurden betrachtet: Geld95.000, SmallInn fehlt, Baum bleibt, Pause00:00:28, Undo „Destroy Small Inn #1“, Bauziel0/3. Die vorherige [native Entfernung](native_remove_two.md) bleibt bestanden; ihre erneute Ausführung diente hier nur als notwendige Fixture für die neue Freigabe.

Lauf `{summary['runtime_run_id']}`, DLL `{summary['parameters']['expected_dll_sha256']}`. Die tatsächliche Registry-Widerrufprüfung und native Quiescence steuern den Freigabejournal. [28 neue Regressionen](remove_diagnostics_tests.md) fanden und sichern die ergänzte SEH-Nullprüfung. Drei unveränderte MCP-Prüfungen wurden per Quellhash übernommen.

Der gemeinsame Client-Auftrag über Host und Client-Replikation ist der nächste Nachweis. Vollständiger Baucommit, Undo-Ausführung, weitere Objektgruppen, laufende Wirtschaft, Events, vollständige Simulation und Steam bleiben offen. Vier Instanzen und großer Save folgen final.
''',encoding='utf-8')
print(f'info: native retirement checkpoint={checkpoint}; no game calls')
