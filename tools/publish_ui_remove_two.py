"""Retain the one native UI removal and its read-only completion evidence."""
from pathlib import Path
import hashlib,json,shutil
ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/'artifacts/acceptance/fc0d8451-5988-48f4-ae1e-54b1a017bbf3'
summary=json.loads((folder/'summary.json').read_text(encoding='utf-8'))
if summary.get('passed') is not True or summary.get('UI_command_replayed') is not False:raise ValueError('native UI Remove completion missing')
admission=json.loads((ROOT/'artifacts/acceptance/7a368fb9-4f8b-4cf1-b30b-2f86dd888786/summary.json').read_text(encoding='utf-8'))
copied=[]
for before,value in ((True,admission),(False,summary)):
    for item in (entry['result'] for entry in value['checks'] if entry['check']=='native-frame-artifact'):
        source=Path(item['path']);name='ui_remove_'+('before' if before else 'after')+'_'+item['peer']+'.bmp'
        if hashlib.sha256(source.read_bytes()).hexdigest()!=item['file_sha256']:raise ValueError('UI Remove frame changed')
        shutil.copy2(source,ROOT/'_drafts/media'/name);copied.append({'frame':name,'sha256':item['file_sha256'],'inspected':True})
checkpoint={'candidate':'NATIVE-UI-REMOVE-1','passed':True,'run_id':summary['runtime_run_id'],
    'DLL':admission['parameters']['expected_dll_sha256'],'action_uuid':summary['action_uuid'],
    'admission':str(ROOT/'artifacts/acceptance/7a368fb9-4f8b-4cf1-b30b-2f86dd888786/summary.json'),
    'completion':str(folder/'summary.json'),'UI_command_replayed':False,'frames':copied,
    'outcome':'one actual Client BuildingInfoWindow intent, Host and Client execution and retirement confirmed; ACK mask3; cash95000; 20 equal ledger groups',
    'full_coop_proven':False}
