"""Replace the current state from native receipts without calling the game."""
import argparse
import hashlib
import json
from pathlib import Path
import uuid

ROOT = Path(__file__).resolve().parents[1]


def load(path):
    return json.loads(path.read_text(encoding='utf-8'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native', required=True)
    parser.add_argument('--hud', required=True)
    parser.add_argument('--dll-sha256', required=True)
    parser.add_argument('--build-job', default='hud_writer_observer_20261012')
    parser.add_argument('--placement')
    args = parser.parse_args()
    native_id, hud_id = str(uuid.UUID(args.native)), str(uuid.UUID(args.hud))
    native = load(ROOT / 'artifacts/acceptance' / native_id / 'summary.json')
    hud_dir = ROOT / 'tools/engine_analysis/calendar_authority/time_acceptance/runs' / hud_id
    hud = load(hud_dir / 'summary.json')
    calendar_proven = hud.get('calendar_formatter_readback_proven') is True
    frame_name = 'paused_frame_checkpoint.json' if calendar_proven else 'frame_checkpoint.json'
    frames = load(hud_dir / frame_name)
    correction_path = hud_dir / (frame_name.replace('_checkpoint.json', '_visual_correction.json'))
    if correction_path.exists():
        correction = load(correction_path)
        if correction['checkpoint_sha256'] != hashlib.sha256((hud_dir / frame_name).read_bytes()).hexdigest():
            raise RuntimeError('visual correction source differs')
        frames['visual_finding'] = correction['visual_finding']
    if not native['passed'] or not hud['passed'] or not hud.get('shared_hud_readback_proven'):
        raise RuntimeError('native load or shared HUD acceptance not confirmed')
    if native['runtime_run_id'] != hud['runtime_run_id'] or frames['runtime_run_id'] != hud['runtime_run_id']:
        raise RuntimeError('native runtime receipts differ')
    if frames['source_sha256'] != hashlib.sha256((hud_dir / 'summary.json').read_bytes()).hexdigest():
        raise RuntimeError('retained frame source receipt differs')
    for role in ('host', 'client-1'):
        manifest = load(ROOT / '_MPTest' / role / 'instance.json')
        if manifest['run_id'] != native['runtime_run_id'] or manifest['installed_binary_sha256']['MT2_Multiplayer.dll'] != args.dll_sha256:
            raise RuntimeError('active owned runtime differs: ' + role)
        retained = frames['frames'][role]
        if hashlib.sha256(Path(retained['retained_path']).read_bytes()).hexdigest() != retained['file_sha256']:
            raise RuntimeError('retained frame hash differs: ' + role)
    running = [entry['result'] for entry in hud['checks'] if entry['check'] == 'native_hud_running']
    paused = [entry['result'] for entry in hud['checks'] if entry['check'] == 'native_hud_paused_confirmed']
    if len(running) != 6 or len(paused) != 2:
        raise RuntimeError('three speed states and final pause required on both peers')
    for entry in running + paused:
        state = entry['result']
        actual = state['native_hud']
        effective = state['effective_scale']
        if not actual['complete'] or actual['time_items'] != 4 or actual['pause_window_visible'] != (effective == 0):
            raise RuntimeError('native HUD readback differs')
        for item in actual['items']:
            expected = item['scale'] == effective
            if item['active'] != expected or item['button_cached_active'] != expected or not item['button_enabled']:
                raise RuntimeError('native item or button cache differs')
        if entry['peer'] == 2 and (actual['native_selected_scale'] != 0 or state['hud_writer']['item_failed'] != 0):
            raise RuntimeError('client clock or writer differs')
    writer = next(entry['result']['hud_writer'] for entry in paused if entry['peer'] == 2)
    average = writer['update_microseconds'] / writer['update_calls']
    run = native['runtime_run_id']
    ledger = next(entry['result']['ledger'] for entry in native['checks'] if entry['check'] == 'initial-host-client-ledger')
    placement_note = ''
    if args.placement:
        placement_id = str(uuid.UUID(args.placement))
        placement_dir = ROOT / 'artifacts/acceptance' / placement_id
        placement = load(placement_dir / 'summary.json')
        placement_frames = load(placement_dir / 'frame_checkpoint.json')
        if placement['passed'] is not True or placement['runtime_run_id'] != run or placement_frames['source_sha256'] != hashlib.sha256((placement_dir / 'summary.json').read_bytes()).hexdigest():
            raise RuntimeError('placement and retained source/run differ')
        expense = next(entry['result'] for entry in placement['checks'] if entry['check'] == 'one-native-placement-expense')
        after = next(entry['result'] for entry in placement['checks'] if entry['check'] == 'paused-ledgers-after-placement')
        placement_note = f"[Diagnostische SmallInn-Platzierung](../artifacts/acceptance/{placement_id}/summary.json): ein Clientrequest, ein Objekt pro Instanz, {expense['cash_delta']} Kosten und {after['cash']} Geld auf beiden Peers. [Betrachtete Bilder](../artifacts/acceptance/{placement_id}/frame_checkpoint.json). {placement_frames['visual_finding']} Regulärer Baucommit mit vollständiger Validierung, Undo und Entfernen bleibt offen."
    calendar_note = ('Native Hostsample, Client-Formatter und sichtbarer Zeittext sind bestätigt. ' + frames['visual_finding']
                     if calendar_proven else 'Die Uhranzeige bleibt ungleich; Kalenderreplikation ist nicht bewiesen.')
    next_action = ('Kleine native Netzwerkaktionen und stabile Objekt-Lifecycles prüfen.' if calendar_proven
                   else 'Kalenderanzeige und Host-Kalenderzustand integrieren, danach kleine native Netzwerkaktionen prüfen.')
    text = f'''# Native Snapshot und Zeitbedienung

[Die kleine kanonische Welt](../artifacts/acceptance/{native_id}/summary.json) wurde auf Host und Client tatsächlich geladen. Datei, Stream, Reader und Worker sind jeweils einmal bestätigt, die Pins freigegeben und der aktuelle Weltursprung geprüft. Datum/Buchungstag {ledger['record_day']}, Geld {ledger['cash']}, Einkommen {ledger['record_income']} und 20 Buchungsgruppen stimmen nach dem Load bei Pause überein.

{placement_note}

Aktiver eigener Lauf `{run}`, DLL `{args.dll_sha256}`. [Release-Build, CTest 3/3 und 1095 Proxy-Exporte](../artifacts/build_jobs/{args.build_job}/status.json). [Reader-Kopie](../artifacts/hud_reader_integration/copy_receipt.json), [Writer-Kopie](../artifacts/hud_writer_integration/copy_receipt.json). Die aktuelle Witness-Optimierung ist separat [hashgebunden](../artifacts/hud_witness_integration/copy_receipt.json).

[Die echte Zeittastenprüfung](../tools/engine_analysis/calendar_authority/time_acceptance/runs/{hud_id}/summary.json) bestätigt Client-F2/F3/F4 und Host-F1 jeweils genau einmal. Beide Bedienleisten zeigen 1x, 3x, 9x und anschließend Pause mit passendem Buttoncache und Pausenrahmen. Der Client simuliert dabei weiterhin mit nativer Clock-Skala 0. Der Host führt alle vier Zeitänderungen aus. [Betrachtete Spielbilder](../tools/engine_analysis/calendar_authority/time_acceptance/runs/{hud_id}/{frame_name}). {calendar_note} Laufende Wirtschaft und Events sind nicht bewiesen.

Der HUD-Writer meldet 0 fehlgeschlagene Item-Stores. Sein gemessener Aufwand im Client beträgt durchschnittlich {average:.0f} Mikrosekunden je Item-Aufruf, maximal {writer['max_update_microseconds']} Mikrosekunden. Vier Items ergeben etwa {average * 4 / 1000:.1f} Millisekunden pro Frame. Die Witness-Optimierung verwendet weiterhin zwei vollständige Eigentümerprüfungen.

[Der vorherige HUD-Fehler](../tools/engine_analysis/calendar_authority/time_acceptance/runs/c9a762e0-c8e0-4c0e-ad4a-fae765fc8eb7/frame_checkpoint.json) bleibt als fehlgeschlagene Beobachtung erhalten. Der damalige Client zeigte bei quittiertem 1x weiterhin Pause. Der gezielte UI-Writer behebt diesen beobachteten Fehler im aktuellen Lauf.

[Der ursprüngliche Tag-0-Versuch](../artifacts/acceptance/a43fa189-9c3d-40e5-8087-c55674423cc4/summary.json) bleibt fehlgeschlagen. Clock.Resolve rekonstruiert FlatDay 0 aus Monat/Tag; nach dem ersten Load stehen Monat/Tag 1, der nächste Load ergibt FlatDay 1. Der kanonische zweite Roundtrip liefert ein stabiles Funktionsfixture und behebt diesen Beitrittsfehler nicht. [Tatsächliche Dateifelder](../artifacts/engine_analysis/calendar_snapshot/small_ffd8_output_20261012_receipt.json). Für diese Prüfung wurde kein Kalenderfeld geschrieben.

Nächster Schritt: {next_action} SPACE, vollständige wirtschaftliche Simulation, stabile Objekt-Lifecycles, Entfernen/Verschieben, übrige Aktionsmatrix und Steam-Abnahme bleiben offen. NET- und MixedBuilding-Adapter sind im Staging; Fixturechecks beweisen keine nativen Zustandsänderungen. NativeOpen setzt isolierte PhysFS-Mounts voraus; ein adversarialer Mountwechsel ist nicht geprüft.

Root besitzt Produktion, Build und Spiel-MCP; Agenten bearbeiten Staging. Kein Computer Use, keine Commits oder Pushes. Private Spielstände bleiben unverändert. Zuerst werden alle Funktionen mit genau zwei Instanzen in der kleinen Welt fertiggestellt. Vier Instanzen und die große Welt folgen erst zur finalen Abnahme. Das Ziel vollständiger Koop bleibt beauftragt.
'''
    (ROOT / '_drafts/native_open.md').write_text(text, encoding='utf-8')
    state_path = ROOT / '_drafts/implementation_state.md'
    state = state_path.read_text(encoding='utf-8')
    start = state.index('## Aktueller MCP-Eingabekandidat\n')
    end = state.index('## Vorherige native Zeit-Runtime-Abnahme\n', start)
    state = state[:start] + f'''## Aktueller MCP-Eingabekandidat

[Aktueller nativer Arbeitsstand](native_open.md): zwei kleine eigene Welten, Lauf `{run}`, DLL `{args.dll_sha256}`. NativeLoad/Roundtrip `{native_id}` besteht mit Datum, Geld und Buchungsgruppen gleich in Pause. HUD-Abnahme `{hud_id}` bestätigt Client-F2/F3/F4 und Host-F1, tatsächliche Items, Buttoncache und Pausenrahmen auf beiden Peers. Client-Clock bleibt 0. {calendar_note} Gemessener Writer-Aufwand etwa {average * 4 / 1000:.1f} ms je Frame; Witness-Optimierung integriert. Vorheriger HUD-Fehler und Tag-0-Joinfehler bleiben erhalten. Root nächster Schritt: {next_action} SPACE, laufende Wirtschaft/Events, NETSTATE, Objekt-Lifecycles, Entfernen/Verschieben, weitere Aktionen und Steam bleiben offen. Owner Root: Produktion, Build, Spiele; Agenten: Staging. Kleine Welt für Funktionen, große Welt finale Abnahme. Keine Computer Use/Commits/Pushes.

''' + state[end:]
    state_path.write_text(state, encoding='utf-8')
    print('info: current small-world checkpoint replaced; historical failures retained; game effects=0')


if __name__ == '__main__':
    main()
