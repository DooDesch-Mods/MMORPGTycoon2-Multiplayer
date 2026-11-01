"""Replace one owned Client process pair without touching the Host or its data."""
from pathlib import Path
import math
import os
import shutil
import subprocess
import uuid


def restart_client(args, processes, api):
    root = Path(os.path.abspath(args.test_root))
    path = api.safe_path(root / 'run.json', root)
    manifest = api.read_json(path)
    api.validate_manifest(manifest, root)
    if not args.expected_run_id or manifest['run_id'] != args.expected_run_id or manifest['state'] != 'transport-ready':
        raise api.LaunchError('Client restart requires the exact transport-ready owned run')
    records = manifest['processes']
    expected = {('relay', 0), ('bridge', 1), ('game', 1), ('bridge', 2), ('game', 2)}
    if len(records) != 5 or {(row['role'], row['peer_id']) for row in records} != expected:
        raise api.LaunchError('Client restart requires exactly one owned two-peer process set')
    client = {row['role']: row for row in records if row['peer_id'] == 2}
    preserved = [row for row in records if row['peer_id'] != 2]
    runtime = api.safe_path(root / 'runs' / manifest['run_id'] / 'client-1', root)
    if any(not api.legacy.same_path(row['runtime_root'], runtime) for row in client.values()):
        raise api.LaunchError('Client restart runtime differs from its exact run/peer directory')

    def retained_alive():
        for record in preserved:
            state = processes.inspect(record)
            if state != 'alive':
                raise api.LaunchError(f"Client restart requires unchanged live {record['role']} peer={record['peer_id']}: {state}")

    retained_alive()
    for record in client.values():
        state = processes.inspect(record)
        if state != 'alive' and state != 'exited' and not state.startswith('exited:'):
            raise api.LaunchError(f"Client restart refuses unknown {record['role']} identity: {state}")
    api.reject_pending(root, {'processes': list(client.values())})
    diagnostics = api.safe_path(runtime / 'profile/diagnostics', root)
    request_path, response_path = diagnostics / 'request.json', diagnostics / 'response.json'
    if request_path.exists():
        request = api.read_json(api.safe_path(request_path, root))
        response = api.read_json(api.safe_path(response_path, root)) if response_path.exists() else {}
        if any(response.get(key) != request.get(key) for key in ('id', 'pid', 'process_start_time')):
            raise api.LaunchError('Client restart refuses an unacknowledged Framework request; original request retained')
    config_path = api.safe_path(runtime / 'profile/data/doodesch.multiplayer/local-test.json', root)
    config = api.read_json(config_path)
    if (config.get('owner') != manifest['owner'] or config.get('run_id') != manifest['run_id']
            or config.get('peer_id') != 2 or str(config.get('session_id')) != manifest['session_id']
            or not api.legacy.same_path(config.get('runtime_root', ''), runtime)
            or type(config.get('control_port')) is not int or not 1 <= config['control_port'] <= 65535
            or config['control_port'] == manifest['relay_port'] or config.get('transport') != 'loopback'
            or not isinstance(config.get('session_token'), str) or len(config['session_token']) != 32):
        raise api.LaunchError('Client restart retained local transport config differs')
    config_hash = api.sha256(config_path)
    copied = api.read_json(api.safe_path(runtime / 'copy-manifest.json', root))

    def check_binaries():
        for name, relative in {**api.MODULES, 'mt2mp_sidecar.exe': 'game/mt2mp_sidecar.exe'}.items():
            actual = api.sha256(api.safe_path(runtime / relative, root))
            if actual != copied.get(relative, {}).get('sha256') or (name in api.MODULES and actual != client['game']['installed_binary_sha256'][name]):
                raise api.LaunchError('Client restart installed binary changed: ' + name)
        if api.sha256(config_path) != config_hash:
            raise api.LaunchError('Client restart local transport config changed')

    check_binaries()
    restart_id = uuid.uuid4().hex
    archive = api.safe_path(runtime / 'restarts' / restart_id, root)
    archive.mkdir(parents=True, exist_ok=False)
    receipt = {'id': restart_id, 'state': 'attempted', 'old_processes': list(client.values()),
               'preserved_processes': preserved, 'new_processes': [], 'mutation_retries': 0}
    manifest.setdefault('client_restarts', []).append(receipt)
    manifest['state'] = 'restarting-client'
    api.atomic_json(path, manifest)
    api.atomic_json(archive / 'before.json', {'processes': list(client.values()), 'config_sha256': config_hash})
    started = []
    reservations = []

    def persist():
        api.atomic_json(path, manifest)
        api.atomic_json(archive / 'receipt.json', receipt)

    def start(role, command, env=None):
        retained_alive(); check_binaries()
        log_path = api.safe_path(runtime / 'logs' / f'{role}-{manifest["run_id"]}-{restart_id}.log', root)
        with log_path.open('wb') as stream:
            process = subprocess.Popen(command, cwd=runtime / 'game', env=env, stdin=subprocess.DEVNULL,
                                       stdout=stream, stderr=subprocess.STDOUT,
                                       creationflags=subprocess.CREATE_BREAKAWAY_FROM_JOB | (subprocess.CREATE_NO_WINDOW if role != 'game' else 0))
        started.append((process, {'role': role, 'pid': process.pid, 'log_path': str(log_path)}))
        receipt.setdefault('process_attempts', []).append(started[-1][1])
        persist()
        identity = processes.identity(process)
        if not api.legacy.same_path(identity['executable_path'], command[0]) or identity['process_start_time'] == client[role]['process_start_time']:
            raise api.LaunchError('Client restart did not produce a fresh exact process identity: ' + role)
        record = {**client[role], **identity, 'log_path': str(log_path)}
        for field in ('stop_state', 'stop_error', 'exit_code', 'cleanup_error'):
            record.pop(field, None)
        started[-1] = (process, record)
        receipt['new_processes'].append(record)
        records[records.index(client[role])] = record
        persist()
        if role == 'game':
            api.atomic_json(api.safe_path(runtime / 'instance.json', root), record)
        return record

    try:
        for role in ('game', 'bridge'):
            state = processes.inspect(client[role], terminate=True)
            if state != 'exited' and not state.startswith(('stopped:', 'exited:')):
                raise api.LaunchError(f'Client restart stop refused role={role}: {state}')
            receipt.setdefault('stopped', {})[role] = state
            persist()
        retained_alive()
        # Preserve prior diagnostics before the replacement process writes its logs.
        for relative in ('profile/diagnostics', 'profile/data/doodesch.multiplayer/mt2mp.log',
                         'profile/data/doodesch.multiplayer/mt2mp_logs', 'game/MT2.RPT'):
            original = api.safe_path(runtime / relative, root)
            if original.is_dir():
                for child in original.rglob('*'):
                    api.safe_path(child, root)
                shutil.copytree(original, archive / relative)
            elif original.is_file():
                target = archive / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(original, target)
        reservations = api.reserve_ports([config['control_port']])
        reservations[0].close()
        bridge = start('bridge', [str(runtime / 'game/mt2mp_sidecar.exe'), '--mode', 'client', '--relay-host', '127.0.0.1',
                       '--relay-port', str(manifest['relay_port']), '--control-port', str(config['control_port']),
                       '--local-connect-timeout-ms', str(math.ceil(args.ready_timeout * 1000)),
                       '--session', str(config['session_id']), '--token', config['session_token']])
        api.wait_marker(Path(bridge['log_path']), 'CONTROL_READY', started, args.ready_timeout)
        env = {key: value for key, value in os.environ.items() if not key.upper().startswith('MT2MP_')}
        env.update(SteamAppId=api.STEAM_APP_ID, SteamGameId=api.STEAM_APP_ID)
        game = start('game', [str(runtime / 'game/MT2.exe')], env)
        api.wait_games([game], started, args.ready_timeout)
        retained_alive()
        receipt['state'] = 'confirmed-new-client-transport'
        manifest['state'] = 'transport-ready'
        persist()
        api.log('info', f'Client replaced run={manifest["run_id"]} peer=2 pid={game["pid"]}; shared world rejoin still requires native acceptance')
        return 0
    except BaseException as error:
        receipt.update(state='failed-no-replay', error=str(error))
        manifest.update(state='client-restart-failed', failure=str(error))
        # Only new Popen handles belong to this failed attempt. The Host and relay
        # stay live, and prior Client identities cannot be reused as cleanup targets.
        for process, record in reversed(started):
            try:
                if process.poll() is None:
                    process.kill(); process.wait(timeout=5)
                record['exit_code'] = process.returncode
            except (OSError, subprocess.TimeoutExpired) as cleanup_error:
                record['cleanup_error'] = str(cleanup_error)
                api.log('error', f'Client restart cleanup role={record["role"]} failed: {cleanup_error}')
        try:
            persist()
        except Exception as persistence_error:
            # Keep the failed operation as the primary exception even when its
            # diagnostic receipt cannot be written, for example on a full disk.
            detail = f'Client restart failure receipt could not be persisted: {persistence_error}'
            error.add_note(detail)
            api.log('error', detail + '; original failure: ' + str(error))
        raise
    finally:
        for endpoint in reservations:
            endpoint.close()
