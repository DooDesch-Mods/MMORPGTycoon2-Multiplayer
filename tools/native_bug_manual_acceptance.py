"""Exercise one original Client BugButton and retain actual native state on both peers."""
import argparse
import copy
import json
from pathlib import Path
import sys
import time
import uuid

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tools'))
from local_acceptance import AcceptanceError, AcceptanceRunner, hash_file, read_json, utc

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--expected-run-id', required=True)
parser.add_argument('--test-root', type=Path, default=root / '_MPTest/tutorial-two/_MPTest')
parser.add_argument('--phase', choices=('observe', 'cancel', 'schedule'), default='observe')
args = parser.parse_args()
args.players, args.cleanup, args.step_timeout, args.ready_timeout = 2, False, 45, 60
runner = AcceptanceRunner(args)
runner.summary['scope'] = 'Original Client BugButton; one ordered action; fresh native Developer/Job/ledger readback'

def require(condition, cause):
    if not condition:
        raise AcceptanceError(cause)

def controller(peer, label):
    engine = runner.inspect(label + '-' + peer, peer)
    state = engine['typed_actions']
    require(state['started'] and not state['locked'] and not state.get('live_error'), peer + ' Controller unavailable: ' + str(state.get('cause')))
    live = state.get('live_state', {})
    require(not live.get('locked') and not live.get('cause'), peer + ' live state failed: ' + str(live.get('cause')))
    require(engine['clock']['paused'] and engine['clock']['scale'] == 0, peer + ' actual Clock must remain paused')
    return state

def read_ui(peer, label):
    value = runner.single(label + '-' + peer, peer, 'game_read_native_bug_fix_ui')
    require(value['observed'] and not value['locked'], peer + ' original UI observation locked or absent')
    return value

def native(peer, label):
    value = runner.single(label + '-' + peer, peer, 'game_read_native_developers')
    runner.checked(label + '-' + peer, value)
    return value

def selected(ui, identity=None):
    rows = [row for row in ui['known_bugs'] if identity is None or row['bug'] == identity]
    require(len(rows) == 1, 'Expected one exact known fixture Bug; actual=' + str(len(rows)))
    return rows[0]

def ui_input(command, ui, bug):
    request = {key: ui[key] for key in ('native_origin_uuid', 'epoch', 'generation')}
    request.update(command=command, token=str(uuid.uuid4()), bug=bug['bug'], schedule=bug['schedule'])
    result = runner.single(command.lower() + '-client-once', 'client-1', 'game_native_bug_fix_ui', request)
    runner.checked(command.lower() + '-original-ui', result)
    require(result.get('confirmed') is True and result.get('unknown') is False, command + ' original UI unconfirmed')
    require(result['attempted_mask'] == result['returned_mask'] and result['attempted_mask'] > 0, command + ' original UI did not return all attempted inputs')
    return result

def wait_ui(identity, field):
    deadline = time.monotonic() + 45
    previous = None
    count = 0
    while True:
        count += 1
        value = read_ui('client-1', 'settle-' + field + '-' + str(count))
        row = selected(value, identity)
        if (value['bugs_page_ready'] if field == 'page' else row['button_ready']):
            return value, row
        cause = value.get('page_cause') if field == 'page' else row.get('button_cause')
        if cause != previous:
            print('info: original UI pending ' + field + ': ' + str(cause), flush=True)
            previous = cause
        require(time.monotonic() < deadline, 'Original UI readiness timed out field=' + field + ' cause=' + str(cause))

def job_semantics(value):
    result = copy.deepcopy(value)
    result.pop('raw_id_untrusted', None)
    result.get('bug', {}).pop('runtime_id_local', None)
    return result

def native_effect(peer, before, after, schedule):
    old, new = before['jobs'], after['jobs']
    require(after['observed'] and before['observed'], peer + ' fresh native observation absent')
    require(new['developers'] == old['developers'], peer + ' actual Developer-to-Job graph changed')
    require(new['queue_count'] == old['queue_count'] + int(schedule), peer + ' actual queue size differs from one selected effect')
    require(len(old['jobs']) == 1, peer + ' retained assigned-job fixture differs')
    expected = copy.deepcopy(old['jobs'][0])
    if not schedule:
        expected['type'], expected['valid'] = 0, False
        detached = new['jobs'][0]['bug']
        require(not detached['present'] and not detached['anchor_present'] and not detached['job_reciprocal'],
                peer + ' aborted physical Job retained a Bug edge')
        expected['bug'] = detached
    require(new['jobs'][0] == expected, peer + ' retained assigned Job changed outside Cancel/manual/backlink effect')
    if schedule:
        added = new['jobs'][1]
        require(added['type'] == 7 and added['valid'] and not added['developer_present'] and added['developer_id'] == -1,
                peer + ' new physical repair Job must be valid and unassigned')
        require(added['bug']['present'] and added['bug']['known'] == 1 and added['bug']['manual'] == 1 and added['bug']['debugged'] == 0 and added['bug']['job_reciprocal'],
                peer + ' new physical Job has wrong Bug values/backlink')

