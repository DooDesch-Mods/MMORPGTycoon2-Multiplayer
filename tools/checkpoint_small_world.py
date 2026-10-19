"""Update the current small-world state from retained native receipts."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
raise RuntimeError('historical fixed checkpoint retired; use checkpoint_hud_writer.py with current receipt arguments')
NATIVE = 'de38de8f-e4c3-4e2b-9169-0ebf39acfbd7'
HUD = 'c9a762e0-c8e0-4c0e-ad4a-fae765fc8eb7'
native = json.loads((ROOT / 'artifacts/acceptance' / NATIVE / 'summary.json').read_text(encoding='utf-8'))
hud = json.loads((ROOT / 'tools/engine_analysis/calendar_authority/time_acceptance/runs' / HUD / 'summary.json').read_text(encoding='utf-8'))
if not native['passed'] or not hud['passed'] or native['runtime_run_id'] != hud['runtime_run_id']:
    raise RuntimeError('current small-world receipts differ or failed')
states = [entry['result'] for entry in hud['checks'] if entry['check'].startswith('native_hud_')]
if len(states) != 6 or not all(entry['result']['native_hud']['complete'] for entry in states):
    raise RuntimeError('native HUD census incomplete')
running = [entry['result'] for entry in hud['checks'] if entry['check'] == 'native_hud_running']
client = next(entry['result'] for entry in running if entry['peer'] == 2)
if client['selected_scale'] != 1 or client['native_hud']['native_selected_scale'] != 0:
    raise RuntimeError('retained client logical/native scale differs')
if client['native_hud']['pause_window_visible'] is not True or client['native_hud']['items'][0]['button_cached_active'] is not True:
    raise RuntimeError('retained HUD failure differs')
build = 'bfa1921fb58e1d872b2cd55019594a12bc922bcde0b158c97f5217e7cc1e8db9'
for peer in ('host', 'client-1'):
    manifest = json.loads((ROOT / '_MPTest' / peer / 'instance.json').read_text(encoding='utf-8'))
    if manifest['run_id'] != native['runtime_run_id'] or manifest['installed_binary_sha256']['MT2_Multiplayer.dll'] != build:
        raise RuntimeError('owned active runtime differs: ' + peer)
text = f'''# Native Snapshot-Herkunft

[Kleine kanonische Welt](../artifacts/acceptance/{NATIVE}/summary.json) tatsächlich bestanden: Host- und Client-Datei,Stream,Reader,Worker und Retirement jeweils einmal bestätigt. Datum/Buchungstag1,Geld100000,Einkommen150000 und20 Buchungsgruppen stimmen bei Pause überein. [Beide betrachteten Bilder](../artifacts/acceptance/{NATIVE}/frame_checkpoint.json) zeigen dieselbe kleine Welt ohne Tutorialdialog,0Subscriber und00:00:28.

Aktiver eigener Lauf `{native['runtime_run_id']}`, DLL `{build}`. Der neue lesende HUD-Reader ist integriert. [Build/CTest3/3/Proxy1095](../artifacts/build_jobs/hud_reader_core_20261012/status.json), [Kopierhashes](../artifacts/hud_reader_integration/copy_receipt.json), [Native HUD-Prüfung](../tools/engine_analysis/calendar_authority/time_acceptance/runs/{HUD}/summary.json). Je Peer fünf Panelkinder und vier echte Zeititems,vollständige Eigentümerkette,kein ActorOwner. Initial Pause und nach HostF1 jeweils richtige Pauseanzeige. ClientF2 wird genau einmal vom Host quittiert; HostHUD zeigt1x und keinen Pausenrahmen,ClientHUD weiterhinPause und roten Rahmen. ClientClock bleibt0,Hostuhr läuft. [Betrachtete laufende Bilder](../tools/engine_analysis/calendar_authority/time_acceptance/runs/{HUD}/frame_checkpoint.json). Laufende Wirtschaft bleibt offen.

[Ursprünglicher Tag-0-Versuch](../artifacts/acceptance/a43fa189-9c3d-40e5-8087-c55674423cc4/summary.json) bleibt fehlgeschlagen. Clock.Resolve rekonstruiert FlatDay0 aus Monat/Tag; nach dem ersten Load stehen Monat/Tag1,der nächste Load ergibtFlatDay1. Der zweite kanonische Roundtrip liefert ein stabiles Funktionsfixture und behebt diesen Beitrittsfehler nicht. [Tatsächliche Dateifelder](../artifacts/engine_analysis/calendar_snapshot/small_ffd8_output_20261012_receipt.json). Kein Kalenderfeld für diese Prüfung geschrieben.

Offen: gezielter HUD-Writer,Pausenrahmen,Kalenderanzeige/Autorität,SPACE,laufende Wirtschaft und vollständige Aktionen. NET-Adapter bleibt im Staging bis Wrongthread-OriginalOnce/Supply-Delta reviewt und nativ geprüft sind. MixedBuilding625 und vollständiger Subscriber-Census fehlen. NativeOpen setzt isolierte PhysFS-Mounts/Runtimeverzeichnisse voraus; kein adversarialer Mountwechselnachweis.

OwnerRoot besitzt Produktion,Build undSpiel-MCP; Kinder nurStaging. Kein Computer Use,kein Commit/Push,private Spielstände unverändert. Kleine Welt alle Funktionen,große Welt finale Abnahme. Goal vollerKoop bleibt aktiv.
'''
(ROOT / '_drafts/native_open.md').write_text(text, encoding='utf-8')
state_path = ROOT / '_drafts/implementation_state.md'
state = state_path.read_text(encoding='utf-8')
start = state.index('## Aktueller MCP-Eingabekandidat\n')
end = state.index('## Vorherige native Zeit-Runtime-Abnahme\n', start)
state = state[:start] + f'''## Aktueller MCP-Eingabekandidat

[Aktueller nativer Arbeitsstand](native_open.md): zwei kleine eigene Welten,Lauf `{native['runtime_run_id']}`,DLL `{build}`. NativeLoad/Roundtrip `{NATIVE}` besteht mit Datum/Geld/Buchungsgruppen gleich in Pause. Ursprünglicher Tag0-Joinfehler bleibt offen. HUD-Reader ist installiert und nativ vollständig bestätigt; F2-Probe `{HUD}` bestätigt Host1x,ClientClock0,aber echte ClientHUDPause/Pausenrahmen falsch. Anschließendes HostF1 bestätigt. Root nächster Schritt: gezielten UI-only HUD-Writer reviewen und integrieren; NET-Leaf-Delta vor Produktion schließen,danach kleine native Netzwerkprobe. Volle Kalender-/Wirtschaftsreplikation,SPACE,MixedBuilding625,Subscriber-Lifetime,Aktionen,Steamonline bleiben offen. OwnerRoot Build/Produktion/Spiele,Agenten nurStaging. Kleine Welt Funktionsprüfungen,große finaleAbnahme. Keine ComputerUse/Commits/Pushes.

''' + state[end:]
state_path.write_text(state, encoding='utf-8')
print('info: small-world current state updated; HUD failure and original day-0 failure retained')
