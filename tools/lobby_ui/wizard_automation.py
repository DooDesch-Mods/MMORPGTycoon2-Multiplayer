"""Complete the original New Game wizard through acknowledged native MCP operations."""
import json
import time
import uuid

PAGE_IDS = tuple('desktop_newgame_' + name + '_displayname'
                 for name in ('name', 'type', 'affinity', 'map', 'logo', 'summary'))
TARGET = 'wizard:doodesch.multiplayer:'


class WizardAutomationError(RuntimeError):
    pass


def _require(condition, cause):
    if not condition:
        raise WizardAutomationError(cause)


def complete_wizard(ui, *, record, capture, checkpoint, wait_seconds=120, clock=time.monotonic):
    """Select required native values, confirm five pages, then invoke Create once.

    Only observations may repeat while a page or original button is pending.
    A returned mutation without its acknowledgement remains an error, never a retry.
    """
    _require(0 < wait_seconds <= 180, 'Wizard wait must be greater than0 and at most180 seconds')

    def request(action, **arguments):
        return ui.request('desktop_activate', {'target': TARGET + action, **arguments})

    def state():
        value = request('state')
        _require(value.get('status') == 'observed' and value.get('confirmed') is True
                 and value.get('native_attempted') is False,
                 'Native wizard state was not observed: ' + json.dumps(value))
        page = value.get('page')
        _require(type(page) is int and 1 <= page <= 6 and value.get('page_id') == PAGE_IDS[page - 1]
                 and type(value.get('map_ready')) is bool,
                 'Native wizard page identity or map state differs: ' + json.dumps(value))
        return value

    def wait_page(expected, previous):
        deadline = clock() + wait_seconds
        while True:
            value = state()
            if value['page'] == expected:
                record('native-wizard-page-' + str(expected) + '-confirmed', value)
                return value
            _require(value['page'] == previous,
                     f'Native wizard page changed unexpectedly: expected={expected} previous={previous} actual={value["page"]}; no further input')
            _require(clock() < deadline,
                     f'Native wizard page timed out: expected={expected} actual={value["page"]} required_seconds={wait_seconds}; Next not replayed')

    def select(action, field, page):
        value = state()
        _require(value['page'] == page, 'Native selection belongs to page' + str(page))
        arguments = {('icon_ordinal' if action == 'select-icon' else 'type_ordinal'): 0}
        observed = request(action, observe=True, **arguments)
        _require(observed.get('guard_passed') is True and observed.get('native_attempted') is False
                 and observed.get('result') == 'observed' and isinstance(observed.get(field), str),
                 'Native selection target was not observed: ' + json.dumps(observed))
        if observed[field]:
            record('native-wizard-' + action + '-preserved', observed)
            return
        checkpoint('native-wizard-' + action + '-once')
        selected = request(action, **arguments)
        record('native-wizard-' + action + '-receipt', selected)
        _require(selected.get('confirmed') is True and selected.get('native_attempted') is True
                 and selected.get('native_returned') is True and selected.get('result') == 'confirmed',
                 'Native selection not confirmed; no replay: ' + json.dumps(selected))

    def wait_button(action, page):
        deadline = clock() + wait_seconds
        first = True
        while True:
            current = state()
            _require(current['page'] == page, 'Wizard left the expected page before ' + action)
            observed = request(action, observe=True)
            _require(observed.get('native_attempted') is False and isinstance(observed.get('guards'), dict),
                     'Native button guard was not observed: ' + json.dumps(observed))
            if observed.get('guard_passed') is True and (action != 'create' or current['map_ready']):
                record('native-wizard-' + action + '-ready-page-' + str(page), observed)
                return
            guards = observed['guards']
            _require(observed.get('guard_passed') is True or (
                guards.get('button_found') is True and guards.get('lookup_exception') == 0
                and guards.get('owner_matches_window') is True and guards.get('disabled_read') is True
                and guards.get('disabled') == 1 and guards.get('shown_read') is True
                and guards.get('visible') is True and guards.get('command_matches') is True),
                'Native button rejected before execution: ' + json.dumps(observed))
            if first:
                record('native-wizard-' + action + '-waiting-page-' + str(page),
                       {'state': current, 'button': observed})
                first = False
            _require(clock() < deadline,
                     f'Native wizard button timed out: action={action} page={page} required_seconds={wait_seconds} '
                     + 'last_state=' + json.dumps(current) + ' last_guard=' + json.dumps(observed) + '; no mutation attempted')

    def affinity_values(value):
        _require(isinstance(value, dict), 'Native affinity snapshot is missing: ' + json.dumps(value))
        fields = ('base', 'allocation', 'displayed')
        _require(all(isinstance(value.get(key), list) and len(value[key]) == 4
                     and all(type(number) is int for number in value[key]) for key in fields)
                 and type(value.get('remaining')) is int and value['remaining'] >= 0,
                 'Native affinity snapshot is malformed: ' + json.dumps(value))
        _require(all(base + allocation == displayed for base, allocation, displayed
                     in zip(value['base'], value['allocation'], value['displayed'])),
                 'Native affinity values disagree: ' + json.dumps(value))
        return {key: value[key] for key in (*fields, 'remaining')}

    def allocate_affinity():
        deadline = clock() + wait_seconds
        previous = None
        points = 0
        while True:
            _require(state()['page'] == 3, 'Native affinity allocation left its original page; no further input')
            observed = request('allocate-affinity', affinity_ordinal=0, observe=True)
            _require(observed.get('guard_passed') is True and observed.get('native_attempted') is False
                     and observed.get('result') == 'observed',
                     'Native affinity target was not observed: ' + json.dumps(observed))
            before = affinity_values(observed.get('before'))
            _require(previous is None or before == previous,
                     'Native affinity changed after confirmed allocation; no further input: ' + json.dumps(observed))
            if before['remaining'] == 0:
                record('native-wizard-affinity-complete', {'allocated_points': points, 'observation': observed})
                capture('native-wizard-affinity-complete')
                return
            _require(clock() < deadline,
                     'Native affinity allocation timed out before next increment: ' + json.dumps(observed))
            token = str(uuid.uuid4())
            checkpoint('native-wizard-affinity-once-' + str(points + 1))
            allocated = request('allocate-affinity', affinity_ordinal=0, affinity_token=token,
                                expected_remaining=before['remaining'], expected_values=before['displayed'])
            record('native-wizard-affinity-receipt-' + str(points + 1), allocated)
            _require(allocated.get('confirmed') is True and allocated.get('native_attempted') is True
                     and allocated.get('native_returned') is True and allocated.get('result') == 'confirmed',
                     'Native affinity allocation not confirmed; no replay: ' + json.dumps(allocated))
            after = affinity_values(allocated.get('after'))
            expected = {key: list(value) if isinstance(value, list) else value for key, value in before.items()}
            expected['allocation'][0] += 1
            expected['displayed'][0] += 1
            expected['remaining'] -= 1
            _require(affinity_values(allocated.get('before')) == before and after == expected,
                     'Native affinity increment readback differs; no replay: ' + json.dumps(allocated))
            previous = after
            points += 1

    initial = state()
    _require(initial['page'] == 1, 'Original wizard must start on its actual Name page; no reset or input attempted')
    record('native-wizard-page-1-confirmed', initial)
    select('select-icon', 'icon_before', 1)
    capture('native-wizard-page-1')
    for expected in range(2, 7):
        previous = expected - 1
        if previous == 2:
            select('select-type', 'type_before', 2)
        if previous == 3:
            allocate_affinity()
        wait_button('next', previous)
        checkpoint('native-wizard-next-once-page-' + str(previous))
        advanced = request('next')
        record('native-wizard-next-receipt-page-' + str(previous), advanced)
        _require(advanced.get('native_attempted') is True and advanced.get('native_returned') is True
                 and advanced.get('result') == 'accepted',
                 'Original Next did not return; no replay: ' + json.dumps(advanced))
        wait_page(expected, previous)
        capture('native-wizard-page-' + str(expected))
    wait_button('create', 6)
    checkpoint('original-create-once')
    created = request('create')
    record('native-wizard-create-receipt', created)
    _require(created.get('native_attempted') is True and created.get('native_returned') is True
             and created.get('result') == 'accepted',
             'Original Create did not return; no replay: ' + json.dumps(created))
    return created