try:
    runner.load_manifests()
    require(runner.summary['runtime_run_id'] == args.expected_run_id, 'Owned run differs')
    digest = hash_file(root / 'build/bin/Release/MT2_Multiplayer.dll')
    require(all(read_json(path)['installed_binary_sha256']['MT2_Multiplayer.dll'] == digest for path in runner.manifests.values()), 'Installed candidate differs')
    runner.summary['dll_sha256'] = digest
    baseline = {peer: controller(peer, 'before-controller') for peer in ('host', 'client-1')}
    require(all(not state['pending'] for state in baseline.values()), 'Controller has outstanding action before test')
    uis = {peer: read_ui(peer, 'before-ui') for peer in ('host', 'client-1')}
    before = {peer: native(peer, 'before-native') for peer in ('host', 'client-1')}
    runner.summary['before'] = before
    bug = selected(uis['client-1'])
    require(selected(uis['host'])['bug'] == bug['bug'], 'Known stable Bug differs across peers')
    runner.summary['bug'] = bug
    runner.persist()
    if args.phase != 'observe':
        schedule = args.phase == 'schedule'
        require(bug['schedule'] == schedule, 'Current original Fix/Cancel eligibility differs; action not attempted')
        ui = uis['client-1']
        if not ui['bugs_page_ready']:
            ui_input('Show', ui, bug)
            ui, bug = wait_ui(bug['bug'], 'page')
        if not bug['button_ready']:
            ui_input('Select', ui, bug)
            ui, bug = wait_ui(bug['bug'], 'button')
        clicked = ui_input('Click', ui, bug)
        action_id = clicked['capture_action_uuid']
        require(str(uuid.UUID(action_id)) == action_id, 'Original Release capture UUID absent')
        runner.summary['action_uuid'] = action_id
        runner.persist()
        deadline = time.monotonic() + 60
        confirmed = {}
        count = 0
        while len(confirmed) != 2:
            count += 1
            for peer in ('host', 'client-1'):
                if peer in confirmed:
                    continue
                state = controller(peer, 'ordered-' + str(count))
                require(state['failed'] == baseline[peer]['failed'], peer + ' Controller failed ordered Bug action: ' + str(state.get('cause')))
                require(state['confirmed'] <= baseline[peer]['confirmed'] + 1, peer + ' action executed more than once')
                operations = [row for row in state['bug_manual']['operations'] if row['action_uuid'] == action_id]
                if not state['pending'] and state['action_uuid'] == action_id and state['confirmed'] == baseline[peer]['confirmed'] + 1:
                    require(len(operations) == 1 and operations[0]['attempted'] and operations[0]['confirmed'] and operations[0]['retired'], peer + ' exact actual Bug action receipt absent')
                    if peer == 'host':
                        require(state['required_mask'] == 3 and state['ack_mask'] == 3, 'Host did not receive both peer acknowledgements')
                        require(operations[0]['job_returned'] and operations[0]['manual_returned'], 'Host original Job/manual property calls did not return')
                    confirmed[peer] = state
                    runner.checked('ordered-confirmed-' + peer, state)
            require(len(confirmed) == 2 or time.monotonic() < deadline, 'Ordered action acknowledgement timed out; retained UUID=' + action_id)
        after = {peer: native(peer, 'after-native') for peer in ('host', 'client-1')}
        runner.summary['after'] = after
        runner.summary['after_ui'] = {peer: read_ui(peer, 'after-ui') for peer in ('host', 'client-1')}
        # Physical graph comparisons are recorded beside the receipt, rather than comparing process pointers.
        for peer in ('host', 'client-1'):
            require(after[peer]['ledger'] == before[peer]['ledger'], peer + ' Bug action changed actual ledger')
            require(after[peer]['developers'] == before[peer]['developers'], peer + ' Bug action changed actual staff state/pose')
            require(after[peer]['developer_movement'] == before[peer]['developer_movement'], peer + ' Bug action changed actual staff movement')
            require(selected(runner.summary['after_ui'][peer], bug['bug'])['schedule'] != schedule, peer + ' actual Bug Job backlink did not change')
            native_effect(peer, before[peer], after[peer], schedule)
        require([job_semantics(row) for row in after['host']['jobs']['jobs']] ==
                [job_semantics(row) for row in after['client-1']['jobs']['jobs']],
                'Fresh physical Host/Client Job semantics differ')
        for peer in ('host', 'client-1'):
            runner.capture(peer)
        runner.summary.update(passed=True, state='ordered-original-bug-' + args.phase + '-confirmed')
    else:
        runner.summary.update(passed=True, state='native-known-bug-baseline-observed')
except Exception as error:
    runner.summary.update(passed=False, state='failed-no-replay', error={'step': runner.summary['step'], 'cause': str(error)})
    print('error: ' + str(error), flush=True)
runner.summary['finished_utc'] = utc()
runner.persist()
print(runner.summary_path)
raise SystemExit(0 if runner.summary['passed'] else 1)
