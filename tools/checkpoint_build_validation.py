"""Update the current record from the deployed two-peer native validation receipts."""
import argparse
import hashlib
import json
from pathlib import Path
import uuid

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--native', type=lambda value: str(uuid.UUID(value)), required=True)
parser.add_argument('--ground', type=lambda value: str(uuid.UUID(value)), required=True)
parser.add_argument('--placement', type=lambda value: str(uuid.UUID(value)), required=True)
parser.add_argument('--dll-sha256', required=True)
args = parser.parse_args()
records = {}
for key in ('native', 'ground', 'placement'):
    records[key] = json.loads((ROOT / 'artifacts/acceptance' / getattr(args, key) / 'summary.json').read_bytes())
    if records[key]['passed'] is not True:
        raise RuntimeError('native acceptance is not confirmed: ' + key)
run = records['native']['runtime_run_id']
if any(record['runtime_run_id'] != run for record in records.values()):
    raise RuntimeError('native validation receipts refer to different runtime identities')
for role in ('host', 'client-1'):
    manifest = json.loads((ROOT / '_MPTest' / role / 'instance.json').read_bytes())
    if manifest['run_id'] != run or manifest['installed_binary_sha256']['MT2_Multiplayer.dll'] != args.dll_sha256:
        raise RuntimeError('active owned runtime differs: ' + role)
directory = ROOT / 'artifacts/acceptance' / args.placement
frames = json.loads((directory / 'frame_checkpoint.json').read_bytes())
if frames['source_sha256'] != hashlib.sha256((directory / 'summary.json').read_bytes()).hexdigest():
    raise RuntimeError('retained frame receipt refers to different placement source')
for role, frame in frames['frames'].items():
    if hashlib.sha256(Path(frame['retained_path']).read_bytes()).hexdigest() != frame['file_sha256']:
        raise RuntimeError('retained image changed: ' + role)
state_path = ROOT / '_drafts/implementation_state.md'
state = state_path.read_text(encoding='utf-8')
start = state.index('## Aktueller MCP-Eingabekandidat\n')
end = state.index('## Vorherige native Zeit-Runtime-Abnahme\n', start)
replacement = f'''## Aktueller MCP-Eingabekandidat

[Aktueller Zweier-Kandidat](small_building_validation.md): eigene kleine Welten, Lauf `{run}`, DLL `{args.dll_sha256}`. NativeLoad und Regionskauf `{args.native}` sowie frische Insel-/Baugrundproben `{args.ground}` bestehen. Eine diagnostische Client-Platzierung `{args.placement}` erscheint genau einmal pro Instanz auf Land bei X7950/Z8550/Y63,5211448669. Je400 Kosten,94600 Geld und20 Buchungsgruppen stimmen überein. Beide nativen Bilder wurden betrachtet und hashgebunden erhalten. Der Baum bleibt überlappend stehen; Scenery-/Undo-Folge und vollständiger regulärer Baucommit sind offen.

Die bekannte Wasserposition wurde mit gleicher neuer DLL vor Kauf und Objekterstellung auf beiden Peers abgewiesen: `1f6b3e2a-62af-4de2-ba55-a138cc8d461b`. Frühere Wasserplatzierung und fehlgeschlagene Küstensuche bleiben erhalten. [Vorheriger Zeitnachweis](native_open.md) bestätigt F2/F3/F4/F1, tatsächliche Bedienleisten und sichtbaren Host-Zeittext; laufende Wirtschaft und Events bleiben offen.

Root nächster Schritt: echte NativeWeakleases, Freequittungen und selection-/highlightschonendes Entfernen integrieren, dann vollständigen Baucommit und Undo schließen. Native Leases und Objekt-Lifecycle sind vollständig im Source gelesen; Runtimeprovider, NativeFreehook und tatsächliche Spielreceipts fehlen. Alle Funktionen zuerst genau zwei Instanzen in kleiner Welt; vier Instanzen und große Welt erst final. Owner Root: Produktion, Build, Spiele und MCP. Agenten: Staging für NativeFree, Buildingaktionen und Netzwerkbau. Keine Computer Use, Commits oder Pushes. Vollständiger Koop, übrige Aktionsmatrix, laufende Simulation und Steam bleiben beauftragt.

'''
state_path.write_text(state[:start] + replacement + state[end:], encoding='utf-8')
print('info: current two-instance checkpoint replaced; original failures retained; game effects=0')
