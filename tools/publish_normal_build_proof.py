"""Carry the completed native UI action from read-only peer receipts, without replay."""
from pathlib import Path
import json
ROOT=Path(__file__).resolve().parents[1]
proof=ROOT/'artifacts/acceptance/ecac525e-11c1-49d4-99a8-3e2728813212/summary.json'
summary=json.loads(proof.read_text())
states={c['result']['peer']:c['result'] for c in summary['checks'] if c['check']=='actual-UI-current-state'}
action='a9d10b5d-570e-403e-9e59-70bd2cb0fff7'
objects={}
for peer,state in states.items():
    controller=state['typed_actions'];receipt=state['ui_build']['receipts'][action]
    assert controller['action_uuid']==action and controller['confirmed']==1 and controller['outcome']==1 and not controller['pending'] and not controller['locked'] and controller['failed']==0
    assert receipt['confirmed'] and receipt['undo_confirmed'] and receipt['private_stage']==18
    assert all(receipt[k]==1 for k in ('purchase','add','undo'))
    objects[peer]=next(p for p in state['props'] if p['uid']==receipt['native_uid'])
assert states['host']['typed_actions']['ack_mask']==3
assert states['client-1']['ui_build']['captured']==1 and states['host']['ui_build']['captured']==0
assert states['host']['simulation']==states['client-1']['simulation']
assert {k:v for k,v in objects['host'].items() if k!='uid'}=={k:v for k,v in objects['client-1'].items() if k!='uid'}
target=ROOT/'_drafts/normal_build_two.md'
target.write_text('''# Normale Client-Platzierung auf zwei Instanzen

Der vorhandene SmallInn-Button und die originalen MouseOver-, Press- und Release-Aufrufe wurden einmal genutzt. Der normale GameUpdate-Bauhook hat genau einen Client-Intent erfasst. [Ursprünglicher UI-Test](../artifacts/acceptance/e49e6110-ca74-4a2c-b6bb-38d2b39febaa/summary.json), [read-only Abschlussbeleg](../artifacts/acceptance/ecac525e-11c1-49d4-99a8-3e2728813212/summary.json).

Aktion a9d10b5d-570e-403e-9e59-70bd2cb0fff7 ist nativ auf beiden Instanzen bestätigt: Host-ACK-Maske3, Purchase/Add/Undo jeweils1, D0-Abschluss18, keine Fehler oder Doppelaufrufe, beide Geld94200 und gleiche20 Buchungsgruppen. Host-UID3176 und Client-UID3177 zeigen die unabhängigen lokalen Objektkennungen. Beide aktuellen Gebäude stehen bei x8100,y75.2188949584961,z8550.

Die Diagnose verlangte anschließend auch im Client die Host-ACK-Sammlung. Der Client speichert diese Sammlung nicht; er bestätigt nach seiner geprüften Host-Completion. Diese falsche Diagnose wurde im Code rollenabhängig korrigiert. Ihr alter Fehler bleibt erhalten. Der erfolgreiche Bauauftrag wurde nicht wiederholt und wird als bestanden übernommen.

Der alte Diagnosejob sperrt noch weitere MCP-Mutationen in diesem Lauf. Daher liegt keine nachträgliche Bildaufnahme vor; native Wirkung, Eingabepfad und aktueller gemeinsamer Zustand sind über die tatsächlichen Receipts/Props belegt. Die normale Sichtbarkeit des Bauwerkzeugs bleibt separat zu betrachten. Der nächste Kandidat ergänzt die Diagnosekorrektur und Move. Voller Koop, weitere Objektfamilien und laufende Simulation bleiben offen.
''',encoding='utf-8')
for name in ('_drafts/progress.html','tools/update_progress.py'):
    path=ROOT/name;text=path.read_text(encoding='utf-8')
    text=text.replace('Normale Mausplatzierung bleibt offen; die aktuelle Bodenhöhe wird für das Entfernen neuer Objekte ergänzt.','<a href="normal_build_two.md">Normaler Client-Bau ebenfalls nativ bestätigt</a>; die Diagnosekorrektur und das Entfernen neuer Objekte folgen.')
    path.write_text(text,encoding='utf-8')
print('info: genuine normal UI construction carried forward, no replay')