target=folder/'native_ui_remove_checkpoint.json'
if target.exists():raise ValueError('native UI Remove checkpoint exists')
target.write_text(json.dumps(checkpoint,indent=2)+'\n',encoding='utf-8')
(ROOT/'_drafts/ui_remove_two.md').write_text('''# Normale Gebäudeentfernung auf Host und Client

Eine tatsächliche Client-BuildingInfoWindow.DestroyBuilding-Aktion ist bestätigt. Lauf e69f112a-5975-4a8d-b383-682e29773ab7, DLL d2c45ff824a4576ddd945e92e699c9ab4cea78129341f95f8eee1f1228e3fbe9, Auftrag b1ab6dde-c017-4cd7-9451-e082ff1ff900. Die native UI erzeugt einen Client-Intent vor jeder lokalen Ausführung. Host führt einmal aus; Client wendet die Bestätigung einmal an. Beide melden vollständige native Referenzfreigabe, Host-ACK-Maske3, Revision1, keine Fehler.

Geld steigt auf beiden von94600 auf95000, alle20 Buchungsgruppen stimmen überein. Die Inn ist in beiden Vorher-Bildern sichtbar und in beiden Nachher-Bildern entfernt. Bauziel1/3 geht auf0/3 zurück, Undo zeigtDestroy Small Inn. Die Undo-Ausführung selbst ist weiterhin offen.

[Unveränderter UI-Nachweis](../artifacts/acceptance/fc0d8451-5988-48f4-ae1e-54b1a017bbf3/native_ui_remove_checkpoint.json), [ursprüngliche Admission](../artifacts/acceptance/7a368fb9-4f8b-4cf1-b30b-2f86dd888786/summary.json), [lesende Completion](../artifacts/acceptance/fc0d8451-5988-48f4-ae1e-54b1a017bbf3/summary.json). Das erste Prüfskript erwartete fälschlichOrigin2. Der tatsächliche SourceenumwertBuildingWindow ist1; kein Produktionsfehler und kein Client-Original vor Hostauthority. Der Auftrag wurde ohne erneuten UI-Befehl abgeschlossen. Diese Fehlbeobachtung bleibt erhalten.

![Client vor der Entfernung](media/ui_remove_before_client-1.bmp)
![Client nach der Entfernung](media/ui_remove_after_client-1.bmp)

[Zeitbuttons und Pause](time_mouse_two.md) sind im selben Lauf bestätigt und bleiben übernommen. Bestehende UI-Ziel-,Bridge-,Consumer- undSDKprüfungen behalten ihre eigenen eingefrorenen Quellstände; neue Root-Zeitänderungen sind separat nachgewiesen. Entfernung ist für aktuelle, eindeutig gebundene Snapshotgebäude belegt. Reguläres Platzieren, neue ungebundene Gebäude, Scenery, Regionen, synchrones Undo/Redo, laufende Wirtschaft und Events bleiben offen. [Business](business_runtime.md) und [Simulationsgrenze](simulation_boundary.md) begrenzen die weitere Hostreplikation.
''',encoding='utf-8')
state=ROOT/'_drafts/implementation_state.md';text=state.read_text(encoding='utf-8')
old='Nächster aktueller Nativeauftrag ist [UI-Remove7a368fb9](../artifacts/acceptance/7a368fb9-4f8b-4cf1-b30b-2f86dd888786/summary.json): normale BuildingInfoWindow-Entfernung via genuine Record, tatsächlich gemeinsamer Clientauftrag. Noch nicht als bestanden markieren.'
new='[Normale UI-Remove2 ist nativ bestanden](ui_remove_two.md): Admission7a368fb9 und read-only Completionfc0d8451, Auftragb1ab6dde. Ein tatsächlicher ClientBuildingWindow-Intent vor lokalem Effekt, je eine Host-/Clientausführung und Referenzfreigabe, Host-ACK-Maske3, Geld95000,20 gleiche Buchungsgruppen. Alle vier Vorher-/Nachher-Bilder betrachtet. Prüfskript-Origin2 war falsch; actualenumBuildingWindow1. Keine UI-/Actionwiederholung. Beide kleinen Instanzen sind pausiert und verbunden; die Inn ist entfernt. Nächster Engine-Scope ist der Owner-/Scene-/Task-/Ticker-Census, weil volle Simulationsgrenze und sichere Live-Business-Anwendung noch fehlen; Subagent nur Staging, Root bleibt alleiniger Gameowner. Read-only Completionregressionen laufen getrennt.'
if old not in text:raise ValueError('UI checkpoint current paragraph changed')
text=text.replace(old,new,1);state.write_text(text,encoding='utf-8')
script=ROOT/'tools/update_progress.py';text=script.read_text(encoding='utf-8')
text=text.replace('Reguläre Gebäudeentfernung wird jetzt geprüft.','Normale BuildingInfoWindow-Gebäudeentfernung ist auf beiden Instanzen bestätigt.',1)
text=text.replace('der Auftrag läuft über den Spiel-MCP; die normale Entfernen-Schaltfläche folgt.','der Auftrag läuft über den Spiel-MCP; die normale Entfernen-Schaltfläche folgt.')
text=text.replace('Der Auftrag läuft über den Spiel-MCP; die normale Entfernen-Schaltfläche folgt.','<a href="ui_remove_two.md">Auch die normale BuildingInfoWindow-Entfernung</a> ist jetzt bestätigt: ein Client-UI-Intent vor lokalem Effekt, je eine Ausführung und Freigabe sowie beide Host-ACKs.',1)
marker='<table><thead>'
note='''<p class="pass"><a href="ui_remove_two.md">Normale Gebäudeentfernung nativ bestanden</a>: SmallInn auf Host und Client genau einmal entfernt, Referenzen freigegeben, Geld95.000. Keine Wiederholung des UI-Befehls.</p>
<details><summary>Client vor und nach der normalen Entfernung</summary><img src="media/ui_remove_before_client-1.bmp" alt="Client: Inn vorhanden, Geld94.600"><img src="media/ui_remove_after_client-1.bmp" alt="Client: Inn entfernt, Geld95.000"></details>
'''
if 'Normale Gebäudeentfernung nativ bestanden' not in text:text=text.replace(marker,note+marker,1)
script.write_text(text,encoding='utf-8')
print('info: native UI Remove checkpoint published='+str(target))
