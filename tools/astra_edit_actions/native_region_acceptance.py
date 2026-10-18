"""Prove one native Client Region edit through the two-peer controller."""
import argparse
import sys
import time
import uuid
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, compare_ledgers, utc, log
sys.path.insert(0, str(ROOT))
from tools.e2e.run import preflight

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup', type=Path, required=True)
    parser.add_argument('--freeze', type=Path, required=True)
    parser.add_argument('--test-root', type=Path, default=ROOT / '_MPTest')
    parser.add_argument('--candidate-binary', type=Path, help='Exact selected mod artifact; required for Framework plugin mode')
    parser.add_argument('--region-id', type=int, required=True)
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument('--preset')
    target.add_argument('--styles-json', type=Path)
    args = parser.parse_args(argv)
    args.players, args.cleanup, args.step_timeout = 2, False, 45
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'one original Client RegionInfoWindow edit, Host ordered original execution once per peer, exact actual region content and tombstone receipts'
    try:
        styles = read_json(args.styles_json) if args.styles_json else None
        preflight(runner, args.freeze, args.setup, candidate_binary=args.candidate_binary)
        before, regions = {}, {}
        for peer in ('host', 'client-1'):
            before[peer] = runner.inspect('before-' + peer, peer)
            controller = before[peer]['typed_actions']
            if not controller.get('started') or controller.get('pending') or controller.get('locked'):
                raise AcceptanceError('Controller is not started and idle: ' + peer)
            regions[peer] = runner.single('region-before-' + peer, peer, 'game_read_region_preset', {'region_id': args.region_id})
            if styles is not None:
                if not isinstance(styles, list) or len(styles) != 4 or any(not isinstance(style, str) or style not in regions[peer]['available_styles'] for style in styles):
                    raise AcceptanceError('styles require four actual catalog keys: ' + peer)
                if styles == regions[peer]['styles']:
                    raise AcceptanceError('styles must change actual native region content: ' + peer)
            elif args.preset not in regions[peer]['available_presets'] or args.preset == regions[peer]['preset']:
                raise AcceptanceError('preset must be a different actual native catalog entry: ' + peer)
        ledger = compare_ledgers(before['host'], before['client-1'])
        for key in ('id', 'preset', 'styles', 'scenery_count', 'scenery_sha256', 'grid_sha256'):
            if regions['host'][key] != regions['client-1'][key]: raise AcceptanceError('native before content differs: ' + key)
        token = str(uuid.uuid4())
        runner.summary['diagnostic_uuid'] = token
        runner.persist()
        command = 'game_native_region_styles_ui' if styles is not None else 'game_native_region_preset_ui'
        arguments = {'diagnostic_uuid': token, 'region_id': args.region_id}
        arguments['styles' if styles is not None else 'preset'] = styles if styles is not None else args.preset
        admission = runner.single('client-region-apply-once', 'client-1', command, arguments)
        if not admission.get('captured_once') or not admission.get('local_state_unchanged'):
            raise AcceptanceError('genuine UI capture or pre-Host unchanged state missing')
        runner.checked('native-region-ui-admission', admission)
        deadline, action = time.monotonic() + 45, None
        after = {}
        while time.monotonic() < deadline:
            after = {peer: runner.inspect('observe-' + peer, peer) for peer in ('host', 'client-1')}
            action = after['client-1']['ui_region'].get('last_action_uuid')
            if any(value['typed_actions'].get('locked') for value in after.values()):
                raise AcceptanceError('Region action locked; inspect native receipts, no replay')
            for peer, value in after.items():
                if value['typed_actions']['failed'] > before[peer]['typed_actions']['failed']:
                    raise AcceptanceError('Region action failed on ' + peer + ': ' + value['typed_actions']['cause'] + '; no replay')
            if action and all(value['typed_actions'].get('action_uuid') == action and not value['typed_actions'].get('pending') and value['typed_actions'].get('outcome') == 1 for value in after.values()):
                break
        else: raise AcceptanceError('exact Region completion timed out; no replay')
        runner.summary['action_uuid'] = action
        if compare_ledgers(after['host'], after['client-1']) != ledger: raise AcceptanceError('Region action changed money')
        receipts = {}
        for peer, value in after.items():
            records = [row for row in value['ui_region']['operations'] if row['action_uuid'] == action]
            if len(records) != 1 or not all(records[0].get(key) for key in ('attempted', 'returned', 'confirmed', 'retired')):
                raise AcceptanceError('original Region execution or retirement missing: ' + peer)
            receipts[peer] = records[0]
            if value['typed_actions']['confirmed'] != before[peer]['typed_actions']['confirmed'] + 1:
                raise AcceptanceError('Region must confirm exactly once: ' + peer)
            if value['ui_region']['captured'] != before[peer]['ui_region']['captured'] + (peer == 'client-1'):
                raise AcceptanceError('Region capture origin or replay differs: ' + peer)
            if styles is not None:
                if records[0]['style_calls'] != 4 or records[0]['edit_depth_before'] != records[0]['edit_depth_after'] or records[0]['edit_opened'] != records[0]['edit_ended'] or records[0]['edit_cleanup_exception'] != 0:
                    raise AcceptanceError('Region original TerrainApply scope differs: ' + peer)
                if records[0]['after']['styles'] != styles:
                    raise AcceptanceError('Region result differs from requested styles: ' + peer)
        if after['host']['typed_actions'].get('ack_mask') != 3: raise AcceptanceError('Host lacks both acknowledgements')
        for key in ('before', 'after', 'retired_shared_keys', 'retired_keys_sha256'):
            if receipts['host'][key] != receipts['client-1'][key]: raise AcceptanceError('Region authority receipt differs: ' + key)
        runner.checked('native-region-confirmed-two', receipts)
        for peer in ('host', 'client-1'):
            runner.capture(peer)
        runner.summary.update(passed=True, state='confirmed-native-region-two')
    except Exception as error:
        runner.summary.update(passed=False, state='failed-native-region-two', error={'step':runner.summary['step'], 'cause':str(error), 'behavior':'remaining mutations skipped; no Region replay'})
        log('error', 'native Region: ' + str(error))
    runner.summary['finished_utc'] = utc()
    runner.persist()
    print(str(runner.summary_path))
    return 0 if runner.summary['passed'] else 1

if __name__ == '__main__': raise SystemExit(main())
