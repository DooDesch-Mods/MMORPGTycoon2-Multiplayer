"""Refresh the current native-load checkpoint from retained real game receipts."""
import hashlib
import json
from pathlib import Path

raise RuntimeError('historical checkpoint superseded; use tools/checkpoint_small_world.py for the current small-world receipts')
ROOT = Path(__file__).resolve().parents[1]
IDENTIFIER = 'a43fa189-9c3d-40e5-8087-c55674423cc4'
READBACK = 'fd440ba6-ae9f-4366-96cf-dd3cb16a2b4f'
summary = json.loads((ROOT / 'artifacts/acceptance' / IDENTIFIER / 'summary.json').read_text(encoding='utf-8'))
checks = {entry['check']: entry['result'] for entry in summary['checks']}
for name in ('host-current-native-origin', 'client-current-native-origin', 'host-origin-preserved-through-save'):
    if checks[name]['current_origin_available'] is not True or checks[name]['pin_close_confirmed'] != 1:
        raise RuntimeError(f'native origin checkpoint differs: {name}')
build = json.loads((ROOT / 'artifacts/build_jobs/native_open_path_20261012/status.json').read_text(encoding='utf-8'))
if build['state'] != 'confirmed-execution' or build['exit_code'] != 0:
    raise RuntimeError('NativeOpen build is not confirmed')
dll = hashlib.sha256((ROOT / 'build/bin/Release/MT2_Multiplayer.dll').read_bytes()).hexdigest()
if dll != '7119c0b476aec9925a2232dc10855191616f4b2a50a9e8de73535d98ab01d207':
    raise RuntimeError('current NativeOpen DLL differs from running proof')
text = f'''# Native Snapshot-Herkunft

DLL `{dll}`, laufende eigene Sitzung `{summary['runtime_run_id']}`. Host und Client haben die native Datei-/Reader-/Workerprüfung bestanden. [Ursprüngliche Gesamtprüfung](../artifacts/acceptance/{IDENTIFIER}/summary.json) scheiterte anschließend am Tagesvergleich; sie bleibt unverändert fehlgeschlagen. [Read-only-Nachprüfung mit betrachteten Bildern](../artifacts/acceptance/{READBACK}/frame_checkpoint.json) besteht ohne erneutes Laden.

Der Host öffnete Snapshot `f5acffea-9a64-49cd-8d06-1a5aa4ad6f4c`,211573Bytes,SHA `e72e7777c65bfc75a7f6cbb613ecd0604f871ef4fd292526a7b4f6f00a043adf`. Sein neuer nativer Save `ffd8ff76-7b5a-42a6-a048-e298889c0836`,213009Bytes,SHA `0fd07115a7f24a3112d2759779394b5f4192422c28d8fb3616ceb0dcaabee9cf` wurde vollständig übertragen und im Client geöffnet. Je Peer: vier Hookreceipts,je ein File-/Stream-/Reader-/Workerreturn,keine Originalausnahme,eine bestätigte Pinfreigabe und aktueller World-Origin. Jeder lokale Native-Weltepochzähler ist1; er ist von der gemeinsamen Netzwerkepoche getrennt.878 fremde Dateiöffnungen wurden übersprungen.

[Build/CTest/Proxy](../artifacts/build_jobs/native_open_path_20261012/status.json) bestehen. [Launcher14/Acceptance42](../artifacts/build_jobs/native_open_integration_20261012/status.json) bleiben gültige Regressionen. [Kopierbeleg](../artifacts/native_open_integration/copy_receipt.json) bindet den geprüften Freeze; Produktionsdelta umfasst Makroguards,vier OriginalOnce-Wrapper und Host-/Client-Loadbindung. Der gemessene PhysFS-Schreibpfad endet mit einem Slash; die Wrapper normalisieren ihn vor der SteamID-Prüfung. Observerlogs unterscheiden fremde Aufrufe und Fehler,begrenzen Wiederholungen und erhalten Recovery.

Die beiden Bilder zeigen dieselbe kleine Welt ohne Tutorialdialog,pausiert,100000 Geld,keine Subscriber und00:00:28. Native Kalender-/Buchungsreadbacks unterscheiden sich: Hosttag0,Clienttag1. Gleiche Bilder beweisen hier keinen Tagesgleichstand. Auch die vorherige [laufende HUD-Prüfung](../tools/engine_analysis/calendar_authority/time_acceptance/runs/518191ee-ae2b-46c2-82e5-dc7af63fbb5f/frame_checkpoint.json) zeigt einen echten Fehler: ClientF2 ist vom Host quittiert,aber der Client hebt weiter Pause hervor. Der bisherige Actor-Item-ACK deckt andere Receiver als die sichtbaren ClockPanel-Items ab.

Offen sind Tages-/Kalenderreplikation,vier tatsächliche HUD-Timeitems,Pausenrahmen,Space-Guard und laufende Wirtschaft. Der NET-State-Adapter bleibt vor Root-Review und eigener kleiner Runtimeprobe im Staging. Eine Root-Review hat gerade pauschales Überspringen nativer Netzwerk-Leafs außerhalb des Mainthreads als Fehler gefunden; der Eigentümer korrigiert den Kandidaten. MixedBuilding625 und vollständiger Subscriber-Census sind noch Staging. NativeOpen schließt parallele Änderungen an PhysFS-Mounts und eigenen Parentverzeichnissen nicht adversarial aus; isolierte Runtime bleibt Voraussetzung. Ein interner PhysFS-Plattformhandle wurde nicht geprüft.

Owner bleibt Root für Produktion,Build und Spiel-MCP. Kinder besitzen nur ihre Stagingpfade. Keine Computer Use,kein Commit/Push,private Spielstände unverändert. Funktionsprüfungen bleiben in der kleinen Welt; große Welt finale Abnahme. Nächster Schritt: NET-Adapter-Reviewdelta schließen und echte HUD-Receiver lesen; kein erneuter Load als Antwort auf unveränderten Tagesfehler.
'''
(ROOT / '_drafts/native_open.md').write_text(text, encoding='utf-8')
path = ROOT / '_drafts/implementation_state.md'
state = path.read_text(encoding='utf-8')
marker = '## Aktueller MCP-Eingabekandidat\n'
end_marker = '## Vorherige native Zeit-Runtime-Abnahme\n'
start, end = state.index(marker), state.index(end_marker, state.index(marker))
state = state[:start] + f'''## Aktueller MCP-Eingabekandidat

[NativeOpen und aktueller Arbeitsstand](native_open.md): DLL `{dll}`,Sitzung `{summary['runtime_run_id']}`,zwei kleine pausierte Welten. Host-/Client-File,Reader,Worker und Retirement tatsächlich bestanden; Gesamtprüfung `{IDENTIFIER}` scheiterte am Hosttag0/Clienttag1. Read-only-Abnahme `{READBACK}` mit zwei betrachteten Bildern besteht. Vorherige neun F1-F4-Inputs sind belegt; sichtbarer Client-1x-Buttonfehler und Kalenderreplikation bleiben offen. Actor-ACK bestätigt keine HUD-Items. NativeOpen ist integriert; NET-Adapter wird vor Installation vollständig reviewed,konkreter Wrongthread-OriginalOnce-Fehler liegt beim Eigentümer. Root besitzt Produktion,Build und Spiele; Kinder nur Staging. Kleine Welt alle Funktionsprüfungen,große Welt finale Abnahme. Goal bleibt voller Koop.

''' + state[end:]
path.write_text(state, encoding='utf-8')
print(f'info: NativeOpen actual proof recorded; overall ledger failure retained; run={summary["runtime_run_id"]}')
