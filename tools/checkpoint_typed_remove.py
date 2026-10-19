"""Retain the proven first authoritative action without repeating game calls."""
from pathlib import Path
import json
import hashlib
from native_remove_acceptance import retirement_confirmed

ROOT=Path(__file__).resolve().parents[1]
ID='c9e11645-0bd8-4547-b691-05d68bab3711'
directory=ROOT/'artifacts/acceptance'/ID
source=directory/'summary.json';summary=json.loads(source.read_text(encoding='utf-8'))
if summary.get('passed') is not True or summary.get('client_request_replication_proven') is not True:
    raise ValueError('actual typed native action proof absent')
rows=[entry['result'] for entry in summary['checks'] if entry['check']=='typed-native-completion']
if len(rows)!=2 or {row['peer'] for row in rows}!={'host','client-1'}:
    raise ValueError('typed native completion peer coverage differs')
for row in rows:
    state=row['state']
    if state.get('action_uuid')!=summary['action_uuid'] or state.get('pending') is not False or state.get('locked') is not False:
        raise ValueError('typed native pending or Unknown completion')
    for field,expected in (('attempted',1),('confirmed',1),('host_revision',1),('failed',0),('outcome',1),('required_mask',3)):
        if type(state.get(field)) is not int or state[field]!=expected:
            raise ValueError('typed native exact completion field differs: '+field)
    if row['peer']=='host' and (type(state.get('ack_mask')) is not int or state['ack_mask']!=3):
        raise ValueError('typed Host both ACKs absent')
    retirement_confirmed(row['retirement'],summary['action_uuid'],row['retirement']['sequence'])
frames=directory/'frame_checkpoint_after.json'
if not frames.exists():raise ValueError('typed action actually inspected images absent')
checkpoint=directory/'typed_remove_checkpoint.json'
if checkpoint.exists():raise ValueError('typed action checkpoint immutable')
checkpoint.write_text(json.dumps({'passed':True,'action_uuid':summary['action_uuid'],'runtime_run_id':summary['runtime_run_id'],
    'dll_sha256':summary['parameters']['expected_dll_sha256'],'summary_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
    'frames_sha256':hashlib.sha256(frames.read_bytes()).hexdigest(),'host_native_executions':1,'client_native_applications':1,
    'native_retired_holders':2,'host_required_mask':3,'host_ack_mask':3,'regular_ui_capture_proven':False},indent=2)+'\n',encoding='utf-8')
(ROOT/'_drafts/typed_remove_two.md').write_text(f'''# Gemeinsamer Remove-Auftrag auf zwei Instanzen

Ein Client-Auftrag entfernt die SmallInn genau einmal beim Host und genau einmal im Client. Beide bestätigen native Entfernung, Undo, Auswahl und sichere Referenzfreigabe. Der Host meldet Erfolg erst mit beiden ACKs: mask3 von required3. Keine Wiederholungen, Fehler oder Unknown. [Abnahme](../artifacts/acceptance/{ID}/summary.json), [unveränderlicher Beleg](../artifacts/acceptance/{ID}/typed_remove_checkpoint.json).

Beide Geldstände sind95.000, alle20 Buchungsgruppen stimmen überein. Kein nachträglicher Geld-Ausgleich: Die native Entfernung erstattet auf beiden Seiten400. Beide [Abschlussbilder](../artifacts/acceptance/{ID}/frame_checkpoint_after.json) wurden betrachtet: SmallInn fehlt, Baum bleibt, Pause00:00:28, Undo „Destroy Small Inn #1“, Bauziel0/3.

Lauf `{summary['runtime_run_id']}`, DLL `{summary['parameters']['expected_dll_sha256']}`, Auftrag `{summary['action_uuid']}`. [34 Bridge- und4 SDK-Regressionen](typed_action_tests.md) sichern vollständige Ledger, Autorität, Teilfehler, Token und Freigabe. Sechs neue Controller-Prüfungen ergänzen die zwölf erhaltenen historischen Pakettests. [Native Remove](native_remove_two.md) und [Referenzfreigabe](native_retirement_two.md) werden übernommen.

Der Auftrag kam über den Spiel-MCP. Die normale Entfernen-Schaltfläche ist noch nicht an diesen Controller gebunden. Vollständiger Baucommit/Undo, weitere Aktionen, laufende Wirtschaft, Events, vollständige Simulation und Steam bleiben offen. Zwei Instanzen bleiben der Arbeitsumfang; vier und großer Save folgen final.
''',encoding='utf-8')
print(f'info: typed native action checkpoint={checkpoint}; no game calls')
