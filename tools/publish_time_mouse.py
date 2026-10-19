"""Publish the native two-peer HUD proof and retain previous failed hypotheses."""
from pathlib import Path
import hashlib,json,shutil
ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/'artifacts/acceptance/9c1700c5-c326-45c7-9d31-5951cb191c2f'
value=json.loads((folder/'summary.json').read_text(encoding='utf-8'))
if value.get('passed') is not True or value.get('state')!='confirmed-native-HUD-mouse-two':raise ValueError('native HUD mouse proof missing')
freeze=ROOT/'artifacts/time_mouse_candidate/20261012T154136488327Z/freeze.json'
frozen=json.loads(freeze.read_text(encoding='utf-8'))
for path,digest in frozen['files'].items():
    if hashlib.sha256((ROOT/path).read_bytes()).hexdigest()!=digest:raise ValueError('native mouse proof source changed: '+path)
frames=[entry['result'] for entry in value['checks'] if entry['check']=='native-frame-artifact']
for index,item in enumerate(frames):
    source=Path(item['path']);name=('fast' if index<2 else 'paused')+'_'+item['peer']+'_time_mouse.bmp'
    if hashlib.sha256(source.read_bytes()).hexdigest()!=item['file_sha256']:raise ValueError('native mouse frame changed')
    shutil.copy2(source,ROOT/'_drafts/media'/name)
checkpoint={'candidate':frozen['candidate'],'passed':True,'run_id':value['runtime_run_id'],
    'deployed_dll_sha256':value['parameters']['expected_dll_sha256'],'source_freeze':str(freeze),
    'summary':str(folder/'summary.json'),'frames_inspected':True,'four_native_mouse_requests':value['attempts'],
    'paused_UI_time':'00:29:19','Client_native_clock':0,'Host_native_setter_effects':4,
    'failed_preflight_receipts':['artifacts/acceptance/eb7b6ccc-a0e9-4f96-a7ff-9e8502f99f6f/summary.json',
        'artifacts/acceptance/018f5e4c-2081-4eb9-8dca-ea11dcd85f6e/summary.json'],
    'failed_effects':0,'regular_build_place_or_full_simulation_proven':False}
