"""Publish completed native cash and same-day clock acceptance."""
import json
import shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
draft=ROOT/'_drafts'
time_id='8493a235-2644-4e2c-8ad1-36e009aa5f19'
remove_id='cbae9529-d65e-48f1-89bd-5d954ba97476'
time_summary=json.loads((ROOT/'artifacts/acceptance'/time_id/'summary.json').read_text())
remove_summary=json.loads((ROOT/'artifacts/acceptance'/remove_id/'summary.json').read_text())
if not time_summary['passed'] or not remove_summary['passed']:raise RuntimeError('native acceptance is incomplete')
for row in time_summary['checks']:
    if row['check']=='native-frame-artifact':
        source=Path(row['result']['path']);peer='host' if 'host' in source.parts else 'client-1'
        shutil.copy2(source,draft/'media'/('live_state_'+peer+'.bmp'))
doc='''# Host-Zustand auf zwei Instanzen

Im Spiel bestanden: Der Client entfernt das größere Gasthaus, beide Instanzen erhalten genau 1.000 zurück. Der Host erhält beide ACKs; native Entfernung und Referenzfreigabe sind bestätigt. Geld 94.600 und alle 20 Buchungsgruppen stimmen überein. [Native Abnahme](../artifacts/acceptance/cbae9529-d65e-48f1-89bd-5d954ba97476/summary.json).

Anschließend fordert der Client normale Geschwindigkeit und Pause an. Der Host läuft von Sekunde 28 auf 220; der Client übernimmt den tatsächlichen Kalender bei eigener Simulationsskala 0. Nach Pause zeigen beide 00:03:40 und unveränderte gleiche Buchungen. Ein tatsächlicher Client-Store ist bestätigt. [Zeitabnahme](../artifacts/acceptance/8493a235-2644-4e2c-8ad1-36e009aa5f19/summary.json).

Der [ursprüngliche Fehler](../artifacts/acceptance/006bb002-b01c-4474-9ad9-2017a25c3546/summary.json) bleibt erhalten. Der Schutz verlangte Edit0, während der native Remove seine Rückerstattung bei Edit1 ausführt. Astra mit High Effort lieferte die gezielte Korrektur; [45 Checks und reproduzierter ursprünglicher Fehler](../tools/astra_cash_failure/runs/7fb8d519-2068-48a2-b320-4d6a85f6ce29/summary.json). Es wurde keine Aktion in der Unknown-Sitzung wiederholt; die Abnahme nutzte eine frisch geladene kleine Welt.

Lauf 276aa01e-c7a5-4e2f-99e9-f17fd26decf3, DLL 22f69b792cf0ea738fb757fb2bdacecf9873b53db262a1f8754b4b8ff5b0c70b. [Vollständiger aktueller Host-Save](../artifacts/acceptance/38dbbd38-c398-43af-9bbc-d2f9d11a3df8/summary.json): 206.983 Bytes, Snapshot 17b2a335-ba4d-4273-bbf6-da10b7fa7fea. Private Saves bleiben erhalten.

Tageswechsel, vollständige Besucher-/Wirtschafts-/Eventzustände, weitere Aktionsfamilien und native Lobby sind noch offen. Diese Abnahme beweist die Zeitübernahme innerhalb eines Tages und den bestätigten Remove-Geldpfad. Vier Instanzen, große Welt und getrennte Steam-Rechner folgen zuletzt.

![Host nach bestätigter Zeitübernahme](media/live_state_host.bmp)

![Client mit gleicher Uhr und gleichem Geld](media/live_state_client-1.bmp)
'''
(draft/'live_host_state_two.md').write_text(doc,encoding='utf-8')
section='''<section id="live-state-current"><h2>Host-Zustand auf zwei Instanzen bestätigt</h2><p>Größeres Gasthaus gemeinsam entfernt: je 1.000 zurück, beide Geld 94.600 und beide Host-ACKs bestätigt. Astra hat den nativen Edit-Scope-Fehler behoben; 45 Regressionen bestanden.</p><p>Host-Uhr von Sekunde 28 auf 220 gelaufen, Client übernimmt denselben Kalender bei eigener Simulationsskala 0. Gemeinsame Pause, gleiche Buchungen und aktueller Host-Save bestätigt. <a href="live_host_state_two.md">Native Belege und ursprüngliche Fehlerursache</a>.</p><p>Tageswechsel, übrige Wirtschaft und Events sowie Szenenobjekte und native Lobby sind noch in Arbeit.</p><img src="media/live_state_host.bmp" alt="Host mit Uhr 00:03:40 und Geld 94.600"><img src="media/live_state_client-1.bmp" alt="Client mit derselben Uhr und demselben Geld"></section>'''
for path in (draft/'progress.html',ROOT/'tools/update_progress.py'):
    content=path.read_text(encoding='utf-8');start=content.index('<section id="live-state-current">');end=content.index('</section>',start)+len('</section>')
    path.write_text(content[:start]+section+content[end:],encoding='utf-8')
