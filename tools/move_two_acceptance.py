"""Exercise one Host-ordered native Move from each peer without replay."""
import argparse
import math
import time
import uuid
from pathlib import Path
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, utc, log, compare_ledgers

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup', type=Path, required=True)
    parser.add_argument('--freeze', type=Path, required=True)
    parser.add_argument('--actor', choices=('both','host','client-1'), default='both')
    args = parser.parse_args()
    args.test_root = ROOT / '_MPTest'; args.players = 2; args.cleanup = False; args.step_timeout = 45
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'original DoMove through common Controller, actor=' + args.actor + ', stable native UID, full pose, unchanged ledger and actual inverse Undo on both peers'
    try:
        setup = read_json(args.setup); frozen = read_json(args.freeze)
        if setup.get('passed') is not True: raise AcceptanceError('fixture setup failed')
        runner.load_manifests()
        if runner.summary['runtime_run_id'] != setup['new_run_id']: raise AcceptanceError('fixture run differs')
        for peer in ('host','client-1'):
            if read_json(runner.manifests[peer])['installed_binary_sha256']['MT2_Multiplayer.dll'] != frozen['dll_sha256']: raise AcceptanceError('installed DLL differs: ' + peer)
            runner.single(peer + '-start-controller', peer, 'game_start_typed_actions')
            runner.single(peer + '-enable-build', peer, 'game_enable_ui_build')
            runner.single(peer + '-enable-remove', peer, 'game_enable_ui_remove')
            runner.single(peer + '-enable-move', peer, 'game_enable_ui_move')
        bindings = {peer:runner.single('initial-bindings-' + peer, peer, 'game_read_remove_bindings') for peer in ('host','client-1')}
        rows = {}
        for peer, value in bindings.items():
            matches = [row for row in value['bindings'] if row.get('type') == 'SmallInn' and row.get('variant') == 'base']
            if len(matches) != 1: raise AcceptanceError('expected one baseline inn: ' + peer)
            rows[peer] = matches[0]
        if rows['host']['record_key'] != rows['client-1']['record_key']: raise AcceptanceError('baseline shared key differs')
        key = rows['host']['record_key']
        actors = ('client-1','host') if args.actor == 'both' else (args.actor,)
        for index, actor in enumerate(actors):
            before = {peer:runner.inspect(actor + '-before-' + peer, peer) for peer in ('host','client-1')}
            ledger = compare_ledgers(before['host'],before['client-1'])
            current = next(prop for prop in before['host']['props'] if prop.get('uid') == rows['host']['local_uid'])
            x = current['x'] + 40; z = current['z']
            ground = runner.single(actor + '-ground', 'host', 'game_probe_building_placement', {'type':'SmallInn','variant':'base','x':x,'z':z})
            action = str(uuid.uuid4())
            building = {'type':'SmallInn','variant':'base','x':x,'y':ground['y'],'z':z,'rotation':[0,0,0,1],'scale':[1,1,1],'alive':True}
            runner.checked(actor + '-immutable-request', {'action_uuid':action,'record_key':key,'building':building})
            runner.single(actor + '-move-once', actor, 'game_request_building_move', {'action_uuid':action,'record_key':key,'building':building})
            deadline = time.monotonic() + 45
            while time.monotonic() < deadline:
                states = {peer:runner.inspect(actor + '-observe-' + peer,peer) for peer in ('host','client-1')}
                for peer,state in states.items():
                    status = state['typed_actions']
                    if status.get('locked') or status.get('outcome') in (2,3): raise AcceptanceError(peer + ' Move failed: ' + str(status.get('cause')))
                if all(state['typed_actions'].get('action_uuid') == action and state['typed_actions'].get('confirmed') == index + 1 and state['typed_actions'].get('pending') is False for state in states.values()): break
            else: raise AcceptanceError('Move observation timeout; no replay')
            if compare_ledgers(states['host'],states['client-1']) != ledger: raise AcceptanceError('Move changed ledger')
            if states['host']['typed_actions'].get('ack_mask') != 3: raise AcceptanceError('Host lacks both ACKs')
            receipts = {}; objects = {}
            for peer,state in states.items():
                receipt = state['ui_move']['last_native']; receipts[peer] = receipt
                if receipt.get('token') != action or receipt.get('native_uid') != rows[peer]['local_uid']: raise AcceptanceError('Move token or native UID differs: ' + peer)
                for field in ('confirmed','undo_confirmed','selection_confirmed','cleanup_confirmed','move_attempted','move_returned','ctor_returned','dtor_returned','loc_returned','begin_returned','open_returned','close_returned','end_returned','vector_returned','string_returned'):
                    if receipt.get(field) is not True: raise AcceptanceError('native Move receipt missing: ' + peer + ':' + field)
                if receipt.get('exception') != 0 or receipt.get('cleanup_exception') != 0: raise AcceptanceError('native Move exception: ' + peer)
                objects[peer] = next(prop for prop in state['props'] if prop.get('uid') == rows[peer]['local_uid'])
                for field in ('x','z'):
                    if objects[peer][field] != building[field]: raise AcceptanceError('Move pose differs: ' + peer + ':' + field)
                if not math.isfinite(objects[peer]['y']): raise AcceptanceError('native settled height is not finite: ' + peer)
            if objects['host']['y'] != objects['client-1']['y']: raise AcceptanceError('native settled Move height differs between peers')
            runner.checked(actor + '-native-move-confirmed', {'action_uuid':action,'record_key':key,'ledger':ledger,'receipts':receipts,'objects':objects})
        for peer in ('host','client-1'):
            row = objects[peer]
            runner.single('camera-' + peer, peer, 'game_focus_test_camera', {'x':row['x'],'y':row['y'],'z':row['z'],'distance':100})
            runner.capture(peer)
        runner.summary.update(passed=True,state='confirmed-native-move-two')
    except Exception as error:
        runner.summary.update(passed=False,state='failed-native-move-two',error={'step':runner.summary['step'],'cause':str(error),'behavior':'remaining mutations skipped; no Move replay'})
        log('error','native Move: ' + str(error))
    runner.summary['finished_utc'] = utc(); runner.persist(); print(str(runner.summary_path))
    return 0 if runner.summary['passed'] else 1

if __name__ == '__main__': raise SystemExit(main())
