"""Prove native Startingpoint actions in an owned Host and Client pair."""
import argparse
from pathlib import Path
import time
import uuid

from local_acceptance import AcceptanceError, AcceptanceRunner, ROOT, compare_ledgers, extract_result, hash_file, read_json, utc
from lobby_ui.native_local_acceptance import button, observe, left, ready

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--expected-run-id', required=True)
parser.add_argument('--test-root', type=Path, required=True)
parser.add_argument('--phase', choices=('prepare', 'begin', 'prove', 'client-prove', 'client-start', 'client-staff', 'retire'), required=True)
parser.add_argument('--host-running', action='store_true', help='Run the Host through the existing native clock MCP during Client Research')
parser.add_argument('--natural-work', action='store_true', help='Let the assigned Host developer complete Startingpoint before ordinary Client Commit')
parser.add_argument('--resume-start-from', type=Path, help='Reuse a confirmed open Research view after a recorded no-effect hidden-HUD time rejection')
parser.add_argument('--snapshot', default='71b654a4-e005-4ad6-af5c-9c2974f9d41f')
parser.add_argument('--require-client', action='store_true', help='Require the joined Client to adopt every Host transition')
args = parser.parse_args()
args.players, args.cleanup = 2, False
args.step_timeout, args.ready_timeout, args.wait_seconds = 45, 60, 60
runner = AcceptanceRunner(args)
runner.summary['scope'] = 'isolated Host original Starting Point completion and ordinary Commit; no peer replication claim'
if args.require_client:
    runner.summary['scope'] = 'joined Host Starting Point completion and ordinary Commit with actual Client project, inventory and Release adoption'
if args.phase == 'begin':
    runner.summary['scope'] = 'joined Host original Starting Point Pay and project creation with Client adoption and full native ledger equality'
if args.phase == 'client-prove':
    runner.summary['scope'] = 'ordinary Client Commit button submits one Host-ordered action and both peers confirm actual deployment and retirement'
if args.phase == 'client-start':
    runner.summary['scope'] = 'ordinary Client Research button submits one Host-ordered purchase and project creation with actual native ledger equality'
if args.phase == 'client-staff':
    runner.summary['scope'] = 'ordinary Client Staff pip orders one native worker allocation, followed by natural Host work and matching Client state'


def shared_state(name, expected):
    deadline = time.monotonic() + 60
    sample = 0
    while True:
        sample += 1
        contexts = {peer: runner.single(name + '-context-' + peer + '-' + str(sample), peer, 'game_read_native_lobby')
                    for peer in ('host', 'client-1')}
        if not all(ready(state) for state in contexts.values()):
            raise AcceptanceError('Both peers must remain Playing during ' + name)
        if contexts['host']['prepare']['snapshot_id'] != contexts['client-1']['prepare']['snapshot_id']:
            raise AcceptanceError('Shared session snapshot differs during ' + name)
        actual = runner.single(name + '-client-' + str(sample), 'client-1', 'game_read_tutorial_development')
        fields = ('project', 'startingpoint', 'startingpoint_release')
        if all(actual[field] == expected[field] for field in fields):
            engine = runner.inspect(name + '-client-clock', 'client-1')
            if engine.get('clock', {}).get('scale') != 0:
                raise AcceptanceError('Client actual simulation Clock must remain0')
            runner.checked(name + '-shared', actual)
            runner.checked(name + '-client-clock', engine['clock'])
            runner.checked(name + '-client-frame', runner.single(name + '-client-frame', 'client-1', 'game_capture_frame'))
            return
        if time.monotonic() >= deadline:
            differences = {field: {'host': expected[field], 'client': actual[field]}
                           for field in fields if actual[field] != expected[field]}
            raise AcceptanceError('Client adoption timeout required<60s during ' + name + ': ' + str(differences))


def fixed_operation(peer, operation, current):
    request = {'token': str(uuid.uuid4()), 'native_origin_uuid': current['snapshot_uuid'],
               'world_epoch': current['world_epoch'], 'world_generation': current['world_generation'],
               'operation': operation, 'expected_development': current}
    runner.checked(operation + '-request', request)
    result = runner.single(operation + '-once', peer, 'game_tutorial_startingpoint', request)
    if any(result.get(field) is not True for field in ('attempted', 'returned', 'confirmed')):
        raise AcceptanceError(operation + ' lacks native attempt/return/readback; no replay')
    if result.get('unknown') is not False or result.get('token') != request['token']:
        raise AcceptanceError(operation + ' outcome/token differs; no replay')
    runner.checked(operation, result)
    return result