path=draft/'implementation_state.md';text=path.read_text(encoding='utf-8')
start=text.index('## Aktueller Stand - Host-Zustand und Rückerstattung');end=text.index('## Frühere Place-/Move-Beobachtungen',start)
checkpoint='''## Aktueller Stand - native Host-Zustandsübernahme

Owner Root, Produktion/Build/Spiel-MCP. Voller spielbarer Zweier-Koop ist weiterhin aktiv; kleine Welt zuerst. Kein Computer Use, kein Commit/Push, private Saves erhalten. Astra High darf konkrete Blocker lösen. Bestandene Funktionen werden übernommen.

Aktueller Lauf 276aa01e-c7a5-4e2f-99e9-f17fd26decf3, installierte DLL22f69b792cf0ea738fb757fb2bdacecf9873b53db262a1f8754b4b8ff5b0c70b. [Gasthaus-Remove und SameDay-Zeitübernahme bestanden](live_host_state_two.md): Geld94.600,20 Gruppen gleich, ACK3, ClientClock0, native Zeit28->220 und Pause. Beide Abschlussbilder betrachtet. Frühere normale Place/Move/Remove-Belege bleiben geschlossen.

[Originaler Fehler006bb002](../artifacts/acceptance/006bb002-b01c-4474-9ad9-2017a25c3546/summary.json) und Logs erhalten. Astra identifiziert Edit1 im normalen Remove-Refund; eng begrenztes Einmalpermit korrigiert,45Checks mit tatsächlicher Baseline-Reproduktion bestanden. Kein Replay in Unknown-Sitzung. [Gesicherter aktueller Host](../artifacts/acceptance/38dbbd38-c398-43af-9bbc-d2f9d11a3df8/summary.json): snapshot17b2a335-ba4d-4273-bbf6-da10b7fa7fea,206983Bytes,SHA216ecf426d89663381915da783165537547cccb32d7a623b10c1a03b73ba1270. Dieser Stand ist die nächste Basis.

Source weiter als installierte DLL: Day-Cash-Rebase1 integriert,37Day+6Windows und49gezielte Mergechecks übernommen, [vollständiger Build](../artifacts/build_history/20261012T230103695944Z/build-1.log) bestanden. Kein neuer Tageswechsel-/History-Nativenachweis. GUI17 integriert und gebaut; nativer Scene-Katalog-Read für echte Namen kommt als kleine separate Scheibe. Rejoin enthält bestehenden Time-Owner-Hold, NativeLoad und MappingACK; nativer Steam-Lobby-/MCP-Desktop-Flow in Staging. Weitere Subscriber-/Loan-/Eventzustände im gleichen Hostframe werden ergänzt.

Nächste Aktion: echten Scene-Katalog-Read integrieren, auf gesicherter aktueller Host-Welt Szenenobjekt-UI und Tageswechsel abnehmen. Native Lobby aus geliefertem Flow integrieren. Keine Wiederholung geschlossener unveränderter Tests. Vier Instanzen, große Welt und Online-Abnahme zuletzt.

'''
path.write_text(text[:start]+checkpoint+text[end:],encoding='utf-8')
print('info: native cash and same-day clock milestone published')
