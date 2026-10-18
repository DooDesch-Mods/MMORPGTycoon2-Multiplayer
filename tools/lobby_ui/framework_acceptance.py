"""Native Framework UI branch for the existing lobby acceptance helper."""
import json
import time
import uuid
from pathlib import Path

from local_acceptance import AcceptanceError, log, read_json, write_json
from runtime import Runtime, FRAMEWORK_PLUGIN_KIND, manifest_runtime_kind

OWNER = 'doodesch.multiplayer'


class _SdkReadTimeout(AcceptanceError):
    pass


def _read_receipt(observations):
    return {target: {key: value for key, value in record.items() if not key.startswith('_')}
            for target, record in observations.items() if record['failures']}


def _read_sdk_json(runtime, path, deadline, observations):
    target = str(path)
    record = observations.setdefault(target, dict(failures=0, recoveries=0, first_cause='', last_cause=''))
    last_error = None
    while True:
        runtime._current()
        runtime._no_framework_alias(path)
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            error = _SdkReadTimeout('Framework SDK read timed out target=' + target
                                    + ' failures=' + str(record['failures'])
                                    + ' first_cause=' + (record['first_cause'] or 'none')
                                    + ' last_cause=' + (record['last_cause'] or 'none')
                                    + '; request retained; no replay')
            raise error from last_error
        try:
            value = read_json(path)
        except (PermissionError, json.JSONDecodeError) as cause:
            last_error = cause
            reason = type(cause).__name__ + ': ' + str(cause)
            record['failures'] += 1
            if not record['first_cause']:
                record['first_cause'] = reason
            now = time.monotonic()
            if record['failures'] == 1 or (type(cause).__name__ != record.get('_last_type')
                                         and now - record.get('_last_log', 0) >= 5):
                log('warn', 'Framework SDK read waiting target=' + target
                    + ' failures=' + str(record['failures']) + ' cause=' + reason
                    + '; observing only; no replay')
                record['_last_log'] = now
            record['_last_type'] = type(cause).__name__
            record['last_cause'] = reason
            time.sleep(min(.1, max(0, deadline - time.monotonic())))
        else:
            if last_error is not None:
                record['recoveries'] += 1
                now = time.monotonic()
                if record['recoveries'] == 1 or now - record.get('_last_recovery_log', 0) >= 5:
                    log('info', 'Framework SDK read recovered target=' + target
                        + ' failures=' + str(record['failures']) + ' first_cause=' + record['first_cause'])
                    record['_last_recovery_log'] = now
            return value


