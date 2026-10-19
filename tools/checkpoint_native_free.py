"""Retain the completed two-peer native holder proof and the current next action."""
import argparse
import hashlib
import json
from pathlib import Path
from native_free_acceptance import confirmed

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--acceptance', required=True)
    args = parser.parse_args()
    directory = ROOT / 'artifacts/acceptance' / args.acceptance
    summary_path = directory / 'summary.json'
    summary = json.loads(summary_path.read_text(encoding='utf-8'))
    frames = json.loads((directory / 'frame_checkpoint.json').read_text(encoding='utf-8'))
    if summary.get('passed') is not True or summary.get('state') != 'confirmed-native-free-two':
        raise ValueError('native Free acceptance not complete')
    records = [item['result'] for item in summary['checks'] if item['check'] == 'native-holder-confirmed']
    if {(item['peer'], item['kind']) for item in records} != {(peer, kind) for peer in ('host', 'client-1') for kind in ('prop', 'building')} or len(records) != 4:
        raise ValueError('four unique current native holder receipts required')
    for item in records:
        value = item['receipt']
        confirmed(value, value['token'], item['kind'], value['world_epoch'], value['world_generation'])
    text = f'''# Native Speicherfreigabe auf zwei Instanzen

Vier echte Engine-Holder sind vollständig quittiert: je Prop und Building auf Host und Client. Factory, Konstruktorlesung, D0, SizedDelete und CRT-free jeweils genau einmal; keine Ausnahme. Gemeinsame Allocation-/Free-IDs je Prozess1 und2. [Abnahme](../artifacts/acceptance/{args.acceptance}/summary.json) und [betrachtete native Bilder](../artifacts/acceptance/{args.acceptance}/frame_checkpoint.json).

Lauf `{summary['runtime_run_id']}`, DLL `{summary['parameters']['expected_dll_sha256']}`. Tatsächliche CRT-/Code-/IAT-/Trampolinerouten mit Maske3 bestätigt. Beide Bilder zeigen die gleiche pausierte Inselkarte mit Uhr00:00:28 und Geld94.600. Private Holder ändern keine Weltobjekte; Gameplay-Entfernen bleibt offen. Der Save-/Load-Lauf3309017f diente nur als Ausgangspunkt für den neuen Build.

[Produktionskopie](../artifacts/native_free_integration/20261012T113121388259Z/copy_receipt.json): neun Dateien mit identischen SHA256. [Release-Build](../artifacts/build_jobs/native_free_core_20261012/status.json), CTest3/3 und1095 erforderliche Proxy-Exporte bestanden. Sechs bestehende Makro-Neudefinitionswarnungen in drei älteren Quellen bleiben im Buildlog sichtbar.

[Gezielte Unit-Tests](native_free_tests.md):12 Diagnoseprüfungen,4 SDK- und8 Quittungsgruppen. Unveränderte Ergebnisse werden hashgebunden übernommen. Keine weitere Nullholder-Spielprüfung ohne relevante Codeänderung.

Nächster Kandidat: tatsächlicher RemoveRuntime für die SmallInn mit eigener Dtor-/Undo-/WeakNULL-Quittung und erhaltenem Auswahlzustand. Freigabe der gehaltenen Prop-Leases benötigt noch das konkrete Quiescence-/Snapshotjournal. Reguläres Platzieren mit Scenery und Undo, laufende Wirtschaft, Events, vollständige Simulation und Steam bleiben offen.
'''
    (ROOT / '_drafts/native_free_two.md').write_text(text, encoding='utf-8')
    path = ROOT / '_drafts/implementation_state.md'
    old = path.read_text(encoding='utf-8')
    marker = '## Aktueller MCP-Eingabekandidat'
    insertion = f'''## Aktueller nativer Löschkandidat

[NativeFree2 bestanden](native_free_two.md). Aktiver Lauf `{summary['runtime_run_id']}`, deployed DLL `{summary['parameters']['expected_dll_sha256']}`. Vier echte private Factory/readback/D0/Sized/Free-Roundtrips `{args.acceptance}` mit allen Countern1. Zwei native Bilder betrachtet und hashgebunden erhalten. Dieser Zustand wird übernommen, kein unverändertes Retest. Frühere Zweier-Persistenz, diagnostischer Bau und Zeit/HUD bleiben historische bestandene Meilensteine.

Root besitzt Produktion/Build/MCP. FreeRuntime und einzelnes Observerpaar sind eingebaut, globales MinHookcleanup nach Vorbereitung bleibt verboten. Aktueller nächster Schritt: versiegelten RemoveRuntime einbauen, stable Snapshot-Key gegen aktuelle NativeWorld und lokalen FullUID prüfen, SmallInn einmal entfernen, eigenen Graph/Dtor/continuously-held WeakNULL und UI-Erhalt beobachten. Holder bleiben bis tatsächlichem Quiescence-/Snapshotjournal erhalten; keine gefälschte Retirement. SDK-/Diagnose-/Quittungs-Units12/4/8 und Acceptance36 sichern Regressionen. Zwei Instanzen zuerst, vier und großer Save final. FullPlace/Scenery/Undo, laufende Wirtschaft/Events, vollständige Simulation/Steam bleiben offen.

'''
    if '## Aktueller nativer Löschkandidat' in old:
        before, tail = old.split('## Aktueller nativer Löschkandidat', 1)
        old = before + marker + tail.split(marker, 1)[1]
    path.write_text(old.replace(marker, insertion + marker, 1), encoding='utf-8')
    receipt = {'passed': True, 'native_acceptance': args.acceptance, 'summary_sha256': hashlib.sha256(summary_path.read_bytes()).hexdigest(),
               'frames_checkpoint_sha256': hashlib.sha256((directory / 'frame_checkpoint.json').read_bytes()).hexdigest(),
               'runtime_run_id': summary['runtime_run_id'], 'holders_confirmed': 4, 'gameplay_remove_proven': False}
    output = directory / 'native_free_checkpoint.json'
    if output.exists():
        raise ValueError('immutable native Free checkpoint already exists')
    output.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(f'info: native Free checkpoint={output}; no game effects')


if __name__ == '__main__':
    main()
