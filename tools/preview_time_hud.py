"""Observe native client speed keys and restore Host pause without replay."""
import argparse
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/engine_analysis/calendar_authority/time_acceptance'))
from run_acceptance import Harness, Sequence, utc, need

def check_shared_hud(presentation, peer):
    hud = presentation.get('native_hud', {})
    need(hud.get('complete') is True, 'native HUD census incomplete: ' + str(hud))
    effective = presentation['effective_scale']
    items = hud['items']
    need(len(items) == 4 and hud['time_items'] == 4, 'four native HUD items required')
    for item in items:
        active = item['scale'] == effective
        need(item['native_state'] == (0 if active else 1) and item['active'] == active and
             item['button_cached_active'] == active and item['button_enabled'] is True,
             f'native HUD item/cache differs peer={peer} effective={effective} item={item}')
    visible = hud['native_pause_indicator'] and hud['pause_option_enabled'] and effective == 0
    need(hud['pause_window_visible'] == visible, f'native pause frame differs peer={peer}')
    if peer != 1:
        writer = presentation.get('hud_writer', {})
        need(hud['native_selected_scale'] == 0, 'client native simulation must remain paused')
        need(writer.get('initialized') is True and writer.get('bound') is True and writer.get('locked') is False and
             writer.get('hook_mask') == 7 and writer.get('revision') == presentation['revision'],
             'native HUD writer binding/revision differs: ' + str(writer))


def check_calendar(presentation, peer):
    value = presentation.get('calendar', {})
    need(value.get('initialized') is True and value.get('bound') is True and value.get('locked') is False and
         value.get('hook_mask') == 3 and value.get('epoch') == presentation['epoch'] and value.get('sample_available') is True,
         f'native Calendar binding/sample differs peer={peer}: {value}')
    wire = value['transport']
    need(wire['bound'] is True and wire['locked'] is False and wire['failures'] == 0,
         f'Calendar transport failed peer={peer}: {wire}')
    if peer != 1:
        need(wire['apply_confirmed'] > 0 and value['formatter_returned'] > 0 and
             value['formatted_epoch'] == presentation['epoch'] and
             0 < value['formatted_revision'] <= value['sample']['revision'] and
             0 < value['formatted_time_revision'] <= presentation['revision'],
             'native formatter has no confirmed Host sample: ' + str(value))
        need(presentation['native_hud']['native_selected_scale'] == 0, 'Calendar must leave client simulation paused')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test-root', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, action='append', required=True)
    parser.add_argument('--expected-dll-sha256', required=True)
    parser.add_argument('--step-timeout', type=float, default=30)
    parser.add_argument('--read-native-hud', action='store_true')
    parser.add_argument('--expect-shared-hud', action='store_true')
    parser.add_argument('--expect-calendar', action='store_true')
    parser.add_argument('--preview-keys', nargs='+', choices=['F2', 'F3', 'F4'], default=['F2'])
    args = parser.parse_args()
    args.keys = ['F1', 'F2', 'F3', 'F4']
    harness = Harness(args)
    harness.summary['scope'] = 'native_client_speed_keys_and_host_pause_hud_preview'
    harness.persist()
    try:
        peers, run = harness.load()
        sequence = Sequence(harness.call, harness.record, peers, run, args.expected_dll_sha256,
                            args.step_timeout, keys=args.keys)
        initial = {peer: sequence.read(peer) for peer in peers}
        sequence.epoch = initial[1]['time']['epoch']
        need(all(state['time']['selected_scale'] == 0 for state in initial.values()), 'initial paused state required')
        harness.record('initial', initial)
        if args.read_native_hud:
            for peer in peers:
                presentation = harness.call(peer, 'game_read_time_ui', {})
                harness.record('native_hud_initial', {'peer': peer, 'result': presentation})
                need(presentation.get('native_hud', {}).get('complete') is True,
                     'native HUD census incomplete before input: ' + str(presentation.get('native_hud')))
        for key in args.preview_keys:
            sequence.input(2, key)
            for peer in peers:
                if args.read_native_hud or args.expect_shared_hud:
                    presentation = harness.call(peer, 'game_read_time_ui', {})
                    harness.record('native_hud_running', {'peer': peer, 'key': key, 'result': presentation})
                    if args.expect_shared_hud:
                        check_shared_hud(presentation, peer)
                    if args.expect_calendar:
                        check_calendar(presentation, peer)
                if key == args.preview_keys[-1]:
                    harness.record('frame', {'peer': peer, 'result': harness.call(peer, 'game_capture_frame', {})})
        sequence.input(1, 'F1', cleanup=True)
        if args.read_native_hud:
            for peer in peers:
                harness.record('native_hud_paused', {'peer': peer, 'result': harness.call(peer, 'game_read_time_ui', {})})
        if args.expect_shared_hud:
            paused = {}
            for peer in peers:
                presentation = harness.call(peer, 'game_read_time_ui', {})
                harness.record('native_hud_paused_confirmed', {'peer': peer, 'result': presentation})
                check_shared_hud(presentation, peer)
                if args.expect_calendar:
                    check_calendar(presentation, peer)
                paused[peer] = presentation
            harness.summary['shared_hud_readback_proven'] = True
            if args.expect_calendar:
                host_fields = paused[1]['calendar']['sample']['fields']
                for peer in peers:
                    if peer == 1:
                        continue
                    client = paused[peer]
                    need(client['calendar']['sample']['fields'] == host_fields and
                         client['calendar']['formatted_fields'] == host_fields and
                         client['calendar']['formatted_time_revision'] == paused[1]['revision'],
                         f'paused Calendar sample/formatter differs peer={peer}')
                    need(client['native_hud']['seconds'] != host_fields['seconds'],
                         'test must distinguish shared display from the local client clock')
                harness.summary['calendar_formatter_readback_proven'] = True
                for peer in peers:
                    harness.record('paused_frame', {'peer': peer, 'result': harness.call(peer, 'game_capture_frame', {})})
        harness.summary.update(state='confirmed', passed=True, native_runtime_proof=True,
                               cleanup={'state': 'confirmed', 'operation': 'host native F1'},
                               visual_review='pending')
    except Exception as error:
        harness.summary.update(state='failed', error={'cause': f'{type(error).__name__}: {error}',
                               'behavior': 'remaining mutations skipped; no replay or automatic repair'})
        print('error: ' + harness.summary['error']['cause'], file=sys.stderr)
    finally:
        harness.summary['finished_utc'] = utc()
        harness.persist()
    print(harness.summary_path)
    return 0 if harness.summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
