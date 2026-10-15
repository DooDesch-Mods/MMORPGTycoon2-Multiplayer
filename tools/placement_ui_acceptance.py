"""Prove a normal Client ItemButton/cursor placement on the owned two-peer fixture."""
import argparse
import time
import uuid
from pathlib import Path
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, compare_ledgers, placement_expense, utc, log

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup', type=Path, required=True)
    parser.add_argument('--test-root', type=Path, default=ROOT / '_MPTest')
    parser.add_argument('--freeze', type=Path, required=True)
    parser.add_argument('--x', type=float, default=8100)
    parser.add_argument('--z', type=float, default=8550)
    parser.add_argument('--reuse-installed', action='store_true')
    parser.add_argument('--type', default='SmallInn')
    parser.add_argument('--variant', default='base')
    parser.add_argument('--expected-cost', type=int)
    args = parser.parse_args()
    args.players = 2; args.cleanup = False; args.step_timeout = 45
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'one genuine Client ItemButton and cursor request, actual normal GameUpdate capture, original native commit per peer and exact Host completion'
    try:
        setup = read_json(args.setup); frozen = read_json(args.freeze)
        if setup.get('passed') is not True:raise AcceptanceError('fixture setup failed')
        runner.load_manifests()
        if runner.summary['runtime_run_id'] != setup.get('new_run_id', setup.get('runtime_run_id')):raise AcceptanceError('owned setup run differs')
        for peer in ('host', 'client-1'):
            if read_json(runner.manifests[peer])['installed_binary_sha256']['MT2_Multiplayer.dll'] != frozen['dll_sha256']:raise AcceptanceError('installed DLL differs: ' + peer)
            prior = runner.inspect(peer + '-read-current-installation', peer) if args.reuse_installed else {}
            if not prior.get('typed_actions', {}).get('started'):
                runner.single(peer + '-start-controller', peer, 'game_start_typed_actions')
            elif prior['typed_actions'].get('pending') or prior['typed_actions'].get('locked') or prior['typed_actions'].get('failed') != 0:
                raise AcceptanceError('reused Controller is not idle: ' + peer)
            if not prior.get('ui_build', {}).get('enabled'):
                runner.single(peer + '-enable-build', peer, 'game_enable_ui_build')
            if not prior.get('ui_remove', {}).get('enabled'):
                runner.single(peer + '-enable-remove', peer, 'game_enable_ui_remove')
        before = {peer:runner.inspect('before-' + peer, peer) for peer in ('host', 'client-1')}
        initial = compare_ledgers(before['host'], before['client-1'])
        base_confirmed = before['host']['typed_actions']['confirmed']
        if before['client-1']['typed_actions']['confirmed'] != base_confirmed: raise AcceptanceError('initial Controller confirmation counts differ')
        ground = runner.single('readonly-client-ground', 'client-1', 'game_probe_building_placement', {'type':args.type,'variant':args.variant,'x':args.x,'z':args.z})
        origin = before['client-1']['native_open_origin']
        token = str(uuid.uuid4())
        runner.summary['acceptance_token'] = token; runner.persist()
        request = {'acceptance_token':token,'snapshot_uuid':origin['uuid'],'expected_world_epoch':origin['world_epoch'],
                   'expected_generation':origin['generation'],'type':args.type,'variant':args.variant,'x':args.x,'y':ground['y'],'z':args.z}
        runner.checked('immutable-native-UI-request', request)
        admitted = runner.single('native-client-button-once', 'client-1', 'game_native_build_button', request)
        runner.checked('native-UI-admission', admitted)
        deadline = time.monotonic() + 45
        while time.monotonic() < deadline:
            receipt = runner.single('observe-native-client-UI', 'client-1', 'game_read_build_ui', {'acceptance_token':token})
            if receipt.get('confirmed') is True:break
            if receipt.get('phase') in (0,5):raise AcceptanceError('native UI job rejected or Unknown: ' + str(receipt))
        else:raise AcceptanceError('native UI completion observation timeout; no replay')
        action = str(uuid.UUID(receipt['action_uuid']))
        if receipt.get('capture_count') != 1 or receipt.get('host_confirmed_count') != 1 or receipt.get('exception') != 0:
            raise AcceptanceError('exact normal UI capture/confirmation differs')
        runner.summary['action_uuid'] = action; runner.persist()
        after = {peer:runner.inspect('after-' + peer, peer) for peer in ('host','client-1')}
        ledger = compare_ledgers(after['host'], after['client-1'])
        actual_cost = after['host']['ui_build']['receipts'].get(action,{}).get('cost')
        expected_cost = args.expected_cost if args.expected_cost is not None else 400 if args.type == 'SmallInn' and args.variant == 'base' else actual_cost
        if type(actual_cost) is not int or actual_cost <= 0 or actual_cost != expected_cost or ledger['cash'] != initial['cash'] - actual_cost:
            raise AcceptanceError('native building cost or cash differs')
        runner.checked('native-building-purchase-ledger', placement_expense(initial,ledger))
        objects = {}
        for peer, engine in after.items():
            current = engine['typed_actions']; native = engine['ui_build']['receipts'].get(action,{})
            if current.get('action_uuid') != action or current.get('pending') or current.get('outcome') != 1 or current.get('confirmed') != base_confirmed + 1:
                raise AcceptanceError('matching Controller completion missing: ' + peer)
            if any(native.get(name) != 1 for name in ('purchase','add','undo')) or native.get('private_stage') != 18 or not native.get('confirmed') or not native.get('undo_confirmed'):
                raise AcceptanceError('native original operation/cleanup missing: ' + peer)
            if args.type == 'StartingPoint':
                preview = native.get('preview_effect', {})
                if (preview.get('checked') is not True or preview.get('slot_cleared') is not True
                        or type(preview.get('present')) is not bool or preview.get('error') != ''):
                    raise AcceptanceError('StartingPoint preview cleanup readback missing: ' + peer)
                if preview.get('present') is True and (any(preview.get(key) is not True for key in ('destroy_attempted', 'destroy_returned', 'removed'))
                        or type(preview.get('manager_count_before')) is not int
                        or preview['manager_count_before'] <= 0
                        or type(preview.get('manager_count_after')) is not int
                        or preview['manager_count_after'] < 0
                        or preview.get('manager_count_after') != preview['manager_count_before'] - 1):
                    raise AcceptanceError('StartingPoint original preview instance retirement differs: ' + peer)
                if preview['present'] is False and (any(preview.get(key) is not False for key in ('destroy_attempted', 'destroy_returned', 'removed'))
                        or type(preview.get('manager_count_before')) is not int or preview['manager_count_before'] != -1
                        or type(preview.get('manager_count_after')) is not int or preview['manager_count_after'] != -1):
                    raise AcceptanceError('StartingPoint absent preview receipt differs: ' + peer)
                runner.checked('startingpoint-preview-retirement-' + peer, preview)
            binding = runner.single('fresh-binding-' + peer, peer, 'game_read_remove_bindings')
            matches = [row for row in binding['bindings'] if row['record_key'] == 'action:' + action]
            if len(matches) != 1 or matches[0]['local_uid'] != native['native_uid']:raise AcceptanceError('shared UI-created object mapping differs: ' + peer)
            objects[peer] = matches[0]
        if after['host']['typed_actions'].get('ack_mask') != 3:raise AcceptanceError('Host lacks both ACKs')
        if after['client-1']['ui_build'].get('captured') != before['client-1']['ui_build'].get('captured') + 1 or after['host']['ui_build'].get('captured') != before['host']['ui_build'].get('captured'):
            raise AcceptanceError('capture origin or replay loop differs')
        for name in ('type','variant','x','y','z'):
            if objects['host'][name] != objects['client-1'][name]:raise AcceptanceError('fresh native object differs: ' + name)
        runner.checked('normal-UI-building-confirmed', {'receipt':receipt,'objects':objects,'ledger':ledger})
        for peer in ('host','client-1'):
            row = objects[peer]
            runner.single('camera-' + peer,peer,'game_focus_test_camera',{'x':row['x'],'y':row['y'],'z':row['z'],'distance':100})
            runner.capture(peer)
        runner.summary.update(passed=True,state='confirmed-normal-building-UI-two')
    except Exception as error:
        runner.summary.update(passed=False,state='failed-normal-building-UI-two',error={'step':runner.summary['step'],'cause':str(error),'behavior':'remaining mutations skipped; no native callback or action replay'})
        log('error','normal building UI: ' + str(error))
    runner.summary['finished_utc'] = utc(); runner.persist(); print(str(runner.summary_path))
    return 0 if runner.summary['passed'] else 1

if __name__ == '__main__':raise SystemExit(main())
