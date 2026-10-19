"""Maintain the current implementation record without changing game state."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
path=ROOT/'_drafts/implementation_state.md'
text=path.read_text(encoding='utf-8')
start=text.index('## Aktueller nativer Löschkandidat')
end=text.index('## Aktueller MCP-Eingabekandidat',start)
current='''## Aktueller gemeinsamer Remove- und UI-Kandidat

[TypedRemove2](typed_remove_two.md) ist nativ bestanden: c9e11645-0bd8-4547-b691-05d68bab3711. Ein Client-Auftrag wird genau einmal beim Host und genau einmal im Client ausgeführt. Beide bestätigen Entfernung und tatsächliche Referenzfreigabe; Host-ACK-Maske3, Revision1, keine Fehler, Geld95.000 und20 gleiche Buchungsgruppen. Beide Nachher-Bilder sind betrachtet und erhalten. Der normale UI-Remove-Befehl ist damit noch nicht belegt.

Aktuell laufen zwei pausierte kleine Welten mit DLL5a3bf08b6732e2bd03ae7836731814de666e773e0d57e2a819188cb948bc4f0b, Lauf99a11838-fdeb-43ea-8324-108a26447abf. Fixture-Snapshotd0ab4499-602a-4e2d-a68d-4bb5403e4808, erforderliches Setupf9aeffa5-6214-4232-a6f1-52f3c9d303f8. Das Haus ist inzwischen entfernt. NativeRetirement19dd, NativeRemovec71, NativeFree0e2c, Zeit/HUD und Persistenz bleiben bestanden. Sie werden nicht unverändert erneut im Spiel geprüft. [34 Bridge- und4 SDK-Regressionen](typed_action_tests.md) sowie [28 Retirement-Regressionen und3 übernommene SDK-Prüfungen](remove_diagnostics_tests.md) sichern ihre eigenen eingefrorenen Quellstände.

Neuer, noch ungebauter Kandidat: Root-UIRuntime, ein gemeinsamer DestroyProps-Hook2D42C0, tatsächliche LocalTarget-/RecordTarget-Auflösung und private Bridge-Replays für HostExecution beziehungsweise ConfirmedHostApplication. UI-Ablehnung nutzt die vorhandene sichtbare NotificationOverlay. Ein angeforderter, fehlgeschlagener UI-Installer lässt keinen diagnostischen Direktfallback zu. Kopierbeleg des geprüften Capturecodes: [20261012T141518841636Z](../artifacts/ui_remove_integration/20261012T141518841636Z/copy_receipt.json). Der additive Root-Originzähler wird getrennt nachvollzogen. Ein Subagent prüft die neuen Root-Zielguards, ein zweiter die tatsächlichen Bridge-Replaygrenzen; vorhandene Unit-Gruppen werden nicht breit wiederholt.

Originaler neuer Guard-Fehlbeleg: [20261012T141812133698Z](../tools/ui_remove_tests/evidence/20261012T141812133698Z/target.log). Sechs abweichende Reader-Weltidentitäten wurden trotz gleichem Manager/UID akzeptiert. Root vergleicht nun vollständige Weltidentität vor und nach dem Lesen; gezielte neue Regressionen sind ausstehend. Es wurde dabei keine Löschung ausgeführt.

Die kritischen Simulationsnachweise bleiben konkrete Blocker: [BusinessReplica](business_runtime.md) zeigt zerstörenden History-/Loan-Load und Singleton-/PostResolve-Folgen; [SimulationBoundary](simulation_boundary.md) belegt direkte Building-/MapEntity-Ticks und UI-Wirtschaftsschreibwege außerhalb der Scheduler. Kein sicherer vollständiger Live-Apply und keine vollständige Client-Simulationssperre bei lebender Kamera/UI sind bewiesen. Darauf wird keine breite Zustandsreplikation aufgebaut. Events, laufende Wirtschaft, vollständiger Baucommit/Undo, weitere Aktionsgruppen, Steam und Online-Abnahme bleiben offen.

OwnerRoot: Produktion, Build, MCP und alleinige native Abnahme. Subagenten nur in ihren Staging-/Testpfaden. Kein Computer Use, keine private Saveänderung, kein Commit oder Release. Nächster Schritt: tatsächlichen BuildingInfoWindow.UICommand über genuine vsRecord im MCP anbinden, neue Guards abschließen, dann einmal UI-Clientauftrag auf beiden Instanzen beweisen. Vier Instanzen und großer Save bleiben finale Abnahmen.

'''
path.write_text(text[:start]+current+text[end:],encoding='utf-8')
progress=ROOT/'tools/update_progress.py'
text=progress.read_text(encoding='utf-8')
text=text.replace('gemeinsamer Befehlsweg folgt</td>','gemeinsamer Befehlsweg im aktuellen TypedRemove2-Beleg bestätigt</td>')
text=text.replace('gehaltene Remove-Referenz noch offen</td>','gehaltene Remove-Referenz im NativeRetirement2-Beleg freigegeben</td>')
marker='<tr><td>Gemeinsamer Client-Remove-Auftrag</td>'
if '<tr><td>Vollständige Host-Simulation</td>' not in text:
    text=text.replace(marker,'<tr><td>Vollständige Host-Simulation</td><td class="open">Konkrete Engine-Blocker</td><td><a href="business_runtime.md">Zerstörender Wirtschaftsload</a>; <a href="simulation_boundary.md">Scheduler-Bypässe und fehlende vollständige Schreibgrenze</a>; kein Live-Apply-Nachweis</td></tr>\n'+marker)
progress.write_text(text,encoding='utf-8')
print('info: current UI checkpoint updated; native acceptance unchanged')