target=folder/'native_time_mouse_checkpoint.json'
if target.exists():raise ValueError('native time mouse checkpoint already exists')
target.write_text(json.dumps(checkpoint,indent=2)+'\n',encoding='utf-8')
(ROOT/'_drafts/time_mouse_two.md').write_text('''# Native HUD-Zeitbuttons auf zwei Instanzen

Der Client-Zeitklick ist auf dem tatsächlichen Spielbuild24993678 bestätigt. Lauf e69f112a-5975-4a8d-b383-682e29773ab7, DLL d2c45ff824a4576ddd945e92e699c9ab4cea78129341f95f8eee1f1228e3fbe9. Vier native ItemButton.Activate-Aufrufe: Client9, ClientPause0, HostNormal1, HostPause0. Jeder erreicht den inneren Mouse-Rücksprung58095C, wird lokal unterdrückt und führt genau einen bestätigten Hostsetter aus. ClientNativeClock bleibt0. Host-/Client-Sitzung und Kalender bleiben verbunden und ohne Lock.

[Native Quittungen und Quellen](../artifacts/acceptance/9c1700c5-c326-45c7-9d31-5951cb191c2f/native_time_mouse_checkpoint.json) enthalten je einen NativeAttempt/Return, einen Capture/Suppress und eine Host-Quittung. Am Ende zeigen beide Pausenbilder00:29:19 und denselben aktiven Pausenbutton. Bilder sind betrachtet; Geschwindigkeit-Bilder wurden zeitlich nacheinander bei laufender Hostuhr aufgenommen.

![Host pausiert](media/paused_host_time_mouse.bmp)
![Client pausiert](media/paused_client-1_time_mouse.bmp)

[100 gezielte Reader-/Mouseprüfungen](time_mouse_tests.md), [Kalenderfehlerprüfungen](time_failure_tests.md) und [SDK-Vertragsprüfungen](time_mouse_sdk_tests.md) sichern die eigenen Quellstände. Alte Tastaturabnahme wird übernommen. Der neue MCP-Achtfeldvertrag trennt Sitzungs- und lokale Weltepoch. HUD-Reader bindet Mode+58-Spielzustand; NativeOpen löst dessen Welt separat auf. Beide zuvor falschen Diagnosevergleiche lehnten vor dem nativen Aufruf ab und sind in den Originalquittungen erhalten.

[Ursprünglicher Nutzerfehler und Savekopien](../artifacts/user_time_failure/20261012T145623477425Z/checkpoint.json), [letzte Sicherung vor Neustart](../artifacts/acceptance/1491963d-1c87-4732-92b0-a9dc81cea84c/summary.json). Reguläres Platzieren, Regionen, Scenery, Wirtschaft, Events und vollständige Host-Simulation sind damit nicht bestätigt. Der normale UI-Remove-Kandidat folgt im aktuellen Lauf; die großen Abnahmen mit vier Instanzen und großem Save bleiben zuletzt.
''',encoding='utf-8')
state=ROOT/'_drafts/implementation_state.md';text=state.read_text(encoding='utf-8')
begin=text.index('## Aktueller Zeitbutton-Fehler und Kandidat');end=text.index('## Frühere MCP-Eingabekandidaten',begin)
text=text[:begin]+'''## Aktueller nativer Zeitbutton-Stand und UI-Remove

[Native Mouse2 bestanden](time_mouse_two.md): Lauf e69f112a-5975-4a8d-b383-682e29773ab7, deployed DLL d2c45ff824a4576ddd945e92e699c9ab4cea78129341f95f8eee1f1228e3fbe9. Client9/Pause0 und HostNormal1/Pause0 durch tatsächliche ItemButton.Activate-Eintritte mit Caller58095C. Vier Hostsetter, je ein Capture/Suppress, kein lokaler TimeActivate-Originalaufruf; ClientNativeClock0. Beide finalen Pausenbilder00:29:19 betrachtet, Session/Calendar unverändert verbunden/unlocked. Sourcefreeze154136488327 und native Quittungen sind im Bericht verlinkt. 100 Reader/Mouse-,12 Transport- und7 SDK-/Runtimeprüfungen mit eigenen Hashgrenzen; alte Tastatur-/Persistenz-/NativeFree-/TypedRemovebelege werden übernommen.

Ursprüngliche Nutzerwelten aus Lauf01022 sind als geprüfte lokale Dateien erhalten: [Originalfehlercheckpoint](../artifacts/user_time_failure/20261012T145623477425Z/checkpoint.json), [neueste Sicherung1491963d](../artifacts/acceptance/1491963d-1c87-4732-92b0-a9dc81cea84c/summary.json). Host-Savebindungen für neue Gebäude waren unbestätigt; echte Dateipersistenz ist bestätigt. Beide Diagnosefehler vor dem Mouseproof (Session-/Worldepoch und State-/Worldpointer verwechselt) liefen ohne NativeAttempt und bleiben erhalten. Aucun Hot-Reload; eigene Testprozesse wurden nach Sicherung neu gestartet.

Nächster aktueller Nativeauftrag ist [UI-Remove7a368fb9](../artifacts/acceptance/7a368fb9-4f8b-4cf1-b30b-2f86dd888786/summary.json): normale BuildingInfoWindow-Entfernung via genuine Record, tatsächlich gemeinsamer Clientauftrag. Noch nicht als bestanden markieren. Root owns Produktion/Build/MCP/native Abnahme, kein Computer Use, keine private Saveänderung, keine Veröffentlichung. Kleine Zweiwelt hat Vorrang; vier Instanzen/große Welt bleiben finale Abnahmen.

Volle Koopaktionserfassung, Platzieren/Scenery/Region, Wirtschaft und Events bleiben offen. [BusinessReplica](business_runtime.md) und [SimulationBoundary](simulation_boundary.md) zeigen konkrete fehlende sichere Live-Anwendung und vollständige Simulationsgrenze bei lebender UI/Kamera. Darauf wird keine breite Zustandsreplikation aufgebaut.

'''+text[end:]
text=text.replace('Aucun Hot-Reload','Kein Hot-Reload')
state.write_text(text,encoding='utf-8')
script=ROOT/'tools/update_progress.py';text=script.read_text(encoding='utf-8')
old='Zeitsteuerung über native MCP-Tasteneingaben ist bestätigt. Ein echter Client-Klick auf den schnellen HUD-Zeitbutton setzte die eigene Uhr auf9 und beendete die Sitzung. Beide bearbeiteten Welten sind lokal mit geprüften Prüfsummen gesichert; beide Instanzen sind pausiert. Der Mouse-Fix ist in Arbeit und noch nicht im Spiel bestätigt.'
new='Der Client-Zeitbuttonfehler ist nativ behoben: vier echte HUD-Buttonaufrufe auf Client und Host, vier Hostquittungen, Clientuhr0. Beide Pausenbilder zeigen00:29:19; die Sitzung bleibt verbunden. Nutzerwelten sind mit geprüften Prüfsummen gesichert. Reguläre Gebäudeentfernung wird jetzt geprüft.'
if old not in text:raise ValueError('progress current paragraph changed')
text=text.replace(old,new,1)
note_start=text.index('<p class="open"><a href="implementation_state.md">Aktueller Blocker</a>')
note_end=text.index('<table><thead>',note_start)
text=text[:note_start]+'''<p class="pass"><a href="time_mouse_two.md">Native Zeitbutton-Abnahme bestanden</a>: Client9/Pause und HostNormal/Pause, je ein Hostsetter. <a href="time_mouse_tests.md">100 Reader-/Mouseprüfungen</a>, <a href="time_failure_tests.md">Kalenderregressionen</a>, <a href="time_mouse_sdk_tests.md">SDK-Vertragsprüfungen</a>.</p>
<details><summary>Bestätigte gemeinsame Pause nach dem Fix</summary><img src="media/paused_host_time_mouse.bmp" alt="Host: Pausenbutton aktiv, Uhr00:29:19"><img src="media/paused_client-1_time_mouse.bmp" alt="Client: Pausenbutton aktiv, Uhr00:29:19"></details>
'''+text[note_end:]
script.write_text(text,encoding='utf-8')
print('info: native mouse checkpoint published='+str(target))