def host_time(scale):
    if scale not in (0, 1, 3, 9):
        raise AcceptanceError('Clock setup requires the existing game UI scales0/1/3/9; no native attempt')
    engine = runner.inspect('start-time-' + str(scale) + '-before', 'host')
    runtime = engine['shared_time']['runtime']
    controller = runtime['controller']
    if runtime['phase'] != 3 or controller['write_locked']:
        raise AcceptanceError('Client Research requires active shared time authority')
    runner.checked('start-time-' + str(scale) + '-native-clock-return',
                   runner.single('start-time-' + str(scale) + '-once', 'host', 'game_set_time_scale', {'scale': scale}))
    deadline, count = time.monotonic() + 45, 0
    while True:
        count += 1
        actual = runner.inspect('start-time-' + str(scale) + '-host-' + str(count), 'host')
        client = runner.inspect('start-time-' + str(scale) + '-client-' + str(count), 'client-1')
        host_state = actual['shared_time']['runtime']['controller']
        client_state = client['shared_time']['runtime']['controller']
        if host_state['write_locked'] or client_state['write_locked']:
            raise AcceptanceError('Research clock setup lost shared time authority')
        if (actual['clock']['scale'] == scale and host_state['observed']['selected'] == scale
                and client['clock']['scale'] == 0 and client_state['applied']['selected'] == scale
                and client_state['applied']['revision'] == host_state['observed']['revision']):
            break
        if time.monotonic() >= deadline:
            raise AcceptanceError('Research native clock setup not observed on both peers required<45s; no replay')
    runner.checked('start-time-' + str(scale) + '-confirmed',
                   {'setup_route': 'existing native clock MCP; production Host polling and Client presentation',
                    'clock': actual['clock'], 'host_observed': host_state['observed'],
                    'client_applied': client_state['applied'], 'client_clock': client['clock']})


def prior_result(prior, step, tool):
    jobs = [job for job in prior['jobs'] if job['step'] == step]
    if len(jobs) != 1 or jobs[0]['state'] != 'confirmed':
        raise AcceptanceError('Resume lacks original confirmed observation: ' + step)
    evidence = Path(jobs[0]['evidence_path'])
    if hash_file(evidence) != jobs[0]['evidence_sha256']:
        raise AcceptanceError('Original resume evidence changed: ' + step)
    return extract_result(read_json(evidence), tool)


def resume_research_view(baseline, client, ledger, controllers, host_engine):
    path = args.resume_start_from.resolve()
    if not path.is_relative_to(ROOT / 'artifacts/acceptance') or not args.host_running:
        raise AcceptanceError('Research resume requires an owned running-Host acceptance record')
    prior = read_json(path)
    if (prior.get('passed') is not False or prior.get('runtime_run_id') != runner.summary['runtime_run_id']
            or prior.get('dll_sha256') != runner.summary['dll_sha256']
            or prior.get('error', {}).get('step') != 'start-time-1-once'
            or 'TIME_MOUSE_REJECTED: time_mouse_actual_hud_panel_not_visible' not in prior['error']['cause']):
        raise AcceptanceError('Research resume is restricted to the recorded no-native hidden-HUD rejection')
    checks = {item['check']: item['result'] for item in prior['checks']}
    if checks.get('client-start-baseline') != baseline:
        raise AcceptanceError('Research resume native Host baseline changed')
    shown = checks.get('client_research_show', {})
    if (shown.get('attempted') is not True or shown.get('returned') is not True
            or shown.get('confirmed') is not True or shown.get('unknown') is not False
            or shown.get('after') != client):
        raise AcceptanceError('Research resume lacks the same confirmed open Client view')
    old_ledger = compare_ledgers(prior_result(prior, 'start-ledger-before-host', 'game_inspect_engine'),
                                 prior_result(prior, 'start-ledger-before-client', 'game_inspect_engine'))
    if ledger != old_ledger:
        raise AcceptanceError('Research resume full native ledger changed')
    for peer, current in controllers.items():
        old = prior_result(prior, 'before-client-start-' + peer, 'game_read_native_lobby')['controller']
        if any(current[field] != old[field] for field in ('attempted', 'confirmed', 'failed', 'host_revision', 'action_uuid')):
            raise AcceptanceError('Research resume Controller changed on ' + peer)
    old_time = prior_result(prior, 'start-time-1-before', 'game_inspect_engine')['shared_time']['runtime']
    current_time = host_engine['shared_time']['runtime']
    old_observed = old_time['controller']['observed']
    current_observed = current_time['controller']['observed']
    if (any(current_time[field] != old_time[field] for field in ('epoch', 'native_set_confirmed'))
            or current_time['controller']['processed_sequences'] != old_time['controller']['processed_sequences']
            or any(current_observed[field] != old_observed[field] for field in ('selected', 'pause', 'present', 'uncertain'))
            or current_observed['revision'] < old_observed['revision']):
        raise AcceptanceError('Research resume current time authority/clock or processed commands changed')
    token = checks['start-time-1-request']['acceptance_token']
    log = Path(read_json(runner.manifests['host'])['runtime_root']) / 'game/mt2mp.log'
    matching = [line for line in log.read_text(encoding='utf-8').splitlines()
                if '[TimeMouse]' in line and 'token=' + token in line and 'native_attempted=false' in line
                and 'failed cause=time_mouse_actual_hud_panel_not_visible' in line]
    if len(matching) != 1:
        raise AcceptanceError('Research resume lacks the original native no-attempt acknowledgement')
    runner.checked('confirmed-open-view-resumed', {'original_acceptance': prior['acceptance_id'],
                   'original_summary_sha256': hash_file(path), 'native_time_attempted': False,
                   'ui_show_replayed': False, 'same_origin_controller_and_ledger': True,
                   'old_time_observation_revision': old_observed['revision'],
                   'current_time_observation_revision': current_observed['revision']})
    return shown


