"""Prove original native building commits over the current two-peer controller."""
import argparse
from pathlib import Path
import time
import uuid
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, utc, log, compare_ledgers


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup', type=Path, required=True)
    parser.add_argument('--expected-dll-sha256', required=True)
    parser.add_argument('--resume-from', type=Path)
    parser.add_argument('--x', type=float, default=7990)
    parser.add_argument('--z', type=float, default=8550)
    args = parser.parse_args()
    args.test_root = ROOT / '_MPTest'; args.players = 2; args.cleanup = False; args.step_timeout = 45
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'original native SmallInn commit on each peer through one Host-ordered controller; live mouse capture remains separate'
    try:
        setup = read_json(args.setup)
        if setup.get('passed') is not True:
            raise AcceptanceError('setup failed; no placement attempted')
        runner.load_manifests()
        if runner.summary['runtime_run_id'] != setup['new_run_id']:
            raise AcceptanceError('owned run differs from exact setup')
        previous = read_json(args.resume_from) if args.resume_from else None
        if previous and (previous['runtime_run_id'] != runner.summary['runtime_run_id'] or
                         previous['state'] != 'failed-original-building-two' or
                         any('submit-once' in job['step'] for job in previous['jobs'])):
            raise AcceptanceError('resume requires same owned run and no attempted placement request')
        for peer in ('host', 'client-1'):
            manifest = read_json(runner.manifests[peer])
            if manifest['installed_binary_sha256']['MT2_Multiplayer.dll'] != args.expected_dll_sha256:
                raise AcceptanceError('active DLL differs: ' + peer)
            if previous:
                state = runner.inspect(peer + '-read-installed', peer)
                if not state['typed_actions'].get('started') or state['typed_actions'].get('attempted') != 0 or \
                   state['typed_actions'].get('locked') or not state['ui_build'].get('enabled') or not state['ui_remove'].get('enabled'):
                    raise AcceptanceError('previous installation or untouched native action state differs: ' + peer)
                runner.checked(peer + '-installed-evidence-reused', {'previous': str(args.resume_from), 'run_id': runner.summary['runtime_run_id']})
            else:
                runner.single(peer + '-start-controller', peer, 'game_start_typed_actions')
                runner.single(peer + '-enable-build', peer, 'game_enable_ui_build')
                runner.single(peer + '-enable-remove', peer, 'game_enable_ui_remove')
        for index, actor in enumerate(('client-1', 'host')):
            before = {peer: runner.inspect(actor + '-before-' + peer, peer) for peer in ('host', 'client-1')}
            compare_ledgers(before['host'], before['client-1'])
            parameters = {'type': 'SmallInn', 'variant': 'base', 'x': args.x + index * 50, 'z': args.z}
            grounded = runner.single(actor + '-readonly-ground', 'host', 'game_probe_building_placement', parameters)
            action = str(uuid.uuid4())
            building = {**parameters, 'y': grounded['y'], 'rotation': [0, 0, 0, 1], 'scale': [1, 1, 1], 'alive': True}
            runner.checked(actor + '-immutable-request', {'action_uuid': action, 'building': building})
            admitted = runner.single(actor + '-submit-once', actor, 'game_request_building_place', {'action_uuid': action, 'building': building})
            runner.checked(actor + '-admission', admitted)
            deadline = time.monotonic() + 45
            states = {}
            while time.monotonic() < deadline:
                states = {peer: runner.inspect(actor + '-observe-' + peer, peer) for peer in ('host', 'client-1')}
                for peer, state in states.items():
                    current = state['typed_actions']
                    if current.get('locked') or current.get('outcome') in (2, 3):
                        raise AcceptanceError(peer + ' original placement failed: ' + str(current.get('cause')))
                if all(state['typed_actions'].get('action_uuid') == action and
                       state['typed_actions'].get('confirmed') == index + 1 and
                       state['typed_actions'].get('pending') is False for state in states.values()):
                    break
            else:
                raise AcceptanceError('placement observation timeout; no replay')
            ledger = compare_ledgers(states['host'], states['client-1'])
            receipts = {}
            for peer, state in states.items():
                receipt = state['ui_build']['receipts'].get(action)
                if not receipt or receipt.get('confirmed') is not True or any(receipt.get(key) != 1 for key in ('purchase', 'add', 'undo')):
                    raise AcceptanceError(peer + ' exact original operation receipt missing')
                if receipt.get('private_stage') != 18 or receipt.get('undo_confirmed') is not True:
                    raise AcceptanceError(peer + ' original cleanup or Undo not confirmed')
                receipts[peer] = receipt
            if receipts['host']['native_uid'] == receipts['client-1']['native_uid']:
                log('info', 'native UID equality is incidental; shared action ID is the identity')
            bindings = {peer: runner.single(actor + '-bindings-' + peer, peer, 'game_read_remove_bindings') for peer in ('host', 'client-1')}
            rows = {}
            for peer, value in bindings.items():
                selected = [row for row in value['bindings'] if row['record_key'] == 'action:' + action]
                if len(selected) != 1 or selected[0]['local_uid'] != receipts[peer]['native_uid']:
                    raise AcceptanceError(peer + ' new shared object mapping differs')
                rows[peer] = selected[0]
            for key in ('type', 'variant', 'x', 'y', 'z'):
                if rows['host'][key] != rows['client-1'][key]:
                    raise AcceptanceError('confirmed shared object differs: ' + key)
            runner.checked(actor + '-original-commit-confirmed', {'action_uuid': action, 'ledger': ledger, 'receipts': receipts, 'objects': rows})
            for peer in ('host', 'client-1'):
                row = rows[peer]
                runner.single(actor + '-camera-' + peer, peer, 'game_focus_test_camera',
                              {'x': row['x'], 'y': row['y'], 'z': row['z'], 'distance': 100})
                runner.single(actor + '-frame-' + peer, peer, 'game_capture_frame')
        runner.summary.update(passed=True, state='confirmed-original-building-two')
    except Exception as error:
        runner.summary.update(passed=False, state='failed-original-building-two',
                              error={'step': runner.summary['step'], 'cause': str(error), 'behavior': 'remaining mutations skipped; no action replay'})
        log('error', 'original placement acceptance: ' + str(error))
    runner.summary['finished_utc'] = utc(); runner.persist()
    print(str(runner.summary_path))
    return 0 if runner.summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
