"""Publish the completed two-instance persistence proof from retained receipts."""
import argparse
import hashlib
import json
from pathlib import Path
import uuid

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('native', 'placement', 'capture', 'reload', 'view'):
        parser.add_argument('--' + name, required=True, type=lambda value: str(uuid.UUID(value)))
    parser.add_argument('--expected-dll-sha256', required=True)
    args = parser.parse_args()
    receipts = {}
    for name in ('native', 'placement', 'capture', 'reload', 'view'):
        path = ROOT / 'artifacts/acceptance' / getattr(args, name) / 'summary.json'
        value = json.loads(path.read_text(encoding='utf-8'))
        if value.get('passed') is not True or value['parameters'].get('players', 2) != 2 or value['parameters']['expected_dll_sha256'] != args.expected_dll_sha256:
            raise RuntimeError('persistence receipt failed or differs: ' + name)
        receipts[name] = {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'value': value}
    values = {name: item['value'] for name, item in receipts.items()}
    if len({values[name]['runtime_run_id'] for name in ('native', 'placement', 'capture')}) != 1 or values['reload']['runtime_run_id'] != values['view']['runtime_run_id']:
        raise RuntimeError('persistence runtime groups differ')
    if values['capture']['parameters']['placement'] != args.placement or values['reload']['parameters']['retained_capture'] != args.capture or values['view']['parameters']['native_acceptance'] != args.reload:
        raise RuntimeError('persistence receipt chain differs')
    capture = values['capture']
    if values['reload']['parameters']['snapshot_id'] != capture['output_snapshot_id']:
        raise RuntimeError('persisted native input file differs')
    for identifier in (args.placement, args.reload, args.view):
        path = ROOT / 'artifacts/acceptance' / identifier / 'frame_checkpoint.json'
        frame = json.loads(path.read_text(encoding='utf-8'))
        source = path.with_name('summary.json')
        if frame['source_sha256'] != hashlib.sha256(source.read_bytes()).hexdigest() or set(frame['frames']) != {'host', 'client-1'}:
            raise RuntimeError('native visual checkpoint differs')
        for item in frame['frames'].values():
            if hashlib.sha256(Path(item['retained_path']).read_bytes()).hexdigest() != item['file_sha256']:
                raise RuntimeError('retained native image differs')
    current_run = values['reload']['runtime_run_id']
    for peer in ('host', 'client-1'):
        manifest = json.loads((ROOT / '_MPTest' / peer / 'instance.json').read_text())
        if manifest['run_id'] != current_run or manifest['installed_binary_sha256']['MT2_Multiplayer.dll'] != args.expected_dll_sha256:
            raise RuntimeError('current owned runtime differs: ' + peer)
    record = {'schema_version': 1, 'run_id': current_run, 'dll_sha256': args.expected_dll_sha256, 'receipts': {name: {key: item[key] for key in ('path', 'sha256')} for name, item in receipts.items()}, 'full_coop_proven': False}
    checkpoint = ROOT / 'artifacts/acceptance' / args.reload / 'persistence_checkpoint.json'
    if checkpoint.exists():
        raise RuntimeError('persistence checkpoint is immutable')
    checkpoint.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
    text = f'''# Kleine Zweier-Welt: Speichern und Neustart

Fertig nach echter Spielabnahme: vollständige Datei, nativer Load auf beiden Instanzen, eine diagnostische Client-Platzierung mit einmal400 Kosten, lokales Speichern und tatsächliches Wiederladen nach Neustart. [Nachweiskette](../artifacts/acceptance/{args.reload}/persistence_checkpoint.json). Diese bestandenen Zustände bleiben erhalten; erneute Spielprüfung nur bei einer Änderung des betroffenen Codepfads.

Aktueller eigener Lauf `{current_run}`, DLL `{args.expected_dll_sha256}`. [Load und frische Weltidentität](../artifacts/acceptance/{args.native}/summary.json), [Platzierung](../artifacts/acceptance/{args.placement}/summary.json), [lokaler Save ohne Client-Verteilung](../artifacts/acceptance/{args.capture}/summary.json), [Neustart und NativeLoad](../artifacts/acceptance/{args.reload}/summary.json).

Die lokale Datei hat {capture['output_snapshot_bytes']} Bytes, SHA256 `{capture['output_snapshot_sha256']}`. Auf beiden Instanzen bleiben Geld94600, Tag1 und alle20 Buchungsgruppen gleich. Je genau eine SmallInn bei X7950/Z8550/Y63,5211448669. Die lokale Objekt-ID ist nach NativeLoad1086 statt3162; sie dient nicht als portable Sitzungsidentität.

[Betrachtete Nahansichten nach Neustart](../artifacts/acceptance/{args.view}/frame_checkpoint.json) zeigen dasselbe Gebäude, Pause,00:00:28 und94.600 Geld. Der überlappende Baum bleibt ein offener Fehler des diagnostischen Baupfads. Vollständiger regulärer Baucommit mit Scenery und Undo, Entfernen, laufende Wirtschaft, Events und die übrige Aktionsmatrix sind noch nicht fertig. Der [vorherige Zeitnachweis](native_open.md) bleibt übernommen.

Der [MCP-Status-Lesefix](mcp_status_publication.md) behandelt ein kurzzeitiges WinError2 mit maximal250ms Publikationswait und frischer Prozessidentität. Der ursprüngliche b77-Abbruch bleibt erhalten; die Fortsetzung nutzte den noch ungeladenen Prozess, ohne Loadwiederholung. Die [MCP-Regression](../test/mcp/evidence/checks.json) besteht. 36 gezielte [Acceptance-Unit-Tests](acceptance_regressions.md) bestehen. Sie prüfen Dateikorruption, Identität, Teilquittungen und Wiederholungsschutz. Native Beobachtungen bleiben separate Belege.

Nur zwei Instanzen in der kleinen Welt. Vier Instanzen, große Welt und getrennte Steam-Rechner folgen zur finalen Abnahme. Private Spielstände bleiben unverändert.
'''
    (ROOT / '_drafts/small_persistence.md').write_text(text, encoding='utf-8')
    path = ROOT / '_drafts/implementation_state.md'
    state = path.read_text(encoding='utf-8')
    start = state.index('## Aktueller MCP-Eingabekandidat\n')
    end = state.index('## Vorherige native Zeit-Runtime-Abnahme\n', start)
    current = f'''## Aktueller MCP-Eingabekandidat

[Kleine Zweier-Persistenz fertig](small_persistence.md): Lauf `{current_run}`, DLL `{args.expected_dll_sha256}`. NativeLoad/frische Weltidentität{args.native}, diagnostische Client-Platzierung{args.placement}, lokaler Save{args.capture}, Neustart/Load{args.reload}, betrachtete Nahansicht{args.view}. SmallInn je genau einmal, Kosten400, Geld94600,20 Buchungsgruppen unverändert. NativeUID nach Neustart1086 statt3162. Kein weiteres unverändertes Gesamt-Retest; Nutzer verlangt Übernahme bestandener Zustände und Subagent-Unit-Tests für Regressionen.

OwnerRoot: Produktion, Build, MCP. Nächster Schritt: Prop-Lease-/Free-Reviewdelta und tatsächliche Rootservices integrieren, selection-/highlightschonendes Entfernen nativ auf zwei Instanzen beweisen; vollständiger Place-Commit mit Scenery/Undo folgt. RemoveBackend, PropHost/Pool, NativeFree und die globalen Allocationservices sind gelesen. Originalcause, preAssign-Lifetime und Unknown-Cleanup sind in versiegelten Kandidaten korrigiert. Der konkrete CRT-/IAT-Installer und seine gezielte private Holder-MCP-Probe werden eingebaut. Ein Subagent erstellt den tatsächlichen RemoveRuntimeadapter, ein zweiter die neuen MCP-/Probe-Unit-Tests. 36 Acceptance-Regressionen sind bestanden; unveränderte frühere Prüfungen werden übernommen. Gemeinsame Zeit/HUD zuvor bestanden und übernommen; laufende Wirtschaft/Events und vollständiger Koop bleiben offen.

Offizielle Enginequelle ist lokal `tools/engine_source/vectorstorm`, Commit5b8c51f69b9df29554b0e240f38a946705f2f1fb vom30.03.2026. Engine-Source gegen installierten Build prüfen; mmo-Spielklassen nicht als darin vorhanden annehmen. Spielsteuerung ausschließlich MCP. Vier Instanzen/große Welt final; private Saves, Commits und Veröffentlichungen bleiben unangetastet.

'''
    path.write_text(state[:start] + current + state[end:], encoding='utf-8')
    print('info: persistence checkpoint and current state published; no game effects')


if __name__ == '__main__':
    main()