def client_start():
    baseline = runner.single('client-start-baseline', 'host', 'game_read_tutorial_development')
    runner.checked('client-start-baseline', baseline)
    if (baseline['project'].get('present') is not False or baseline['startingpoint'].get('record_present') is not True
            or any(baseline['startingpoint'].get(field) is not False for field in ('purchased', 'deployed', 'custom', 'owned'))
            or baseline['startingpoint'].get('inventory_count') != 0
            or baseline['startingpoint_release'].get('feature_count') != 0):
        raise AcceptanceError('Client Research requires unpurchased unowned Startingpoint and no current project')
    shared_state('client-start-baseline', baseline)
    host_engine = runner.inspect('start-ledger-before-host', 'host')
    client_engine = runner.inspect('start-ledger-before-client', 'client-1')
    ledger_before = compare_ledgers(host_engine, client_engine)
    before = {}
    for peer in ('host', 'client-1'):
        context = runner.single('before-client-start-' + peer, peer, 'game_read_native_lobby')
        if not ready(context) or context['controller'].get('pending') is not False:
            raise AcceptanceError('Client Research requires both peers Playing with an idle Controller')
        before[peer] = context['controller']
    client = runner.single('client-start-current', 'client-1', 'game_read_tutorial_development')
    shown = (resume_research_view(baseline, client, ledger_before, before, host_engine)
             if args.resume_start_from else fixed_operation('client-1', 'client_research_show', client))
    price = shown.get('price')
    if (type(price) is not int or price < 0 or shown.get('page_requested') is not True
            or shown.get('selection_confirmed') is not False):
        raise AcceptanceError('Research Show lacks actual price and confirmed page request')
    deadline, sample = time.monotonic() + 30, 0
    while True:
        sample += 1
        state_request = {'token': str(uuid.uuid4()), 'native_origin_uuid': client['snapshot_uuid'],
                         'world_epoch': client['world_epoch'], 'world_generation': client['world_generation'],
                         'operation': 'client_research_state', 'expected_development': client}
        state = runner.single('research-layout-state-' + str(sample), 'client-1',
                              'game_tutorial_startingpoint', state_request)
        if (state.get('read_only') is not True or state.get('attempted') is not False
                or state.get('confirmed') is not True or state.get('unknown') is not False):
            raise AcceptanceError('Research presentation observation lacks read-only acknowledgement')
        runner.checked('research-layout-state-' + str(sample), state)
        if state.get('ready') is True:
            break
        if state.get('ready') is not False or time.monotonic() >= deadline:
            raise AcceptanceError('Research native page/layout not ready required<30s; no UI replay: ' + str(state))
    runner.checked('client-research-button-before', runner.single('client-research-button-before', 'client-1', 'game_capture_frame'))
    if args.host_running:
        host_time(1)
    client = runner.single('client-research-current', 'client-1', 'game_read_tutorial_development')
    submitted = fixed_operation('client-1', 'client_research', client)
    if (submitted.get('price') != price or submitted.get('selection_confirmed') is not True
            or submitted.get('selection_attempted') is not True or submitted.get('activation_attempted') is not True):
        raise AcceptanceError('Research lacks original selection/activation or stable native price/row; no replay')
    for field in ('ledger_before', 'ledger_after'):
        receipt = submitted.get(field, {})
        if any(receipt.get(key) != value for key, value in ledger_before.items()):
            raise AcceptanceError('Client Research changed local ledger before applying the Host result: ' + field)
    action_id = submitted.get('action_uuid')
    try:
        if submitted.get('submitted') is not True or str(uuid.UUID(action_id)) != action_id:
            raise ValueError('actual action UUID missing')
    except (ValueError, TypeError, AttributeError) as error:
        raise AcceptanceError('Client Research has no confirmed submission identity; no replay: ' + str(error)) from error
    runner.checked('submitted-start-action', {'action_uuid': action_id, 'submitted': True, 'result_confirmed': False})
    deadline, sample = time.monotonic() + 60, 0
    while True:
        sample += 1
        final = {peer: runner.single('start-result-' + peer + '-' + str(sample), peer, 'game_read_native_lobby')
                 for peer in ('host', 'client-1')}
        for peer, context in final.items():
            state = context['controller']
            if state.get('locked') or state.get('failed') != before[peer]['failed']:
                raise AcceptanceError('Client Research failed peer=' + peer + ' cause=' + str(state.get('cause')))
            if state.get('pending') is True and state.get('outcome') == 0:
                if (context['prepare'].get('playing') is not True or context['prepare'].get('error')
                        or context['snapshot'].get('world_authority_valid') is not True):
                    raise AcceptanceError('Pending Client Research lost its actual world/session on ' + peer)
            elif not ready(context):
                raise AcceptanceError('Peer lost Playing during Client Research: ' + peer)
        if all(context['controller'].get('action_uuid') == action_id
               and context['controller'].get('pending') is False and context['controller'].get('outcome') == 1
               and context['controller'].get('confirmed') == before[peer]['confirmed'] + 1
               for peer, context in final.items()):
            break
        if time.monotonic() >= deadline:
            raise AcceptanceError('Client Research result timeout required<60s; no replay: ' +
                                  str({peer: context['controller'] for peer, context in final.items()}))
    for peer, context in final.items():
        state = context['controller']
        if (state.get('attempted') != before[peer]['attempted'] + 1
                or state.get('host_revision') != before[peer]['host_revision'] + 1):
            raise AcceptanceError('Client Research attempt/revision delta differs from1 on ' + peer)
        runner.checked('confirmed-start-controller-' + peer, state)
    if final['host']['controller']['ack_mask'] != final['host']['controller']['required_mask']:
        raise AcceptanceError('Host lacks the complete Research adoption ACK mask')
    if args.host_running:
        clock = runner.inspect('start-confirmed-running-host', 'host')['clock']
        if clock.get('scale') != 1 or clock.get('paused') is not False:
            raise AcceptanceError('Host was not running when Research confirmation was observed')
        runner.checked('start-confirmed-running-clock', clock)
        host_time(0)
    after = runner.single('after-client-start-host', 'host', 'game_read_tutorial_development')
    project, technology = after['project'], after['startingpoint']
    if (project.get('present') is not True or project.get('kind') != 0 or project.get('tech_key') != 'startingpoint'
            or technology.get('purchased') is not True
            or any(technology.get(field) is not False for field in ('deployed', 'custom', 'owned'))
            or technology.get('inventory_count') != 0 or after['startingpoint_release'] != baseline['startingpoint_release']):
        raise AcceptanceError('Research result lacks the actual purchased current Startingpoint project')
    shared_state('client-start', after)
    after_ledger = compare_ledgers(runner.inspect('start-ledger-after-host', 'host'),
                                   runner.inspect('start-ledger-after-client', 'client-1'))
    expected = {**ledger_before, 'line_counts': list(ledger_before['line_counts']), 'line_income': list(ledger_before['line_income'])}
    expected['cash'] -= price
    expected['record_cash'] -= price
    expected['record_expense'] += price
    expected['line_counts'][3] += 1
    expected['line_income'][3] -= price
    if after_ledger != expected:
        raise AcceptanceError('Research full ledger differs from one category3 purchase: ' +
                              str({'expected': expected, 'actual': after_ledger}))
    progress = runner.single('client-start-progress', 'client-1', 'game_read_tutorial_progress')
    if (progress.get('client_start_submitted') is not True or progress.get('client_start_suppressed') is not True
            or progress.get('client_start_action_uuid') != action_id or progress.get('client_start_cause')
            or progress.get('client_start_presentation_cause')):
        raise AcceptanceError('Ordinary Client Research capture/presentation receipt differs')
    runner.checked('client-start-hook-submission', progress)
    runner.checked('client-start-host-frame', runner.single('client-start-host-frame', 'host', 'game_capture_frame'))
    runner.checked('client-start-ledger-after', after_ledger)
    runner.summary.update(before=baseline, after=after, action_uuid=action_id, ordinary_client_start_proven=True,
                          client_adoption_proven=True, two_peer_action_confirmation_proven=True,
                          ledger_equality_proven=True, native_purchase_price=price, host_running_proven=args.host_running)


