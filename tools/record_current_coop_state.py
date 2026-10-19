"""Reconcile current native evidence, regression receipts and concrete engine blockers."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
matrix=ROOT/'_drafts/action_matrix.md'
text=matrix.read_text(encoding='utf-8')
updates={
    'B04':'| B04 | Gebäude und andere platzierte Objekte löschen | P | P | P | E8: aktuelle eindeutig gebundene SmallInn per echter BuildingInfoWindow-UI, je eine Host-/Clientausführung und Freigabe, Rückfluss400. Neue ungebundene Objekte, andere Typen und Dekoration bleiben U |',
    'C01':'| C01 | Pause, Normal, Schnell und Schnellste Zeit | P | P | P | E7: native Tastatur0/1/3/9 plus echte Client-/Host-HUD-Aufrufe, je ein Hostsetter, Clientuhr0 und gleicher pausierter Zeittext. Volle Host-Simulation und Steam bleiben U |',
    'G03':'| G03 | Speichern, Autosave, Laden und Rückkehr zum Titel | S | L | S | Vollständige Snapshots und [kleine Save-/Neustart-/Load-Abnahme](small_persistence.md) bestätigt. Gemeinsame Save-UI, Autosave nach jeder Aktion und Wiederbeitritt bleiben U |',
    'J04':'| J04 | Zwei und vier Teilnehmer; Steam-P2P nach dem lokalen Test | P | P | P | Zwei eigene Instanzen mit Zeitsteuerung und begrenzter Gebäudeentfernung bestätigt. Historischer Vierer-Start/Load bleibt erhalten; funktionaler Vierer-/Steamtest folgt zuletzt |'
}
lines=text.splitlines()
for index,line in enumerate(lines):
    for key,value in updates.items():
        if line.startswith('| '+key+' |'):lines[index]=value
text='\n'.join(lines)+'\n'
marker='## Tatsächliche Belege\n\n'
new='''**E7 - gemeinsame Zeit und echte HUD-Buttons:** [Native Mouse2](time_mouse_two.md), DLL d2c45ff824a4576ddd945e92e699c9ab4cea78129341f95f8eee1f1228e3fbe9. Client9/Pause0 und HostNormal1/Pause0, vier bestätigte Hostsetter, ClientNativeClock0. Beide betrachteten Pausenbilder00:29:19, Session/Calendar verbunden. Frühere native Tastaturabnahme0/1/3/9 bleibt übernommen. Laufende komplette Simulation ist damit nicht bewiesen.

**E8 - normale Gebäudeentfernung:** [Native UI-Remove2](ui_remove_two.md), derselbe Zweierlauf. Ein ClientBuildingWindow-Intent vor lokalem Effekt, je ein Host-/Client-DestroyProp und vollständige Referenzfreigabe, ACK-Maske3, Geld94600→95000 und20 gleiche Buchungsgruppen. Die sichtbare Inn ist vorher vorhanden und nachher entfernt. Neue ungebundene Gebäude, Dekoration und Undo-Ausführung bleiben offen.

'''
if '**E7 - gemeinsame Zeit' not in text:text=text.replace(marker,marker+new,1)
text=text.replace('**E1 - lokale Host-Uhr:**','**E1 - frühere lokale Host-Uhr:**',1)
matrix.write_text(text,encoding='utf-8')
state=ROOT/'_drafts/implementation_state.md';text=state.read_text(encoding='utf-8')
text=text.replace('Read-only Completionregressionen laufen getrennt.','[Read-only Completionregressionen](ui_remove_completion_tests.md) sind abgeschlossen:51 Fälle übernommen,36 gezielte CLI-Prüfungen hinzugefügt, kein Game-/Actionreplay.',1)
marker='Volle Koopaktionserfassung, Platzieren/Scenery/Region, Wirtschaft und Events bleiben offen.'
text=text.replace(marker,'[Simulation-Owner-Census](simulation_owner_census.md) ergänzt136 vollständige COFF-Bodies,48 Layoutbelege und26 Schema-/Readguardunits. Konkrete Blocker: Mainthread-Immediatecallbacks fehlen in Queues; FrameTasksDone deckt nur zwei von sechs Lanes; Worker mutiert Arrays unterMutex; Scene-/Entity-/Camera-Pins fehlen. Kein sicherer nativer Censusadapter und kein Copyplan wurden erzeugt. Nächster erforderlicher Eingriff ist eine tatsächliche mutex-/ownergehaltene Snapshot-/Retainoberfläche mit World-/Subobjectzuordnung. Eine bloße Queue0-,Pointerread- oder Symbolbehauptung wird nicht als Simulationsautorität verwendet. Das Gesamtziel bleibt blockiert, die neuen Zeit-/UI-Remove-Meilensteine bleiben bestanden.\n\n'+marker,1)
state.write_text(text,encoding='utf-8')
script=ROOT/'tools/update_progress.py';text=script.read_text(encoding='utf-8')
text=text.replace('; normale UI-Anbindung offen','; <a href="ui_remove_two.md">normale BuildingInfoWindow-Anbindung bestätigt</a>',1)
text=text.replace('; kein Live-Apply-Nachweis</td></tr>','; <a href="simulation_owner_census.md">Immediatecallbacks, Worker-Lifetimes und fehlende Ownerpins</a>; kein vollständiger Live-Apply-Nachweis</td></tr>',1)
marker='<table><thead>'
note='<p><a href="ui_remove_completion_tests.md">51 Completion-Fälle und36 gezielte CLI-Prüfungen</a> sichern den lesenden Abnahmerunner. <a href="action_matrix.md">Aktuelle vollständige Aktionsmatrix</a>; neue ungebundene Gebäude, Regionen, Scenery, Wirtschaft und Events bleiben offen.</p>\n'
if '51 Completion-Fälle und36' not in text:text=text.replace(marker,note+marker,1)
script.write_text(text,encoding='utf-8')
print('info: current coop state, matrix and concrete simulation blocker reconciled')