class FrameworkUI:
    def __init__(self, runner, peer):
        self.runner, self.peer = runner, peer
        self.runtime = Runtime(runner.root)
        self.identity = self.runtime.bind(str(runner.manifests[peer]))['bound']
        if (manifest_runtime_kind(self.identity) != FRAMEWORK_PLUGIN_KIND
                or self.identity['run_id'] != runner.summary['runtime_run_id']):
            raise AcceptanceError('Framework UI exact run/kind differs; no input attempted')
        self.directory = Path(self.identity['framework_profile']) / 'diagnostics'
        self.runtime._no_framework_alias(self.directory)

    def request(self, operation, arguments=None):
        deadline = time.monotonic() + self.runner.args.wait_seconds
        observations = {}
        self.runtime._current()
        pending, response = self.directory / 'request.json', self.directory / 'response.json'
        for path in (pending, response):
            self.runtime._no_framework_alias(path)
        if pending.exists():
            previous = _read_sdk_json(self.runtime, pending, deadline, observations)
            actual = _read_sdk_json(self.runtime, response, deadline, observations) if response.exists() else {}
            if any(actual.get(key) != previous.get(key) for key in ('id', 'pid', 'process_start_time')):
                raise AcceptanceError('Framework request has no exact acknowledgement; retained request must not be replayed: ' + str(pending))
        if time.monotonic() >= deadline:
            raise AcceptanceError('Framework ' + operation + ' observation deadline reached before publication; target='
                                  + str(pending) + '; previous request retained; no input attempted; reads=' + str(_read_receipt(observations)))
        self.runtime._current()
        self.runtime._no_framework_alias(pending)
        identifier = str(uuid.uuid4())
        command = dict(id=identifier, pid=self.identity['pid'], process_start_time=self.identity['process_start_time'],
                       operation=operation, arguments=arguments or {})
        evidence = self.runner.directory / 'framework-ui' / (identifier + '.json')
        record = dict(peer=self.peer, request=command, state='attempted', mailbox=str(pending))
        write_json(evidence, record)
        write_json(pending, command)
        while time.monotonic() < deadline:
            self.runtime._current()
            if response.exists():
                try:
                    actual = _read_sdk_json(self.runtime, response, deadline, observations)
                except _SdkReadTimeout as error:
                    record.update(state='observation-timeout', error=str(error), read_observations=_read_receipt(observations))
                    write_json(evidence, record)
                    raise
                if actual.get('id') == identifier:
                    record.update(response=actual, state='response-observed', read_observations=_read_receipt(observations))
                    if any(actual.get(key) != command[key] for key in ('pid', 'process_start_time')):
                        record['state'] = 'identity-rejected'
                        write_json(evidence, record)
                        raise AcceptanceError('Framework response process identity differs: ' + str(evidence))
                    record['state'] = 'acknowledged'
                    write_json(evidence, record)
                    if actual.get('ok') is not True:
                        raise AcceptanceError('Framework ' + operation + ' failed: ' + str(actual.get('error')) + '; evidence=' + str(evidence))
                    self.runner.checked('framework-' + operation + '-' + self.peer, dict(evidence=str(evidence), response=actual))
                    return actual['result']
            time.sleep(min(.1, max(0, deadline - time.monotonic())))
        cause = 'No exact SDK response target=' + str(response) + '; request retained; no replay; reads=' + str(_read_receipt(observations))
        record.update(state='observation-timeout', error=cause, read_observations=_read_receipt(observations))
        write_json(evidence, record)
        raise AcceptanceError('Framework ' + operation + ' timed out; ' + cause + '; evidence=' + str(evidence))

    def read(self):
        value = self.runner.single('framework-lobby-read-' + self.peer, self.peer, 'game_read_native_lobby')
        if value.get('cause') or value.get('prepare', {}).get('error'):
            raise AcceptanceError('Framework lobby failed: ' + str(value.get('cause') or value['prepare']['error']))
        return value

    def wait(self, predicate, label):
        deadline = time.monotonic() + self.runner.args.wait_seconds
        value = None
        while time.monotonic() < deadline:
            value = self.read()
            if predicate(value):
                self.runner.checked('framework-' + label + '-' + self.peer, value)
                return value
        raise AcceptanceError('Framework ' + label + ' timed out; no mutation replay; last=' + str(value))

    def activate(self, target):
        receipt = self.request('desktop_activate', {'target': target})
        if receipt.get('handler_reached') is not True:
            raise AcceptanceError('Framework native activation handler not reached: ' + str(receipt))
        return receipt

    def open(self):
        initial = self.read()
        if visible(initial):
            return initial
        status = self.request('status')
        mods = status['mods']
        matches = [i for i, mod in enumerate(mods) if mod.get('id') == OWNER and mod.get('active') is True]
        if len(matches) != 1:
            raise AcceptanceError('Framework active Multiplayer row is not unique')
        desktop = self.request('desktop_status')
        if desktop.get('error'):
            raise AcceptanceError('Framework desktop failed: ' + str(desktop['error']))
        if desktop.get('panel_open') is not True:
            if initial.get('title_active') is True:
                self.activate('icon')
            elif initial.get('title_active') is False:
                launches = desktop.get('start_menu_launches')
                if type(launches) is not int or launches < 0:
                    raise AcceptanceError('Framework Start menu launch counter unavailable; no input attempted')
                shown = self.request('desktop_activate', {'target': 'start_menu'})
                if (shown.get('menu_registered') is not True or type(shown.get('launches')) is not int
                        or shown.get('launches') != launches):
                    raise AcceptanceError('Framework Start menu registration/count differs; no Mods selection attempted')
                opened = self.request('desktop_activate', {'target': 'start_mods'})
                if (opened.get('menu_registered') is not True or opened.get('panel_open') is not True
                        or type(opened.get('launches')) is not int or opened.get('launches') != launches + 1):
                    raise AcceptanceError('Framework Start menu Mods launch not confirmed exactly once; no replay')
            else:
                raise AcceptanceError('Framework title/ingame state unavailable; no input attempted')
            desktop = self.request('desktop_status')
            if desktop.get('error') or desktop.get('panel_open') is not True:
                raise AcceptanceError('Framework Mods panel not open after native entry: ' + str(desktop))
        target_page, slot = divmod(matches[0], 6)
        page = desktop.get('page')
        if type(page) is not int or page < 0 or page >= max(1, (len(mods) + 5) // 6):
            raise AcceptanceError('Framework native mod page unreadable')
        while page != target_page:
            self.activate('next' if page < target_page else 'previous')
            fresh = self.request('desktop_status')
            expected = page + (1 if page < target_page else -1)
            if fresh.get('page') != expected:
                raise AcceptanceError('Framework native mod page did not advance once')
            page = expected
        selected = self.activate('mod' + str(slot))
        if selected.get('selected') != OWNER:
            raise AcceptanceError('Framework native selection differs; no app open attempted')
        self.activate('open_app')
        return self.wait(visible, 'open-visible')

    def click(self, widget, predicate=lambda value: True):
        before = self.read()
        if not visible(before):
            raise AcceptanceError('Framework app is not ready and visible; no input attempted')
        ui = before['framework_ui']
        counters(ui)
        if ui['queued'] != ui['processed']:
            raise AcceptanceError('Framework action already pending; no input attempted')
        # The native view validates the clicked widget's current visibility and
        # enabled state before queueing. Its processed receipt is required below.
        self.activate('app:' + OWNER + ':' + widget)
        return self.wait(lambda value: processed(value, before, widget) and predicate(value), widget + '-processed')


def counters(ui):
    if any(type(ui.get(key)) is not int or ui[key] < 0 for key in ('queued', 'processed', 'rejected')):
        raise AcceptanceError('Framework action counters unavailable')


def visible(value):
    ui = value.get('framework_ui', {})
    return (ui.get('app_id') == 'main' and all(ui.get(key) is True for key in ('ready', 'open', 'visibility_known', 'visible'))
            and ui.get('suspended') is False)


def processed(value, before, widget):
    ui, old = value.get('framework_ui', {}), before['framework_ui']
    counters(ui)
    action = ui.get('last_action') or {}
    if ui['rejected'] != old['rejected'] or action.get('handler_failed') is True:
        raise AcceptanceError('Framework action rejected: ' + str(action))
    if ui['queued'] > old['queued'] + 1 or ui['processed'] > old['processed'] + 1:
        raise AcceptanceError('Framework action counters include another input; no replay')
    if ui['queued'] != old['queued'] + 1 or ui['processed'] != old['processed'] + 1:
        return False
    if (action.get('widget') != widget or action.get('sequence') != ui['queued']
            or action.get('attempted') is not True or action.get('handler_failed') is not False):
        raise AcceptanceError('Framework processed action identity/receipt differs: ' + str(action))
    return True


def button(runner, peer, name, **extra):
    if name not in ('open', 'host', 'join', 'pick', 'start', 'leave') or set(extra) - {'world'}:
        raise AcceptanceError('Framework acceptance button is unsupported: ' + name)
    ui = FrameworkUI(runner, peer)
    if name == 'open':
        result = ui.open()
    elif name in ('host', 'join'):
        role = 'host' if name == 'host' else 'client'
        result = ui.click(name, lambda value: value.get('admitted') is True and value.get('local_admitted') is True
                          and value.get('local_role') == role and value.get('callback_pending') is False)
    elif name == 'pick':
        world = extra.get('world')
        if not isinstance(world, str) or not world:
            raise AcceptanceError('Framework Pick requires exact saved world name')
        opened = ui.click('pick', lambda value: value.get('world_selection_open') is True and value.get('discovery_pending') is False)
        worlds = opened.get('saved_worlds', [])
        if worlds.count(world) != 1:
            raise AcceptanceError('Requested world absent or duplicated in actual native picker: ' + world)
        page, slot = divmod(worlds.index(world), 4)
        for index in range(page):
            ui.click('list_next', lambda value, expected=index+1: value.get('discovery_page') == expected and value.get('saved_worlds') == worlds)
        result = ui.click('list' + str(slot), lambda value: value.get('selected_world') == world and bool(value.get('selected_file')) and value.get('world_selection_open') is False)
    elif name == 'start':
        result = ui.click('start', lambda value: value.get('prepare', {}).get('start_requested') is True or value.get('prepare', {}).get('playing') is True)
    else:
        previous = ui.read()['prepare']['leave_confirmed']
        result = ui.click('leave', lambda value: value.get('admitted') is False and value.get('local_admitted') is False
                          and value.get('prepare', {}).get('leave_confirmed') == previous + 1 and value.get('prepare', {}).get('leaving') is False)
    runner.checked('framework-button-' + name + '-' + peer, result)
    return result