def client_staff():
    baseline = runner.single('staff-baseline-host', 'host', 'game_read_tutorial_development')
    project = baseline['project']
    if (project.get('present') is not True or project.get('tech_key') != 'startingpoint'
            or project.get('desired_workers') != 0 or baseline['startingpoint'].get('purchased') is not True
            or baseline['startingpoint'].get('deployed') is not False):
        raise AcceptanceError('Staff requires the current purchased Startingpoint project with desired workers0')
    runner.checked('staff-baseline', baseline)
    shared_state('staff-baseline', baseline)
    ledger_before = compare_ledgers(runner.inspect('staff-ledger-before-host', 'host'),
                                    runner.inspect('staff-ledger-before-client', 'client-1'))
    before = {}
    for peer in ('host', 'client-1'):
        context = runner.single('before-client-staff-' + peer, peer, 'game_read_native_lobby')
        if not ready(context) or context['controller'].get('pending') is not False:
            raise AcceptanceError('Staff requires both peers Playing with an idle Controller')
        before[peer] = context['controller']
    client = runner.single('staff-baseline-client', 'client-1', 'game_read_tutorial_development')
    shown = fixed_operation('client-1', 'client_staff_show', client)
    if shown.get('page_requested') is not True or shown.get('desired_workers') != 0:
        raise AcceptanceError('Staff Show lacks the confirmed page request and initial desired workers0')
    deadline, sample = time.monotonic() + 30, 0
    while True:
        sample += 1
        request = {'token': str(uuid.uuid4()), 'native_origin_uuid': client['snapshot_uuid'],
                   'world_epoch': client['world_epoch'], 'world_generation': client['world_generation'],
                   'operation': 'client_staff_state', 'expected_development': client}
        state = runner.single('staff-layout-state-' + str(sample), 'client-1', 'game_tutorial_startingpoint', request)
        if (state.get('read_only') is not True or state.get('attempted') is not False
                or state.get('confirmed') is not True or state.get('unknown') is not False):
            raise AcceptanceError('Staff state lacks effectless observation acknowledgement')
        runner.checked('staff-layout-state-' + str(sample), state)
        if state.get('ready') is True:
            break
        if state.get('ready') is not False or time.monotonic() >= deadline:
            raise AcceptanceError('Staff native page/layout not ready required<30s; no input replay: ' + str(state))
    runner.checked('client-staff-before-frame', runner.single('client-staff-before-frame', 'client-1', 'game_capture_frame'))
    submitted = fixed_operation('client-1', 'client_staff_one', client)
    if (submitted.get('activation_attempted') is not True or submitted.get('desired_workers') != 1
            or submitted.get('host_completion_confirmed') is not False
            or submitted.get('before') != client or submitted.get('after') != client):
        raise AcceptanceError('Staff lacks original pip activation or changed native project before Host result')
    for field in ('ledger_before', 'ledger_after'):
        if any(submitted.get(field, {}).get(key) != value for key, value in ledger_before.items()):
            raise AcceptanceError('Staff changed local ledger before Host result: ' + field)
    action_id = submitted.get('action_uuid')
    try:
        if submitted.get('submitted') is not True or str(uuid.UUID(action_id)) != action_id:
            raise ValueError('actual action UUID missing')
    except (ValueError, TypeError, AttributeError) as error:
        raise AcceptanceError('Staff lacks confirmed intent submission; no replay: ' + str(error)) from error
    runner.checked('staff-submitted', {'action_uuid': action_id, 'submitted': True, 'host_completion_confirmed': False})
    deadline, sample = time.monotonic() + 60, 0
    while True:
        sample += 1
        final = {peer: runner.single('staff-result-' + peer + '-' + str(sample), peer, 'game_read_native_lobby')
                 for peer in ('host', 'client-1')}
        for peer, context in final.items():
            state = context['controller']
            if state.get('locked') or state.get('failed') != before[peer]['failed']:
                raise AcceptanceError('Staff failed peer=' + peer + ' cause=' + str(state.get('cause')))
            if state.get('pending') is True and state.get('outcome') == 0:
                if (context['prepare'].get('playing') is not True or context['prepare'].get('error')
                        or context['snapshot'].get('world_authority_valid') is not True):
                    raise AcceptanceError('Pending Staff lost actual world/session on ' + peer)
            elif not ready(context):
                raise AcceptanceError('Peer lost Playing during Staff: ' + peer)
        if all(context['controller'].get('action_uuid') == action_id
               and context['controller'].get('pending') is False and context['controller'].get('outcome') == 1
               and context['controller'].get('confirmed') == before[peer]['confirmed'] + 1
               for peer, context in final.items()):
            break
        if time.monotonic() >= deadline:
            raise AcceptanceError('Staff result timeout required<60s; no replay: ' + str(final))
    for peer, context in final.items():
        state = context['controller']
        if (state.get('attempted') != before[peer]['attempted'] + 1
                or state.get('host_revision') != before[peer]['host_revision'] + 1):
            raise AcceptanceError('Staff native attempt/revision delta differs from1 on ' + peer)
        runner.checked('staff-confirmed-controller-' + peer, state)
    if final['host']['controller']['ack_mask'] != final['host']['controller']['required_mask']:
        raise AcceptanceError('Staff Host lacks the complete Client adoption ACK mask')
    allocated = runner.single('staff-allocated-host', 'host', 'game_read_tutorial_development')
    expected_project = {**baseline['project'], 'desired_workers': 1}
    if (allocated['project'] != expected_project or allocated['startingpoint'] != baseline['startingpoint']
            or allocated['startingpoint_release'] != baseline['startingpoint_release']):
        raise AcceptanceError('Staff native result differs outside desired workers1')
    shared_state('staff-allocated', allocated)
    ledger_after = compare_ledgers(runner.inspect('staff-ledger-after-host', 'host'),
                                   runner.inspect('staff-ledger-after-client', 'client-1'))
    if ledger_after != ledger_before:
        raise AcceptanceError('Staff action changed full native ledger')
    progress = runner.single('staff-capture-progress', 'client-1', 'game_read_tutorial_progress')
    if (progress.get('client_staff_suppressed') is not True or progress.get('client_staff_submitted') is not True
            or progress.get('client_staff_action_uuid') != action_id or progress.get('client_staff_cause')
            or progress.get('client_staff_desired_workers') != 1):
        raise AcceptanceError('Staff hook capture receipt differs from confirmed action')
    runner.checked('staff-capture', progress)
    runner.summary.update(action_uuid=action_id, ordinary_client_staff_proven=True,
                          two_peer_action_confirmation_proven=True, ledger_equality_proven=True)
    host_time(1)
    deadline, sample = time.monotonic() + 60, 0
    while True:
        sample += 1
        work = runner.single('staff-natural-work-' + str(sample), 'host', 'game_read_tutorial_development')
        row = work['project']
        if row.get('present') is not True or row.get('desired_workers') != 1:
            raise AcceptanceError('Natural Staff work lost current project/desired workers1')
        if row.get('work', 0) > allocated['project']['work'] or row.get('lines', 0) > allocated['project']['lines']:
            break
        if time.monotonic() >= deadline:
            raise AcceptanceError('Natural Host work did not advance required<60s; actual=' + str(row))
    runner.checked('staff-natural-work-observed', work)
    host_time(0)
    after = runner.single('staff-natural-work-paused-host', 'host', 'game_read_tutorial_development')
    shared_state('staff-natural-work', after)
    ledger = compare_ledgers(runner.inspect('staff-work-ledger-host', 'host'),
                             runner.inspect('staff-work-ledger-client', 'client-1'))
    runner.checked('staff-work-ledger', ledger)
    runner.checked('staff-host-frame', runner.single('staff-host-frame', 'host', 'game_capture_frame'))
    runner.summary.update(before=baseline, allocated=allocated, after=after, natural_host_work_proven=True,
                          client_adoption_proven=True, diagnostic_completion_used=False)


