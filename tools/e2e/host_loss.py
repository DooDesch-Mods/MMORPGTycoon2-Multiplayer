"""Terminate one exact owned Host and observe the Client's session-end notice."""
import argparse
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT))
from local_acceptance import AcceptanceError, AcceptanceRunner, extract_result, log, read_json, utc
from local_test import RuntimeLock, WindowsProcesses, same_path, validate_manifest
from tools.e2e.run import preflight
from runtime import FRAMEWORK_PLUGIN_KIND, manifest_runtime_kind


def manifest_root(root, identity):
    if manifest_runtime_kind(identity) != FRAMEWORK_PLUGIN_KIND:
        return root
    if root.name != identity['run_id'] or root.parent.name != 'runs':
        raise AcceptanceError('Framework Host-loss requires the exact selected runs/<run_id> directory')
    return root.parent.parent


def owned_host(run, root, expected_run, identity, control_root=None):
    validate_manifest(run, control_root if control_root is not None else root)
    if run['run_id'] != expected_run:
        raise AcceptanceError('Host-loss runtime changed before termination')
    hosts = [p for p in run['processes'] if p['role'] == 'game' and p.get('peer_id') == 1]
    if len(hosts) != 1:
        raise AcceptanceError('Host-loss requires exactly one owned Host game')
    host = hosts[0]
    if (host['pid'] != identity['pid'] or host['process_start_time'] != identity['process_start_time']
            or not same_path(host['runtime_root'], root / 'host')
            or not same_path(host['executable_path'], root / 'host/game/MT2.exe')):
        raise AcceptanceError('Host-loss exact bound Host identity differs')
    return host


def ended(engine, lobby=None):
    disconnected = (engine.get('session_connected') is False and engine.get('peer_count') == 0
                    and engine.get('game_thread_ready') is True
                    and isinstance(engine.get('session_status'), str)
                    and engine['session_status'].startswith('Sitzung beendet:'))
    if lobby is None:
        return disconnected and engine.get('overlay_visible') is True
    ui = lobby.get('framework_ui', {})
    return (disconnected and lobby.get('entry_owner') == 'framework_native_app'
            and lobby.get('panel_registered') is True and lobby.get('panel_visible') is True
            and ui.get('ready') is True and ui.get('open') is True
            and ui.get('visibility_known') is True and ui.get('visible') is True
            and ui.get('suspended') is False)


def execute(args, runner_factory=AcceptanceRunner, processes_factory=WindowsProcesses, now=time.monotonic):
    args.test_root = getattr(args, 'test_root', ROOT / '_MPTest')
    args.players, args.cleanup, args.step_timeout = 2, False, 45
    runner = runner_factory(args)
    runner.summary['scope'] = 'exact owned Host process loss; Client visible session-end notice and native frame; no restart or action replay'
    runner.summary['host_termination'] = {'state': 'never-executed'}
    try:
        preflight(runner, args.freeze, args.setup, candidate_binary=getattr(args, 'candidate_binary', None))
        before = runner.inspect('host-loss-client-before', 'client-1')
        if before.get('session_connected') is not True or before.get('peer_count') != 1:
            raise AcceptanceError('Host-loss needs an actually connected two-peer Client')
        identity = read_json(runner.manifests['host'])
        framework = manifest_runtime_kind(identity) == FRAMEWORK_PLUGIN_KIND
        processes = processes_factory()
        control_root = manifest_root(runner.root, identity)
        with RuntimeLock(control_root):
            run = read_json(control_root / 'run.json')
            host = owned_host(run, runner.root, runner.summary['runtime_run_id'], identity, control_root)
            observed = processes.inspect(host)
            if observed != 'alive':
                raise AcceptanceError('Host-loss requires exact live Host, actual=' + observed)
            runner.summary['host_termination'] = {'state': 'attempted', 'pid': host['pid'], 'process_start_time': host['process_start_time']}
            runner.persist()
            stopped = processes.inspect(host, terminate=True)
            if not stopped.startswith('stopped:'):
                raise AcceptanceError('Host termination unconfirmed actual=' + stopped + '; no retry')
            runner.summary['host_termination'].update(state='confirmed', result=stopped)
            runner.persist()
        deadline, poll, last = now() + 30, 0, None
        while now() < deadline:
            poll += 1
            remaining = deadline - now()
            if remaining <= 2:
                break
            calls = [{'tool': 'game_inspect_engine', 'arguments': {}}]
            if framework:
                calls.append({'tool': 'game_read_native_lobby', 'arguments': {}})
            receipt = runner.job('host-loss-client-' + str(poll), 'client-1', calls, remaining=remaining)
            last = extract_result(receipt, 'game_inspect_engine')
            lobby = extract_result(receipt, 'game_read_native_lobby') if framework else None
            if ended(last, lobby):
                runner.checked('client-visible-session-ended', {'before': before, 'after': last, 'native_lobby': lobby})
                runner.capture('client-1')
                runner.summary.update(passed=True, state='confirmed-native-host-loss-notice',
                                      visual_acceptance='native captured frame requires inspection')
                break
        else:
            last = last or {}
        if runner.summary.get('passed') is not True:
            raise AcceptanceError('Host-loss Client notice timeout required<=30s status=' + str((last or {}).get('session_status')) + '; Host remains stopped; no replay')
    except Exception as error:
        runner.summary.update(passed=False, state='failed-native-host-loss', error={
            'step': runner.summary['step'], 'cause': str(error),
            'behavior': 'only exact owned Host termination permitted once; no restart, cleanup or replay'})
        log('error', str(error))
    runner.summary['finished_utc'] = utc()
    runner.persist()
    print(runner.summary_path)
    return 0 if runner.summary.get('passed') is True else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup', type=Path, required=True)
    parser.add_argument('--freeze', type=Path, required=True)
    parser.add_argument('--test-root', type=Path, default=ROOT / '_MPTest')
    parser.add_argument('--candidate-binary', type=Path, help='Exact selected Framework plugin artifact')
    return execute(parser.parse_args(argv))


if __name__ == '__main__':
    raise SystemExit(main())
