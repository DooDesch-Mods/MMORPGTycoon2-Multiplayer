"""Observe graceful native shutdown of exactly owned title-mode test games."""
import argparse
import sys
import shutil
import time
import ctypes
from ctypes import wintypes
from pathlib import Path
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, utc, log
from local_test import WindowsProcesses


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test-root', type=Path, default=ROOT / '_MPTest')
    parser.add_argument('--restart', action='store_true')
    parser.add_argument('--peer', choices=('both','host','client-1'), default='both')
    parser.add_argument('--expected-run-id')
    parser.add_argument('--disconnected-title', action='store_true', help='Use native Title ownership when the transport has already disconnected')
    args = parser.parse_args()
    args.cleanup = False
    args.step_timeout = 30
    args.ready_timeout = 60
    runner = AcceptanceRunner(args)
    runner.summary['scope'] = 'native exit flag acknowledgement and exact owned process termination'
    try:
        if args.restart:
            runner.command('stop-owned', [sys.executable, '-X', 'utf8', str(ROOT / 'tools/stop_local_test.py'), '--test-root', str(args.test_root)])
            runner.command('start-owned', [sys.executable, '-X', 'utf8', str(ROOT / 'tools/local_test.py'), '--test-root', str(args.test_root)])
        runner.load_manifests()
        if args.expected_run_id and runner.summary['runtime_run_id'] != args.expected_run_id:
            raise AcceptanceError('owned run differs; no exit requested')
        peers=('client-1','host') if args.peer=='both' else (args.peer,)
        for peer in peers:
            if args.disconnected_title:
                title=runner.single(f'{peer}-disconnected-title',peer,'game_read_native_lobby')
                if title.get('title_active') is not True or title.get('ui_mode')!=2 or title.get('ui_owner_current') is not True:
                    raise AcceptanceError(f'native exit requires the current owned Title peer={peer}')
                runner.checked(f'{peer}-disconnected-title-owned',title)
            else:
                runner.poll_title(peer)
        processes = WindowsProcesses()
        for peer in peers:
            identity = read_json(runner.manifests[peer])
            handle=processes.api.OpenProcess(processes.QUERY|processes.SYNCHRONIZE,False,identity['pid'])
            if not handle:
                raise ctypes.WinError(ctypes.get_last_error())
            try:
                actual=processes._identity(handle,identity['pid'])
                if any(actual[key]!=identity[key] for key in ('pid','process_start_time','executable_path')):
                    raise AcceptanceError('owned native exit process identity differs; no exit requested')
                result = runner.single(f'{peer}-exit-once', peer, 'game_exit_game')
                if result.get('state') != 'native_exit_flag_confirmed':
                    raise AcceptanceError(f'native exit flag not confirmed peer={peer} actual={result}')
                if processes.api.WaitForSingleObject(handle,15000)!=0:
                    raise AcceptanceError(f'native exit observation timeout peer={peer} required<=15s; no replay')
                code=wintypes.DWORD()
                if not processes.api.GetExitCodeProcess(handle,ctypes.byref(code)):
                    raise ctypes.WinError(ctypes.get_last_error())
                state=f'exited:{code.value}'
                if code.value!=0:
                    raise AcceptanceError(f'native exit failed peer={peer} code={code.value} required=0')
            finally:
                if not processes.api.CloseHandle(handle):
                    raise ctypes.WinError(ctypes.get_last_error())
            runner.checked(f'{peer}-native-process-exited', {'identity': identity, 'state': state})
            source = Path(identity['runtime_root']) / 'game/mt2mp.log'
            archived = runner.directory / f'{peer}-shutdown.log'
            shutil.copy2(source, archived)
            text = archived.read_text(encoding='utf-8')
            for marker in ('[RUNTIME] Shutdown', 'TRANSPORT_WORKER_STOPPED', 'Logger shutting down'):
                if marker not in text:
                    raise AcceptanceError(f'native shutdown marker absent peer={peer} marker={marker} log={archived}')
            runner.checked(f'{peer}-native-shutdown-path', {'log': str(archived), 'mod_shutdown': True, 'transport_worker_stopped': True, 'logger_closed': True})
            log('info', f'graceful native exit peer={peer} pid={identity["pid"]} state={state}')
        runner.summary['passed'] = True
        runner.summary['state'] = 'confirmed-native-exit'
    except Exception as error:
        runner.summary['state'] = 'failed-native-exit'
        runner.summary['error'] = {'step': runner.summary['step'], 'cause': f'{type(error).__name__}: {error}', 'mutation_replayed': False}
        log('error', f'native exit acceptance failed: {error}')
    runner.summary['finished_utc'] = utc()
    runner.persist()
    log('info' if runner.summary['passed'] else 'error', f'native exit passed={runner.summary["passed"]} summary={runner.summary_path}')
    return 0 if runner.summary['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