def client_commit():
    baseline = runner.single('client-commit-baseline', 'host', 'game_read_tutorial_development')
    runner.checked('client-commit-baseline', baseline)
    if (baseline['project'].get('present') is not True or baseline['project'].get('tech_key') != 'startingpoint'
            or baseline['startingpoint'].get('purchased') is not True
            or baseline['startingpoint'].get('deployed') is not False
            or baseline['startingpoint_release'].get('change_count') != 0):
        raise AcceptanceError('Client Commit requires the current unowned initial Startingpoint project')
    shared_state('client-commit-baseline', baseline)
    if args.natural_work:
        already_complete = baseline['project'].get('work', 0) >= 7200 and all(
            baseline['project'].get(key) == 0 for key in ('bugs_total', 'bugs_known', 'bugs_unknown'))
        if not already_complete and baseline['project'].get('desired_workers') != 1:
            raise AcceptanceError('Natural completion requires the accepted one-worker project')
        if not already_complete:
            host_time(9)
        deadline, sample = time.monotonic() + 180, 0
        while not already_complete:
            sample += 1
            development = runner.single('natural-completion-' + str(sample), 'host', 'game_read_tutorial_development')
            project = development['project']
            if project.get('present') is not True or project.get('tech_key') != 'startingpoint':
                raise AcceptanceError('Natural completion lost the current Startingpoint project')
            runner.checked('natural-completion-' + str(sample), project)
            if project.get('work', 0) >= 7200 and all(project.get(key) == 0 for key in ('bugs_total', 'bugs_known', 'bugs_unknown')):
                break
            if time.monotonic() >= deadline:
                host_time(0)
                raise AcceptanceError('Natural Startingpoint completion not ready required<180s work>=7200/bugs0; actual=' + str(project))
        if not already_complete:
            host_time(0)
        runner.summary.update(natural_host_completion_observed=True, completion_diagnostic_used=False,
                              already_complete_at_entry=already_complete)
    else:
        fixed_operation('host', 'complete', baseline)
    completed = runner.single('host-completed-project', 'host', 'game_read_tutorial_development')
    runner.checked('host-completed-project', completed)
    shared_state('client-complete', completed)
    before = {}
    for peer in ('host', 'client-1'):
        context = runner.single('before-client-commit-' + peer, peer, 'game_read_native_lobby')
        if not ready(context) or context['controller'].get('pending') is not False:
            raise AcceptanceError('Client Commit requires both peers Playing with an idle Controller')
        before[peer] = context['controller']
    ledger_before = compare_ledgers(runner.inspect('commit-ledger-before-host', 'host'),
                                    runner.inspect('commit-ledger-before-client', 'client-1'))
    client = runner.single('client-completed-project', 'client-1', 'game_read_tutorial_development')
    fixed_operation('client-1', 'client_show', client)
    runner.checked('client-button-before', runner.single('client-button-before', 'client-1', 'game_capture_frame'))
    client = runner.single('client-button-current-project', 'client-1', 'game_read_tutorial_development')
    submitted = fixed_operation('client-1', 'client_commit', client)
    action_id = submitted.get('action_uuid')
    try:
        if submitted.get('submitted') is not True or str(uuid.UUID(action_id)) != action_id:
            raise ValueError('actual action UUID missing')
    except (ValueError, TypeError, AttributeError) as error:
        raise AcceptanceError('Client button has no confirmed submission identity; no replay: ' + str(error)) from error
    runner.checked('submitted-action', {'action_uuid': action_id, 'submitted': True, 'result_confirmed': False})
    deadline = time.monotonic() + 60
    sample = 0
    while True:
        sample += 1
        final = {peer: runner.single('commit-result-' + peer + '-' + str(sample), peer, 'game_read_native_lobby')
                 for peer in ('host', 'client-1')}
        for peer, context in final.items():
            state = context['controller']
            if state.get('locked') or state.get('failed') != before[peer]['failed']:
                raise AcceptanceError('Client Commit failed peer=' + peer + ' cause=' + str(state.get('cause')))
            if state.get('pending') is True and state.get('outcome') == 0:
                if (context['prepare'].get('playing') is not True or context['prepare'].get('error')
                        or context['snapshot'].get('world_authority_valid') is not True):
                    raise AcceptanceError('Pending Client Commit lost its actual world/session on ' + peer)
                if state.get('cause'):
                    runner.checked('pending-commit-' + peer + '-' + str(sample),
                                   {'action_uuid': state.get('action_uuid'), 'cause': state['cause']})
            elif not ready(context):
                raise AcceptanceError('Peer lost Playing during Client Commit: ' + peer)
        if all(context['controller'].get('action_uuid') == action_id
               and context['controller'].get('pending') is False
               and context['controller'].get('outcome') == 1
               and context['controller'].get('confirmed') == before[peer]['confirmed'] + 1
               for peer, context in final.items()):
            break
        if time.monotonic() >= deadline:
            raise AcceptanceError('Client Commit result timeout required<60s; no replay: ' +
                                  str({peer: context['controller'] for peer, context in final.items()}))
    for peer, context in final.items():
        state = context['controller']
        if state.get('attempted') != before[peer]['attempted'] + 1:
            raise AcceptanceError('Client Commit native attempt count differs from1 on ' + peer)
        if state.get('host_revision') != before[peer]['host_revision'] + 1:
            raise AcceptanceError('Client Commit ordered Host revision delta differs from1 on ' + peer)
        runner.checked('confirmed-controller-' + peer, state)
    if final['host']['controller']['ack_mask'] != final['host']['controller']['required_mask']:
        raise AcceptanceError('Host lacks the full required native adoption ACK mask')
    actual = runner.single('after-client-commit-host', 'host', 'game_read_tutorial_development')
    runner.checked('after-client-commit-host', actual)
    project, technology, release = actual['project'], actual['startingpoint'], actual['startingpoint_release']
    if (project.get('present') is not False or technology.get('deployed') is not True
            or technology.get('owned') is not True or technology.get('inventory_count') != 1
            or release.get('feature_count') != 1 or release.get('change_count') != 1
            or release.get('mutation_counter') != 1):
        raise AcceptanceError('Host result lacks exact native project retirement and one inventory/Release effect')
    shared_state('client-commit', actual)
    ledger_after = compare_ledgers(runner.inspect('commit-ledger-after-host', 'host'),
                                   runner.inspect('commit-ledger-after-client', 'client-1'))
    if ledger_after != ledger_before:
        raise AcceptanceError('Client Commit changed full native cash/current-day ledger')
    progress = runner.single('client-commit-progress', 'client-1', 'game_read_tutorial_progress')
    if (progress.get('client_commit_submitted') is not True or progress.get('client_commit_suppressed') is not True
            or progress.get('client_commit_action_uuid') != action_id or progress.get('client_commit_cause')):
        raise AcceptanceError('Ordinary Client Commit hook submission receipt differs')
    runner.checked('client-hook-submission', progress)
    runner.checked('client-commit-host-frame', runner.single('client-commit-host-frame', 'host', 'game_capture_frame'))
    runner.checked('commit-ledger-unchanged', ledger_after)
    runner.summary.update(before=baseline, after=actual, action_uuid=action_id,
                          ordinary_client_commit_proven=True, client_adoption_proven=True,
                          two_peer_action_confirmation_proven=True, ledger_equality_proven=True)


def begin_project():
    baseline = runner.single('begin-baseline', 'host', 'game_read_tutorial_development')
    runner.checked('begin-baseline', baseline)
    if (baseline.get('schema') != 1 or baseline.get('mainthread_confirmed') is not True
            or baseline.get('lifetime_confirmed') is not True):
        raise AcceptanceError('Begin requires confirmed native development observation')
    project, technology = baseline['project'], baseline['startingpoint']
    if project.get('present') is not False or any(project.get(field) != 0 for field in
            ('work', 'lines', 'desired_workers', 'bugs_total', 'bugs_known')):
        raise AcceptanceError('Begin requires no current project and reset native counters; no mutation attempted')
    if (technology.get('record_present') is not True or any(technology.get(field) is not False
            for field in ('purchased', 'deployed', 'custom', 'owned'))
            or technology.get('inventory_count') != 0 or baseline['startingpoint_release'].get('feature_count') != 0):
        raise AcceptanceError('Begin requires an existing unpurchased unowned record and no item/feature; no mutation attempted')
    shared_state('begin-baseline', baseline)
    before = compare_ledgers(runner.inspect('begin-before-host', 'host'),
                             runner.inspect('begin-before-client', 'client-1'))
    runner.checked('begin-ledger-before', before)
    request = {'token': str(uuid.uuid4()), 'native_origin_uuid': baseline['snapshot_uuid'],
               'world_epoch': baseline['world_epoch'], 'world_generation': baseline['world_generation'],
               'operation': 'begin', 'expected_development': baseline}
    runner.checked('begin-request', request)
    result = runner.single('begin-once', 'host', 'game_tutorial_startingpoint', request)
    for field in ('attempted', 'returned', 'confirmed', 'payment_accepted', 'set_current_attempted'):
        if result.get(field) is not True:
            raise AcceptanceError('Begin lacks native ' + field + '; no replay')
    if (result.get('unknown') is not False or result.get('pay_exception') != 0
            or result.get('set_current_exception') != 0 or result.get('token') != request['token']
            or result.get('operation') != 'begin' or result.get('before') != baseline):
        raise AcceptanceError('Begin native outcome/token/baseline differs; no replay')
    runner.checked('begin', result)
    after = runner.single('after-begin', 'host', 'game_read_tutorial_development')
    runner.checked('after-begin', after)
    if any(after.get(field) != baseline.get(field) for field in
           ('schema', 'mainthread_confirmed', 'lifetime_confirmed', 'snapshot_uuid', 'world_epoch', 'world_generation')):
        raise AcceptanceError('Begin changed native observation/origin; no replay')
    if result.get('after') != after:
        raise AcceptanceError('Begin current native development differs from original receipt; no replay')
    project, technology = after['project'], after['startingpoint']
    if (project.get('present') is not True or project.get('kind') != 0 or project.get('tech_key') != 'startingpoint'
            or technology.get('record_present') is not True or technology.get('purchased') is not True
            or any(technology.get(field) is not False for field in ('deployed', 'custom', 'owned'))
            or technology.get('inventory_count') != baseline['startingpoint']['inventory_count']
            or after['startingpoint_release'] != baseline['startingpoint_release']):
        raise AcceptanceError('Begin did not produce the purchased, unowned current Startingpoint project; no replay')
    shared_state('begin', after)
    after_ledger = compare_ledgers(runner.inspect('begin-after-host', 'host'),
                                   runner.inspect('begin-after-client', 'client-1'))
    price = result.get('price')
    if type(price) is not int or price < 0:
        raise AcceptanceError('Begin native price is absent/invalid; no replay')
    expected = {**before, 'line_counts': list(before['line_counts']), 'line_income': list(before['line_income'])}
    expected['cash'] -= price
    expected['record_cash'] -= price
    expected['record_expense'] += price
    expected['line_counts'][3] += 1
    expected['line_income'][3] -= price
    if after_ledger != expected:
        raise AcceptanceError('Begin full ledger differs from exactly one category3 purchase; no replay: '
                              + str({'expected': expected, 'actual': after_ledger}))
    for field in before:
        if result.get('ledger_before', {}).get(field) != before[field] or result.get('ledger_after', {}).get(field) != after_ledger[field]:
            raise AcceptanceError('Begin original native ledger receipt differs for ' + field + '; no replay')
    runner.checked('begin-ledger-after', after_ledger)
    runner.checked('begin-host-frame', runner.single('begin-host-frame', 'host', 'game_capture_frame'))
    runner.summary.update(before=baseline, after=after, native_host_begin_proven=True,
                          client_adoption_proven=True, ledger_equality_proven=True, native_purchase_price=price)

try:
    runner.load_manifests()
    if runner.summary['runtime_run_id'] != args.expected_run_id:
        raise AcceptanceError('Owned run differs; no mutation attempted')
    digest = hash_file(ROOT / 'build/bin/Release/MT2_Multiplayer.dll')
    for peer, manifest in runner.manifests.items():
        if read_json(manifest)['installed_binary_sha256']['MT2_Multiplayer.dll'] != digest:
            raise AcceptanceError('Candidate DLL differs on ' + peer)
    runner.summary['dll_sha256'] = digest
    if args.phase == 'prepare':
        for peer in ('host', 'client-1'):
            runner.poll_title(peer)
            state = runner.single('idle-' + peer, peer, 'game_read_native_lobby')
            if state.get('admitted') is not False or state['controller'].get('initialized') is not False:
                raise AcceptanceError('Fixture requires an unadmitted, uninitialized peer: ' + peer)
        runner.checked('load-once', runner.single('load-once', 'host', 'game_load_snapshot', {'snapshot_id': args.snapshot}))
        deadline = time.monotonic() + 60
        while True:
            state = runner.inspect('loaded-host', 'host')
            world = state.get('native_world', {})
            if world.get('active_mode') == 3 and world.get('mode_state') == 4 and world.get('map_task_pending') is False:
                break
            if time.monotonic() >= deadline:
                raise AcceptanceError('Host load did not reach idle mode3/state4 within60s; no replay')
        runner.checked('loaded-development', runner.single('loaded-development', 'host', 'game_read_tutorial_development'))
    elif args.phase == 'client-prove':
        client_commit()
    elif args.phase == 'client-start':
        client_start()
    elif args.phase == 'client-staff':
        client_staff()
    elif args.phase == 'begin':
        begin_project()
    elif args.phase == 'prove':
        baseline = runner.single('baseline', 'host', 'game_read_tutorial_development')
        runner.checked('baseline', baseline)
        release = baseline['startingpoint_release']
        if release['feature_count'] != 0 or release['feature_present'] is not False:
            raise AcceptanceError('Starting Point Release feature must be absent before Commit')
        if args.require_client:
            shared_state('baseline', baseline)
        current = baseline
        for operation in ('show', 'complete', 'commit'):
            request = {'token': str(uuid.uuid4()), 'native_origin_uuid': current['snapshot_uuid'],
                       'world_epoch': current['world_epoch'], 'world_generation': current['world_generation'],
                       'operation': operation, 'expected_development': current}
            result = runner.single(operation + '-once', 'host', 'game_tutorial_startingpoint', request)
            for field in ('attempted', 'returned', 'confirmed'):
                if result.get(field) is not True:
                    raise AcceptanceError(operation + ' lacks native ' + field + '; no replay')
            if result.get('unknown') is not False:
                raise AcceptanceError(operation + ' effect is unknown; no replay')
            runner.checked(operation, result)
            current = runner.single('after-' + operation, 'host', 'game_read_tutorial_development')
            runner.checked('after-' + operation, current)
            runner.checked('frame-' + operation, runner.single('frame-' + operation, 'host', 'game_capture_frame'))
            if args.require_client:
                shared_state(operation, current)
        actual = current['startingpoint_release']
        if actual['feature_count'] != 1 or actual['feature_present'] is not True:
            raise AcceptanceError('Original Commit did not insert exactly one native Starting Point Release feature')
        if actual['mutation_counter'] != (release['mutation_counter'] + 1) % (1 << 32):
            raise AcceptanceError('Original Commit Release mutation counter delta differs from1')
        runner.summary.update(before=baseline, after=current, native_host_commit_proven=True,
                              client_adoption_proven=args.require_client)
    else:
        previous = {}
        for peer in ('client-1', 'host'):
            state = runner.single('before-leave-' + peer, peer, 'game_read_native_lobby')
            previous[peer] = state['prepare']['leave_confirmed']
            button(runner, peer, 'leave')
        observe(runner, 'retired', lambda peer, value: left(value, previous[peer]))
    runner.summary.update(passed=True, state='startingpoint-' + args.phase + '-confirmed')
except Exception as error:
    runner.summary.update(passed=False, state='failed-no-replay', error={'step': runner.summary['step'], 'cause': str(error)})
runner.summary['finished_utc'] = utc()
runner.persist()
print(runner.summary_path)
raise SystemExit(0 if runner.summary['passed'] else 1)
